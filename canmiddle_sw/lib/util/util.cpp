#include "util.h"

#include <Arduino.h>
#include <pb_decode.h>
#include <pb_encode.h>

#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "message.pb.h"

SnoopBuffer snoop_buffer;
SnoopBuffer *get_snoop_buffer() { return &snoop_buffer; }

bool add_to_snoop_buffer(const SnoopData &s) {
  size_t start_from = snoop_buffer.position;
  uint8_t *length_field = snoop_buffer.buffer + snoop_buffer.position;
  start_from += 1;
  pb_ostream_t stream =
      pb_ostream_from_buffer(snoop_buffer.buffer + start_from, snoop_buffer_max_size - start_from);
  bool status = pb_encode(&stream, &SnoopData_msg, &s);
  if (!status) {
    ESP_LOGW(EXAMPLE_TAG, "Encoding failed: %s\r\n", PB_GET_ERROR(&stream));
    return false;
  }

  *length_field = stream.bytes_written;
  snoop_buffer.position += stream.bytes_written + 1;
  return true;
}

Stats stats;
Stats *get_stats() { return &stats; }

void make_message(uint32_t parity, CanMessage *msg) {
  msg->has_prop = true;
  if (random(2) == 0) {
    msg->prop = random(1 << 11) << 1 + parity;
  } else {
    msg->prop = random(1 << 29) << 1 + parity;
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

bool send_over_uart(const CanMessage &msg) {
  uint8_t buffer[send_recv_buffer_size];
  size_t message_length;

  pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

  if (!pb_encode(&stream, &CanMessage_msg, &msg)) {
    ESP_LOGW(EXAMPLE_TAG, "Encoding failed: %s\r\n", PB_GET_ERROR(&stream));
    return false;
  }
  message_length = stream.bytes_written;

  if (message_length <= 0) {
    ESP_LOGW(EXAMPLE_TAG, "zero length message");
    return false;
  }
  if (message_length > sizeof(buffer)) {
    ESP_LOGW(EXAMPLE_TAG, "message too long");
    return false;
  }

  stats.ser_tx_err += 1;
  if (message_length <= 0) {
    ESP_LOGE(EXAMPLE_TAG, "zero length message");
    return false;
  }
  if (message_length > send_recv_buffer_size) {
    ESP_LOGE(EXAMPLE_TAG, "message too long");
    return false;
  }

  uint8_t byte_len = static_cast<uint8_t>(message_length & 0xff);
  int bytes_written = uart_write_bytes(UART_NUM_2, &byte_len, sizeof(byte_len));
  if (bytes_written < 1) {
    ESP_LOGW(EXAMPLE_TAG, "write length unexpected: wrote %d bytes, want %d\r\n", bytes_written, 1);
    return false;
  }
  stats.ser_bytes_tx += bytes_written;

  bytes_written = uart_write_bytes(UART_NUM_2, buffer, message_length);
  if (bytes_written < message_length) {
    ESP_LOGW(EXAMPLE_TAG, "write length unexpected: wrote %d bytes, want %d\r\n", bytes_written,
             message_length);
    return false;
  }
  stats.ser_bytes_tx += bytes_written;
  stats.ser_pkt_tx += 1;

  stats.ser_tx_err -= 1;
  return true;
}

bool recv_over_uart(CanMessage *msg) {
  stats.ser_rx_err += 1;

  size_t bytes_read = 0;
  uint8_t message_length = 0;

  int len = uart_read_bytes(UART_NUM_2, &message_length, 1, portMAX_DELAY);
  if (len != 1) {
    return 0;
  }
  if (message_length > sizeof(CanMessage)) {
    ESP_LOGW(EXAMPLE_TAG, "UART received message is too long: %d", message_length);
    return 0;
  }

  uint8_t buf[send_recv_buffer_size];
  uint32_t rx_start = millis();
  len = uart_read_bytes(UART_NUM_2, buf, message_length, pdMS_TO_TICKS(100));
  if (len < 0) {
    ESP_LOGW(EXAMPLE_TAG, "UART read error: %d", len);
    return 0;
  }
  if (len < message_length) {
    ESP_LOGW(EXAMPLE_TAG,
             "message: read length unexpected: read %d bytes, want %d after "
             "%dms\r\n",
             len, message_length, millis() - rx_start);
    return 0;
  }
  stats.ser_bytes_rx += len;

  pb_istream_t stream = pb_istream_from_buffer(buf, message_length);

  bool status = pb_decode(&stream, &CanMessage_msg, msg);
  if (!status) {
    ESP_LOGW(EXAMPLE_TAG, "Decoding failed: %s", PB_GET_ERROR(&stream));
    return 0;
  }
  stats.ser_pkt_rx += 1;
  stats.ser_rx_err -= 1;
  return message_length;
}
