cmake_minimum_required(VERSION 3.6.0)

# ADDITIONAL_BIN_PATH is used to pass Ninja's location to the ExternalProject below
get_filename_component(NINJA_BIN_DIR "${CMAKE_MAKE_PROGRAM}" DIRECTORY)
set(ADDITIONAL_BIN_PATH "${CMAKE_PROGRAM_PATH}")
list(APPEND ADDITIONAL_BIN_PATH "${NINJA_BIN_DIR}")

# mozjpeg build configurations
set(MOZJPEG_PREFIX ${PROJECT_BINARY_DIR}/mozjpeg)
set(MOZJPEG_SRC ${MOZJPEG_PREFIX}/src)
set(MOZJPEG_BIN ${MOZJPEG_PREFIX}/bin)
set(MOZJPEG_INSTALL ${PROJECT_BINARY_DIR}/install)

set(MOZJPEG_STATIC_LIB ${MOZJPEG_BIN}/libturbojpeg.a)

include(ExternalProject)
ExternalProject_Add(
    cmozjpeg
    PREFIX ${MOZJPEG_PREFIX}
    GIT_REPOSITORY https://github.com/espresso3389/mozjpeg
    SOURCE_DIR ${MOZJPEG_SRC}
    BINARY_DIR ${MOZJPEG_BIN}
    STAMP_DIR ${MOZJPEG_PREFIX}/stamp
    INSTALL_DIR ${MOZJPEG_INSTALL}
    # It's important to pass each variables one by one.
    # And the most important variable is CMAKE_PROGRAM_PATH that contains Ninja's location.
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DANDROID_ABI=${ANDROID_ABI} -DANDROID_PLATFORM=${ANDROID_PLATFORM} -DANDROID_NDK=${ANDROID_NDK} -DCMAKE_TOOLCHAIN_FILE=${CMAKE_ANDROID_NDK}/build/cmake/android.toolchain.cmake -DANDROID_TOOLCHAIN_NAME=${ANDROID_TOOLCHAIN_NAME} -DENABLE_SHARED=FALSE -DENABLE_STATIC=TRUE -DPNG_SUPPORTED=FALSE -DWITH_SIMD=TRUE -DWITH_TURBOJPEG=TRUE -DANDROID_ARM_NEON=TRUE -DCMAKE_PROGRAM_PATH=${ADDITIONAL_BIN_PATH}
    BUILD_BYPRODUCTS ${MOZJPEG_STATIC_LIB}
)

add_library(mozjpeglib STATIC IMPORTED)
add_dependencies(mozjpeglib cmozjpeg)
set_target_properties(mozjpeglib PROPERTIES IMPORTED_LOCATION ${MOZJPEG_STATIC_LIB})
set_target_properties(mozjpeglib PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${MOZJPEG_SRC};${MOZJPEG_BIN}")

set(IOS_CLASSES_DIR "../ios/Classes")

file(GLOB SRC_FILES
    ${IOS_CLASSES_DIR}/*.cpp
    ${IOS_CLASSES_DIR}/*.c
    ${IOS_CLASSES_DIR}/dart-sdk/*.c
)

add_library(flutter_mozjpeg
    SHARED
    ${SRC_FILES}
)
target_include_directories(flutter_mozjpeg PRIVATE ${IOS_CLASSES_DIR} ${IOS_CLASSES_DIR}/dart-sdk)
target_link_libraries(flutter_mozjpeg mozjpeglib)
target_compile_options(flutter_mozjpeg PRIVATE -DBUILD_FOR_ANDROID)
