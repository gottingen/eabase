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

if(NOT DEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNDEBUG")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNDEBUG")
endif()

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${PROJECT_SOURCE_DIR}/src)

add_library(UTILITY_LIB OBJECT ${UTILITY_SOURCES})
add_library(SOURCES_LIB OBJECT ${SOURCES})

# shared library needs POSITION_INDEPENDENT_CODE
set_property(TARGET ${SOURCES_LIB} PROPERTY POSITION_INDEPENDENT_CODE 1)
set_property(TARGET ${UTILITY_LIB} PROPERTY POSITION_INDEPENDENT_CODE 1)

add_library(eabase-static STATIC $<TARGET_OBJECTS:UTILITY_LIB>
                               $<TARGET_OBJECTS:SOURCES_LIB>)

SET_TARGET_PROPERTIES(eabase-static PROPERTIES OUTPUT_NAME eabase CLEAN_DIRECT_OUTPUT 1)

if(BUILD_SHARED_LIBS)
    add_library(eabase-shared SHARED $<TARGET_OBJECTS:UTILITY_LIB>
                                   $<TARGET_OBJECTS:SOURCES_LIB>)
    target_link_libraries(eabase-shared ${DYNAMIC_LIB})
    SET_TARGET_PROPERTIES(eabase-shared PROPERTIES OUTPUT_NAME eabase CLEAN_DIRECT_OUTPUT 1)

    install(TARGETS eabase-shared
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            )
endif()



install(TARGETS eabase-static
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )
