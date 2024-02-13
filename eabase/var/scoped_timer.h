// Copyright 2023 The EA Authors.
// part of Elastic AI Search
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//

#ifndef  EABASE_VAR_SCOPED_TIMER_H_
#define  EABASE_VAR_SCOPED_TIMER_H_

#include "eabase/utility/time.h"

// Accumulate microseconds spent by scopes into var, useful for debugging.
// Example:
//   eabase::Adder<int64_t> g_function1_spent;
//   ...
//   void function1() {
//     // time cost by function1() will be sent to g_spent_time when
//     // the function returns.
//     eabase::ScopedTimer tm(g_function1_spent);
//     ...
//   }
// To check how many microseconds the function spend in last second, you
// can wrap the var within PerSecond and make it viewable from /vars
//   eabase::PerSecond<eabase::Adder<int64_t> > g_function1_spent_second(
//     "function1_spent_second", &g_function1_spent);
namespace eabase{
template <typename T>
class ScopedTimer {
public:
    explicit ScopedTimer(T& var)
        : _start_time(eabase::cpuwide_time_us()), _var(&var) {}

    ~ScopedTimer() {
        *_var << (eabase::cpuwide_time_us() - _start_time);
    }

    void reset() { _start_time = eabase::cpuwide_time_us(); }

private:
    EA_DISALLOW_COPY_AND_ASSIGN(ScopedTimer);
    int64_t _start_time;
    T* _var;
};
} // namespace eabase

#endif  // EABASE_VAR_SCOPED_TIMER_H_
