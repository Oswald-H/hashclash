# These valid bitcondition codes contradict each other: Q0 equals Q-1,
# while Q-1 must differ from Q0. Exercise the real worker failure path.
file(MAKE_DIRECTORY "${WORK}")
file(WRITE "${WORK}/path.txt"
  "Q-2: |........ ........ ........ ........|\n"
  "Q-1: |........ ........ ........ .......Y|\n"
  "Q0: |........ ........ ........ .......^|\n")
execute_process(
  COMMAND "${HELPER}" --pathfromtext --inputfile1 path.txt --outputfile1 path.bin.gz
  WORKING_DIRECTORY "${WORK}"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
  TIMEOUT 10
)
if(NOT "${result}" STREQUAL "0")
  message(FATAL_ERROR "Could not create path fixture (${result}): ${output}${error}")
endif()

foreach(threads 1 2)
  execute_process(
    COMMAND "${HELPER}" --findcollision --inputfile1 path.bin.gz --threads ${threads} -w .
    WORKING_DIRECTORY "${WORK}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
    TIMEOUT 10
  )
  if(NOT "${result}" STREQUAL "1" OR NOT "${error}" MATCHES "ambiguous path")
    message(FATAL_ERROR
      "Worker failure with ${threads} thread(s) must return 1 and report the cause; "
      "got '${result}': ${output}${error}")
  endif()
endforeach()

# A zero-difference path reaches the actual output code quickly. Fix Q1 to
# avoid the large free-word enumeration; no expensive collision is needed.
file(WRITE "${WORK}/path.txt" "")
foreach(step RANGE -3 64)
  if(step EQUAL 1)
    file(APPEND "${WORK}/path.txt" "Q${step}: |00000000 00000000 00000000 00000000|\n")
  else()
    file(APPEND "${WORK}/path.txt" "Q${step}: |........ ........ ........ ........|\n")
  endif()
endforeach()
execute_process(
  COMMAND "${HELPER}" --pathfromtext --inputfile1 path.txt --outputfile1 path.bin.gz
  WORKING_DIRECTORY "${WORK}" OUTPUT_QUIET COMMAND_ERROR_IS_FATAL ANY TIMEOUT 10
)
file(REMOVE_RECURSE "${WORK}/output")
file(MAKE_DIRECTORY "${WORK}/output")
foreach(destination output missing/output)
  execute_process(
    COMMAND "${HELPER}" --findcollision --inputfile1 path.bin.gz --threads 1 -w "${destination}"
    WORKING_DIRECTORY "${WORK}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
    TIMEOUT 10
  )
  if(destination STREQUAL "output")
    if(NOT "${result}" STREQUAL "0" OR NOT "${output}" MATCHES "Found collision!")
      message(FATAL_ERROR "Successful search returned '${result}': ${output}${error}")
    endif()
    file(GLOB blocks "${WORK}/output/coll1_*" "${WORK}/output/coll2_*")
    list(LENGTH blocks count)
    if(NOT count EQUAL 2)
      message(FATAL_ERROR "Successful search must save two collision blocks")
    endif()
    foreach(block IN LISTS blocks)
      file(SIZE "${block}" size)
      if(NOT size EQUAL 64)
        message(FATAL_ERROR "Collision block is not 64 bytes: ${block}")
      endif()
    endforeach()
  elseif(NOT "${result}" STREQUAL "1" OR NOT "${error}" MATCHES "Cannot open")
    message(FATAL_ERROR "Output failure must return 1; got '${result}': ${output}${error}")
  endif()
endforeach()
file(REMOVE_RECURSE "${WORK}")
