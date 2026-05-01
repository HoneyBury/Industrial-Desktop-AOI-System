#include "program/ProgramManager.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string_view>
#include <vector>

namespace {

namespace json {

enum class Type {
  Null,
  Bool,
  Number,
  String,
  Array,
  Object,
};

struct Value {
  Type type {Type::Null};
  bool boolValue {false};
  double numberValue {0.0};
  std::string stringValue;
  std::vector<Value> arrayValue;
  std::map<std::string, Value> objectValue;

  static Value boolean(const bool value) {
    Value result;
    result.type = Type::Bool;
    result.boolValue = value;
    return result;
  }

  static Value number(const double value) {
    Value result;
    result.type = Type::Number;
    result.numberValue = value;
    return result;
  }

  static Value string(std::string value) {
    Value result;
    result.type = Type::String;
    result.stringValue = std::move(value);
    return result;
  }

  static Value array(std::vector<Value> value = {}) {
    Value result;
    result.type = Type::Array;
    result.arrayValue = std::move(value);
    return result;
  }

  static Value object(std::map<std::string, Value> value = {}) {
    Value result;
    result.type = Type::Object;
    result.objectValue = std::move(value);
    return result;
  }

  [[nodiscard]] const Value *find(const std::string &key) const {
    if (type != Type::Object) {
      return nullptr;
    }
    const auto iterator = objectValue.find(key);
    return iterator == objectValue.end() ? nullptr : &iterator->second;
  }
};

class Parser {
public:
  explicit Parser(const std::string &text) : text_(text) {}

  Result<Value> parse() {
    skipWhitespace();
    const auto valueResult = parseValue();
    if (!valueResult) {
      return valueResult;
    }

    skipWhitespace();
    if (position_ != text_.size()) {
      return Result<Value>::failure("Unexpected trailing characters in JSON.");
    }

    return valueResult;
  }

private:
  Result<Value> parseValue() {
    skipWhitespace();
    if (position_ >= text_.size()) {
      return Result<Value>::failure("Unexpected end of JSON.");
    }

    const char ch = text_[position_];
    if (ch == '{') {
      return parseObject();
    }
    if (ch == '[') {
      return parseArray();
    }
    if (ch == '"') {
      return parseString();
    }
    if (ch == 't' || ch == 'f') {
      return parseBool();
    }
    if (ch == 'n') {
      return parseNull();
    }
    if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch))) {
      return parseNumber();
    }

    return Result<Value>::failure("Unexpected JSON token.");
  }

  Result<Value> parseObject() {
    ++position_; // {
    skipWhitespace();

    std::map<std::string, Value> object;
    if (match('}')) {
      return Result<Value>::success(Value::object(std::move(object)));
    }

    while (position_ < text_.size()) {
      const auto keyResult = parseString();
      if (!keyResult) {
        return keyResult;
      }
      skipWhitespace();
      if (!match(':')) {
        return Result<Value>::failure("Expected ':' in JSON object.");
      }

      const auto valueResult = parseValue();
      if (!valueResult) {
        return valueResult;
      }
      object.emplace(keyResult.value.stringValue, valueResult.value);

      skipWhitespace();
      if (match('}')) {
        return Result<Value>::success(Value::object(std::move(object)));
      }
      if (!match(',')) {
        return Result<Value>::failure("Expected ',' in JSON object.");
      }
      skipWhitespace();
    }

    return Result<Value>::failure("Unterminated JSON object.");
  }

  Result<Value> parseArray() {
    ++position_; // [
    skipWhitespace();

    std::vector<Value> array;
    if (match(']')) {
      return Result<Value>::success(Value::array(std::move(array)));
    }

    while (position_ < text_.size()) {
      const auto valueResult = parseValue();
      if (!valueResult) {
        return valueResult;
      }
      array.push_back(valueResult.value);

      skipWhitespace();
      if (match(']')) {
        return Result<Value>::success(Value::array(std::move(array)));
      }
      if (!match(',')) {
        return Result<Value>::failure("Expected ',' in JSON array.");
      }
      skipWhitespace();
    }

    return Result<Value>::failure("Unterminated JSON array.");
  }

  Result<Value> parseString() {
    if (!match('"')) {
      return Result<Value>::failure("Expected JSON string.");
    }

    std::string result;
    while (position_ < text_.size()) {
      const char ch = text_[position_++];
      if (ch == '"') {
        return Result<Value>::success(Value::string(std::move(result)));
      }
      if (ch == '\\') {
        if (position_ >= text_.size()) {
          return Result<Value>::failure("Invalid JSON escape sequence.");
        }
        const char escaped = text_[position_++];
        switch (escaped) {
        case '"':
          result += '"';
          break;
        case '\\':
          result += '\\';
          break;
        case 'n':
          result += '\n';
          break;
        case 'r':
          result += '\r';
          break;
        case 't':
          result += '\t';
          break;
        default:
          result += escaped;
          break;
        }
        continue;
      }

      result += ch;
    }

    return Result<Value>::failure("Unterminated JSON string.");
  }

  Result<Value> parseBool() {
    if (text_.compare(position_, 4, "true") == 0) {
      position_ += 4;
      return Result<Value>::success(Value::boolean(true));
    }
    if (text_.compare(position_, 5, "false") == 0) {
      position_ += 5;
      return Result<Value>::success(Value::boolean(false));
    }
    return Result<Value>::failure("Invalid JSON boolean literal.");
  }

  Result<Value> parseNull() {
    if (text_.compare(position_, 4, "null") != 0) {
      return Result<Value>::failure("Invalid JSON null literal.");
    }

    position_ += 4;
    return Result<Value>::success(Value {});
  }

  Result<Value> parseNumber() {
    const std::size_t start = position_;
    if (text_[position_] == '-') {
      ++position_;
    }

    while (position_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[position_]))) {
      ++position_;
    }

    if (position_ < text_.size() && text_[position_] == '.') {
      ++position_;
      while (position_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[position_]))) {
        ++position_;
      }
    }

    if (position_ < text_.size() && (text_[position_] == 'e' || text_[position_] == 'E')) {
      ++position_;
      if (position_ < text_.size() && (text_[position_] == '+' || text_[position_] == '-')) {
        ++position_;
      }
      while (position_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[position_]))) {
        ++position_;
      }
    }

    const std::string token = text_.substr(start, position_ - start);
    char *end = nullptr;
    const double number = std::strtod(token.c_str(), &end);
    if (end == nullptr || *end != '\0') {
      return Result<Value>::failure("Invalid JSON number.");
    }

    return Result<Value>::success(Value::number(number));
  }

  void skipWhitespace() {
    while (position_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[position_]))) {
      ++position_;
    }
  }

  bool match(const char expected) {
    if (position_ < text_.size() && text_[position_] == expected) {
      ++position_;
      return true;
    }
    return false;
  }

  const std::string &text_;
  std::size_t position_ {0};
};

