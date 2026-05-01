#include "program/ProgramManager.h"

#include <fstream>
#include <optional>
#include <regex>
#include <sstream>
#include <string_view>

namespace {

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

std::vector<double> parseDoubleArray(const std::string &content, const std::string &fieldName) {
  std::vector<double> values;
  const auto section = extractArraySection(content, fieldName);
  if (!section.has_value()) {
    return values;
  }

  const std::regex pattern(R"([-+]?\d*\.?\d+)");
  for (std::sregex_iterator iterator(section->begin(), section->end(), pattern), end; iterator != end;
       ++iterator) {
    values.push_back(std::stod(iterator->str()));
  }

  return values;
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

std::vector<MarkReferenceRecord> parseMarkReferences(const std::string &content) {
  std::vector<MarkReferenceRecord> references;
  const auto section = extractArraySection(content, "markReferences");
  if (!section.has_value()) {
    return references;
  }

  for (const std::string &object : extractObjects(*section)) {
    MarkReferenceRecord reference;
    if (const auto value = extractStringField(object, "name"); value.has_value()) {
      reference.name = *value;
    }
    if (const auto value = extractDoubleField(object, "referencePixelX"); value.has_value()) {
      reference.referencePixel.x = *value;
    }
    if (const auto value = extractDoubleField(object, "referencePixelY"); value.has_value()) {
      reference.referencePixel.y = *value;
    }
    if (const auto value = extractDoubleField(object, "referenceProductX"); value.has_value()) {
      reference.referenceProductPoint.x = *value;
    }
    if (const auto value = extractDoubleField(object, "referenceProductY"); value.has_value()) {
      reference.referenceProductPoint.y = *value;
    }
    if (const auto value = extractBoolField(object, "enabled"); value.has_value()) {
      reference.enabled = *value;
    }
    references.push_back(reference);
  }

  return references;
}

std::vector<RoiDetectorConfig> parseRoiDetectorConfigs(const std::string &content) {
  std::vector<RoiDetectorConfig> configs;
  const auto section = extractArraySection(content, "roiDetectorConfigs");
  if (!section.has_value()) {
    return configs;
  }

  for (const std::string &object : extractObjects(*section)) {
    RoiDetectorConfig config;
    if (const auto value = extractStringField(object, "roiName"); value.has_value()) {
      config.roiName = *value;
    }
    if (const auto value = extractStringField(object, "detectorType"); value.has_value()) {
      config.detectorType = roiDetectorTypeFromString(*value);
    }
    if (const auto value = extractDoubleField(object, "threshold"); value.has_value()) {
      config.threshold = *value;
    }
    if (const auto value = extractStringField(object, "templateImagePath"); value.has_value()) {
      config.templateImagePath = *value;
    }
    if (const auto value = extractStringField(object, "aiModelPath"); value.has_value()) {
      config.aiModelPath = *value;
    }
    if (const auto value = extractStringField(object, "parameterSummary"); value.has_value()) {
      config.parameterSummary = *value;
    }
    if (const auto value = extractBoolField(object, "enabled"); value.has_value()) {
      config.enabled = *value;
    }
    configs.push_back(config);
  }

  return configs;
}

void syncRuntimeCompatibilityFields(ProgramModel &model) {
  model.templateCachePath = model.runtimeSummary.templateCachePath;
  model.latestTemplateMatchSummary = model.runtimeSummary.latestTemplateMatchSummary;
  model.hasMarkCalibration = model.runtimeSummary.hasMarkCalibration;
  model.markCalibrationOffsetXmm = model.runtimeSummary.markCalibrationOffsetXmm;
  model.markCalibrationOffsetYmm = model.runtimeSummary.markCalibrationOffsetYmm;
  model.markCalibrationRotationDegrees = model.runtimeSummary.markCalibrationRotationDegrees;
  model.hasOriginCalibration = model.runtimeSummary.hasOriginCalibration;
  model.originCorrectedX = model.runtimeSummary.originCorrectedPose.x;
  model.originCorrectedY = model.runtimeSummary.originCorrectedPose.y;
  model.originCorrectedZ = model.runtimeSummary.originCorrectedPose.z;
  model.originCorrectedR = model.runtimeSummary.originCorrectedPose.r;
}

void syncCanonicalRuntimeSummary(ProgramModel &model) {
  if (model.runtimeSummary.templateCachePath.empty()) {
    model.runtimeSummary.templateCachePath = model.templateCachePath;
  }
  if (model.runtimeSummary.latestTemplateMatchSummary.empty()) {
    model.runtimeSummary.latestTemplateMatchSummary = model.latestTemplateMatchSummary;
  }
  if (model.hasMarkCalibration) {
    model.runtimeSummary.hasMarkCalibration = true;
    model.runtimeSummary.markCalibrationOffsetXmm = model.markCalibrationOffsetXmm;
    model.runtimeSummary.markCalibrationOffsetYmm = model.markCalibrationOffsetYmm;
    model.runtimeSummary.markCalibrationRotationDegrees = model.markCalibrationRotationDegrees;
  }
  if (model.hasOriginCalibration) {
    model.runtimeSummary.hasOriginCalibration = true;
    model.runtimeSummary.originCorrectedPose = MechanicalPose {
        model.originCorrectedX, model.originCorrectedY, model.originCorrectedZ, model.originCorrectedR};
  }
}

void syncCanonicalCalibration(ProgramModel &model) {
  if (!model.cameraIntrinsicCalibration.calibrated &&
      (model.calibrationData.fx != 0.0 || model.calibrationData.fy != 0.0)) {
    model.cameraIntrinsicCalibration.calibrated = true;
    model.cameraIntrinsicCalibration.fx = model.calibrationData.fx;
    model.cameraIntrinsicCalibration.fy = model.calibrationData.fy;
    model.cameraIntrinsicCalibration.cx = model.calibrationData.cx;
    model.cameraIntrinsicCalibration.cy = model.calibrationData.cy;
  }

  if (!model.pixelScaleCalibration.calibrated) {
    model.pixelScaleCalibration.calibrated = true;
    model.pixelScaleCalibration.pixelToMillimeterX = model.calibrationData.pixelToMillimeterX;
    model.pixelScaleCalibration.pixelToMillimeterY = model.calibrationData.pixelToMillimeterY;
  }
}

void syncCompatibilityCalibration(ProgramModel &model) {
  model.calibrationData.fx = model.cameraIntrinsicCalibration.fx;
  model.calibrationData.fy = model.cameraIntrinsicCalibration.fy;
  model.calibrationData.cx = model.cameraIntrinsicCalibration.cx;
  model.calibrationData.cy = model.cameraIntrinsicCalibration.cy;
  model.calibrationData.pixelToMillimeterX = model.pixelScaleCalibration.pixelToMillimeterX;
  model.calibrationData.pixelToMillimeterY = model.pixelScaleCalibration.pixelToMillimeterY;
}

void deriveMarkReferencesFromMarks(ProgramModel &model) {
  if (!model.markReferences.empty()) {
    return;
  }

  const std::size_t markCount = std::min<std::size_t>(2, model.marks.size());
  for (std::size_t index = 0; index < markCount; ++index) {
    const auto &mark = model.marks[index];
    model.markReferences.push_back(MarkReferenceRecord {
        mark.name,
        PixelPoint {mark.x, mark.y},
        MillimeterPoint {mark.x * model.pixelScaleCalibration.pixelToMillimeterX,
                         mark.y * model.pixelScaleCalibration.pixelToMillimeterY},
        mark.enabled,
    });
  }
}

void deriveRoiDetectorConfigsFromRois(ProgramModel &model) {
  if (!model.roiDetectorConfigs.empty()) {
    return;
  }

  for (const auto &roi : model.rois) {
    RoiDetectorConfig config;
    config.roiName = roi.name;
    config.threshold = roi.threshold;
    config.enabled = roi.enabled;
    config.detectorType = roi.name == model.codeRegionName ? RoiDetectorType::Code : RoiDetectorType::Geometry;
    config.parameterSummary =
        config.detectorType == RoiDetectorType::Code ? "QR/DM read region" : "Geometry threshold inspect";
    model.roiDetectorConfigs.push_back(config);
  }
}

void syncProgramModel(ProgramModel &model) {
  syncCanonicalCalibration(model);
  syncCanonicalRuntimeSummary(model);
  deriveMarkReferencesFromMarks(model);
  deriveRoiDetectorConfigsFromRois(model);
  syncCompatibilityCalibration(model);
  syncRuntimeCompatibilityFields(model);
}

ProgramModel buildDefaultProgram() {
  ProgramModel model;
  model.name = "default_demo_program";
  model.aiModelPath = "models/demo.onnx";
  model.calibrationFilePath = "config/camera_calib.yaml";
  model.codeRegionName = "Code-Area";

  model.cameraIntrinsicCalibration = CameraIntrinsicCalibration {
      true, 1000.0, 1000.0, 640.0, 360.0, {0.0, 0.0, 0.0, 0.0}};
  model.pixelScaleCalibration = PixelScaleCalibration {true, 0.01, 0.01};
  model.originCalibration = OriginCalibration {false, PixelPoint {0.0, 0.0}, MechanicalPose {0.0, 0.0, 0.0, 0.0}};
  model.laserOffsetCalibration = LaserOffsetCalibration {false, 0.0, 0.0};

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

  model.runtimeSummary.templateCachePath.clear();
  model.runtimeSummary.latestTemplateMatchSummary.clear();

  syncProgramModel(model);
  return model;
}

void writeMarkArray(std::ostream &output, const std::vector<MarkPoint> &marks) {
  output << "  \"marks\": [\n";
  for (std::size_t index = 0; index < marks.size(); ++index) {
    const auto &mark = marks[index];
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
    output << (index + 1 < marks.size() ? ",\n" : "\n");
  }
  output << "  ],\n";
}

void writeRoiArray(std::ostream &output, const std::vector<RoiRegion> &rois) {
  output << "  \"rois\": [\n";
  for (std::size_t index = 0; index < rois.size(); ++index) {
    const auto &roi = rois[index];
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
    output << (index + 1 < rois.size() ? ",\n" : "\n");
  }
  output << "  ],\n";
}

void writeMarkReferences(std::ostream &output, const std::vector<MarkReferenceRecord> &markReferences) {
  output << "  \"markReferences\": [\n";
  for (std::size_t index = 0; index < markReferences.size(); ++index) {
    const auto &reference = markReferences[index];
    output << "    {\n"
           << "      \"name\": \"" << escapeJson(reference.name) << "\",\n"
           << "      \"referencePixelX\": " << reference.referencePixel.x << ",\n"
           << "      \"referencePixelY\": " << reference.referencePixel.y << ",\n"
           << "      \"referenceProductX\": " << reference.referenceProductPoint.x << ",\n"
           << "      \"referenceProductY\": " << reference.referenceProductPoint.y << ",\n"
           << "      \"enabled\": " << (reference.enabled ? "true" : "false") << "\n"
           << "    }";
    output << (index + 1 < markReferences.size() ? ",\n" : "\n");
  }
  output << "  ],\n";
}

void writeRoiDetectorConfigs(std::ostream &output, const std::vector<RoiDetectorConfig> &configs) {
  output << "  \"roiDetectorConfigs\": [\n";
  for (std::size_t index = 0; index < configs.size(); ++index) {
    const auto &config = configs[index];
    output << "    {\n"
           << "      \"roiName\": \"" << escapeJson(config.roiName) << "\",\n"
           << "      \"detectorType\": \"" << toString(config.detectorType) << "\",\n"
           << "      \"threshold\": " << config.threshold << ",\n"
           << "      \"templateImagePath\": \"" << escapeJson(config.templateImagePath) << "\",\n"
           << "      \"aiModelPath\": \"" << escapeJson(config.aiModelPath) << "\",\n"
           << "      \"parameterSummary\": \"" << escapeJson(config.parameterSummary) << "\",\n"
           << "      \"enabled\": " << (config.enabled ? "true" : "false") << "\n"
           << "    }";
    output << (index + 1 < configs.size() ? ",\n" : "\n");
  }
  output << "  ],\n";
}

void writeDistortionCoefficients(std::ostream &output, const std::vector<double> &coefficients) {
  output << "[";
  for (std::size_t index = 0; index < coefficients.size(); ++index) {
    output << coefficients[index];
    if (index + 1 < coefficients.size()) {
      output << ", ";
    }
  }
  output << "]";
}

} // namespace

Result<void> ProgramManager::createDefaultProgram() {
  currentProgram_ = buildDefaultProgram();
  return Result<void>::success("Default AOI program created.");
}

Result<void> ProgramManager::createProgram(const ProgramModel &program) {
  currentProgram_ = program;
  syncProgramModel(*currentProgram_);
  return Result<void>::success("Program created.");
}

Result<void> ProgramManager::saveProgram(const std::string &filePath) const {
  if (!currentProgram_.has_value()) {
    return Result<void>::failure("No active program to save.");
  }

  ProgramModel serializableProgram = *currentProgram_;
  syncProgramModel(serializableProgram);

  std::ofstream output(filePath);
  if (!output.is_open()) {
    return Result<void>::failure("Failed to open program file for writing.");
  }

  output << "{\n"
         << "  \"name\": \"" << escapeJson(serializableProgram.name) << "\",\n"
         << "  \"aiModelPath\": \"" << escapeJson(serializableProgram.aiModelPath) << "\",\n"
         << "  \"calibrationFilePath\": \"" << escapeJson(serializableProgram.calibrationFilePath) << "\",\n"
         << "  \"codeRegionName\": \"" << escapeJson(serializableProgram.codeRegionName) << "\",\n"
         << "  \"cameraIntrinsicCalibration\": {\n"
         << "    \"calibrated\": "
         << (serializableProgram.cameraIntrinsicCalibration.calibrated ? "true" : "false") << ",\n"
         << "    \"fx\": " << serializableProgram.cameraIntrinsicCalibration.fx << ",\n"
         << "    \"fy\": " << serializableProgram.cameraIntrinsicCalibration.fy << ",\n"
         << "    \"cx\": " << serializableProgram.cameraIntrinsicCalibration.cx << ",\n"
         << "    \"cy\": " << serializableProgram.cameraIntrinsicCalibration.cy << ",\n"
         << "    \"distortionCoefficients\": ";
  writeDistortionCoefficients(output, serializableProgram.cameraIntrinsicCalibration.distortionCoefficients);
  output << "\n  },\n"
         << "  \"pixelScaleCalibration\": {\n"
         << "    \"calibrated\": " << (serializableProgram.pixelScaleCalibration.calibrated ? "true" : "false")
         << ",\n"
         << "    \"pixelToMillimeterX\": " << serializableProgram.pixelScaleCalibration.pixelToMillimeterX
         << ",\n"
         << "    \"pixelToMillimeterY\": " << serializableProgram.pixelScaleCalibration.pixelToMillimeterY
         << "\n  },\n"
         << "  \"originCalibration\": {\n"
         << "    \"calibrated\": " << (serializableProgram.originCalibration.calibrated ? "true" : "false")
         << ",\n"
         << "    \"imageReferenceX\": " << serializableProgram.originCalibration.imageReferencePixel.x << ",\n"
         << "    \"imageReferenceY\": " << serializableProgram.originCalibration.imageReferencePixel.y << ",\n"
         << "    \"machineReferenceX\": " << serializableProgram.originCalibration.machineReferencePose.x << ",\n"
         << "    \"machineReferenceY\": " << serializableProgram.originCalibration.machineReferencePose.y << ",\n"
         << "    \"machineReferenceZ\": " << serializableProgram.originCalibration.machineReferencePose.z << ",\n"
         << "    \"machineReferenceR\": " << serializableProgram.originCalibration.machineReferencePose.r
         << "\n  },\n"
         << "  \"laserOffsetCalibration\": {\n"
         << "    \"calibrated\": " << (serializableProgram.laserOffsetCalibration.calibrated ? "true" : "false")
         << ",\n"
         << "    \"cameraToLaserDxMm\": " << serializableProgram.laserOffsetCalibration.cameraToLaserDxMm
         << ",\n"
         << "    \"cameraToLaserDyMm\": " << serializableProgram.laserOffsetCalibration.cameraToLaserDyMm
         << "\n  },\n";

  writeMarkReferences(output, serializableProgram.markReferences);
  writeRoiDetectorConfigs(output, serializableProgram.roiDetectorConfigs);
  writeMarkArray(output, serializableProgram.marks);
  writeRoiArray(output, serializableProgram.rois);

  output << "  \"runtimeSummary\": {\n"
         << "    \"templateCachePath\": \"" << escapeJson(serializableProgram.runtimeSummary.templateCachePath)
         << "\",\n"
         << "    \"latestTemplateMatchSummary\": \""
         << escapeJson(serializableProgram.runtimeSummary.latestTemplateMatchSummary) << "\",\n"
         << "    \"hasMarkCalibration\": "
         << (serializableProgram.runtimeSummary.hasMarkCalibration ? "true" : "false") << ",\n"
         << "    \"markCalibrationOffsetXmm\": " << serializableProgram.runtimeSummary.markCalibrationOffsetXmm
         << ",\n"
         << "    \"markCalibrationOffsetYmm\": " << serializableProgram.runtimeSummary.markCalibrationOffsetYmm
         << ",\n"
         << "    \"markCalibrationRotationDegrees\": "
         << serializableProgram.runtimeSummary.markCalibrationRotationDegrees << ",\n"
         << "    \"hasOriginCalibration\": "
         << (serializableProgram.runtimeSummary.hasOriginCalibration ? "true" : "false") << ",\n"
         << "    \"originCorrectedX\": " << serializableProgram.runtimeSummary.originCorrectedPose.x << ",\n"
         << "    \"originCorrectedY\": " << serializableProgram.runtimeSummary.originCorrectedPose.y << ",\n"
         << "    \"originCorrectedZ\": " << serializableProgram.runtimeSummary.originCorrectedPose.z << ",\n"
         << "    \"originCorrectedR\": " << serializableProgram.runtimeSummary.originCorrectedPose.r
         << "\n  }\n"
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
  if (const auto codeRegionName = extractStringField(content, "codeRegionName"); codeRegionName.has_value()) {
    model.codeRegionName = *codeRegionName;
  }

  if (const auto value = extractBoolField(content, "calibrated"); value.has_value()) {
    model.cameraIntrinsicCalibration.calibrated = *value;
  }
  if (const auto value = extractDoubleField(content, "fx"); value.has_value()) {
    model.cameraIntrinsicCalibration.fx = *value;
  }
  if (const auto value = extractDoubleField(content, "fy"); value.has_value()) {
    model.cameraIntrinsicCalibration.fy = *value;
  }
  if (const auto value = extractDoubleField(content, "cx"); value.has_value()) {
    model.cameraIntrinsicCalibration.cx = *value;
  }
  if (const auto value = extractDoubleField(content, "cy"); value.has_value()) {
    model.cameraIntrinsicCalibration.cy = *value;
  }
  const auto distortion = parseDoubleArray(content, "distortionCoefficients");
  if (!distortion.empty()) {
    model.cameraIntrinsicCalibration.distortionCoefficients = distortion;
  }

  if (const auto value = extractDoubleField(content, "pixelToMillimeterX"); value.has_value()) {
    model.pixelScaleCalibration.calibrated = true;
    model.pixelScaleCalibration.pixelToMillimeterX = *value;
  }
  if (const auto value = extractDoubleField(content, "pixelToMillimeterY"); value.has_value()) {
    model.pixelScaleCalibration.calibrated = true;
    model.pixelScaleCalibration.pixelToMillimeterY = *value;
  }

  if (const auto value = extractDoubleField(content, "imageReferenceX"); value.has_value()) {
    model.originCalibration.imageReferencePixel.x = *value;
  }
  if (const auto value = extractDoubleField(content, "imageReferenceY"); value.has_value()) {
    model.originCalibration.imageReferencePixel.y = *value;
  }
  if (const auto value = extractDoubleField(content, "machineReferenceX"); value.has_value()) {
    model.originCalibration.machineReferencePose.x = *value;
  }
  if (const auto value = extractDoubleField(content, "machineReferenceY"); value.has_value()) {
    model.originCalibration.machineReferencePose.y = *value;
  }
  if (const auto value = extractDoubleField(content, "machineReferenceZ"); value.has_value()) {
    model.originCalibration.machineReferencePose.z = *value;
  }
  if (const auto value = extractDoubleField(content, "machineReferenceR"); value.has_value()) {
    model.originCalibration.machineReferencePose.r = *value;
  }

  if (const auto value = extractDoubleField(content, "cameraToLaserDxMm"); value.has_value()) {
    model.laserOffsetCalibration.calibrated = true;
    model.laserOffsetCalibration.cameraToLaserDxMm = *value;
  }
  if (const auto value = extractDoubleField(content, "cameraToLaserDyMm"); value.has_value()) {
    model.laserOffsetCalibration.calibrated = true;
    model.laserOffsetCalibration.cameraToLaserDyMm = *value;
  }

  model.markReferences = parseMarkReferences(content);
  model.roiDetectorConfigs = parseRoiDetectorConfigs(content);

  if (const auto templateCachePath = extractStringField(content, "templateCachePath");
      templateCachePath.has_value()) {
    model.runtimeSummary.templateCachePath = *templateCachePath;
  }
  if (const auto latestTemplateMatchSummary = extractStringField(content, "latestTemplateMatchSummary");
      latestTemplateMatchSummary.has_value()) {
    model.runtimeSummary.latestTemplateMatchSummary = *latestTemplateMatchSummary;
  }
  if (const auto value = extractBoolField(content, "hasMarkCalibration"); value.has_value()) {
    model.runtimeSummary.hasMarkCalibration = *value;
  }
  if (const auto value = extractDoubleField(content, "markCalibrationOffsetXmm"); value.has_value()) {
    model.runtimeSummary.markCalibrationOffsetXmm = *value;
  }
  if (const auto value = extractDoubleField(content, "markCalibrationOffsetYmm"); value.has_value()) {
    model.runtimeSummary.markCalibrationOffsetYmm = *value;
  }
  if (const auto value = extractDoubleField(content, "markCalibrationRotationDegrees"); value.has_value()) {
    model.runtimeSummary.markCalibrationRotationDegrees = *value;
  }
  if (const auto value = extractBoolField(content, "hasOriginCalibration"); value.has_value()) {
    model.runtimeSummary.hasOriginCalibration = *value;
  }
  if (const auto value = extractDoubleField(content, "originCorrectedX"); value.has_value()) {
    model.runtimeSummary.originCorrectedPose.x = *value;
  }
  if (const auto value = extractDoubleField(content, "originCorrectedY"); value.has_value()) {
    model.runtimeSummary.originCorrectedPose.y = *value;
  }
  if (const auto value = extractDoubleField(content, "originCorrectedZ"); value.has_value()) {
    model.runtimeSummary.originCorrectedPose.z = *value;
  }
  if (const auto value = extractDoubleField(content, "originCorrectedR"); value.has_value()) {
    model.runtimeSummary.originCorrectedPose.r = *value;
  }

  model.marks = parseMarks(content);
  model.rois = parseRois(content);

  syncProgramModel(model);
  currentProgram_ = model;
  return Result<ProgramModel>::success(model, "Program loaded.");
}

std::optional<ProgramModel> ProgramManager::currentProgram() const { return currentProgram_; }

ProgramModel *ProgramManager::mutableProgram() {
  return currentProgram_.has_value() ? &*currentProgram_ : nullptr;
}
