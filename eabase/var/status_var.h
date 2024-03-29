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

#ifndef  EABASE_VAR_STATUS_H_
#define  EABASE_VAR_STATUS_H_

#include <string>                       // std::string
#include "eabase/utility/atomicops.h"
#include "eabase/utility/type_traits.h"
#include "eabase/utility/string_printf.h"
#include "eabase/utility/synchronization/lock.h"
#include "eabase/var/detail/is_atomical.h"
#include "eabase/var/variable.h"
#include "eabase/var/reducer.h"

namespace eabase {

// Display a rarely or periodically updated value.
// Usage:
//   eabase::Status<int> foo_count1(17);
//   foo_count1.expose("my_value");
//
//   eabase::Status<int> foo_count2;
//   foo_count2.set_value(17);
//   
//   eabase::Status<int> foo_count3("my_value", 17);
template <typename T, typename Enabler = void>
class StatusVar : public Variable {
public:
    StatusVar() {}
    StatusVar(const T& value) : _value(value) {}
    StatusVar(const eabase::StringPiece& name, const T& value) : _value(value) {
        this->expose(name);
    }
    StatusVar(const eabase::StringPiece& prefix,
           const eabase::StringPiece& name, const T& value) : _value(value) {
        this->expose_as(prefix, name);
    }
    // Calling hide() manually is a MUST required by Variable.
    ~StatusVar() { hide(); }

    void describe(std::ostream& os, bool /*quote_string*/) const override {
        os << get_value();
    }
    
#ifdef BAIDU_INTERNAL
    void get_value(boost::any* value) const override {
        eabase::AutoLock guard(_lock);
        *value = _value;
    }
#endif

    T get_value() const {
        eabase::AutoLock guard(_lock);
        const T res = _value;
        return res;
    }

    void set_value(const T& value) {
        eabase::AutoLock guard(_lock);
        _value = value;
    }

private:
    T _value;
    // We use lock rather than eabase::atomic for generic values because
    // eabase::atomic requires the type to be memcpy-able (POD basically)
    mutable eabase::Lock _lock;
};

template <typename T>
class StatusVar<T, typename eabase::enable_if<detail::is_atomical<T>::value>::type>
    : public Variable {
public:
    struct PlaceHolderOp {
        void operator()(T&, const T&) const {}
    };
    class SeriesSampler : public detail::Sampler {
    public:
        typedef typename eabase::conditional<
        true, detail::AddTo<T>, PlaceHolderOp>::type Op;
        explicit SeriesSampler(StatusVar* owner)
            : _owner(owner), _series(Op()) {}
        void take_sample() { _series.append(_owner->get_value()); }
        void describe(std::ostream& os) { _series.describe(os, NULL); }
    private:
        StatusVar* _owner;
        detail::Series<T, Op> _series;
    };

public:
    StatusVar() : _series_sampler(NULL) {}
    StatusVar(const T& value) : _value(value), _series_sampler(NULL) { }
    StatusVar(const eabase::StringPiece& name, const T& value)
        : _value(value), _series_sampler(NULL) {
        this->expose(name);
    }
    StatusVar(const eabase::StringPiece& prefix,
           const eabase::StringPiece& name, const T& value)
        : _value(value), _series_sampler(NULL) {
        this->expose_as(prefix, name);
    }
    ~StatusVar() {
        hide();
        if (_series_sampler) {
            _series_sampler->destroy();
            _series_sampler = NULL;
        }
    }

    void describe(std::ostream& os, bool /*quote_string*/) const override {
        os << get_value();
    }
    
#ifdef BAIDU_INTERNAL
    void get_value(boost::any* value) const override {
        *value = get_value();
    }
#endif
    
    T get_value() const {
        return _value.load(eabase::memory_order_relaxed);
    }
    
    void set_value(const T& value) {
        _value.store(value, eabase::memory_order_relaxed);
    }

    int describe_series(std::ostream& os, const SeriesOptions& options) const override {
        if (_series_sampler == NULL) {
            return 1;
        }
        if (!options.test_only) {
            _series_sampler->describe(os);
        }
        return 0;
    }

protected:
    int expose_impl(const eabase::StringPiece& prefix,
                    const eabase::StringPiece& name,
                    DisplayFilter display_filter) override {
        const int rc = Variable::expose_impl(prefix, name, display_filter);
        if (rc == 0 &&
            _series_sampler == NULL &&
            FLAGS_save_series) {
            _series_sampler = new SeriesSampler(this);
            _series_sampler->schedule();
        }
        return rc;
    }

private:
    eabase::atomic<T> _value;
    SeriesSampler* _series_sampler;
};

// Specialize for std::string, adding a printf-style set_value().
template <>
class StatusVar<std::string, void> : public Variable {
public:
    StatusVar() {}
    StatusVar(const eabase::StringPiece& name, const char* fmt, ...) {
        if (fmt) {
            va_list ap;
            va_start(ap, fmt);
            eabase::string_vprintf(&_value, fmt, ap);
            va_end(ap);
        }
        expose(name);
    }
    StatusVar(const eabase::StringPiece& prefix,
           const eabase::StringPiece& name, const char* fmt, ...) {
        if (fmt) {
            va_list ap;
            va_start(ap, fmt);
            eabase::string_vprintf(&_value, fmt, ap);
            va_end(ap);
        }
        expose_as(prefix, name);
    }

    ~StatusVar() { hide(); }

    void describe(std::ostream& os, bool quote_string) const override {
        if (quote_string) {
            os << '"' << get_value() << '"';
        } else {
            os << get_value();
        }
    }

    std::string get_value() const {
        eabase::AutoLock guard(_lock);
        return _value;
    }

#ifdef BAIDU_INTERNAL
    void get_value(boost::any* value) const override {
        *value = get_value();
    }
#endif
    
    void set_value(const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        {
            eabase::AutoLock guard(_lock);
            eabase::string_vprintf(&_value, fmt, ap);
        }
        va_end(ap);
    }

    void set_value(const std::string& s) {
        eabase::AutoLock guard(_lock);
        _value = s;
    }

private:
    std::string _value;
    mutable eabase::Lock _lock;
};

}  // namespace eabase

#endif  // EABASE_VAR_STATUS_H_
