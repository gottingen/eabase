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

#ifndef FIBER_REMOTE_TASK_QUEUE_H_
#define FIBER_REMOTE_TASK_QUEUE_H_

#include "eabase/utility/containers/bounded_queue.h"
#include "eabase/utility/macros.h"

namespace eabase {

class TaskGroup;

// A queue for storing fibers created by non-workers. Since non-workers
// randomly choose a TaskGroup to push which distributes the contentions,
// this queue is simply implemented as a queue protected with a lock.
// The function names should be self-explanatory.
class RemoteTaskQueue {
public:
    RemoteTaskQueue() {}

    int init(size_t cap) {
        const size_t memsize = sizeof(fiber_t) * cap;
        void* q_mem = malloc(memsize);
        if (q_mem == NULL) {
            return -1;
        }
        eabase::BoundedQueue<fiber_t> q(q_mem, memsize, eabase::OWNS_STORAGE);
        _tasks.swap(q);
        return 0;
    }

    bool pop(fiber_t* task) {
        if (_tasks.empty()) {
            return false;
        }
        _mutex.lock();
        const bool result = _tasks.pop(task);
        _mutex.unlock();
        return result;
    }

    bool push(fiber_t task) {
        _mutex.lock();
        const bool res = push_locked(task);
        _mutex.unlock();
        return res;
    }

    bool push_locked(fiber_t task) {
        return _tasks.push(task);
    }

    size_t capacity() const { return _tasks.capacity(); }
    
private:
friend class TaskGroup;
    EA_DISALLOW_COPY_AND_ASSIGN(RemoteTaskQueue);
    eabase::BoundedQueue<fiber_t> _tasks;
    eabase::Mutex _mutex;
};

}  // namespace eabase

#endif  // FIBER_REMOTE_TASK_QUEUE_H_
