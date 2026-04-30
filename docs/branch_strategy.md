# Git 分支策略

## 分支职责

- `main`：稳定发布分支
- `develop`：日常集成分支
- `feature/*`：功能开发分支
- `bugfix/*`：缺陷修复分支
- `release/*`：发布准备分支
- `hotfix/*`：紧急修复分支

## 合并方向

- `feature/*` -> `develop`
- `bugfix/*` -> `develop`
- `release/*` -> `main`，并回合并到 `develop`
- `hotfix/*` -> `main`，并回合并到 `develop`

## 发布流程

1. 日常功能先集成到 `develop`
2. 准备发布时创建 `release/*`
3. 进入范围冻结与回归验证阶段
4. 发布完成后合并到 `main`
5. 创建 `v*` Tag
6. 将发布分支改动回合并到 `develop`