std::string escape(const std::string &input) {
  std::string output;
  output.reserve(input.size() * 2);
  for (const char ch : input) {
    switch (ch) {
    case '\\':
      output += "\\\\";
      break;
    case '"':
      output += "\\\"";
      break;
    case '\n':
      output += "\\n";
      break;
    case '\r':
      output += "\\r";
      break;
    case '\t':
      output += "\\t";
      break;
    default:
      output += ch;
      break;
    }
  }
  return output;
}

void writeIndent(std::ostream &output, const int indent) {
  for (int index = 0; index < indent; ++index) {
    output.put(' ');
  }
}

void writeValue(std::ostream &output, const Value &value, const int indent = 0) {
  switch (value.type) {
  case Type::Null:
    output << "null";
    return;
  case Type::Bool:
    output << (value.boolValue ? "true" : "false");
    return;
  case Type::Number:
    output << value.numberValue;
    return;
  case Type::String:
    output << '"' << escape(value.stringValue) << '"';
    return;
  case Type::Array: {
    output << "[";
    if (!value.arrayValue.empty()) {
      output << "\n";
      for (std::size_t index = 0; index < value.arrayValue.size(); ++index) {
        writeIndent(output, indent + 2);
        writeValue(output, value.arrayValue[index], indent + 2);
        output << (index + 1 < value.arrayValue.size() ? ",\n" : "\n");
      }
      writeIndent(output, indent);
    }
    output << "]";
    return;
  }
  case Type::Object: {
    output << "{";
    if (!value.objectValue.empty()) {
      output << "\n";
      std::size_t index = 0;
      for (const auto &[key, child] : value.objectValue) {
        writeIndent(output, indent + 2);
        output << '"' << escape(key) << "\": ";
        writeValue(output, child, indent + 2);
        output << (index + 1 < value.objectValue.size() ? ",\n" : "\n");
        ++index;
      }
      writeIndent(output, indent);
    }
    output << "}";
    return;
  }
  }
}

} // namespace json

std::string stringOrDefault(const json::Value *value, const std::string &defaultValue = {}) {
  if (value == nullptr || value->type != json::Type::String) {
    return defaultValue;
  }
  return value->stringValue;
}

bool boolOrDefault(const json::Value *value, const bool defaultValue = false) {
  if (value == nullptr || value->type != json::Type::Bool) {
    return defaultValue;
  }
  return value->boolValue;
}

double doubleOrDefault(const json::Value *value, const double defaultValue = 0.0) {
  if (value == nullptr || value->type != json::Type::Number) {
    return defaultValue;
  }
  return value->numberValue;
}

int intOrDefault(const json::Value *value, const int defaultValue = 0) {
  return static_cast<int>(doubleOrDefault(value, static_cast<double>(defaultValue)));
}

const json::Value *objectField(const json::Value &value, const std::string &key) {
  return value.find(key);
}

