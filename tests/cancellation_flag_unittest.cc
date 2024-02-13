// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests of CancellationFlag class.

#include "eabase/utility/synchronization/cancellation_flag.h"

#include "eabase/utility/logging.h"
#include "eabase/utility/synchronization/spin_wait.h"
#include "eabase/utility/time/time.h"
#include <gtest/gtest.h>

namespace eabase {

namespace {

TEST(CancellationFlagTest, SimpleSingleThreadedTest) {
  CancellationFlag flag;
  ASSERT_FALSE(flag.IsSet());
  flag.Set();
  ASSERT_TRUE(flag.IsSet());
}

TEST(CancellationFlagTest, DoubleSetTest) {
  CancellationFlag flag;
  ASSERT_FALSE(flag.IsSet());
  flag.Set();
  ASSERT_TRUE(flag.IsSet());
  flag.Set();
  ASSERT_TRUE(flag.IsSet());
}

}  // namespace

}  // namespace eabase
