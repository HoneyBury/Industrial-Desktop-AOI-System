#include <gtest/gtest.h>

#include "program/ProgramManager.h"

#include <filesystem>

TEST(ProgramManagerTest, CreatesDefaultProgramTemplate) {
  ProgramManager manager;

  ASSERT_TRUE(manager.createDefaultProgram());
  ASSERT_TRUE(manager.currentProgram().has_value());
  EXPECT_EQ(manager.currentProgram()->name, std::string("default_demo_program"));
  EXPECT_EQ(manager.currentProgram()->marks.size(), static_cast<std::size_t>(2));
  EXPECT_EQ(manager.currentProgram()->rois.size(), static_cast<std::size_t>(1));
}

TEST(ProgramManagerTest, SavesAndLoadsProgramRoundTrip) {
  ProgramManager manager;
  ASSERT_TRUE(manager.createDefaultProgram());

  const std::filesystem::path outputPath =
      std::filesystem::current_path() / "program_manager_roundtrip.json";
  ASSERT_TRUE(manager.saveProgram(outputPath.string()));

  ProgramManager loader;
  const auto loadResult = loader.loadProgram(outputPath.string());
  ASSERT_TRUE(loadResult);
  EXPECT_EQ(loadResult.value.name, std::string("default_demo_program"));
  EXPECT_EQ(loadResult.value.aiModelPath, std::string("models/demo.onnx"));
  EXPECT_EQ(loadResult.value.marks.size(), static_cast<std::size_t>(2));
  EXPECT_EQ(loadResult.value.rois.size(), static_cast<std::size_t>(1));
  EXPECT_EQ(loadResult.value.filePath, outputPath.string());

  std::filesystem::remove(outputPath);
}
