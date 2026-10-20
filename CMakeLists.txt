cmake_minimum_required(VERSION 3.25)

project(HashClash VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(HASHCLASH_ENABLE_CUDA "Build the CUDA implementation of md5_birthdaysearch" OFF)
option(HASHCLASH_BUILD_TESTS "Build HashClash tests" OFF)
option(HASHCLASH_WARNINGS "Enable compiler warnings for local development" ON)
option(HASHCLASH_ENABLE_LTO "Enable link-time optimization" OFF)
option(HASHCLASH_ENABLE_AVX2 "Build the AVX2 birthday-search implementation when supported" ON)
option(HASHCLASH_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(HASHCLASH_ENABLE_ASAN "Enable the AddressSanitizer instrumentation" OFF)
option(HASHCLASH_ENABLE_TSAN "Enable the ThreadSanitizer instrumentation" OFF)
option(HASHCLASH_ENABLE_UBSAN "Enable the UndefinedBehaviorSanitizer instrumentation" OFF)
option(HASHCLASH_ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)

set(HASHCLASH_WARNING_OPTIONS)
if(HASHCLASH_WARNINGS)
  if(MSVC)
    list(APPEND HASHCLASH_WARNING_OPTIONS /W4 /permissive-)
  else()
    list(APPEND HASHCLASH_WARNING_OPTIONS -Wall -Wextra -Wpedantic)
  endif()
endif()
if(HASHCLASH_WARNINGS_AS_ERRORS)
  if(MSVC)
    list(APPEND HASHCLASH_WARNING_OPTIONS /WX)
  else()
    list(APPEND HASHCLASH_WARNING_OPTIONS -Werror)
  endif()
endif()

# Detect AVX2 support and compile only the SIMD translation unit with AVX2
# enabled; the runtime checks in birthday.cpp keep the binary usable on CPUs
# without AVX2.
set(HASHCLASH_HAVE_AVX2 FALSE)
if(HASHCLASH_ENABLE_AVX2 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|i[3-6]86)$")
  include(CheckCXXCompilerFlag)
  if(MSVC)
    check_cxx_compiler_flag("/arch:AVX2" HASHCLASH_COMPILER_HAS_AVX2_FLAG)
    set(HASHCLASH_AVX2_FLAG "/arch:AVX2")
  else()
    check_cxx_compiler_flag("-mavx2" HASHCLASH_COMPILER_HAS_AVX2_FLAG)
    set(HASHCLASH_AVX2_FLAG "-mavx2")
  endif()
  if(HASHCLASH_COMPILER_HAS_AVX2_FLAG)
    set(HASHCLASH_HAVE_AVX2 TRUE)
    message(STATUS "AVX2 birthday-search implementation enabled")
  else()
    message(STATUS "AVX2 requested but the compiler does not support the flag")
  endif()
endif()

if(HASHCLASH_ENABLE_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT HASHCLASH_IPO_SUPPORTED OUTPUT HASHCLASH_IPO_MESSAGE)
  if(NOT HASHCLASH_IPO_SUPPORTED)
    message(WARNING "Link-time optimization is not supported: ${HASHCLASH_IPO_MESSAGE}")
  else()
    message(STATUS "Link-time optimization enabled")
  endif()
endif()

# Sanitizers (GCC/Clang). ASan and TSan are mutually exclusive; UBSan may be
# combined with either. MSVC does not support these flags, so they are ignored
# there with a warning.
if(HASHCLASH_ENABLE_ASAN AND HASHCLASH_ENABLE_TSAN)
  message(FATAL_ERROR
    "HASHCLASH_ENABLE_ASAN and HASHCLASH_ENABLE_TSAN are mutually exclusive")
endif()

set(HASHCLASH_SANITIZERS)
if(HASHCLASH_ENABLE_ASAN)
  list(APPEND HASHCLASH_SANITIZERS address)
endif()
if(HASHCLASH_ENABLE_TSAN)
  list(APPEND HASHCLASH_SANITIZERS thread)
endif()
if(HASHCLASH_ENABLE_UBSAN)
  list(APPEND HASHCLASH_SANITIZERS undefined)
endif()

if(HASHCLASH_ENABLE_CUDA AND (HASHCLASH_SANITIZERS OR HASHCLASH_ENABLE_COVERAGE))
  message(FATAL_ERROR
    "CUDA cannot be combined with HASHCLASH_ENABLE_ASAN, HASHCLASH_ENABLE_TSAN, "
    "HASHCLASH_ENABLE_UBSAN, or HASHCLASH_ENABLE_COVERAGE")
endif()

set(HASHCLASH_SANITIZERS_SUPPORTED FALSE)
if(HASHCLASH_SANITIZERS)
  if(MSVC)
    message(WARNING
      "Sanitizers are not supported by MSVC; HASHCLASH_ENABLE_ASAN/TSAN/UBSAN are ignored")
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
    set(HASHCLASH_SANITIZERS_SUPPORTED TRUE)
    string(REPLACE ";" "," HASHCLASH_SANITIZE_FLAG "${HASHCLASH_SANITIZERS}")
    message(STATUS "Sanitizers enabled: ${HASHCLASH_SANITIZE_FLAG}")
  else()
    message(FATAL_ERROR
      "Sanitizers are supported only by GCC and Clang; "
      "${CMAKE_CXX_COMPILER_ID} is not supported")
  endif()
endif()

set(HASHCLASH_COVERAGE_SUPPORTED FALSE)
if(HASHCLASH_ENABLE_COVERAGE)
  if(MSVC)
    message(WARNING "Coverage flags are not supported by MSVC; HASHCLASH_ENABLE_COVERAGE is ignored")
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
    set(HASHCLASH_COVERAGE_SUPPORTED TRUE)
    message(STATUS "Coverage instrumentation enabled")
  else()
    message(WARNING
      "Coverage flags are supported only by GCC and Clang; "
      "HASHCLASH_ENABLE_COVERAGE is ignored for ${CMAKE_CXX_COMPILER_ID}")
  endif()
endif()

function(hashclash_apply_project_options target)
  cmake_parse_arguments(
    PARSE_ARGV 1 HASHCLASH_OPTIONS
    "EXPORT_RUNTIME_LINK_OPTIONS" "" ""
  )
  # Static-library consumers need the instrumentation runtime at their final link.
  set_target_properties("${target}" PROPERTIES CXX_EXTENSIONS OFF)

  if(HASHCLASH_WARNING_OPTIONS)
    foreach(HASHCLASH_WARNING_OPTION IN LISTS HASHCLASH_WARNING_OPTIONS)
      target_compile_options("${target}" PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${HASHCLASH_WARNING_OPTION}>"
      )
    endforeach()
  endif()

  if(HASHCLASH_SANITIZERS_SUPPORTED)
    target_compile_options("${target}" PRIVATE
      "$<$<COMPILE_LANGUAGE:CXX>:-fsanitize=${HASHCLASH_SANITIZE_FLAG}>"
    )
    if(HASHCLASH_OPTIONS_EXPORT_RUNTIME_LINK_OPTIONS)
      target_link_options("${target}" INTERFACE
        "$<$<LINK_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-fsanitize=${HASHCLASH_SANITIZE_FLAG}>"
      )
    endif()
    if(HASHCLASH_ENABLE_ASAN)
      target_compile_options("${target}" PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:-fno-omit-frame-pointer>"
      )
    endif()
  endif()

  if(HASHCLASH_COVERAGE_SUPPORTED)
    target_compile_options("${target}" PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:--coverage>")
    if(HASHCLASH_OPTIONS_EXPORT_RUNTIME_LINK_OPTIONS)
      target_link_options("${target}" INTERFACE
        "$<$<LINK_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:--coverage>"
      )
    endif()
  endif()

  if(HASHCLASH_ENABLE_LTO AND HASHCLASH_IPO_SUPPORTED)
    set_property(TARGET "${target}" PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()
endfunction()

find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(BZip2 REQUIRED)
find_package(CLI11 CONFIG REQUIRED)

find_package(cereal CONFIG REQUIRED)

set(HASHCLASH_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(GENERATE
  OUTPUT "${HASHCLASH_GENERATED_INCLUDE_DIR}/hashclash/config.h"
  CONTENT "#ifndef HASHCLASH_CONFIG_H
#define HASHCLASH_CONFIG_H

/* Generated by CMake. */

$<$<BOOL:${HASHCLASH_HAVE_AVX2}>:#define HASHCLASH_HAVE_AVX2>

#endif
"
)

set(HASHCLASH_LIBRARY_SOURCES
  src/hashclash/booleanfunction.cpp
  src/hashclash/conditions.cpp
  src/hashclash/differentialpath.cpp
  src/hashclash/md5detail.cpp
  src/hashclash/rng.cpp
  src/hashclash/sdr.cpp
  src/hashclash/sha1detail.cpp
  src/hashclash/sha1differentialpath.cpp
  src/hashclash/sha1messagespace.cpp
  src/hashclash/timer.cpp
)

add_library(hashclash STATIC ${HASHCLASH_LIBRARY_SOURCES})
add_library(HashClash::hashclash ALIAS hashclash)
target_compile_features(hashclash PUBLIC cxx_std_20)
target_include_directories(hashclash
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${HASHCLASH_GENERATED_INCLUDE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
hashclash_apply_project_options(hashclash EXPORT_RUNTIME_LINK_OPTIONS)
target_link_libraries(hashclash
  PUBLIC
    cereal::cereal
    BZip2::BZip2
    Threads::Threads
    ZLIB::ZLIB
)
if(WIN32)
  target_link_libraries(hashclash PUBLIC advapi32)
endif()

# Keep command-line parsing optional for consumers of the algorithm library.
add_library(hashclash_cli_options INTERFACE)
add_library(HashClash::cli11_options ALIAS hashclash_cli_options)
set_target_properties(hashclash_cli_options PROPERTIES EXPORT_NAME cli11_options)
target_compile_features(hashclash_cli_options INTERFACE cxx_std_20)
target_include_directories(hashclash_cli_options INTERFACE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_link_libraries(hashclash_cli_options INTERFACE CLI11::CLI11)

add_library(hashclash_sha1_attackgenerator_core OBJECT
  src/sha1attackgenerator/filters.cpp
  src/sha1attackgenerator/program_generator.cpp
  src/sha1attackgenerator/tunnel_analysis.cpp
)
target_include_directories(hashclash_sha1_attackgenerator_core
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/sha1attackgenerator"
)
target_link_libraries(hashclash_sha1_attackgenerator_core PUBLIC hashclash)
hashclash_apply_project_options(hashclash_sha1_attackgenerator_core)

function(hashclash_add_executable target)
  cmake_parse_arguments(PARSE_ARGV 1 HASHCLASH_APP "NO_HASHCLASH" "" "")
  add_executable("${target}" ${HASHCLASH_APP_UNPARSED_ARGUMENTS})
  set_target_properties("${target}" PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  )
  target_link_libraries("${target}" PRIVATE hashclash_cli_options)
  if(NOT HASHCLASH_APP_NO_HASHCLASH)
    target_link_libraries("${target}" PRIVATE hashclash)
  endif()
  hashclash_apply_project_options("${target}")
  set(HASHCLASH_EXECUTABLES ${HASHCLASH_EXECUTABLES} "${target}" PARENT_SCOPE)
endfunction()

hashclash_add_executable(md5_fastcoll
  src/md5fastcoll/main.cpp
  src/md5fastcoll/md5.cpp
  src/md5fastcoll/block0.cpp
  src/md5fastcoll/block1.cpp
  src/md5fastcoll/block1wang.cpp
  src/md5fastcoll/block1stevens00.cpp
  src/md5fastcoll/block1stevens01.cpp
  src/md5fastcoll/block1stevens10.cpp
  src/md5fastcoll/block1stevens11.cpp
)

hashclash_add_executable(md5_textcoll
  src/md5textcoll/main.cpp
  src/md5textcoll/prepareblock1.cpp
  src/md5textcoll/block1.cpp
  src/md5textcoll/block2.cpp
)

hashclash_add_executable(md5_diffpathforward
  src/md5forward/dostep.cpp
  src/md5forward/forward.cpp
  src/md5forward/main.cpp
)

hashclash_add_executable(md5_diffpathbackward
  src/md5backward/backward.cpp
  src/md5backward/dostep.cpp
  src/md5backward/main.cpp
)

hashclash_add_executable(md5_diffpathconnect
  src/md5connect/connect.cpp
  src/md5connect/dostep.cpp
  src/md5connect/main.cpp
)

hashclash_add_executable(md5_diffpathhelper
  src/md5helper/collisionfinding.cpp
  src/md5helper/convert.cpp
  src/md5helper/main.cpp
  src/md5helper/startnearcollision.cpp
)

set(HASHCLASH_BIRTHDAYSEARCH_SOURCES
  src/md5birthdaysearch/birthday.cpp
  src/md5birthdaysearch/dostep.cpp
  src/md5birthdaysearch/main.cpp
  src/md5birthdaysearch/storage.cpp
  src/md5birthdaysearch/simd_avx256.cpp
)
if(HASHCLASH_ENABLE_CUDA)
  enable_language(CUDA)
  find_package(CUDAToolkit REQUIRED)
  list(APPEND HASHCLASH_BIRTHDAYSEARCH_SOURCES src/md5birthdaysearch/cuda_md5.cu)
endif()
if(HASHCLASH_HAVE_AVX2)
  set_source_files_properties(src/md5birthdaysearch/simd_avx256.cpp
    PROPERTIES COMPILE_OPTIONS "${HASHCLASH_AVX2_FLAG}")
endif()
hashclash_add_executable(md5_birthdaysearch ${HASHCLASH_BIRTHDAYSEARCH_SOURCES})
if(HASHCLASH_ENABLE_CUDA)
  set_target_properties(md5_birthdaysearch PROPERTIES CUDA_STANDARD 20 CUDA_STANDARD_REQUIRED ON)
  target_compile_definitions(md5_birthdaysearch PRIVATE HASHCLASH_HAVE_CUDA)
  target_link_libraries(md5_birthdaysearch PRIVATE CUDA::cudart CUDA::cuda_driver)
endif()

hashclash_add_executable(sha1_diffpathforward
  src/sha1forward/dostep.cpp
  src/sha1forward/forward.cpp
  src/sha1forward/main.cpp
)

hashclash_add_executable(sha1_diffpathbackward
  src/sha1backward/backward.cpp
  src/sha1backward/dostep.cpp
  src/sha1backward/main.cpp
)

hashclash_add_executable(sha1_diffpathconnect
  src/sha1connect/connect.cpp
  src/sha1connect/dostep.cpp
  src/sha1connect/main.cpp
)

hashclash_add_executable(sha1_diffpathhelper
  src/sha1helper/convert.cpp
  src/sha1helper/filter.cpp
  src/sha1helper/main.cpp
)

hashclash_add_executable(sha1_attackgenerator
  NO_HASHCLASH
  src/sha1attackgenerator/checkokpaths.cpp
  src/sha1attackgenerator/collisionfinding.cpp
  src/sha1attackgenerator/main.cpp
)
target_link_libraries(sha1_attackgenerator PRIVATE hashclash_sha1_attackgenerator_core)

hashclash_add_executable(sha1_nearcollisionattack
  src/sha1attackgenerator/collfind.cpp
)

if(HASHCLASH_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

set(HASHCLASH_PACKAGE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/HashClash")

install(TARGETS hashclash
  EXPORT HashClashTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(TARGETS hashclash_cli_options
  EXPORT HashClashCLITargets
)
install(TARGETS ${HASHCLASH_EXECUTABLES}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(EXPORT HashClashTargets
  FILE HashClashTargets.cmake
  NAMESPACE HashClash::
  DESTINATION ${HASHCLASH_PACKAGE_INSTALL_DIR}
)
install(EXPORT HashClashCLITargets
  FILE HashClashCLITargets.cmake
  NAMESPACE HashClash::
  DESTINATION ${HASHCLASH_PACKAGE_INSTALL_DIR}
)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/HashClashConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/HashClashConfig.cmake"
  INSTALL_DESTINATION "${HASHCLASH_PACKAGE_INSTALL_DIR}"
)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/HashClashConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion
)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/HashClashConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/HashClashConfigVersion.cmake"
  DESTINATION "${HASHCLASH_PACKAGE_INSTALL_DIR}"
)

install(DIRECTORY include/hashclash
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.hpp"
)

install(FILES "${HASHCLASH_GENERATED_INCLUDE_DIR}/hashclash/config.h"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/hashclash
)
