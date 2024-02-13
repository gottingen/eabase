// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "eabase/utility/barrier_closure.h"

#include "eabase/utility/bind.h"
#include <gtest/gtest.h>

namespace {

void Increment(int* count) { (*count)++; }

TEST(BarrierClosureTest, RunImmediatelyForZeroClosures) {
  int count = 0;
  eabase::Closure doneClosure(eabase::Bind(&Increment, eabase::Unretained(&count)));

  eabase::Closure barrierClosure = eabase::BarrierClosure(0, doneClosure);
  EXPECT_EQ(1, count);
}

TEST(BarrierClosureTest, RunAfterNumClosures) {
  int count = 0;
  eabase::Closure doneClosure(eabase::Bind(&Increment, eabase::Unretained(&count)));

  eabase::Closure barrierClosure = eabase::BarrierClosure(2, doneClosure);
  EXPECT_EQ(0, count);

  barrierClosure.Run();
  EXPECT_EQ(0, count);

  barrierClosure.Run();
  EXPECT_EQ(1, count);
}

}  // namespace
