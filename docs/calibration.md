# Calibration Design

The calibration module models a typical industrial AOI flow:

- chessboard image acquisition
- camera intrinsic parameter estimation
- distortion compensation
- pixel-to-millimeter mapping
- persistence of calibration parameters

In the bootstrap version, the calibrator returns a deterministic placeholder calibration result so
that UI, program storage, and tests can evolve before full OpenCV calibration logic is introduced.

