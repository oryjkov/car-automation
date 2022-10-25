#include <Arduino.h>
#include <CAN.h>
#include <Preferences.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <pb_encode.h>
#include <pb_decode.h>

#include "util.h"
#include "message.pb.h"
#include "secrets.h"

AsyncWebServer server(80);

const char* ssid = SECRET_WIFI_SSID;
const char* password = SECRET_WIFI_PASSWORD;

uint8_t *snoop_buffer;
size_t snoop_buffer_position = 0;
uint32_t snoop_end_ms = 0;
constexpr size_t snoop_buffer_max_size = 50 * (1<<10);

Preferences pref;

enum Role: uint32_t {
  MASTER=1,
  SLAVE=2,
  // Sends on CAN and prints what it gets on CAN.
  DEBUGGER=3,
  MIRROR_ODD=4,
};

Role r;

void program_as(Role r) {
  pref.begin("canmiddle", false);
  int x = pref.getUInt("role", 0);
  Serial.printf("read role: %d, want: %d\r\n", x, r);
  if (x == r) {
    Serial.println("already programmed as required.");
    return;
  }
  delay(5000);

  pref.clear();
  pref.putUInt("role", r);
  pref.end();

  Serial.printf("programmed as: %d\r\n", r);
}

void setup() {
  Serial.begin(115200);
  Serial.println("serial port initialized");

  esp_chip_info_t chip;
  esp_chip_info(&chip);
  Serial.printf("chip info: model: %d, cores: %d, revision: %d\r\n", chip.model, chip.cores, chip.revision);
  //program_as(DEBUGGER);

  pref.begin("canmiddle", true);
  r = static_cast<Role>(pref.getUInt("role", 0));

  Serial2.setRxBufferSize(send_recv_buffer_size);
  Serial2.begin(5000000);

  CAN.setPins(GPIO_NUM_26, GPIO_NUM_25);
  if (!CAN.begin(500E3)) {
    Serial.println("can init failed");
    while(1){};
  }
  Serial.println("can port ready");

  pinMode(LED_BUILTIN, OUTPUT);

  Serial.printf("running as role: %d\r\n", r);
  if (r == MASTER) {
    snoop_buffer = (uint8_t *)malloc(snoop_buffer_max_size);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.printf("WiFi Failed!\n");
      return;
    }

    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      AsyncResponseStream *response = request->beginResponseStream("text/plain");
      print_stats(response, get_stats());
      request->send(response);
    });
    server.on("/get_snoop", HTTP_GET, [](AsyncWebServerRequest *request){
      if (snoop_end_ms > millis()) {
        request->send(400, "text/plain", "snoop is still active");
      } else {
        AsyncWebServerResponse *response = request->beginResponse_P(200, "application/octet-stream", snoop_buffer,
          snoop_buffer_position);
        request->send(response);
      }
    });
    server.on("/start_snoop", HTTP_GET, [](AsyncWebServerRequest *request){
      AsyncResponseStream *response = request->beginResponseStream("text/plain");
      if (snoop_end_ms > millis()) {
        response->printf("already running for another %d ms, snooped %d bytes\r\n",
          snoop_end_ms - millis(), snoop_buffer_position);
      } else {
        uint32_t duration_ms = 1000;
        if (request->hasParam("duration_ms")) {
          auto p = request->getParam("duration_ms");
          int d = p->value().toInt();
          if ((d>0) && (d<10000)) {
            duration_ms = d;
          }
        }
        snoop_end_ms = millis() + duration_ms;
        snoop_buffer_position = 0;
        response->printf("Snoop started. will run for %d buffer available: %d", duration_ms,
          snoop_buffer_max_size);
      }
      request->send(response);
    });
    server.begin();

    digitalWrite(LED_BUILTIN, 1);
  } else if (r == SLAVE) {
    digitalWrite(LED_BUILTIN, 0);
  } else if (r == DEBUGGER) {
    digitalWrite(LED_BUILTIN, 0);
  } else if (r == MIRROR_ODD) {
    digitalWrite(LED_BUILTIN, 0);
  } else {
    Serial.printf("role unknown: %d\n", r);
    while(1) {}
  }
}

