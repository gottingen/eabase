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

cmake_minimum_required(VERSION 3.10)
project(eabase C CXX)

option(WITH_BORINGSSL "With BoringSSL" OFF)
option(DEBUG "Print debug logs" OFF)
option(WITH_DEBUG_SYMBOLS "With debug symbols" ON)
option(WITH_RDMA "With RDMA" OFF)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_UNIT_TESTS "Whether to build unit tests" OFF)
option(DOWNLOAD_GTEST "Download and build a fresh copy of googletest. Requires Internet access." ON)

# Enable MACOSX_RPATH. Run "cmake --help-policy CMP0042" for policy details.
if (POLICY CMP0042)
    cmake_policy(SET CMP0042 NEW)
endif ()

set(EABASE_VERSION 0.2.0)

SET(CPACK_GENERATOR "DEB")
SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "eabase authors")
INCLUDE(CPack)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # require at least gcc 4.8
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.3)
        message(FATAL_ERROR "GCC is too old, please install a newer version supporting C++17")
    endif ()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # require at least clang 3.3
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.3)
        message(FATAL_ERROR "Clang is too old, please install a newer version supporting C++17")
    endif ()
else ()
    message(WARNING "You are using an unsupported compiler! Compilation has only been tested with Clang and GCC.")
endif ()

if (WITH_DEBUG_SYMBOLS)
    set(DEBUG_SYMBOL "-g")
endif ()

set(WITH_RDMA_VAL "0")
if (WITH_RDMA)
    set(WITH_RDMA_VAL "1")
endif ()

include(GNUInstallDirs)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

find_package(GFLAGS REQUIRED)

execute_process(
        COMMAND bash -c "grep \"namespace [_A-Za-z0-9]\\+ {\" ${GFLAGS_INCLUDE_PATH}/gflags/gflags_declare.h | head -1 | awk '{print $2}' | tr -d '\n'"
        OUTPUT_VARIABLE GFLAGS_NS
)
if (${GFLAGS_NS} STREQUAL "GFLAGS_NAMESPACE")
    execute_process(
            COMMAND bash -c "grep \"#define GFLAGS_NAMESPACE [_A-Za-z0-9]\\+\" ${GFLAGS_INCLUDE_PATH}/gflags/gflags_declare.h | head -1 | awk '{print $3}' | tr -d '\n'"
            OUTPUT_VARIABLE GFLAGS_NS
    )
endif ()

