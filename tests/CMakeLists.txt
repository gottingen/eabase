# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

find_package(Gperftools)
include_directories(${GPERFTOOLS_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

include(CompileProto)
set(TEST_PROTO_FILES
        tests/iobuf.proto
)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/tests/hdrs)
set(PROTOC_FLAGS ${PROTOC_FLAGS} -I${CMAKE_SOURCE_DIR})
compile_proto(PROTO_HDRS PROTO_SRCS ${CMAKE_BINARY_DIR}/tests
        ${CMAKE_BINARY_DIR}/tests/hdrs
        ${CMAKE_SOURCE_DIR}
        "${TEST_PROTO_FILES}")
add_library(TEST_PROTO_LIB OBJECT ${PROTO_SRCS} ${PROTO_HDRS})

include_directories(${CMAKE_CURRENT_BINARY_DIR}/tests)
set(EABASE_SYSTEM_GTEST_SOURCE_DIR "" CACHE PATH "System googletest source directory.")

if (DOWNLOAD_GTEST)
    include(SetupGtest)
elseif (EABASE_SYSTEM_GTEST_SOURCE_DIR)
    add_subdirectory("${EABASE_SYSTEM_GTEST_SOURCE_DIR}" "${PROJECT_BINARY_DIR}/system-googletest-build")
else ()
    message(FATAL_ERROR "Googletest is not available")
endif ()

set(CMAKE_CPP_FLAGS "${CMAKE_CPP_FLAGS} ${DEFINE_CLOCK_GETTIME} -DGFLAGS_NS=${GFLAGS_NS}")
set(CMAKE_CPP_FLAGS "${CMAKE_CPP_FLAGS} -DFIBER_USE_FAST_PTHREAD_MUTEX -D__const__=__unused__ -D_GNU_SOURCE -DUSE_SYMBOLIZE -DNO_TCMALLOC -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -DUNIT_TEST -Dprivate=public -Dprotected=public -DVAR_NOT_LINK_DEFAULT_VARIABLES -D__STRICT_ANSI__ -include ${PROJECT_SOURCE_DIR}/tests/sstream_workaround.h")
set(CMAKE_CXX_FLAGS "${CMAKE_CPP_FLAGS} -g -O2 -pipe -Wall -W -fPIC -fstrict-aliasing -Wno-invalid-offsetof -Wno-unused-parameter -fno-omit-frame-pointer")
use_cxx17()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    #required by eabase/utility/crc32.cc to boost performance for 10x
    if ((CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)") AND NOT (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.4))
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4 -msse4.2")
    endif ()
    if (NOT (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.0))
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-aligned-new")
    endif ()
endif ()

SET(TEST_UTILITY_SOURCES
        ${PROJECT_SOURCE_DIR}/tests/recordio_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/popen_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/bounded_queue_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/at_exit_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/atomicops_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/base64_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/base64url_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/big_endian_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/bits_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/hash_tables_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/linked_list_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/mru_cache_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/small_map_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/stack_container_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/mpsc_queue_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/cpu_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/crash_logging_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/leak_tracker_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/stack_trace_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/environment_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/file_util_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/dir_reader_posix_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/file_path_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/file_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/scoped_temp_dir_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/guid_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/hash_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/lazy_instance_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/aligned_memory_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/linked_ptr_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/ref_counted_memory_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/ref_counted_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/scoped_ptr_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/scoped_vector_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/singleton_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/weak_ptr_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/observer_list_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/file_descriptor_shuffle_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/rand_util_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/safe_numerics_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/scoped_clear_errno_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/scoped_generic_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/security_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/sha1_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/stl_util_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/nullable_string16_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/safe_sprintf_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/string16_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/stringprintf_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/string_number_conversions_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/string_piece_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/string_split_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/string_tokenizer_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/string_util_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/stringize_macros_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/sys_string_conversions_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/utf_offset_string_conversions_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/utf_string_conversions_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/cancellation_flag_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/condition_variable_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/lock_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/waitable_event_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/type_traits_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/non_thread_safe_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/platform_thread_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/simple_thread_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/thread_checker_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/thread_collision_warner_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/thread_id_name_manager_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/thread_local_storage_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/thread_local_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/watchdog_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/time_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/version_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/logging_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/cacheline_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/class_name_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/endpoint_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/unique_ptr_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/errno_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/fd_guard_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/file_watcher_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/find_cstr_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/scoped_lock_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/status_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/string_printf_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/string_splitter_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/synchronous_event_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/temp_file_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/baidu_thread_local_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/thread_key_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/baidu_time_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/flat_map_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/crc32c_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/iobuf_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/object_pool_unittest.cc
        ${PROJECT_SOURCE_DIR}/tests/test_switches.cc
        ${PROJECT_SOURCE_DIR}/tests/scoped_locale.cc
        ${PROJECT_SOURCE_DIR}/tests/butil_unittest_main.cc
        ${PROJECT_SOURCE_DIR}/tests/butil_unittest_main.cc
)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    SET(TEST_UTILITY_SOURCES ${TEST_UTILITY_SOURCES}
            ${PROJECT_SOURCE_DIR}/tests/proc_maps_linux_unittest.cc
            ${PROJECT_SOURCE_DIR}/tests/test_file_util_linux.cc)
endif ()

# fibers_* functions are used in logging.cc, and they need to be marked as
# weak symbols explicitly in Darwin system.
if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(DYNAMIC_LIB ${DYNAMIC_LIB}
            "-Wl,-U,_fiber_getspecific"
            "-Wl,-U,_fiber_setspecific"
            "-Wl,-U,_fiber_key_create")
endif ()

add_library(UTILITY_DEBUG_LIB OBJECT ${UTILITY_SOURCES})
add_library(SOURCES_DEBUG_LIB OBJECT ${SOURCES})

# shared library needs POSITION_INDEPENDENT_CODE
set_property(TARGET ${UTILITY_DEBUG_LIB} PROPERTY POSITION_INDEPENDENT_CODE 1)
set_property(TARGET ${SOURCES_DEBUG_LIB} PROPERTY POSITION_INDEPENDENT_CODE 1)

add_library(eabase-shared-debug SHARED $<TARGET_OBJECTS:UTILITY_DEBUG_LIB>
        $<TARGET_OBJECTS:SOURCES_DEBUG_LIB>)
# change the debug lib output dir to be different from the release output
set_target_properties(eabase-shared-debug PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/tests)

target_link_libraries(eabase-shared-debug ${DYNAMIC_LIB})

# test_utility
add_executable(test_utility ${TEST_UTILITY_SOURCES} $<TARGET_OBJECTS:TEST_PROTO_LIB>)
target_link_libraries(test_utility eabase-shared-debug
        gtest
        ${GPERFTOOLS_LIBRARIES})

add_test(NAME test_utility COMMAND test_utility)

# test_var
# -DVAR_NOT_LINK_DEFAULT_VARIABLES not work for gcc >= 5.0, just remove the file to prevent linking into unit tests
list(REMOVE_ITEM VAR_SOURCES ${PROJECT_SOURCE_DIR}/eabase/var/default_variables.cc)

add_library(VAR_DEBUG_LIB OBJECT ${VAR_SOURCES})
file(GLOB TEST_VAR_SRCS "var_*_unittest.cc")
add_executable(test_var ${TEST_VAR_SRCS}
        $<TARGET_OBJECTS:VAR_DEBUG_LIB>
        $<TARGET_OBJECTS:UTILITY_DEBUG_LIB>)
target_link_libraries(test_var gtest
        ${GPERFTOOLS_LIBRARIES}
        ${DYNAMIC_LIB})
add_test(NAME test_var COMMAND test_var)

# fiber tests
file(GLOB FIBER_UNITTESTS "fiber*unittest.cc")
foreach (FIBER_UT ${FIBER_UNITTESTS})
    get_filename_component(FIBER_UT_WE ${FIBER_UT} NAME_WE)
    add_executable(${FIBER_UT_WE} ${FIBER_UT})
    target_link_libraries(${FIBER_UT_WE} gtest_main
            eabase-shared-debug
            ${GPERFTOOLS_LIBRARIES})
    add_test(NAME ${FIBER_UT_WE} COMMAND ${FIBER_UT_WE})
endforeach ()

