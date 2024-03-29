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

#ifndef FIBER_ALLOCATE_STACK_INL_H_
#define FIBER_ALLOCATE_STACK_INL_H_

DECLARE_int32(guard_page_size);
DECLARE_int32(tc_stack_small);
DECLARE_int32(tc_stack_normal);

namespace eabase {

struct MainStackClass {};

struct SmallStackClass {
    static int* stack_size_flag;
    // Older gcc does not allow static const enum, use int instead.
    static const int stacktype = (int)STACK_TYPE_SMALL;
};

struct NormalStackClass {
    static int* stack_size_flag;
    static const int stacktype = (int)STACK_TYPE_NORMAL;
};

struct LargeStackClass {
    static int* stack_size_flag;
    static const int stacktype = (int)STACK_TYPE_LARGE;
};

template <typename StackClass> struct StackFactory {
    struct Wrapper : public ContextualStack {
        explicit Wrapper(void (*entry)(intptr_t)) {
            if (allocate_stack_storage(&storage, *StackClass::stack_size_flag,
                                       FLAGS_guard_page_size) != 0) {
                storage.zeroize();
                context = NULL;
                return;
            }
            context = fiber_make_fcontext(storage.bottom, storage.stacksize, entry);
            stacktype = (StackType)StackClass::stacktype;
        }
        ~Wrapper() {
            if (context) {
                context = NULL;
                deallocate_stack_storage(&storage);
                storage.zeroize();
            }
        }
    };
    
    static ContextualStack* get_stack(void (*entry)(intptr_t)) {
        return eabase::get_object<Wrapper>(entry);
    }
    
    static void return_stack(ContextualStack* sc) {
        eabase::return_object(static_cast<Wrapper*>(sc));
    }
};

template <> struct StackFactory<MainStackClass> {
    static ContextualStack* get_stack(void (*)(intptr_t)) {
        ContextualStack* s = new (std::nothrow) ContextualStack;
        if (NULL == s) {
            return NULL;
        }
        s->context = NULL;
        s->stacktype = STACK_TYPE_MAIN;
        s->storage.zeroize();
        return s;
    }
    
    static void return_stack(ContextualStack* s) {
        delete s;
    }
};

inline ContextualStack* get_stack(StackType type, void (*entry)(intptr_t)) {
    switch (type) {
    case STACK_TYPE_PTHREAD:
        return NULL;
    case STACK_TYPE_SMALL:
        return StackFactory<SmallStackClass>::get_stack(entry);
    case STACK_TYPE_NORMAL:
        return StackFactory<NormalStackClass>::get_stack(entry);
    case STACK_TYPE_LARGE:
        return StackFactory<LargeStackClass>::get_stack(entry);
    case STACK_TYPE_MAIN:
        return StackFactory<MainStackClass>::get_stack(entry);
    }
    return NULL;
}

inline void return_stack(ContextualStack* s) {
    if (NULL == s) {
        return;
    }
    switch (s->stacktype) {
    case STACK_TYPE_PTHREAD:
        assert(false);
        return;
    case STACK_TYPE_SMALL:
        return StackFactory<SmallStackClass>::return_stack(s);
    case STACK_TYPE_NORMAL:
        return StackFactory<NormalStackClass>::return_stack(s);
    case STACK_TYPE_LARGE:
        return StackFactory<LargeStackClass>::return_stack(s);
    case STACK_TYPE_MAIN:
        return StackFactory<MainStackClass>::return_stack(s);
    }
}

inline void jump_stack(ContextualStack* from, ContextualStack* to) {
    fiber_jump_fcontext(&from->context, to->context, 0/*not skip remained*/);
}

}  // namespace eabase

namespace eabase {

template <> struct ObjectPoolBlockMaxItem<
    eabase::StackFactory<eabase::LargeStackClass>::Wrapper> {
    static const size_t value = 64;
};
template <> struct ObjectPoolBlockMaxItem<
    eabase::StackFactory<eabase::NormalStackClass>::Wrapper> {
    static const size_t value = 64;
};

template <> struct ObjectPoolBlockMaxItem<
    eabase::StackFactory<eabase::SmallStackClass>::Wrapper> {
    static const size_t value = 64;
};

template <> struct ObjectPoolFreeChunkMaxItem<
    eabase::StackFactory<eabase::SmallStackClass>::Wrapper> {
    inline static size_t value() {
        return (FLAGS_tc_stack_small <= 0 ? 0 : FLAGS_tc_stack_small);
    }
};

template <> struct ObjectPoolFreeChunkMaxItem<
    eabase::StackFactory<eabase::NormalStackClass>::Wrapper> {
    inline static size_t value() {
        return (FLAGS_tc_stack_normal <= 0 ? 0 : FLAGS_tc_stack_normal);
    }
};

template <> struct ObjectPoolFreeChunkMaxItem<
    eabase::StackFactory<eabase::LargeStackClass>::Wrapper> {
    inline static size_t value() { return 1UL; }
};

template <> struct ObjectPoolValidator<
    eabase::StackFactory<eabase::LargeStackClass>::Wrapper> {
    inline static bool validate(
        const eabase::StackFactory<eabase::LargeStackClass>::Wrapper* w) {
        return w->context != NULL;
    }
};

template <> struct ObjectPoolValidator<
    eabase::StackFactory<eabase::NormalStackClass>::Wrapper> {
    inline static bool validate(
        const eabase::StackFactory<eabase::NormalStackClass>::Wrapper* w) {
        return w->context != NULL;
    }
};

template <> struct ObjectPoolValidator<
    eabase::StackFactory<eabase::SmallStackClass>::Wrapper> {
    inline static bool validate(
        const eabase::StackFactory<eabase::SmallStackClass>::Wrapper* w) {
        return w->context != NULL;
    }
};
    
}  // namespace eabase

#endif  // FIBER_ALLOCATE_STACK_INL_H_
