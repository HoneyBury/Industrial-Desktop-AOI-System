#include "program/ProgramManager.h"

#include <fstream>
#include <optional>
#include <regex>
#include <sstream>

namespace {

ProgramModel buildDefaultProgram() {
  ProgramModel model;
  model.name = "default_demo_program";
  model.aiModelPath = "models/demo.onnx";
  model.calibrationFilePath = "config/camera_calib.yaml";
  model.codeRegionName = "qr_region_01";
  model.marks = {{100.0, 80.0, 1.0}, {240.0, 82.0, 1.0}};
  model.rois = {{20.0, 20.0, 120.0, 60.0}};
  model.calibrationData.fx = 1000.0;
  model.calibrationData.fy = 1000.0;
  model.calibrationData.cx = 640.0;
  model.calibrationData.cy = 360.0;
  model.calibrationData.pixelToMillimeterX = 0.01;
  model.calibrationData.pixelToMillimeterY = 0.01;
  return model;
}

std::string escapeJson(const std::string &input) {
  std::string output;
  output.reserve(input.size());

  for (const char character : input) {
    switch (character) {
    case '\\':
      output += "\\\\";
      break;
    case '"':
      output += "\\\"";
      break;
    case '\n':
      output += "\\n";
      break;
    default:
      output += character;
      break;
    }
  }

  return output;
}

std::optional<std::string> extractStringField(const std::string &content,
                                              const std::string &fieldName) {
  const std::regex pattern("\"" + fieldName + R"(\"\s*:\s*\"([^\"]*)\")");
  std::smatch match;
  if (!std::regex_search(content, match, pattern)) {
    return std::nullopt;
  }

  return match[1].str();
}

std::optional<std::string> extractArraySection(const std::string &content,
                                               const std::string &fieldName) {
  const std::regex pattern("\"" + fieldName + R"(\"\s*:\s*\[([\s\S]*?)\])");
  std::smatch match;
  if (!std::regex_search(content, match, pattern)) {
    return std::nullopt;
  }

  return match[1].str();
}

std::vector<MarkPoint> parseMarks(const std::string &content) {
  std::vector<MarkPoint> marks;
  const auto section = extractArraySection(content, "marks");
  if (!section.has_value()) {
    return marks;
  }

  const std::regex pattern(
      R"(\{\s*"x"\s*:\s*([-+]?\d*\.?\d+)\s*,\s*"y"\s*:\s*([-+]?\d*\.?\d+)\s*,\s*"score"\s*:\s*([-+]?\d*\.?\d+)\s*\})");

  for (std::sregex_iterator iterator(section->begin(), section->end(), pattern), end; iterator != end;
       ++iterator) {
    const std::smatch match = *iterator;
    marks.push_back(
        {std::stod(match[1].str()), std::stod(match[2].str()), std::stod(match[3].str())});
  }

  return marks;
}

std::vector<RoiRegion> parseRois(const std::string &content) {
  std::vector<RoiRegion> rois;
  const auto section = extractArraySection(content, "rois");
  if (!section.has_value()) {
    return rois;
  }

  const std::regex pattern(
      R"(\{\s*"x"\s*:\s*([-+]?\d*\.?\d+)\s*,\s*"y"\s*:\s*([-+]?\d*\.?\d+)\s*,\s*"width"\s*:\s*([-+]?\d*\.?\d+)\s*,\s*"height"\s*:\s*([-+]?\d*\.?\d+)\s*\})");

  for (std::sregex_iterator iterator(section->begin(), section->end(), pattern), end; iterator != end;
       ++iterator) {
    const std::smatch match = *iterator;
    rois.push_back({std::stod(match[1].str()), std::stod(match[2].str()),
                    std::stod(match[3].str()), std::stod(match[4].str())});
  }

  return rois;
}

} // namespace

Result<void> ProgramManager::createDefaultProgram() {
  currentProgram_ = buildDefaultProgram();
  return Result<void>::success("Default AOI program created.");
}

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
         << "  \"name\": \"" << escapeJson(currentProgram_->name) << "\",\n"
         << "  \"aiModelPath\": \"" << escapeJson(currentProgram_->aiModelPath) << "\",\n"
         << "  \"calibrationFilePath\": \"" << escapeJson(currentProgram_->calibrationFilePath)
         << "\",\n"
         << "  \"codeRegionName\": \"" << escapeJson(currentProgram_->codeRegionName) << "\",\n"
         << "  \"marks\": [\n";

  for (std::size_t index = 0; index < currentProgram_->marks.size(); ++index) {
    const auto &mark = currentProgram_->marks[index];
    output << "    {\n"
           << "      \"x\": " << mark.x << ",\n"
           << "      \"y\": " << mark.y << ",\n"
           << "      \"score\": " << mark.score << "\n"
           << "    }";
    output << (index + 1 < currentProgram_->marks.size() ? ",\n" : "\n");
  }

  output << "  ],\n"
         << "  \"rois\": [\n";

  for (std::size_t index = 0; index < currentProgram_->rois.size(); ++index) {
    const auto &roi = currentProgram_->rois[index];
    output << "    {\n"
           << "      \"x\": " << roi.x << ",\n"
           << "      \"y\": " << roi.y << ",\n"
           << "      \"width\": " << roi.width << ",\n"
           << "      \"height\": " << roi.height << "\n"
           << "    }";
    output << (index + 1 < currentProgram_->rois.size() ? ",\n" : "\n");
  }

  output << "  ]\n"
         << "}\n";
  return Result<void>::success("Program saved.");
}

Result<ProgramModel> ProgramManager::loadProgram(const std::string &filePath) {
  std::ifstream input(filePath);
  if (!input.is_open()) {
    return Result<ProgramModel>::failure("Program file not found.");
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();
  const std::string content = buffer.str();

  ProgramModel model = buildDefaultProgram();
  model.filePath = filePath;

  if (const auto name = extractStringField(content, "name"); name.has_value()) {
    model.name = *name;
  }

  if (const auto aiModelPath = extractStringField(content, "aiModelPath"); aiModelPath.has_value()) {
    model.aiModelPath = *aiModelPath;
  }

  if (const auto calibrationFilePath = extractStringField(content, "calibrationFilePath");
      calibrationFilePath.has_value()) {
    model.calibrationFilePath = *calibrationFilePath;
  }

  if (const auto codeRegionName = extractStringField(content, "codeRegionName");
      codeRegionName.has_value()) {
    model.codeRegionName = *codeRegionName;
  }

  model.marks = parseMarks(content);
  model.rois = parseRois(content);
  currentProgram_ = model;
  return Result<ProgramModel>::success(model, "Program loaded.");
}

std::optional<ProgramModel> ProgramManager::currentProgram() const { return currentProgram_; }
