use anyhow::{anyhow, Context, Result};
use byteorder::{BigEndian, ReadBytesExt};
use bytes::{BufMut, Bytes, BytesMut};
use clap::Parser;
use futures::{stream::StreamExt, SinkExt};
use lazy_static::lazy_static;
use rumqttc::{AsyncClient, Event, MqttOptions, Packet, QoS};
use serde::{Deserialize, Serialize};
use std::io::Cursor;
use std::io::{self};
use std::sync::{Arc, Mutex};
use std::time::Duration;
use tokio::sync::mpsc;
use tokio::task::JoinSet;
use tokio_serial::SerialPortBuilderExt;
use tokio_util::codec::{Decoder, Encoder};

#[derive(Parser, Debug)]
#[command(version, about, long_about = None)]
struct Args {
    #[arg(long, default_value = "/dev/ttyS0")]
    serial_port: String,
    #[arg(long)]
    mqtt_host: String,
    #[arg(long, default_value = "daly-bms")]
    mqtt_client_id: String,
    #[arg(long, default_value_t = 1883)]
    mqtt_port: u16,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
struct MqttOriginMessage {
    name: String,
    sw: String,
}
impl Default for MqttOriginMessage {
    fn default() -> Self {
        MqttOriginMessage {
            name: "mqtt-rust".to_string(),
            sw: "0.1".to_string(),
        }
    }
}
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
struct MqttDeviceMessage {
    identifiers: Vec<String>,
    manufacturer: String,
    model: String,
    name: String,
}
impl Default for MqttDeviceMessage {
    fn default() -> Self {
        MqttDeviceMessage {
            identifiers: vec!["6402".to_string()],
            manufacturer: "homelab".to_string(),
            model: "mqtt-rust".to_string(),
            name: "BMS".to_string(),
        }
    }
}

#[derive(Clone, Default, Debug, PartialEq, Serialize, Deserialize)]
struct MqttDiscoveryMessage {
    name: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    command_topic: Option<String>,
    state_topic: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    unit_of_measuremenmt: Option<String>,
    unique_id: String,
    origin: MqttOriginMessage,
    device: MqttDeviceMessage,
}
struct DalyBms {
    soc: MqttEntity,
    current: MqttEntity,
    voltage: MqttEntity,
    rem_ah: MqttEntity,
    temperature: MqttEntity,
    tx: Option<mpsc::Sender<(String, Vec<u8>)>>,
}

lazy_static! {
    static ref BMS: Arc<Mutex<DalyBms>> = Arc::new(Mutex::new(DalyBms {
        soc: MqttEntity {
            name: "Level".to_string(),
            component_type: "sensor".to_string(),
            object_id: "soc".to_string(),
            state_topic: format!("homeassistant/sensor/daly-mqtt/soc"),
            unit: Some("%".to_string()),
        },
        rem_ah: MqttEntity {
            name: "Remaining Capacity".to_string(),
            component_type: "sensor".to_string(),
            object_id: "rem_ah".to_string(),
            state_topic: format!("homeassistant/sensor/daly-mqtt/rem_ah"),
            unit: Some("Ah".to_string()),
        },
        voltage: MqttEntity {
            name: "Voltage".to_string(),
            component_type: "sensor".to_string(),
            object_id: "voltage".to_string(),
            state_topic: format!("homeassistant/sensor/daly-mqtt/voltage"),
            unit: Some("V".to_string()),
        },
        current: MqttEntity {
            name: "Current".to_string(),
            component_type: "sensor".to_string(),
            object_id: "current".to_string(),
            state_topic: format!("homeassistant/sensor/daly-mqtt/current"),
            unit: Some("A".to_string()),
        },
        temperature: MqttEntity {
            name: "Temperature".to_string(),
            component_type: "sensor".to_string(),
            object_id: "temperature".to_string(),
            state_topic: format!("homeassistant/sensor/daly-mqtt/temperature"),
            unit: Some("°C".to_string()),
        },
        tx: None,
    }));
}

struct MqttEntity {
    name: String,
    component_type: String,
    object_id: String,
    state_topic: String,
    unit: Option<String>,
}

impl MqttEntity {
    fn discovery_msg(&self) -> (String, Vec<u8>) {
        // MQTT discovery message topic: <discovery_prefix>/<component>/[<node_id>/]<object_id>/config
        let topic = format!(
            "homeassistant/{}/{}/config",
            self.component_type, self.object_id
        );
        let d = MqttDiscoveryMessage {
            name: self.name.clone(),
            state_topic: self.state_topic.clone(),
            unit_of_measuremenmt: self.unit.clone(),
            unique_id: format!("daly-mqtt-{}", self.object_id),
            ..Default::default()
        };
        (topic, serde_json::to_vec(&d).unwrap())
    }
    fn update_msg(&self, val: serde_json::Value) -> (String, Vec<u8>) {
        (self.state_topic.clone(), serde_json::to_vec(&val).unwrap())
    }
}

const HA_STATUS_TOPIC: &str = "homeassistant/status";
const MQTT_COMMAND: &str = "homeassistant/light/photo-frame/set";

// Sends out the mqtt discovery messages
async fn discover(client: &AsyncClient) -> anyhow::Result<()> {
    let (topic, msg) = BMS.lock().unwrap().soc.discovery_msg();
    client
        .publish(&topic, QoS::AtLeastOnce, false, msg)
        .await
        .unwrap();
    let (topic, msg) = BMS.lock().unwrap().current.discovery_msg();
    client
        .publish(&topic, QoS::AtLeastOnce, false, msg)
        .await
        .unwrap();
    let (topic, msg) = BMS.lock().unwrap().rem_ah.discovery_msg();
    client
        .publish(&topic, QoS::AtLeastOnce, false, msg)
        .await
        .unwrap();
    let (topic, msg) = BMS.lock().unwrap().voltage.discovery_msg();
    client
        .publish(&topic, QoS::AtLeastOnce, false, msg)
        .await
        .unwrap();
    let (topic, msg) = BMS.lock().unwrap().temperature.discovery_msg();
    client
        .publish(&topic, QoS::AtLeastOnce, false, msg)
        .await
        .unwrap();
    Ok(())
}

// Handles MQTT notifications from HA.
async fn handle_notification(notification: &Event, client: &AsyncClient) -> anyhow::Result<()> {
    match &notification {
        Event::Outgoing(_) => {
            //println!("Received outgoing = {:?}", n);
        }
        Event::Incoming(inc) => match inc {
            Packet::Publish(p) => {
                let message = std::str::from_utf8(&p.payload)
                    .with_context(|| format!("MQTT message is not utf8"))?;
                match p.topic.as_str() {
                    HA_STATUS_TOPIC => {
                        if message == "online" {
                            println!("re-discovering");
                            discover(&client).await?;
                        }
                    }
                    MQTT_COMMAND => match message {
                        "OFF" => {
                            println!("should turn off");
                            //do_ctrl(false, &uhubctl).await?;
                        }
                        "ON" => {
                            println!("should turn on");
                            // do_ctrl(true, &uhubctl).await?;
                        }
                        _ => {
                            println!("unexpected command: {}", message);
                        }
                    },
                    _ => {
                        println!("message payload: {}", message);
                    }
                }
            }
            _ => {
                //println!("Received = {:?}", notification);
            }
        },
    };
    Ok(())
}

struct DalyMsg;

impl Decoder for DalyMsg {
    type Item = Bytes;
    type Error = io::Error;

