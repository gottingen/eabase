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
#include "eabase/utility/macros.h"                       // BAIDU_CASSERT
#include "eabase/utility/logging.h"
#include "eabase/fiber/task_group.h"                // TaskGroup
#include "eabase/fiber/task_control.h"              // TaskControl
#include "eabase/fiber/timer_thread.h"
#include "eabase/fiber/list_of_abafree_id.h"
#include "eabase/fiber/fiber.h"

namespace eabase {

DEFINE_int32(fiber_concurrency, 8 + FIBER_EPOLL_THREAD_NUM,
             "Number of pthread workers");

DEFINE_int32(fiber_min_concurrency, 0,
            "Initial number of pthread workers which will be added on-demand."
            " The laziness is disabled when this value is non-positive,"
            " and workers will be created eagerly according to -fiber_concurrency and fiber_setconcurrency(). ");

DEFINE_int32(fiber_current_tag, FIBER_TAG_DEFAULT, "Set fiber concurrency for this tag");

DEFINE_int32(fiber_concurrency_by_tag, 0,
             "Number of pthread workers of FLAGS_fiber_current_tag");

static bool never_set_fiber_concurrency = true;
static bool never_set_fiber_concurrency_by_tag = true;

static bool validate_fiber_concurrency(const char*, int32_t val) {
    // fiber_setconcurrency sets the flag on success path which should
    // not be strictly in a validator. But it's OK for a int flag.
    return fiber_setconcurrency(val) == 0;
}
const int ALLOW_UNUSED register_FLAGS_fiber_concurrency =
    ::GFLAGS_NS::RegisterFlagValidator(&FLAGS_fiber_concurrency,
                                    validate_fiber_concurrency);

static bool validate_fiber_min_concurrency(const char*, int32_t val);

const int ALLOW_UNUSED register_FLAGS_fiber_min_concurrency =
    ::GFLAGS_NS::RegisterFlagValidator(&FLAGS_fiber_min_concurrency,
                                    validate_fiber_min_concurrency);

static bool validate_fiber_current_tag(const char*, int32_t val);

const int ALLOW_UNUSED register_FLAGS_fiber_current_tag =
    ::GFLAGS_NS::RegisterFlagValidator(&FLAGS_fiber_current_tag, validate_fiber_current_tag);

static bool validate_fiber_concurrency_by_tag(const char*, int32_t val);

const int ALLOW_UNUSED register_FLAGS_fiber_concurrency_by_tag =
    ::GFLAGS_NS::RegisterFlagValidator(&FLAGS_fiber_concurrency_by_tag,
                                       validate_fiber_concurrency_by_tag);

