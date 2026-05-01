#include "config/AppSettings.h"

#include <fstream>
#include <sstream>
#include <string>

namespace {

std::string escapeJson(const std::string &input) {
  std::string output;
  output.reserve(input.size() * 2);
  for (const char ch : input) {
    switch (ch) {
    case '"': output += "\\\""; break;
    case '\\': output += "\\\\"; break;
    case '\n': output += "\\n"; break;
    case '\r': output += "\\r"; break;
    case '\t': output += "\\t"; break;
    default: output += ch; break;
    }
  }
  return output;
}

std::string readFile(const std::string &filePath) {
  std::ifstream file(filePath);
  if (!file.is_open()) {
    return {};
  }
  std::ostringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

// Extracts a string value for a top-level key: "key": "..."
std::string extractJsonString(const std::string &json, const std::string &key) {
  const std::string search = "\"" + key + "\": \"";
  const auto pos = json.find(search);
  if (pos == std::string::npos) {
    return {};
  }
  const auto start = pos + search.size();
  auto end = start;
  while (end < json.size()) {
    if (json[end] == '"' && (end == start || json[end - 1] != '\\')) {
      break;
    }
    ++end;
  }
  return json.substr(start, end - start);
}

bool extractJsonBool(const std::string &json, const std::string &key, bool defaultValue) {
  const std::string search = "\"" + key + "\": ";
  const auto pos = json.find(search);
  if (pos == std::string::npos) {
    return defaultValue;
  }
  const auto start = pos + search.size();
  return json.substr(start, 4) == "true";
}

int extractJsonInt(const std::string &json, const std::string &key, int defaultValue) {
  const std::string search = "\"" + key + "\": ";
  const auto pos = json.find(search);
  if (pos == std::string::npos) {
    return defaultValue;
  }
  const auto start = pos + search.size();
  try {
    return std::stoi(json.substr(start));
  } catch (...) {
    return defaultValue;
  }
}

// Extracts a value from a nested group by locating the group object first, then
// the key within it. Falls back to the top-level flat key.
std::string extractNestedString(const std::string &json,
                                const std::string &group,
                                const std::string &key,
                                const std::string &defaultValue) {
  // Try nested group first.
  const std::string groupSearch = "\"" + group + "\": {";
  const auto groupPos = json.find(groupSearch);
  if (groupPos != std::string::npos) {
    const std::string groupJson = json.substr(groupPos);
    const std::string val = extractJsonString(groupJson, key);
    if (!val.empty()) {
      return val;
    }
  }
  // Fall back to flat key.
  const std::string flat = extractJsonString(json, key);
  return flat.empty() ? defaultValue : flat;
}

bool extractNestedBool(const std::string &json,
                       const std::string &group,
                       const std::string &key,
                       bool defaultValue) {
  const std::string groupSearch = "\"" + group + "\": {";
  const auto groupPos = json.find(groupSearch);
  if (groupPos != std::string::npos) {
    const std::string groupJson = json.substr(groupPos);
    // Only use nested value if the key actually exists there.
    if (groupJson.find("\"" + key + "\": ") != std::string::npos) {
      return extractJsonBool(groupJson, key, defaultValue);
    }
  }
  return extractJsonBool(json, key, defaultValue);
}

int extractNestedInt(const std::string &json,
                     const std::string &group,
                     const std::string &key,
                     int defaultValue) {
  const std::string groupSearch = "\"" + group + "\": {";
  const auto groupPos = json.find(groupSearch);
  if (groupPos != std::string::npos) {
    const std::string groupJson = json.substr(groupPos);
    if (groupJson.find("\"" + key + "\": ") != std::string::npos) {
      return extractJsonInt(groupJson, key, defaultValue);
    }
  }
  return extractJsonInt(json, key, defaultValue);
}

} // namespace

AppSettings AppSettingsManager::load(const std::string &filePath) {
  AppSettings settings;
  const std::string json = readFile(filePath);
  if (json.empty()) {
    return settings;
  }

  // Application group (with flat fallback)
  settings.language = extractNestedString(json, "application", "language", "zh_CN");
  settings.programOpenPath = extractNestedString(json, "application", "programOpenPath", "");
  settings.templateFolderPath = extractNestedString(json, "application", "templateFolderPath", "");
  settings.persistLogs = extractNestedBool(json, "application", "persistLogs", false);
  settings.logFilePath = extractNestedString(json, "application", "logFilePath", "");
  settings.logMinLevel = extractNestedString(json, "application", "logMinLevel", "Info");

  // UI group
  settings.showLogWindow = extractNestedBool(json, "ui", "showLogWindow", false);
  settings.theme = extractNestedString(json, "ui", "theme", "dark");

  // Runtime group
  settings.workMode = extractNestedString(json, "runtime", "workMode", "manual");
  settings.alarmEnabled = extractNestedBool(json, "runtime", "alarmEnabled", false);
  settings.maxDefectCount = extractNestedInt(json, "runtime", "maxDefectCount", 10);

  // Camera group
  settings.cameraDeviceIndex = extractNestedInt(json, "camera", "deviceIndex", 0);
  settings.cameraWidth = extractNestedInt(json, "camera", "width", 1920);
  settings.cameraHeight = extractNestedInt(json, "camera", "height", 1080);

  return settings;
}

bool AppSettingsManager::save(const AppSettings &settings, const std::string &filePath) {
  std::ofstream file(filePath);
  if (!file.is_open()) {
    return false;
  }

  file << "{\n"

       << "  \"application\": {\n"
       << "    \"language\": \"" << escapeJson(settings.language) << "\",\n"
       << "    \"programOpenPath\": \"" << escapeJson(settings.programOpenPath) << "\",\n"
       << "    \"templateFolderPath\": \"" << escapeJson(settings.templateFolderPath) << "\",\n"
       << "    \"persistLogs\": " << (settings.persistLogs ? "true" : "false") << ",\n"
       << "    \"logFilePath\": \"" << escapeJson(settings.logFilePath) << "\",\n"
       << "    \"logMinLevel\": \"" << escapeJson(settings.logMinLevel) << "\"\n"
       << "  },\n"

       << "  \"ui\": {\n"
       << "    \"showLogWindow\": " << (settings.showLogWindow ? "true" : "false") << ",\n"
       << "    \"theme\": \"" << escapeJson(settings.theme) << "\"\n"
       << "  },\n"

       << "  \"runtime\": {\n"
       << "    \"workMode\": \"" << escapeJson(settings.workMode) << "\",\n"
       << "    \"alarmEnabled\": " << (settings.alarmEnabled ? "true" : "false") << ",\n"
       << "    \"maxDefectCount\": " << settings.maxDefectCount << "\n"
       << "  },\n"

       << "  \"camera\": {\n"
       << "    \"deviceIndex\": " << settings.cameraDeviceIndex << ",\n"
       << "    \"width\": " << settings.cameraWidth << ",\n"
       << "    \"height\": " << settings.cameraHeight << "\n"
       << "  }\n"

       << "}\n";

  return true;
}
