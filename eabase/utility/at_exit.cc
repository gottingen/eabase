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

#include "eabase/utility/at_exit.h"

#include <stddef.h>
#include <ostream>

#include "eabase/utility/logging.h"

namespace eabase {

// Keep a stack of registered AtExitManagers.  We always operate on the most
// recent, and we should never have more than one outside of testing (for a
// statically linked version of this library).  Testing may use the shadow
// version of the constructor, and if we are building a dynamic library we may
// end up with multiple AtExitManagers on the same process.  We don't protect
// this for thread-safe access, since it will only be modified in testing.
static AtExitManager* g_top_manager = NULL;

AtExitManager::AtExitManager() : next_manager_(g_top_manager) {
// If multiple modules instantiate AtExitManagers they'll end up living in this
// module... they have to coexist.
#if !defined(COMPONENT_BUILD)
  DCHECK(!g_top_manager);
#endif
  g_top_manager = this;
}

AtExitManager::~AtExitManager() {
  if (!g_top_manager) {
    NOTREACHED() << "Tried to ~AtExitManager without an AtExitManager";
    return;
  }
  DCHECK_EQ(this, g_top_manager);

  ProcessCallbacksNow();
  g_top_manager = next_manager_;
}

// static
void AtExitManager::RegisterCallback(AtExitCallbackType func, void* param) {
  DCHECK(func);
  if (!g_top_manager) {
    NOTREACHED() << "Tried to RegisterCallback without an AtExitManager";
    return;
  }

  AutoLock lock(g_top_manager->lock_);
  g_top_manager->stack_.push({func, param});
}

// static
void AtExitManager::ProcessCallbacksNow() {
  if (!g_top_manager) {
    NOTREACHED() << "Tried to ProcessCallbacksNow without an AtExitManager";
    return;
  }

  AutoLock lock(g_top_manager->lock_);

  while (!g_top_manager->stack_.empty()) {
    Callback task = g_top_manager->stack_.top();
    task.func(task.param);
    g_top_manager->stack_.pop();
  }
}

AtExitManager::AtExitManager(bool shadow) : next_manager_(g_top_manager) {
  DCHECK(shadow || !g_top_manager);
  g_top_manager = this;
}

}  // namespace eabase
