#include <Arduino.h>
#include <CAN.h>

#include <pb_encode.h>
#include <pb_decode.h>

#include "message.pb.h"
#include "util.h"

uint32_t stats_print_ms = 0;
void print_stats(uint32_t period_ms) {
  if (millis() > stats_print_ms + period_ms) {
    stats_print_ms = millis();
    Serial.printf("[%08d] can: tx/err tx, rx/err rx: %d/%d, %d/%d",
     stats_print_ms, stats.can_tx, stats.can_tx_err, stats.can_rx, stats.can_rx_err);
    if (stats.ser_tx > 0 || stats.ser_rx > 0) {
      Serial.printf(", ser: tx/err tx, rx/err rx: %d/%d, %d/%d",
      stats.ser_tx, stats.ser_tx_err, stats.ser_rx, stats.ser_rx_err);
    }
    Serial.printf(" ser drops: %d", stats.ser_drops);
    Serial.println();
  }
}

bool issue_rpc(const Request &req, Response *rep) {
  if (!send_over_serial(req, Request_fields)) {
    return false;
  }
  if (!recv_over_serial(rep, Response_fields)) {
    delay(10);
    Serial2.flush();
    return false;
  }
  return true;
}

bool send_over_can(const CanMessage &msg) {
  stats.can_tx += 1;
  stats.can_tx_err += 1;

  if (!msg.has_prop) {
    Serial.println("can property is required");
    return false;
  }
  if (!msg.has_value) {
    Serial.println("can value is required");
    return false;
  }
  if (msg.value.size < 1  || msg.value.size > 8) {
    Serial.println("can value size is wrong");
    return false;
  }

  if (msg.extended) {
    CAN.beginExtendedPacket(msg.prop, msg.value.size, false);
  } else {
    CAN.beginPacket(msg.prop, msg.value.size, false);
  }
  CAN.write(msg.value.bytes, msg.value.size);
  if (CAN.endPacket() != 1) {
    return false;
  };

  stats.can_tx_err -= 1;
  return true;
}


bool recv_over_can(CanMessage *msg) {
  stats.can_rx += 1;
  stats.can_rx_err += 1;
  if (CAN.packetExtended()) {
    msg->has_extended = true;
    msg->extended = true;
    Serial2.print("extended, ");
  }
  msg->has_prop = true;
  msg->prop = CAN.packetId();
  msg->has_value = true;
  msg->value.size = CAN.packetDlc();
  size_t bytes_read = 0;
  bytes_read = CAN.readBytes(msg->value.bytes, msg->value.size);
  if (bytes_read != msg->value.size) {
    return false;
  }
 
  stats.can_rx_err -= 1;
  return true;
}

void dump_msg(const CanMessage &msg) {
  if (msg.has_prop) {
    Serial.printf("prop: %d, ", msg.prop);
  }
  if (msg.has_extended) {
    Serial.printf("ext: %d, ", msg.extended);
  }
  if (msg.has_value) {
    Serial.printf("data_len: %d, data: [ ", msg.value.size);
    for (int i = 0; i < msg.value.size; i++) {
      Serial.printf("0x%x, ", msg.value.bytes[i]);
    }
    Serial.print("]");
  }
  Serial.println();
}
