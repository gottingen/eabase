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

#include <limits>                           //std::numeric_limits

#include "eabase/var/reducer.h"

#include "eabase/utility/time.h"
#include "eabase/utility/macros.h"
#include "eabase/utility/string_printf.h"
#include "eabase/utility/string_splitter.h"

#include <gtest/gtest.h>

namespace {
class ReducerTest : public testing::Test {
protected:
    void SetUp() {}
    void TearDown() {}
};

TEST_F(ReducerTest, atomicity) {
    ASSERT_EQ(sizeof(int32_t), sizeof(eabase::detail::ElementContainer<int32_t>));
    ASSERT_EQ(sizeof(int64_t), sizeof(eabase::detail::ElementContainer<int64_t>));
    ASSERT_EQ(sizeof(float), sizeof(eabase::detail::ElementContainer<float>));
    ASSERT_EQ(sizeof(double), sizeof(eabase::detail::ElementContainer<double>));
}

TEST_F(ReducerTest, adder) {    
    eabase::Adder<uint32_t> reducer1;
    ASSERT_TRUE(reducer1.valid());
    reducer1 << 2 << 4;
    ASSERT_EQ(6ul, reducer1.get_value());
#ifdef BAIDU_INTERNAL
    boost::any v1;
    reducer1.get_value(&v1);
    ASSERT_EQ(6u, boost::any_cast<unsigned int>(v1));
#endif

    eabase::Adder<double> reducer2;
    ASSERT_TRUE(reducer2.valid());
    reducer2 << 2.0 << 4.0;
    ASSERT_DOUBLE_EQ(6.0, reducer2.get_value());

    eabase::Adder<int> reducer3;
    ASSERT_TRUE(reducer3.valid());
    reducer3 << -9 << 1 << 0 << 3;
    ASSERT_EQ(-5, reducer3.get_value());
}

const size_t OPS_PER_THREAD = 500000;

static void *thread_counter(void *arg) {
    eabase::Adder<uint64_t> *reducer = (eabase::Adder<uint64_t> *)arg;
    eabase::Timer timer;
    timer.start();
    for (size_t i = 0; i < OPS_PER_THREAD; ++i) {
        (*reducer) << 2;
    }
    timer.stop();
    return (void *)(timer.n_elapsed());
}

void *add_atomic(void *arg) {
    eabase::atomic<uint64_t> *counter = (eabase::atomic<uint64_t> *)arg;
    eabase::Timer timer;
    timer.start();
    for (size_t i = 0; i < OPS_PER_THREAD / 100; ++i) {
        counter->fetch_add(2, eabase::memory_order_relaxed);
    }
    timer.stop();
    return (void *)(timer.n_elapsed());
}

static long start_perf_test_with_atomic(size_t num_thread) {
    eabase::atomic<uint64_t> counter(0);
    pthread_t threads[num_thread];
    for (size_t i = 0; i < num_thread; ++i) {
        pthread_create(&threads[i], NULL, &add_atomic, (void *)&counter);
    }
    long totol_time = 0;
    for (size_t i = 0; i < num_thread; ++i) {
        void *ret; 
        pthread_join(threads[i], &ret);
        totol_time += (long)ret;
    }
    long avg_time = totol_time / (OPS_PER_THREAD / 100 * num_thread);
    EXPECT_EQ(2ul * num_thread * OPS_PER_THREAD / 100, counter.load());
    return avg_time;
}

static long start_perf_test_with_adder(size_t num_thread) {
    eabase::Adder<uint64_t> reducer;
    EXPECT_TRUE(reducer.valid());
    pthread_t threads[num_thread];
    for (size_t i = 0; i < num_thread; ++i) {
        pthread_create(&threads[i], NULL, &thread_counter, (void *)&reducer);
    }
    long totol_time = 0;
    for (size_t i = 0; i < num_thread; ++i) {
        void *ret = NULL; 
        pthread_join(threads[i], &ret);
        totol_time += (long)ret;
    }
    long avg_time = totol_time / (OPS_PER_THREAD * num_thread);
    EXPECT_EQ(2ul * num_thread * OPS_PER_THREAD, reducer.get_value());
    return avg_time;
}

TEST_F(ReducerTest, perf) {
    std::ostringstream oss;
    for (size_t i = 1; i <= 24; ++i) {
        oss << i << '\t' << start_perf_test_with_adder(i) << '\n';
    }
    LOG(INFO) << "Adder performance:\n" << oss.str();
    oss.str("");
    for (size_t i = 1; i <= 24; ++i) {
        oss << i << '\t' << start_perf_test_with_atomic(i) << '\n';
    }
    LOG(INFO) << "Atomic performance:\n" << oss.str();
}

TEST_F(ReducerTest, Min) {
    eabase::Miner<uint64_t> reducer;
    ASSERT_EQ(std::numeric_limits<uint64_t>::max(), reducer.get_value());
    reducer << 10 << 20;
    ASSERT_EQ(10ul, reducer.get_value());
    reducer << 5;
    ASSERT_EQ(5ul, reducer.get_value());
    reducer << std::numeric_limits<uint64_t>::max();
    ASSERT_EQ(5ul, reducer.get_value());
    reducer << 0;
    ASSERT_EQ(0ul, reducer.get_value());

    eabase::Miner<int> reducer2;
    ASSERT_EQ(std::numeric_limits<int>::max(), reducer2.get_value());
    reducer2 << 10 << 20;
    ASSERT_EQ(10, reducer2.get_value());
    reducer2 << -5;
    ASSERT_EQ(-5, reducer2.get_value());
    reducer2 << std::numeric_limits<int>::max();
    ASSERT_EQ(-5, reducer2.get_value());
    reducer2 << 0;
    ASSERT_EQ(-5, reducer2.get_value());
    reducer2 << std::numeric_limits<int>::min();
    ASSERT_EQ(std::numeric_limits<int>::min(), reducer2.get_value());
}

TEST_F(ReducerTest, max) {
    eabase::Maxer<uint64_t> reducer;
    ASSERT_EQ(std::numeric_limits<uint64_t>::min(), reducer.get_value());
    ASSERT_TRUE(reducer.valid());
    reducer << 20 << 10;
    ASSERT_EQ(20ul, reducer.get_value());
    reducer << 30;
    ASSERT_EQ(30ul, reducer.get_value());
    reducer << 0;
    ASSERT_EQ(30ul, reducer.get_value());

    eabase::Maxer<int> reducer2;
    ASSERT_EQ(std::numeric_limits<int>::min(), reducer2.get_value());
    ASSERT_TRUE(reducer2.valid());
    reducer2 << 20 << 10;
    ASSERT_EQ(20, reducer2.get_value());
    reducer2 << 30;
    ASSERT_EQ(30, reducer2.get_value());
    reducer2 << 0;
    ASSERT_EQ(30, reducer2.get_value());
    reducer2 << std::numeric_limits<int>::max();
    ASSERT_EQ(std::numeric_limits<int>::max(), reducer2.get_value());
}

eabase::Adder<long> g_a;

TEST_F(ReducerTest, global) {
    ASSERT_TRUE(g_a.valid());
    g_a.get_value();
}

void ReducerTest_window() {
    eabase::Adder<int> c1;
    eabase::Maxer<int> c2;
    eabase::Miner<int> c3;
    eabase::Window<eabase::Adder<int> > w1(&c1, 1);
    eabase::Window<eabase::Adder<int> > w2(&c1, 2);
    eabase::Window<eabase::Adder<int> > w3(&c1, 3);
    eabase::Window<eabase::Maxer<int> > w4(&c2, 1);
    eabase::Window<eabase::Maxer<int> > w5(&c2, 2);
    eabase::Window<eabase::Maxer<int> > w6(&c2, 3);
    eabase::Window<eabase::Miner<int> > w7(&c3, 1);
    eabase::Window<eabase::Miner<int> > w8(&c3, 2);
    eabase::Window<eabase::Miner<int> > w9(&c3, 3);

#if !EABASE_WITH_GLOG
    logging::StringSink log_str;
    logging::LogSink* old_sink = logging::SetLogSink(&log_str);
    c2.get_value();
    ASSERT_EQ(&log_str, logging::SetLogSink(old_sink));
    ASSERT_NE(std::string::npos, log_str.find(
                  "You should not call Reducer<int, eabase::detail::MaxTo<int>>"
                  "::get_value() when a Window<> is used because the operator"
                  " does not have inverse."));
#endif
    const int N = 6000;
    int count = 0;
    int total_count = 0;
    int64_t last_time = eabase::gettimeofday_us();
    for (int i = 1; i <= N; ++i) {
        c1 << 1;
        c2 << N - i;
        c3 << i;
        ++count;
        ++total_count;
        int64_t now = eabase::gettimeofday_us();
        if (now - last_time >= 1000000L) {
            last_time = now;
            ASSERT_EQ(total_count, c1.get_value());
            LOG(INFO) << "c1=" << total_count
                      << " count=" << count
                      << " w1=" << w1
                      << " w2=" << w2
                      << " w3=" << w3
                      << " w4=" << w4
                      << " w5=" << w5
                      << " w6=" << w6
                      << " w7=" << w7
                      << " w8=" << w8
                      << " w9=" << w9;
            count = 0;
        } else {
            usleep(950);
        }
    }
}

TEST_F(ReducerTest, window) {
#if !EABASE_WITH_GLOG
    ReducerTest_window();
    logging::StringSink log_str;
    logging::LogSink* old_sink = logging::SetLogSink(&log_str);
    sleep(1);
    ASSERT_EQ(&log_str, logging::SetLogSink(old_sink));
    if (log_str.find("Removed ") != std::string::npos) {
        ASSERT_NE(std::string::npos, log_str.find("Removed 3, sampled 0")) << log_str;
    }
#endif
}

struct Foo {
    int x;
    Foo() : x(0) {}
    explicit Foo(int x2) : x(x2) {}
    void operator+=(const Foo& rhs) {
        x += rhs.x;
    }
};

std::ostream& operator<<(std::ostream& os, const Foo& f) {
    return os << "Foo{" << f.x << "}";
}

TEST_F(ReducerTest, non_primitive) {
    eabase::Adder<Foo> adder;
    adder << Foo(2) << Foo(3) << Foo(4);
    ASSERT_EQ(9, adder.get_value().x);
}

bool g_stop = false;
struct StringAppenderResult {
    int count;
};

static void* string_appender(void* arg) {
    eabase::Adder<std::string>* cater = (eabase::Adder<std::string>*)arg;
    int count = 0;
    std::string id = eabase::string_printf("%lld", (long long)pthread_self());
    std::string tmp = "a";
    for (count = 0; !count || !g_stop; ++count) {
        *cater << id << ":";
        for (char c = 'a'; c <= 'z'; ++c) {
            tmp[0] = c;
            *cater << tmp;
        }
        *cater << ".";
    }
    StringAppenderResult* res = new StringAppenderResult;
    res->count = count;
    LOG(INFO) << "Appended " << count;
    return res;
}

TEST_F(ReducerTest, non_primitive_mt) {
    eabase::Adder<std::string> cater;
    pthread_t th[8];
    g_stop = false;
    for (size_t i = 0; i < arraysize(th); ++i) {
        pthread_create(&th[i], NULL, string_appender, &cater);
    }
    usleep(50000);
    g_stop = true;
    eabase::hash_map<pthread_t, int> appended_count;
    for (size_t i = 0; i < arraysize(th); ++i) {
        StringAppenderResult* res = NULL;
        pthread_join(th[i], (void**)&res);
        appended_count[th[i]] = res->count;
        delete res;
    }
    eabase::hash_map<pthread_t, int> got_count;
    std::string res = cater.get_value();
    for (eabase::StringSplitter sp(res.c_str(), '.'); sp; ++sp) {
        char* endptr = NULL;
        ++got_count[(pthread_t)strtoll(sp.field(), &endptr, 10)];
        ASSERT_EQ(27LL, sp.field() + sp.length() - endptr)
            << eabase::StringPiece(sp.field(), sp.length());
        ASSERT_EQ(0, memcmp(":abcdefghijklmnopqrstuvwxyz", endptr, 27));
    }
    ASSERT_EQ(appended_count.size(), got_count.size());
    for (size_t i = 0; i < arraysize(th); ++i) {
        ASSERT_EQ(appended_count[th[i]], got_count[th[i]]);
    }
}

TEST_F(ReducerTest, simple_window) {
    eabase::Adder<int64_t> a;
    eabase::Window<eabase::Adder<int64_t> > w(&a, 10);
    a << 100;
    sleep(3);
    const int64_t v = w.get_value();
    ASSERT_EQ(100, v) << "v=" << v;
}
} // namespace
