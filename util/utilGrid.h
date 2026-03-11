// Util: Grid utilities
// Copyright (c) 2026 University of Virginia
// This file is licensed under the MIT License.
// See the LICENSE file in the root of this repository for more details.

#ifndef PIM_FUNC_SIM_APPS_UTIL_GRID_H
#define PIM_FUNC_SIM_APPS_UTIL_GRID_H

#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include "libpimeval.h"

struct GridPartitioning {
  uint64_t tileHeight;
  uint64_t tileWidth;
  uint64_t numCoresVertical;
  uint64_t numCoresHorizontal;
  uint64_t totalCores;
};

GridPartitioning calculateGridPartitioning(const uint64_t gridWidth, const uint64_t gridHeight, const uint64_t maxAvailableCores, const uint64_t maxTileWidth, const uint64_t maxTileHeight) {
  //! @todo grid: verify + cleanup -- what if grid doesn't fit nicely?

  GridPartitioning partitioning;

  for (uint64_t candidateTotalCores = maxAvailableCores; candidateTotalCores > 0; --candidateTotalCores) {
    bool foundForThisCoreCount = false;
    uint64_t bestCoreShapeDiff = std::numeric_limits<uint64_t>::max();
    for (uint64_t candidateCoresHorizontal = 1; candidateCoresHorizontal <= candidateTotalCores; ++candidateCoresHorizontal) {
      if (candidateTotalCores % candidateCoresHorizontal != 0) {
        continue;
      }

      const uint64_t candidateCoresVertical = candidateTotalCores / candidateCoresHorizontal;
      if (gridWidth % candidateCoresHorizontal != 0 || gridHeight % candidateCoresVertical != 0) {
        continue;
      }

      const uint64_t candidateTileWidth = gridWidth / candidateCoresHorizontal;
      const uint64_t candidateTileHeight = gridHeight / candidateCoresVertical;
      if (candidateTileWidth > maxTileWidth || candidateTileHeight > maxTileHeight) {
        continue;
      }

      const uint64_t candidateCoreShapeDiff =
          (candidateCoresVertical > candidateCoresHorizontal)
              ? (candidateCoresVertical - candidateCoresHorizontal)
              : (candidateCoresHorizontal - candidateCoresVertical);

      if (!foundForThisCoreCount ||
          candidateCoreShapeDiff < bestCoreShapeDiff ||
          (candidateCoreShapeDiff == bestCoreShapeDiff && candidateCoresHorizontal > partitioning.numCoresHorizontal)) {
        foundForThisCoreCount = true;
        bestCoreShapeDiff = candidateCoreShapeDiff;
        partitioning.totalCores = candidateTotalCores;
        partitioning.numCoresHorizontal = candidateCoresHorizontal;
        partitioning.numCoresVertical = candidateCoresVertical;
        partitioning.tileWidth = candidateTileWidth;
        partitioning.tileHeight = candidateTileHeight;
      }
    }

    if (foundForThisCoreCount) {
      break;
    }
  }

  assert(gridWidth == partitioning.numCoresHorizontal * partitioning.tileWidth);
  assert(gridHeight == partitioning.numCoresVertical * partitioning.tileHeight);
  assert(partitioning.tileHeight <= maxTileHeight);
  assert(partitioning.tileWidth <= maxTileWidth);
  assert(gridHeight % partitioning.tileHeight == 0);
  assert(gridWidth % partitioning.tileWidth == 0);
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