// Util: Grid utilities
// Copyright (c) 2026 University of Virginia
// This file is licensed under the MIT License.
// See the LICENSE file in the root of this repository for more details.

#ifndef PIM_FUNC_SIM_APPS_UTIL_GRID_H
#define PIM_FUNC_SIM_APPS_UTIL_GRID_H

#include <cassert>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include "libpimeval.h"

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

//! @brief  Sums the neighbors of each element in a stencil row to compute the horizontal stencil sum
//!
//! Sums radius number of elemements to the left and right of center element, including center element
//! Puts each result pimRowSum[i] where i is the center index
//! Formula: pimRowSum[i] = Σ (j ∈ [i-radius, i+radius]) mid[j]
//! Works by shifting mid to the left and right and adding shifted versions
//! @param[in]  mid  PIM row to be summed
//! @param[out]  pimRowSum  The resultant PIM object to place the sum into
//! @param[in,out]  shiftBackup  Temporary PIM object used for calculations
//! @param[in]  radius  The stencil radius
void sumStencilRow(PimObjId mid, PimObjId pimRowSum, PimObjId shiftBackup, const uint64_t radius) {
  PimStatus status;

  if(radius == 0) {
    return;
  }

  status = pimCopyObjectToObject(mid, shiftBackup);
  assert (status == PIM_OK);

  status = pimShiftElementsRight(shiftBackup);
  assert (status == PIM_OK);

  status = pimAdd(mid, shiftBackup, pimRowSum);
  assert (status == PIM_OK);

  for(uint64_t shiftIter=1; shiftIter<radius; ++shiftIter) {
    status = pimShiftElementsRight(shiftBackup);
    assert (status == PIM_OK);

    status = pimAdd(pimRowSum, shiftBackup, pimRowSum);
    assert (status == PIM_OK);
  }

  status = pimCopyObjectToObject(mid, shiftBackup);
  assert (status == PIM_OK);

  for(uint64_t shiftIter=0; shiftIter<radius; ++shiftIter) {
    status = pimShiftElementsLeft(shiftBackup);
    assert (status == PIM_OK);

    status = pimAdd(pimRowSum, shiftBackup, pimRowSum);
    assert (status == PIM_OK);
  }
}

template <typename GridType, typename StencilOp>
void stencilCpu(std::span<GridType> &src, std::span<GridType> &dst, const uint64_t iterations, const uint64_t radius, uint64_t width, uint64_t height, StencilOp &&stencilOp) {
  for(uint64_t iter=1; iter<=iterations; ++iter) {
    // Only compute when stencil is fully in range
    const uint64_t startY = radius*iter;
    const uint64_t endY = height - startY;
    const uint64_t startX = radius*iter;
    const uint64_t endX = width - startX;
#if defined(_OPENMP)
#pragma omp parallel for collapse(2)
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