include(FetchContent)

find_package(Catch2 3 CONFIG QUIET)
if(NOT Catch2_FOUND)
  message(STATUS "Catch2 not found; fetching Catch2 v3.16.1")
  FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.16.1
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(Catch2)
endif()

add_executable(hashclash_tests
  test_aligned_buffer.cpp
  test_booleanfunction.cpp
  test_cli11_options.cpp
  test_conditions.cpp
  test_constants.cpp
  test_differentialpath.cpp
  test_cpuperformance.cpp
  test_md5detail.cpp
  test_operators.cpp
  test_program_generator.cpp
  test_rng.cpp
  test_saveload.cpp
  test_sdr.cpp
  test_sha1detail.cpp
  test_sha1differentialpath.cpp
  test_timer.cpp
  test_workload.cpp
)

target_include_directories(hashclash_tests PRIVATE
  "${PROJECT_SOURCE_DIR}/src/sha1attackgenerator"
)

target_link_libraries(hashclash_tests
  PRIVATE
    hashclash_cli_options
    hashclash_sha1_attackgenerator_core
    Catch2::Catch2WithMain
)
hashclash_apply_project_options(hashclash_tests)

include(Catch)
catch_discover_tests(hashclash_tests
  PROPERTIES
    TIMEOUT 60
)

add_test(NAME md5_fastcoll_help
  COMMAND $<TARGET_FILE:md5_fastcoll> --help
)
set_tests_properties(md5_fastcoll_help PROPERTIES TIMEOUT 5)

add_test(NAME md5_collision_failure
  COMMAND "${CMAKE_COMMAND}"
    "-DHELPER=$<TARGET_FILE:md5_diffpathhelper>"
    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/md5-collision-failure"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_md5_collision_failure.cmake"
)
set_tests_properties(md5_collision_failure PROPERTIES TIMEOUT 30)

add_test(NAME md5_partial_prefix
  COMMAND "${CMAKE_COMMAND}"
    "-DHELPER=$<TARGET_FILE:md5_diffpathhelper>"
    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/md5-partial-prefix"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_md5_partial_prefix.cmake"
)
set_tests_properties(md5_partial_prefix PROPERTIES TIMEOUT 15)

add_test(NAME sha1_filter_limit
  COMMAND "${CMAKE_COMMAND}"
    "-DHELPER=$<TARGET_FILE:sha1_diffpathhelper>"
    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/sha1-filter-limit"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_sha1_filter_limit.cmake"
)
set_tests_properties(sha1_filter_limit PROPERTIES TIMEOUT 15)

find_program(HASHCLASH_BASH_EXECUTABLE bash)
if(HASHCLASH_BASH_EXECUTABLE)
  add_test(NAME cpc_collision_search
    COMMAND "${HASHCLASH_BASH_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_cpc.sh"
  )
  set_tests_properties(cpc_collision_search PROPERTIES TIMEOUT 30)
endif()
