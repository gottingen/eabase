// Copyright 2023 The EA Authors.
// part of Elastic AI Search
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//

#include "eabase/utility/atomicops.h"     // eabase::atomic<int>
#include "eabase/fiber/butex.h"
#include "eabase/fiber/countdown_event.h"

namespace eabase {

CountdownEvent::CountdownEvent(int initial_count) {
    RELEASE_ASSERT_VERBOSE(initial_count >= 0,
                           "Invalid initial_count=%d",
                           initial_count);
    _butex = butex_create_checked<int>();
    *_butex = initial_count;
    _wait_was_invoked = false;
}

CountdownEvent::~CountdownEvent() {
    butex_destroy(_butex);
}

void CountdownEvent::signal(int sig, bool flush) {
    // Have to save _butex, *this is probably defreferenced by the wait thread
    // which sees fetch_sub
    void* const saved_butex = _butex;
    const int prev = ((eabase::atomic<int>*)_butex)
        ->fetch_sub(sig, eabase::memory_order_release);
    // DON'T touch *this ever after
    if (prev > sig) {
        return;
    }
    LOG_IF(ERROR, prev < sig) << "Counter is over decreased";
    butex_wake_all(saved_butex, flush);
}

int CountdownEvent::wait() {
    _wait_was_invoked = true;
    for (;;) {
        const int seen_counter = 
            ((eabase::atomic<int>*)_butex)->load(eabase::memory_order_acquire);
        if (seen_counter <= 0) {
            return 0;
        }
        if (butex_wait(_butex, seen_counter, NULL) < 0 &&
            errno != EWOULDBLOCK && errno != EINTR) {
            return errno;
        }
    }
}

void CountdownEvent::add_count(int v) {
    if (v <= 0) {
        LOG_IF(ERROR, v < 0) << "Invalid count=" << v;
        return;
    }
    LOG_IF(ERROR, _wait_was_invoked) 
            << "Invoking add_count() after wait() was invoked";
    ((eabase::atomic<int>*)_butex)->fetch_add(v, eabase::memory_order_release);
}

void CountdownEvent::reset(int v) {
    if (v < 0) {
        LOG(ERROR) << "Invalid count=" << v;
        return;
    }
    const int prev_counter =
            ((eabase::atomic<int>*)_butex)
                ->exchange(v, eabase::memory_order_release);
    LOG_IF(ERROR, _wait_was_invoked && prev_counter)
        << "Invoking reset() while count=" << prev_counter;
    _wait_was_invoked = false;
}

int CountdownEvent::timed_wait(const timespec& duetime) {
    _wait_was_invoked = true;
    for (;;) {
        const int seen_counter = 
            ((eabase::atomic<int>*)_butex)->load(eabase::memory_order_acquire);
        if (seen_counter <= 0) {
            return 0;
        }
        if (butex_wait(_butex, seen_counter, &duetime) < 0 &&
            errno != EWOULDBLOCK && errno != EINTR) {
            return errno;
        }
    }
}

}  // namespace eabase
