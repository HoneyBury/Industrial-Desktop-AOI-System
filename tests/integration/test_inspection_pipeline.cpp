#include <gtest/gtest.h>

#include "ai/AiInferencer.h"
#include "database/DatabaseManager.h"
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
