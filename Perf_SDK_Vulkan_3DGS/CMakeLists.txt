cmake_minimum_required(VERSION 3.6...3.31)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_CXX_STANDARD 20)

# Project setup
get_filename_component(PROJNAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
project(${PROJNAME} LANGUAGES C CXX)
message(STATUS "-------------------------------")
message(STATUS "Processing Project ${PROJNAME}:")

#####################################################################################
# Dependency Setup
#

# NvPro Core Setup
set(NVPRO_GIT_TAG main)
set(NVPRO_TGT_SRC_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/_deps)

if(NOT BASE_DIRECTORY)
  find_path(BASE_DIRECTORY
    NAMES nvpro_core/cmake/setup.cmake
    PATHS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_SOURCE_DIR}/external
    DOC "Directory containing nvpro_core")
endif()

if(EXISTS ${BASE_DIRECTORY}/nvpro_core/cmake/setup.cmake)
  include(${BASE_DIRECTORY}/nvpro_core/cmake/setup.cmake)
else()
  # Download nvpro_core if not found
  execute_process( 
    COMMAND git config --get remote.origin.url 
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} 
    OUTPUT_VARIABLE GIT_REPO_URL 
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  
  string(FIND "${GIT_REPO_URL}" "github.com" FOUND_INDEX)
  if(FOUND_INDEX GREATER -1)
    string(REGEX MATCH ".*github\\.com" GIT_BASE_URL "${GIT_REPO_URL}")
    string(FIND "${GIT_REPO_URL}" "git@" SSH_FOUND_INDEX)
    if(SSH_FOUND_INDEX GREATER -1)
      set(NVPRO_GIT_URL ${GIT_BASE_URL}:nvpro-samples/nvpro_core.git)
    else()
      set(NVPRO_GIT_URL ${GIT_BASE_URL}/nvpro-samples/nvpro_core.git)
    endif()
    if("${NVPRO_GIT_TAG}" STREQUAL "main")
      set(NVPRO_GIT_TAG master)
    endif()
  else()
    string(REGEX MATCH "^[^/]+//[^/]+/" GIT_BASE_URL "${GIT_REPO_URL}")
    set(NVPRO_GIT_URL ${GIT_BASE_URL}devtechproviz/nvpro-samples/nvpro_core.git)
  endif()

  execute_process(
    COMMAND git clone --depth 1 --branch ${NVPRO_GIT_TAG} ${NVPRO_GIT_URL} ${CMAKE_CURRENT_BINARY_DIR}/_deps/nvpro_core
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  
  find_path(BASE_DIRECTORY
    NAMES nvpro_core
    PATHS ${CMAKE_CURRENT_BINARY_DIR}/_deps
    REQUIRED
    DOC "Directory containing nvpro_core")
  
  if(EXISTS ${BASE_DIRECTORY}/nvpro_core/cmake/setup.cmake)
    set(OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/bin_x64)
    include(${BASE_DIRECTORY}/nvpro_core/cmake/setup.cmake)
  else()
    message(FATAL_ERROR "Could not find or download nvpro_core")
  endif()
endif()

set(NVPRO_CORE_DIR ${BASE_DIRECTORY}/nvpro_core)
_add_project_definitions(${PROJNAME})

# Install directory
set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/_install" CACHE PATH "Install directory" FORCE)

# Default scene option
option(DISABLE_DEFAULT_SCENE "Disable default scene loading" OFF)
if(NOT DISABLE_DEFAULT_SCENE)
  add_definitions(-DWITH_DEFAULT_SCENE_FEATURE)
  download_files(FILENAMES flowers_1.zip EXTRACT)
endif()

#####################################################################################
# Required Packages
#

# Find required packages before target creation
find_package(Vulkan REQUIRED)
find_package(ShaderC REQUIRED)
find_package(Threads REQUIRED)

_add_package_VulkanSDK()
_add_package_ShaderC()
_add_package_IMGUI()
_add_package_NVML()

# Vulkan Radix Sort
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/vrdx)
foreach(target IN ITEMS vk_radix_sort spine_comp downsweep_comp downsweep_key_value_comp upsweep_comp)
  set_property(TARGET ${target} PROPERTY FOLDER "ThirdParty/vrdx")
endforeach()

#####################################################################################
# Project Sources
#

file(GLOB SOURCE_FILES src/*.cpp src/*.h)
file(GLOB SHADER_FILES shaders/*.glsl shaders/*.h)
file(GLOB EXTERN_FILES 3rdparty/miniply/*.*)

# Main executable
add_executable(${PROJNAME} 
  ${SOURCE_FILES}
  ${COMMON_SOURCE_FILES}
  ${PACKAGE_SOURCE_FILES}
  ${SHADER_FILES}
  ${EXTERN_FILES}
)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJNAME})

# Include directories
target_include_directories(${PROJNAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders
  ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/miniply
  ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/vrdx
  "C:/Horizon Lab/vk_gaussian_splatting/argparse/include"
  ${CMAKE_SOURCE_DIR}/NvPerf/include
  ${CMAKE_SOURCE_DIR}/Samples/NvPerfUtility/include
)

if(WIN32)
  target_include_directories(${PROJNAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/NvPerf/include/windows-desktop-x64
  )
  add_definitions(/wd4996 /wd4244 /wd4305)  # MSVC warnings to disable
else()
  add_definitions(-fpermissive)
  set(UNIXLINKLIBS dl pthread)
endif()

# Compile definitions
target_compile_definitions(${PROJNAME} PRIVATE 
  NVPRO_CORE_DIR="${NVPRO_CORE_DIR}")

# Source groups
source_group("Source Files" FILES ${SOURCE_FILES})
source_group("Shader Files" FILES ${SHADER_FILES})
source_group("Extern Files" FILES ${EXTERN_FILES})
source_group("Common Files" FILES ${COMMON_SOURCE_FILES} ${PACKAGE_SOURCE_FILES})

# Link libraries
target_link_libraries(${PROJNAME} PRIVATE
  nvpro_core_vk_shaderc  # Updated to correct library name
  Vulkan::Vulkan
  vk_radix_sort
  ${PLATFORM_LIBRARIES}
  ${UNIXLINKLIBS}
  ${LIBRARIES_DEBUG}
  ${LIBRARIES_OPTIMIZED}
)

# Finalize and install
_finalize_target(${PROJNAME})

install(FILES ${SHADER_FILES} 
  CONFIGURATIONS Release 
  DESTINATION "bin_${ARCH}/GLSL_${PROJNAME}")

install(FILES ${SHADER_FILES} 
  CONFIGURATIONS Debug 
  DESTINATION "bin_${ARCH}_debug/GLSL_${PROJNAME}")