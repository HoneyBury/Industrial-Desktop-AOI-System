#include <gtest/gtest.h>

#include "boardscan/BoardScanExecutor.h"
#include "boardscan/BoardScanPlanner.h"
#include "boardscan/BoardStitcher.h"
#include "boardscan/BoardViewTransform.h"
#include "motion/VirtualMotionController.h"

TEST(BoardScanPlannerTest, PlansLeftToRightGridWithExpectedCoverage) {
  BoardScanPlanner planner;
  const BoardDefinition board {100.0, 50.0, 30.0};
  const ScanRecipe recipe {40.0, 20.0, ScanOrder::LeftToRight, true};

  const auto result = planner.plan(board, recipe, MechanicalPose {10.0, 20.0, 0.0, 0.0});
  ASSERT_TRUE(result);
  EXPECT_EQ(result.value.tileRows, 3);
  EXPECT_EQ(result.value.tileColumns, 3);
  ASSERT_EQ(result.value.poses.size(), static_cast<std::size_t>(9));
  EXPECT_EQ(result.value.poses.front().row, 0);
  EXPECT_EQ(result.value.poses.front().column, 0);
  EXPECT_EQ(result.value.poses[1].row, 0);
  EXPECT_EQ(result.value.poses[1].column, 1);
  EXPECT_EQ(result.value.poses[3].row, 1);
  EXPECT_EQ(result.value.poses[3].column, 0);
  EXPECT_NEAR(result.value.poses.back().productTopLeftMm.x, 60.0, 1e-9);
  EXPECT_NEAR(result.value.poses.back().productTopLeftMm.y, 30.0, 1e-9);
  EXPECT_NEAR(result.value.poses.front().machinePose.x, 30.0, 1e-9);
  EXPECT_NEAR(result.value.poses.front().machinePose.y, 30.0, 1e-9);
}

TEST(BoardScanPlannerTest, PlansTopToBottomTraversalOrder) {
  BoardScanPlanner planner;
  const BoardDefinition board {90.0, 70.0, 30.0};
  const ScanRecipe recipe {45.0, 25.0, ScanOrder::TopToBottom, true};

  const auto result = planner.plan(board, recipe, MechanicalPose {5.0, 8.0, 0.0, 0.0});
  ASSERT_TRUE(result);
  ASSERT_EQ(result.value.poses.size(), static_cast<std::size_t>(6));
  EXPECT_EQ(result.value.poses[0].row, 0);
  EXPECT_EQ(result.value.poses[0].column, 0);
  EXPECT_EQ(result.value.poses[1].row, 1);
  EXPECT_EQ(result.value.poses[1].column, 0);
  EXPECT_EQ(result.value.poses[2].row, 2);
  EXPECT_EQ(result.value.poses[2].column, 0);
  EXPECT_EQ(result.value.poses[3].row, 0);
  EXPECT_EQ(result.value.poses[3].column, 1);
}

TEST(BoardScanExecutorTest, MovesAxesAndCapturesAllTiles) {
  BoardScanPlanner planner;
  const auto planResult =
      planner.plan(BoardDefinition {80.0, 50.0, 30.0}, ScanRecipe {40.0, 25.0, ScanOrder::LeftToRight, true},
                   MechanicalPose {100.0, 200.0, 3.0, 5.0});
  ASSERT_TRUE(planResult);

  VirtualMotionController controller;
  BoardScanExecutor executor;
  int captureCount = 0;
  const auto executeResult = executor.execute(
      planResult.value, controller,
      [&captureCount](const FovCapturePose &pose) {
        ++captureCount;
        return "tile_" + std::to_string(pose.row) + "_" + std::to_string(pose.column) + ".png";
      });

  ASSERT_TRUE(executeResult);
  EXPECT_EQ(captureCount, 4);
  ASSERT_EQ(executeResult.value.size(), static_cast<std::size_t>(4));
  ASSERT_TRUE(controller.position(MotionAxis::X).has_value());
  ASSERT_TRUE(controller.position(MotionAxis::Y).has_value());
  EXPECT_NEAR(*controller.position(MotionAxis::X), 160.0, 1e-9);
  EXPECT_NEAR(*controller.position(MotionAxis::Y), 237.5, 1e-9);
  EXPECT_NEAR(*controller.position(MotionAxis::Z), 3.0, 1e-9);
  EXPECT_NEAR(*controller.position(MotionAxis::R), 5.0, 1e-9);
}

