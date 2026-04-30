# 开发流程

## 端到端流程

Issue -> feature 分支 -> commit -> Pull Request -> CI 检查 -> Code Review -> 合并到 `develop` -> release 分支 -> 合并到 `main` -> tag 发布

## 工作规则

1. 从 GitHub Issue 开始，或者先补齐已有 Issue 的描述
2. 从 `develop` 拉取开发分支
3. 保持一次改动聚焦一个明确目标
4. 使用 Conventional Commits
5. 逻辑变更应同步补充测试
6. 合并前必须通过 CI
7. 架构、接口、流程发生变化时必须同步更新文档

## Pull Request 期望

- 说明业务背景
- 总结技术改动
- 提供测试证据
- 说明风险与兼容性影响
- 涉及界面变化时提供截图或视频
