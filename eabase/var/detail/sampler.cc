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

#include <gflags/gflags.h>
#include "eabase/utility/threading/platform_thread.h"
#include "eabase/utility/time.h"
#include "eabase/utility/memory/singleton_on_pthread_once.h"
#include "eabase/var/reducer.h"
#include "eabase/var/detail/sampler.h"
#include "eabase/var/passive_status.h"
#include "eabase/var/window.h"

namespace eabase {
namespace detail {

const int WARN_NOSLEEP_THRESHOLD = 2;

// Combine two circular linked list into one.
struct CombineSampler {
    void operator()(Sampler* & s1, Sampler* s2) const {
        if (s2 == NULL) {
            return;
        }
        if (s1 == NULL) {
            s1 = s2;
            return;
        }
        s1->InsertBeforeAsList(s2);
    }
};

// True iff pthread_atfork was called. The callback to atfork works for child
// of child as well, no need to register in the child again.
static bool registered_atfork = false;

// Call take_sample() of all scheduled samplers.
// This can be done with regular timer thread, but it's way too slow(global
// contention + log(N) heap manipulations). We need it to be super fast so that
// creation overhead of Window<> is negliable.
// The trick is to use Reducer<Sampler*, CombineSampler>. Each Sampler is
// doubly linked, thus we can reduce multiple Samplers into one cicurlarly
// doubly linked list, and multiple lists into larger lists. We create a
// dedicated thread to periodically get_value() which is just the combined
// list of Samplers. Waking through the list and call take_sample().
// If a Sampler needs to be deleted, we just mark it as unused and the
// deletion is taken place in the thread as well.
class SamplerCollector : public eabase::Reducer<Sampler*, CombineSampler> {
public:
    SamplerCollector()
        : _created(false)
        , _stop(false)
        , _cumulated_time_us(0) {
        create_sampling_thread();
    }
    ~SamplerCollector() {
        if (_created) {
            _stop = true;
            pthread_join(_tid, NULL);
            _created = false;
        }
    }

private:
    // Support for fork:
    // * The singleton can be null before forking, the child callback will not
    //   be registered.
    // * If the singleton is not null before forking, the child callback will
    //   be registered and the sampling thread will be re-created.
    // * A forked program can be forked again.

    static void child_callback_atfork() {
        eabase::get_leaky_singleton<SamplerCollector>()->after_forked_as_child();
    }

    void create_sampling_thread() {
        const int rc = pthread_create(&_tid, NULL, sampling_thread, this);
        if (rc != 0) {
            LOG(FATAL) << "Fail to create sampling_thread, " << berror(rc);
        } else {
            _created = true;
            if (!registered_atfork) {
                registered_atfork = true;
                pthread_atfork(NULL, NULL, child_callback_atfork);
            }
        }
    }

    void after_forked_as_child() {
        _created = false;
        create_sampling_thread();
    }

    void run();

    static void* sampling_thread(void* arg) {
        eabase::PlatformThread::SetName("var_sampler");
        static_cast<SamplerCollector*>(arg)->run();
        return NULL;
    }

    static double get_cumulated_time(void* arg) {
        return static_cast<SamplerCollector*>(arg)->_cumulated_time_us / 1000.0 / 1000.0;
    }

private:
    bool _created;
    bool _stop;
    int64_t _cumulated_time_us;
    pthread_t _tid;
};

#ifndef UNIT_TEST
static PassiveStatus<double>* s_cumulated_time_var = NULL;
static eabase::PerSecond<eabase::PassiveStatus<double> >* s_sampling_thread_usage_var = NULL;
#endif

DEFINE_int32(var_sampler_thread_start_delay_us, 10000, "var sampler thread start delay us");

void SamplerCollector::run() {
    ::usleep(FLAGS_var_sampler_thread_start_delay_us);
    
#ifndef UNIT_TEST
    // NOTE:
    // * Following vars can't be created on thread's stack since this thread
    //   may be abandoned at any time after forking.
    // * They can't created inside the constructor of SamplerCollector as well,
    //   which results in deadlock.
    if (s_cumulated_time_var == NULL) {
        s_cumulated_time_var =
            new PassiveStatus<double>(get_cumulated_time, this);
    }
    if (s_sampling_thread_usage_var == NULL) {
        s_sampling_thread_usage_var =
            new eabase::PerSecond<eabase::PassiveStatus<double> >(
                    "var_sampler_collector_usage", s_cumulated_time_var, 10);
    }
#endif

    eabase::LinkNode<Sampler> root;
    int consecutive_nosleep = 0;
    while (!_stop) {
        int64_t abstime = eabase::gettimeofday_us();
        Sampler* s = this->reset();
        if (s) {
            s->InsertBeforeAsList(&root);
        }
        for (eabase::LinkNode<Sampler>* p = root.next(); p != &root;) {
            // We may remove p from the list, save next first.
            eabase::LinkNode<Sampler>* saved_next = p->next();
            Sampler* s = p->value();
            s->_mutex.lock();
            if (!s->_used) {
                s->_mutex.unlock();
                p->RemoveFromList();
                delete s;
            } else {
                s->take_sample();
                s->_mutex.unlock();
            }
            p = saved_next;
        }
        bool slept = false;
        int64_t now = eabase::gettimeofday_us();
        _cumulated_time_us += now - abstime;
        abstime += 1000000L;
        while (abstime > now) {
            ::usleep(abstime - now);
            slept = true;
            now = eabase::gettimeofday_us();
        }
        if (slept) {
            consecutive_nosleep = 0;
        } else {            
            if (++consecutive_nosleep >= WARN_NOSLEEP_THRESHOLD) {
                consecutive_nosleep = 0;
                LOG(WARNING) << "var is busy at sampling for "
                             << WARN_NOSLEEP_THRESHOLD << " seconds!";
            }
        }
    }
}

Sampler::Sampler() : _used(true) {}

Sampler::~Sampler() {}

DEFINE_bool(var_enable_sampling, true, "is enable var sampling");

void Sampler::schedule() {
    // since the SamplerCollector is initialized before the program starts
    // flags will not take effect if used in the SamplerCollector constructor
    if (FLAGS_var_enable_sampling) {
        *eabase::get_leaky_singleton<SamplerCollector>() << this;
    }
}

void Sampler::destroy() {
    _mutex.lock();
    _used = false;
    _mutex.unlock();
}

}  // namespace detail
}  // namespace eabase