uint32_t last_send_ms = 0;
void loop_debugger() {
  print_stats_ser(1000);

  int packetSize = CAN.parsePacket();
  if (packetSize > 0) {
    CanMessage msg = CanMessage_init_zero;
    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));
    if (!recv_over_can(&msg)) {
    }
    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));
  }

  if (millis() > last_send_ms+1) {
    CanMessage msg = CanMessage_init_zero;
    make_message(1, &msg);

    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));
    send_over_can(msg);
    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));

    last_send_ms=millis();
  }
}

void loop_mirror(uint32_t parity) {
  print_stats_ser(1000);

  int packetSize = CAN.parsePacket();
  if (packetSize > 0) {
    CanMessage msg = CanMessage_init_zero;
    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));
    bool status = recv_over_can(&msg);
    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));
    if (status <= 0) {
      return;
    }
    if (msg.has_prop && msg.prop % 2 == parity) {
      send_over_can(msg);
    }
  }

  if (millis() > last_send_ms+1) {
    CanMessage msg = CanMessage_init_zero;
    make_message(1-parity, &msg);

    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));
    send_over_can(msg);
    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));

    last_send_ms=millis();
  }

}

size_t add_to_snoop_buffer(uint8_t *buf, size_t start_from, size_t max_size,
  const SnoopData &s) {
  uint8_t *length_field = buf + start_from;
  start_from += 1;
  pb_ostream_t stream = pb_ostream_from_buffer(buf + start_from, max_size - start_from);
  bool status = pb_encode(&stream, &SnoopData_msg, &s);
  if (!status) {
    Serial.printf("Encoding failed: %s\r\n", PB_GET_ERROR(&stream));
    return 0;
  }
  
  *length_field = stream.bytes_written;
  return stream.bytes_written+1;
}

void loop_master() {
  print_stats_ser(3000);

  Request req = Request_init_zero;
  Response rep = Response_init_zero;

  int packetSize = CAN.parsePacket();
  if (packetSize > 0) {
    req.has_message_in = true;
    if (!recv_over_can(&req.message_in)) {
      return;
    }
  }

  if (!issue_rpc(req, &rep)) {
    return;
  }

  if (rep.has_drops) {
    get_stats()->ser_drops = rep.drops;
  }
  for (int i = 0; i < rep.messages_out_count; i++) {
    bool status = send_over_can(rep.messages_out[i]);
    if (!status) {
      return;
    }
  }
  // Snooping..
  if (snoop_end_ms > millis()) {
    auto recv_ms = millis();
    if (req.has_message_in) {
      snoop_buffer_position += add_to_snoop_buffer(snoop_buffer, snoop_buffer_position, snoop_buffer_max_size, 
        {.message = req.message_in, .metadata = {.recv_ms = recv_ms, .source = Metadata_Source_MASTER}});
    }
    for (int i = 0; i < rep.messages_out_count; i++) {
      snoop_buffer_position += add_to_snoop_buffer(snoop_buffer, snoop_buffer_position, snoop_buffer_max_size, 
        {.message = rep.messages_out[i], .metadata = {.recv_ms = recv_ms, .source = Metadata_Source_MASTER}});
    }
  }
}

void loop_slave() {
  print_stats_ser(3000);

  int packetSize = CAN.parsePacket();
  if (packetSize > 0) {
    CanMessage *m = alloc_in_buffer();
    if (!recv_over_can(m)) {
      dealloc_in_buffer();
    }
  }

  if (Serial2.available()) {
    Request req = Request_init_zero;
    size_t recv_len = recv_over_serial(&req, Request_fields);
    if (recv_len <= 0) {
      return;
    }
    Response rep = Response_init_zero;
    populate_response(&rep);
    send_over_serial(rep, Response_fields);

    if (req.has_message_in) {
      send_over_can(req.message_in);
    }
  }
}


void loop() {
  if (r == MASTER) {
    loop_master();
  } else if (r == SLAVE) {
    loop_slave();
  } else if (r == DEBUGGER) {
    loop_mirror(0);
    //loop_debugger();
  } else if (r == MIRROR_ODD) {
    loop_mirror(1);
  } else {
    while(1) {}
  }
}