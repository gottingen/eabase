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
#include <sstream>
#include "eabase/utility/time.h"
#include "eabase/utility/macros.h"
#include "eabase/var/var.h"
#include <gtest/gtest.h>

namespace {
class StatusTest : public testing::Test {
protected:
    void SetUp() {
    }
    void TearDown() {
        ASSERT_EQ(0UL, eabase::Variable::count_exposed());
    }
};

TEST_F(StatusTest, status) {
    eabase::StatusVar<std::string> st1;
    st1.set_value("hello %d", 9);
#ifdef BAIDU_INTERNAL
    boost::any v1;
    st1.get_value(&v1);
    ASSERT_EQ("hello 9", boost::any_cast<std::string>(v1));
#endif
    ASSERT_EQ(0, st1.expose("var1"));
    ASSERT_EQ("hello 9", eabase::Variable::describe_exposed("var1"));
    ASSERT_EQ("\"hello 9\"", eabase::Variable::describe_exposed("var1", true));
    std::vector<std::string> vars;
    eabase::Variable::list_exposed(&vars);
    ASSERT_EQ(1UL, vars.size());
    ASSERT_EQ("var1", vars[0]);
    ASSERT_EQ(1UL, eabase::Variable::count_exposed());

    eabase::StatusVar<std::string> st2;
    st2.set_value("world %d", 10);
    ASSERT_EQ(-1, st2.expose("var1"));
    ASSERT_EQ(1UL, eabase::Variable::count_exposed());
    ASSERT_EQ("world 10", st2.get_description());
    ASSERT_EQ("hello 9", eabase::Variable::describe_exposed("var1"));
    ASSERT_EQ(1UL, eabase::Variable::count_exposed());

    ASSERT_TRUE(st1.hide());
    ASSERT_EQ(0UL, eabase::Variable::count_exposed());
    ASSERT_EQ("", eabase::Variable::describe_exposed("var1"));
    ASSERT_EQ(0, st1.expose("var1"));
    ASSERT_EQ(1UL, eabase::Variable::count_exposed());
    ASSERT_EQ("hello 9",
              eabase::Variable::describe_exposed("var1"));
    
    ASSERT_EQ(0, st2.expose("var2"));
    ASSERT_EQ(2UL, eabase::Variable::count_exposed());
    ASSERT_EQ("hello 9", eabase::Variable::describe_exposed("var1"));
    ASSERT_EQ("world 10", eabase::Variable::describe_exposed("var2"));
    eabase::Variable::list_exposed(&vars);
    ASSERT_EQ(2UL, vars.size());
    ASSERT_EQ("var1", vars[0]);
    ASSERT_EQ("var2", vars[1]);

    ASSERT_TRUE(st2.hide());
    ASSERT_EQ(1UL, eabase::Variable::count_exposed());
    ASSERT_EQ("", eabase::Variable::describe_exposed("var2"));
    eabase::Variable::list_exposed(&vars);
    ASSERT_EQ(1UL, vars.size());
    ASSERT_EQ("var1", vars[0]);

    st2.expose("var2 again");
    ASSERT_EQ("world 10", eabase::Variable::describe_exposed("var2_again"));
    eabase::Variable::list_exposed(&vars);
    ASSERT_EQ(2UL, vars.size());
    ASSERT_EQ("var1", vars[0]);
    ASSERT_EQ("var2_again", vars[1]);
    ASSERT_EQ(2UL, eabase::Variable::count_exposed());

    eabase::StatusVar<std::string> st3("var3", "foobar");
    ASSERT_EQ("var3", st3.name());
    ASSERT_EQ(3UL, eabase::Variable::count_exposed());
    ASSERT_EQ("foobar", eabase::Variable::describe_exposed("var3"));
    eabase::Variable::list_exposed(&vars);
    ASSERT_EQ(3UL, vars.size());
    ASSERT_EQ("var1", vars[0]);
    ASSERT_EQ("var3", vars[1]);
    ASSERT_EQ("var2_again", vars[2]);
    ASSERT_EQ(3UL, eabase::Variable::count_exposed());

    eabase::StatusVar<int> st4("var4", 9);
    ASSERT_EQ("var4", st4.name());
    ASSERT_EQ(4UL, eabase::Variable::count_exposed());
#ifdef BAIDU_INTERNAL
    boost::any v4;
    st4.get_value(&v4);
    ASSERT_EQ(9, boost::any_cast<int>(v4));
#endif
    ASSERT_EQ("9", eabase::Variable::describe_exposed("var4"));
    eabase::Variable::list_exposed(&vars);
    ASSERT_EQ(4UL, vars.size());
    ASSERT_EQ("var1", vars[0]);
    ASSERT_EQ("var3", vars[1]);
    ASSERT_EQ("var4", vars[2]);
    ASSERT_EQ("var2_again", vars[3]);

    eabase::StatusVar<void*> st5((void*)19UL);
    LOG(INFO) << st5;
#ifdef BAIDU_INTERNAL
    boost::any v5;
    st5.get_value(&v5);
    ASSERT_EQ((void*)19UL, boost::any_cast<void*>(v5));
#endif
    ASSERT_EQ("0x13", st5.get_description());
}

void print1(std::ostream& os, void* arg) {
    os << arg;
}

int64_t print2(void* arg) {
    return *(int64_t*)arg;
}

TEST_F(StatusTest, passive_status) {
    eabase::BasicPassiveStatus<std::string> st1("var11", print1, (void*)9UL);
    LOG(INFO) << st1;
#ifdef BAIDU_INTERNAL
    boost::any v1;
    st1.get_value(&v1);
    ASSERT_EQ("0x9", boost::any_cast<std::string>(v1));
#endif
    std::ostringstream ss;
    ASSERT_EQ(0, eabase::Variable::describe_exposed("var11", ss));
    ASSERT_EQ("0x9", ss.str());
    std::vector<std::string> vars;
    eabase::Variable::list_exposed(&vars);
    ASSERT_EQ(1UL, vars.size());
    ASSERT_EQ("var11", vars[0]);
    ASSERT_EQ(1UL, eabase::Variable::count_exposed());

    int64_t tmp2 = 9;
    eabase::BasicPassiveStatus<int64_t> st2("var12", print2, &tmp2);
#ifdef BAIDU_INTERNAL
    boost::any v2;
    st2.get_value(&v2);
    try {
        boost::any_cast<int32_t>(v2);
        ASSERT_TRUE(false);
    } catch (boost::bad_any_cast & e) {
        LOG(INFO) << "Casting int64_t to int32_t throws.";
    }
    ASSERT_EQ(9, boost::any_cast<int64_t>(v2));
#endif
    ss.str("");
    ASSERT_EQ(0, eabase::Variable::describe_exposed("var12", ss));
    ASSERT_EQ("9", ss.str());
    eabase::Variable::list_exposed(&vars);
    ASSERT_EQ(2UL, vars.size());
    ASSERT_EQ("var11", vars[0]);
    ASSERT_EQ("var12", vars[1]);
    ASSERT_EQ(2UL, eabase::Variable::count_exposed());
}

struct Foo {
    int x;
    Foo() : x(0) {}
    explicit Foo(int x2) : x(x2) {}
    Foo operator+(const Foo& rhs) const {
        return Foo(x + rhs.x);
    }
};

std::ostream& operator<<(std::ostream& os, const Foo& f) {
    return os << "Foo{" << f.x << "}";
}

TEST_F(StatusTest, non_primitive) {
    eabase::StatusVar<Foo> st;
    ASSERT_EQ(0, st.get_value().x);
    st.set_value(Foo(1));
    ASSERT_EQ(1, st.get_value().x);
#ifdef BAIDU_INTERNAL
    boost::any a1;
    st.get_value(&a1);
    ASSERT_EQ(1, boost::any_cast<Foo>(a1).x);
#endif
}
} // namespace
