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

#include <gtest/gtest.h>
#include <gflags/gflags.h>
#include "eabase/utility/atomicops.h"
#include "eabase/utility/time.h"
#include "eabase/utility/macros.h"
#include "eabase/utility/logging.h"
#include "eabase/utility/thread_local.h"
#include <eabase/fiber/butex.h>
#include "eabase/utility/logging.h"
#include "eabase/fiber/fiber.h"
#include "eabase/fiber/task_control.h"

namespace eabase {
    extern TaskControl* g_task_control;
}

namespace {
void* dummy(void*) {
    return NULL;
}

TEST(FiberTest, setconcurrency) {
    ASSERT_EQ(8 + FIBER_EPOLL_THREAD_NUM, (size_t)fiber_getconcurrency());
    ASSERT_EQ(EINVAL, fiber_setconcurrency(FIBER_MIN_CONCURRENCY - 1));
    ASSERT_EQ(EINVAL, fiber_setconcurrency(0));
    ASSERT_EQ(EINVAL, fiber_setconcurrency(-1));
    ASSERT_EQ(EINVAL, fiber_setconcurrency(FIBER_MAX_CONCURRENCY + 1));
    ASSERT_EQ(0, fiber_setconcurrency(FIBER_MIN_CONCURRENCY));
    ASSERT_EQ(FIBER_MIN_CONCURRENCY, fiber_getconcurrency());
    ASSERT_EQ(0, fiber_setconcurrency(FIBER_MIN_CONCURRENCY + 1));
    ASSERT_EQ(FIBER_MIN_CONCURRENCY + 1, fiber_getconcurrency());
    ASSERT_EQ(0, fiber_setconcurrency(FIBER_MIN_CONCURRENCY));  // smaller value
    fiber_t th;
    ASSERT_EQ(0, fiber_start(&th, NULL, dummy, NULL));
    ASSERT_EQ(FIBER_MIN_CONCURRENCY + 1, fiber_getconcurrency());
    ASSERT_EQ(0, fiber_setconcurrency(FIBER_MIN_CONCURRENCY + 5));
    ASSERT_EQ(FIBER_MIN_CONCURRENCY + 5, fiber_getconcurrency());
    ASSERT_EQ(EPERM, fiber_setconcurrency(FIBER_MIN_CONCURRENCY + 1));
    ASSERT_EQ(FIBER_MIN_CONCURRENCY + 5, fiber_getconcurrency());
}

static eabase::atomic<int> *odd;
static eabase::atomic<int> *even;

static eabase::atomic<int> nfibers(0);
static eabase::atomic<int> npthreads(0);
static BAIDU_THREAD_LOCAL bool counted = false;
static eabase::atomic<bool> stop (false);

static void *odd_thread(void *) {
    nfibers.fetch_add(1);
    while (!stop) {
        if (!counted) {
            counted = true;
            npthreads.fetch_add(1);
        }
        eabase::butex_wake_all(even);
        eabase::butex_wait(odd, 0, NULL);
    }
    return NULL;
}

static void *even_thread(void *) {
    nfibers.fetch_add(1);
    while (!stop) {
        if (!counted) {
            counted = true;
            npthreads.fetch_add(1);
        }
        eabase::butex_wake_all(odd);
        eabase::butex_wait(even, 0, NULL);
    }
    return NULL;
}

TEST(FiberTest, setconcurrency_with_running_fiber) {
    odd = eabase::butex_create_checked<eabase::atomic<int> >();
    even = eabase::butex_create_checked<eabase::atomic<int> >();
    ASSERT_TRUE(odd != NULL && even != NULL);
    *odd = 0;
    *even = 0;
    std::vector<fiber_t> tids;
    const int N = 500;
    for (int i = 0; i < N; ++i) {
        fiber_t tid;
        fiber_start_lazy(&tid, &FIBER_ATTR_SMALL, odd_thread, NULL);
        tids.push_back(tid);
        fiber_start_lazy(&tid, &FIBER_ATTR_SMALL, even_thread, NULL);
        tids.push_back(tid);
    }
    for (int i = 100; i <= N; ++i) {
        ASSERT_EQ(0, fiber_setconcurrency(i));
        ASSERT_EQ(i, fiber_getconcurrency());
    }
    usleep(1000 * N);
    *odd = 1;
    *even = 1;
    stop =  true;
    eabase::butex_wake_all(odd);
    eabase::butex_wake_all(even);
    for (size_t i = 0; i < tids.size(); ++i) {
        fiber_join(tids[i], NULL);
    }
    LOG(INFO) << "All fibers has quit";
    ASSERT_EQ(2*N, nfibers);
    // This is not necessarily true, not all workers need to run sth.
    //ASSERT_EQ(N, npthreads);
    LOG(INFO) << "Touched pthreads=" << npthreads;
}

void* sleep_proc(void*) {
    usleep(100000);
    return NULL;
}

void* add_concurrency_proc(void*) {
    fiber_t tid;
    fiber_start_lazy(&tid, &FIBER_ATTR_SMALL, sleep_proc, NULL);
    fiber_join(tid, NULL);
    return NULL;
}

bool set_min_concurrency(int num) {
    std::stringstream ss;
    ss << num;
    std::string ret = GFLAGS_NS::SetCommandLineOption("fiber_min_concurrency", ss.str().c_str());
    return !ret.empty();
}

int get_min_concurrency() {
    std::string ret;
    GFLAGS_NS::GetCommandLineOption("fiber_min_concurrency", &ret);
    return atoi(ret.c_str());
}

TEST(FiberTest, min_concurrency) {
    ASSERT_EQ(1, set_min_concurrency(-1)); // set min success
    ASSERT_EQ(1, set_min_concurrency(0)); // set min success
    ASSERT_EQ(0, get_min_concurrency());
    int conn = fiber_getconcurrency();
    int add_conn = 100;

    ASSERT_EQ(0, set_min_concurrency(conn + 1)); // set min failed
    ASSERT_EQ(0, get_min_concurrency());

    ASSERT_EQ(1, set_min_concurrency(conn - 1)); // set min success
    ASSERT_EQ(conn - 1, get_min_concurrency());

    ASSERT_EQ(EINVAL, fiber_setconcurrency(conn - 2)); // set max failed
    ASSERT_EQ(0, fiber_setconcurrency(conn + add_conn + 1)); // set max success
    ASSERT_EQ(0, fiber_setconcurrency(conn + add_conn)); // set max success
    ASSERT_EQ(conn + add_conn, fiber_getconcurrency());
    ASSERT_EQ(conn, eabase::g_task_control->concurrency());

    ASSERT_EQ(1, set_min_concurrency(conn + 1)); // set min success
    ASSERT_EQ(conn + 1, get_min_concurrency());
    ASSERT_EQ(conn + 1, eabase::g_task_control->concurrency());

    std::vector<fiber_t> tids;
    for (int i = 0; i < conn; ++i) {
        fiber_t tid;
        fiber_start_lazy(&tid, &FIBER_ATTR_SMALL, sleep_proc, NULL);
        tids.push_back(tid);
    }
    for (int i = 0; i < add_conn; ++i) {
        fiber_t tid;
        fiber_start_lazy(&tid, &FIBER_ATTR_SMALL, add_concurrency_proc, NULL);
        tids.push_back(tid);
    }
    for (size_t i = 0; i < tids.size(); ++i) {
        fiber_join(tids[i], NULL);
    }
    ASSERT_EQ(conn + add_conn, fiber_getconcurrency());
    ASSERT_EQ(conn + add_conn, eabase::g_task_control->concurrency());
}

int current_tag(int tag) {
    std::stringstream ss;
    ss << tag;
    std::string ret = GFLAGS_NS::SetCommandLineOption("fiber_current_tag", ss.str().c_str());
    return !(ret.empty());
}

TEST(FiberTest, current_tag) {
    ASSERT_EQ(false, current_tag(-1));
    ASSERT_EQ(true, current_tag(0));
    ASSERT_EQ(false, current_tag(1));
}

int concurrency_by_tag(int num) {
    std::stringstream ss;
    ss << num;
    std::string ret =
        GFLAGS_NS::SetCommandLineOption("fiber_concurrency_by_tag", ss.str().c_str());
    return !(ret.empty());
}

TEST(FiberTest, concurrency_by_tag) {
    ASSERT_EQ(concurrency_by_tag(1), true);
    ASSERT_EQ(concurrency_by_tag(1), false);
    auto con = fiber_getconcurrency_by_tag(0);
    ASSERT_EQ(concurrency_by_tag(con), true);
    ASSERT_EQ(concurrency_by_tag(con + 1), false);
    fiber_setconcurrency(con + 1);
    ASSERT_EQ(concurrency_by_tag(con + 1), true);
}

} // namespace
