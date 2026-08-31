cmake_minimum_required(VERSION 3.27)


set(CMAKE_CXX_STANDARD 17)
###project(tirteb_radiosity)
##set(tirteb_radiosity ${PROJNAME})
##message(STATUS "-------------------------------")
##message(STATUS "Processing Project ${PROJNAME}:")
project(tirteb_radiosity)

set(CMAKE_OSX_ARCHITECTURES "arm64")

MESSAGE("ABC")

##--------------------------------------------------------------------------------------------------
## Project setting
#get_filename_component(PROJNAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
#set(PROJNAME ${PROJNAME})
#project(${PROJNAME} LANGUAGES C CXX)
#message(STATUS "-------------------------------")
#message(STATUS "Processing Project ${PROJNAME}:")

#add_executable(tirteb_radiosity main.cpp
#        base/facetio.h
#        base/facetio.cpp
#        base/structs.h
#        base/scifuns.h
#        base/rt.cpp
#        base/rt.h
#        base/eb.h
#        base/eb.cpp
#        base/leafopt.cpp
#        base/leafopt.h
#        base/soilopt.cpp
#        base/soilopt.h
#        base/engine.cpp
#        base/engine.h
#        base/eb.cpp
#        base/eb.h
#        base/evapo.cpp
#        base/evapo.h
#        base/aerodynamic.cpp
#        base/aerodynamic.h
#        base/biochemical.cpp
#        base/biochemical.h
#        base/geometry.cpp
#        base/geometry.h
#        base/utils.h
#        base/utils.cpp
#        base/xmlexamples.cpp
#        base/xmlexamples.h
#        base/scene.cpp
#        base/scene.h
#        base/fileio.cpp
#        base/fileio.h
#        radiosity/radiosity.cpp
#        radiosity/radiosity.h
#        radiosity/radiosityio.cpp
#        radiosity/radiosityio.h
#        radiosityeb/radiosityeb.cpp
#        radiosityeb/radiosityeb.h
#        radiosityeb/radiosityebio.cpp
#        radiosityeb/radiosityebio.h
#        base/compo.cpp
#        base/compo.h
#        base/meshio.cpp
#        base/meshio.h
#        base/virtualio.cpp
#        base/virtualio.h
#        base/virtual.cpp
#        base/virtual.h
#        base/objloader.h
#        base/objloader.cpp
#        base/instanceio.cpp
#        base/instanceio.h
#        base/compo.cpp
#        base/compo.h
#        thirdparty/tiny_obj_loader.h
#        thirdparty/tinyxml.h
#        thirdparty/nvmath.h
#        base/scifuns.h
#        base/scifuns.cpp
#        base/scifuns.cpp
#        base/defined.cpp
#        base/defined.h
#        base/leafopt.cpp
#        base/leafopt.h
#        base/soilopt.cpp
#        base/soilopt.h
#)

add_executable(tirteb_radiosity main.cpp
)

option(RADIOSITY_ENABLE_VULKAN "Build the raster-only Vulkan facet radiosity model" ON)

