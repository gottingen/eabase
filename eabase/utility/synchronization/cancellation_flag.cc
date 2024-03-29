// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "eabase/utility/synchronization/cancellation_flag.h"

#include "eabase/utility/logging.h"

namespace eabase {

void CancellationFlag::Set() {
#if !defined(NDEBUG)
  DCHECK_EQ(set_on_, PlatformThread::CurrentId());
#endif
  eabase::subtle::Release_Store(&flag_, 1);
}

bool CancellationFlag::IsSet() const {
  return eabase::subtle::Acquire_Load(&flag_) != 0;
}

}  // namespace eabase
