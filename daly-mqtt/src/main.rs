use anyhow::{anyhow, Context, Result};
use byteorder::{BigEndian, ReadBytesExt};
use bytes::{BufMut, Bytes, BytesMut};
use clap::Parser;
use futures::{stream::StreamExt, SinkExt};
use lazy_static::lazy_static;
use log::{debug, info, warn};
use rumqttc::{AsyncClient, Event, MqttOptions, Packet, QoS};
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
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
    charge_mos: MqttEntity,
    discharge_mos: MqttEntity,
    mqtt_tx: Option<mpsc::Sender<(String, Vec<u8>)>>,
    uart_tx: Option<mpsc::Sender<Bytes>>,

    command_to_entity: HashMap<String, u8>,
}

lazy_static! {
    static ref BMS: Arc<Mutex<DalyBms>> = Arc::new(Mutex::new(DalyBms {
        soc: MqttEntity {
            name: "Level".to_string(),
            component_type: "sensor".to_string(),
            object_id: "soc".to_string(),
            state_topic: format!("homeassistant/sensor/daly-mqtt/soc"),
            unit: Some("%".to_string()),
            command: None,
        },
        rem_ah: MqttEntity {
            name: "Remaining Capacity".to_string(),
            component_type: "sensor".to_string(),
            object_id: "rem_ah".to_string(),
            state_topic: format!("homeassistant/sensor/daly-mqtt/rem_ah"),
            unit: Some("Ah".to_string()),
            command: None,
        },
        voltage: MqttEntity {
            name: "Voltage".to_string(),
            component_type: "sensor".to_string(),
            object_id: "voltage".to_string(),
            state_topic: format!("homeassistant/sensor/daly-mqtt/voltage"),
            unit: Some("V".to_string()),
            command: None,
        },
        current: MqttEntity {
            name: "Current".to_string(),
            component_type: "sensor".to_string(),
            object_id: "current".to_string(),
            state_topic: format!("homeassistant/sensor/daly-mqtt/current"),
            unit: Some("A".to_string()),
            command: None,
        },
        temperature: MqttEntity {
            name: "Temperature".to_string(),
            component_type: "sensor".to_string(),
            object_id: "temperature".to_string(),
            state_topic: format!("homeassistant/sensor/daly-mqtt/temperature"),
            unit: Some("°C".to_string()),
            command: None,
        },
        charge_mos: MqttEntity {
            name: "Charging MOS".to_string(),
            component_type: "light".to_string(),
            object_id: "charging_mos".to_string(),
            state_topic: format!("homeassistant/light/daly-mqtt/charging_mos"),
            unit: None,
            command: Some(CommandStruct {
                topic: format!("homeassistant/light/daly-mqtt/set-charging-mos"),
                uart_cmd: 0xDA,
            })
        },
        discharge_mos: MqttEntity {
            name: "Discharging MOS".to_string(),
            component_type: "light".to_string(),
            object_id: "discharging_mos".to_string(),
            state_topic: format!("homeassistant/light/daly-mqtt/discharging_mos"),
            unit: None,
            command: Some(CommandStruct {
                topic: format!("homeassistant/light/daly-mqtt/set-discharging-mos"),
                uart_cmd: 0xD9,
            })
        },
        mqtt_tx: None,
        uart_tx: None,
        command_to_entity: HashMap::new(),
    }));
}

#[derive(Clone)]
struct CommandStruct {
    topic: String,
    uart_cmd: u8,
    // do_fn: Box<dyn FnMut()>,
}
struct MqttEntity {
    name: String,
    component_type: String,
    object_id: String,
    state_topic: String,
    unit: Option<String>,
    command: Option<CommandStruct>,
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
            command_topic: self.command.as_ref().map(|x| x.topic.clone()),
            ..Default::default()
        };
        (topic, serde_json::to_vec(&d).unwrap())
    }
    fn update_msg(&self, val: Vec<u8>) -> (String, Vec<u8>) {
        (self.state_topic.clone(), val)
    }
}

