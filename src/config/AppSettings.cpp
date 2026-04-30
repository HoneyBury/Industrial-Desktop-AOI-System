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

} // namespace

AppSettings AppSettingsManager::load(const std::string &filePath) {
  AppSettings settings;
  const std::string json = readFile(filePath);
  if (json.empty()) {
    return settings;
  }

  settings.programOpenPath = extractJsonString(json, "programOpenPath");
  settings.templateFolderPath = extractJsonString(json, "templateFolderPath");
  settings.persistLogs = extractJsonBool(json, "persistLogs", false);
  settings.logFilePath = extractJsonString(json, "logFilePath");
  settings.showLogWindow = extractJsonBool(json, "showLogWindow", false);
  settings.workMode = extractJsonString(json, "workMode");
  if (settings.workMode.empty()) {
    settings.workMode = "manual";
  }
  settings.alarmEnabled = extractJsonBool(json, "alarmEnabled", false);
  settings.maxDefectCount = extractJsonInt(json, "maxDefectCount", 10);

  return settings;
}

bool AppSettingsManager::save(const AppSettings &settings, const std::string &filePath) {
  std::ofstream file(filePath);
  if (!file.is_open()) {
    return false;
  }

  file << "{\n"
       << "  \"programOpenPath\": \"" << escapeJson(settings.programOpenPath) << "\",\n"
       << "  \"templateFolderPath\": \"" << escapeJson(settings.templateFolderPath) << "\",\n"
       << "  \"persistLogs\": " << (settings.persistLogs ? "true" : "false") << ",\n"
       << "  \"logFilePath\": \"" << escapeJson(settings.logFilePath) << "\",\n"
       << "  \"showLogWindow\": " << (settings.showLogWindow ? "true" : "false") << ",\n"
       << "  \"workMode\": \"" << escapeJson(settings.workMode) << "\",\n"
       << "  \"alarmEnabled\": " << (settings.alarmEnabled ? "true" : "false") << ",\n"
       << "  \"maxDefectCount\": " << settings.maxDefectCount << "\n"
       << "}\n";

  return true;
}