    static_assert(sizeof(TaskControl*) == sizeof(eabase::atomic<TaskControl*>), "atomic size match");

pthread_mutex_t g_task_control_mutex = PTHREAD_MUTEX_INITIALIZER;
// Referenced in rpc, needs to be extern.
// Notice that we can't declare the variable as atomic<TaskControl*> which
// are not constructed before main().
TaskControl* g_task_control = NULL;

extern BAIDU_THREAD_LOCAL TaskGroup* tls_task_group;
extern void (*g_worker_startfn)();
extern void (*g_tagged_worker_startfn)(fiber_tag_t);

inline TaskControl* get_task_control() {
    return g_task_control;
}

inline TaskControl* get_or_new_task_control() {
    eabase::atomic<TaskControl*>* p = (eabase::atomic<TaskControl*>*)&g_task_control;
    TaskControl* c = p->load(eabase::memory_order_consume);
    if (c != NULL) {
        return c;
    }
    BAIDU_SCOPED_LOCK(g_task_control_mutex);
    c = p->load(eabase::memory_order_consume);
    if (c != NULL) {
        return c;
    }
    c = new (std::nothrow) TaskControl;
    if (NULL == c) {
        return NULL;
    }
    int concurrency = FLAGS_fiber_min_concurrency > 0 ?
        FLAGS_fiber_min_concurrency :
        FLAGS_fiber_concurrency;
    if (c->init(concurrency) != 0) {
        LOG(ERROR) << "Fail to init g_task_control";
        delete c;
        return NULL;
    }
    p->store(c, eabase::memory_order_release);
    return c;
}

static int add_workers_for_each_tag(int num) {
    int added = 0;
    auto c = get_task_control();
    for (auto i = 0; i < num; ++i) {
        added += c->add_workers(1, i % FLAGS_task_group_ntags);
    }
    return added;
}

static bool validate_fiber_min_concurrency(const char*, int32_t val) {
    if (val <= 0) {
        return true;
    }
    if (val < FIBER_MIN_CONCURRENCY || val > FLAGS_fiber_concurrency) {
        return false;
    }
    TaskControl* c = get_task_control();
    if (!c) {
        return true;
    }
    BAIDU_SCOPED_LOCK(g_task_control_mutex);
    int concurrency = c->concurrency();
    if (val > concurrency) {
        int added = eabase::add_workers_for_each_tag(val - concurrency);
        return added == (val - concurrency);
    } else {
        return true;
    }
}

static bool validate_fiber_current_tag(const char*, int32_t val) {
    if (val < FIBER_TAG_DEFAULT || val >= FLAGS_task_group_ntags) {
        return false;
    }
    BAIDU_SCOPED_LOCK(eabase::g_task_control_mutex);
    auto c = eabase::get_task_control();
    if (c == NULL) {
        FLAGS_fiber_concurrency_by_tag = 0;
        return true;
    }
    FLAGS_fiber_concurrency_by_tag = c->concurrency(val);
    return true;
}

static bool validate_fiber_concurrency_by_tag(const char*, int32_t val) {
    return fiber_setconcurrency_by_tag(val, FLAGS_fiber_current_tag) == 0;
}

__thread TaskGroup* tls_task_group_nosignal = NULL;

BUTIL_FORCE_INLINE int
start_from_non_worker(fiber_t* __restrict tid,
                      const fiber_attr_t* __restrict attr,
                      void* (*fn)(void*),
                      void* __restrict arg) {
    TaskControl* c = get_or_new_task_control();
    if (NULL == c) {
        return ENOMEM;
    }
    auto tag = FIBER_TAG_DEFAULT;
    if (attr != NULL && attr->tag != FIBER_TAG_INVALID) {
        tag = attr->tag;
    }
    if (attr != NULL && (attr->flags & FIBER_NOSIGNAL)) {
        // Remember the TaskGroup to insert NOSIGNAL tasks for 2 reasons:
        // 1. NOSIGNAL is often for creating many fibers in batch,
        //    inserting into the same TaskGroup maximizes the batch.
        // 2. fiber_flush() needs to know which TaskGroup to flush.
        auto g = tls_task_group_nosignal;
        if (NULL == g) {
            g = c->choose_one_group(tag);
            tls_task_group_nosignal = g;
        }
        return g->start_background<true>(tid, attr, fn, arg);
    }
    return c->choose_one_group(tag)->start_background<true>(tid, attr, fn, arg);
}

// Meet one of the three conditions, can run in thread local
// attr is nullptr
// tag equal to thread local
// tag equal to FIBER_TAG_INVALID
BUTIL_FORCE_INLINE bool can_run_thread_local(const fiber_attr_t* __restrict attr) {
    return attr == nullptr || attr->tag == eabase::tls_task_group->tag() ||
           attr->tag == FIBER_TAG_INVALID;
}

struct TidTraits {
    static const size_t BLOCK_SIZE = 63;
    static const size_t MAX_ENTRIES = 65536;
    static const fiber_t ID_INIT;
    static bool exists(fiber_t id) { return eabase::TaskGroup::exists(id); }
};
const fiber_t TidTraits::ID_INIT = INVALID_FIBER;

typedef ListOfABAFreeId<fiber_t, TidTraits> TidList;

struct TidStopper {
    void operator()(fiber_t id) const { fiber_stop(id); }
};
struct TidJoiner {
    void operator()(fiber_t & id) const {
        fiber_join(id, NULL);
        id = INVALID_FIBER;
    }
};

}  // namespace eabase

