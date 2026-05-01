cmake_minimum_required(VERSION 3.24)

project(
  IndustrialDesktopAOISystem
  VERSION 0.1.0
  DESCRIPTION "Desktop industrial AOI vision inspection and virtual motion calibration demo"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(AOI_BUILD_TESTS "Build unit and integration tests." ON)
option(AOI_ENABLE_GUI "Build Qt desktop UI when Qt 6 is available." ON)
option(AOI_ENABLE_OPENCV "Enable OpenCV-backed camera and vision helpers when OpenCV is available." ON)
option(AOI_ENABLE_SQLITE "Enable SQLite-backed database integration when SQLite3 is available." ON)
option(AOI_FETCH_GTEST "Fetch GoogleTest when it is not installed." OFF)
set(AOI_THIRD_PARTY_OPENCV_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/opencv"
    CACHE PATH "Project-local OpenCV root directory.")

include(CTest)
include(GNUInstallDirs)

find_package(Threads REQUIRED)
find_package(Python3 QUIET COMPONENTS Interpreter)

set(AOI_HAS_QT OFF)
set(AOI_HAS_OPENCV OFF)
set(AOI_HAS_SQLITE OFF)

if(AOI_ENABLE_GUI)
  find_package(Qt6 QUIET COMPONENTS Widgets Sql Charts Concurrent Network Test)
  if(Qt6_FOUND)
    set(AOI_HAS_QT ON)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)
  else()
    # 本地没有 Qt 时仍保留控制台入口，方便先推进核心库、测试和文档。
    message(STATUS "Qt6 Widgets not found. Building console bootstrap target only.")
  endif()
endif()

if(AOI_ENABLE_OPENCV)
  include(cmake/ResolveOpenCV.cmake)
  aoi_resolve_opencv("${AOI_THIRD_PARTY_OPENCV_ROOT}")
  find_package(OpenCV QUIET COMPONENTS core imgproc videoio objdetect dnn)
  if(OpenCV_FOUND)
    set(AOI_HAS_OPENCV ON)
    message(STATUS "OpenCV enabled from: ${OpenCV_DIR}")
  else()
    # 当前项目允许在无 OpenCV 环境下先以占位实现通过配置和编译。
    message(STATUS "OpenCV not found. Camera and CV-backed code will use stub implementations.")
  endif()
endif()

if(AOI_ENABLE_SQLITE)
  find_package(SQLite3 QUIET)
  if(SQLite3_FOUND)
    set(AOI_HAS_SQLITE ON)
  else()
    # 数据库层先保留接口和最小行为，避免依赖缺失阻塞仓库初始化。
    message(STATUS "SQLite3 not found. Database manager will run in stub mode.")
  endif()
endif()

add_library(aoi_core
  src/common/Logger.cpp
  src/config/AppSettings.cpp
  src/boardscan/BoardViewTransform.cpp
  src/boardscan/BoardScanPlanner.cpp
  src/boardscan/BoardScanExecutor.cpp
  src/boardscan/BoardStitcher.cpp
  src/camera/UsbCamera.cpp
  src/calibration/CameraIntrinsicCalibrator.cpp
  src/calibration/LaserOffsetCalibrator.cpp
  src/calibration/MarkReferenceCalibrator.cpp
  src/calibration/OriginCalibrator.cpp
  src/calibration/PixelScaleCalibrator.cpp
  src/coordinate/CoordinateTransformer.cpp
  src/alignment/MarkAlignmentSolver.cpp
  src/alignment/MarkDetector.cpp
  src/process/IProcessStep.cpp
  src/process/BoardWorkflow.cpp
  src/process/ProcessEngine.cpp
  src/process/ImageCaptureStep.cpp
  src/process/LoadBoardStep.cpp
  src/process/MarkAlignStep.cpp
  src/process/RoughPositionStep.cpp
  src/process/DefectInspectStep.cpp
  src/process/PreLaserStep.cpp
  src/process/LaserExecuteStep.cpp
  src/process/PostLaserVerifyStep.cpp
  src/vision/CameraCalibrator.cpp
  src/vision/MarkDetector.cpp
  src/vision/RoiDetector.cpp
  src/vision/CodeReader.cpp
  src/laser/VirtualLaserController.cpp
  src/motion/VirtualMotionController.cpp
  src/transport/VirtualTransportController.cpp
  src/program/ProgramManager.cpp
  src/database/DatabaseManager.cpp
  src/ai/AiInferencer.cpp)

