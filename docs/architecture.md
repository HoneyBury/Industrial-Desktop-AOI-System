# Architecture Overview

## Project Positioning

This repository demonstrates a desktop industrial AOI software architecture for interview and
portfolio use. The system emulates a real AOI workstation with machine vision, motion control,
inspection program management, database traceability, and AI deployment hooks.

## Layered Design

- `UI Layer` handles operator workflow, program editing, calibration dialogs, and demo visibility.
- `Application Layer` coordinates program lifecycle, inspection orchestration, and user actions.
- `Domain Layer` encapsulates vision, motion, coordinate transformation, and inspection rules.
- `Infrastructure Layer` integrates camera access, SQLite persistence, AI runtime, and CI/CD.

## Core Modules

- `camera`: `ICamera` abstracts acquisition devices. `UsbCamera` currently uses a Mac webcam and is
  designed to be extended by future `HikCamera` and `DahuaCamera` adapters.
- `vision`: calibration, Mark detection, ROI extraction, QR reading, and coordinate mapping.
- `motion`: `IMotionController` abstracts motion APIs; `VirtualMotionController` simulates X/Y/Z/R.
- `program`: captures the persistent definition of an AOI recipe.
- `database`: central entry point for program metadata and inspection records.
- `ai`: isolates ONNX model loading and inference lifecycle from UI and vision workflows.
- `ui`: Qt-based workstation shell and operation dialogs.

## Engineering Principles

- Prefer interface-driven hardware abstraction.
- Keep algorithm code testable without a GUI runtime.
- Degrade gracefully when local hardware SDKs are unavailable.
- Preserve a path from interview demo code to production-grade industrial integration.

