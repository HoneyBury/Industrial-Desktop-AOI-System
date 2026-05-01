#pragma once

#include "coordinate/CoordinateTransformer.h"
#include "program/ProgramModel.h"

#include <optional>

struct BoardScenePoint {
  double x {0.0};
  double y {0.0};
};

struct BoardSceneRect {
  double x {0.0};
  double y {0.0};
  double width {0.0};
  double height {0.0};
};

struct BoardSceneLayout {
  BoardDefinition boardDefinition;
  BoardSceneRect boardAreaRect;
  BoardSceneRect boardRect;
  double pixelsPerMillimeter {1.0};
  double railHeightPixels {0.0};
  bool valid {false};
};

class BoardViewTransform {
public:
  [[nodiscard]] static BoardSceneLayout computeLayout(const BoardDefinition &boardDefinition,
                                                      const BoardSceneRect &boardAreaRect);

  [[nodiscard]] static std::optional<MillimeterPoint> sceneToBoardPoint(const BoardSceneLayout &layout,
                                                                        const BoardScenePoint &scenePoint,
                                                                        bool clampToBoard = true);
  [[nodiscard]] static BoardScenePoint boardToScenePoint(const BoardSceneLayout &layout,
                                                         const MillimeterPoint &boardPoint);

  [[nodiscard]] static std::optional<BoardSceneRect> sceneToBoardRect(const BoardSceneLayout &layout,
                                                                      const BoardSceneRect &sceneRect,
                                                                      bool clampToBoard = true);
  [[nodiscard]] static BoardSceneRect boardToSceneTopLeftRect(const BoardSceneLayout &layout,
                                                              const MillimeterPoint &topLeftPoint,
                                                              double widthMm,
                                                              double heightMm);
  [[nodiscard]] static BoardSceneRect boardToSceneCenteredRect(const BoardSceneLayout &layout,
                                                               const MillimeterPoint &centerPoint,
                                                               double widthMm,
                                                               double heightMm);

  [[nodiscard]] static MillimeterPoint clampCenteredPoint(const BoardDefinition &boardDefinition,
                                                          const MillimeterPoint &centerPoint,
                                                          double widthMm,
                                                          double heightMm);
  [[nodiscard]] static MillimeterPoint clampTopLeftPoint(const BoardDefinition &boardDefinition,
                                                         const MillimeterPoint &topLeftPoint,
                                                         double widthMm,
                                                         double heightMm);
};
