cmake_minimum_required(VERSION 3.15)

project(rast LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(Vulkan REQUIRED)
# add_subdirectory(../third_party/VulkanMemoryAllocator EXCLUDE_FROM_ALL)
# add_subdirectory(third_party/vulkan_radix_sort EXCLUDE_FROM_ALL)
add_subdirectory(../third_party/glfw ./glfw EXCLUDE_FROM_ALL)
add_subdirectory(../third_party/glm ./glm EXCLUDE_FROM_ALL)
add_subdirectory(../third_party/argparse ./argparse EXCLUDE_FROM_ALL)
add_subdirectory(../third_party/tinygltf ./tinygltf EXCLUDE_FROM_ALL)

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

# vkgs
add_library(rast SHARED
  # src/vkgs/scene/camera.cc
  # src/vkgs/engine/engine.cc
  # src/vkgs/engine/splat_load_thread.cc
  # src/vkgs/engine/vulkan/attachment.cc
  # src/vkgs/engine/vulkan/buffer.cc
  # src/vkgs/engine/vulkan/compute_pipeline.cc
  # src/vkgs/engine/vulkan/context.cc
  # src/vkgs/engine/vulkan/cpu_buffer.cc
  # src/vkgs/engine/vulkan/descriptor_layout.cc
  # src/vkgs/engine/vulkan/descriptor.cc
  # src/vkgs/engine/vulkan/framebuffer.cc
  # src/vkgs/engine/vulkan/graphics_pipeline.cc
  # src/vkgs/engine/vulkan/pipeline_layout.cc
  # src/vkgs/engine/vulkan/render_pass.cc
  # src/vkgs/engine/vulkan/swapchain.cc
  # src/vkgs/engine/vulkan/uniform_buffer.cc
  # src/vkgs/engine/vulkan/vma_impl.cc
  src/rast/rast.cpp
  src/rast/gltf_scene.cpp
  src/rast/texture.cpp
  src/rast/camera.cc
  # src/vkgs/engine/vulkan/tiny_obj_loader.cc
  # imgui
  ../third_party/imgui/backends/imgui_impl_glfw.cpp
  ../third_party/imgui/backends/imgui_impl_vulkan.cpp
  ../third_party/imgui/imgui.cpp
  ../third_party/imgui/imgui_draw.cpp
  ../third_party/imgui/imgui_demo.cpp
  ../third_party/imgui/imgui_tables.cpp
  ../third_party/imgui/imgui_widgets.cpp
  ../third_party/tinygltf/tiny_gltf.cc
)

add_shader(rast src/shader/main.vert main_vert)
add_shader(rast src/shader/main.frag main_frag)

target_include_directories(rast
  PUBLIC include 
  PRIVATE
    src
    ../third_party/tinygltf
    ../third_party/imgui
    ../third_party/imgui/backends
)

target_link_libraries(rast
  PUBLIC glm
  PRIVATE Vulkan::Vulkan glfw
)

function(print_include_directories target)
    get_target_property(INCLUDE_DIRS ${target} INCLUDE_DIRECTORIES)
    message(STATUS "Include directories for ${target}: ${INCLUDE_DIRS}")
endfunction()

print_include_directories(rast)

# executable
add_executable(rasterizer src/rasterizer.cpp)
target_link_libraries(rasterizer
  PUBLIC rast 
  PRIVATE argparse
)
