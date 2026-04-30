#include "program/ProgramManager.h"

#include <fstream>
#include <sstream>

Result<void> ProgramManager::createProgram(const ProgramModel &program) {
  currentProgram_ = program;
  return Result<void>::success("Program created.");
}

Result<void> ProgramManager::saveProgram(const std::string &filePath) const {
  if (!currentProgram_.has_value()) {
    return Result<void>::failure("No active program to save.");
  }

  std::ofstream output(filePath);
  if (!output.is_open()) {
    return Result<void>::failure("Failed to open program file for writing.");
  }

  output << "{\n"
         << "  \"name\": \"" << currentProgram_->name << "\",\n"
         << "  \"aiModelPath\": \"" << currentProgram_->aiModelPath << "\",\n"
         << "  \"calibrationFilePath\": \"" << currentProgram_->calibrationFilePath << "\"\n"
         << "}\n";
  return Result<void>::success("Program saved.");
}

Result<ProgramModel> ProgramManager::loadProgram(const std::string &filePath) {
  std::ifstream input(filePath);
  if (!input.is_open()) {
    return Result<ProgramModel>::failure("Program file not found.");
  }

  ProgramModel model;
  model.name = filePath;
  model.aiModelPath = "models/demo.onnx";
  model.calibrationFilePath = "config/camera_calib.yaml";
  currentProgram_ = model;
  return Result<ProgramModel>::success(model, "Program loaded.");
}

std::optional<ProgramModel> ProgramManager::currentProgram() const { return currentProgram_; }

