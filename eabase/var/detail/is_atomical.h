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

#ifndef EABASE_VAR_DETAIL_IS_ATOMICAL_H_
#define EABASE_VAR_DETAIL_IS_ATOMICAL_H_

#include "eabase/utility/type_traits.h"

namespace eabase {
namespace detail {
template <class T> struct is_atomical
: eabase::integral_constant<bool, (eabase::is_integral<T>::value ||
                                 eabase::is_floating_point<T>::value)
                                 // FIXME(gejun): Not work in gcc3.4
                                 // eabase::is_enum<T>::value ||
                                 // NOTE(gejun): Ops on pointers are not
                                 // atomic generally
                                 // eabase::is_pointer<T>::value
                          > {};
template <class T> struct is_atomical<const T> : is_atomical<T> { };
template <class T> struct is_atomical<volatile T> : is_atomical<T> { };
template <class T> struct is_atomical<const volatile T> : is_atomical<T> { };

}  // namespace detail
}  // namespace eabase

#endif  // EABASE_VAR_DETAIL_IS_ATOMICAL_H_
