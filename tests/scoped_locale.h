// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUTIL_TEST_SCOPED_LOCALE_H_
#define BUTIL_TEST_SCOPED_LOCALE_H_

#include <string>

#include "eabase/utility/basictypes.h"

namespace eabase {

// Sets the given |locale| on construction, and restores the previous locale
// on destruction.
class ScopedLocale {
 public:
  explicit ScopedLocale(const std::string& locale);
  ~ScopedLocale();

 private:
  std::string prev_locale_;

  EA_DISALLOW_COPY_AND_ASSIGN(ScopedLocale);
};

}  // namespace eabase

#endif  // BUTIL_TEST_SCOPED_LOCALE_H_
