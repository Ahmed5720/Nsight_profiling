cmake_minimum_required(VERSION 3.6...3.31)

get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
Project(${PROJECT_NAME} LANGUAGES C CXX)

# Add the cmake folder to the module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(NvproCore2 REQUIRED)

# set with -DDISABLE_DEFAULT_SCENE=ON on command line or use cmake-gui to set teh parameter
# if default scene is not disabled at build using this option, one can still disable it by 
# providing "-loadDefaultScene 0" to the sample command line
option(DISABLE_DEFAULT_SCENE "If set to true the default scene is not downloaded by CMake, and the sample does not try to open it when starting up with no scene as parameter" OFF)

# Download the default scene
if (NOT DISABLE_DEFAULT_SCENE)
  add_definitions(-DWITH_DEFAULT_SCENE_FEATURE)
  download_files(FILENAMES flowers_1.zip EXTRACT)
endif()

#####################################################################################
# additions of 3rdparty

#####
# Include Vulkan Radix Sort
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/vrdx)

set_property(TARGET vk_radix_sort PROPERTY FOLDER "ThirdParty/vrdx")
set_property(TARGET spine_comp PROPERTY FOLDER "ThirdParty/vrdx")
set_property(TARGET downsweep_comp PROPERTY FOLDER "ThirdParty/vrdx")
set_property(TARGET downsweep_key_value_comp PROPERTY FOLDER "ThirdParty/vrdx")
set_property(TARGET upsweep_comp PROPERTY FOLDER "ThirdParty/vrdx")



####################################################################################

# NvPerf Setup
#
#set(NVPERF_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/NvPerf)
#set(NVPERF_UTILITY_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/Samples/NvPerfUtility)

# option(NVPERF "Enable Nsight Perf instrumentation" ON) 
# IF(${NVPERF}) 
# find_package(NvPerf REQUIRED PATHS ${CMAKE_CURRENT_SOURCE_DIR}/cmake) 
# find_package(NvPerfUtility REQUIRED PATHS ${CMAKE_CURRENT_SOURCE_DIR}/cmake) 
# ENDIF() 
#set(NVPERF_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/NvPerf")
#find_package(NvPerf REQUIRED)

#set(NVPERF_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Samples/NvPerfUtility")
#find_package(nvPerfUtility REQUIRED)


# NVIDIA Perf SDK setup
option(NVPERF "Enable Nsight Perf instrumentation" ON)
if(${NVPERF})
    # Point to your copied CMake config files
    #list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
    
    find_package(NvPerf REQUIRED PATHS ./cmake) 
    find_package(NvPerfUtility REQUIRED PATHS ./cmake) 
    
    # For Windows Vulkan, we need these specific includes
    include_directories(
        ${NvPerf_INCLUDE_DIRS}
        ${NvPerfUtility_INCLUDE_DIRS}
        "${CMAKE_CURRENT_SOURCE_DIR}/NvPerf/include/windows-desktop-x64"
    )
endif()







	#if(NvPerf_FOUND)
		#target_compile_definitions(${PROJECT_NAME} PRIVATE NV_PERF_ENABLE_INSTRUMENTATION)
		#target_link_libraries(${PROJECT_NAME} NvPerf NvPerfUtility)
		#DeployNvPerf(${PROJECT_NAME} NvPerf)
		#target_link_libraries(${PROJECT_NAME} NvPerfUtilityImportsRyml NvPerfUtilityImportsImPlot)
	#endif()

#####################################################################################
# Source files for this project
#
file(GLOB SOURCE_FILES src/*.*)
file(GLOB SHADER_FILES shaders/*.glsl shaders/*.h)
file(GLOB EXTERN_FILES 3rdparty/miniply/*.*)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/src 
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders 
  ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/miniply
  ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/vrdx
  #${CMAKE_CURRENT_SOURCE_DIR}/NvPerf/include
  #${CMAKE_CURRENT_SOURCE_DIR}/Samples/NvPerfUtility/include
  #${CMAKE_CURRENT_SOURCE_DIR}/NvPerf/include/windows-desktop-x64
  )

#####################################################################################
# Executable
#

add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${COMMON_SOURCE_FILES} ${PACKAGE_SOURCE_FILES} ${SHADER_FILES} ${EXTERN_FILES})

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_DEFINE_MATH_OPERATORS )

#####################################################################################
# common source code needed for this sample
#
source_group(common FILES 
  ${COMMON_SOURCE_FILES}
  ${PACKAGE_SOURCE_FILES}
)
source_group("Shader Files" FILES ${SHADER_FILES})
source_group("Source Files" FILES ${SOURCE_FILES})
source_group("Extern Files" FILES ${EXTERN_FILES})

#####################################################################################
# Linkage
#

# Linking with other libraries
# Link libraries and include directories
target_link_libraries(${PROJECT_NAME} PRIVATE
  nvpro2::nvapp
  nvpro2::nvgui
  nvpro2::nvvkglsl
  nvpro2::nvshaders_host
  nvpro2::nvutils
  nvpro2::nvvk
  nvpro2::nvgpu_monitor
  vk_radix_sort
  #NvPerf-shared
  #NvPerf
  #NvPerfUtility
)



# NVIDIA Perf specific settings
if(NvPerf_FOUND)
    #Add the required compile definition
    target_compile_definitions(${PROJECT_NAME} PRIVATE NV_PERF_ENABLE_INSTRUMENTATION)
    
    # Link against NvPerf libraries
    target_link_libraries(${PROJECT_NAME} PRIVATE
        NvPerf
        NvPerfUtility
    )
    
    # Deploy the DLL to your output directory
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/NvPerf/bin/x64/nvperf_grfx_host.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
endif()





add_project_definitions(${PROJECT_NAME})

#####################################################################################
# copies files
#

copy_to_runtime_and_install( ${PROJECT_NAME}
    LOCAL_DIRS "${CMAKE_CURRENT_LIST_DIR}/shaders"
    AUTO
)
