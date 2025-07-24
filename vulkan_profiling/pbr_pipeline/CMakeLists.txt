cmake_minimum_required(VERSION 3.15)

project(pbr LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)


find_package(Vulkan REQUIRED)
add_subdirectory(../third_party/glfw ./glfw EXCLUDE_FROM_ALL)
add_subdirectory(../third_party/glm ./glm EXCLUDE_FROM_ALL)
add_subdirectory(../third_party/argparse ./argparse EXCLUDE_FROM_ALL)
add_subdirectory(../third_party/tinygltf ./tinygltf EXCLUDE_FROM_ALL)
# add_subdirectory(../third_party/KTX-Software ./KTX-Software EXCLUDE_FROM_ALL)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# shaders
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/generated)

# Shader compilation
# add_shader(TARGET SHADER OUTPUT DEFINE...)
function(add_shader)
  list(POP_FRONT ARGV TARGET SHADER OUTPUT)
  list(TRANSFORM ARGV PREPEND "-D" OUTPUT_VARIABLE DEFINES)

  get_filename_component(SHADER ${SHADER} ABSOLUTE)

  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/generated/${OUTPUT}.h
    COMMAND
      ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}
      --target-env spirv1.5
      -V
      --vn ${OUTPUT}
      -o ${CMAKE_CURRENT_SOURCE_DIR}/src/generated/${OUTPUT}.h
      ${DEFINES}
      ${SHADER}
    DEPENDS ${SHADER}
    COMMENT "Compiling ${CMAKE_CURRENT_SOURCE_DIR}/src/generated/${OUTPUT}.h"
  )

  add_custom_target(${OUTPUT} DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/generated/${OUTPUT}.h)
  add_dependencies(${TARGET} ${OUTPUT})
endfunction()

# file(GLOB SOURCES "../third_party/KTX-Software/lib/*.c")

add_library(pbr SHARED
	src/pbr/pbr.cpp
	src/base/VulkanBuffer.cpp
	src/base/VulkanDebug.cpp
	src/base/VulkanDevice.cpp
	src/base/vulkanexamplebase.cpp
	src/base/VulkanglTFModel.cpp
	src/base/VulkanSwapChain.cpp
	src/base/VulkanTexture.cpp
	src/base/VulkanTools.cpp
	# src/base/VulkanUIOverlay.cpp
	../third_party/imgui/backends/imgui_impl_glfw.cpp
	../third_party/imgui/backends/imgui_impl_vulkan.cpp
	../third_party/imgui/imgui.cpp
	../third_party/imgui/imgui_draw.cpp
	../third_party/imgui/imgui_demo.cpp
	../third_party/imgui/imgui_tables.cpp
	../third_party/imgui/imgui_widgets.cpp
	../third_party/tinygltf/tiny_gltf.cc)

# add_shader(pbr src/shader/filtercube.vert filtercube)
# add_shader(pbr src/shader/genbrdflut.frag genbrdflut_frag)
# add_shader(pbr src/shader/genbrdflut.vert genbrdflut_vert)
# add_shader(pbr src/shader/irradiancecube.frag irradiancecube)
# add_shader(pbr src/shader/scene.frag scene_frag)
# add_shader(pbr src/shader/scene.vert scene_vert)
add_shader(pbr src/shader/pbr.frag pbr_frag)
add_shader(pbr src/shader/pbr.vert pbr_vert)
add_shader(pbr src/shader/pbr_shadow.frag pbr_shadow_frag)
add_shader(pbr src/shader/pbr_shadow.vert pbr_shadow_vert)
add_shader(pbr src/shader/offscreen.frag offscreen_frag)
add_shader(pbr src/shader/offscreen.vert offscreen_vert)
# add_shader(pbr src/shader/quad.frag quad_frag)
# add_shader(pbr src/shader/quad.vert quad_vert)
# add_shader(pbr src/shader/prefilterenvmap.frag prefilterenvmap)

target_include_directories(pbr
	PUBLIC include
		src/base
		src
		../third_party/imgui
		../third_party/tinygltf
)

target_link_libraries(pbr
  PUBLIC glm
  PRIVATE Vulkan::Vulkan glfw
)

function(print_include_directories target)
    get_target_property(INCLUDE_DIRS ${target} INCLUDE_DIRECTORIES)
    message(STATUS "Include directories for ${target}: ${INCLUDE_DIRS}")
endfunction()

print_include_directories(pbr)

add_executable(pbr_viewer src/pbr_pipe.cpp)
target_link_libraries(pbr_viewer
	PUBLIC pbr
	PRIVATE argparse
)