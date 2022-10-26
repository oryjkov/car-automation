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

constexpr size_t message_buffer_size = 32;
CanMessage message_buffer[message_buffer_size];
size_t messages_in_buffer = 0;
size_t messages_consumed = 0;
size_t lost_messages = 0;
size_t get_lost_messages() {
  return lost_messages;
}
// Returns a CanMessage pointer to read into.
CanMessage *alloc_in_buffer() {
  if (messages_in_buffer < message_buffer_size) {
    messages_in_buffer += 1;
  } else {
    lost_messages += 1;
  }
  return &message_buffer[messages_in_buffer-1];
}
void dealloc_in_buffer() {
  if (messages_in_buffer > 0) {
    messages_in_buffer -= 1;
  }
}
CanMessage *consume_from_buffer() {
  if (messages_consumed < messages_in_buffer) {
    messages_consumed += 1;
    return &message_buffer[messages_consumed-1];
  }

  messages_in_buffer = 0;
  return nullptr;
}

void populate_response(Response *rep) {
  rep->has_drops = true;
  rep->drops = get_lost_messages();
  messages_consumed = 0;
  while (CanMessage *m = consume_from_buffer()) {
    if (m == nullptr) {
      break;
    }
    memcpy(&rep->messages_out[rep->messages_out_count], m, sizeof(CanMessage));
    rep->messages_out_count += 1;
  }
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
void print_stats_ser(uint32_t period_ms) {
  if (millis() > stats_print_ms + period_ms) {
    stats_print_ms = millis();
    print_stats(&Serial, &stats);
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
  if (len > send_recv_buffer_size) {
    Serial.println("message too long");
    return 0;
  }

  uint8_t byte_len[2] = {static_cast<uint8_t>((len>>8) & 0xff), static_cast<uint8_t>(len & 0xff)};
  size_t bytes_written = Serial2.write(byte_len, sizeof(byte_len));
  stats.ser_bytes_tx += bytes_written;
  if (bytes_written < sizeof(byte_len)) {
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
size_t recv_buf_over_serial(uint8_t *buf, size_t max_length, uint32_t timeout_ms) {
  stats.ser_rx_err += 1;
  uint32_t rx_start = millis();
  uint16_t message_length;

  size_t bytes_read = 0;
  uint8_t bytes_len[2] = {0, 0};

  while (bytes_read < sizeof(bytes_len) && (millis()-rx_start) < timeout_ms) {
    size_t tmp_read = Serial2.read(bytes_len, sizeof(bytes_len));
    bytes_read += tmp_read;
    stats.ser_bytes_rx += tmp_read;
	  delay(1);
  }
  if (bytes_read < sizeof(bytes_len)) {
    Serial.printf("preamble: read length unexpected: read %d bytes, want 1, after %dms\r\n", bytes_read, millis()-rx_start);
    return 0;
  }
  message_length = (static_cast<uint16_t>(bytes_len[0])<<8) + bytes_len[1];
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
  stats.can_pkt_tx += 1;
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
  stats.can_pkt_rx += 1;
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