extern "C" {

int fiber_start(fiber_t* __restrict tid,
                         const fiber_attr_t* __restrict attr,
                         void * (*fn)(void*),
                         void* __restrict arg) {
    eabase::TaskGroup* g = eabase::tls_task_group;
    if (g) {
        // if attribute is null use thread local task group
        if (eabase::can_run_thread_local(attr)) {
            return eabase::TaskGroup::start_foreground(&g, tid, attr, fn, arg);
        }
    }
    return eabase::start_from_non_worker(tid, attr, fn, arg);
}

int fiber_start_lazy(fiber_t* __restrict tid,
                             const fiber_attr_t* __restrict attr,
                             void * (*fn)(void*),
                             void* __restrict arg) {
    eabase::TaskGroup* g = eabase::tls_task_group;
    if (g) {
        // if attribute is null use thread local task group
        if (eabase::can_run_thread_local(attr)) {
            return g->start_background<false>(tid, attr, fn, arg);
        }
    }
    return eabase::start_from_non_worker(tid, attr, fn, arg);
}

void fiber_flush() {
    eabase::TaskGroup* g = eabase::tls_task_group;
    if (g) {
        return g->flush_nosignal_tasks();
    }
    g = eabase::tls_task_group_nosignal;
    if (g) {
        // NOSIGNAL tasks were created in this non-worker.
        eabase::tls_task_group_nosignal = NULL;
        return g->flush_nosignal_tasks_remote();
    }
}

int fiber_interrupt(fiber_t tid) {
    return eabase::TaskGroup::interrupt(tid, eabase::get_task_control());
}

int fiber_stop(fiber_t tid) {
    eabase::TaskGroup::set_stopped(tid);
    return fiber_interrupt(tid);
}

int fiber_stopped(fiber_t tid) {
    return (int)eabase::TaskGroup::is_stopped(tid);
}

fiber_t fiber_self(void) {
    eabase::TaskGroup* g = eabase::tls_task_group;
    // note: return 0 for main tasks now, which include main thread and
    // all work threads. So that we can identify main tasks from logs
    // more easily. This is probably questionable in future.
    if (g != NULL && !g->is_current_main_task()/*note*/) {
        return g->current_tid();
    }
    return INVALID_FIBER;
}

int fiber_equal(fiber_t t1, fiber_t t2) {
    return t1 == t2;
}

void fiber_exit(void* retval) {
    eabase::TaskGroup* g = eabase::tls_task_group;
    if (g != NULL && !g->is_current_main_task()) {
        throw eabase::ExitException(retval);
    } else {
        pthread_exit(retval);
    }
}

int fiber_join(fiber_t tid, void** thread_return) {
    return eabase::TaskGroup::join(tid, thread_return);
}

int fiber_attr_init(fiber_attr_t* a) {
    *a = FIBER_ATTR_NORMAL;
    return 0;
}

int fiber_attr_destroy(fiber_attr_t*) {
    return 0;
}

int fiber_getattr(fiber_t tid, fiber_attr_t* attr) {
    return eabase::TaskGroup::get_attr(tid, attr);
}

int fiber_getconcurrency(void) {
    return eabase::FLAGS_fiber_concurrency;
}

int fiber_setconcurrency(int num) {
    if (num < FIBER_MIN_CONCURRENCY || num > FIBER_MAX_CONCURRENCY) {
        LOG(ERROR) << "Invalid concurrency=" << num;
        return EINVAL;
    }
    if (eabase::FLAGS_fiber_min_concurrency > 0) {
        if (num < eabase::FLAGS_fiber_min_concurrency) {
            return EINVAL;
        }
        if (eabase::never_set_fiber_concurrency) {
            eabase::never_set_fiber_concurrency = false;
        }
        eabase::FLAGS_fiber_concurrency = num;
        return 0;
    }
    eabase::TaskControl* c = eabase::get_task_control();
    if (c != NULL) {
        if (num < c->concurrency()) {
            return EPERM;
        } else if (num == c->concurrency()) {
            return 0;
        }
    }
    BAIDU_SCOPED_LOCK(eabase::g_task_control_mutex);
    c = eabase::get_task_control();
    if (c == NULL) {
        if (eabase::never_set_fiber_concurrency) {
            eabase::never_set_fiber_concurrency = false;
            eabase::FLAGS_fiber_concurrency = num;
        } else if (num > eabase::FLAGS_fiber_concurrency) {
            eabase::FLAGS_fiber_concurrency = num;
        }
        return 0;
    }
    if (eabase::FLAGS_fiber_concurrency != c->concurrency()) {
        LOG(ERROR) << "CHECK failed: fiber_concurrency="
                   << eabase::FLAGS_fiber_concurrency
                   << " != tc_concurrency=" << c->concurrency();
        eabase::FLAGS_fiber_concurrency = c->concurrency();
    }
    if (num > eabase::FLAGS_fiber_concurrency) {
        // Create more workers if needed.
        auto added = eabase::add_workers_for_each_tag(num - eabase::FLAGS_fiber_concurrency);
        eabase::FLAGS_fiber_concurrency += added;
    }
    return (num == eabase::FLAGS_fiber_concurrency ? 0 : EPERM);
}

int fiber_getconcurrency_by_tag(fiber_tag_t tag) {
    BAIDU_SCOPED_LOCK(eabase::g_task_control_mutex);
    auto c = eabase::get_task_control();
    if (c == NULL) {
        return EPERM;
    }
    return c->concurrency(tag);
}

int fiber_setconcurrency_by_tag(int num, fiber_tag_t tag) {
    if (eabase::never_set_fiber_concurrency_by_tag) {
        eabase::never_set_fiber_concurrency_by_tag = false;
        return 0;
    }
    BAIDU_SCOPED_LOCK(eabase::g_task_control_mutex);
    auto c = eabase::get_task_control();
    if (c == NULL) {
        return EPERM;
    }
    auto ngroup = c->concurrency();
    auto tag_ngroup = c->concurrency(tag);
    auto add = num - tag_ngroup;
    if (ngroup + add > eabase::FLAGS_fiber_concurrency) {
        LOG(ERROR) << "Fail to set concurrency by tag " << tag
                   << ", Whole concurrency larger than fiber_concurrency";
        return EPERM;
    }
    auto added = 0;
    if (add > 0) {
        added = c->add_workers(add, tag);
        return (add == added ? 0 : EPERM);
    }
    return (num == tag_ngroup ? 0 : EPERM);
}

int fiber_about_to_quit() {
    eabase::TaskGroup* g = eabase::tls_task_group;
    if (g != NULL) {
        eabase::TaskMeta* current_task = g->current_task();
        if(!(current_task->attr.flags & FIBER_NEVER_QUIT)) {
            current_task->about_to_quit = true;
        }
        return 0;
    }
    return EPERM;
}

int fiber_timer_add(fiber_timer_t* id, timespec abstime,
                      void (*on_timer)(void*), void* arg) {
    eabase::TaskControl* c = eabase::get_or_new_task_control();
    if (c == NULL) {
        return ENOMEM;
    }
    eabase::TimerThread* tt = eabase::get_or_create_global_timer_thread();
    if (tt == NULL) {
        return ENOMEM;
    }
    fiber_timer_t tmp = tt->schedule(on_timer, arg, abstime);
    if (tmp != 0) {
        *id = tmp;
        return 0;
    }
    return ESTOP;
}

int fiber_timer_del(fiber_timer_t id) {
    eabase::TaskControl* c = eabase::get_task_control();
    if (c != NULL) {
        eabase::TimerThread* tt = eabase::get_global_timer_thread();
        if (tt == NULL) {
            return EINVAL;
        }
        const int state = tt->unschedule(id);
        if (state >= 0) {
            return state;
        }
    }
    return EINVAL;
}

int fiber_usleep(uint64_t microseconds) {
    eabase::TaskGroup* g = eabase::tls_task_group;
    if (NULL != g && !g->is_current_pthread_task()) {
        return eabase::TaskGroup::usleep(&g, microseconds);
    }
    return ::usleep(microseconds);
}

int fiber_yield(void) {
    eabase::TaskGroup* g = eabase::tls_task_group;
    if (NULL != g && !g->is_current_pthread_task()) {
        eabase::TaskGroup::yield(&g);
        return 0;
    }
    // pthread_yield is not available on MAC
    return sched_yield();
}

int fiber_set_worker_startfn(void (*start_fn)()) {
    if (start_fn == NULL) {
        return EINVAL;
    }
    eabase::g_worker_startfn = start_fn;
    return 0;
}

int fiber_set_tagged_worker_startfn(void (*start_fn)(fiber_tag_t)) {
    if (start_fn == NULL) {
        return EINVAL;
    }
    eabase::g_tagged_worker_startfn = start_fn;
    return 0;
}

void fiber_stop_world() {
    eabase::TaskControl* c = eabase::get_task_control();
    if (c != NULL) {
        c->stop_and_join();
    }
}

int fiber_list_init(fiber_list_t* list,
                      unsigned /*size*/,
                      unsigned /*conflict_size*/) {
    list->impl = new (std::nothrow) eabase::TidList;
    if (NULL == list->impl) {
        return ENOMEM;
    }
    // Set unused fields to zero as well.
    list->head = 0;
    list->size = 0;
    list->conflict_head = 0;
    list->conflict_size = 0;
    return 0;
}

void fiber_list_destroy(fiber_list_t* list) {
    delete static_cast<eabase::TidList*>(list->impl);
    list->impl = NULL;
}

int fiber_list_add(fiber_list_t* list, fiber_t id) {
    if (list->impl == NULL) {
        return EINVAL;
    }
    return static_cast<eabase::TidList*>(list->impl)->add(id);
}

int fiber_list_stop(fiber_list_t* list) {
    if (list->impl == NULL) {
        return EINVAL;
    }
    static_cast<eabase::TidList*>(list->impl)->apply(eabase::TidStopper());
    return 0;
}

int fiber_list_join(fiber_list_t* list) {
    if (list->impl == NULL) {
        return EINVAL;
    }
    static_cast<eabase::TidList*>(list->impl)->apply(eabase::TidJoiner());
    return 0;
}

fiber_tag_t fiber_self_tag(void) {
    return eabase::tls_task_group != nullptr ? eabase::tls_task_group->tag()
                                              : FIBER_TAG_DEFAULT;
}

int is_running_on_fiber() {
    return eabase::tls_task_group != NULL && !eabase::tls_task_group->is_current_pthread_task();
}

int is_running_on_pthread() {
    return eabase::tls_task_group == NULL || eabase::tls_task_group->is_current_pthread_task();

}

int is_running_on_fiber_pthread() {
    return eabase::tls_task_group != NULL && eabase::tls_task_group->is_current_pthread_task();
}

}  // extern "C"
