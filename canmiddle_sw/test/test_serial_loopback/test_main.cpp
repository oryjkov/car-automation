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
  delay(10);
  TEST_ASSERT_EQUAL(l, recv_over_serial(&m2, &CanMessage_msg));
  TEST_ASSERT_EQUAL(0, memcmp(&m, &m2, sizeof(CanMessage)));
}

// Verifies that stats are updated.
void test_send_recv_stats() {
  memset(get_stats(), 0, sizeof(Stats));
  uint8_t buf[10];
  uint8_t buf2[10];
  memcpy(buf, "testbuffer", sizeof(buf));

  TEST_ASSERT_EQUAL(sizeof(buf), send_buf_over_serial(buf, sizeof(buf)));
  Serial2.flush(true);
  TEST_ASSERT_EQUAL(sizeof(buf), recv_buf_over_serial(buf2, sizeof(buf2)));
  TEST_ASSERT_EQUAL(12, get_stats()->ser_bytes_tx);
  TEST_ASSERT_EQUAL(get_stats()->ser_bytes_rx, get_stats()->ser_bytes_tx);
  TEST_ASSERT_EQUAL(0, get_stats()->ser_rx_err);
  TEST_ASSERT_EQUAL(0, get_stats()->ser_tx_err);
}

// Tests that many CAN messages are handled.
void test_recv_can_many() {
  auto drops = get_lost_messages();
  CanMessage ms[32];
  for (int i = 0; i < 32; i++) {
    CanMessage *m = alloc_in_buffer();
    *m = CanMessage_init_zero;
    make_message(i % 2, m);
    ms[i] = *m;
  }
  Response rep = Response_init_zero;
  populate_response(&rep);

  TEST_ASSERT_EQUAL(32, rep.messages_out_count);
  for (int i = 0; i < 32; i++) {
    TEST_ASSERT_EQUAL(0, memcmp(&rep.messages_out[i], &ms[i], sizeof(CanMessage)));
  }

  TEST_ASSERT_EQUAL(get_lost_messages(), drops);
}

void test_recv_can_too_many() {
  auto drops = get_lost_messages();
  for (int i = 0; i < 33; i++) {
    CanMessage *m = alloc_in_buffer();
    *m = CanMessage_init_zero;
    make_message(i % 2, m);
  }
  Response rep = Response_init_zero;
  populate_response(&rep);

  TEST_ASSERT_EQUAL(get_lost_messages(), drops+1);
}

void test_send_recv_long() {
  for (int i = 0; i < 32+1; i++) {
    CanMessage *m = alloc_in_buffer();
    *m = CanMessage_init_zero;
    make_message(i % 2, m);
  }
  Response rep = Response_init_zero;
  populate_response(&rep);
  Response rep2 = Response_init_zero;

  size_t l = send_over_serial(rep, Response_fields);
  TEST_ASSERT_GREATER_THAN(1, l);
  delay(10);
  TEST_ASSERT_EQUAL(l, recv_over_serial(&rep2, &Response_msg));
  TEST_ASSERT_EQUAL(32, rep2.messages_out_count);

  TEST_ASSERT_EQUAL(get_lost_messages(), 1);
}

void setup() {
	Serial.begin(115200);
  Serial2.setRxBufferSize(send_recv_buffer_size);
	Serial2.begin(5000000);

	delay(2000);

	UNITY_BEGIN();
	RUN_TEST(test_send_recv);
  RUN_TEST(test_send_recv_message);
  RUN_TEST(test_send_recv_stats);
	RUN_TEST(test_send_recv_long);
	RUN_TEST(test_recv_can_many);
	RUN_TEST(test_recv_can_many);
	RUN_TEST(test_recv_can_too_many);
	UNITY_END();
}

void loop() {

}