TEST(BoardStitcherTest, BuildsRuleBasedMosaicLayout) {
  BoardScanPlanner planner;
  const auto planResult =
      planner.plan(BoardDefinition {100.0, 60.0, 30.0}, ScanRecipe {50.0, 30.0, ScanOrder::LeftToRight, true},
                   MechanicalPose {});
  ASSERT_TRUE(planResult);

  BoardStitcher stitcher;
  const auto layoutResult = stitcher.buildLayout(planResult.value, 640, 360);
  ASSERT_TRUE(layoutResult);
  EXPECT_EQ(layoutResult.value.tileRows, 2);
  EXPECT_EQ(layoutResult.value.tileColumns, 2);
  EXPECT_EQ(layoutResult.value.mosaicPixelWidth, 1280);
  EXPECT_EQ(layoutResult.value.mosaicPixelHeight, 720);
  ASSERT_EQ(layoutResult.value.placements.size(), static_cast<std::size_t>(4));
  EXPECT_EQ(layoutResult.value.placements.front().pixelX, 0);
  EXPECT_EQ(layoutResult.value.placements.front().pixelY, 0);
  EXPECT_EQ(layoutResult.value.placements.back().pixelX, 640);
  EXPECT_EQ(layoutResult.value.placements.back().pixelY, 360);
}

TEST(BoardViewTransformTest, ConvertsBetweenSceneAndBoardCoordinates) {
  const BoardSceneLayout layout =
      BoardViewTransform::computeLayout(BoardDefinition {200.0, 100.0, 20.0}, BoardSceneRect {180.0, 120.0, 1320.0, 880.0});
  ASSERT_TRUE(layout.valid);

  const auto boardPoint = BoardViewTransform::sceneToBoardPoint(
      layout, BoardScenePoint {layout.boardRect.x + layout.boardRect.width * 0.25,
                               layout.boardRect.y + layout.boardRect.height * 0.50});
  ASSERT_TRUE(boardPoint.has_value());
  EXPECT_NEAR(boardPoint->x, 50.0, 1e-9);
  EXPECT_NEAR(boardPoint->y, 50.0, 1e-9);

  const auto scenePoint = BoardViewTransform::boardToScenePoint(layout, MillimeterPoint {150.0, 25.0});
  EXPECT_NEAR(scenePoint.x, layout.boardRect.x + layout.boardRect.width * 0.75, 1e-9);
  EXPECT_NEAR(scenePoint.y, layout.boardRect.y + layout.boardRect.height * 0.25, 1e-9);
}

TEST(BoardViewTransformTest, ClampsSceneRectToBoardBounds) {
  const BoardSceneLayout layout =
      BoardViewTransform::computeLayout(BoardDefinition {120.0, 60.0, 20.0}, BoardSceneRect {180.0, 120.0, 1320.0, 880.0});
  ASSERT_TRUE(layout.valid);

  const auto boardRect = BoardViewTransform::sceneToBoardRect(
      layout,
      BoardSceneRect {layout.boardRect.x - 20.0, layout.boardRect.y - 10.0,
                      layout.boardRect.width * 0.50, layout.boardRect.height * 0.50});
  ASSERT_TRUE(boardRect.has_value());
  EXPECT_NEAR(boardRect->x, 0.0, 1e-9);
  EXPECT_NEAR(boardRect->y, 0.0, 1e-9);
  EXPECT_TRUE(boardRect->width > 0.0);
  EXPECT_TRUE(boardRect->height > 0.0);
}
