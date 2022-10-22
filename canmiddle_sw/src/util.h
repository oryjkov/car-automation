#ifndef _UTIL_H_
#define _UTIL_H_

#include <Arduino.h>
#include "message.pb.h"

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
void print_stats(uint32_t period_ms);

template<typename M>
size_t send_over_serial(const M &msg, const pb_msgdesc_t *fields) {
  stats.ser_tx += 1;
  stats.ser_tx_err += 1;

  uint8_t buffer[255];
  bool status;
  size_t message_length;

  pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

  status = pb_encode(&stream, fields, &msg);
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
  bytes_written = Serial.write(buffer, message_length);
  if (bytes_written < message_length) {
    Serial.printf("write length unexpected: wrote %d bytes, want %d\r\n", bytes_written, message_length);
    return 0;
  }
  Serial.print("sent over serial: ");
  Serial.println(bytes_written+1);
  Serial2.flush(true);
  stats.ser_tx_err -= 1;
  return message_length;
}

template <typename M>
size_t recv_over_serial(M *msg, const pb_msgdesc_t *fields) {
  uint32_t timeout_ms = 1000;
  stats.ser_rx += 1;
  stats.ser_rx_err += 1;
  uint8_t buffer[255];
  bool status;
  uint8_t message_length;
  uint32_t rx_start = millis();

  size_t bytes_read;

  while (bytes_read < 1 && millis()-rx_start < timeout_ms) {
	size_t tmp_read = Serial2.read(&message_length, 1);
	bytes_read += tmp_read;
  }
  if (bytes_read < 1) {
    Serial.printf("preamble: read length unexpected: read %d bytes, want 1\r\n", bytes_read);
    return 0;
  }

  bytes_read = 0;
  while (bytes_read < message_length && millis()-rx_start < timeout_ms) {
  	size_t tmp_read = Serial2.read(buffer+bytes_read, message_length-bytes_read);
	bytes_read += tmp_read;
  }
  if (bytes_read < message_length) {
    Serial.printf("message: read length unexpected: read %d bytes, want %d\r\n",
	  bytes_read, message_length);
    return 0;
  }

  pb_istream_t stream = pb_istream_from_buffer(buffer, message_length);
        
  status = pb_decode(&stream, fields, msg);
  if (!status) {
    Serial.printf("Decoding failed: %s\r\n", PB_GET_ERROR(&stream));
    return 0;
  }

  stats.ser_rx_err -= 1;
  return message_length;
}

bool issue_rpc(const Request &req, Response *rep);
bool send_over_can(const CanMessage &msg);
bool recv_over_can(CanMessage *msg);

void dump_msg(const CanMessage &msg);

#endif  // _UTIL_H_