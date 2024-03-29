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
#include "eabase/utility/class_name.h"
#include "eabase/utility/logging.h"

namespace eabase {
namespace foobar {
struct MyClass {};
}
}

namespace {
class ClassNameTest : public ::testing::Test {
protected:
    virtual void SetUp() {
        srand(time(0));
    };
};

TEST_F(ClassNameTest, demangle) {
    ASSERT_EQ("add_something", eabase::demangle("add_something"));
    ASSERT_EQ("guard variable for eabase::my_ip()::ip",
              eabase::demangle("_ZGVZN5butil5my_ipEvE2ip"));
    ASSERT_EQ("dp::FiberPBCommand<proto::PbRouteTable, proto::PbRouteAck>::marshal(dp::ParamWriter*)::__FUNCTION__",
              eabase::demangle("_ZZN2dp14FiberPBCommandIN5proto12PbRouteTableENS1_10PbRouteAckEE7marshalEPNS_11ParamWriterEE12__FUNCTION__"));
    ASSERT_EQ("7&8", eabase::demangle("7&8"));
}

TEST_F(ClassNameTest, class_name_sanity) {
    ASSERT_EQ("char", eabase::class_name_str('\0'));
    ASSERT_STREQ("short", eabase::class_name<short>());
    ASSERT_EQ("long", eabase::class_name_str(1L));
    ASSERT_EQ("unsigned long", eabase::class_name_str(1UL));
    ASSERT_EQ("float", eabase::class_name_str(1.1f));
    ASSERT_EQ("double", eabase::class_name_str(1.1));
    ASSERT_STREQ("char*", eabase::class_name<char*>());
    ASSERT_STREQ("char const*", eabase::class_name<const char*>());
    ASSERT_STREQ("eabase::foobar::MyClass", eabase::class_name<eabase::foobar::MyClass>());

    int array[32];
    ASSERT_EQ("int [32]", eabase::class_name_str(array));

    LOG(INFO) << eabase::class_name_str(this);
    LOG(INFO) << eabase::class_name_str(*this);
}
}
