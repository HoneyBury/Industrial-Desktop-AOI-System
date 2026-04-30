# Interview Demo Script

## Opening

Introduce the project as a desktop industrial AOI software skeleton designed to reflect real
machine vision and motion-control engineering practices, not only algorithm demos.

## Demo Path

1. Show the repository structure and explain the modular split.
2. Open the Qt main window and explain how Mac camera + virtual axes replace unavailable hardware.
3. Walk through calibration, Mark alignment, ROI detection, program management, and AI pipeline docs.
4. Show unit and integration tests.
5. Show GitHub Actions, CodeQL, branch strategy, and code review checklist.

## Closing

Explain the hardware upgrade path:

- replace `UsbCamera` with Hikvision / Dahua adapters
- replace `VirtualMotionController` with a real motion card adapter
- replace stub inference with ONNX Runtime / TensorRT

