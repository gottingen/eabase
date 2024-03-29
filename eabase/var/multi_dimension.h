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

#ifndef EABASE_VAR_MULTI_DIMENSION_H_
#define EABASE_VAR_MULTI_DIMENSION_H_

#include "eabase/utility/logging.h"                           // LOG
#include "eabase/utility/macros.h"                            // BAIDU_CASSERT
#include "eabase/utility/scoped_lock.h"                       // BAIDU_SCOPE_LOCK
#include "eabase/utility/containers/doubly_buffered_data.h"   // DBD
#include "eabase/utility/containers/flat_map.h"               // eabase::FlatMap
#include "eabase/var/mvariable.h"

namespace eabase {

constexpr uint64_t MAX_MULTI_DIMENSION_STATS_COUNT = 20000;

template <typename  T>
class MultiDimension : public MVariable {
public:

    enum STATS_OP {
        READ_ONLY,
        READ_OR_INSERT,
    };

    typedef MVariable Base;
    typedef std::list<std::string> key_type;
    typedef T value_type;
    typedef T* value_ptr_type;

    struct KeyHash {
        size_t operator() (const key_type& key) const {
            size_t hash_value = 0;
            for (auto &k : key) {
                hash_value += std::hash<std::string>()(k);
            }
            return hash_value;
        }
    };
    
    typedef value_ptr_type op_value_type;
    typedef typename eabase::FlatMap<key_type, op_value_type, KeyHash> MetricMap;

    typedef typename MetricMap::const_iterator MetricMapConstIterator;
    typedef typename eabase::DoublyBufferedData<MetricMap> MetricMapDBD;
    typedef typename MetricMapDBD::ScopedPtr MetricMapScopedPtr;
    
    explicit MultiDimension(const key_type& labels);
    
    MultiDimension(const eabase::StringPiece& name,
                   const key_type& labels);
    
    MultiDimension(const eabase::StringPiece& prefix,
                   const eabase::StringPiece& name,
                   const key_type& labels);

    ~MultiDimension();

    // Implement this method to print the variable into ostream.
    void describe(std::ostream& os);

    // Dump real var pointer
    size_t dump(Dumper* dumper, const DumpOptions* options);

    // Get real var pointer object
    // Return real var pointer on success, NULL otherwise.
    T* get_stats(const key_type& labels_value) {
        return get_stats_impl(labels_value, READ_OR_INSERT);
    }

    // Remove stat so those not count and dump
    void delete_stats(const key_type& labels_value);

    // Remove all stat
    void clear_stats();

    // True if var pointer exists
    bool has_stats(const key_type& labels_value);

    // Get number of stats
    size_t count_stats();

    // Put name of all stats label into `names'
    void list_stats(std::vector<key_type>* names);
    
#ifdef UNIT_TEST
    // Get real var pointer object
    // Return real var pointer if labels_name exist, NULL otherwise.
    // CAUTION!!! Just For Debug!!!
    T* get_stats_read_only(const key_type& labels_value) {
        return get_stats_impl(labels_value);
    }

    // Get real var pointer object
    // Return real var pointer if labels_name exist, otherwise(not exist) create var pointer.
    // CAUTION!!! Just For Debug!!!
    T* get_stats_read_or_insert(const key_type& labels_value, bool* do_write = NULL) {
        return get_stats_impl(labels_value, READ_OR_INSERT, do_write);
    }
#endif

private:
    T* get_stats_impl(const key_type& labels_value);

    T* get_stats_impl(const key_type& labels_value, STATS_OP stats_op, bool* do_write = NULL);

    void make_dump_key(std::ostream& os, 
                       const key_type& labels_value, 
                       const std::string& suffix = "", 
                       const int quantile = 0);

    void make_labels_kvpair_string(std::ostream& os, 
                       const key_type& labels_value, 
                       const int quantile);

    bool is_valid_lables_value(const key_type& labels_value) const;
    
    // Remove all stats so those not count and dump
    void delete_stats();
    
    static size_t init_flatmap(MetricMap& bg);
    
private:
    MetricMapDBD _metric_map;
};

} // namespace eabase

#include "eabase/var/multi_dimension_inl.h"

#endif // EABASE_VAR_MULTI_DIMENSION_H_