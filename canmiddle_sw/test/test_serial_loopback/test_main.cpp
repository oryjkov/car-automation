#include <Arduino.h>
#include <unity.h>

#include "util.h"
#include "HardwareSerial.h"
#include "message.pb.h"

void setUp() {
}

void teadDown() {
}

void test_send_recv() {
  uint8_t buf[10];
  uint8_t buf2[10];
  memcpy(buf, "testbuffer", sizeof(buf));

  TEST_ASSERT_EQUAL(sizeof(buf), send_buf_over_serial(buf, sizeof(buf)));
  Serial2.flush(true);
  TEST_ASSERT_EQUAL(sizeof(buf), recv_buf_over_serial(buf2, sizeof(buf2)));
  TEST_ASSERT_EQUAL(0, memcmp(buf, buf2, sizeof(buf2)));
}

void test_send_recv_message() {
  CanMessage m = CanMessage_init_zero;
  CanMessage m2 = CanMessage_init_zero;
  make_message(1, &m);

  size_t l = send_over_serial(m, CanMessage_fields);
  TEST_ASSERT_GREATER_THAN(1, l);
  Serial2.flush(true);
  TEST_ASSERT_EQUAL(l, recv_over_serial(&m2, CanMessage_fields));
  TEST_ASSERT_EQUAL(0, memcmp(&m, &m2, sizeof(CanMessage)));
}

void test_send_recv_stats() {
  memset(get_stats(), 0, sizeof(Stats));
  uint8_t buf[10];
  uint8_t buf2[10];
  memcpy(buf, "testbuffer", sizeof(buf));

  TEST_ASSERT_EQUAL(sizeof(buf), send_buf_over_serial(buf, sizeof(buf)));
  Serial2.flush(true);
  TEST_ASSERT_EQUAL(sizeof(buf), recv_buf_over_serial(buf2, sizeof(buf2)));
  TEST_ASSERT_EQUAL(11, get_stats()->ser_bytes_tx);
  TEST_ASSERT_EQUAL(get_stats()->ser_bytes_rx, get_stats()->ser_bytes_tx);
  TEST_ASSERT_EQUAL(0, get_stats()->ser_rx_err);
  TEST_ASSERT_EQUAL(0, get_stats()->ser_tx_err);
}


void setup() {
	Serial.begin(115200);
	Serial2.begin(5000000);

	delay(2000);

	UNITY_BEGIN();
	RUN_TEST(test_send_recv);
	RUN_TEST(test_send_recv_message);
	RUN_TEST(test_send_recv_stats);
	UNITY_END();
}

void loop() {

}