include_directories(
        ${PROJECT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)

execute_process(
        COMMAND bash -c "${PROJECT_SOURCE_DIR}/tools/get_eabase_revision.sh ${PROJECT_SOURCE_DIR} | tr -d '\n'"
        OUTPUT_VARIABLE EABASE_REVISION
)

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    include(CheckFunctionExists)
    CHECK_FUNCTION_EXISTS(clock_gettime HAVE_CLOCK_GETTIME)
    if (NOT HAVE_CLOCK_GETTIME)
        set(DEFINE_CLOCK_GETTIME "-DNO_CLOCK_GETTIME_IN_MAC")
    endif ()
    set(CMAKE_CPP_FLAGS "${CMAKE_CPP_FLAGS} -Wno-deprecated-declarations -Wno-inconsistent-missing-override")
endif ()

set(CMAKE_CPP_FLAGS "${CMAKE_CPP_FLAGS} ${DEFINE_CLOCK_GETTIME} -DGFLAGS_NS=${GFLAGS_NS}")
set(CMAKE_CPP_FLAGS "${CMAKE_CPP_FLAGS} -DFIBER_USE_FAST_PTHREAD_MUTEX -D__const__=__unused__ -D_GNU_SOURCE -DUSE_SYMBOLIZE -DNO_TCMALLOC -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -DEABASE_REVISION=\\\"${EABASE_REVISION}\\\" -D__STRICT_ANSI__")
set(CMAKE_CPP_FLAGS "${CMAKE_CPP_FLAGS} ${DEBUG_SYMBOL} ${THRIFT_CPP_FLAG}")
set(CMAKE_CXX_FLAGS "${CMAKE_CPP_FLAGS} -O2 -pipe -Wall -W -fPIC -fstrict-aliasing -Wno-invalid-offsetof -Wno-unused-parameter -fno-omit-frame-pointer")
set(CMAKE_C_FLAGS "${CMAKE_CPP_FLAGS} -O2 -pipe -Wall -W -fPIC -fstrict-aliasing -Wno-unused-parameter -fno-omit-frame-pointer")

macro(use_cxx17)
    if (CMAKE_VERSION VERSION_LESS "3.1.3")
        if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
        endif ()
        if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
        endif ()
    else ()
        set(CMAKE_CXX_STANDARD 17)
        set(CMAKE_CXX_STANDARD_REQUIRED ON)
    endif ()
endmacro(use_cxx17)

use_cxx17()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    #required by eabase/utility/crc32.cc to boost performance for 10x
    if ((CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)") AND NOT (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.4))
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4 -msse4.2")
    elseif ((CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64"))
        # segmentation fault in libcontext
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-gcse")
    endif ()
    if (NOT (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.0))
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-aligned-new")
    endif ()
endif ()

find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)

if (WITH_RDMA)
    message("eabase compile with rdma")
    find_path(RDMA_INCLUDE_PATH NAMES infiniband/verbs.h)
    find_library(RDMA_LIB NAMES ibverbs)
    if ((NOT RDMA_INCLUDE_PATH) OR (NOT RDMA_LIB))
        message(FATAL_ERROR "Fail to find ibverbs")
    endif ()
endif ()

find_library(PROTOC_LIB NAMES protoc)
if (NOT PROTOC_LIB)
    message(FATAL_ERROR "Fail to find protoc lib")
endif ()

if (WITH_BORINGSSL)
    find_package(BoringSSL)
    include_directories(${BORINGSSL_INCLUDE_DIR})
else ()
    if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(OPENSSL_ROOT_DIR
                "/usr/local/opt/openssl" # Homebrew installed OpenSSL
        )
    endif ()

    find_package(OpenSSL)
    include_directories(${OPENSSL_INCLUDE_DIR})
endif ()

include_directories(
        ${GFLAGS_INCLUDE_PATH}
        ${PROTOBUF_INCLUDE_DIRS}
        ${LEVELDB_INCLUDE_PATH}
)

set(DYNAMIC_LIB
        ${GFLAGS_LIBRARY}
        ${PROTOBUF_LIBRARIES}
        ${PROTOC_LIB}
        ${CMAKE_THREAD_LIBS_INIT}
        dl
        z)

if (WITH_BORINGSSL)
    list(APPEND DYNAMIC_LIB ${BORINGSSL_SSL_LIBRARY})
    list(APPEND DYNAMIC_LIB ${BORINGSSL_CRYPTO_LIBRARY})
else ()
    list(APPEND DYNAMIC_LIB ${OPENSSL_CRYPTO_LIBRARY})
    list(APPEND DYNAMIC_LIB ${OPENSSL_SSL_LIBRARY})
endif ()

if (WITH_RDMA)
    list(APPEND DYNAMIC_LIB ${RDMA_LIB})
endif ()

set(EABASE_PRIVATE_LIBS "-lgflags -lprotobuf -lprotoc -lssl -lcrypto -ldl -lz")


if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(DYNAMIC_LIB ${DYNAMIC_LIB} rt)
    set(EABASE_PRIVATE_LIBS "${EABASE_PRIVATE_LIBS} -lrt")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(DYNAMIC_LIB ${DYNAMIC_LIB}
            pthread
            "-framework CoreFoundation"
            "-framework CoreGraphics"
            "-framework CoreData"
            "-framework CoreText"
            "-framework Security"
            "-framework Foundation"
            "-Wl,-U,_MallocExtension_ReleaseFreeMemory"
            "-Wl,-U,_ProfilerStart"
            "-Wl,-U,_ProfilerStop"
            "-Wl,-U,__Z13GetStackTracePPvii")
endif ()

# for *.so
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/output/lib)
# for *.a
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/output/lib)

# the reason why not using file(GLOB_RECURSE...) is that we want to
# include different files on different platforms.
set(UTILITY_SOURCES
        ${PROJECT_SOURCE_DIR}/eabase/utility/third_party/dmg_fp/g_fmt.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/third_party/dmg_fp/dtoa_wrapper.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/third_party/dynamic_annotations/dynamic_annotations.c
        ${PROJECT_SOURCE_DIR}/eabase/utility/third_party/icu/icu_utf.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/third_party/superfasthash/superfasthash.c
        ${PROJECT_SOURCE_DIR}/eabase/utility/third_party/modp_b64/modp_b64.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/third_party/symbolize/demangle.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/third_party/symbolize/symbolize.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/third_party/snappy/snappy-sinksource.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/third_party/snappy/snappy-stubs-internal.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/third_party/snappy/snappy.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/third_party/murmurhash3/murmurhash3.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/arena.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/at_exit.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/atomicops_internals_x86_gcc.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/base64.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/base64url.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/big_endian.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/cpu.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/debug/alias.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/debug/asan_invalid_access.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/debug/crash_logging.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/debug/debugger.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/debug/debugger_posix.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/debug/dump_without_crashing.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/debug/proc_maps_linux.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/debug/stack_trace.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/debug/stack_trace_posix.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/environment.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/files/file.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/files/file_posix.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/files/file_enumerator.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/files/file_enumerator_posix.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/files/file_path.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/files/file_path_constants.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/files/memory_mapped_file.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/files/memory_mapped_file_posix.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/files/scoped_file.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/files/scoped_temp_dir.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/file_util.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/file_util_posix.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/guid.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/guid_posix.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/hash.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/lazy_instance.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/location.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/memory/aligned_memory.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/memory/ref_counted.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/memory/ref_counted_memory.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/memory/singleton.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/memory/weak_ptr.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/posix/file_descriptor_shuffle.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/posix/global_descriptors.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/process_util.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/rand_util.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/rand_util_posix.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/fast_rand.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/safe_strerror_posix.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/sha1_portable.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/strings/latin1_string_conversions.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/strings/nullable_string16.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/strings/safe_sprintf.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/strings/string16.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/strings/string_number_conversions.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/strings/string_split.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/strings/string_piece.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/strings/string_util.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/strings/string_util_constants.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/strings/stringprintf.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/strings/utf_offset_string_conversions.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/strings/utf_string_conversion_utils.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/strings/utf_string_conversions.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/synchronization/cancellation_flag.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/synchronization/condition_variable_posix.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/synchronization/waitable_event_posix.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/threading/non_thread_safe_impl.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/threading/platform_thread_posix.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/threading/simple_thread.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/threading/thread_checker_impl.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/threading/thread_collision_warner.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/threading/thread_id_name_manager.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/threading/thread_local_posix.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/threading/thread_local_storage.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/threading/thread_local_storage_posix.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/threading/thread_restrictions.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/threading/watchdog.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/time/clock.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/time/default_clock.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/time/default_tick_clock.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/time/tick_clock.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/time/time.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/time/time_posix.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/version.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/logging.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/class_name.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/errno.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/find_cstr.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/status.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/string_printf.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/thread_local.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/thread_key.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/unix_socket.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/endpoint.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/fd_utility.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/files/temp_file.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/files/file_watcher.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/time.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/zero_copy_stream_as_streambuf.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/crc32c.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/containers/case_ignored_flat_map.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/iobuf.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/binary_printer.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/recordio.cc
        ${PROJECT_SOURCE_DIR}/eabase/utility/popen.cc
)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(UTILITY_SOURCES ${UTILITY_SOURCES}
            ${PROJECT_SOURCE_DIR}/eabase/utility/file_util_linux.cc
            ${PROJECT_SOURCE_DIR}/eabase/utility/threading/platform_thread_linux.cc
            ${PROJECT_SOURCE_DIR}/eabase/utility/strings/sys_string_conversions_posix.cc)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(UTILITY_SOURCES ${UTILITY_SOURCES}
            ${PROJECT_SOURCE_DIR}/eabase/utility/mac/bundle_locations.mm
            ${PROJECT_SOURCE_DIR}/eabase/utility/mac/foundation_util.mm
            ${PROJECT_SOURCE_DIR}/eabase/utility/file_util_mac.mm
            ${PROJECT_SOURCE_DIR}/eabase/utility/threading/platform_thread_mac.mm
            ${PROJECT_SOURCE_DIR}/eabase/utility/strings/sys_string_conversions_mac.mm
            ${PROJECT_SOURCE_DIR}/eabase/utility/time/time_mac.cc
            ${PROJECT_SOURCE_DIR}/eabase/utility/mac/scoped_mach_port.cc)
endif ()

file(GLOB_RECURSE VAR_SOURCES "${PROJECT_SOURCE_DIR}/eabase/var/*.cc")
file(GLOB_RECURSE FIBER_SOURCES "${PROJECT_SOURCE_DIR}/eabase/fiber/*.cc")

set(SOURCES
        ${VAR_SOURCES}
        ${FIBER_SOURCES}
)

add_subdirectory(eabase)
if (BUILD_UNIT_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

install(DIRECTORY ${PROJECT_SOURCE_DIR}/eabase
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING
        PATTERN "*.h"
        PATTERN "*.hpp"
)

# Install pkgconfig
configure_file(cmake/eabase.pc.in ${PROJECT_BINARY_DIR}/eabase.pc @ONLY)
install(FILES ${PROJECT_BINARY_DIR}/eabase.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
