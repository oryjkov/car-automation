use anyhow::{anyhow, Result};
use clap::Parser;
use reqwest::header::{HeaderMap, HeaderValue};
use std::collections::HashMap;
use std::io::Cursor;
use xml::reader::{EventReader, XmlEvent};

#[derive(Parser, Debug)]
#[command(version, about, long_about = None)]
struct Args {
    #[arg(long, default_value = "http://192.168.8.1")]
    url: String,
    #[arg(long, default_value = "")]
    pin: String,
}

#[tokio::main(flavor = "current_thread")]
async fn main() -> Result<()> {
    let args = Args::parse();

    let status_url = format!("{}/api/monitoring/status", args.url);
    let session_url = format!("{}/api/webserver/SesTokInfo", args.url);
    let pin_url = format!("{}/api/pin/operate", args.url);

    let client = reqwest::Client::builder().build()?;
    let session_res = client.get(session_url).send().await?;
    let session_xml = session_res.text().await?;
    println!("{}", session_xml);

    let h = xml_to_map(&session_xml)?;
    let mut headers = HeaderMap::new();
    headers.insert(
        "Cookie",
        HeaderValue::from_str(h.get("SesInfo").unwrap()).unwrap(),
    );
    headers.insert(
        "__RequestVerificationToken",
        HeaderValue::from_str(h.get("TokInfo").unwrap()).unwrap(),
    );
    println!("headers: {:?}", headers);

    let status_xml = client
        .get(status_url)
        .headers(headers.clone())
        .send()
        .await?
        .text()
        .await?;
    let h = xml_to_map(&status_xml)?;
    println!("status: {:?}", h);

    if h.get("SimStatus") == Some(&"1".to_string()) {
        println!("already connected");
        return Ok(());
    }

    let payload = format!(
        r#"<?xml version="1.0" encoding="UTF-8"?><request><OperateType>0</OperateType><CurrentPin>{}</CurrentPin><NewPin></NewPin><PukCode></PukCode></request>"#,
        args.pin
    );
    let pin_res = client
        .post(pin_url)
        .body(payload)
        .headers(headers.clone())
        .send()
        .await?;
    println!("{:?}", pin_res);
    let pin_text = pin_res.text().await?;
    println!("{:?}", pin_text);
    Ok(())
}

fn xml_to_map(xml: &str) -> Result<HashMap<String, String>> {
    let parser = EventReader::new(Cursor::new(xml));
    let mut h = HashMap::new();
    let mut save_fn: Option<Box<dyn FnMut(String, &mut HashMap<String, String>)>> = None;
    for e in parser {
        match e {
            Ok(XmlEvent::StartElement { name, .. }) => {
                let key = name.local_name.clone();
                save_fn.replace(Box::new(move |value, h| {
                    h.entry(key.to_string()).or_insert_with(|| value);
                }));
            }
            Ok(XmlEvent::EndElement { .. }) => {
                save_fn = None;
            }
            Ok(XmlEvent::Characters(x)) => {
                if let Some(mut f) = save_fn.take() {
                    f(x, &mut h);
                }
            }
            Err(e) => {
                return Err(anyhow!(format!("xml parse failed: {:?}", e)));
            }
            _ => {}
        }
    }
    Ok(h)
}
