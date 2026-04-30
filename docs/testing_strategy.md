# Testing Strategy

## Test Pyramid

- `Unit tests`: coordinate conversion, motion simulation, mark geometry, utility logic
- `Integration tests`: program management, database wiring, inspection pipeline orchestration
- `Future hardware-in-the-loop`: camera SDK and real motion card verification

## Current Baseline

- Coordinate transformer conversion tests
- Virtual motion absolute and relative movement tests
- Dual-Mark rotation calculation tests
- Bootstrap inspection pipeline test

## Expansion Plan

- calibration fixture regression cases
- ROI/Mark image golden tests
- program serialization tests
- SQLite persistence tests
- AI inference adapter contract tests

