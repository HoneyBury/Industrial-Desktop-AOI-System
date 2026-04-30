# CI/CD Strategy

## CI

`ci.yml` runs on pushes to `main` and `develop`, and on pull requests targeting those branches.

Stages:

- install Qt 6, OpenCV, CMake, Ninja
- configure with CMake presets
- build project
- run CTest / GoogleTest
- run non-blocking `clang-tidy`

## Security

`codeql.yml` scans the C++ codebase on:

- push to `main` / `develop`
- pull request to `main`
- weekly scheduled scan

## Release

`release.yml` packages a macOS demo artifact when a tag matching `v*` is pushed.