const HA_STATUS_TOPIC: &str = "homeassistant/status";

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

    let (topic, msg) = BMS.lock().unwrap().charge_mos.discovery_msg();
    client
        .publish(&topic, QoS::AtLeastOnce, false, msg)
        .await
        .unwrap();
    let command_struct = BMS.lock().unwrap().charge_mos.command.clone();
    if let Some(command) = command_struct {
        client
            .subscribe(&command.topic, QoS::AtMostOnce)
            .await
            .with_context(|| format!("MQTT subscribe failed"))?;
        BMS.lock()
            .unwrap()
            .command_to_entity
            .insert(command.topic, command.uart_cmd);
    }
    let (topic, msg) = BMS.lock().unwrap().discharge_mos.discovery_msg();
    client
        .publish(&topic, QoS::AtLeastOnce, false, msg)
        .await
        .unwrap();
    let command_struct = BMS.lock().unwrap().discharge_mos.command.clone();
    if let Some(command) = command_struct {
        client
            .subscribe(&command.topic, QoS::AtMostOnce)
            .await
            .with_context(|| format!("MQTT subscribe failed"))?;
        BMS.lock()
            .unwrap()
            .command_to_entity
            .insert(command.topic, command.uart_cmd);
    }
    Ok(())
}

// Handles MQTT notifications from HA.
async fn handle_notification(notification: &Event, client: &AsyncClient) -> anyhow::Result<()> {
    match &notification {
        Event::Outgoing(_) => {}
        Event::Incoming(inc) => match inc {
            Packet::Publish(p) => {
                let message = std::str::from_utf8(&p.payload)
                    .with_context(|| format!("MQTT message is not utf8"))?;
                if p.topic == HA_STATUS_TOPIC {
                    if message == "online" {
                        info!("re-discovering");
                        discover(&client).await?;
                    }
                } else {
                    let x = BMS
                        .lock()
                        .unwrap()
                        .command_to_entity
                        .get(&p.topic)
                        .map(|x| *x);
                    if let Some(entity) = x {
                        let arg = match message {
                            "OFF" => 0,
                            "ON" => 1,
                            _ => return Err(anyhow!(format!("unexpected command: {}", message))),
                        };
                        let req = make_req(entity, arg);
                        let tx = BMS.lock().unwrap().uart_tx.as_ref().unwrap().clone();
                        tx.send(req).await?;
                    } else {
                        return Err(anyhow!(format!("message on unknown topic: {}", message)));
                    }
                }
            }
            _ => {
                debug!("Received {:?}", notification);
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
        info!("out\t{:02X?}", item.to_vec());
        dst.put(item);
        Ok(())
    }
}

fn make_req(cmd: u8, arg1: u8) -> Bytes {
    let mut b = BytesMut::from(&b"\xA5\x40\x00\x08\x00\x00\x00\x00\x00\x00\x00\x00"[..]);
    // see https://github.com/softwarecrash/Daly2MQTT/blob/6e213d3ed5aec81f79ddeaa330ac7fa44f8eb4a4/src/daly.cpp#L698
    b[2] = cmd;
    b[4] = arg1;
    let cksum = b[0..12].iter().fold(0u8, |a, b| a.wrapping_add(*b));
    b.put_u8(cksum);
    b.freeze()
}

async fn parse_msg(msg: Bytes) -> Result<()> {
    info!("in\t{:02X?}", msg.to_vec());
    let cksum = msg[0..12].iter().fold(0u8, |a, b| a.wrapping_add(*b));
    if cksum != msg[12] {
        return Err(anyhow!(format!(
            "checksum failed, {} != {}",
            cksum, msg[12]
        )));
    }
    let mqtt_tx = BMS.lock().unwrap().mqtt_tx.clone().unwrap();
    let mut rdr = Cursor::new(&msg[4..]);
    match msg[2] {
        0x90 => {
            let cum_volts = rdr.read_u16::<BigEndian>().unwrap() as f64 / 10.;
            let m = BMS
                .lock()
                .unwrap()
                .voltage
                .update_msg(serde_json::to_vec(&cum_volts).unwrap());
            mqtt_tx.send(m).await?;

            let _ = rdr.read_u16::<BigEndian>().unwrap();

            let current = (rdr.read_u16::<BigEndian>().unwrap() as f64 - 30000.) / 10.;
            let m = BMS
                .lock()
                .unwrap()
                .current
                .update_msg(serde_json::to_vec(&current).unwrap());
            mqtt_tx.send(m).await?;

            let soc = rdr.read_u16::<BigEndian>().unwrap() as f64 / 10.;
            let m = BMS
                .lock()
                .unwrap()
                .soc
                .update_msg(serde_json::to_vec(&soc).unwrap());
            mqtt_tx.send(m).await?;
        }
        0x93 => {
            let _state = rdr.read_u8().unwrap();

            let charge_mos = if rdr.read_u8().unwrap() == 0 {
                b"OFF"[..].to_vec()
            } else {
                b"ON"[..].to_vec()
            };
            let m = BMS.lock().unwrap().charge_mos.update_msg(charge_mos);
            mqtt_tx.send(m).await?;

            let discharge_mos = if rdr.read_u8().unwrap() == 0 {
                b"OFF"[..].to_vec()
            } else {
                b"ON"[..].to_vec()
            };
            let m = BMS.lock().unwrap().discharge_mos.update_msg(discharge_mos);
            mqtt_tx.send(m).await?;

            let _bms_life = rdr.read_u8().unwrap();

            let ah = rdr.read_u32::<BigEndian>().unwrap() as f64 / 1000.;
            let m = BMS
                .lock()
                .unwrap()
                .rem_ah
                .update_msg(serde_json::to_vec(&ah).unwrap());
            mqtt_tx.send(m).await?;
        }
        0x96 => {
            let _ = rdr.read_u8().unwrap();
            let temperature = rdr.read_u8().unwrap() as f64 - 40.;
            let m = BMS
                .lock()
                .unwrap()
                .temperature
                .update_msg(serde_json::to_vec(&temperature).unwrap());
            mqtt_tx.send(m).await?;
        }
        0xD9 => {
            let dis_charge_mos = if rdr.read_u8().unwrap() == 0 {
                b"OFF"[..].to_vec()
            } else {
                b"ON"[..].to_vec()
            };
            let m = BMS.lock().unwrap().discharge_mos.update_msg(dis_charge_mos);
            mqtt_tx.send(m).await?;
        }
        0xDA => {
            let charge_mos = if rdr.read_u8().unwrap() == 0 {
                b"OFF"[..].to_vec()
            } else {
                b"ON"[..].to_vec()
            };
            let m = BMS.lock().unwrap().charge_mos.update_msg(charge_mos);
            mqtt_tx.send(m).await?;
        }
        _ => {
            return Err(anyhow!(format!(
                "unknown message: 0x{:0X}, {:X?}",
                msg[2],
                msg.to_vec()
            )));
        }
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
                warn!("handle notification failed with {:?}", r);
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

    let (tx, mut rx) = mpsc::channel(32);
    tx.send(make_req(0x90, 0)).await?;
    tx.send(make_req(0x93, 0)).await?;
    tx.send(make_req(0x96, 0)).await?;

    let (command_tx, mut command_rx) = mpsc::channel::<Bytes>(32);
    BMS.lock().unwrap().uart_tx = Some(command_tx);

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
            cmd = command_rx.recv() => {
                if let Some(cmd) = cmd {
                    reader.send(cmd.into()).await?;
                } else {
                    break;
                }
            }
            _ =  tokio::time::sleep(Duration::from_secs(10)) => {
                if let Some(b) = rx.recv().await {
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
    BMS.lock().unwrap().mqtt_tx = Some(tx);

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
