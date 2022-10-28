#include <gtest/gtest.h>

#include "esp_abstraction.h"
#include "model.h"

using ::testing::_;
using ::testing::Expectation;
using ::testing::Field;
using ::testing::AllOf;

TEST(ModelTest, SendState) {
  // clang-format off
  Model<MockEspAbstraction> m = {.props = {
	 {.prop = 0x052a, .send_delay_ms = 40, .val = {.size = 1, .bytes = { 0x3a, }}},
	 {.prop = 0x053a, .send_delay_ms = 0, .val = {.size = 1, .bytes = { 0x3a, }}},
  }};
  // clang-format on
  MockEspAbstraction &esp = m.esp;
  EXPECT_CALL(esp, Delay(m.props[0].send_delay_ms));
  EXPECT_CALL(esp, Delay(m.props[1].send_delay_ms));
  EXPECT_CALL(esp, Enqueue(AllOf(
    Field(&CanMessage::has_prop, true),
    Field(&CanMessage::prop, 0x052a)
    )));
  EXPECT_CALL(esp, Enqueue(AllOf(
    Field(&CanMessage::has_prop, true),
    Field(&CanMessage::prop, 0x053a)
    )));

  EXPECT_TRUE(m.SendState(1));
}

TEST(ModelTest, SendStateRepetitions) {
  // clang-format off
  Model<MockEspAbstraction> m = {.props = {
{ .prop = 0x1b00002c, .send_delay_ms =  22, .val = { .size = 8, .bytes = { 0x2c, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, } }, .iteration = 1 },
{ .prop = 0x1b00002d, .send_delay_ms =  40, .val = { .size = 8, .bytes = { 0x2c, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, } }, .iteration = 2 },
{ .prop = 0x1b00002c, .send_delay_ms = 200, .val = { .size = 8, .bytes = { 0x2c, 0x00, 0x01, 0x01, 0x04, 0x00, 0x00, 0x00, } } },
  }};
  // clang-format on
  MockEspAbstraction &esp = m.esp;
  EXPECT_CALL(esp, Delay(m.props[0].send_delay_ms));
  EXPECT_CALL(esp, Enqueue(AllOf(
    Field(&CanMessage::has_prop, true),
    Field(&CanMessage::prop, 0x1b00002c),
    Field(&CanMessage::has_extended, true),
    Field(&CanMessage::extended, true)
    )));
  EXPECT_TRUE(m.SendState(1));

  EXPECT_CALL(esp, Delay(m.props[1].send_delay_ms));
  EXPECT_CALL(esp, Enqueue(AllOf(
    Field(&CanMessage::has_prop, true),
    Field(&CanMessage::prop, 0x1b00002d),
    Field(&CanMessage::has_extended, true),
    Field(&CanMessage::extended, true)
    )));
  EXPECT_TRUE(m.SendState(2));

  EXPECT_CALL(esp, Delay(m.props[2].send_delay_ms));
  EXPECT_CALL(esp, Enqueue(AllOf(
    Field(&CanMessage::has_prop, true),
    Field(&CanMessage::prop, 0x1b00002c),
    Field(&CanMessage::has_extended, true),
    Field(&CanMessage::has_prop, true)
    )));
  EXPECT_TRUE(m.SendState(3));

  EXPECT_CALL(esp, Delay(m.props[2].send_delay_ms));
  EXPECT_CALL(esp, Enqueue(AllOf(
    Field(&CanMessage::has_prop, true),
    Field(&CanMessage::prop, 0x1b00002c),
    Field(&CanMessage::has_extended, true),
    Field(&CanMessage::has_prop, true)
    )));
  EXPECT_TRUE(m.SendState(4));
}

TEST(ModelTest, Update) {
  // clang-format off
  Model<MockEspAbstraction> m = {.props = {
	 {.prop = 0x052a, .send_delay_ms = 40, .val = {.size = 1, .bytes = { 0x3a, }}},
	 {.prop = 0x053a, .send_delay_ms = 0, .val = {.size = 1, .bytes = { 0x3a, }}},
  }};
  // clang-format on
  MockEspAbstraction &esp = m.esp;

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

#if defined(ARDUINO)
#include <Arduino.h>

void setup() {
  // should be the same value as for the `test_speed` option in "platformio.ini"
  // default value is test_speed=115200
  Serial.begin(115200);

  ::testing::InitGoogleTest();
}

void loop() {
  // Run tests
  if (RUN_ALL_TESTS())
    ;

  // sleep 1 sec
  delay(1000);
}

#else
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (RUN_ALL_TESTS())
    ;
  // Always return zero-code and allow PlatformIO to parse results
  return 0;
}
#endif