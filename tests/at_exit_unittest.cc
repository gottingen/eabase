// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "eabase/utility/at_exit.h"

#include <gtest/gtest.h>

namespace {

int g_test_counter_1 = 0;
int g_test_counter_2 = 0;

void IncrementTestCounter1(void*) {
  ++g_test_counter_1;
}

void IncrementTestCounter2(void*) {
  ++g_test_counter_2;
}

void ZeroTestCounters() {
  g_test_counter_1 = 0;
  g_test_counter_2 = 0;
}

void ExpectCounter1IsZero(void* unused) {
  EXPECT_EQ(0, g_test_counter_1);
}

void ExpectParamIsNull(void* param) {
  EXPECT_EQ(static_cast<void*>(NULL), param);
}

void ExpectParamIsCounter(void* param) {
  EXPECT_EQ(&g_test_counter_1, param);
}

}  // namespace

class AtExitTest : public testing::Test {
 private:
  // Don't test the global AtExitManager, because asking it to process its
  // AtExit callbacks can ruin the global state that other tests may depend on.
  eabase::ShadowingAtExitManager exit_manager_;
};

TEST_F(AtExitTest, Basic) {
  ZeroTestCounters();
  eabase::AtExitManager::RegisterCallback(&IncrementTestCounter1, NULL);
  eabase::AtExitManager::RegisterCallback(&IncrementTestCounter2, NULL);
  eabase::AtExitManager::RegisterCallback(&IncrementTestCounter1, NULL);

  EXPECT_EQ(0, g_test_counter_1);
  EXPECT_EQ(0, g_test_counter_2);
  eabase::AtExitManager::ProcessCallbacksNow();
  EXPECT_EQ(2, g_test_counter_1);
  EXPECT_EQ(1, g_test_counter_2);
}

TEST_F(AtExitTest, LIFOOrder) {
  ZeroTestCounters();
  eabase::AtExitManager::RegisterCallback(&IncrementTestCounter1, NULL);
  eabase::AtExitManager::RegisterCallback(&ExpectCounter1IsZero, NULL);
  eabase::AtExitManager::RegisterCallback(&IncrementTestCounter2, NULL);

  EXPECT_EQ(0, g_test_counter_1);
  EXPECT_EQ(0, g_test_counter_2);
  eabase::AtExitManager::ProcessCallbacksNow();
  EXPECT_EQ(1, g_test_counter_1);
  EXPECT_EQ(1, g_test_counter_2);
}

TEST_F(AtExitTest, Param) {
  eabase::AtExitManager::RegisterCallback(&ExpectParamIsNull, NULL);
  eabase::AtExitManager::RegisterCallback(&ExpectParamIsCounter,
                                        &g_test_counter_1);
  eabase::AtExitManager::ProcessCallbacksNow();
}
