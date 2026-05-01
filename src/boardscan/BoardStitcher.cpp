#include "boardscan/BoardStitcher.h"

#ifdef AOI_HAS_OPENCV
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#endif

#include <algorithm>
#include <sstream>

BoardScanLayoutResult BoardStitcher::buildLayout(const PlannedBoardScan &plan,
                                                 const int tilePixelWidth,
                                                 const int tilePixelHeight) const {
  if (plan.poses.empty()) {
    return BoardScanLayoutResult::failure("Board scan plan is empty.");
  }
  if (tilePixelWidth <= 0 || tilePixelHeight <= 0) {
    return BoardScanLayoutResult::failure("Tile pixel size must be positive.");
  }

  BoardScanMosaicLayout layout;
  layout.tileRows = plan.tileRows;
  layout.tileColumns = plan.tileColumns;
  layout.tilePixelWidth = tilePixelWidth;
  layout.tilePixelHeight = tilePixelHeight;
  layout.mosaicPixelWidth = plan.tileColumns * tilePixelWidth;
  layout.mosaicPixelHeight = plan.tileRows * tilePixelHeight;
  layout.placements.reserve(plan.poses.size());

  for (const auto &pose : plan.poses) {
    layout.placements.push_back(BoardScanTilePlacement {
        pose,
        pose.column * tilePixelWidth,
        pose.row * tilePixelHeight,
        tilePixelWidth,
        tilePixelHeight,
        {},
    });
  }

  return BoardScanLayoutResult::success(layout, "Board scan layout prepared.");
}

