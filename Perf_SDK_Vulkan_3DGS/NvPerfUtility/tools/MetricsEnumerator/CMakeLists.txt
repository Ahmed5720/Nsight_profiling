#[[
* Copyright 2014-2025 NVIDIA Corporation.  All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
]]

find_package(Vulkan)

set(NVPERF_UTILITY_FILES)
file(GLOB NVPERF_UTILITY_FILES "${NvPerfUtility_INCLUDE_DIRS}/*.h")
source_group("NvPerfUtility" FILES ${NVPERF_UTILITY_FILES})

set(SOURCES
    MetricsEnumerator.cpp
    MetricsEnumeratorHtmlTemplate.cpp
)

add_executable(MetricsEnumerator
    ${SOURCES}
    ${NVPERF_FILES}
    ${NVPERF_UTILITY_FILES}
)
target_include_directories(MetricsEnumerator
    PRIVATE
        ${Vulkan_INCLUDE_DIRS}
        ${NvPerfUtilityImportsDir}/json-3.9.1
)
target_link_libraries(MetricsEnumerator
    PRIVATE
        NvPerf
        NvPerfUtility
)

if(NOT WIN32)
    target_link_libraries(MetricsEnumerator 
        PRIVATE 
            dl
    )
endif()

if(MSVC)
  target_compile_options(MetricsEnumerator PRIVATE /WX)
endif()

DeployNvPerf(MetricsEnumerator NvPerf)
