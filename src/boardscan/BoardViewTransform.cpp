#include "boardscan/BoardViewTransform.h"

#include <algorithm>

namespace {

double clampValue(const double value, const double minValue, const double maxValue) {
  return std::clamp(value, minValue, maxValue);
}

BoardSceneRect normalizeRect(const BoardSceneRect &rect) {
  const double left = std::min(rect.x, rect.x + rect.width);
  const double top = std::min(rect.y, rect.y + rect.height);
  const double right = std::max(rect.x, rect.x + rect.width);
  const double bottom = std::max(rect.y, rect.y + rect.height);
  return BoardSceneRect {left, top, right - left, bottom - top};
}

} // namespace

BoardSceneLayout BoardViewTransform::computeLayout(const BoardDefinition &boardDefinition,
                                                   const BoardSceneRect &boardAreaRect) {
  BoardSceneLayout layout;
  layout.boardDefinition = boardDefinition;
  layout.boardAreaRect = boardAreaRect;

  if (boardDefinition.boardLengthMm <= 0.0 || boardDefinition.boardWidthMm <= 0.0 ||
      boardAreaRect.width <= 0.0 || boardAreaRect.height <= 0.0) {
    return layout;
  }

  const double scale =
      std::min(boardAreaRect.width / boardDefinition.boardLengthMm, boardAreaRect.height / boardDefinition.boardWidthMm);
  const double boardPixelWidth = boardDefinition.boardLengthMm * scale;
  const double boardPixelHeight = boardDefinition.boardWidthMm * scale;
  layout.boardRect = BoardSceneRect {
      boardAreaRect.x + (boardAreaRect.width - boardPixelWidth) / 2.0,
      boardAreaRect.y + (boardAreaRect.height - boardPixelHeight) / 2.0,
      boardPixelWidth,
      boardPixelHeight,
  };
  layout.pixelsPerMillimeter = scale;
  layout.railHeightPixels = std::clamp(boardDefinition.railWidthMm * scale, 12.0, 90.0);
  layout.valid = true;
  return layout;
}

std::optional<MillimeterPoint> BoardViewTransform::sceneToBoardPoint(const BoardSceneLayout &layout,
                                                                     const BoardScenePoint &scenePoint,
                                                                     const bool clampToBoard) {
  if (!layout.valid || layout.boardRect.width <= 0.0 || layout.boardRect.height <= 0.0) {
    return std::nullopt;
  }

  double sceneX = scenePoint.x;
  double sceneY = scenePoint.y;
  const double boardLeft = layout.boardRect.x;
  const double boardTop = layout.boardRect.y;
  const double boardRight = layout.boardRect.x + layout.boardRect.width;
  const double boardBottom = layout.boardRect.y + layout.boardRect.height;

  if (clampToBoard) {
    sceneX = clampValue(sceneX, boardLeft, boardRight);
    sceneY = clampValue(sceneY, boardTop, boardBottom);
  } else if (sceneX < boardLeft || sceneX > boardRight || sceneY < boardTop || sceneY > boardBottom) {
    return std::nullopt;
  }

  return MillimeterPoint {
      (sceneX - boardLeft) / layout.pixelsPerMillimeter,
      (sceneY - boardTop) / layout.pixelsPerMillimeter,
  };
}

BoardScenePoint BoardViewTransform::boardToScenePoint(const BoardSceneLayout &layout, const MillimeterPoint &boardPoint) {
  if (!layout.valid) {
    return {};
  }

  return BoardScenePoint {
      layout.boardRect.x + boardPoint.x * layout.pixelsPerMillimeter,
      layout.boardRect.y + boardPoint.y * layout.pixelsPerMillimeter,
  };
}

