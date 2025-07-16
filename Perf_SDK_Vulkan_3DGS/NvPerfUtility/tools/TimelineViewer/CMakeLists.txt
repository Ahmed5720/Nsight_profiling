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

# =================
#   NvPerf headers
# =================
set(NVPERF_FILES)
foreach(NvPerf_INCLUDE_DIR IN LISTS NvPerf_INCLUDE_DIRS)
    file(GLOB FILES "${NvPerf_INCLUDE_DIR}/*.h")
    list(APPEND NVPERF_FILES ${FILES})
endforeach()
source_group("NvPerf" FILES ${NVPERF_FILES})


# =================
#   NvPerfUtility
# =================
set(NVPERF_UTILITY_FILES)
file(GLOB NVPERF_UTILITY_FILES "${NvPerfUtility_INCLUDE_DIRS}/*.h")
source_group("NvPerfUtility" FILES ${NVPERF_UTILITY_FILES})


# =================
#       GLFW
# =================
set(GLFW_INSTALL        OFF CACHE STRING "" FORCE)
set(GLFW_BUILD_DOCS     OFF CACHE STRING "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE STRING "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE STRING "" FORCE)
add_subdirectory(${NvPerfUtility_IMPORTS_GLFW_DIR} ${CMAKE_BINARY_DIR}/imports/glfw)


# =================
#        NFD
# =================
SET(NFD_SOURCES
    ${NvPerfUtility_IMPORTS_NFD_DIR}/src/nfd_common.c
)
SET(NFD_PRIV_INCLUDE_DIR
    ${NvPerfUtility_IMPORTS_NFD_DIR}/src
)
if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    list(APPEND NFD_SOURCES
        ${NvPerfUtility_IMPORTS_NFD_DIR}/src/nfd_win.cpp
    )
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0) # requires gtk3 to be installed(e.g. via "apt install libgtk-3-dev")
    list(APPEND NFD_SOURCES
        ${NvPerfUtility_IMPORTS_NFD_DIR}/src/nfd_gtk.c
    )
    list(APPEND NFD_PRIV_INCLUDE_DIR
        ${GTK3_INCLUDE_DIRS}
    )
endif()
add_library(NFD STATIC ${NFD_SOURCES})
set_target_properties(NFD PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_include_directories(NFD
    PUBLIC
        ${NvPerfUtility_IMPORTS_NFD_DIR}/src/include
    PRIVATE
        ${NFD_PRIV_INCLUDE_DIR}
)
if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    target_compile_options(NFD
        PRIVATE
            ${GTK3_CFLAGS_OTHER}
    )
    target_link_directories(NFD
        PRIVATE
            ${GTK3_LIBRARY_DIRS}
    )
    target_link_libraries(NFD
        PRIVATE
            ${GTK3_LIBRARIES}
    )
endif()


# =================
#  Timeline Viewer
# =================
set(SOURCES
    DataProcess.cpp
    DefaultIni.cpp
    FileLoader.cpp
    g_Font_FASolid900TTF.cpp
    g_Icon.cpp
    ImGuiUtils.cpp
    Logging.cpp
    Main.cpp
    TopLevelWindow.cpp
    Utils.cpp
    ViewerTab.cpp
)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    add_executable(TimelineViewer WIN32
        ${SOURCES}
        ${NVPERF_FILES}
        ${NVPERF_UTILITY_FILES}
    )
else()
    add_executable(TimelineViewer
        ${SOURCES}
        ${NVPERF_FILES}
        ${NVPERF_UTILITY_FILES}
    )
endif()

target_include_directories(TimelineViewer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${NvPerfUtility_IMPORTS_RYML_DIR} # required by NvPerfHudDataModel.h
        ${Vulkan_INCLUDE_DIRS}
        ${NvPerfUtility_IMPORTS_STB_IMAGE_DIR}
        ${NvPerfUtility_IMPORTS_ICON_FONT_CPP_DIR}
)

target_link_libraries(TimelineViewer
    PRIVATE
        NvPerf
        NvPerfUtility
        ${Vulkan_LIBRARY}
        NvPerfUtilityImportsImGui
        NvPerfUtilityImportsImPlot
        NFD
        glfw
)

DeployNvPerf(TimelineViewer NvPerf)
