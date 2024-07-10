use anyhow::{Context, Result};
use byteorder::{BigEndian, ReadBytesExt};
use bytes::{BufMut, Bytes, BytesMut};
use clap::Parser;
use futures::{stream::StreamExt, SinkExt};
use std::io::Cursor;
use std::io::{self};
use tokio_serial::SerialPortBuilderExt;
use tokio_util::codec::{Decoder, Encoder};

#[derive(Parser, Debug)]
#[command(version, about, long_about = None)]
struct Args {
    #[arg(long, default_value = "/dev/ttyS0")]
    port: String,
    #[arg(long, default_value = "")]
    pin: String,
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

struct LineCodec;

impl Decoder for LineCodec {
    type Item = Bytes;
    type Error = io::Error;

    fn decode(&mut self, src: &mut BytesMut) -> Result<Option<Self::Item>, Self::Error> {
        let p = src.iter().enumerate().find(|(_, x)| **x == 0xa5u8);
        if let Some(p) = p {
            if src.len() - p.0 == 13 {
                return Ok(Some(src.split().freeze()));
            }
            // println!("read {} bytes", src.len());
        }
        Ok(None)

        // let newline = src.as_ref().iter().position(|b| *b == b'\n');
        // if let Some(n) = newline {
        //     let line = src.split_to(n + 1);
        //     return match str::from_utf8(line.as_ref()) {
        //         Ok(s) => Ok(Some(s.to_string())),
        //         Err(_) => Err(io::Error::new(io::ErrorKind::Other, "Invalid String")),
        //     };
        // }
        // Ok(None)
    }
}

impl Encoder<Bytes> for LineCodec {
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

#[tokio::main(flavor = "current_thread")]
async fn main() -> Result<()> {
    let mut port = tokio_serial::new("/dev/ttyS0", 9600).open_native_async()?;
    port.set_exclusive(false)
        .with_context(|| format!("failed to set exclusive"))?;
    let mut reader = LineCodec.framed(port);
    println!("starting serial read");

    let b = make_req(0x90);
    // let b = make_req(0x61);
    println!("will send {} bytes: {:?}", b.len(), b);
    reader.send(b).await?;

    while let Some(line_result) = reader.next().await {
        let b = line_result.expect("Failed to read line");
        println!("got {:?}", b);
        let cksum = b[0..12].iter().fold(0u8, |a, b| a.wrapping_add(*b));
        if cksum != b[12] {
            println!("checksum failed, {} != {}", cksum, b[12]);
            continue;
        }
        let mut rdr = Cursor::new(&b[4..]);
        match b[2] {
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
    }
    Ok(())
}
