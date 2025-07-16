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

set(NvPerfUtility_FOUND ON)
message(STATUS "Found NvPerfUtility")

SET(NvPerfUtility_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../NvPerfUtility/include")
SET(NvPerfUtility_IMPORTS_DIR "${CMAKE_CURRENT_LIST_DIR}/../NvPerfUtility/imports")

add_library(NvPerfUtility INTERFACE)
set_target_properties(NvPerfUtility PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${NvPerfUtility_INCLUDE_DIRS}")
SET(NvPerfUtility_LIBRARIES NvPerfUtility)


# =================
#       IMGUI
# =================
SET(NvPerfUtility_IMPORTS_IMGUI_DIR "${NvPerfUtility_IMPORTS_DIR}/imgui-docking")
set(NvPerfUtility_IMPORTS_IMGUI_SOURCES
    ${NvPerfUtility_IMPORTS_IMGUI_DIR}/imgui.cpp
    ${NvPerfUtility_IMPORTS_IMGUI_DIR}/imgui_draw.cpp
    ${NvPerfUtility_IMPORTS_IMGUI_DIR}/imgui_tables.cpp
    ${NvPerfUtility_IMPORTS_IMGUI_DIR}/imgui_widgets.cpp
    ${NvPerfUtility_IMPORTS_IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${NvPerfUtility_IMPORTS_IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)
add_library(NvPerfUtilityImportsImGui INTERFACE)
set_target_properties(
    NvPerfUtilityImportsImGui
    PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES
            "${NvPerfUtility_IMPORTS_IMGUI_DIR};${NvPerfUtility_IMPORTS_IMGUI_DIR}/backends"
        INTERFACE_SOURCES
            "${NvPerfUtility_IMPORTS_IMGUI_SOURCES}"
)


# =================
#      IMPLOT
# =================
SET(NvPerfUtility_IMPORTS_IMPLOT_DIR "${NvPerfUtility_IMPORTS_DIR}/implot-0.16")
set(NvPerfUtility_IMPORTS_IMPLOT_SOURCES
    ${NvPerfUtility_IMPORTS_IMPLOT_DIR}/implot.cpp
    ${NvPerfUtility_IMPORTS_IMPLOT_DIR}/implot_items.cpp
)
add_library(NvPerfUtilityImportsImPlot INTERFACE)
set_target_properties(NvPerfUtilityImportsImPlot
    PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES
            "${NvPerfUtility_IMPORTS_IMPLOT_DIR}"
        INTERFACE_SOURCES
            "${NvPerfUtility_IMPORTS_IMPLOT_SOURCES}"
)


# =================
#     RapidYaml
# =================
set(NvPerfUtility_IMPORTS_RYML_DIR "${NvPerfUtility_IMPORTS_DIR}/rapidyaml-0.4.0")
add_library(NvPerfUtilityImportsRyml INTERFACE)
set_target_properties(NvPerfUtilityImportsRyml
    PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES
            "${NvPerfUtility_IMPORTS_RYML_DIR}"
)


# =================
#       GLFW
# =================
set(NvPerfUtility_IMPORTS_GLFW_DIR "${NvPerfUtility_IMPORTS_DIR}/glfw-3.3.10")
# glfw folder has its own CMake file that can be conditionally included by an application on need. Simply add that directory in your application's cmake file
# and you're good to go. It is deliberately not included here globally because some samples would prefer to use its own version of glfw.


# =================
#   ICON_FONT_CPP
# =================
set(NvPerfUtility_IMPORTS_ICON_FONT_CPP_DIR "${NvPerfUtility_IMPORTS_DIR}/IconFontCppHeaders")


# =================
#        NFD
# =================
set(NvPerfUtility_IMPORTS_NFD_DIR "${NvPerfUtility_IMPORTS_DIR}/nativefiledialog-release_116")
# NFD on Linux requires GTK3, so a precheck like find_package(PkgConfig REQUIRED) & pkg_check_modules(GTK3 REQUIRED gtk+-3.0) will fail on machines that don't have
# GTK3 installed. Given not all tools use NFD, let's have each application include NFD on demand.


# =================
#    STB IMAGE
# =================
set(NvPerfUtility_IMPORTS_STB_IMAGE_DIR "${NvPerfUtility_IMPORTS_DIR}/stb_image")