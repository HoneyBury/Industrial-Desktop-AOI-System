#pragma once

#include <string>

struct AppSettings {
  // --- 应用配置 ---
  std::string programOpenPath;     // 程序文件默认打开路径
  std::string templateFolderPath;  // 模板文件夹路径
  bool persistLogs = false;        // 是否持久化保存日志
  std::string logFilePath;         // 日志文件持久化路径

  // --- UI 配置 ---
  bool showLogWindow = false;      // 启动时是否自动打开日志窗口

  // --- 运行配置 ---
  std::string workMode = "manual"; // 工作模式: "manual" | "auto"
  bool alarmEnabled = false;       // 是否开启报警
  int maxDefectCount = 10;         // 最大缺陷数阈值
};

struct AppSettingsManager {
  static AppSettings load(const std::string &filePath);
  static bool save(const AppSettings &settings, const std::string &filePath);
};
