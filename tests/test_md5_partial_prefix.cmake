file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(WRITE "${WORK}/prefix.bin" "abcde")

execute_process(
  COMMAND "${HELPER}" --startpartialpathfromfile --inputfile1 prefix.bin
    --outputfile1 preserved.bin --outputfile2 path.bin.gz
  WORKING_DIRECTORY "${WORK}"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
  TIMEOUT 10
)
if(NOT "${result}" STREQUAL "2" OR NOT "${error}" MATCHES "partial words after the first word")
  message(FATAL_ERROR "Unaligned prefix must fail clearly; got '${result}': ${output}${error}")
endif()
if(EXISTS "${WORK}/preserved.bin" OR EXISTS "${WORK}/path.bin.gz")
  message(FATAL_ERROR "Rejected prefix must not create output files")
endif()

file(WRITE "${WORK}/prefix.bin" "abcd")
execute_process(
  COMMAND "${HELPER}" --startpartialpathfromfile --inputfile1 prefix.bin
    --outputfile1 preserved.bin --outputfile2 path.bin.gz
  WORKING_DIRECTORY "${WORK}"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
  TIMEOUT 10
)
if(NOT "${result}" STREQUAL "0" OR NOT EXISTS "${WORK}/path.bin.gz")
  message(FATAL_ERROR "Word-aligned prefix should still work; got '${result}': ${output}${error}")
endif()

file(REMOVE_RECURSE "${WORK}")
