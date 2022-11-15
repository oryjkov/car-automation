#include <gtest/gtest.h>

#include "esp_abstraction.h"
#include "model.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Expectation;
using ::testing::Field;

void HandlePropUpdate(uint32_t prop, size_t len, const uint8_t *new_v, const uint8_t *old_v) {}

typedef Model<MockLockAbstraction, MockQueueAbstraction> TestModel;

TEST(ModelTest, GetBit) {
  ASSERT_EQ(getBit(0, (uint8_t *)"\x80"), 1);
  ASSERT_EQ(getBit(7, (uint8_t *)"\x01"), 1);
  ASSERT_EQ(getBit(8, (uint8_t *)"\x00\x80"), 1);
  ASSERT_EQ(getBit(15, (uint8_t *)"\x00\x01"), 1);
  ASSERT_EQ(getBit(3, (uint8_t *)"\x10"), 1);
  ASSERT_EQ(getBit(11, (uint8_t *)"\x00\x10"), 1);
}

TEST(ModelTest, GetBits) {
  ASSERT_EQ(getBits(0, 1, (uint8_t *)"\x80"), 1);
  ASSERT_EQ(getBits(2, 3, (uint8_t *)"\x31"), 6);
}

TEST(ModelTest, SendState) {
  // clang-format off
  TestModel m = {.props = {
	 {.prop = 0x052a, .send_delay_ms = 40, .val = {.size = 1, .bytes = { 0x3a, }}},
	 {.prop = 0x053a, .send_delay_ms = 0, .val = {.size = 1, .bytes = { 0x3a, }}},
  }};
  // clang-format on
  MockLockAbstraction &lock_abs = m.lock_abs;
  MockQueueAbstraction &queue_abs = m.queue_abs;
  EXPECT_CALL(lock_abs, Delay(m.props[0].send_delay_ms));
  EXPECT_CALL(lock_abs, Delay(m.props[1].send_delay_ms));
  EXPECT_CALL(queue_abs,
              Enqueue(AllOf(Field(&CanMessage::has_prop, true), Field(&CanMessage::prop, 0x052a))));
  EXPECT_CALL(queue_abs,
              Enqueue(AllOf(Field(&CanMessage::has_prop, true), Field(&CanMessage::prop, 0x053a))));

  EXPECT_TRUE(m.SendState(1));
}

// Tests that only a delay is triggered for the special property.
TEST(ModelTest, SendStateDelay) {
  // clang-format off
  TestModel m = {.props = {
	 {.prop = DELAY_ONLY_PROP, .send_delay_ms = 40, .val = {}},
	 {.prop = 0x053a, .send_delay_ms = 0, .val = {.size = 1, .bytes = { 0x3a, }}},
  }};
  // clang-format on
  MockLockAbstraction &lock_abs = m.lock_abs;
  MockQueueAbstraction &queue_abs = m.queue_abs;
  EXPECT_CALL(lock_abs, Delay(m.props[0].send_delay_ms));
  EXPECT_CALL(lock_abs, Delay(m.props[1].send_delay_ms));
  EXPECT_CALL(queue_abs,
              Enqueue(AllOf(Field(&CanMessage::has_prop, true), Field(&CanMessage::prop, 0x053a))));

  EXPECT_TRUE(m.SendState(1));
}

TEST(ModelTest, SendStateRepetitions) {
  // clang-format off
  TestModel m = {.props = {
{ .prop = 0x1b00002c, .send_delay_ms =  22, .val = { .size = 8, .bytes = { 0x2c, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, } }, .iteration = 1 },
{ .prop = 0x1b00002d, .send_delay_ms =  40, .val = { .size = 8, .bytes = { 0x2c, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, } }, .iteration = 2 },
{ .prop = 0x1b00002c, .send_delay_ms = 200, .val = { .size = 8, .bytes = { 0x2c, 0x00, 0x01, 0x01, 0x04, 0x00, 0x00, 0x00, } } },
  }};
  // clang-format on
  MockLockAbstraction &lock_abs = m.lock_abs;
  MockQueueAbstraction &queue_abs = m.queue_abs;
  EXPECT_CALL(lock_abs, Delay(m.props[0].send_delay_ms));
  EXPECT_CALL(
      queue_abs,
      Enqueue(AllOf(Field(&CanMessage::has_prop, true), Field(&CanMessage::prop, 0x1b00002c),
                    Field(&CanMessage::has_extended, true), Field(&CanMessage::extended, true))));
  EXPECT_TRUE(m.SendState(1));

  EXPECT_CALL(lock_abs, Delay(m.props[1].send_delay_ms));
  EXPECT_CALL(
      queue_abs,
      Enqueue(AllOf(Field(&CanMessage::has_prop, true), Field(&CanMessage::prop, 0x1b00002d),
                    Field(&CanMessage::has_extended, true), Field(&CanMessage::extended, true))));
  EXPECT_TRUE(m.SendState(2));

  EXPECT_CALL(lock_abs, Delay(m.props[2].send_delay_ms));
  EXPECT_CALL(
      queue_abs,
      Enqueue(AllOf(Field(&CanMessage::has_prop, true), Field(&CanMessage::prop, 0x1b00002c),
                    Field(&CanMessage::has_extended, true), Field(&CanMessage::has_prop, true))));
  EXPECT_TRUE(m.SendState(3));

  EXPECT_CALL(lock_abs, Delay(m.props[2].send_delay_ms));
  EXPECT_CALL(
      queue_abs,
      Enqueue(AllOf(Field(&CanMessage::has_prop, true), Field(&CanMessage::prop, 0x1b00002c),
                    Field(&CanMessage::has_extended, true), Field(&CanMessage::has_prop, true))));
  EXPECT_TRUE(m.SendState(4));
}

TEST(ModelTest, Update) {
  // clang-format off
  TestModel m = {.props = {
	 {.prop = 0x052a, .send_delay_ms = 40, .val = {.size = 1, .bytes = { 0x3a, }}},
	 {.prop = 0x053a, .send_delay_ms = 0, .val = {.size = 1, .bytes = { 0x3a, }}},
  }};
  // clang-format on
  MockLockAbstraction &lock_abs = m.lock_abs;
  MockQueueAbstraction &queue_abs = m.queue_abs;

  CanMessage msg = {
      .has_prop = true,
      .prop = 0x52a,
      .has_value = true,
      .value = {.size = 2, .bytes = {1, 2}},
  };
  EXPECT_FALSE(m.UpdateState(msg));  // wrong value length

  msg.value.size = 1;
  EXPECT_TRUE(m.UpdateState(msg));

  msg.prop = 1;
  EXPECT_FALSE(m.UpdateState(msg));  // unkown property
}