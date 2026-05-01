#pragma once

#include <string>
#include <vector>

enum class ProcessStepType {
  LoadBoard,
  RoughPosition,
  ImageCapture,
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

struct WorkflowStepRunResult {
  bool advanced {false};
  bool boardCompleted {false};
  bool boardOk {false};
  int nextStepIndex {0};
  StepExecutionRecord record;
  WorkflowRunResult boardResult;
};
