#include <Arduino.h>
#include <CAN.h>

#include <pb_encode.h>
#include <pb_decode.h>

#include "message.pb.h"
#include "util.h"

Stats stats;
Stats *get_stats() {
  return &stats;
}

void make_message(uint32_t parity, CanMessage *msg) {
  msg->has_prop = true;
  if (random(2) == 0) {
    msg->prop = random(1<<11) << 1 + parity;
  } else {
    msg->prop = random(1<<29) << 1 + parity;
    msg->has_extended = true;
    msg->extended = true;
  }
  
  msg->has_value = true;
  msg->value.size = random(9);
  for (int i = 0; i < msg->value.size; i++) {
    msg->value.bytes[i] = random(256);
  }
}


uint32_t stats_print_ms = 0;
void print_stats(uint32_t period_ms) {
  if (millis() > stats_print_ms + period_ms) {
    stats_print_ms = millis();
    Serial.printf("[%08d] can: tx/err tx, rx/err rx: %d/%d, %d/%d",
     stats_print_ms, stats.can_tx, stats.can_tx_err, stats.can_rx, stats.can_rx_err);
    if (stats.ser_bytes_tx > 0 || stats.ser_bytes_rx > 0) {
      Serial.printf(", ser: tx/err tx, rx/err rx: %d/%d, %d/%d",
      stats.ser_bytes_tx, stats.ser_tx_err, stats.ser_bytes_rx, stats.ser_rx_err);
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
    delay(1);
    Serial2.flush(true);
    return false;
  }
  return true;
}

// Returns the number of bytes from buf that were sent.
size_t send_buf_over_serial(uint8_t *buf, size_t len) {
  stats.ser_tx_err += 1;
  if (len <= 0) {
    Serial.println("zero length message");
    return 0;
  }
  if (len > 255) {
    Serial.println("message too long");
    return 0;
  }

  uint8_t byte_len = len & 0xff;
  size_t bytes_written = Serial2.write(&byte_len, 1);
  stats.ser_bytes_tx += bytes_written;
  if (bytes_written < 1) {
    Serial.printf("write length unexpected: read %d bytes, want 1\r\n", bytes_written);
    return 0;
  }
  bytes_written = Serial2.write(buf, len);
  stats.ser_bytes_tx += bytes_written;
  if (bytes_written < len) {
    Serial.printf("write length unexpected: wrote %d bytes, want %d\r\n", bytes_written, len);
    return 0;
  }

  stats.ser_tx_err -= 1;
  return len;
}

// Receives data from the serial port. Up to max_length bytes will be read.
// Returns the number of bytes read in to the buffer.
size_t recv_buf_over_serial(uint8_t *buf, size_t max_length) {
  stats.ser_rx_err += 1;

  uint32_t timeout_ms = 100;
  uint32_t rx_start = millis();
  uint8_t message_length;

  size_t bytes_read = 0;

  while (bytes_read < 1 && (millis()-rx_start) < timeout_ms) {
    size_t tmp_read = Serial2.read(&message_length, 1);
    bytes_read += tmp_read;
    stats.ser_bytes_rx += tmp_read;
	  delay(1);
  }
  if (bytes_read < 1) {
    Serial.printf("preamble: read length unexpected: read %d bytes, want 1, after %dms\r\n", bytes_read, millis()-rx_start);
    return 0;
  }
  if (message_length > max_length) {
	  return 0;
  }

  bytes_read = 0;
  while (bytes_read < message_length && (millis()-rx_start) < timeout_ms) {
  	size_t tmp_read = Serial2.read(buf+bytes_read, message_length-bytes_read);
    bytes_read += tmp_read;
    stats.ser_bytes_rx += tmp_read;
	  delay(1);
  }
  if (bytes_read < message_length) {
    Serial.printf("message: read length unexpected: read %d bytes, want %d after %dms\r\n",
	    bytes_read, message_length, millis()-rx_start);
    return 0;
  }

  stats.ser_rx_err -= 1;
  return message_length;
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
  if (msg.value.size > 8) {
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
