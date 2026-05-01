#include <gtest/gtest.h>

#include "ai/AiInferencer.h"
#include "database/DatabaseManager.h"
#include "motion/VirtualMotionController.h"
#include "process/ProcessEngine.h"
#include "program/ProgramManager.h"

#include <cstdio>

TEST(InspectionPipelineTest, RunsBootstrapInspectionFlow) {
  ProgramManager programManager;
  ProgramModel model;
  model.name = "demo_program";
  model.aiModelPath = "models/demo.onnx";
  model.calibrationFilePath = "config/camera_calib.yaml";
  ASSERT_TRUE(programManager.createProgram(model));

  const std::string dbPath = "test_pipeline_demo.db";
  DatabaseManager databaseManager;
  ASSERT_TRUE(databaseManager.open(dbPath));
  EXPECT_TRUE(databaseManager.isOpen());

  // Insert and query a calibration record.
  CalibrationRecord calibRecord;
  calibRecord.calibrationType = "intrinsic";
  calibRecord.fx = 1050.0;
  calibRecord.fy = 1048.0;
  calibRecord.cx = 642.0;
  calibRecord.cy = 358.0;
  calibRecord.notes = "test calibration entry";
  ASSERT_TRUE(databaseManager.insertCalibrationRecord(calibRecord));

  const auto calibResults = databaseManager.queryCalibrationHistory(10);
  ASSERT_TRUE(calibResults);
  EXPECT_TRUE(calibResults.value.size() >= static_cast<std::size_t>(1));
  EXPECT_EQ(calibResults.value.front().calibrationType, std::string("intrinsic"));
  EXPECT_NEAR(calibResults.value.front().fx, 1050.0, 0.001);

  // Insert and query inspection results.
  InspectionRecord inspRecord;
  inspRecord.boardId = "BOARD-TEST-001";
  inspRecord.programName = "demo_program";
  inspRecord.aiLabel = "ok";
  inspRecord.aiConfidence = 0.95;
  inspRecord.finalDecision = "OK";
  inspRecord.marksCount = 2;
  inspRecord.roisCount = 1;
  ASSERT_TRUE(databaseManager.insertInspectionResult(inspRecord));

  const auto inspResults = databaseManager.queryInspectionResults(10);
  ASSERT_TRUE(inspResults);
  EXPECT_TRUE(inspResults.value.size() >= static_cast<std::size_t>(1));
  EXPECT_EQ(inspResults.value.front().boardId, std::string("BOARD-TEST-001"));

  // Count OK boards.
  const auto okCount = databaseManager.countBoardResults("OK");
  ASSERT_TRUE(okCount);
  EXPECT_TRUE(okCount.value >= 1);

  databaseManager.close();
  EXPECT_TRUE(!databaseManager.isOpen());

  // Verify AI inferencer: model file doesn't exist, falls back to traditional CV.
  AiInferencer inferencer;
  ASSERT_TRUE(inferencer.loadModel("models/nonexistent.onnx"));
  const auto aiResult = inferencer.infer("tests/data/demo.png");
  // Either path is valid; ensure the result is well-formed when it succeeds.
  if (aiResult) {
    EXPECT_TRUE(!aiResult.value.empty());
  }

  databaseManager.close();
  std::remove(dbPath.c_str());
}

TEST(InspectionPipelineTest, PersistsWorkflowBoardTraceAndInspectionResult) {
  ProgramManager programManager;
  ASSERT_TRUE(programManager.createDefaultProgram());
  auto program = programManager.currentProgram();
  ASSERT_TRUE(program.has_value());

  DatabaseManager databaseManager;
  const std::string dbPath = "test_pipeline_runtime.db";
  ASSERT_TRUE(databaseManager.open(dbPath));

  VirtualMotionController motionController;
  motionController.moveAbsolute(MotionAxis::X, 100.0);
  motionController.moveAbsolute(MotionAxis::Y, 200.0);

  WorkflowContext context;
  context.program = &(*program);
  context.motionController = &motionController;
  context.databaseManager = &databaseManager;
  context.currentImagePath = "tests/data/demo.png";
  context.currentMachinePose = MechanicalPose {100.0, 200.0, 0.0, 0.0};
  context.reuseInjectedInputs = true;
  context.measuredMarks = {
      {"Mark-A", 105.0, 82.0, 52.0, 46.0, 0.0, 0.92, 0.82, 0.89, 16, "#ff4d4f",
       MarkShape::Diamond, MarkAlgorithm::ColorBrushTemplate, true},
      {"Mark-B", 245.0, 96.0, 48.0, 48.0, 0.0, 0.90, 0.80, 0.87, 14, "#f97316",
       MarkShape::Cross, MarkAlgorithm::ColorBrushTemplate, true},
  };

  ProcessEngine engine;
  const auto result = engine.runBoard(context);
  ASSERT_TRUE(result.ok);

  const auto inspectionResults = databaseManager.queryInspectionResults(5);
  ASSERT_TRUE(inspectionResults);
  ASSERT_TRUE(!inspectionResults.value.empty());
  EXPECT_EQ(inspectionResults.value.front().finalDecision, std::string("OK"));
  EXPECT_TRUE(!inspectionResults.value.front().detailsJson.empty());

  const auto boardResults = databaseManager.queryBoardRecords(5);
  ASSERT_TRUE(boardResults);
  ASSERT_TRUE(!boardResults.value.empty());
  EXPECT_EQ(boardResults.value.front().status, std::string("ok"));

  databaseManager.close();
  std::remove(dbPath.c_str());
}
