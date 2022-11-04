#ifndef _UTIL_H_
#define _UTIL_H_

#include <Arduino.h>
#include <pb_decode.h>
#include <pb_encode.h>

#include "message.pb.h"
#include "snoop_buffer.h"

#define EXAMPLE_TAG "TWAI Self Test"

bool recv_over_uart(CanMessage *msg);
bool send_over_uart(const CanMessage &msg);

struct Stats {
  uint32_t can_pkt_tx;
  uint32_t can_pkt_rx;
  uint32_t can_tx_err;
  uint32_t can_rx_err;

  uint32_t ser_pkt_tx;
  uint32_t ser_bytes_tx;
  uint32_t ser_tx_err;
  uint32_t ser_pkt_rx;
  uint32_t ser_bytes_rx;
  uint32_t ser_rx_err;

  uint32_t ser_drops;
};
void print_stats_ser(uint32_t period_ms);
Stats *get_stats();

template <typename P>
void print_stats(P *printer, Stats *stats) {
  printer->printf(
      "can: tx %.2f/%.2f, rx: %.2f/%.2f | "
      "ser: tx %.2f/%.2f, rx: %.2f/%.2f | ser drops: %d\r\n",
      float(stats->can_pkt_tx) * 1000.0 / millis(), float(stats->can_tx_err) * 1000.0 / millis(),
      float(stats->can_pkt_rx) * 1000.0 / millis(), float(stats->can_rx_err) * 1000.0 / millis(),
      float(stats->ser_pkt_tx) * 1000.0 / millis(), float(stats->ser_tx_err) * 1000.0 / millis(),
      float(stats->ser_pkt_rx) * 1000.0 / millis(), float(stats->ser_rx_err) * 1000.0 / millis(),
      stats->ser_drops);
}

constexpr size_t send_recv_buffer_size = 1024;

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