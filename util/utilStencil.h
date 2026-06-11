// Util: Stencil utilities
// Copyright (c) 2026 University of Virginia
// This file is licensed under the MIT License.
// See the LICENSE file in the root of this repository for more details.

#ifndef PIM_FUNC_SIM_APPS_UTIL_STENCIL_H
#define PIM_FUNC_SIM_APPS_UTIL_STENCIL_H

#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <span>

#if defined(_OPENMP)
#include <omp.h>
#endif

//! @brief  Stencil pattern types
//! @note   Mirrors the StencilPattern enum in libpimeval.h. Defined here (guarded
//!         by PIM_STENCIL_PATTERN_DEFINED) so baselines can use the same pattern
//!         selectors without pulling in the full libpimeval header. Keep the
//!         enumerator values in sync with libpimeval.h.
#ifndef PIM_STENCIL_PATTERN_DEFINED
#define PIM_STENCIL_PATTERN_DEFINED
enum StencilPattern {
  STENCIL_PATTERN_BOX = 0,
  STENCIL_PATTERN_STAR,
};
#endif

uint64_t calculateStencilAreaInt(const StencilPattern stencilPattern, const uint64_t radius) {
  uint64_t stencilAreaInt;
  if(stencilPattern == STENCIL_PATTERN_BOX) {
    stencilAreaInt = (2 * radius + 1) * (2 * radius + 1);
  } else if(stencilPattern == STENCIL_PATTERN_STAR) {
    stencilAreaInt = 4 * radius + 1;
  } else {
    std::cerr << "Unrecognized stencil pattern!" << std::endl;
    std::exit(1);
  }
  return stencilAreaInt;
}

struct GridPartitioning {
  uint64_t tileHeight;
  uint64_t tileWidth;
  uint64_t tileHeightLast;
  uint64_t tileWidthLast;
  uint64_t numCoresVertical;
  uint64_t numCoresHorizontal;
  uint64_t totalCores;
};