std::optional<BoardSceneRect> BoardViewTransform::sceneToBoardRect(const BoardSceneLayout &layout,
                                                                   const BoardSceneRect &sceneRect,
                                                                   const bool clampToBoard) {
  if (!layout.valid) {
    return std::nullopt;
  }

  const BoardSceneRect normalized = normalizeRect(sceneRect);
  double left = normalized.x;
  double top = normalized.y;
  double right = normalized.x + normalized.width;
  double bottom = normalized.y + normalized.height;
  const double boardLeft = layout.boardRect.x;
  const double boardTop = layout.boardRect.y;
  const double boardRight = layout.boardRect.x + layout.boardRect.width;
  const double boardBottom = layout.boardRect.y + layout.boardRect.height;

  if (clampToBoard) {
    left = clampValue(left, boardLeft, boardRight);
    right = clampValue(right, boardLeft, boardRight);
    top = clampValue(top, boardTop, boardBottom);
    bottom = clampValue(bottom, boardTop, boardBottom);
  } else if (left < boardLeft || right > boardRight || top < boardTop || bottom > boardBottom) {
    return std::nullopt;
  }

  if (right <= left || bottom <= top) {
    return std::nullopt;
  }

  const auto topLeft = sceneToBoardPoint(layout, BoardScenePoint {left, top}, false);
  const auto bottomRight = sceneToBoardPoint(layout, BoardScenePoint {right, bottom}, false);
  if (!topLeft || !bottomRight) {
    return std::nullopt;
  }

  return BoardSceneRect {
      topLeft->x,
      topLeft->y,
      bottomRight->x - topLeft->x,
      bottomRight->y - topLeft->y,
  };
}

BoardSceneRect BoardViewTransform::boardToSceneTopLeftRect(const BoardSceneLayout &layout,
                                                           const MillimeterPoint &topLeftPoint,
                                                           const double widthMm,
                                                           const double heightMm) {
  const MillimeterPoint clampedTopLeft =
      clampTopLeftPoint(layout.boardDefinition, topLeftPoint, std::max(0.0, widthMm), std::max(0.0, heightMm));
  const BoardScenePoint sceneTopLeft = boardToScenePoint(layout, clampedTopLeft);
  return BoardSceneRect {
      sceneTopLeft.x,
      sceneTopLeft.y,
      std::max(0.0, widthMm) * layout.pixelsPerMillimeter,
      std::max(0.0, heightMm) * layout.pixelsPerMillimeter,
  };
}

BoardSceneRect BoardViewTransform::boardToSceneCenteredRect(const BoardSceneLayout &layout,
                                                            const MillimeterPoint &centerPoint,
                                                            const double widthMm,
                                                            const double heightMm) {
  const MillimeterPoint clampedCenter =
      clampCenteredPoint(layout.boardDefinition, centerPoint, std::max(0.0, widthMm), std::max(0.0, heightMm));
  const MillimeterPoint topLeft {
      clampedCenter.x - std::max(0.0, widthMm) / 2.0,
      clampedCenter.y - std::max(0.0, heightMm) / 2.0,
  };
  return boardToSceneTopLeftRect(layout, topLeft, widthMm, heightMm);
}

MillimeterPoint BoardViewTransform::clampCenteredPoint(const BoardDefinition &boardDefinition,
                                                       const MillimeterPoint &centerPoint,
                                                       const double widthMm,
                                                       const double heightMm) {
  const double halfWidth = std::max(0.0, widthMm) / 2.0;
  const double halfHeight = std::max(0.0, heightMm) / 2.0;
  return MillimeterPoint {
      clampValue(centerPoint.x, halfWidth, std::max(halfWidth, boardDefinition.boardLengthMm - halfWidth)),
      clampValue(centerPoint.y, halfHeight, std::max(halfHeight, boardDefinition.boardWidthMm - halfHeight)),
  };
}

MillimeterPoint BoardViewTransform::clampTopLeftPoint(const BoardDefinition &boardDefinition,
                                                      const MillimeterPoint &topLeftPoint,
                                                      const double widthMm,
                                                      const double heightMm) {
  return MillimeterPoint {
      clampValue(topLeftPoint.x, 0.0, std::max(0.0, boardDefinition.boardLengthMm - std::max(0.0, widthMm))),
      clampValue(topLeftPoint.y, 0.0, std::max(0.0, boardDefinition.boardWidthMm - std::max(0.0, heightMm))),
  };
}
