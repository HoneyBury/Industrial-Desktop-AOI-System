#pragma once

#include "boardscan/BoardScanTypes.h"

class BoardStitcher {
public:
  [[nodiscard]] BoardScanLayoutResult buildLayout(const PlannedBoardScan &plan,
                                                  int tilePixelWidth,
                                                  int tilePixelHeight) const;

  /// Compose captured tiles into a single mosaic image.  When OpenCV is
  /// available the tiles are read, placed at their layout positions and
  /// blended at overlap seams; otherwise a stub failure is returned.
  /// The output is written to `outputPath` (PNG) and the same path is
  /// returned inside the result on success.
  [[nodiscard]] BoardScanCaptureResult
  stitch(const BoardScanMosaicLayout &layout,
         const std::vector<CapturedFovTile> &tiles,
         const std::string &outputPath) const;

  /// Stitch from individually-provided tile image paths instead of
  /// CapturedFovTile records.  Callers must ensure the paths vector
  /// matches layout.placements in row-major order.
  [[nodiscard]] BoardScanCaptureResult
  stitchFromPaths(const BoardScanMosaicLayout &layout,
                  const std::vector<std::string> &tileImagePaths,
                  const std::string &outputPath) const;
};
