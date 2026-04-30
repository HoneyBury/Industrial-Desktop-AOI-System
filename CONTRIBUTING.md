# 贡献指南

## 目标

本仓库用于展示工业 AOI 桌面软件的架构设计与工程化能力。所有贡献都应尽量保持以下特征：

- 模块边界清晰
- 易于测试
- 文档可读性强
- 适合面试展示与后续继续扩展

## 开发流程

1. 先创建或完善 GitHub Issue。
2. 从 `develop` 拉出 `feature/*`、`bugfix/*` 或 `hotfix/*` 分支。
3. 保持一次提交只解决一个相对清晰的问题。
4. 使用 Conventional Commits 提交信息。
5. 除紧急修复外，默认向 `develop` 发起 Pull Request。
6. 行为变化、接口变化或架构变化时，同步更新文档。

## Commit 规范

- `feat:` 新功能
- `fix:` 修复问题
- `docs:` 文档修改
- `test:` 测试相关
- `refactor:` 重构
- `ci:` CI/CD
- `chore:` 杂项维护

## 本地构建

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
```

## 编码规范

- 遵循 C++20 规范以及仓库中的 `.clang-format` / `.clang-tidy`
- 保持 UI、运动、视觉、数据层解耦
- 涉及硬件的能力优先通过抽象接口建模
- 逻辑变更应补充或更新测试
- 文档、流程说明、架构说明默认使用中文
- 代码注释只在必要时添加，并尽量使用简洁中文说明设计意图

## Pull Request 要求

- 说明业务背景和改动目的
- 说明主要实现点
- 提供测试结果
- 说明风险与兼容性影响
- 如涉及 UI，请提供截图或演示视频
