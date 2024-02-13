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

#include <pthread.h>
#include <cstddef>
#include <memory>
#include <list>
#include <iostream>
#include <sstream>
#include <eabase/utility/time.h>
#include <eabase/utility/macros.h>
#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include "eabase/var/var.h"
#include "eabase/var/window.h"

class WindowTest : public testing::Test {
protected:
    void SetUp() {}
    void TearDown() {}
};

TEST_F(WindowTest, window) {
    const int window_size = 2;
    // test eabase::Adder
    eabase::Adder<int> adder;
    eabase::Window<eabase::Adder<int> > window_adder(&adder, window_size);
    eabase::PerSecond<eabase::Adder<int> > per_second_adder(&adder, window_size);
    eabase::WindowEx<eabase::Adder<int>, 2> window_ex_adder("window_ex_adder");
    eabase::PerSecondEx<eabase::Adder<int>, window_size> per_second_ex_adder("per_second_ex_adder");

    // test eabase::Maxer
    eabase::Maxer<int> maxer;
    eabase::Window<eabase::Maxer<int> > window_maxer(&maxer, window_size);
    eabase::WindowEx<eabase::Maxer<int>, window_size> window_ex_maxer;

    // test eabase::Miner
    eabase::Miner<int> miner;
    eabase::Window<eabase::Miner<int> > window_miner(&miner, window_size);
    eabase::WindowEx<eabase::Miner<int>, window_size> window_ex_miner;

    // test eabase::IntRecorder
    eabase::IntRecorder recorder;
    eabase::Window<eabase::IntRecorder> window_int_recorder(&recorder, window_size);
    eabase::WindowEx<eabase::IntRecorder, window_size> window_ex_int_recorder("window_ex_int_recorder");

    adder << 10;
    window_ex_adder << 10;
    per_second_ex_adder << 10;

    maxer << 10;
    window_ex_maxer << 10;
    miner << 10;
    window_ex_miner << 10;

    recorder << 10;
    window_ex_int_recorder << 10;

    sleep(1);
    adder << 2;
    window_ex_adder << 2;
    per_second_ex_adder << 2;

    maxer << 2;
    window_ex_maxer << 2;
    miner << 2;
    window_ex_miner << 2;

    recorder << 2;
    window_ex_int_recorder << 2;
    sleep(1);

    ASSERT_EQ(window_adder.get_value(), window_ex_adder.get_value());
    ASSERT_EQ(per_second_adder.get_value(), per_second_ex_adder.get_value());

    ASSERT_EQ(window_maxer.get_value(), window_ex_maxer.get_value());
    ASSERT_EQ(window_miner.get_value(), window_ex_miner.get_value());

    eabase::Stat recorder_stat = window_int_recorder.get_value();
    eabase::Stat window_ex_recorder_stat = window_ex_int_recorder.get_value();
    ASSERT_EQ(recorder_stat.get_average_int(), window_ex_recorder_stat.get_average_int());
    ASSERT_DOUBLE_EQ(recorder_stat.get_average_double(), window_ex_recorder_stat.get_average_double());
}
