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

size_t send_over_serial(const CanMessage &msg);
bool send_over_can(const CanMessage &msg);

size_t recv_over_serial(CanMessage *msg);
bool recv_over_can(CanMessage *msg);

void dump_msg(const CanMessage &msg);

#endif  // _UTIL_H_