namespace {

#ifdef AOI_HAS_OPENCV

/// Blend a tile into the mosaic with linear alpha blending at the right
/// and bottom edges so that adjacent tiles transition smoothly when
/// overlap exists.
void blendTileIntoMosaic(cv::Mat &mosaic, const cv::Mat &tile,
                         const int px, const int py,
                         const int blendBandPixels) {
  const int tileW = tile.cols;
  const int tileH = tile.rows;
  const int mosaicW = mosaic.cols;
  const int mosaicH = mosaic.rows;

  const int xEnd = std::min(px + tileW, mosaicW);
  const int yEnd = std::min(py + tileH, mosaicH);
  const int roiW = xEnd - px;
  const int roiH = yEnd - py;
  if (roiW <= 0 || roiH <= 0) {
    return;
  }

  cv::Rect roiRect(px, py, roiW, roiH);
  cv::Mat mosaicRoi = mosaic(roiRect);
  cv::Mat tileRoi = tile(cv::Rect(0, 0, roiW, roiH));

  if (blendBandPixels <= 0) {
    tileRoi.copyTo(mosaicRoi);
    return;
  }

  // Build a per-pixel alpha mask: 1.0 in the interior, ramping to 0.0
  // at the right and bottom seam bands so that overlapping neighbours
  // blend together.
  cv::Mat alpha(roiH, roiW, CV_32FC1, cv::Scalar(1.0f));

  const int band = std::min(blendBandPixels, roiW / 2);
  for (int col = roiW - band; col < roiW; ++col) {
    const float weight = static_cast<float>(roiW - col) / static_cast<float>(band);
    for (int row = 0; row < roiH; ++row) {
      alpha.at<float>(row, col) = std::min(alpha.at<float>(row, col), weight);
    }
  }

  const int bandY = std::min(blendBandPixels, roiH / 2);
  for (int row = roiH - bandY; row < roiH; ++row) {
    const float weight = static_cast<float>(roiH - row) / static_cast<float>(bandY);
    for (int col = 0; col < roiW; ++col) {
      alpha.at<float>(row, col) = std::min(alpha.at<float>(row, col), weight);
    }
  }

  for (int row = 0; row < roiH; ++row) {
    for (int col = 0; col < roiW; ++col) {
      const float a = alpha.at<float>(row, col);
      const float invA = 1.0f - a;
      cv::Vec3b &dst = mosaicRoi.at<cv::Vec3b>(row, col);
      const cv::Vec3b &src = tileRoi.at<cv::Vec3b>(row, col);
      dst[0] = cv::saturate_cast<uchar>(a * src[0] + invA * dst[0]);
      dst[1] = cv::saturate_cast<uchar>(a * src[1] + invA * dst[1]);
      dst[2] = cv::saturate_cast<uchar>(a * src[2] + invA * dst[2]);
    }
  }
}

BoardScanCaptureResult stitchWithOpenCV(const BoardScanMosaicLayout &layout,
                                        const std::vector<std::string> &tilePaths,
                                        const std::string &outputPath) {
  if (tilePaths.empty()) {
    return BoardScanCaptureResult {outputPath, layout.tileRows, layout.tileColumns,
                                   0, "No tile images provided.", {}};
  }

  cv::Mat mosaic(layout.mosaicPixelHeight, layout.mosaicPixelWidth,
                 CV_8UC3, cv::Scalar(30, 30, 30));

  std::vector<CapturedFovTile> capturedTiles;
  capturedTiles.reserve(tilePaths.size());

  int placedCount = 0;
  for (std::size_t i = 0; i < tilePaths.size() && i < layout.placements.size(); ++i) {
    const auto &placement = layout.placements[i];
    const auto &path = tilePaths[i];

    cv::Mat tile = cv::imread(path, cv::IMREAD_COLOR);
    if (tile.empty()) {
      continue;
    }

    // Resize tile to match the expected pixel dimensions if necessary.
    if (tile.cols != layout.tilePixelWidth || tile.rows != layout.tilePixelHeight) {
      cv::resize(tile, tile, cv::Size(layout.tilePixelWidth, layout.tilePixelHeight),
                 0.0, 0.0, cv::INTER_LINEAR);
    }

    // Blend band of 8 pixels at right/bottom seams to smooth transitions.
    blendTileIntoMosaic(mosaic, tile, placement.pixelX, placement.pixelY, 8);

    capturedTiles.push_back(CapturedFovTile {placement.pose, path});
    ++placedCount;
  }

  if (placedCount == 0) {
    return BoardScanCaptureResult {outputPath, layout.tileRows, layout.tileColumns,
                                   0, "No tiles could be placed in the mosaic.", {}};
  }

  if (!cv::imwrite(outputPath, mosaic)) {
    std::ostringstream ss;
    ss << "Failed to write mosaic image to " << outputPath << ".";
    return BoardScanCaptureResult {outputPath, layout.tileRows, layout.tileColumns,
                                   placedCount, ss.str(), std::move(capturedTiles)};
  }

  std::ostringstream summary;
  summary << "Mosaic stitched: " << layout.mosaicPixelWidth << "x"
          << layout.mosaicPixelHeight << " px, " << placedCount << " tiles.";
  return BoardScanCaptureResult {outputPath, layout.tileRows, layout.tileColumns,
                                 placedCount, summary.str(), std::move(capturedTiles)};
}

#else // !AOI_HAS_OPENCV

BoardScanCaptureResult stitchStub(const BoardScanMosaicLayout &layout,
                                  const std::vector<std::string> & /*tilePaths*/,
                                  const std::string &outputPath) {
  return BoardScanCaptureResult {outputPath, layout.tileRows, layout.tileColumns, 0,
                                 "OpenCV is not available. Cannot stitch mosaic image.", {}};
}

#endif

} // namespace

BoardScanCaptureResult
BoardStitcher::stitch(const BoardScanMosaicLayout &layout,
                      const std::vector<CapturedFovTile> &tiles,
                      const std::string &outputPath) const {
  std::vector<std::string> paths;
  paths.reserve(tiles.size());
  for (const auto &tile : tiles) {
    paths.push_back(tile.imagePath);
  }
  return stitchFromPaths(layout, paths, outputPath);
}

BoardScanCaptureResult
BoardStitcher::stitchFromPaths(const BoardScanMosaicLayout &layout,
                               const std::vector<std::string> &tileImagePaths,
                               const std::string &outputPath) const {
#ifdef AOI_HAS_OPENCV
  return stitchWithOpenCV(layout, tileImagePaths, outputPath);
#else
  return stitchStub(layout, tileImagePaths, outputPath);
#endif
}
