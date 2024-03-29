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

// Date 2014/10/13 19:47:59

#include <pthread.h>                                // pthread_*

#include <cstddef>
#include <memory>
#include <iostream>
#include "eabase/utility/time.h"
#include "eabase/utility/macros.h"
#include "eabase/var/recorder.h"
#include "eabase/var/latency_recorder.h"
#include <gtest/gtest.h>

namespace {
TEST(RecorderTest, test_complement) {
    LOG(INFO) << "sizeof(LatencyRecorder)=" << sizeof(eabase::LatencyRecorder)
              << " " << sizeof(eabase::detail::Percentile)
              << " " << sizeof(eabase::Maxer<int64_t>)
              << " " << sizeof(eabase::IntRecorder)
              << " " << sizeof(eabase::Window<eabase::IntRecorder>)
              << " " << sizeof(eabase::Window<eabase::detail::Percentile>);

    for (int a = -10000000; a < 10000000; ++a) {
        const uint64_t complement = eabase::IntRecorder::_get_complement(a);
        const int64_t b = eabase::IntRecorder::_extend_sign_bit(complement);
        ASSERT_EQ(a, b);
    }
}

TEST(RecorderTest, test_compress) {
    const uint64_t num = 125345;
    const uint64_t sum = 26032906;
    const uint64_t compressed = eabase::IntRecorder::_compress(num, sum);
    ASSERT_EQ(num, eabase::IntRecorder::_get_num(compressed));
    ASSERT_EQ(sum, eabase::IntRecorder::_get_sum(compressed));
}

TEST(RecorderTest, test_compress_negtive_number) {
    for (int a = -10000000; a < 10000000; ++a) {
        const uint64_t sum = eabase::IntRecorder::_get_complement(a);
        const uint64_t num = 123456;
        const uint64_t compressed = eabase::IntRecorder::_compress(num, sum);
        ASSERT_EQ(num, eabase::IntRecorder::_get_num(compressed));
        ASSERT_EQ(a, eabase::IntRecorder::_extend_sign_bit(eabase::IntRecorder::_get_sum(compressed)));
    }
}

TEST(RecorderTest, sanity) {
    {
        eabase::IntRecorder recorder;
        ASSERT_TRUE(recorder.valid());
        ASSERT_EQ(0, recorder.expose("var1"));
        for (size_t i = 0; i < 100; ++i) {
            recorder << 2;
        }
        ASSERT_EQ(2l, (int64_t)recorder.average());
        ASSERT_EQ("2", eabase::Variable::describe_exposed("var1"));
        std::vector<std::string> vars;
        eabase::Variable::list_exposed(&vars);
        ASSERT_EQ(1UL, vars.size());
        ASSERT_EQ("var1", vars[0]);
        ASSERT_EQ(1UL, eabase::Variable::count_exposed());
    }
    ASSERT_EQ(0UL, eabase::Variable::count_exposed());
}

TEST(RecorderTest, window) {
    eabase::IntRecorder c1;
    ASSERT_TRUE(c1.valid());
    eabase::Window<eabase::IntRecorder> w1(&c1, 1);
    eabase::Window<eabase::IntRecorder> w2(&c1, 2);
    eabase::Window<eabase::IntRecorder> w3(&c1, 3);

    const int N = 10000;
    int64_t last_time = eabase::gettimeofday_us();
    for (int i = 1; i <= N; ++i) {
        c1 << i;
        int64_t now = eabase::gettimeofday_us();
        if (now - last_time >= 1000000L) {
            last_time = now;
            LOG(INFO) << "c1=" << c1 << " w1=" << w1 << " w2=" << w2 << " w3=" << w3;
        } else {
            usleep(950);
        }
    }
}

TEST(RecorderTest, negative) {
    eabase::IntRecorder recorder;
    ASSERT_TRUE(recorder.valid());
    for (size_t i = 0; i < 3; ++i) {
        recorder << -2;
    }
    ASSERT_EQ(-2, recorder.average());
}

TEST(RecorderTest, positive_overflow) {
    eabase::IntRecorder recorder1;
    ASSERT_TRUE(recorder1.valid());
    for (int i = 0; i < 5; ++i) {
        recorder1 << std::numeric_limits<int64_t>::max();
    }
    ASSERT_EQ(std::numeric_limits<int>::max(), recorder1.average());

    eabase::IntRecorder recorder2;
    ASSERT_TRUE(recorder2.valid());
    recorder2.set_debug_name("recorder2");
    for (int i = 0; i < 5; ++i) {
        recorder2 << std::numeric_limits<int64_t>::max();
    }
    ASSERT_EQ(std::numeric_limits<int>::max(), recorder2.average());

    eabase::IntRecorder recorder3;
    ASSERT_TRUE(recorder3.valid());
    recorder3.expose("recorder3");
    for (int i = 0; i < 5; ++i) {
        recorder3 << std::numeric_limits<int64_t>::max();
    }
    ASSERT_EQ(std::numeric_limits<int>::max(), recorder3.average());

    eabase::LatencyRecorder latency1;
    latency1.expose("latency1");
    latency1 << std::numeric_limits<int64_t>::max();

    eabase::LatencyRecorder latency2;
    latency2 << std::numeric_limits<int64_t>::max();
}

TEST(RecorderTest, negtive_overflow) {
    eabase::IntRecorder recorder1;
    ASSERT_TRUE(recorder1.valid());
    for (int i = 0; i < 5; ++i) {
        recorder1 << std::numeric_limits<int64_t>::min();
    }
    ASSERT_EQ(std::numeric_limits<int>::min(), recorder1.average());

    eabase::IntRecorder recorder2;
    ASSERT_TRUE(recorder2.valid());
    recorder2.set_debug_name("recorder2");
    for (int i = 0; i < 5; ++i) {
        recorder2 << std::numeric_limits<int64_t>::min();
    }
    ASSERT_EQ(std::numeric_limits<int>::min(), recorder2.average());

    eabase::IntRecorder recorder3;
    ASSERT_TRUE(recorder3.valid());
    recorder3.expose("recorder3");
    for (int i = 0; i < 5; ++i) {
        recorder3 << std::numeric_limits<int64_t>::min();
    }
    ASSERT_EQ(std::numeric_limits<int>::min(), recorder3.average());

    eabase::LatencyRecorder latency1;
    latency1.expose("latency1");
    latency1 << std::numeric_limits<int64_t>::min();

    eabase::LatencyRecorder latency2;
    latency2 << std::numeric_limits<int64_t>::min();
}

const size_t OPS_PER_THREAD = 20000000;

static void *thread_counter(void *arg) {
    eabase::IntRecorder *recorder = (eabase::IntRecorder *)arg;
    eabase::Timer timer;
    timer.start();
    for (int i = 0; i < (int)OPS_PER_THREAD; ++i) {
        *recorder << i;
    }
    timer.stop();
    return (void *)(timer.n_elapsed());
}

TEST(RecorderTest, perf) {
    eabase::IntRecorder recorder;
    ASSERT_TRUE(recorder.valid());
    pthread_t threads[8];
    for (size_t i = 0; i < ARRAY_SIZE(threads); ++i) {
        pthread_create(&threads[i], NULL, &thread_counter, (void *)&recorder);
    }
    long totol_time = 0;
    for (size_t i = 0; i < ARRAY_SIZE(threads); ++i) {
        void *ret; 
        pthread_join(threads[i], &ret);
        totol_time += (long)ret;
    }
    ASSERT_EQ(((int64_t)OPS_PER_THREAD - 1) / 2, recorder.average());
    LOG(INFO) << "Recorder takes " << totol_time / (OPS_PER_THREAD * ARRAY_SIZE(threads)) 
              << "ns per sample with " << ARRAY_SIZE(threads) 
              << " threads";
}

TEST(RecorderTest, latency_recorder_qps_accuracy) {
    eabase::LatencyRecorder lr1(2); // set windows size to 2s
    eabase::LatencyRecorder lr2(2);
    eabase::LatencyRecorder lr3(2);
    eabase::LatencyRecorder lr4(2);
    usleep(3000000); // wait sampler to sample 3 times

    auto write = [](eabase::LatencyRecorder& lr, int times) {
        for (int i = 0; i < times; ++i) {
            lr << 1;
        }
    };
    write(lr1, 10);
    write(lr2, 11);
    write(lr3, 3);
    write(lr4, 1);
    usleep(1000000); // wait sampler to sample 1 time

    auto read = [](eabase::LatencyRecorder& lr, double exp_qps, int window_size = 0) {
        int64_t qps_sum = 0;
        int64_t exp_qps_int = (int64_t)exp_qps;
        for (int i = 0; i < 1000; ++i) {
            int64_t qps = window_size ? lr.qps(window_size): lr.qps();
            EXPECT_GE(qps, exp_qps_int - 1);
            EXPECT_LE(qps, exp_qps_int + 1);
            qps_sum += qps;
        }
        double err = fabs(qps_sum / 1000.0 - exp_qps);
        return err;
    };
    ASSERT_GT(0.1, read(lr1, 10/2.0));
    ASSERT_GT(0.1, read(lr2, 11/2.0));
    ASSERT_GT(0.1, read(lr3, 3/2.0));
    ASSERT_GT(0.1, read(lr4, 1/2.0));

    ASSERT_GT(0.1, read(lr1, 10/3.0, 3));
    ASSERT_GT(0.2, read(lr2, 11/3.0, 3));
    ASSERT_GT(0.1, read(lr3, 3/3.0, 3));
    ASSERT_GT(0.1, read(lr4, 1/3.0, 3));
}

} // namespace