GridPartitioning calculateGridPartitioning(const uint64_t gridWidth, const uint64_t gridHeight, const uint64_t maxAvailableCores,
                                           const uint64_t maxTileWidth, const uint64_t maxTileHeight, const uint64_t numHalo) {

  //! @todo grid: currently seeks to mimize the tile height, then to maximize the number of cores, then to prefer wider core layouts.
  //!               This is a heuristic that this model of PIM should be more efficient horizontally as it has horizontal parallelism.
  //!               However this needs to be tested.

  GridPartitioning partitioning = {};

  bool foundAnyPartitioning = false;
  uint64_t bestMaxTileHeight = std::numeric_limits<uint64_t>::max();
  uint64_t bestTotalCores = 0;

  for (uint64_t candidateTotalCores = 1; candidateTotalCores <= maxAvailableCores; ++candidateTotalCores) {
    for (uint64_t candidateCoresHorizontal = 1; candidateCoresHorizontal <= candidateTotalCores; ++candidateCoresHorizontal) {
      if (candidateTotalCores % candidateCoresHorizontal != 0) {
        continue;
      }

      uint64_t adjustedCoresHorizontal = candidateCoresHorizontal;
      uint64_t adjustedCoresVertical = candidateTotalCores / candidateCoresHorizontal;

      uint64_t candidateTileWidth = (gridWidth + adjustedCoresHorizontal - 1) / adjustedCoresHorizontal;
      uint64_t candidateTileHeight = (gridHeight + adjustedCoresVertical - 1) / adjustedCoresVertical;
      uint64_t candidateTileWidthLast = gridWidth - ((adjustedCoresHorizontal - 1) * candidateTileWidth);
      uint64_t candidateTileHeightLast = gridHeight - ((adjustedCoresVertical - 1) * candidateTileHeight);

      if (candidateTileWidthLast == 0 && adjustedCoresHorizontal > 1) {
        --adjustedCoresHorizontal;
      }
      if (candidateTileHeightLast == 0 && adjustedCoresVertical > 1) {
        --adjustedCoresVertical;
      }

      candidateTileWidth = (gridWidth + adjustedCoresHorizontal - 1) / adjustedCoresHorizontal;
      candidateTileHeight = (gridHeight + adjustedCoresVertical - 1) / adjustedCoresVertical;
      candidateTileWidthLast = gridWidth - ((adjustedCoresHorizontal - 1) * candidateTileWidth);
      candidateTileHeightLast = gridHeight - ((adjustedCoresVertical - 1) * candidateTileHeight);
      if (candidateTileWidthLast == 0) {
        candidateTileWidthLast = candidateTileWidth;
      }
      if (candidateTileHeightLast == 0) {
        candidateTileHeightLast = candidateTileHeight;
      }

      if (candidateTileWidth == 0 || candidateTileHeight == 0 ||
          candidateTileWidthLast == 0 || candidateTileHeightLast == 0) {
        continue;
      }

      if (candidateTileWidth > maxTileWidth || candidateTileHeight > maxTileHeight ||
          candidateTileWidthLast > maxTileWidth || candidateTileHeightLast > maxTileHeight) {
        continue;
      }

      if (candidateTileHeight < 3*numHalo || candidateTileWidth < 3*numHalo) {
        continue;
      }

      const uint64_t adjustedTotalCores = adjustedCoresHorizontal * adjustedCoresVertical;

      const uint64_t candidateMaxTileHeight =
          (candidateTileHeight > candidateTileHeightLast) ? candidateTileHeight : candidateTileHeightLast;
      const bool betterMaxTileHeight = candidateMaxTileHeight < bestMaxTileHeight;
      const bool sameMaxTileHeight = candidateMaxTileHeight == bestMaxTileHeight;
      const bool betterCoreCount = adjustedTotalCores > bestTotalCores;
      const bool sameCoreCount = adjustedTotalCores == bestTotalCores;
      const bool preferWiderCoreLayout = adjustedCoresHorizontal > partitioning.numCoresHorizontal;

      if (!foundAnyPartitioning ||
          betterMaxTileHeight ||
          (sameMaxTileHeight && (betterCoreCount || (sameCoreCount && preferWiderCoreLayout)))) {
        foundAnyPartitioning = true;
        bestMaxTileHeight = candidateMaxTileHeight;
        bestTotalCores = adjustedTotalCores;
        partitioning.totalCores = adjustedTotalCores;
        partitioning.numCoresHorizontal = adjustedCoresHorizontal;
        partitioning.numCoresVertical = adjustedCoresVertical;
        partitioning.tileWidth = candidateTileWidth;
        partitioning.tileHeight = candidateTileHeight;
        partitioning.tileWidthLast = candidateTileWidthLast;
        partitioning.tileHeightLast = candidateTileHeightLast;
      }
    }
  }

  assert(foundAnyPartitioning);
  assert(partitioning.totalCores > 0);
  assert(partitioning.numCoresHorizontal > 0);
  assert(partitioning.numCoresVertical > 0);
  assert(partitioning.tileWidth > 0);
  assert(partitioning.tileHeight > 0);
  assert(partitioning.tileWidthLast > 0);
  assert(partitioning.tileHeightLast > 0);
  assert(partitioning.tileHeight <= maxTileHeight);
  assert(partitioning.tileWidth <= maxTileWidth);
  assert(partitioning.tileHeightLast <= partitioning.tileHeight);
  assert(partitioning.tileWidthLast <= partitioning.tileWidth);
  assert(gridWidth == (partitioning.numCoresHorizontal - 1) * partitioning.tileWidth + partitioning.tileWidthLast);
  assert(gridHeight == (partitioning.numCoresVertical - 1) * partitioning.tileHeight + partitioning.tileHeightLast);
  assert(partitioning.totalCores <= maxAvailableCores);
  assert(partitioning.totalCores == partitioning.numCoresVertical * partitioning.numCoresHorizontal);

  return partitioning;
}

template <typename GridType, typename StencilOp>
void stencilCpu(std::span<GridType> &src, std::span<GridType> &dst, const uint64_t iterations, const uint64_t radius, uint64_t width, uint64_t height, StencilOp &&stencilOp) {
  for(uint64_t iter=1; iter<=iterations; ++iter) {
    // Only compute when stencil is fully in range
    const uint64_t startY = radius*iter;
    const uint64_t endY = height - startY;
    const uint64_t startX = radius*iter;
    const uint64_t endX = width - startX;
    // Only go parallel once there is enough work to amortize the OpenMP
    // fork/join/barrier overhead; small grids run serially to avoid a fixed
    // thread-management floor dominating the measured runtime.
    const uint64_t activeGridPoints =
        (endX > startX && endY > startY) ? (endX - startX) * (endY - startY) : 0;
#if defined(_OPENMP)
#pragma omp parallel for collapse(2) if(activeGridPoints > 100000)
// #pragma omp parallel for collapse(2)
#endif
    for(uint64_t gridY=startY; gridY<endY; ++gridY) {
      for(uint64_t gridX=startX; gridX<endX; ++gridX) {
        dst[gridY * width + gridX] = stencilOp(src, width, gridX, gridY, radius);
      }
    }
    std::swap(src, dst);
  }
  std::swap(src, dst);
}

#endif