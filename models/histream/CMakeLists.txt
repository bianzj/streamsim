# CMakeList.txt: TIRT_3D 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required(VERSION 3.9.0 FATAL_ERROR)

#--------------------------------------------------------------------------------------------------
# Project setting
get_filename_component(PROJNAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(PROJNAME ${PROJNAME})
project(${PROJNAME} LANGUAGES C CXX)
message(STATUS "-------------------------------")

option(HISTREAM_ENABLE_FACET "Build integrated FacetRT and FacetEB backends" ON)
if (HISTREAM_ENABLE_FACET)
    find_package(Vulkan REQUIRED)
endif()
message(STATUS "Processing Project ${PROJNAME}:")


#--------------------------------------------------------------------------------------------------
# C++ target and defines
set(CMAKE_CXX_STANDARD 17)
add_executable(${PROJNAME}
        src/main.cpp)



if (MSVC)
    add_definitions(/wd26812)  # 'enum class' over 'enum'
    add_definitions(/wd26451)  # Arithmetic overflow, casting 4 byte value to 8 byte value
    target_compile_options(${PROJNAME} PRIVATE /utf-8)
endif ()

# find_package(LibLZMA REQUIRED)
# find_package(Eigen3 CONFIG REQUIRED)
# find_package(Boost REQUIRED)


#--------------------------------------------------------------------------------------------------
# look for nvpro_core 1) as a sub-folder 2) at some other locations
# this cannot be put anywhere else since we still didn't find setup.cmake yet
if (NOT BASE_DIRECTORY)
    find_path(BASE_DIRECTORY
            NAMES nvpro_core/cmake/setup.cmake
            PATHS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/../..
            REQUIRED
            DOC "Directory containing nvpro_core"
    )
endif ()


# Include customized C++ headers of the project
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

## Various functions and macros REQUIRED
if (EXISTS ${BASE_DIRECTORY}/nvpro_core/cmake/setup.cmake)
    include(${BASE_DIRECTORY}/nvpro_core/cmake/setup.cmake)
    include(${BASE_DIRECTORY}/nvpro_core/cmake/utilities.cmake)
else ()
    message(FATAL_ERROR "could not find base directory, please set BASE_DIRECTORY to folder containing nvpro_core")
endif ()

#--------------------------------------------------------------------------------------------------
# Package shared by all projects
_add_package_VulkanSDK()
_add_package_ImGUI()
#_add_package_FreeImage()

# nvpro_core ships the Windows NVML SDK supplement, but its FindNVML module
# only searches CUDA Toolkit directories for the import library.
if (WIN32)
    set(NVML_BUNDLED_DIR "${BASE_DIRECTORY}/nvpro_core/third_party/binaries/nvml")
    if (EXISTS "${NVML_BUNDLED_DIR}/nvml.h"
            AND EXISTS "${NVML_BUNDLED_DIR}/lib/x64/nvml.lib")
        set(NVML_LOCATION "${NVML_BUNDLED_DIR}" CACHE PATH
                "Directory containing the bundled NVML headers")
        set(NVML_LIBRARIES "${NVML_BUNDLED_DIR}/lib/x64/nvml.lib" CACHE FILEPATH
                "Bundled NVML import library" FORCE)
    endif ()
endif ()

# Add the following for GPU load and memory
_add_package_NVML()
# This should be added after all packages
_add_nvpro_core_lib()
# Check if using new verion nvpro_core

add_definitions(-DUSE_NEW_NVPRO_CORE)
message(STATUS "Use new nvpro_core")


#--------------------------------------------------------------------------------------------------
# Memory Allocation
#   Forcing to use our memory allocator DMA (similar to Vulkan Memory Allocator (VMA))
#target_compile_definitions(${PROJNAME} PUBLIC ALLOC_VMA)
target_compile_definitions(${PROJNAME} PUBLIC ALLOC_DMA)
#target_compile_definitions(${PROJNAME} PUBLIC ALLOC_DEDICATED)


#--------------------------------------------------------------------------------------------------
# Default definitions: PROJECT_RELDIRECTORY, ...
_add_project_definitions(${PROJNAME})

#--------------------------------------------------------------------------------------------------
# For NVML, the DLL is loaded by the application
#if(MSVC)
#  set_target_properties(${PROJNAME} PROPERTIES LINK_FLAGS "/DELAYLOAD:nvml.dll")
#endif()


#--------------------------------------------------------------------------------------------------
# Source files for this project
file(GLOB SOURCE_FILES
        src/*.cpp
        src/base/*.cpp
        src/thirdparty/*.cpp
        src/raytracing/*.cpp
        src/voxeleb/*.cpp
        src/voxelrt/*.cpp
)

file(GLOB HEADER_FILES
        src/*.h
        src/base/*.h
        src/thirdparty/*.h
        src/raytracing/*.h
        src/voxeleb/*.h
        src/voxelrt/*.h
)

if (HISTREAM_ENABLE_FACET)
    file(GLOB FACET_SOURCE_FILES
            src/facetrt/*.cpp
            src/faceteb/*.cpp)
    file(GLOB FACET_HEADER_FILES
            src/facetrt/*.h
            src/faceteb/*.h)
    list(APPEND SOURCE_FILES ${FACET_SOURCE_FILES})
    list(APPEND HEADER_FILES ${FACET_HEADER_FILES})
endif()

#--------------------------------------------------------------------------------------------------
# GLSL to SPIR-V custom build
#compile_glsl_directory(
#	SRC "${CMAKE_CURRENT_SOURCE_DIR}/shader/VoxelTracing"
#	DST "${CMAKE_CURRENT_SOURCE_DIR}/shader/VoxelTracing"
#	VULKAN_TARGET "vulkan1.3"
#	DEPENDENCY ${VULKAN_BUILD_DEPENDENCIES}
#	)


#--------------------------------------------------------------------------------------------------
# Sources
target_sources(${PROJNAME} PUBLIC ${SOURCE_FILES} ${HEADER_FILES})
target_sources(${PROJNAME} PUBLIC ${COMMON_SOURCE_FILES})
target_sources(${PROJNAME} PUBLIC ${PACKAGE_SOURCE_FILES})
target_sources(${PROJNAME} PUBLIC ${GLSL_SOURCES})
target_sources(${PROJNAME} PUBLIC ${GLSL_HEADERS})

find_program(HISTREAM_GLSLC
        NAMES glslc
        HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/Bin32"
        REQUIRED)

set(HISTREAM_RAYTRACING_SHADER_FILES
        offline.rgen
        closestHit.rchit
        rmiss.rmiss
        shadow.rmiss)
set(HISTREAM_RAYTRACING_SHADER_BINARY_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/shader/raytracing")
set(HISTREAM_RAYTRACING_SHADER_OUTPUTS)
foreach (RAYTRACING_SHADER_FILE IN LISTS HISTREAM_RAYTRACING_SHADER_FILES)
    set(RAYTRACING_SHADER_OUTPUT
            "${HISTREAM_RAYTRACING_SHADER_BINARY_DIR}/${RAYTRACING_SHADER_FILE}.spv")
    add_custom_command(
            OUTPUT "${RAYTRACING_SHADER_OUTPUT}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                    "${HISTREAM_RAYTRACING_SHADER_BINARY_DIR}"
            COMMAND "${HISTREAM_GLSLC}" --target-env=vulkan1.2
                    "${CMAKE_CURRENT_SOURCE_DIR}/shader/raytracing/${RAYTRACING_SHADER_FILE}"
                    -o "${RAYTRACING_SHADER_OUTPUT}"
            DEPENDS
                    "${CMAKE_CURRENT_SOURCE_DIR}/shader/raytracing/${RAYTRACING_SHADER_FILE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/shader/global.glsl"
                    "${CMAKE_CURRENT_SOURCE_DIR}/shader/functions.glsl"
                    "${CMAKE_CURRENT_SOURCE_DIR}/shader/bsdf.glsl"
            VERBATIM)
    list(APPEND HISTREAM_RAYTRACING_SHADER_OUTPUTS "${RAYTRACING_SHADER_OUTPUT}")
endforeach()
add_custom_target(histream_raytracing_shaders ALL
        DEPENDS ${HISTREAM_RAYTRACING_SHADER_OUTPUTS})
add_dependencies(${PROJNAME} histream_raytracing_shaders)

if (HISTREAM_ENABLE_FACET)
    target_link_libraries(${PROJNAME} Vulkan::Vulkan)

    set(HISTREAM_FACET_SHADER_FILES
            visibility.vert
            visibility.frag
            visibility_resolve.comp
            direct_resolve.comp
            radiosity.comp)
    foreach (FACET_KIND IN ITEMS facetrt faceteb)
        set(FACET_SHADER_SOURCE_DIR
                "${CMAKE_CURRENT_SOURCE_DIR}/shader/${FACET_KIND}")
        set(FACET_SHADER_BINARY_DIR
                "${CMAKE_CURRENT_BINARY_DIR}/shader/${FACET_KIND}")
        set(FACET_SHADER_OUTPUTS)
        foreach (FACET_SHADER_FILE IN LISTS HISTREAM_FACET_SHADER_FILES)
            set(FACET_SHADER_OUTPUT
                    "${FACET_SHADER_BINARY_DIR}/${FACET_SHADER_FILE}.spv")
            add_custom_command(
                    OUTPUT "${FACET_SHADER_OUTPUT}"
                    COMMAND "${CMAKE_COMMAND}" -E make_directory
                            "${FACET_SHADER_BINARY_DIR}"
                    COMMAND "${HISTREAM_GLSLC}" --target-env=vulkan1.2
                            "${FACET_SHADER_SOURCE_DIR}/${FACET_SHADER_FILE}"
                            -o "${FACET_SHADER_OUTPUT}"
                    DEPENDS "${FACET_SHADER_SOURCE_DIR}/${FACET_SHADER_FILE}"
                    VERBATIM)
            list(APPEND FACET_SHADER_OUTPUTS "${FACET_SHADER_OUTPUT}")
        endforeach()
        add_custom_target(histream_${FACET_KIND}_shaders ALL
                DEPENDS ${FACET_SHADER_OUTPUTS})
        add_dependencies(${PROJNAME} histream_${FACET_KIND}_shaders)
    endforeach()
endif()

file(GLOB HISTREAM_VOXELEB_SHADER_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/shader/voxeleb/*.comp")
set(HISTREAM_VOXELEB_SHADER_OUTPUTS)
set(HISTREAM_VOXELEB_SHADER_BINARY_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/shader/voxeleb")
foreach (VOXELEB_SHADER_SOURCE IN LISTS HISTREAM_VOXELEB_SHADER_FILES)
    get_filename_component(VOXELEB_SHADER_FILE "${VOXELEB_SHADER_SOURCE}" NAME)
    set(VOXELEB_SHADER_OUTPUT
            "${HISTREAM_VOXELEB_SHADER_BINARY_DIR}/${VOXELEB_SHADER_FILE}.spv")
    add_custom_command(
            OUTPUT "${VOXELEB_SHADER_OUTPUT}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                    "${HISTREAM_VOXELEB_SHADER_BINARY_DIR}"
            COMMAND "${HISTREAM_GLSLC}" --target-env=vulkan1.2
                    "${VOXELEB_SHADER_SOURCE}"
                    -o "${VOXELEB_SHADER_OUTPUT}"
            DEPENDS "${VOXELEB_SHADER_SOURCE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/shader/parameters.h"
                    "${CMAKE_CURRENT_SOURCE_DIR}/shader/bindingLayout.h"
                    "${CMAKE_CURRENT_SOURCE_DIR}/shader/global.glsl"
                    "${CMAKE_CURRENT_SOURCE_DIR}/shader/voxeleb/fluid_common.glsl"
                    "${CMAKE_CURRENT_SOURCE_DIR}/shader/voxeleb/fluid_coupling.glsl"
            VERBATIM)
    list(APPEND HISTREAM_VOXELEB_SHADER_OUTPUTS "${VOXELEB_SHADER_OUTPUT}")
endforeach()
add_custom_target(histream_voxeleb_shaders ALL
        DEPENDS ${HISTREAM_VOXELEB_SHADER_OUTPUTS})
add_dependencies(${PROJNAME} histream_voxeleb_shaders)

file(GLOB HISTREAM_VOXELRT_SHADER_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/shader/voxelrt/*.comp")
set(HISTREAM_VOXELRT_SHADER_OUTPUTS)
set(HISTREAM_VOXELRT_SHADER_BINARY_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/shader/voxelrt")
foreach (VOXELRT_SHADER_SOURCE IN LISTS HISTREAM_VOXELRT_SHADER_FILES)
    get_filename_component(VOXELRT_SHADER_FILE "${VOXELRT_SHADER_SOURCE}" NAME)
    set(VOXELRT_SHADER_OUTPUT
            "${HISTREAM_VOXELRT_SHADER_BINARY_DIR}/${VOXELRT_SHADER_FILE}.spv")
    add_custom_command(
            OUTPUT "${VOXELRT_SHADER_OUTPUT}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                    "${HISTREAM_VOXELRT_SHADER_BINARY_DIR}"
            COMMAND "${HISTREAM_GLSLC}" --target-env=vulkan1.2
                    "${VOXELRT_SHADER_SOURCE}"
                    -o "${VOXELRT_SHADER_OUTPUT}"
            DEPENDS "${VOXELRT_SHADER_SOURCE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/shader/parameters.h"
            VERBATIM)
    list(APPEND HISTREAM_VOXELRT_SHADER_OUTPUTS "${VOXELRT_SHADER_OUTPUT}")
endforeach()
add_custom_target(histream_voxelrt_shaders ALL
        DEPENDS ${HISTREAM_VOXELRT_SHADER_OUTPUTS})
add_dependencies(${PROJNAME} histream_voxelrt_shaders)


#-------------------------------------------------------------------------------------------------
# Sub-folders in Visual Studio
#
source_group("Common" FILES ${COMMON_SOURCE_FILES} ${PACKAGE_SOURCE_FILES})
source_group("Sources" FILES ${SOURCE_FILES})
source_group("Header Files" FILES ${HEADER_FILES})
source_group("Shader Sources" FILES ${GLSL_SOURCES})
source_group("Shader Headers" FILES ${GLSL_HEADERS})


#####################################################################################
# Linkage
#

#set(Boost_USE_STATIC_LIBS ON)
#find_package(Boost COMPONENTS filesystem iostreams REQUIRED)
#if (Boost_FOUND)
#    include_directories(${Boost_INCLUDE_DIRS})
#    link_directories(${Boost_LIBRARY_DIRS})
#endif ()

##########################################################
### external libararies
target_link_libraries(${PROJNAME} ${PLATFORM_LIBRARIES} nvpro_core)

find_package(GDAL REQUIRED)
target_include_directories(${PROJNAME} PUBLIC ${GDAL_INCLUDE_DIRS})
target_link_libraries(${PROJNAME} ${GDAL_LIBRARIES})

find_package(Eigen3 CONFIG REQUIRED)
target_link_libraries(${PROJNAME}  Eigen3::Eigen)

set(OpenCV_DIR "${VCPKG_INSTALLED_DIR}/x64-windows/share/opencv2")
find_package(OpenCV REQUIRED)
target_link_libraries(${PROJNAME} ${OpenCV_LIBS})


foreach (DEBUGLIB ${LIBRARIES_DEBUG})
    target_link_libraries(${PROJNAME} debug ${DEBUGLIB})
endforeach (DEBUGLIB)

foreach (RELEASELIB ${LIBRARIES_OPTIMIZED})
    target_link_libraries(${PROJNAME} optimized ${RELEASELIB})
endforeach (RELEASELIB)

#####################################################################################
# copies binaries that need to be put next to the exe files (ZLib, etc.)
#


# Deploy runtime resources next to histream.exe so XML-driven projects use the same
# shader and predefined-data layout as the built-in configurations.
add_custom_command(TARGET ${PROJNAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/shader"
            "$<TARGET_FILE_DIR:${PROJNAME}>/shader"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/defined"
            "$<TARGET_FILE_DIR:${PROJNAME}>/defined"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_BINARY_DIR}/shader/facetrt"
            "$<TARGET_FILE_DIR:${PROJNAME}>/shader/facetrt"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_BINARY_DIR}/shader/faceteb"
            "$<TARGET_FILE_DIR:${PROJNAME}>/shader/faceteb"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_BINARY_DIR}/shader/voxeleb"
            "$<TARGET_FILE_DIR:${PROJNAME}>/shader/voxeleb"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_BINARY_DIR}/shader/voxelrt"
            "$<TARGET_FILE_DIR:${PROJNAME}>/shader/voxelrt"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_BINARY_DIR}/shader/raytracing"
            "$<TARGET_FILE_DIR:${PROJNAME}>/shader/raytracing"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "$<IF:$<CONFIG:Debug>,${VCPKG_INSTALLED_DIR}/x64-windows/debug/bin,${VCPKG_INSTALLED_DIR}/x64-windows/bin>"
            "$<TARGET_FILE_DIR:${PROJNAME}>"
    COMMENT "Deploying HiStream shader and defined runtime resources"
)

_finalize_target(${PROJNAME})
