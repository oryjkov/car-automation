#include <gtest/gtest.h>

#include "esp_abstraction.h"
#include "snoop_buffer.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Expectation;
using ::testing::Field;
using ::testing::Return;

typedef SnoopBuffer<MockLockAbstraction> TestBuf;

TEST(SnoopTest, TestActivateDeactivate) {
  TestBuf buf(100);
  auto &la = buf.lock_abs;
  EXPECT_CALL(la, Millis()).WillRepeatedly(Return(0));
  ASSERT_FALSE(buf.IsActive());
  ASSERT_FALSE(buf.Snoop(SnoopData{}));
  buf.Activate(100);
  ASSERT_TRUE(buf.IsActive());
  ASSERT_TRUE(buf.Snoop(SnoopData{}));
  ASSERT_EQ(buf.TimeRemainingMs(), 100);
  EXPECT_CALL(la, Millis()).WillRepeatedly(Return(100));
  ASSERT_FALSE(buf.IsActive());
}

TEST(SnoopTest, TestSnoop) {
  TestBuf buf(100);
  auto &la = buf.lock_abs;
  EXPECT_CALL(la, Millis()).WillRepeatedly(Return(0));

  buf.Activate(100);
  ASSERT_EQ(buf.position, 0);
  ASSERT_TRUE(buf.Snoop(SnoopData_init_zero));
  ASSERT_GT(buf.position, 0);
}

TEST(SnoopTest, TestSnoopTooSmall) {
  TestBuf buf(1);
  auto &la = buf.lock_abs;
  EXPECT_CALL(la, Millis()).WillRepeatedly(Return(0));

  buf.Activate(100);
  ASSERT_EQ(buf.position, 0);
  ASSERT_FALSE(buf.Snoop(SnoopData_init_zero));
  ASSERT_EQ(buf.position, 0);
}

TEST(SnoopTest, TestActivateResetsBuffer) {
  TestBuf buf(100);
  auto &la = buf.lock_abs;
  EXPECT_CALL(la, Millis()).WillRepeatedly(Return(0));

  buf.Activate(100);
  ASSERT_TRUE(buf.Snoop(SnoopData_init_zero));
  ASSERT_GT(buf.position, 0);

  EXPECT_CALL(la, Millis()).WillRepeatedly(Return(100));
  ASSERT_TRUE(buf.Activate(200));
  ASSERT_EQ(buf.position, 0);
}