std::vector<double> parseDoubleVector(const json::Value *value) {
  std::vector<double> results;
  if (value == nullptr || value->type != json::Type::Array) {
    return results;
  }

  for (const auto &entry : value->arrayValue) {
    if (entry.type == json::Type::Number) {
      results.push_back(entry.numberValue);
    }
  }

  return results;
}

std::vector<MarkPoint> parseMarks(const json::Value *value) {
  std::vector<MarkPoint> marks;
  if (value == nullptr || value->type != json::Type::Array) {
    return marks;
  }

  for (const auto &entry : value->arrayValue) {
    if (entry.type != json::Type::Object) {
      continue;
    }

    MarkPoint mark;
    mark.name = stringOrDefault(objectField(entry, "name"), mark.name);
    mark.x = doubleOrDefault(objectField(entry, "x"), mark.x);
    mark.y = doubleOrDefault(objectField(entry, "y"), mark.y);
    mark.width = doubleOrDefault(objectField(entry, "width"), mark.width);
    mark.height = doubleOrDefault(objectField(entry, "height"), mark.height);
    mark.rotation = doubleOrDefault(objectField(entry, "rotation"), mark.rotation);
    mark.score = doubleOrDefault(objectField(entry, "score"), mark.score);
    mark.minimumScore = doubleOrDefault(objectField(entry, "minimumScore"), mark.minimumScore);
    mark.previewScore = doubleOrDefault(objectField(entry, "previewScore"), mark.previewScore);
    mark.colorTolerance = intOrDefault(objectField(entry, "colorTolerance"), mark.colorTolerance);
    mark.sampledColor = stringOrDefault(objectField(entry, "sampledColor"), mark.sampledColor);
    mark.shape = markShapeFromString(stringOrDefault(objectField(entry, "shape"), "rectangle"));
    mark.algorithm = markAlgorithmFromString(
        stringOrDefault(objectField(entry, "algorithm"), "color_brush_template"));
    mark.enabled = boolOrDefault(objectField(entry, "enabled"), mark.enabled);
    marks.push_back(mark);
  }

  return marks;
}

std::vector<RoiRegion> parseRois(const json::Value *value) {
  std::vector<RoiRegion> rois;
  if (value == nullptr || value->type != json::Type::Array) {
    return rois;
  }

  for (const auto &entry : value->arrayValue) {
    if (entry.type != json::Type::Object) {
      continue;
    }

    RoiRegion roi;
    roi.name = stringOrDefault(objectField(entry, "name"), roi.name);
    roi.x = doubleOrDefault(objectField(entry, "x"), roi.x);
    roi.y = doubleOrDefault(objectField(entry, "y"), roi.y);
    roi.width = doubleOrDefault(objectField(entry, "width"), roi.width);
    roi.height = doubleOrDefault(objectField(entry, "height"), roi.height);
    roi.rotation = doubleOrDefault(objectField(entry, "rotation"), roi.rotation);
    roi.threshold = doubleOrDefault(objectField(entry, "threshold"), roi.threshold);
    roi.shape = roiShapeFromString(stringOrDefault(objectField(entry, "shape"), "rectangle"));
    roi.enabled = boolOrDefault(objectField(entry, "enabled"), roi.enabled);
    rois.push_back(roi);
  }

  return rois;
}

std::vector<MarkReferenceRecord> parseMarkReferences(const json::Value *value) {
  std::vector<MarkReferenceRecord> references;
  if (value == nullptr || value->type != json::Type::Array) {
    return references;
  }

  for (const auto &entry : value->arrayValue) {
    if (entry.type != json::Type::Object) {
      continue;
    }

    MarkReferenceRecord reference;
    reference.name = stringOrDefault(objectField(entry, "name"));
    reference.referencePixel.x = doubleOrDefault(objectField(entry, "referencePixelX"));
    reference.referencePixel.y = doubleOrDefault(objectField(entry, "referencePixelY"));
    reference.referenceProductPoint.x = doubleOrDefault(objectField(entry, "referenceProductX"));
    reference.referenceProductPoint.y = doubleOrDefault(objectField(entry, "referenceProductY"));
    reference.enabled = boolOrDefault(objectField(entry, "enabled"), true);
    references.push_back(reference);
  }

  return references;
}

std::vector<RoiDetectorConfig> parseRoiDetectorConfigs(const json::Value *value) {
  std::vector<RoiDetectorConfig> configs;
  if (value == nullptr || value->type != json::Type::Array) {
    return configs;
  }

  for (const auto &entry : value->arrayValue) {
    if (entry.type != json::Type::Object) {
      continue;
    }

    RoiDetectorConfig config;
    config.roiName = stringOrDefault(objectField(entry, "roiName"));
    config.detectorType =
        roiDetectorTypeFromString(stringOrDefault(objectField(entry, "detectorType"), "geometry"));
    config.threshold = doubleOrDefault(objectField(entry, "threshold"), config.threshold);
    config.templateImagePath = stringOrDefault(objectField(entry, "templateImagePath"));
    config.aiModelPath = stringOrDefault(objectField(entry, "aiModelPath"));
    config.parameterSummary = stringOrDefault(objectField(entry, "parameterSummary"));
    config.enabled = boolOrDefault(objectField(entry, "enabled"), config.enabled);
    configs.push_back(config);
  }

  return configs;
}

