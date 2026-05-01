#pragma once

#include <string>

struct AppSettings {
  // --- Application ---
  std::string language {"zh_CN"};
  std::string programOpenPath;
  std::string templateFolderPath;
  bool persistLogs {false};
  std::string logFilePath;
  std::string logMinLevel {"Info"};

  // --- UI ---
  bool showLogWindow {false};
  std::string theme {"dark"};

  // --- Runtime ---
  std::string workMode {"manual"};
  bool alarmEnabled {false};
  int maxDefectCount {10};

  // --- Camera ---
  int cameraDeviceIndex {0};
  int cameraWidth {1920};
  int cameraHeight {1080};
};

struct AppSettingsManager {
  static AppSettings load(const std::string &filePath);
  static bool save(const AppSettings &settings, const std::string &filePath);
};
