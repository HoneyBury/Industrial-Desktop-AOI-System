#pragma once

#include "common/Result.h"
#include "program/ProgramModel.h"

#include <optional>
#include <string>

class ProgramManager {
public:
  Result<void> createDefaultProgram();
  Result<void> createProgram(const ProgramModel &program);
  Result<void> saveProgram(const std::string &filePath) const;
  Result<ProgramModel> loadProgram(const std::string &filePath);
  std::optional<ProgramModel> currentProgram() const;

private:
  std::optional<ProgramModel> currentProgram_;
};
