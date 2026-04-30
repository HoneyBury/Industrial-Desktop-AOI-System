#include <gtest/gtest.h>

#include "ai/AiInferencer.h"
#include "database/DatabaseManager.h"
#include "program/ProgramManager.h"
#include "vision/CodeReader.h"
#include "vision/MarkDetector.h"
#include "vision/RoiDetector.h"

TEST(InspectionPipelineTest, RunsBootstrapInspectionFlow) {
  ProgramManager programManager;
  ProgramModel model;
  model.name = "demo_program";
  model.aiModelPath = "models/demo.onnx";
  model.calibrationFilePath = "config/camera_calib.yaml";
  ASSERT_TRUE(programManager.createProgram(model));

  DatabaseManager databaseManager;
  ASSERT_TRUE(databaseManager.open("data/demo.db"));
  EXPECT_TRUE(databaseManager.isOpen());

  MarkDetector markDetector;
  const auto markResult = markDetector.detectTemplateMarks("tests/data/demo.png");
  ASSERT_TRUE(markResult);
  EXPECT_EQ(markResult.value.size(), static_cast<std::size_t>(2));

  RoiDetector roiDetector;
  const auto roiResult = roiDetector.detectByThreshold("tests/data/demo.png");
  ASSERT_TRUE(roiResult);
  EXPECT_EQ(roiResult.value.size(), static_cast<std::size_t>(1));

  CodeReader codeReader;
  const auto codeResult = codeReader.readQrCode("tests/data/demo.png");
  ASSERT_TRUE(codeResult);
  EXPECT_EQ(codeResult.value, std::string("DEMO-CODE-001"));

  AiInferencer inferencer;
  ASSERT_TRUE(inferencer.loadModel("models/demo.onnx"));
  const auto aiResult = inferencer.infer("tests/data/demo.png");
  ASSERT_TRUE(aiResult);
  EXPECT_EQ(aiResult.value.front().label, std::string("ok"));
}

