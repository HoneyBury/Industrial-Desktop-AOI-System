# Code Review Checklist

Reviewers should verify:

- 是否符合模块边界
- 是否有测试
- 是否影响已有接口
- 是否有线程安全问题
- 是否有资源释放问题
- 是否有异常处理
- 是否更新文档

## Additional AOI-Specific Focus

- Does camera logic stay isolated from UI code?
- Does motion logic avoid unsafe direct coupling with vision algorithms?
- Are coordinate transform assumptions explicit and testable?
- Are calibration, Mark, ROI, and AI paths traceable in logs?

