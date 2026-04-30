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
  model.marks = {
      {"Mark-Left", 100.0, 80.0, 56.0, 48.0, 0.0, 0.91, 0.82, 0.88, 16, "#ff4d4f",
       MarkShape::Diamond, MarkAlgorithm::ColorBrushTemplate, true},
      {"Mark-Right", 240.0, 82.0, 44.0, 44.0, 0.0, 0.89, 0.80, 0.86, 12, "#ffffff",
       MarkShape::Circle, MarkAlgorithm::BinaryGeometry, true},
  };
  model.rois = {
      {"Inspect-Top", 20.0, 20.0, 120.0, 60.0, 0.0, 0.78, RoiShape::Rectangle, true},
      {"Code-Area", 220.0, 40.0, 92.0, 92.0, 0.0, 0.83, RoiShape::Circle, true},
  };
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

std::optional<double> extractDoubleField(const std::string &content, const std::string &fieldName) {
  const std::regex pattern("\"" + fieldName + R"(\"\s*:\s*([-+]?\d*\.?\d+))");
  std::smatch match;
  if (!std::regex_search(content, match, pattern)) {
    return std::nullopt;
  }

  return std::stod(match[1].str());
}

std::optional<bool> extractBoolField(const std::string &content, const std::string &fieldName) {
  const std::regex pattern("\"" + fieldName + R"(\"\s*:\s*(true|false))");
  std::smatch match;
  if (!std::regex_search(content, match, pattern)) {
    return std::nullopt;
  }

  return match[1].str() == "true";
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

std::vector<std::string> extractObjects(const std::string &content) {
  std::vector<std::string> objects;
  const std::regex pattern(R"(\{[\s\S]*?\})");
  for (std::sregex_iterator iterator(content.begin(), content.end(), pattern), end; iterator != end;
       ++iterator) {
    objects.push_back(iterator->str());
  }

  return objects;
}

std::vector<MarkPoint> parseMarks(const std::string &content) {
  std::vector<MarkPoint> marks;
  const auto section = extractArraySection(content, "marks");
  if (!section.has_value()) {
    return marks;
  }

  for (const std::string &object : extractObjects(*section)) {
    MarkPoint mark;
    if (const auto value = extractStringField(object, "name"); value.has_value()) {
      mark.name = *value;
    }
    if (const auto value = extractDoubleField(object, "x"); value.has_value()) {
      mark.x = *value;
    }
    if (const auto value = extractDoubleField(object, "y"); value.has_value()) {
      mark.y = *value;
    }
    if (const auto value = extractDoubleField(object, "width"); value.has_value()) {
      mark.width = *value;
    }
    if (const auto value = extractDoubleField(object, "height"); value.has_value()) {
      mark.height = *value;
    }
    if (const auto value = extractDoubleField(object, "rotation"); value.has_value()) {
      mark.rotation = *value;
    }
    if (const auto value = extractDoubleField(object, "score"); value.has_value()) {
      mark.score = *value;
    }
    if (const auto value = extractDoubleField(object, "minimumScore"); value.has_value()) {
      mark.minimumScore = *value;
    }
    if (const auto value = extractDoubleField(object, "previewScore"); value.has_value()) {
      mark.previewScore = *value;
    }
    if (const auto value = extractDoubleField(object, "colorTolerance"); value.has_value()) {
      mark.colorTolerance = static_cast<int>(*value);
    }
    if (const auto value = extractStringField(object, "sampledColor"); value.has_value()) {
      mark.sampledColor = *value;
    }
    if (const auto value = extractStringField(object, "shape"); value.has_value()) {
      mark.shape = markShapeFromString(*value);
    }
    if (const auto value = extractStringField(object, "algorithm"); value.has_value()) {
      mark.algorithm = markAlgorithmFromString(*value);
    }
    if (const auto value = extractBoolField(object, "enabled"); value.has_value()) {
      mark.enabled = *value;
    }

    marks.push_back(mark);
  }

  return marks;
}

std::vector<RoiRegion> parseRois(const std::string &content) {
  std::vector<RoiRegion> rois;
  const auto section = extractArraySection(content, "rois");
  if (!section.has_value()) {
    return rois;
  }

  for (const std::string &object : extractObjects(*section)) {
    RoiRegion roi;
    if (const auto value = extractStringField(object, "name"); value.has_value()) {
      roi.name = *value;
    }
    if (const auto value = extractDoubleField(object, "x"); value.has_value()) {
      roi.x = *value;
    }
    if (const auto value = extractDoubleField(object, "y"); value.has_value()) {
      roi.y = *value;
    }
    if (const auto value = extractDoubleField(object, "width"); value.has_value()) {
      roi.width = *value;
    }
    if (const auto value = extractDoubleField(object, "height"); value.has_value()) {
      roi.height = *value;
    }
    if (const auto value = extractDoubleField(object, "rotation"); value.has_value()) {
      roi.rotation = *value;
    }
    if (const auto value = extractDoubleField(object, "threshold"); value.has_value()) {
      roi.threshold = *value;
    }
    if (const auto value = extractStringField(object, "shape"); value.has_value()) {
      roi.shape = roiShapeFromString(*value);
    }
    if (const auto value = extractBoolField(object, "enabled"); value.has_value()) {
      roi.enabled = *value;
    }

    rois.push_back(roi);
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
           << "      \"name\": \"" << escapeJson(mark.name) << "\",\n"
           << "      \"x\": " << mark.x << ",\n"
           << "      \"y\": " << mark.y << ",\n"
           << "      \"width\": " << mark.width << ",\n"
           << "      \"height\": " << mark.height << ",\n"
           << "      \"rotation\": " << mark.rotation << ",\n"
           << "      \"score\": " << mark.score << ",\n"
           << "      \"minimumScore\": " << mark.minimumScore << ",\n"
           << "      \"previewScore\": " << mark.previewScore << ",\n"
           << "      \"colorTolerance\": " << mark.colorTolerance << ",\n"
           << "      \"sampledColor\": \"" << escapeJson(mark.sampledColor) << "\",\n"
           << "      \"shape\": \"" << toString(mark.shape) << "\",\n"
           << "      \"algorithm\": \"" << toString(mark.algorithm) << "\",\n"
           << "      \"enabled\": " << (mark.enabled ? "true" : "false") << "\n"
           << "    }";
    output << (index + 1 < currentProgram_->marks.size() ? ",\n" : "\n");
  }

  output << "  ],\n"
         << "  \"rois\": [\n";

  for (std::size_t index = 0; index < currentProgram_->rois.size(); ++index) {
    const auto &roi = currentProgram_->rois[index];
    output << "    {\n"
           << "      \"name\": \"" << escapeJson(roi.name) << "\",\n"
           << "      \"x\": " << roi.x << ",\n"
           << "      \"y\": " << roi.y << ",\n"
           << "      \"width\": " << roi.width << ",\n"
           << "      \"height\": " << roi.height << ",\n"
           << "      \"rotation\": " << roi.rotation << ",\n"
           << "      \"threshold\": " << roi.threshold << ",\n"
           << "      \"shape\": \"" << toString(roi.shape) << "\",\n"
           << "      \"enabled\": " << (roi.enabled ? "true" : "false") << "\n"
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
