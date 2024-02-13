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
#include "eabase/utility/atomicops.h"
#include "eabase/utility/time.h"
#include "eabase/utility/macros.h"
#include "eabase/utility/logging.h"
#include "eabase/fiber/butex.h"
#include "eabase/fiber/task_control.h"
#include "eabase/fiber/task_group.h"
#include "eabase/fiber/fiber.h"
#include "eabase/fiber/unstable.h"
#include "eabase/fiber/interrupt_pthread.h"

namespace eabase {
extern eabase::atomic<TaskControl*> g_task_control;
inline TaskControl* get_task_control() {
    return g_task_control.load(eabase::memory_order_consume);
}
} // namespace eabase

namespace {
TEST(ButexTest, wait_on_already_timedout_butex) {
    uint32_t* butex = eabase::butex_create_checked<uint32_t>();
    ASSERT_TRUE(butex);
    timespec now;
    ASSERT_EQ(0, clock_gettime(CLOCK_REALTIME, &now));
    *butex = 1;
    ASSERT_EQ(-1, eabase::butex_wait(butex, 1, &now));
    ASSERT_EQ(ETIMEDOUT, errno);
}

void* sleeper(void* arg) {
    fiber_usleep((uint64_t)arg);
    return NULL;
}

void* joiner(void* arg) {
    const long t1 = eabase::gettimeofday_us();
    for (fiber_t* th = (fiber_t*)arg; *th; ++th) {
        if (0 != fiber_join(*th, NULL)) {
            LOG(FATAL) << "fail to join thread_" << th - (fiber_t*)arg;
        }
        long elp = eabase::gettimeofday_us() - t1;
        EXPECT_LE(labs(elp - (th - (fiber_t*)arg + 1) * 100000L), 15000L)
            << "timeout when joining thread_" << th - (fiber_t*)arg;
        LOG(INFO) << "Joined thread " << *th << " at " << elp << "us ["
                  << fiber_self() << "]";
    }
    for (fiber_t* th = (fiber_t*)arg; *th; ++th) {
        EXPECT_EQ(0, fiber_join(*th, NULL));
    }
    return NULL;
}

struct A {
    uint64_t a;
    char dummy[0];
};

struct B {
    uint64_t a;
};


TEST(ButexTest, with_or_without_array_zero) {
    ASSERT_EQ(sizeof(B), sizeof(A));
}


TEST(ButexTest, join) {
    const size_t N = 6;
    const size_t M = 6;
    fiber_t th[N+1];
    fiber_t jth[M];
    pthread_t pth[M];
    for (size_t i = 0; i < N; ++i) {
        fiber_attr_t attr = (i == 0 ? FIBER_ATTR_PTHREAD : FIBER_ATTR_NORMAL);
        ASSERT_EQ(0, fiber_start(
                      &th[i], &attr, sleeper,
                      (void*)(100000L/*100ms*/ * (i + 1))));
    }
    th[N] = 0;  // joiner will join tids in `th' until seeing 0.
    for (size_t i = 0; i < M; ++i) {
        ASSERT_EQ(0, fiber_start(&jth[i], NULL, joiner, th));
    }
    for (size_t i = 0; i < M; ++i) {
        ASSERT_EQ(0, pthread_create(&pth[i], NULL, joiner, th));
    }
    
    for (size_t i = 0; i < M; ++i) {
        ASSERT_EQ(0, fiber_join(jth[i], NULL))
            << "i=" << i << " error=" << berror();
    }
    for (size_t i = 0; i < M; ++i) {
        ASSERT_EQ(0, pthread_join(pth[i], NULL));
    }
}


struct WaiterArg {
    int expected_result;
    int expected_value;
    eabase::atomic<int> *butex;
    const timespec *ptimeout;
};

void* waiter(void* arg) {
    WaiterArg * wa = (WaiterArg*)arg;
    const long t1 = eabase::gettimeofday_us();
    const int rc = eabase::butex_wait(
        wa->butex, wa->expected_value, wa->ptimeout);
    const long t2 = eabase::gettimeofday_us();
    if (rc == 0) {
        EXPECT_EQ(wa->expected_result, 0) << fiber_self();
    } else {
        EXPECT_EQ(wa->expected_result, errno) << fiber_self();
    }
    LOG(INFO) << "after wait, time=" << (t2-t1) << "us";
    return NULL;
}

TEST(ButexTest, sanity) {
    const size_t N = 5;
    WaiterArg args[N * 4];
    pthread_t t1, t2;
    eabase::atomic<int>* b1 =
        eabase::butex_create_checked<eabase::atomic<int> >();
    ASSERT_TRUE(b1);
    eabase::butex_destroy(b1);
    
    b1 = eabase::butex_create_checked<eabase::atomic<int> >();
    *b1 = 1;
    ASSERT_EQ(0, eabase::butex_wake(b1));

    WaiterArg *unmatched_arg = new WaiterArg;
    unmatched_arg->expected_value = *b1 + 1;
    unmatched_arg->expected_result = EWOULDBLOCK;
    unmatched_arg->butex = b1;
    unmatched_arg->ptimeout = NULL;
    pthread_create(&t2, NULL, waiter, unmatched_arg);
    fiber_t th;
    ASSERT_EQ(0, fiber_start(&th, NULL, waiter, unmatched_arg));

    const timespec abstime = eabase::seconds_from_now(1);
    for (size_t i = 0; i < 4*N; ++i) {
        args[i].expected_value = *b1;
        args[i].butex = b1;
        if ((i % 2) == 0) {
            args[i].expected_result = 0;
            args[i].ptimeout = NULL;
        } else {
            args[i].expected_result = ETIMEDOUT;
            args[i].ptimeout = &abstime;
        }
        if (i < 2*N) { 
            pthread_create(&t1, NULL, waiter, &args[i]);
        } else {
            ASSERT_EQ(0, fiber_start(&th, NULL, waiter, &args[i]));
        }
    }
    
    sleep(2);
    for (size_t i = 0; i < 2*N; ++i) {
        ASSERT_EQ(1, eabase::butex_wake(b1));
    }
    ASSERT_EQ(0, eabase::butex_wake(b1));
    sleep(1);
    eabase::butex_destroy(b1);
}


struct ButexWaitArg {
    int* butex;
    int expected_val;
    long wait_msec;
    int error_code;
};

void* wait_butex(void* void_arg) {
    ButexWaitArg* arg = static_cast<ButexWaitArg*>(void_arg);
    const timespec ts = eabase::milliseconds_from_now(arg->wait_msec);
    int rc = eabase::butex_wait(arg->butex, arg->expected_val, &ts);
    int saved_errno = errno;
    if (arg->error_code) {
        EXPECT_EQ(-1, rc);
        EXPECT_EQ(arg->error_code, saved_errno);
    } else {
        EXPECT_EQ(0, rc);
    }
    return NULL;
}

TEST(ButexTest, wait_without_stop) {
    int* butex = eabase::butex_create_checked<int>();
    *butex = 7;
    eabase::Timer tm;
    const long WAIT_MSEC = 500;
    for (int i = 0; i < 2; ++i) {
        const fiber_attr_t attr =
            (i == 0 ? FIBER_ATTR_PTHREAD : FIBER_ATTR_NORMAL);
        ButexWaitArg arg = { butex, *butex, WAIT_MSEC, ETIMEDOUT };
        fiber_t th;
        
        tm.start();
        ASSERT_EQ(0, fiber_start(&th, &attr, wait_butex, &arg));
        ASSERT_EQ(0, fiber_join(th, NULL));
        tm.stop();
        
        ASSERT_LT(labs(tm.m_elapsed() - WAIT_MSEC), 250);
    }
    eabase::butex_destroy(butex);
}

TEST(ButexTest, stop_after_running) {
    int* butex = eabase::butex_create_checked<int>();
    *butex = 7;
    eabase::Timer tm;
    const long WAIT_MSEC = 500;
    const long SLEEP_MSEC = 10;
    for (int i = 0; i < 2; ++i) {
        const fiber_attr_t attr =
            (i == 0 ? FIBER_ATTR_PTHREAD : FIBER_ATTR_NORMAL);
        fiber_t th;
        ButexWaitArg arg = { butex, *butex, WAIT_MSEC, EINTR };

        tm.start();
        ASSERT_EQ(0, fiber_start(&th, &attr, wait_butex, &arg));
        ASSERT_EQ(0, fiber_usleep(SLEEP_MSEC * 1000L));
        ASSERT_EQ(0, fiber_stop(th));
        ASSERT_EQ(0, fiber_join(th, NULL));
        tm.stop();

        ASSERT_LT(labs(tm.m_elapsed() - SLEEP_MSEC), 25);
        // ASSERT_TRUE(eabase::get_task_control()->
        //             timer_thread()._idset.empty());
        ASSERT_EQ(EINVAL, fiber_stop(th));
    }    
    eabase::butex_destroy(butex);
}

TEST(ButexTest, stop_before_running) {
    int* butex = eabase::butex_create_checked<int>();
    *butex = 7;
    eabase::Timer tm;
    const long WAIT_MSEC = 500;

    for (int i = 0; i < 2; ++i) {
        const fiber_attr_t attr =
            (i == 0 ? FIBER_ATTR_PTHREAD : FIBER_ATTR_NORMAL) | FIBER_NOSIGNAL;
        fiber_t th;
        ButexWaitArg arg = { butex, *butex, WAIT_MSEC, EINTR };
        
        tm.start();
        ASSERT_EQ(0, fiber_start_lazy(&th, &attr, wait_butex, &arg));
        ASSERT_EQ(0, fiber_stop(th));
        fiber_flush();
        ASSERT_EQ(0, fiber_join(th, NULL));
        tm.stop();
        
        ASSERT_LT(tm.m_elapsed(), 5);
        // ASSERT_TRUE(eabase::get_task_control()->
        //             timer_thread()._idset.empty());
        ASSERT_EQ(EINVAL, fiber_stop(th));
    }
    eabase::butex_destroy(butex);
}

void* join_the_waiter(void* arg) {
    EXPECT_EQ(0, fiber_join((fiber_t)arg, NULL));
    return NULL;
}

TEST(ButexTest, join_cant_be_wakeup) {
    const long WAIT_MSEC = 100;
    int* butex = eabase::butex_create_checked<int>();
    *butex = 7;
    eabase::Timer tm;
    ButexWaitArg arg = { butex, *butex, 1000, EINTR };

    for (int i = 0; i < 2; ++i) {
        const fiber_attr_t attr =
            (i == 0 ? FIBER_ATTR_PTHREAD : FIBER_ATTR_NORMAL);
        tm.start();
        fiber_t th, th2;
        ASSERT_EQ(0, fiber_start(&th, NULL, wait_butex, &arg));
        ASSERT_EQ(0, fiber_start(&th2, &attr, join_the_waiter, (void*)th));
        ASSERT_EQ(0, fiber_stop(th2));
        ASSERT_EQ(0, fiber_usleep(WAIT_MSEC / 2 * 1000L));
        ASSERT_TRUE(eabase::TaskGroup::exists(th));
        ASSERT_TRUE(eabase::TaskGroup::exists(th2));
        ASSERT_EQ(0, fiber_usleep(WAIT_MSEC / 2 * 1000L));
        ASSERT_EQ(0, fiber_stop(th));
        ASSERT_EQ(0, fiber_join(th2, NULL));
        ASSERT_EQ(0, fiber_join(th, NULL));
        tm.stop();
        ASSERT_LT(tm.m_elapsed(), WAIT_MSEC + 15);
        ASSERT_EQ(EINVAL, fiber_stop(th));
        ASSERT_EQ(EINVAL, fiber_stop(th2));
    }
    eabase::butex_destroy(butex);
}

TEST(ButexTest, stop_after_slept) {
    eabase::Timer tm;
    const long SLEEP_MSEC = 100;
    const long WAIT_MSEC = 10;
    
    for (int i = 0; i < 2; ++i) {
        const fiber_attr_t attr =
            (i == 0 ? FIBER_ATTR_PTHREAD : FIBER_ATTR_NORMAL);
        tm.start();
        fiber_t th;
        ASSERT_EQ(0, fiber_start(
                      &th, &attr, sleeper, (void*)(SLEEP_MSEC*1000L)));
        ASSERT_EQ(0, fiber_usleep(WAIT_MSEC * 1000L));
        ASSERT_EQ(0, fiber_stop(th));
        ASSERT_EQ(0, fiber_join(th, NULL));
        tm.stop();
        if (attr.stack_type == FIBER_STACKTYPE_PTHREAD) {
            ASSERT_LT(labs(tm.m_elapsed() - SLEEP_MSEC), 15);
        } else {
            ASSERT_LT(labs(tm.m_elapsed() - WAIT_MSEC), 15);
        }
        // ASSERT_TRUE(eabase::get_task_control()->
        //             timer_thread()._idset.empty());
        ASSERT_EQ(EINVAL, fiber_stop(th));
    }
}

TEST(ButexTest, stop_just_when_sleeping) {
    eabase::Timer tm;
    const long SLEEP_MSEC = 100;
    
    for (int i = 0; i < 2; ++i) {
        const fiber_attr_t attr =
            (i == 0 ? FIBER_ATTR_PTHREAD : FIBER_ATTR_NORMAL);
        tm.start();
        fiber_t th;
        ASSERT_EQ(0, fiber_start(
                      &th, &attr, sleeper, (void*)(SLEEP_MSEC*1000L)));
        ASSERT_EQ(0, fiber_stop(th));
        ASSERT_EQ(0, fiber_join(th, NULL));
        tm.stop();
        if (attr.stack_type == FIBER_STACKTYPE_PTHREAD) {
            ASSERT_LT(labs(tm.m_elapsed() - SLEEP_MSEC), 15);
        } else {
            ASSERT_LT(tm.m_elapsed(), 15);
        }
        // ASSERT_TRUE(eabase::get_task_control()->
        //             timer_thread()._idset.empty());
        ASSERT_EQ(EINVAL, fiber_stop(th));
    }
}

TEST(ButexTest, stop_before_sleeping) {
    eabase::Timer tm;
    const long SLEEP_MSEC = 100;

    for (int i = 0; i < 2; ++i) {
        fiber_t th;
        const fiber_attr_t attr =
            (i == 0 ? FIBER_ATTR_PTHREAD : FIBER_ATTR_NORMAL) | FIBER_NOSIGNAL;
        
        tm.start();
        ASSERT_EQ(0, fiber_start_lazy(&th, &attr, sleeper,
                                              (void*)(SLEEP_MSEC*1000L)));
        ASSERT_EQ(0, fiber_stop(th));
        fiber_flush();
        ASSERT_EQ(0, fiber_join(th, NULL));
        tm.stop();

        if (attr.stack_type == FIBER_STACKTYPE_PTHREAD) {
            ASSERT_LT(labs(tm.m_elapsed() - SLEEP_MSEC), 10);
        } else {
            ASSERT_LT(tm.m_elapsed(), 10);
        }
        // ASSERT_TRUE(eabase::get_task_control()->
        //             timer_thread()._idset.empty());
        ASSERT_EQ(EINVAL, fiber_stop(th));
    }
}

void* trigger_signal(void* arg) {
    pthread_t * th = (pthread_t*)arg;
    const long t1 = eabase::gettimeofday_us();
    for (size_t i = 0; i < 50; ++i) {
      usleep(100000);
      if (eabase::interrupt_pthread(*th) == ESRCH) {
        LOG(INFO) << "waiter thread end, trigger count=" << i;
        break;
      }
    }
    const long t2 = eabase::gettimeofday_us();
    LOG(INFO) << "trigger signal thread end, elapsed=" << (t2-t1) << "us";
    return NULL;
}

TEST(ButexTest, wait_with_signal_triggered) {
    eabase::Timer tm;

    const int64_t WAIT_MSEC = 500;
    WaiterArg waiter_args;
    pthread_t waiter_th, tigger_th;
    eabase::atomic<int>* butex =
        eabase::butex_create_checked<eabase::atomic<int> >();
    ASSERT_TRUE(butex);
    *butex = 1;
    ASSERT_EQ(0, eabase::butex_wake(butex));

    const timespec abstime = eabase::milliseconds_from_now(WAIT_MSEC);
    waiter_args.expected_value = *butex;
    waiter_args.butex = butex;
    waiter_args.expected_result = ETIMEDOUT;
    waiter_args.ptimeout = &abstime;
    tm.start();
    pthread_create(&waiter_th, NULL, waiter, &waiter_args);
    pthread_create(&tigger_th, NULL, trigger_signal, &waiter_th);
    
    ASSERT_EQ(0, pthread_join(waiter_th, NULL));
    tm.stop();
    auto wait_elapsed_ms = tm.m_elapsed();;
    LOG(INFO) << "waiter thread end, elapsed " << wait_elapsed_ms << " ms";

    ASSERT_LT(labs(wait_elapsed_ms - WAIT_MSEC), 250);

    ASSERT_EQ(0, pthread_join(tigger_th, NULL));
    eabase::butex_destroy(butex);
}

} // namespace
