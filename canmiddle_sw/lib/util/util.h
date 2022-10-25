#ifndef _UTIL_H_
#define _UTIL_H_

#include <pb_encode.h>
#include <pb_decode.h>

#include <Arduino.h>
#include "message.pb.h"

CanMessage *alloc_in_buffer();
void dealloc_in_buffer();
size_t get_lost_messages();
void populate_response(Response *rep);

struct Stats {
  uint32_t can_tx;
  uint32_t can_rx;
  uint32_t can_tx_err;
  uint32_t can_rx_err;

  uint32_t ser_bytes_tx;
  uint32_t ser_tx_err;
  uint32_t ser_bytes_rx;
  uint32_t ser_rx_err;

  uint32_t ser_drops;
};
void print_stats_ser(uint32_t period_ms);
Stats *get_stats();

template<typename P>
void print_stats(P *printer, Stats *stats) {
  printer->printf("can: tx/err tx, rx/err rx: %d/%d, %d/%d",
    stats->can_tx, stats->can_tx_err, stats->can_rx, stats->can_rx_err);
  if (stats->ser_bytes_tx > 0 || stats->ser_bytes_rx > 0) {
    printer->printf(", ser: tx/err tx, rx/err rx: %d/%d, %d/%d",
    stats->ser_bytes_tx, stats->ser_tx_err, stats->ser_bytes_rx, stats->ser_rx_err);
  }
  printer->printf(" ser drops: %d\r\n", stats->ser_drops);
}


constexpr size_t send_recv_buffer_size = 1024;
size_t send_buf_over_serial(uint8_t *buf, size_t len);
size_t recv_buf_over_serial(uint8_t *buf, size_t max_length, uint32_t timeout_ms = 100);

template<typename M>
size_t send_over_serial(const M &msg, const pb_msgdesc_t *fields) {
  uint8_t buffer[send_recv_buffer_size];
  size_t message_length;

  pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

  bool status = pb_encode(&stream, fields, &msg);
  if (!status) {
    Serial.printf("Encoding failed: %s\r\n", PB_GET_ERROR(&stream));
    return 0;
  }
  message_length = stream.bytes_written;

  if (message_length <= 0) {
    Serial.println("zero length message");
    return 0;
  }
  if (message_length > sizeof(buffer)) {
    Serial.println("message too long");
    return 0;
  }
  send_buf_over_serial(buffer, message_length);
  return message_length;
}

template <typename M>
size_t recv_over_serial(M *msg, const pb_msgdesc_t *fields) {
  uint8_t buf[send_recv_buffer_size];

  size_t message_length = recv_buf_over_serial(buf, sizeof(buf));
  if (message_length <= 0) {
    return 0;
  }
  pb_istream_t stream = pb_istream_from_buffer(buf, message_length);
        
  bool status = pb_decode(&stream, fields, msg);
  if (!status) {
    Serial.printf("Decoding failed: %s\r\n", PB_GET_ERROR(&stream));
    return 0;
  }

  return message_length;
}

bool issue_rpc(const Request &req, Response *rep);
bool send_over_can(const CanMessage &msg);
bool recv_over_can(CanMessage *msg);

template <typename P>
void dump_msg(P *printer, const CanMessage &msg) {
  if (msg.has_prop) {
    printer->printf("prop: %d, ", msg.prop);
  }
  if (msg.has_extended) {
    printer->printf("ext: %d, ", msg.extended);
  }
  if (msg.has_value) {
    printer->printf("data_len: %d, data: [ ", msg.value.size);
    for (int i = 0; i < msg.value.size; i++) {
      printer->printf("0x%x, ", msg.value.bytes[i]);
    }
    printer->print("]");
  }
  printer->println();
}

void make_message(uint32_t parity, CanMessage *msg);
#endif  // _UTIL_H_