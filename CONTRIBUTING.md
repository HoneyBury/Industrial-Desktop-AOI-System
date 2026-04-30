# Contributing Guide

## Goal

This repository demonstrates an industrial AOI desktop software architecture with professional
engineering workflow. Contributions should preserve modular boundaries, testability, and
interview readability.

## Workflow

1. Create or refine a GitHub Issue.
2. Branch from `develop` using `feature/*`, `bugfix/*`, or `hotfix/*`.
3. Keep commits aligned with Conventional Commits.
4. Open a Pull Request to `develop` unless the change is a hotfix.
5. Ensure CI passes and update documentation when behavior changes.

## Commit Convention

- `feat:` new feature
- `fix:` bug fix
- `docs:` documentation
- `test:` tests
- `refactor:` refactor
- `ci:` CI/CD
- `chore:` maintenance

## Local Build

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
```

## Coding Rules

- Follow C++20 and the repository `.clang-format` / `.clang-tidy`.
- Keep UI, motion, vision, and data layers decoupled.
- Prefer interfaces for hardware-related modules.
- Add or update tests for logic changes.
- Update `docs/` for workflow, architecture, or interface changes.

