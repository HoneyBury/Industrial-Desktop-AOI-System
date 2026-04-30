# CI/CD 策略

## CI 流程

`ci.yml` 会在以下场景触发：

- push 到 `main`
- push 到 `develop`
- Pull Request 指向 `main` 或 `develop`

主要阶段包括：

- 使用 CMake Preset 配置快速 CI 工程
- 关闭 Qt / OpenCV / SQLite 可选依赖，直接验证核心库与测试
- 编译项目
- 运行 CTest / GoogleTest
- 运行非阻塞模式的 `clang-tidy`

## 发布流程

`release.yml` 会在推送符合 `v*` 规则的 Tag 时触发，并构建 macOS Demo artifact，便于做阶段性展示与归档。
