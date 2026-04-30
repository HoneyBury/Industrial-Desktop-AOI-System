if(NOT DEFINED CLANG_TIDY_EXE)
  message(FATAL_ERROR "CLANG_TIDY_EXE is required.")
endif()

if(NOT DEFINED BUILD_DIR)
  message(FATAL_ERROR "BUILD_DIR is required.")
endif()

if(NOT DEFINED TIDY_FILES)
  message(FATAL_ERROR "TIDY_FILES is required.")
endif()

string(REPLACE "|" ";" TIDY_FILES "${TIDY_FILES}")

execute_process(
  COMMAND xcrun --show-sdk-path
  OUTPUT_VARIABLE SDK_PATH
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET)

set(TIDY_COMMAND
  ${CLANG_TIDY_EXE}
  -p
  ${BUILD_DIR})

if(SDK_PATH)
  list(APPEND TIDY_COMMAND
    --extra-arg=-isysroot
    --extra-arg=${SDK_PATH})
endif()

list(APPEND TIDY_COMMAND ${TIDY_FILES})

execute_process(
  COMMAND ${TIDY_COMMAND}
  RESULT_VARIABLE TIDY_RESULT
  COMMAND_ECHO STDOUT)

if(NOT TIDY_RESULT EQUAL 0)
  # 初始化阶段允许 clang-tidy 报告问题，但不阻塞构建和 CI 主流程。
  message(WARNING "clang-tidy reported issues or environment-specific parsing errors; continuing.")
endif()
