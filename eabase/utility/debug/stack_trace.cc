// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "eabase/utility/debug/stack_trace.h"

#include "eabase/utility/basictypes.h"

#include <string.h>

#include <algorithm>
#include <sstream>

namespace eabase {
namespace debug {

StackTrace::StackTrace(const void* const* trace, size_t count) {
  count = std::min(count, arraysize(trace_));
  if (count)
    memcpy(trace_, trace, count * sizeof(trace_[0]));
  count_ = count;
}

StackTrace::~StackTrace() {
}

const void *const *StackTrace::Addresses(size_t* count) const {
  *count = count_;
  if (count_)
    return trace_;
  return NULL;
}

std::string StackTrace::ToString() const {
  std::stringstream stream;
#if !defined(__UCLIBC__)
  OutputToStream(&stream);
#endif
  return stream.str();
}

}  // namespace debug
}  // namespace eabase