    fn decode(&mut self, src: &mut BytesMut) -> Result<Option<Self::Item>, Self::Error> {
        let p = src.iter().enumerate().find(|(_, x)| **x == 0xa5u8);
        if let Some(p) = p {
            if src.len() - p.0 == 13 {
                return Ok(Some(src.split().freeze()));
            }
        }
        Ok(None)
    }
}

impl Encoder<Bytes> for DalyMsg {
    type Error = io::Error;

    fn encode(&mut self, item: Bytes, dst: &mut BytesMut) -> Result<(), Self::Error> {
        dst.put(item);
        Ok(())
    }
}

fn make_req(cmd: u8) -> Bytes {
    let mut b = BytesMut::from(&b"\xA5\x40\x00\x08\x00\x00\x00\x00\x00\x00\x00\x00"[..]);
    b[2] = cmd;
    let cksum = b[0..12].iter().fold(0u8, |a, b| a.wrapping_add(*b));
    b.put_u8(cksum);
    b.freeze()
}

async fn parse_msg(msg: Bytes) -> Result<()> {
    println!("got {:?}", msg);
    let cksum = msg[0..12].iter().fold(0u8, |a, b| a.wrapping_add(*b));
    if cksum != msg[12] {
        return Err(anyhow!(format!(
            "checksum failed, {} != {}",
            cksum, msg[12]
        )));
    }
    let tx = BMS.lock().unwrap().tx.clone().unwrap();
    let mut rdr = Cursor::new(&msg[4..]);
    match msg[2] {
        0x90 => {
            let cum_volts = rdr.read_u16::<BigEndian>().unwrap() as f64 / 10.;
            let m = BMS
                .lock()
                .unwrap()
                .voltage
                .update_msg(serde_json::Value::from(cum_volts));
            tx.send(m).await?;

            let _ = rdr.read_u16::<BigEndian>().unwrap();

            let current = (rdr.read_u16::<BigEndian>().unwrap() as f64 - 30000.) / 10.;
            let m = BMS
                .lock()
                .unwrap()
                .current
                .update_msg(serde_json::Value::from(current));
            tx.send(m).await?;

            let soc = rdr.read_u16::<BigEndian>().unwrap() as f64 / 10.;
            let m = BMS
                .lock()
                .unwrap()
                .soc
                .update_msg(serde_json::Value::from(soc));
            tx.send(m).await?;

            println!("SOC, cum_volts: {cum_volts}, current: {current}, soc: {soc}");
        }
        0x93 => {
            let state = rdr.read_u8().unwrap();
            let charge_mos = rdr.read_u8().unwrap();
            let discharge_mos = rdr.read_u8().unwrap();
            let bms_life = rdr.read_u8().unwrap();

            let ah = rdr.read_u32::<BigEndian>().unwrap() as f64 / 1000.;
            let m = BMS
                .lock()
                .unwrap()
                .rem_ah
                .update_msg(serde_json::Value::from(ah));
            tx.send(m).await?;

            println!("mos status, ah: {ah}, life: {bms_life}");
        }
        0x96 => {
            let _ = rdr.read_u8().unwrap();
            let temperature = rdr.read_u8().unwrap() as f64 - 40.;
            let m = BMS
                .lock()
                .unwrap()
                .temperature
                .update_msg(serde_json::Value::from(temperature));
            tx.send(m).await?;
        }
        _ => {}
    }
    Ok(())
}

async fn do_mqtt(
    mut rx: mpsc::Receiver<(String, Vec<u8>)>,
    host: String,
    port: u16,
    client_id: String,
) -> Result<()> {
    let mut mqttoptions = MqttOptions::new(&client_id, &host, port);
    mqttoptions.set_keep_alive(Duration::from_secs(60));

    let (client, mut eventloop) = AsyncClient::new(mqttoptions, 10);
    client
        .subscribe(HA_STATUS_TOPIC, QoS::AtMostOnce)
        .await
        .with_context(|| format!("MQTT subscribe failed"))?;
    client
        .subscribe(MQTT_COMMAND, QoS::AtMostOnce)
        .await
        .with_context(|| format!("MQTT subscribe failed"))?;

    discover(&client)
        .await
        .with_context(|| format!("failed to send MQTT discover"))?;

    let c = client.clone();
    let mut set: JoinSet<Result<()>> = JoinSet::new();
    set.spawn(async move {
        while let Some((topic, msg)) = rx.recv().await {
            c.publish(&topic, QoS::AtLeastOnce, false, msg).await?;
        }
        Ok(())
    });

    set.spawn(async move {
        loop {
            let notification = eventloop.poll().await?;
            let r = handle_notification(&notification, &client).await;
            if r.is_err() {
                println!("handle notification failed with {:?}", r);
            }
        }
    });

    while let Some(res) = set.join_next().await {
        res??;
    }
    Ok(())
}

async fn do_serial(serial_port: String) -> Result<()> {
    let mut port = tokio_serial::new(&serial_port, 9600).open_native_async()?;
    port.set_exclusive(false)
        .with_context(|| format!("failed to set exclusive"))?;
    let mut reader = DalyMsg.framed(port);
    println!("starting serial read");

    let (tx, mut rx) = mpsc::channel(32);
    tx.send(make_req(0x90)).await?;
    tx.send(make_req(0x93)).await?;
    tx.send(make_req(0x96)).await?;

    loop {
        tokio::time::sleep(Duration::from_millis(100)).await;
        tokio::select! {
            msg = reader.next() => {
                if let Some(msg) = msg {
                    let msg = msg.expect("Failed to read line");
                    parse_msg(msg).await?;
                } else {
                    break;
                }
            }
            _ =  tokio::time::sleep(Duration::from_secs(3)) => {
                if let Some(b) = rx.recv().await {
                    println!("will send {} bytes: {:?}", b.len(), b);
                    reader.send(b.clone()).await?;
                    tx.send(b).await?;
                } else {
                    break;
                }
            }
        }
    }

    Ok(())
}

#[tokio::main(flavor = "current_thread")]
async fn main() -> Result<()> {
    let args = Args::parse();

    let (tx, rx) = mpsc::channel(32);
    BMS.lock().unwrap().tx = Some(tx);

    let mut set = JoinSet::new();
    set.spawn(do_mqtt(
        rx,
        args.mqtt_host,
        args.mqtt_port,
        args.mqtt_client_id,
    ));
    set.spawn(do_serial(args.serial_port.clone()));
    while let Some(res) = set.join_next().await {
        res??;
    }

    Ok(())
}
