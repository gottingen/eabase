// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUTIL_DEBUG_DUMP_WITHOUT_CRASHING_H_
#define BUTIL_DEBUG_DUMP_WITHOUT_CRASHING_H_

#include "eabase/utility/base_export.h"
#include "eabase/utility/compiler_specific.h"
#include "eabase/utility/build_config.h"

namespace eabase {

namespace debug {

// Handler to silently dump the current process without crashing.
BUTIL_EXPORT void DumpWithoutCrashing();

// Sets a function that'll be invoked to dump the current process when
// DumpWithoutCrashing() is called.
BUTIL_EXPORT void SetDumpWithoutCrashingFunction(void (CDECL *function)());

}  // namespace debug

}  // namespace eabase

#endif  // BUTIL_DEBUG_DUMP_WITHOUT_CRASHING_H_
