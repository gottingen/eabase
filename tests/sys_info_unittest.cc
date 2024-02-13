// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "eabase/utility/environment.h"
#include "eabase/utility/file_util.h"
#include "eabase/utility/sys_info.h"
#include "eabase/utility/threading/platform_thread.h"
#include "eabase/utility/time/time.h"
#include <gtest/gtest.h>
#include <gtest/gtest.h>

typedef testing::Test SysInfoTest;
using eabase::FilePath;

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
TEST_F(SysInfoTest, MaxSharedMemorySize) {
  // We aren't actually testing that it's correct, just that it's sane.
  EXPECT_GT(eabase::SysInfo::MaxSharedMemorySize(), 0u);
}
#endif

TEST_F(SysInfoTest, NumProcs) {
  // We aren't actually testing that it's correct, just that it's sane.
  EXPECT_GE(eabase::SysInfo::NumberOfProcessors(), 1);
}

TEST_F(SysInfoTest, AmountOfMem) {
  // We aren't actually testing that it's correct, just that it's sane.
  EXPECT_GT(eabase::SysInfo::AmountOfPhysicalMemory(), 0);
  EXPECT_GT(eabase::SysInfo::AmountOfPhysicalMemoryMB(), 0);
  // The maxmimal amount of virtual memory can be zero which means unlimited.
  EXPECT_GE(eabase::SysInfo::AmountOfVirtualMemory(), 0);
}

TEST_F(SysInfoTest, AmountOfFreeDiskSpace) {
  // We aren't actually testing that it's correct, just that it's sane.
  FilePath tmp_path;
  ASSERT_TRUE(eabase::GetTempDir(&tmp_path));
  EXPECT_GT(eabase::SysInfo::AmountOfFreeDiskSpace(tmp_path), 0)
            << tmp_path.value();
}

#if defined(OS_WIN) || defined(OS_MACOSX)
TEST_F(SysInfoTest, OperatingSystemVersionNumbers) {
  int32_t os_major_version = -1;
  int32_t os_minor_version = -1;
  int32_t os_bugfix_version = -1;
  eabase::SysInfo::OperatingSystemVersionNumbers(&os_major_version,
                                               &os_minor_version,
                                               &os_bugfix_version);
  EXPECT_GT(os_major_version, -1);
  EXPECT_GT(os_minor_version, -1);
  EXPECT_GT(os_bugfix_version, -1);
}
#endif

TEST_F(SysInfoTest, Uptime) {
  int64_t up_time_1 = eabase::SysInfo::Uptime();
  // UpTime() is implemented internally using TimeTicks::Now(), which documents
  // system resolution as being 1-15ms. Sleep a little longer than that.
  eabase::PlatformThread::Sleep(eabase::TimeDelta::FromMilliseconds(20));
  int64_t up_time_2 = eabase::SysInfo::Uptime();
  EXPECT_GT(up_time_1, 0);
  EXPECT_GT(up_time_2, up_time_1);
}

#if defined(OS_CHROMEOS)

TEST_F(SysInfoTest, GoogleChromeOSVersionNumbers) {
  int32_t os_major_version = -1;
  int32_t os_minor_version = -1;
  int32_t os_bugfix_version = -1;
  const char* kLsbRelease =
      "FOO=1234123.34.5\n"
      "CHROMEOS_RELEASE_VERSION=1.2.3.4\n";
  eabase::SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease, eabase::Time());
  eabase::SysInfo::OperatingSystemVersionNumbers(&os_major_version,
                                               &os_minor_version,
                                               &os_bugfix_version);
  EXPECT_EQ(1, os_major_version);
  EXPECT_EQ(2, os_minor_version);
  EXPECT_EQ(3, os_bugfix_version);
}

TEST_F(SysInfoTest, GoogleChromeOSVersionNumbersFirst) {
  int32_t os_major_version = -1;
  int32_t os_minor_version = -1;
  int32_t os_bugfix_version = -1;
  const char* kLsbRelease =
      "CHROMEOS_RELEASE_VERSION=1.2.3.4\n"
      "FOO=1234123.34.5\n";
  eabase::SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease, eabase::Time());
  eabase::SysInfo::OperatingSystemVersionNumbers(&os_major_version,
                                               &os_minor_version,
                                               &os_bugfix_version);
  EXPECT_EQ(1, os_major_version);
  EXPECT_EQ(2, os_minor_version);
  EXPECT_EQ(3, os_bugfix_version);
}

TEST_F(SysInfoTest, GoogleChromeOSNoVersionNumbers) {
  int32_t os_major_version = -1;
  int32_t os_minor_version = -1;
  int32_t os_bugfix_version = -1;
  const char* kLsbRelease = "FOO=1234123.34.5\n";
  eabase::SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease, eabase::Time());
  eabase::SysInfo::OperatingSystemVersionNumbers(&os_major_version,
                                               &os_minor_version,
                                               &os_bugfix_version);
  EXPECT_EQ(0, os_major_version);
  EXPECT_EQ(0, os_minor_version);
  EXPECT_EQ(0, os_bugfix_version);
}

TEST_F(SysInfoTest, GoogleChromeOSLsbReleaseTime) {
  const char* kLsbRelease = "CHROMEOS_RELEASE_VERSION=1.2.3.4";
  // Use a fake time that can be safely displayed as a string.
  const eabase::Time lsb_release_time(eabase::Time::FromDoubleT(12345.6));
  eabase::SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease, lsb_release_time);
  eabase::Time parsed_lsb_release_time = eabase::SysInfo::GetLsbReleaseTime();
  EXPECT_DOUBLE_EQ(lsb_release_time.ToDoubleT(),
                   parsed_lsb_release_time.ToDoubleT());
}

TEST_F(SysInfoTest, IsRunningOnChromeOS) {
  eabase::SysInfo::SetChromeOSVersionInfoForTest("", eabase::Time());
  EXPECT_FALSE(eabase::SysInfo::IsRunningOnChromeOS());

  const char* kLsbRelease1 =
      "CHROMEOS_RELEASE_NAME=Non Chrome OS\n"
      "CHROMEOS_RELEASE_VERSION=1.2.3.4\n";
  eabase::SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease1, eabase::Time());
  EXPECT_FALSE(eabase::SysInfo::IsRunningOnChromeOS());

  const char* kLsbRelease2 =
      "CHROMEOS_RELEASE_NAME=Chrome OS\n"
      "CHROMEOS_RELEASE_VERSION=1.2.3.4\n";
  eabase::SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease2, eabase::Time());
  EXPECT_TRUE(eabase::SysInfo::IsRunningOnChromeOS());

  const char* kLsbRelease3 =
      "CHROMEOS_RELEASE_NAME=Chromium OS\n";
  eabase::SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease3, eabase::Time());
  EXPECT_TRUE(eabase::SysInfo::IsRunningOnChromeOS());
}

#endif  // OS_CHROMEOS
