// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "eabase/utility/debug/dump_without_crashing.h"

#include "eabase/utility/logging.h"

namespace {

// Pointer to the function that's called by DumpWithoutCrashing() to dump the
// process's memory.
void (CDECL *dump_without_crashing_function_)() = NULL;

}  // namespace

namespace eabase {

namespace debug {

void DumpWithoutCrashing() {
  if (dump_without_crashing_function_)
    (*dump_without_crashing_function_)();
}

void SetDumpWithoutCrashingFunction(void (CDECL *function)()) {
  dump_without_crashing_function_ = function;
}

}  // namespace debug

}  // namespace eabase
