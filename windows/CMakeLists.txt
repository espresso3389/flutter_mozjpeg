cmake_minimum_required(VERSION 3.14)

# Project-level configuration.
set(PROJECT_NAME "flutter_mozjpeg")
project(${PROJECT_NAME} LANGUAGES CXX C)

# Explicitly opt in to modern CMake behaviors to avoid warnings with recent
# versions of CMake.
cmake_policy(VERSION 3.14...3.25)

# This value is used when generating builds using this plugin, so it must
# not be changed
set(PLUGIN_NAME "flutter_mozjpeg_plugin")

# ADDITIONAL_BIN_PATH is used to pass Ninja's location to the ExternalProject below
get_filename_component(NINJA_BIN_DIR "${CMAKE_MAKE_PROGRAM}" DIRECTORY)
set(ADDITIONAL_BIN_PATH "${CMAKE_PROGRAM_PATH}")
list(APPEND ADDITIONAL_BIN_PATH "${NINJA_BIN_DIR}")

# mozjpeg build configurations
set(MOZJPEG_PREFIX ${PROJECT_BINARY_DIR}/mozjpeg)
set(MOZJPEG_SRC ${MOZJPEG_PREFIX}/src)
set(MOZJPEG_BIN ${MOZJPEG_PREFIX}/bin)
set(MOZJPEG_INSTALL ${PROJECT_BINARY_DIR}/install)

if(CMAKE_BUILD_TYPE_INIT STREQUAL "Debug")
    set(MOZJPEG_BUILD_TYPE "Debug")
else()
    set(MOZJPEG_BUILD_TYPE "Release")
endif()

set(PLUGIN_DLL_NAME ${PROJECT_BINARY_DIR}/${MOZJPEG_BUILD_TYPE}/flutter_mozjpeg_plugin.dll)

set(MOZJPEG_STATIC_LIB ${MOZJPEG_BIN}/${MOZJPEG_BUILD_TYPE}/turbojpeg-static.lib)


include(ExternalProject)
ExternalProject_Add(
    cmozjpeg
    PREFIX ${MOZJPEG_PREFIX}
    GIT_REPOSITORY https://github.com/espresso3389/mozjpeg
    SOURCE_DIR ${MOZJPEG_SRC}
    BINARY_DIR ${MOZJPEG_BIN}
    STAMP_DIR ${MOZJPEG_PREFIX}/stamp
    INSTALL_DIR ${MOZJPEG_INSTALL}
    # Pass each CMake variable one by one.
    # And the most important variable is CMAKE_PROGRAM_PATH that contains Ninja's location.
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DENABLE_SHARED=FALSE -DENABLE_STATIC=TRUE -DPNG_SUPPORTED=FALSE -DWITH_SIMD=TRUE -DWITH_TURBOJPEG=TRUE -DCMAKE_PROGRAM_PATH=${ADDITIONAL_BIN_PATH} -DCMAKE_INSTALL_PREFIX=${MOZJPEG_INSTALL} BUILD_BYPRODUCTS ${MOZJPEG_STATIC_LIB}
)

add_library(mozjpeglib STATIC IMPORTED)
add_dependencies(mozjpeglib cmozjpeg)
set_target_properties(mozjpeglib PROPERTIES IMPORTED_LOCATION ${MOZJPEG_STATIC_LIB})
set_target_properties(mozjpeglib PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${MOZJPEG_SRC};${MOZJPEG_BIN}")

# Any new source files that you add to the plugin should be added here.
set(IOS_CLASSES_DIR "../ios/Classes")

file(GLOB PLUGIN_SOURCES
    ${IOS_CLASSES_DIR}/*.cpp
    ${IOS_CLASSES_DIR}/*.c
    ${IOS_CLASSES_DIR}/dart-sdk/*.c
)

# Define the plugin library target. Its name must not be changed (see comment
# on PLUGIN_NAME above).
add_library(${PLUGIN_NAME} SHARED
  ${PLUGIN_SOURCES}
)

target_include_directories(${PLUGIN_NAME} PRIVATE ${IOS_CLASSES_DIR} ${IOS_CLASSES_DIR}/dart-sdk)

# Apply a standard set of build settings that are configured in the
# application-level CMakeLists.txt. This can be removed for plugins that want
# full control over build settings.
apply_standard_settings(${PLUGIN_NAME})

# Symbols are hidden by default to reduce the chance of accidental conflicts
# between plugins. This should not be removed; any symbols that should be
# exported should be explicitly exported with the FLUTTER_PLUGIN_EXPORT macro.
set_target_properties(${PLUGIN_NAME} PROPERTIES
  CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL _CRT_SECURE_NO_WARNINGS DART_SHARED_LIB)

# Source include directories and library dependencies. Add any plugin-specific
# dependencies here.
target_include_directories(${PLUGIN_NAME} INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include")
  target_link_libraries(${PLUGIN_NAME} PRIVATE flutter flutter_wrapper_plugin mozjpeglib)

# List of absolute paths to libraries that should be bundled with the plugin.
# This list could contain prebuilt libraries, or libraries created by an
# external build triggered from this build file.
set(flutter_mozjpeg_bundled_libraries
  ${PLUGIN_DLL_NAME}
  PARENT_SCOPE
)
