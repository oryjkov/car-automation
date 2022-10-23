#include <Arduino.h>
#include <CAN.h>
#include <Preferences.h>

#include <pb_encode.h>
#include <pb_decode.h>

#include "util.h"
#include "message.pb.h"

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
  print_stats(1000);

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
  print_stats(1000);

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

void loop_master() {
  print_stats(3000);

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
  if (rep.has_message_out) {
    bool status = send_over_can(rep.message_out);
    if (!status) {
      return;
    }
  }
}

CanMessage message_buffer = CanMessage_init_zero;
bool has_message = false;
uint32_t lost_messages = 0;

void loop_slave() {
  print_stats(3000);

  int packetSize = CAN.parsePacket();
  if (packetSize > 0) {
    if (recv_over_can(&message_buffer)) {
      if (has_message) {
        lost_messages += 1;
      }
      has_message = true;
    }
  }

  if (Serial2.available()) {
    Request req = Request_init_zero;
    Response rep = Response_init_zero;
    rep.has_drops = true;
    rep.drops = lost_messages;
    size_t recv_len = recv_over_serial(&req, Request_fields);
    if (recv_len <= 0) {
      return;
    }
    if (has_message) {
      memcpy(&rep.message_out, &message_buffer, sizeof(message_buffer));
      rep.has_message_out = true;
      has_message = false;
    }
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