if (RADIOSITY_ENABLE_VULKAN)
    find_package(Vulkan REQUIRED)

    target_sources(tirteb_radiosity PRIVATE
            vulkanradiosity/vulkan_radiosity.cpp
            vulkanradiosity/vulkan_radiosity.h
            vulkanradiosity/radiosityeb_vulkan.cpp
            vulkanradiosity/radiosityeb_vulkan.h)
    target_link_libraries(tirteb_radiosity Vulkan::Vulkan)
    target_compile_definitions(tirteb_radiosity PRIVATE RADIOSITY_ENABLE_VULKAN=1)

    find_program(RADIOSITY_GLSLC
            NAMES glslc
            HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/Bin32"
            REQUIRED)

    set(RADIOSITY_SHADER_SOURCE_DIR
            "${CMAKE_CURRENT_SOURCE_DIR}/shader/vulkanradiosity")
    set(RADIOSITY_SHADER_BINARY_DIR
            "${CMAKE_CURRENT_BINARY_DIR}/shader/vulkanradiosity")
    set(RADIOSITY_GLSL
            visibility.vert
            visibility.frag
            visibility_resolve.comp
            direct_resolve.comp
            radiosity.comp)
    set(RADIOSITY_SPIRV)
    foreach (SHADER_FILE IN LISTS RADIOSITY_GLSL)
        set(SPIRV_FILE "${RADIOSITY_SHADER_BINARY_DIR}/${SHADER_FILE}.spv")
        add_custom_command(
                OUTPUT "${SPIRV_FILE}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${RADIOSITY_SHADER_BINARY_DIR}"
                COMMAND "${RADIOSITY_GLSLC}" --target-env=vulkan1.2
                        "${RADIOSITY_SHADER_SOURCE_DIR}/${SHADER_FILE}" -o "${SPIRV_FILE}"
                DEPENDS "${RADIOSITY_SHADER_SOURCE_DIR}/${SHADER_FILE}"
                VERBATIM)
        list(APPEND RADIOSITY_SPIRV "${SPIRV_FILE}")
    endforeach ()
    add_custom_target(radiosity_vulkan_shaders ALL DEPENDS ${RADIOSITY_SPIRV})
    add_dependencies(tirteb_radiosity radiosity_vulkan_shaders)
    target_compile_definitions(tirteb_radiosity PRIVATE
            RADIOSITY_VULKAN_SHADER_DIR="${RADIOSITY_SHADER_BINARY_DIR}")

    add_executable(vulkan_radiosity_smoke EXCLUDE_FROM_ALL
            vulkanradiosity/smoke.cpp
            vulkanradiosity/vulkan_radiosity.cpp
            vulkanradiosity/vulkan_radiosity.h)
    target_link_libraries(vulkan_radiosity_smoke Vulkan::Vulkan)
    target_compile_definitions(vulkan_radiosity_smoke PRIVATE
            RADIOSITY_VULKAN_SHADER_DIR="${RADIOSITY_SHADER_BINARY_DIR}")
    add_dependencies(vulkan_radiosity_smoke radiosity_vulkan_shaders)

    add_executable(vulkan_radiosity_tree EXCLUDE_FROM_ALL
            vulkanradiosity/tree_scene.cpp
            vulkanradiosity/vulkan_radiosity.cpp
            vulkanradiosity/vulkan_radiosity.h)
    target_link_libraries(vulkan_radiosity_tree Vulkan::Vulkan)
    target_compile_definitions(vulkan_radiosity_tree PRIVATE
            RADIOSITY_VULKAN_SHADER_DIR="${RADIOSITY_SHADER_BINARY_DIR}")
    add_dependencies(vulkan_radiosity_tree radiosity_vulkan_shaders)
endif ()

#--------------------------------------------------------------------------------------------------
# Source files for this project
file(GLOB SOURCE_FILES
        base/*.cpp
        thirdparty/*.cpp
        radiosity/*.cpp
        radiosityeb/*.cpp
)

file(GLOB HEADER_FILES
        base/*.h
        thirdparty/*.h
        radiosity/*.h
        radiosityeb/*.h
)

#--------------------------------------------------------------------------------------------------
# Sources
target_sources(tirteb_radiosity PUBLIC ${SOURCE_FILES} ${HEADER_FILES})

#find_package(GDAL REQUIRED)
#MESSAGE(${GDAL_INCLUDE_DIRS})
#target_include_directories(tirteb_radiosity ${GDAL_INCLUDE_DIRS})
#target_link_libraries(tirteb_radiosity GDAL::GDAL)
#
## find libInterpolate
#find_package(libInterpolate REQUIRED)

find_package(GDAL REQUIRED)
target_link_libraries(tirteb_radiosity GDAL::GDAL)

#
set(OpenCV_DIR "${VCPKG_INSTALLED_DIR}/x64-windows/share/opencv2")
find_package(OpenCV REQUIRED)
target_link_libraries(tirteb_radiosity ${OpenCV_LIBS})

if (RADIOSITY_ENABLE_VULKAN)
    add_executable(radiosity_web_runner EXCLUDE_FROM_ALL
            gui/native_runner.cpp
            vulkanradiosity/vulkan_radiosity.cpp
            vulkanradiosity/vulkan_radiosity.h)
    target_link_libraries(radiosity_web_runner Vulkan::Vulkan)
    target_compile_definitions(radiosity_web_runner PRIVATE
            RADIOSITY_VULKAN_SHADER_DIR="${RADIOSITY_SHADER_BINARY_DIR}")
    add_dependencies(radiosity_web_runner radiosity_vulkan_shaders)
endif ()

find_package(glm CONFIG QUIET)
if (glm_FOUND)
    target_link_libraries(tirteb_radiosity glm::glm)
elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../nvpro_core/third_party/glm/glm/glm.hpp")
    target_include_directories(tirteb_radiosity PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../nvpro_core/third_party/glm")
else ()
    message(FATAL_ERROR "GLM not found")
endif ()

find_package(Eigen3 REQUIRED)
target_link_libraries(tirteb_radiosity Eigen3::Eigen)

#set(Boost_USE_STATIC_LIBS ON)
#find_package(Boost COMPONENTS filesystem iostreams REQUIRED)
#if (Boost_FOUND)
#    include_directories(${Boost_INCLUDE_DIRS})
#    link_directories(${Boost_LIBRARY_DIRS})
#endif ()
#find_package(tinyobjloader CONFIG REQUIRED)
#target_link_libraries(tirteb_radiosity tinyobjloader::tinyobjloader)

######################################################################################
## copies binaries that need to be put next to the exe files (ZLib, etc.)
##
#_finalize_target(${PROJNAME})