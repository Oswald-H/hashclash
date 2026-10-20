file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(WRITE "${WORK}/path.txt" "Q-4: |........ ........ ........ ........|\n")

execute_process(
  COMMAND "${HELPER}" --pathfromtext --inputfile1 path.txt --outputfile1 path.bin.bz2
  WORKING_DIRECTORY "${WORK}"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
  TIMEOUT 10
)
if(NOT "${result}" STREQUAL "0")
  message(FATAL_ERROR "Could not create path fixture (${result}): ${output}${error}")
endif()

execute_process(
  COMMAND "${HELPER}" --filterfeasible --filtert -3 --maxstates 1
    --inputfile1 path.bin.bz2 --outputfile1 filtered.bin.bz2
  WORKING_DIRECTORY "${WORK}"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
  TIMEOUT 10
)
if(NOT "${result}" STREQUAL "1" OR NOT "${error}" MATCHES "reached --maxstates=1")
  message(FATAL_ERROR "State limit must fail clearly; got '${result}': ${output}${error}")
endif()
if(EXISTS "${WORK}/filtered.bin.bz2")
  message(FATAL_ERROR "State limit must not save a partial result")
endif()

execute_process(
  COMMAND "${HELPER}" --filterfeasible --filtert -3 --maxstates 2
    --inputfile1 path.bin.bz2 --outputfile1 filtered.bin.bz2
  WORKING_DIRECTORY "${WORK}"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
  TIMEOUT 10
)
if(NOT "${result}" STREQUAL "0" OR NOT EXISTS "${WORK}/filtered.bin.bz2")
  message(FATAL_ERROR "Feasible path should pass within the limit; got '${result}': ${output}${error}")
endif()

execute_process(
  COMMAND "${HELPER}" --filterfeasible --filtert -3 --maxstates 0
    --inputfile1 path.bin.bz2 --outputfile1 unlimited.bin.bz2
  WORKING_DIRECTORY "${WORK}"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
  TIMEOUT 10
)
if(NOT "${result}" STREQUAL "0" OR NOT EXISTS "${WORK}/unlimited.bin.bz2")
  message(FATAL_ERROR "Explicit unlimited search should work; got '${result}': ${output}${error}")
endif()

execute_process(
  COMMAND "${HELPER}" --filterfeasible --filtert -5 --inputfile1 path.bin.bz2
  WORKING_DIRECTORY "${WORK}"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
  TIMEOUT 10
)
if(NOT "${result}" STREQUAL "2" OR NOT "${error}" MATCHES "--filtert")
  message(FATAL_ERROR "Invalid filtert must fail clearly; got '${result}': ${output}${error}")
endif()

file(REMOVE_RECURSE "${WORK}")
