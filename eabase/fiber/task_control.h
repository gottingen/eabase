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

#ifndef FIBER_TASK_CONTROL_H_
#define FIBER_TASK_CONTROL_H_

#ifndef NDEBUG
#include <iostream>                             // std::ostream
#endif
#include <stddef.h>                             // size_t
#include <vector>
#include <array>
#include <memory>
#include "eabase/utility/atomicops.h"                     // eabase::atomic
#include "eabase/var/var.h"                          // eabase::PassiveStatus
#include "eabase/fiber/task_meta.h"                  // TaskMeta
#include "eabase/utility/resource_pool.h"                 // ResourcePool
#include "eabase/fiber/work_stealing_queue.h"        // WorkStealingQueue
#include "eabase/fiber/parking_lot.h"

DECLARE_int32(task_group_ntags);
namespace eabase {

class TaskGroup;

// Control all task groups
class TaskControl {
    friend class TaskGroup;

public:
    TaskControl();
    ~TaskControl();

    // Must be called before using. `nconcurrency' is # of worker pthreads.
    int init(int nconcurrency);
    
    // Create a TaskGroup in this control.
    TaskGroup* create_group(fiber_tag_t tag);

    // Steal a task from a "random" group.
    bool steal_task(fiber_t* tid, size_t* seed, size_t offset);

    // Tell other groups that `n' tasks was just added to caller's runqueue
    void signal_task(int num_task, fiber_tag_t tag);

    // Stop and join worker threads in TaskControl.
    void stop_and_join();
    
    // Get # of worker threads.
    int concurrency() const 
    { return _concurrency.load(eabase::memory_order_acquire); }

    int concurrency(fiber_tag_t tag) const
    { return _tagged_ngroup[tag].load(eabase::memory_order_acquire); }

    void print_rq_sizes(std::ostream& os);

    double get_cumulated_worker_time();
    double get_cumulated_worker_time_with_tag(fiber_tag_t tag);
    int64_t get_cumulated_switch_count();
    int64_t get_cumulated_signal_count();

    // [Not thread safe] Add more worker threads.
    // Return the number of workers actually added, which may be less than |num|
    int add_workers(int num, fiber_tag_t tag);

    // Choose one TaskGroup (randomly right now).
    // If this method is called after init(), it never returns NULL.
    TaskGroup* choose_one_group(fiber_tag_t tag = FIBER_TAG_DEFAULT);

private:
    typedef std::array<TaskGroup*, FIBER_MAX_CONCURRENCY> TaggedGroups;
    static const int PARKING_LOT_NUM = 4;
    typedef std::array<ParkingLot, PARKING_LOT_NUM> TaggedParkingLot;
    // Add/Remove a TaskGroup.
    // Returns 0 on success, -1 otherwise.
    int _add_group(TaskGroup*, fiber_tag_t tag);
    int _destroy_group(TaskGroup*);

    // Tag group
    TaggedGroups& tag_group(fiber_tag_t tag) { return _tagged_groups[tag]; }

    // Tag ngroup
    eabase::atomic<size_t>& tag_ngroup(int tag) { return _tagged_ngroup[tag]; }

    // Tag parking slot
    TaggedParkingLot& tag_pl(fiber_tag_t tag) { return _pl[tag]; }

    static void delete_task_group(void* arg);

    static void* worker_thread(void* task_control);

    template <typename F>
    void for_each_task_group(F const& f);

    eabase::LatencyRecorder& exposed_pending_time();
    eabase::LatencyRecorder* create_exposed_pending_time();
    eabase::Adder<int64_t>& tag_nworkers(fiber_tag_t tag);
    eabase::Adder<int64_t>& tag_nfibers(fiber_tag_t tag);

    std::vector<eabase::atomic<size_t>> _tagged_ngroup;
    std::vector<TaggedGroups> _tagged_groups;
    eabase::Mutex _modify_group_mutex;

    eabase::atomic<bool> _init;  // if not init, var will case coredump
    bool _stop;
    eabase::atomic<int> _concurrency;
    std::vector<pthread_t> _workers;
    eabase::atomic<int> _next_worker_id;

    eabase::Adder<int64_t> _nworkers;
    eabase::Mutex _pending_time_mutex;
    eabase::atomic<eabase::LatencyRecorder*> _pending_time;
    eabase::PassiveStatus<double> _cumulated_worker_time;
    eabase::PerSecond<eabase::PassiveStatus<double> > _worker_usage_second;
    eabase::PassiveStatus<int64_t> _cumulated_switch_count;
    eabase::PerSecond<eabase::PassiveStatus<int64_t> > _switch_per_second;
    eabase::PassiveStatus<int64_t> _cumulated_signal_count;
    eabase::PerSecond<eabase::PassiveStatus<int64_t> > _signal_per_second;
    eabase::PassiveStatus<std::string> _status;
    eabase::Adder<int64_t> _nfibers;

    std::vector<eabase::Adder<int64_t>*> _tagged_nworkers;
    std::vector<eabase::PassiveStatus<double>*> _tagged_cumulated_worker_time;
    std::vector<eabase::PerSecond<eabase::PassiveStatus<double>>*> _tagged_worker_usage_second;
    std::vector<eabase::Adder<int64_t>*> _tagged_nfibers;

    std::vector<TaggedParkingLot> _pl;
};

inline eabase::LatencyRecorder& TaskControl::exposed_pending_time() {
    eabase::LatencyRecorder* pt = _pending_time.load(eabase::memory_order_consume);
    if (!pt) {
        pt = create_exposed_pending_time();
    }
    return *pt;
}

inline eabase::Adder<int64_t>& TaskControl::tag_nworkers(fiber_tag_t tag) {
    return *_tagged_nworkers[tag];
}

inline eabase::Adder<int64_t>& TaskControl::tag_nfibers(fiber_tag_t tag) {
    return *_tagged_nfibers[tag];
}

template <typename F>
inline void TaskControl::for_each_task_group(F const& f) {
    if (_init.load(eabase::memory_order_acquire) == false) {
        return;
    }
    for (size_t i = 0; i < _tagged_groups.size(); ++i) {
        auto ngroup = tag_ngroup(i).load(eabase::memory_order_relaxed);
        auto& groups = tag_group(i);
        for (size_t j = 0; j < ngroup; ++j) {
            f(groups[j]);
        }
    }
}

}  // namespace eabase

#endif  // FIBER_TASK_CONTROL_H_
