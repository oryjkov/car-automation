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
            name: "daly mqtt".to_string(),
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
struct MqttEntity {
    name: String,
    state_topic: String,
    unit: Option<String>,
}

struct DalyBms {
    soc: MqttEntity,
}

lazy_static! {
    static ref BMS: Arc<Mutex<DalyBms>> = Arc::new(Mutex::new(DalyBms {
        soc: MqttEntity {
            name: "Battery Level".to_string(),
            state_topic: "TODO".to_string(),
            unit: Some("%".to_string()),
        }
    }));
}

impl MqttEntity {
    fn discovery_msg(&self) -> Vec<u8> {
        let d = MqttDiscoveryMessage {
            name: self.name.clone(),
            state_topic: self.state_topic.clone(),
            unit_of_measuremenmt: self.unit.clone(),
            ..Default::default()
        };
        serde_json::to_vec(&d).unwrap()
    }
}

const HA_STATUS_TOPIC: &str = "homeassistant/status";
const MQTT_COMMAND: &str = "homeassistant/light/photo-frame/set";
const MQTT_STATE_TEMPLATE: &str = "homeassistant/light/photo-frame/state";
const MQTT_STATE: &str = "homeassistant/light/photo-frame/state";
const MQTT_DISCOVERY: &str = "homeassistant/light/photo-frame/config";
const DISCOVERY_CONFIG: &str = r#"
{
   "name":"Photo Frame",
   "command_topic":"homeassistant/light/photo-frame/set",
   "state_topic":"homeassistant/light/photo-frame/state",
   "unique_id":"photo_frame_9813",
   "origin": {
     "name":"photo-frame",
     "sw": "0.1"
   },
   "device":{
      "identifiers":[
         "9813"
      ],
      "manufacturer": "home",
      "model": "PIPI",
      "name":"Pi"
   }
}
"#;

// Sends out the mqtt discovery messages
async fn discover(client: &AsyncClient) -> anyhow::Result<()> {
    let m = BMS.lock().unwrap().soc.discovery_msg();
    client
        .publish(MQTT_DISCOVERY, QoS::AtLeastOnce, false, m)
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

struct DalyReq {}
//   request_message[0] = 0xA5;         // Start Flag
//   request_message[1] = this->addr_;  // Communication Module Address
//   request_message[2] = data_id;      // Data ID
//   request_message[3] = 0x08;         // Data Length (Fixed)
//   request_message[4] = 0x00;         // Empty Data
//   request_message[5] = 0x00;         //     |
//   request_message[6] = 0x00;         //     |
//   request_message[7] = 0x00;         //     |
//   request_message[8] = 0x00;         //     |
//   request_message[9] = 0x00;         //     |
//   request_message[10] = 0x00;        //     |
//   request_message[11] = 0x00;        // Empty Data

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

fn parse_msg(msg: Bytes) -> Result<()> {
    println!("got {:?}", msg);
    let cksum = msg[0..12].iter().fold(0u8, |a, b| a.wrapping_add(*b));
    if cksum != msg[12] {
        return Err(anyhow!(format!(
            "checksum failed, {} != {}",
            cksum, msg[12]
        )));
    }
    let mut rdr = Cursor::new(&msg[4..]);
    match msg[2] {
        0x90 => {
            let cum_volts = rdr.read_u16::<BigEndian>().unwrap() as f64 / 10.;
            let _ = rdr.read_u16::<BigEndian>().unwrap();
            let current = (rdr.read_u16::<BigEndian>().unwrap() as f64 - 30000.) / 10.;
            let soc = rdr.read_u16::<BigEndian>().unwrap() as f64 / 10.;
            println!("SOC, cum_volts: {cum_volts}, current: {current}, soc: {soc}");
        }
        0x93 => {
            let state = rdr.read_u8().unwrap();
            let charge_mos = rdr.read_u8().unwrap();
            let discharge_mos = rdr.read_u8().unwrap();
            let bms_life = rdr.read_u8().unwrap();
            let mah = rdr.read_u32::<BigEndian>().unwrap() as f64 / 1000.;
            println!("mos status, mah: {mah}, life: {bms_life}");
        }
        _ => {}
    }
    Ok(())
}

async fn do_mqtt(host: String, port: u16, client_id: String) -> Result<()> {
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

    loop {
        let notification = eventloop.poll().await.unwrap();
        let r = handle_notification(&notification, &client).await;
        if r.is_err() {
            println!("handle notification failed with {:?}", r);
        }
    }
}

async fn do_serial(serial_port: String) -> Result<()> {
    let mut port = tokio_serial::new(&serial_port, 9600).open_native_async()?;
    port.set_exclusive(false)
        .with_context(|| format!("failed to set exclusive"))?;
    let mut reader = DalyMsg.framed(port);
    println!("starting serial read");

    let b = make_req(0x90);
    println!("will send {} bytes: {:?}", b.len(), b);
    reader.send(b).await?;

    while let Some(line_result) = reader.next().await {
        let msg = line_result.expect("Failed to read line");
        parse_msg(msg)?;
    }
    Ok(())
}

#[tokio::main(flavor = "current_thread")]
async fn main() -> Result<()> {
    let args = Args::parse();

    let mut set = JoinSet::new();
    set.spawn(do_mqtt(args.mqtt_host, args.mqtt_port, args.mqtt_client_id));
    set.spawn(do_serial(args.serial_port.clone()));
    while let Some(res) = set.join_next().await {
        res??;
    }

    Ok(())
}
