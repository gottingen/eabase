// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

// Date: 2016/06/03 13:25:44

#include <gtest/gtest.h>
#include <eabase/fiber/countdown_event.h>
#include "eabase/utility/atomicops.h"
#include "eabase/utility/time.h"

namespace {
struct Arg {
    eabase::CountdownEvent event;
    eabase::atomic<int> num_sig;
};

void *signaler(void *arg) {
    Arg* a = (Arg*)arg;
    a->num_sig.fetch_sub(1, eabase::memory_order_relaxed);
    a->event.signal();
    return NULL;
}

TEST(CountdonwEventTest, sanity) {
    for (int n = 1; n < 10; ++n) {
        Arg a;
        a.num_sig = n;
        a.event.reset(n);
        for (int i = 0; i < n; ++i) {
            fiber_t tid;
            ASSERT_EQ(0, fiber_start(&tid, NULL, signaler, &a));
        }
        a.event.wait();
        ASSERT_EQ(0, a.num_sig.load(eabase::memory_order_relaxed));
    }
}

TEST(CountdonwEventTest, timed_wait) {
    eabase::CountdownEvent event;
    int rc = event.timed_wait(eabase::milliseconds_from_now(100));
    ASSERT_EQ(rc, ETIMEDOUT);
    event.signal();
    rc = event.timed_wait(eabase::milliseconds_from_now(100));
    ASSERT_EQ(rc, 0);
    eabase::CountdownEvent event1;
    event1.signal();
    rc = event.timed_wait(eabase::milliseconds_from_now(1));
    ASSERT_EQ(rc, 0);
}
} // namespace
