// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "eabase/utility/threading/thread_id_name_manager.h"

#include "eabase/utility/threading/platform_thread.h"
#include <gtest/gtest.h>

typedef testing::Test ThreadIdNameManagerTest;

namespace {

TEST_F(ThreadIdNameManagerTest, ThreadNameInterning) {
  eabase::ThreadIdNameManager* manager = eabase::ThreadIdNameManager::GetInstance();

  eabase::PlatformThreadId a_id = eabase::PlatformThread::CurrentId();
  eabase::PlatformThread::SetName("First Name");
  std::string version = manager->GetName(a_id);

  eabase::PlatformThread::SetName("New name");
  EXPECT_NE(version, manager->GetName(a_id));
  eabase::PlatformThread::SetName("");
}

TEST_F(ThreadIdNameManagerTest, ResettingNameKeepsCorrectInternedValue) {
  eabase::ThreadIdNameManager* manager = eabase::ThreadIdNameManager::GetInstance();

  eabase::PlatformThreadId a_id = eabase::PlatformThread::CurrentId();
  eabase::PlatformThread::SetName("Test Name");
  std::string version = manager->GetName(a_id);

  eabase::PlatformThread::SetName("New name");
  EXPECT_NE(version, manager->GetName(a_id));

  eabase::PlatformThread::SetName("Test Name");
  EXPECT_EQ(version, manager->GetName(a_id));

  eabase::PlatformThread::SetName("");
}

}  // namespace
