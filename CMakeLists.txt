cmake_minimum_required(VERSION 3.15...3.25)

set(PROJECT_NAME coffee-engine)
project(${PROJECT_NAME} CXX)

macro(set_option OPTION_NAME OPTION_VALUE)
    set(${OPTION_NAME} ${OPTION_VALUE} CACHE INTERNAL "" FORCE)
endmacro()

macro(apply_patch LIBRARY_NAME)
    execute_process(COMMAND cmd /C "cd ${CMAKE_CURRENT_SOURCE_DIR}/libs/${LIBRARY_NAME} && git apply --ignore-space-change --ignore-whitespace ../${LIBRARY_NAME}.patch" OUTPUT_QUIET ERROR_QUIET)
endmacro()

set_option(BUILD_SHARED_LIBS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/GR-)
    add_compile_options(/EHa)
endif()

add_compile_definitions(ZSTD_STATIC_LINKING_ONLY)
add_compile_definitions(WIN32_LEAN_AND_MEAN)
add_compile_definitions(NOMINMAX)

find_package(Vulkan REQUIRED)

set_option(FMT_INSTALL OFF)
add_subdirectory(libs/fmt)

set_option(GLFW_BUILD_DOCS OFF)
set_option(GLFW_INSTALL OFF)
add_subdirectory(libs/glfw)

set_option(BUILD_SHARED_LIBS OFF)
set_option(BUILD_STATIC_LIBS ON)
add_subdirectory(libs/glm)

# OpenAL must be build as shared library (LGPL)
set_option(ALSOFT_UTILS OFF)
set_option(ALSOFT_NO_CONFIG_UTIL ON)
set_option(ALSOFT_EXAMPLES OFF)
set_option(ALSOFT_BUILD_IMPORT_LIB ON)
set_option(ALSOFT_INSTALL OFF)
set_option(ALSOFT_INSTALL_CONFIG OFF)
set_option(ALSOFT_INSTALL_HRTF_DATA OFF)
set_option(ALSOFT_INSTALL_AMBDEC_PRESETS OFF)
set_option(ALSOFT_INSTALL_EXAMPLES OFF)
set_option(ALSOFT_INSTALL_UTILS OFF)
set_option(ALSOFT_INSTALL_AMBDEC_PRESETS OFF)
add_subdirectory(libs/openal)

# TBB must be build as shared library (Unstable static build)
set_option(BUILD_SHARED_LIBS ON)
set_option(BUILD_STATIC_LIBS OFF)
set_option(TBB_TEST OFF)
set_option(TBB_OUTPUT_DIR_BASE "")
add_subdirectory(libs/tbb)

apply_patch(glm)
apply_patch(openal)
apply_patch(tbb)

file(GLOB_RECURSE SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.inl)

add_library(${PROJECT_NAME} STATIC ${SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/single-headers)
target_link_libraries(${PROJECT_NAME} PUBLIC fmt::fmt glfw glm::glm TBB::tbb TBB::tbbmalloc TBB::tbbmalloc_proxy OpenAL)

if(MSVC)
    target_link_options(${PROJECT_NAME} PUBLIC $<IF:$<EQUAL:${CMAKE_SIZEOF_VOID_P},4>,/include:___TBB_malloc_proxy,/include:__TBB_malloc_proxy>)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    
    set_target_properties(${PROJECT_NAME} PROPERTIES
        VS_GLOBAL_VcpkgEnabled FALSE
        VS_GLOBAL_ObjectFileName "$(IntDir)$(RelativeDir)"
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
    set_target_properties(fmt glfw glm glm_static OpenAL build_version common ex-common tbb tbbmalloc tbbmalloc_proxy PROPERTIES
        VS_GLOBAL_VcpkgEnabled FALSE
        FOLDER "coffee-engine-libs"
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
    
    foreach(SOURCE IN ITEMS ${SOURCES})
    	get_filename_component(SOURCE_PATH "${SOURCE}" PATH)
    	string(REPLACE "${CMAKE_SOURCE_DIR}" "" GROUP_PATH "${SOURCE_PATH}")
    	string(REPLACE "/" "\\" GROUP_PATH "${GROUP_PATH}")
    	source_group("${GROUP_PATH}" FILES "${SOURCE}")
    endforeach()
endif()