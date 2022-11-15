#include <gtest/gtest.h>

#include "esp_abstraction.h"
#include "props_logger.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Expectation;
using ::testing::Field;
using ::testing::Return;

typedef PropsLogger<MockLockAbstraction> TestLogger;

TEST(PropsLoggerTest, TestBasic) {
  TestLogger log;
  auto &la = log.lock_abs;

  ASSERT_TRUE(log.log(1, 1, reinterpret_cast<const uint8_t *>("x")));
  log.filter(1);
  ASSERT_FALSE(log.log(1, 1, reinterpret_cast<const uint8_t *>("x")));
  ASSERT_TRUE(log.log(2, 1, reinterpret_cast<const uint8_t *>("x")));
}