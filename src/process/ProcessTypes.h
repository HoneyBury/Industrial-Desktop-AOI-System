#pragma once

#include <string>
#include <vector>

enum class ProcessStepType {
  LoadBoard,
  RoughPosition,
  MarkAlign,
  DefectInspect,
  PreLaser,
  LaserExecute,
  PostLaserVerify,
  OutputResult,
};

enum class StepExecutionStatus {
  Pending,
  Running,
  Succeeded,
  Failed,
  Skipped,
};

enum class StepFailurePolicy {
  StopWorkflow,
  ContinueWorkflow,
};

struct StepExecutionRecord {
  std::string stepId;
  ProcessStepType stepType {ProcessStepType::MarkAlign};
  StepExecutionStatus status {StepExecutionStatus::Pending};
  std::string message;
};

struct WorkflowRunResult {
  bool ok {false};
  std::vector<StepExecutionRecord> records;
  std::string message;
};
