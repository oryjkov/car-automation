#ifndef _UTIL_H_
#define _UTIL_H_

#include <Arduino.h>
#include "message.pb.h"

size_t send_over_serial(const CanMessage &msg);
bool send_over_can(const CanMessage &msg);

size_t recv_over_serial(CanMessage *msg);
bool recv_over_can(CanMessage *msg);

void dump_msg(const CanMessage &msg);

#endif  // _UTIL_H_