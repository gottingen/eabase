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
#ifndef EABASE_VAR_COLLECTOR_H_
#define EABASE_VAR_COLLECTOR_H_

#include "eabase/utility/containers/linked_list.h"
#include "eabase/utility/fast_rand.h"
#include "eabase/utility/time.h"
#include "eabase/utility/atomicops.h"
#include "eabase/var/passive_status.h"

namespace eabase {

// Containing the context for limiting sampling speed.
struct CollectorSpeedLimit {
    // [Managed by Collector, don't change!]
    size_t sampling_range;
    bool ever_grabbed;
    eabase::static_atomic<int> count_before_grabbed;
    int64_t first_sample_real_us;
};

static const size_t COLLECTOR_SAMPLING_BASE = 16384;

#define VAR_COLLECTOR_SPEED_LIMIT_INITIALIZER                          \
    { ::eabase::COLLECTOR_SAMPLING_BASE, false, BUTIL_STATIC_ATOMIC_INIT(0), 0 }

class Collected;

// For processing samples in batch before dumping.
class CollectorPreprocessor {
public:
    virtual ~CollectorPreprocessor() = default;
    virtual void process(std::vector<Collected*>& samples) = 0;
};

// Steps for sampling and dumping sth:
//  1. Implement Collected
//  2. Create an instance and fill in data.
//  3. submit() the instance.
class Collected : public eabase::LinkNode<Collected> {
public:
    virtual ~Collected() {}
    
    // Sumbit the sample for later dumping, a sample can only be submitted once.
    // submit() is implemented as writing a value to eabase::Reducer which does
    // not compete globally. This function generally does not alter the
    // interleaving status of threads even in highly contended situations.
    // You should also create the sample using a malloc() impl. that are
    // unlikely to contend, keeping interruptions minimal.
    // `cpuwide_us' should be got from eabase::cpuwide_time_us(). If it's far
    // from the timestamp updated by collecting thread(which basically means
    // the thread is not scheduled by OS in time), this sample is directly
    // destroy()-ed to avoid memory explosion.
    void submit(int64_t cpuwide_us);
    void submit() { submit(eabase::cpuwide_time_us()); }

    // Implement this method to dump the sample into files and destroy it.
    // This method is called in a separate thread and can be blocked
    // indefinitely long(not recommended). If too many samples wait for
    // this funcion due to previous sample's blocking, they'll be destroy()-ed.
    // If you need to run destruction code upon thread's exit, use
    // eabase::thread_atexit. Dumping thread run this function in batch, each
    // batch is counted as one "round", `round_index' is the round that
    // dumping thread is currently at, counting from 1.
    virtual void dump_and_destroy(size_t round_index) = 0;

    // Destroy the sample. Will be called for at most once. Since dumping
    // thread generally quits upon the termination of program, some samples
    // are directly recycled along with program w/o calling destroy().
    virtual void destroy() = 0;

    // Returns an object to control #samples collected per second.
    // If NULL is returned, samples collected per second is limited by a
    // global speed limit shared with other samples also returning NULL.
    // All instances of a subclass of Collected should return a same instance
    // of CollectorSpeedLimit. The instance should remain valid during lifetime
    // of program.
    virtual CollectorSpeedLimit* speed_limit() = 0;

    // If this method returns a non-NULL instance, it will be applied to
    // samples in batch before dumping. You can sort or shuffle the samples
    // in the impl.
    // All instances of a subclass of Collected should return a same instance
    // of CollectorPreprocessor. The instance should remain valid during
    // lifetime of program.
    virtual CollectorPreprocessor* preprocessor() { return NULL; }
};

// To know if an instance should be sampled.
// Returns a positive number when the object should be sampled, 0 otherwise.
// The number is approximately the current probability of sampling times
// COLLECTOR_SAMPLING_BASE, it varies from seconds to seconds being adjusted
// by collecting thread to control the samples collected per second.
// This function should cost less than 10ns in most cases.
inline size_t is_collectable(CollectorSpeedLimit* speed_limit) {
    if (speed_limit->ever_grabbed) { // most common case
        const size_t sampling_range = speed_limit->sampling_range;
        // fast_rand is faster than fast_rand_in
        if ((eabase::fast_rand() & (COLLECTOR_SAMPLING_BASE - 1)) >= sampling_range) {
            return 0;
        }
        return sampling_range;
    }
    // Slower, only runs before -var_collector_expected_per_second samples are
    // collected to calculate a more reasonable sampling_range for the type.
    extern size_t is_collectable_before_first_time_grabbed(CollectorSpeedLimit*);
    return is_collectable_before_first_time_grabbed(speed_limit);
}

// An utility for displaying current sampling ratio according to speed limit.
class DisplaySamplingRatio {
public:
    DisplaySamplingRatio(const char* name, const CollectorSpeedLimit*);
private:
    eabase::PassiveStatus<double> _var;
};

}  // namespace eabase

#endif  // EABASE_VAR_COLLECTOR_H_
