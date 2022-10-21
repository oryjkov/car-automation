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

  Serial2.begin(500000);

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

struct {
  uint32_t can_tx;
  uint32_t can_rx;
  uint32_t can_tx_err;
  uint32_t can_rx_err;

  uint32_t ser_tx;
  uint32_t ser_tx_err;
  uint32_t ser_rx;
  uint32_t ser_rx_err;
} stats;
uint32_t stats_print_ms = 0;
void print_stats(uint32_t period_ms) {
  if (millis() > stats_print_ms + period_ms) {
    stats_print_ms = millis();
    Serial.printf("can: tx/err tx, rx/err rx: %d/%d, %d/%d\r\n",
     stats.can_tx, stats.can_tx_err, stats.can_rx, stats.can_rx_err);
    if (stats.ser_tx > 0 || stats.ser_rx > 0) {
      Serial.printf("ser: tx/err tx, rx/err rx: %d/%d, %d/%d\r\n",
      stats.ser_tx, stats.ser_tx_err, stats.ser_rx, stats.ser_rx_err);
    }
  }
}


void loop_slave() {
  int packetSize = CAN.parsePacket();

  if (packetSize > 0) {
    CanMessage msg = CanMessage_init_zero;
    stats.can_rx += 1;
    if (!recv_over_can(&msg)) {
      stats.can_rx_err += 1;
    }

    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));
    stats.ser_tx += 1;
    if (send_over_serial(msg)==0) {
      stats.ser_tx_err += 1;
    }
    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));
#if DEBUG
    Serial.print("message from CAN: ");
    dump_msg(msg);
#endif
  }

  if (Serial2.available()) {
    CanMessage msg;
    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));
    stats.ser_rx += 1;
    size_t cnt = recv_over_serial(&msg);
    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));
    if (cnt <= 0) {
      stats.ser_rx_err += 1;
      //Serial.println("zero len message");
      return;
    }
    stats.can_tx += 1;
    bool status = send_over_can(msg);
    if (!status) {
      stats.can_tx_err += 1;
      //Serial.println("can send failed");
      return;
    }
#if DEBUG
    Serial.print("message from Serial: ");
    dump_msg(msg);
#endif
  }
  print_stats(10000);
}

uint32_t last_send_ms = 0;
void loop_debugger() {
  int packetSize = CAN.parsePacket();
  if (packetSize > 0) {
    CanMessage msg = CanMessage_init_zero;
    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));
    stats.can_rx += 1;
    if (!recv_over_can(&msg)) {
      stats.can_rx_err += 1;
    }
    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));
  }

  if (millis() > last_send_ms+10) {
    CanMessage msg = CanMessage_init_zero;

    msg.prop = 0x11;
    msg.has_prop = true;
    msg.has_value = true;
    msg.value.size = 8;
    msg.value.bytes[0] = 'g';
    msg.value.bytes[1] = 'e';
    msg.value.bytes[2] = 'n';
    msg.value.bytes[3] = 'e';
    msg.value.bytes[4] = 'r';
    msg.value.bytes[5] = 'a';
    msg.value.bytes[6] = 't';
    msg.value.bytes[7] = 'o';

    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));
    stats.can_tx += 1;
    bool status = send_over_can(msg);
    if (!status) {
      //Serial.println("can send failed");
      stats.can_tx_err += 1;
    }
    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));

    last_send_ms=millis();
  }

  print_stats(1000);
}

void loop_mirror(uint32_t parity) {
  int packetSize = CAN.parsePacket();
  if (packetSize > 0) {
    CanMessage msg = CanMessage_init_zero;
    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));
    stats.can_rx += 1;
    if (!recv_over_can(&msg)) {
      stats.can_rx_err += 1;
    }
    if (msg.has_prop && msg.prop % 2 == parity) {
      stats.can_tx += 1;
      if (!send_over_can(msg)) {
        stats.can_tx_err += 1;
      }
    }
    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));
  }
  print_stats(1000);
}

void loop_master() {
  int packetSize = CAN.parsePacket();

  if (packetSize > 0) {
    CanMessage msg = CanMessage_init_zero;

    stats.can_rx += 1;
    if (!recv_over_can(&msg)) {
      stats.can_rx_err += 1;
    }
    stats.ser_tx += 1;
    if (send_over_serial(msg) == 0) {
      stats.ser_tx_err += 1;
    }
  }

  if (Serial2.available()) {
    int cnt = 0;
    CanMessage msg;
    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));
    stats.ser_rx += 1;
    cnt = recv_over_serial(&msg);
    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));
    if (cnt <= 0) {
      stats.ser_rx_err += 1;
      Serial.println("zero len message");
      return;
    }
    stats.can_tx += 1;
    bool status = send_over_can(msg);
    if (!status) {
      stats.can_tx_err += 1;
      Serial.println("can send failed");
      return;
    }
  }

  print_stats(1000);
}

void loop() {
  if (r == MASTER) {
    loop_master();
  } else if (r == SLAVE) {
    loop_slave();
  } else if (r == DEBUGGER) {
    loop_debugger();
  } else if (r == MIRROR_ODD) {
    loop_mirror(1);
  } else {
    while(1) {}
  }
}