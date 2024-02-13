// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "eabase/utility/guid.h"

#include "eabase/utility/rand_util.h"
#include "eabase/utility/strings/stringprintf.h"

namespace eabase {

std::string GenerateGUID() {
  uint64_t sixteen_bytes[2] = { eabase::RandUint64(), eabase::RandUint64() };
  return RandomDataToGUIDString(sixteen_bytes);
}

// TODO(cmasone): Once we're comfortable this works, migrate Windows code to
// use this as well.
std::string RandomDataToGUIDString(const uint64_t bytes[2]) {
  return StringPrintf("%08X-%04X-%04X-%04X-%012llX",
                      static_cast<unsigned int>(bytes[0] >> 32),
                      static_cast<unsigned int>((bytes[0] >> 16) & 0x0000ffff),
                      static_cast<unsigned int>(bytes[0] & 0x0000ffff),
                      static_cast<unsigned int>(bytes[1] >> 48),
                      bytes[1] & 0x0000ffffffffffffULL);
}

}  // namespace guid