std::vector<LaserPointTask> parseLaserPointTasks(const json::Value *value) {
  std::vector<LaserPointTask> tasks;
  if (value == nullptr || value->type != json::Type::Array) {
    return tasks;
  }

  for (const auto &entry : value->arrayValue) {
    if (entry.type != json::Type::Object) {
      continue;
    }

    LaserPointTask task;
    task.name = stringOrDefault(objectField(entry, "name"));
    task.x = doubleOrDefault(objectField(entry, "x"), task.x);
    task.y = doubleOrDefault(objectField(entry, "y"), task.y);
    task.linkedRoiName = stringOrDefault(objectField(entry, "linkedRoiName"));
    task.expectedCodeText =
        stringOrDefault(objectField(entry, "expectedCodeText"), task.expectedCodeText);
    task.enabled = boolOrDefault(objectField(entry, "enabled"), task.enabled);
    tasks.push_back(task);
  }

  return tasks;
}

void syncCanonicalCalibration(ProgramModel &model) {
  if (!model.cameraIntrinsicCalibration.calibrated &&
      model.cameraIntrinsicCalibration.fx == 0.0 &&
      model.cameraIntrinsicCalibration.fy == 0.0 &&
      model.cameraIntrinsicCalibration.cx == 0.0 &&
      model.cameraIntrinsicCalibration.cy == 0.0 &&
      (model.calibrationData.fx != 0.0 || model.calibrationData.fy != 0.0)) {
    model.cameraIntrinsicCalibration.calibrated = true;
    model.cameraIntrinsicCalibration.fx = model.calibrationData.fx;
    model.cameraIntrinsicCalibration.fy = model.calibrationData.fy;
    model.cameraIntrinsicCalibration.cx = model.calibrationData.cx;
    model.cameraIntrinsicCalibration.cy = model.calibrationData.cy;
  }

  if (!model.pixelScaleCalibration.calibrated &&
      model.pixelScaleCalibration.pixelToMillimeterX == 0.0 &&
      model.pixelScaleCalibration.pixelToMillimeterY == 0.0) {
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

void deriveLaserPointTasksFromRois(ProgramModel &model) {
  if (!model.laserPointTasks.empty()) {
    return;
  }

  for (const auto &roi : model.rois) {
    LaserPointTask task;
    task.name = "Laser-" + roi.name;
    task.x = roi.x + roi.width / 2.0;
    task.y = roi.y + roi.height / 2.0;
    task.linkedRoiName = roi.name;
    task.expectedCodeText = "DEMO-CODE-001";
    task.enabled = roi.enabled;
    model.laserPointTasks.push_back(task);
  }
}

void syncProgramModel(ProgramModel &model) {
  syncCanonicalCalibration(model);
  deriveMarkReferencesFromMarks(model);
  deriveRoiDetectorConfigsFromRois(model);
  deriveLaserPointTasksFromRois(model);
  syncCompatibilityCalibration(model);
}

ProgramModel buildDefaultProgram() {
  ProgramModel model;
  model.name = "default_demo_program";
  model.aiModelPath = "models/demo.onnx";
  model.calibrationFilePath = "config/camera_calib.yaml";
  model.codeRegionName = "Code-Area";
  model.boardDefinition = BoardDefinition {260.0, 180.0, 32.0};
  model.scanRecipe = ScanRecipe {32.0, 24.0, ScanOrder::LeftToRight, true};

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
  model.laserPointTasks = {
      {"Laser-Inspect-Top", 80.0, 50.0, "Inspect-Top", "DEMO-CODE-001", true},
      {"Laser-Code-Area", 266.0, 86.0, "Code-Area", "DEMO-CODE-001", true},
  };

  model.runtimeSummary.templateCachePath.clear();
  model.runtimeSummary.latestTemplateMatchSummary.clear();
  model.runtimeSummary.wholeBoardImagePath.clear();
  model.runtimeSummary.scanTileRows = 0;
  model.runtimeSummary.scanTileColumns = 0;
  model.runtimeSummary.lastBoardScanSummary.clear();

  syncProgramModel(model);
  return model;
}

json::Value serializeProgram(const ProgramModel &program) {
  ProgramModel serializableProgram = program;
  syncProgramModel(serializableProgram);

  std::vector<json::Value> distortionCoefficients;
  for (const double value : serializableProgram.cameraIntrinsicCalibration.distortionCoefficients) {
    distortionCoefficients.push_back(json::Value::number(value));
  }

  std::vector<json::Value> markReferences;
  for (const auto &reference : serializableProgram.markReferences) {
    markReferences.push_back(json::Value::object({
        {"enabled", json::Value::boolean(reference.enabled)},
        {"name", json::Value::string(reference.name)},
        {"referencePixelX", json::Value::number(reference.referencePixel.x)},
        {"referencePixelY", json::Value::number(reference.referencePixel.y)},
        {"referenceProductX", json::Value::number(reference.referenceProductPoint.x)},
        {"referenceProductY", json::Value::number(reference.referenceProductPoint.y)},
    }));
  }

  std::vector<json::Value> roiDetectorConfigs;
  for (const auto &config : serializableProgram.roiDetectorConfigs) {
    roiDetectorConfigs.push_back(json::Value::object({
        {"aiModelPath", json::Value::string(config.aiModelPath)},
        {"detectorType", json::Value::string(std::string(toString(config.detectorType)))},
        {"enabled", json::Value::boolean(config.enabled)},
        {"parameterSummary", json::Value::string(config.parameterSummary)},
        {"roiName", json::Value::string(config.roiName)},
        {"templateImagePath", json::Value::string(config.templateImagePath)},
        {"threshold", json::Value::number(config.threshold)},
    }));
  }

  std::vector<json::Value> laserPointTasks;
  for (const auto &task : serializableProgram.laserPointTasks) {
    laserPointTasks.push_back(json::Value::object({
        {"enabled", json::Value::boolean(task.enabled)},
        {"expectedCodeText", json::Value::string(task.expectedCodeText)},
        {"linkedRoiName", json::Value::string(task.linkedRoiName)},
        {"name", json::Value::string(task.name)},
        {"x", json::Value::number(task.x)},
        {"y", json::Value::number(task.y)},
    }));
  }

  std::vector<json::Value> marks;
  for (const auto &mark : serializableProgram.marks) {
    marks.push_back(json::Value::object({
        {"algorithm", json::Value::string(std::string(toString(mark.algorithm)))},
        {"colorTolerance", json::Value::number(mark.colorTolerance)},
        {"enabled", json::Value::boolean(mark.enabled)},
        {"height", json::Value::number(mark.height)},
        {"minimumScore", json::Value::number(mark.minimumScore)},
        {"name", json::Value::string(mark.name)},
        {"previewScore", json::Value::number(mark.previewScore)},
        {"rotation", json::Value::number(mark.rotation)},
        {"sampledColor", json::Value::string(mark.sampledColor)},
        {"score", json::Value::number(mark.score)},
        {"shape", json::Value::string(std::string(toString(mark.shape)))},
        {"width", json::Value::number(mark.width)},
        {"x", json::Value::number(mark.x)},
        {"y", json::Value::number(mark.y)},
    }));
  }

  std::vector<json::Value> rois;
  for (const auto &roi : serializableProgram.rois) {
    rois.push_back(json::Value::object({
        {"enabled", json::Value::boolean(roi.enabled)},
        {"height", json::Value::number(roi.height)},
        {"name", json::Value::string(roi.name)},
        {"rotation", json::Value::number(roi.rotation)},
        {"shape", json::Value::string(std::string(toString(roi.shape)))},
        {"threshold", json::Value::number(roi.threshold)},
        {"width", json::Value::number(roi.width)},
        {"x", json::Value::number(roi.x)},
        {"y", json::Value::number(roi.y)},
    }));
  }

  return json::Value::object({
      {"aiModelPath", json::Value::string(serializableProgram.aiModelPath)},
      {"boardDefinition",
       json::Value::object({
           {"boardLengthMm", json::Value::number(serializableProgram.boardDefinition.boardLengthMm)},
           {"boardWidthMm", json::Value::number(serializableProgram.boardDefinition.boardWidthMm)},
           {"railWidthMm", json::Value::number(serializableProgram.boardDefinition.railWidthMm)},
       })},
      {"calibrationFilePath", json::Value::string(serializableProgram.calibrationFilePath)},
      {"cameraIntrinsicCalibration",
       json::Value::object({
           {"calibrated", json::Value::boolean(serializableProgram.cameraIntrinsicCalibration.calibrated)},
           {"cx", json::Value::number(serializableProgram.cameraIntrinsicCalibration.cx)},
           {"cy", json::Value::number(serializableProgram.cameraIntrinsicCalibration.cy)},
           {"distortionCoefficients", json::Value::array(std::move(distortionCoefficients))},
           {"fx", json::Value::number(serializableProgram.cameraIntrinsicCalibration.fx)},
           {"fy", json::Value::number(serializableProgram.cameraIntrinsicCalibration.fy)},
       })},
      {"codeRegionName", json::Value::string(serializableProgram.codeRegionName)},
      {"laserFrequencyKhz", json::Value::number(serializableProgram.laserFrequencyKhz)},
      {"laserOffsetCalibration",
       json::Value::object({
           {"calibrated", json::Value::boolean(serializableProgram.laserOffsetCalibration.calibrated)},
           {"cameraToLaserDxMm", json::Value::number(serializableProgram.laserOffsetCalibration.cameraToLaserDxMm)},
           {"cameraToLaserDyMm", json::Value::number(serializableProgram.laserOffsetCalibration.cameraToLaserDyMm)},
       })},
      {"laserPowerPercent", json::Value::number(serializableProgram.laserPowerPercent)},
      {"laserPulseWidthUs", json::Value::number(serializableProgram.laserPulseWidthUs)},
      {"laserRepeatCount", json::Value::number(serializableProgram.laserRepeatCount)},
      {"laserPointTasks", json::Value::array(std::move(laserPointTasks))},
      {"markReferences", json::Value::array(std::move(markReferences))},
      {"marks", json::Value::array(std::move(marks))},
      {"name", json::Value::string(serializableProgram.name)},
      {"originCalibration",
       json::Value::object({
           {"calibrated", json::Value::boolean(serializableProgram.originCalibration.calibrated)},
           {"imageReferenceX", json::Value::number(serializableProgram.originCalibration.imageReferencePixel.x)},
           {"imageReferenceY", json::Value::number(serializableProgram.originCalibration.imageReferencePixel.y)},
           {"machineReferenceR", json::Value::number(serializableProgram.originCalibration.machineReferencePose.r)},
           {"machineReferenceX", json::Value::number(serializableProgram.originCalibration.machineReferencePose.x)},
           {"machineReferenceY", json::Value::number(serializableProgram.originCalibration.machineReferencePose.y)},
           {"machineReferenceZ", json::Value::number(serializableProgram.originCalibration.machineReferencePose.z)},
       })},
      {"pixelScaleCalibration",
       json::Value::object({
           {"calibrated", json::Value::boolean(serializableProgram.pixelScaleCalibration.calibrated)},
           {"pixelToMillimeterX", json::Value::number(serializableProgram.pixelScaleCalibration.pixelToMillimeterX)},
           {"pixelToMillimeterY", json::Value::number(serializableProgram.pixelScaleCalibration.pixelToMillimeterY)},
       })},
      {"scanRecipe",
       json::Value::object({
           {"enabled", json::Value::boolean(serializableProgram.scanRecipe.enabled)},
           {"fovHeightMm", json::Value::number(serializableProgram.scanRecipe.fovHeightMm)},
           {"fovWidthMm", json::Value::number(serializableProgram.scanRecipe.fovWidthMm)},
           {"scanOrder", json::Value::string(std::string(toString(serializableProgram.scanRecipe.scanOrder)))},
       })},
      {"roiDetectorConfigs", json::Value::array(std::move(roiDetectorConfigs))},
      {"rois", json::Value::array(std::move(rois))},
      {"runtimeSummary",
       json::Value::object({
           {"hasMarkCalibration", json::Value::boolean(false)},
           {"hasOriginCalibration",
            json::Value::boolean(serializableProgram.originCalibration.calibrated)},
           {"lastBoardScanSummary", json::Value::string(serializableProgram.runtimeSummary.lastBoardScanSummary)},
           {"latestTemplateMatchSummary",
            json::Value::string(serializableProgram.runtimeSummary.latestTemplateMatchSummary)},
           {"markCalibrationOffsetXmm", json::Value::number(0.0)},
           {"markCalibrationOffsetYmm", json::Value::number(0.0)},
           {"markCalibrationRotationDegrees", json::Value::number(0.0)},
           {"originCorrectedR",
            json::Value::number(serializableProgram.originCalibration.machineReferencePose.r)},
           {"originCorrectedX",
            json::Value::number(serializableProgram.originCalibration.machineReferencePose.x)},
           {"originCorrectedY",
            json::Value::number(serializableProgram.originCalibration.machineReferencePose.y)},
           {"originCorrectedZ",
            json::Value::number(serializableProgram.originCalibration.machineReferencePose.z)},
           {"scanTileColumns", json::Value::number(serializableProgram.runtimeSummary.scanTileColumns)},
           {"scanTileRows", json::Value::number(serializableProgram.runtimeSummary.scanTileRows)},
           {"templateCachePath", json::Value::string(serializableProgram.runtimeSummary.templateCachePath)},
           {"wholeBoardImagePath", json::Value::string(serializableProgram.runtimeSummary.wholeBoardImagePath)},
       })},
  });
}

void deserializeCameraIntrinsic(ProgramModel &model, const json::Value *value) {
  if (value == nullptr || value->type != json::Type::Object) {
    return;
  }

  model.cameraIntrinsicCalibration.calibrated = boolOrDefault(objectField(*value, "calibrated"));
  model.cameraIntrinsicCalibration.fx = doubleOrDefault(objectField(*value, "fx"), model.cameraIntrinsicCalibration.fx);
  model.cameraIntrinsicCalibration.fy = doubleOrDefault(objectField(*value, "fy"), model.cameraIntrinsicCalibration.fy);
  model.cameraIntrinsicCalibration.cx = doubleOrDefault(objectField(*value, "cx"), model.cameraIntrinsicCalibration.cx);
  model.cameraIntrinsicCalibration.cy = doubleOrDefault(objectField(*value, "cy"), model.cameraIntrinsicCalibration.cy);
  model.cameraIntrinsicCalibration.distortionCoefficients =
      parseDoubleVector(objectField(*value, "distortionCoefficients"));
}

void deserializePixelScale(ProgramModel &model, const json::Value *value) {
  if (value == nullptr || value->type != json::Type::Object) {
    return;
  }

  model.pixelScaleCalibration.calibrated = boolOrDefault(objectField(*value, "calibrated"));
  model.pixelScaleCalibration.pixelToMillimeterX =
      doubleOrDefault(objectField(*value, "pixelToMillimeterX"), model.pixelScaleCalibration.pixelToMillimeterX);
  model.pixelScaleCalibration.pixelToMillimeterY =
      doubleOrDefault(objectField(*value, "pixelToMillimeterY"), model.pixelScaleCalibration.pixelToMillimeterY);
}

void deserializeOriginCalibration(ProgramModel &model, const json::Value *value) {
  if (value == nullptr || value->type != json::Type::Object) {
    return;
  }

  model.originCalibration.calibrated = boolOrDefault(objectField(*value, "calibrated"));
  model.originCalibration.imageReferencePixel.x =
      doubleOrDefault(objectField(*value, "imageReferenceX"), model.originCalibration.imageReferencePixel.x);
  model.originCalibration.imageReferencePixel.y =
      doubleOrDefault(objectField(*value, "imageReferenceY"), model.originCalibration.imageReferencePixel.y);
  model.originCalibration.machineReferencePose.x =
      doubleOrDefault(objectField(*value, "machineReferenceX"), model.originCalibration.machineReferencePose.x);
  model.originCalibration.machineReferencePose.y =
      doubleOrDefault(objectField(*value, "machineReferenceY"), model.originCalibration.machineReferencePose.y);
  model.originCalibration.machineReferencePose.z =
      doubleOrDefault(objectField(*value, "machineReferenceZ"), model.originCalibration.machineReferencePose.z);
  model.originCalibration.machineReferencePose.r =
      doubleOrDefault(objectField(*value, "machineReferenceR"), model.originCalibration.machineReferencePose.r);
}

void deserializeLaserOffset(ProgramModel &model, const json::Value *value) {
  if (value == nullptr || value->type != json::Type::Object) {
    return;
  }

  model.laserOffsetCalibration.calibrated = boolOrDefault(objectField(*value, "calibrated"));
  model.laserOffsetCalibration.cameraToLaserDxMm =
      doubleOrDefault(objectField(*value, "cameraToLaserDxMm"), model.laserOffsetCalibration.cameraToLaserDxMm);
  model.laserOffsetCalibration.cameraToLaserDyMm =
      doubleOrDefault(objectField(*value, "cameraToLaserDyMm"), model.laserOffsetCalibration.cameraToLaserDyMm);
}

void deserializeBoardDefinition(ProgramModel &model, const json::Value *value) {
  if (value == nullptr || value->type != json::Type::Object) {
    return;
  }

  model.boardDefinition.boardLengthMm =
      doubleOrDefault(objectField(*value, "boardLengthMm"), model.boardDefinition.boardLengthMm);
  model.boardDefinition.boardWidthMm =
      doubleOrDefault(objectField(*value, "boardWidthMm"), model.boardDefinition.boardWidthMm);
  model.boardDefinition.railWidthMm =
      doubleOrDefault(objectField(*value, "railWidthMm"), model.boardDefinition.railWidthMm);
}

void deserializeScanRecipe(ProgramModel &model, const json::Value *value) {
  if (value == nullptr || value->type != json::Type::Object) {
    return;
  }

  model.scanRecipe.fovWidthMm =
      doubleOrDefault(objectField(*value, "fovWidthMm"), model.scanRecipe.fovWidthMm);
  model.scanRecipe.fovHeightMm =
      doubleOrDefault(objectField(*value, "fovHeightMm"), model.scanRecipe.fovHeightMm);
  model.scanRecipe.enabled = boolOrDefault(objectField(*value, "enabled"), model.scanRecipe.enabled);
  model.scanRecipe.scanOrder =
      scanOrderFromString(stringOrDefault(objectField(*value, "scanOrder"), std::string(toString(model.scanRecipe.scanOrder))));
}

void deserializeRuntimeSummary(ProgramModel &model, const json::Value *value) {
  if (value == nullptr || value->type != json::Type::Object) {
    return;
  }

  model.runtimeSummary.templateCachePath =
      stringOrDefault(objectField(*value, "templateCachePath"), model.runtimeSummary.templateCachePath);
  model.runtimeSummary.latestTemplateMatchSummary = stringOrDefault(
      objectField(*value, "latestTemplateMatchSummary"), model.runtimeSummary.latestTemplateMatchSummary);

  // Legacy mirror fields from ProgramRuntimeSummary → redirect into canonical structs
  if (!model.originCalibration.calibrated) {
    const bool oldHasOrigin = boolOrDefault(objectField(*value, "hasOriginCalibration"), false);
    if (oldHasOrigin) {
      model.originCalibration.calibrated = true;
      model.originCalibration.machineReferencePose.x =
          doubleOrDefault(objectField(*value, "originCorrectedX"), model.originCalibration.machineReferencePose.x);
      model.originCalibration.machineReferencePose.y =
          doubleOrDefault(objectField(*value, "originCorrectedY"), model.originCalibration.machineReferencePose.y);
      model.originCalibration.machineReferencePose.z =
          doubleOrDefault(objectField(*value, "originCorrectedZ"), model.originCalibration.machineReferencePose.z);
      model.originCalibration.machineReferencePose.r =
          doubleOrDefault(objectField(*value, "originCorrectedR"), model.originCalibration.machineReferencePose.r);
    }
  }

  model.runtimeSummary.wholeBoardImagePath =
      stringOrDefault(objectField(*value, "wholeBoardImagePath"), model.runtimeSummary.wholeBoardImagePath);
  model.runtimeSummary.scanTileRows =
      intOrDefault(objectField(*value, "scanTileRows"), model.runtimeSummary.scanTileRows);
  model.runtimeSummary.scanTileColumns =
      intOrDefault(objectField(*value, "scanTileColumns"), model.runtimeSummary.scanTileColumns);
  model.runtimeSummary.lastBoardScanSummary =
      stringOrDefault(objectField(*value, "lastBoardScanSummary"), model.runtimeSummary.lastBoardScanSummary);
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

  std::ofstream output(filePath);
  if (!output.is_open()) {
    return Result<void>::failure("Failed to open program file for writing.");
  }

  const json::Value document = serializeProgram(*currentProgram_);
  json::writeValue(output, document, 0);
  output << "\n";
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

  json::Parser parser(content);
  const auto parsed = parser.parse();
  if (!parsed) {
    return Result<ProgramModel>::failure("Program JSON parse failed: " + parsed.message);
  }
  if (parsed.value.type != json::Type::Object) {
    return Result<ProgramModel>::failure("Program JSON root must be an object.");
  }

  ProgramModel model = buildDefaultProgram();
  model.filePath = filePath;

  model.name = stringOrDefault(objectField(parsed.value, "name"), model.name);
  model.aiModelPath = stringOrDefault(objectField(parsed.value, "aiModelPath"), model.aiModelPath);
  model.calibrationFilePath =
      stringOrDefault(objectField(parsed.value, "calibrationFilePath"), model.calibrationFilePath);
  model.codeRegionName = stringOrDefault(objectField(parsed.value, "codeRegionName"), model.codeRegionName);
  deserializeBoardDefinition(model, objectField(parsed.value, "boardDefinition"));
  deserializeScanRecipe(model, objectField(parsed.value, "scanRecipe"));

  deserializeCameraIntrinsic(model, objectField(parsed.value, "cameraIntrinsicCalibration"));
  deserializePixelScale(model, objectField(parsed.value, "pixelScaleCalibration"));
  deserializeOriginCalibration(model, objectField(parsed.value, "originCalibration"));
  deserializeLaserOffset(model, objectField(parsed.value, "laserOffsetCalibration"));

  model.laserPowerPercent =
      doubleOrDefault(objectField(parsed.value, "laserPowerPercent"), model.laserPowerPercent);
  model.laserFrequencyKhz =
      doubleOrDefault(objectField(parsed.value, "laserFrequencyKhz"), model.laserFrequencyKhz);
  model.laserPulseWidthUs =
      doubleOrDefault(objectField(parsed.value, "laserPulseWidthUs"), model.laserPulseWidthUs);
  model.laserRepeatCount =
      intOrDefault(objectField(parsed.value, "laserRepeatCount"), model.laserRepeatCount);

  model.markReferences = parseMarkReferences(objectField(parsed.value, "markReferences"));
  model.roiDetectorConfigs = parseRoiDetectorConfigs(objectField(parsed.value, "roiDetectorConfigs"));
  model.laserPointTasks = parseLaserPointTasks(objectField(parsed.value, "laserPointTasks"));
  model.marks = parseMarks(objectField(parsed.value, "marks"));
  model.rois = parseRois(objectField(parsed.value, "rois"));
  deserializeRuntimeSummary(model, objectField(parsed.value, "runtimeSummary"));

  syncProgramModel(model);
  currentProgram_ = model;
  return Result<ProgramModel>::success(model, "Program loaded.");
}

std::optional<ProgramModel> ProgramManager::currentProgram() const { return currentProgram_; }

ProgramModel *ProgramManager::mutableProgram() {
  return currentProgram_.has_value() ? &*currentProgram_ : nullptr;
}