target_include_directories(aoi_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(aoi_core
  PUBLIC
    Threads::Threads)

target_compile_definitions(aoi_core
  PUBLIC
    $<$<BOOL:${AOI_HAS_QT}>:AOI_HAS_QT_WIDGETS>
    $<$<BOOL:${AOI_HAS_OPENCV}>:AOI_HAS_OPENCV>
    $<$<BOOL:${AOI_HAS_SQLITE}>:AOI_HAS_SQLITE>)

if(AOI_HAS_OPENCV)
  target_link_libraries(aoi_core PUBLIC ${OpenCV_LIBS})
  target_include_directories(aoi_core PUBLIC ${OpenCV_INCLUDE_DIRS})
endif()

if(AOI_HAS_SQLITE)
  target_link_libraries(aoi_core PUBLIC SQLite::SQLite3)
endif()

set(AOI_APP_SOURCES
  app/main.cpp)

if(AOI_HAS_QT)
  list(APPEND AOI_APP_SOURCES
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.ui
    src/ui/RunModeWidget.cpp
    src/ui/RunModeWidget.h
    src/ui/CadGraphicsView.cpp
    src/ui/CadGraphicsView.h
    src/ui/CalibrationCaptureWidget.cpp
    src/ui/CalibrationCaptureWidget.h
    src/ui/LaserOffsetCalibDialog.cpp
    src/ui/LaserOffsetCalibDialog.h
    src/ui/CadRulerWidget.cpp
    src/ui/CadRulerWidget.h
    src/ui/CameraCalibDialog.cpp
    src/ui/CameraCalibDialog.h
    src/ui/LogWindow.cpp
    src/ui/LogWindow.h
    src/ui/SettingsDialog.cpp
    src/ui/SettingsDialog.h
    src/ui/ProgramEditDialog.cpp
    src/ui/ProgramEditDialog.h
    src/ui/NewProgramDialog.cpp
    src/ui/NewProgramDialog.h
    src/ui/MarkEditDialog.cpp
    src/ui/MarkEditDialog.h
    src/ui/MotionControlDialog.cpp
    src/ui/MotionControlDialog.h
    src/ui/OriginCalibDialog.cpp
    src/ui/OriginCalibDialog.h
    src/ui/MarkOffsetDialog.cpp
    src/ui/MarkOffsetDialog.h
    src/ui/DataCollectDialog.cpp
    src/ui/DataCollectDialog.h
    src/ui/ProductionHistoryDialog.cpp
    src/ui/ProductionHistoryDialog.h)
endif()

if(AOI_HAS_QT)
  # Disable the standalone host-writer example; we only need the support library.
  set(LASERSPC_BUILD_HOST_WRITER_EXAMPLE OFF CACHE BOOL "" FORCE)
  add_subdirectory(LaserSpc)

  list(APPEND AOI_APP_SOURCES
    src/spc/LaserSpcBridge.cpp
    src/spc/LaserSpcBridge.h
    src/spc/LaserSpcWindow.cpp
    src/spc/LaserSpcWindow.h)
endif()

add_executable(industrial_desktop_aoi ${AOI_APP_SOURCES})
target_link_libraries(industrial_desktop_aoi PRIVATE aoi_core)

if(AOI_HAS_QT)
  target_link_libraries(industrial_desktop_aoi PRIVATE
    Qt6::Widgets
    LaserSpcUi
    LaserSpcHostWriterSupport)
endif()

install(TARGETS industrial_desktop_aoi
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(AOI_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
  set(AOI_TIDY_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/Logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/camera/UsbCamera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vision/CoordinateTransformer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/motion/VirtualMotionController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/program/ProgramManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/database/DatabaseManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ai/AiInferencer.cpp)
  list(JOIN AOI_TIDY_FILES "|" AOI_TIDY_FILE_LIST)

  add_custom_target(aoi_clang_tidy
    COMMAND ${CMAKE_COMMAND}
      -DCLANG_TIDY_EXE=${CLANG_TIDY_EXE}
      -DBUILD_DIR=${CMAKE_BINARY_DIR}
      -DTIDY_FILES=${AOI_TIDY_FILE_LIST}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunClangTidy.cmake
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running clang-tidy in non-blocking bootstrap mode")
else()
  add_custom_target(aoi_clang_tidy
    COMMAND ${CMAKE_COMMAND} -E echo "clang-tidy not found; skipping local analysis target."
    COMMENT "clang-tidy is unavailable")
endif()
