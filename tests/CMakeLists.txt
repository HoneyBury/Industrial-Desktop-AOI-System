include(FetchContent)

set(AOI_GTEST_TARGET GTest::gtest_main)
set(AOI_USE_MINI_GTEST OFF)

find_package(GTest QUIET CONFIG)

if(GTest_FOUND)
  message(STATUS "Using installed GoogleTest package.")
elseif(AOI_FETCH_GTEST)
  message(STATUS "GoogleTest not found, attempting FetchContent bootstrap.")
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip)
  FetchContent_MakeAvailable(googletest)
else()
  # 允许在离线或未安装 GTest 的环境中先跑通基础测试。
  message(STATUS "GoogleTest not found. Using offline bootstrap-compatible mini test harness.")
  set(AOI_USE_MINI_GTEST ON)

  add_library(aoi_mini_gtest STATIC
    support/gtest_main.cpp)
  target_include_directories(aoi_mini_gtest PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/support)
  add_library(GTest::gtest ALIAS aoi_mini_gtest)
  add_library(GTest::gtest_main ALIAS aoi_mini_gtest)
endif()

add_executable(aoi_core_tests
  unit/test_board_scan.cpp
  unit/test_calibration_modules.cpp
  unit/test_coordinate_transformer.cpp
  unit/test_mark_alignment.cpp
  unit/test_usb_camera.cpp
  unit/test_process_engine.cpp
  unit/test_program_manager.cpp
  unit/test_virtual_motion_controller.cpp
  unit/test_virtual_camera_device.cpp
  unit/test_mark_offset.cpp
  unit/test_laser_and_steps.cpp
  integration/test_inspection_pipeline.cpp)

target_link_libraries(aoi_core_tests
  PRIVATE
    aoi_core
    ${AOI_GTEST_TARGET})

if(AOI_USE_MINI_GTEST)
  target_include_directories(aoi_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
endif()

add_test(NAME aoi_core_tests COMMAND aoi_core_tests)
