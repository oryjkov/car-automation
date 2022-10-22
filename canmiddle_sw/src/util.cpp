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
      Serial.printf(", ser: tx/err tx, rx/err rx: %d/%d, %d/%d\r\n",
      stats.ser_tx, stats.ser_tx_err, stats.ser_rx, stats.ser_rx_err);
    } else {
      Serial.println();
    }
  }
}

// Sends msg on Serial2.
size_t send_over_serial(const CanMessage &msg) {
  stats.ser_tx += 1;
  stats.ser_tx_err += 1;

  uint8_t buffer[255];
  bool status;
  size_t message_length;

  pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

  status = pb_encode(&stream, CanMessage_fields, &msg);
  if (!status) {
    Serial.printf("Encoding failed: %s\r\n", PB_GET_ERROR(&stream));
    return 0;
  }
  message_length = stream.bytes_written;

  if (message_length <= 0) {
    Serial.println("zero length message");
    return 0;
  }
  if (message_length > 255) {
    Serial.println("message too long");
    return 0;
  }

  size_t bytes_written = Serial2.write(static_cast<uint8_t>(message_length));
  if (bytes_written < 1) {
    Serial.printf("write length unexpected: read %d bytes, want 1\r\n", bytes_written);
    return 0;
  }
  bytes_written = Serial2.write(buffer, message_length);
  if (bytes_written < message_length) {
    Serial.printf("write length unexpected: wrote %d bytes, want %d\r\n", bytes_written, message_length);
    return 0;
  }
  Serial2.flush(true);
  stats.ser_tx_err -= 1;
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

size_t recv_over_serial(CanMessage *msg) {
  stats.ser_rx += 1;
  stats.ser_rx_err += 1;
  uint8_t buffer[255];
  bool status;
  uint8_t message_length;

  size_t bytes_read;
  bytes_read = Serial2.read(&message_length, 1);
  if (bytes_read < 1) {
    Serial.printf("read length unexpected: read %d bytes, want 1\r\n", bytes_read);
    return 0;
  }
  bytes_read = Serial2.read(buffer, message_length);
  if (bytes_read < message_length) {
    Serial.printf("read length unexpected: read %d bytes, want %d\r\n", bytes_read, message_length);
    return 0;
  }

  pb_istream_t stream = pb_istream_from_buffer(buffer, message_length);
        
  status = pb_decode(&stream, CanMessage_fields, msg);
  if (!status) {
    Serial.printf("Decoding failed: %s\r\n", PB_GET_ERROR(&stream));
    return 0;
  }

  stats.ser_rx_err -= 1;
  return message_length;
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

