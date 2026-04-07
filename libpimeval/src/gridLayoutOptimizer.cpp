// File: gridLayoutOptimizer.cpp
// PIMeval Simulator - Grid Layout Optimization
// Copyright (c) 2026 University of Virginia
// This file is licensed under the MIT License.
// See the LICENSE file in the root of this repository for more details.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include "gridLayoutOptimizer.h"

struct LayoutPair {
  uint64_t width;
  uint64_t height;
};

bool lessByWidthThenHeight(const LayoutPair& lhs, const LayoutPair& rhs) {
  if (lhs.width != rhs.width) {
    return lhs.width < rhs.width;
  }
  return lhs.height < rhs.height;
}

std::vector<LayoutPair> getPossibleLayouts(const uint64_t capacity) {
  if (capacity == 0) {
    throw std::invalid_argument("cannot compute layouts for zero capacity");
  }

  std::vector<LayoutPair> layouts;
  for (uint64_t width = 1; width <= capacity; ++width) {
    const uint64_t maxHeight = capacity / width;
    for (uint64_t height = 1; height <= maxHeight; ++height) {
      layouts.push_back({width, height});
    }
  }

  std::sort(layouts.begin(), layouts.end(), lessByWidthThenHeight);
  layouts.erase(
      std::unique(layouts.begin(), layouts.end(), [](const LayoutPair& lhs, const LayoutPair& rhs) {
        return lhs.width == rhs.width && lhs.height == rhs.height;
      }),
      layouts.end());
  return layouts;
}

bool isLayoutWithinCapacity(const uint64_t width,
                            const uint64_t height,
                            const uint64_t capacity) {
  if (width == 0 || height == 0 || capacity == 0) {
    return false;
  }
  return width <= capacity && height <= (capacity / width);
}

//! @brief Calculate number of transferred elements from neighbor-pair counts
//! @param horizontalNeighborPairs Number of horizontal neighboring block pairs
//! @param verticalNeighborPairs Number of vertical neighboring block pairs
//! @param diagonalNeighborPairs Number of diagonal neighboring block pairs
//! @param blockWidth The width of each block in elements
//! @param blockHeight The height of each block in elements
//! @param radius The halo radius in elements
uint64_t
getStats(const uint64_t horizontalNeighborPairs,
  const uint64_t verticalNeighborPairs,
  const uint64_t diagonalNeighborPairs,
  const uint64_t blockWidth,
  const uint64_t blockHeight,
  const uint64_t radius
) {
  const uint64_t toMoveHorizontal = 2 * horizontalNeighborPairs * radius * (blockHeight - 2 * radius);
  const uint64_t toMoveVertical = 2 * verticalNeighborPairs * radius * (blockWidth - 2 * radius);
  const uint64_t toMoveDiagonal = 4 * diagonalNeighborPairs * radius * radius;
  const uint64_t toMoveTotal = toMoveHorizontal + toMoveVertical + toMoveDiagonal;
  return toMoveTotal;
}

struct BoundaryPairCounts {
  uint64_t horizontalSubarray;
  uint64_t horizontalBank;
  uint64_t horizontalRank;
  uint64_t verticalSubarray;
  uint64_t verticalBank;
  uint64_t verticalRank;
  uint64_t diagonalSubarray;
  uint64_t diagonalBank;
  uint64_t diagonalRank;
};

BoundaryPairCounts countBoundaryPairsByLevel(const uint64_t totalGridWidth,
                                             const uint64_t totalGridHeight,
                                             const uint64_t subarrayGridWidth,
                                             const uint64_t subarrayGridHeight,
                                             const uint64_t bankGridWidth,
                                             const uint64_t bankGridHeight) {
  if (totalGridWidth == 0 || totalGridHeight == 0) {
    throw std::invalid_argument("total grid dimensions must be non-zero");
  }
  if (subarrayGridWidth == 0 || subarrayGridHeight == 0 || bankGridWidth == 0 || bankGridHeight == 0) {
    throw std::invalid_argument("grid dimensions must be non-zero");
  }

  const uint64_t widthEdges = totalGridWidth - 1;
  const uint64_t heightEdges = totalGridHeight - 1;

  const uint64_t bankTileWidth = subarrayGridWidth;
  const uint64_t bankTileHeight = subarrayGridHeight;
  const uint64_t rankTileWidth = bankGridWidth * bankTileWidth;
  const uint64_t rankTileHeight = bankGridHeight * bankTileHeight;

  const uint64_t rankBoundaryColumns = (rankTileWidth == 0) ? 0 : (widthEdges / rankTileWidth);
  const uint64_t bankBoundaryColumnsTotal = (bankTileWidth == 0) ? 0 : (widthEdges / bankTileWidth);
  const uint64_t bankBoundaryColumns = bankBoundaryColumnsTotal - rankBoundaryColumns;
  const uint64_t subarrayBoundaryColumns = widthEdges - bankBoundaryColumnsTotal;

  const uint64_t rankBoundaryRows = (rankTileHeight == 0) ? 0 : (heightEdges / rankTileHeight);
  const uint64_t bankBoundaryRowsTotal = (bankTileHeight == 0) ? 0 : (heightEdges / bankTileHeight);
  const uint64_t bankBoundaryRows = bankBoundaryRowsTotal - rankBoundaryRows;
  const uint64_t subarrayBoundaryRows = heightEdges - bankBoundaryRowsTotal;

  const uint64_t horizontalRank = totalGridHeight * rankBoundaryColumns;
  const uint64_t horizontalBank = totalGridHeight * bankBoundaryColumns;
  const uint64_t horizontalSubarray = totalGridHeight * subarrayBoundaryColumns;

  const uint64_t verticalRank = totalGridWidth * rankBoundaryRows;
  const uint64_t verticalBank = totalGridWidth * bankBoundaryRows;
  const uint64_t verticalSubarray = totalGridWidth * subarrayBoundaryRows;

  const uint64_t totalDiagonalPairs = widthEdges * heightEdges;
  const uint64_t widthEdgesNonRank = widthEdges - rankBoundaryColumns;
  const uint64_t heightEdgesNonRank = heightEdges - rankBoundaryRows;

  const uint64_t diagonalRank =
      rankBoundaryColumns * heightEdges + rankBoundaryRows * widthEdges - rankBoundaryColumns * rankBoundaryRows;
  const uint64_t diagonalBank =
      bankBoundaryColumns * heightEdgesNonRank + bankBoundaryRows * widthEdgesNonRank - bankBoundaryColumns * bankBoundaryRows;
  const uint64_t diagonalSubarray = totalDiagonalPairs - diagonalRank - diagonalBank;

  return {
      horizontalSubarray,
      horizontalBank,
      horizontalRank,
      verticalSubarray,
      verticalBank,
      verticalRank,
      diagonalSubarray,
      diagonalBank,
      diagonalRank,
  };
}

//! @brief Calculate the total data movement cost for a given blocking configuration and hardware configuration
//! @param subarrayGridWidth The number of subarrays in the horizontal direction of the grid (per bank)
//! @param subarrayGridHeight The number of subarrays in the vertical direction of the grid (per bank)
//! @param bankGridWidth The number of banks in the horizontal direction of the grid (per rank)
//! @param bankGridHeight The number of banks in the vertical direction of the grid (per rank)
//! @param rankGridWidth The number of ranks in the horizontal direction of the grid
//! @param rankGridHeight The number of ranks in the vertical direction of the grid
//! @param params Grouped block, hardware topology, halo, and transfer cost parameters
double totalMoveCost(const uint64_t subarrayGridWidth,
                     const uint64_t subarrayGridHeight,
                     const uint64_t bankGridWidth,
                     const uint64_t bankGridHeight,
                     const uint64_t rankGridWidth,
                     const uint64_t rankGridHeight,
                     const TotalMoveCostParams& params) {
  if (!isLayoutWithinCapacity(subarrayGridWidth, subarrayGridHeight, params.subarraysPerBank)) {
    throw std::invalid_argument("subarray layout exceeds subarraysPerBank capacity");
  }
  if (!isLayoutWithinCapacity(bankGridWidth, bankGridHeight, params.banksPerRank)) {
    throw std::invalid_argument("bank layout exceeds banksPerRank capacity");
  }
  if (!isLayoutWithinCapacity(rankGridWidth, rankGridHeight, params.ranks)) {
    throw std::invalid_argument("rank layout exceeds ranks capacity");
  }
  if (params.totalGridWidth == 0 || params.totalGridHeight == 0) {
    throw std::invalid_argument("total grid dimensions must be non-zero");
  }
  if (params.radius > std::numeric_limits<uint64_t>::max() / 2) {
    throw std::invalid_argument("radius is too large");
  }

  const uint64_t twoRadius = 2 * params.radius;
  if (params.subarrayBlockWidth < twoRadius || params.subarrayBlockHeight < twoRadius) {
    throw std::invalid_argument("subarray block dimensions must be >= 2 * radius");
  }

  const uint64_t totalLayoutWidthInSubarrays = subarrayGridWidth * bankGridWidth * rankGridWidth;
  const uint64_t totalLayoutHeightInSubarrays = subarrayGridHeight * bankGridHeight * rankGridHeight;
  if (params.totalGridWidth > totalLayoutWidthInSubarrays || params.totalGridHeight > totalLayoutHeightInSubarrays) {
    throw std::invalid_argument("total grid dimensions exceed layout capacity");
  }

  // Count neighbor boundaries over the actually used subarray region, classifying each boundary
  // by the hierarchy link it crosses (subarray, bank, or rank).
  const BoundaryPairCounts pairs = countBoundaryPairsByLevel(
      params.totalGridWidth,
      params.totalGridHeight,
      subarrayGridWidth,
      subarrayGridHeight,
      bankGridWidth,
      bankGridHeight);

  const uint64_t toMoveS2S = getStats(
      pairs.horizontalSubarray,
      pairs.verticalSubarray,
      pairs.diagonalSubarray,
      params.subarrayBlockWidth,
      params.subarrayBlockHeight,
      params.radius);
  const uint64_t toMoveB2B = getStats(
      pairs.horizontalBank,
      pairs.verticalBank,
      pairs.diagonalBank,
      params.subarrayBlockWidth,
      params.subarrayBlockHeight,
      params.radius);
  const uint64_t toMoveR2R = getStats(
      pairs.horizontalRank,
      pairs.verticalRank,
      pairs.diagonalRank,
      params.subarrayBlockWidth,
      params.subarrayBlockHeight,
      params.radius);

  double cost = params.transferCostSubarrayToSubarray * static_cast<double>(toMoveS2S);
  cost += params.transferCostBankToBank * static_cast<double>(toMoveB2B);
  cost += params.transferCostRankToRank * static_cast<double>(toMoveR2R);
  return cost;
}

bool lessByCostThenShape(const GridLayoutConfig& lhs, const GridLayoutConfig& rhs) {
  if (lhs.cost != rhs.cost) {
    return lhs.cost < rhs.cost;
  }
  if (lhs.subarrayGridWidth != rhs.subarrayGridWidth) {
    return lhs.subarrayGridWidth < rhs.subarrayGridWidth;
  }
  if (lhs.subarrayGridHeight != rhs.subarrayGridHeight) {
    return lhs.subarrayGridHeight < rhs.subarrayGridHeight;
  }
  if (lhs.bankGridWidth != rhs.bankGridWidth) {
    return lhs.bankGridWidth < rhs.bankGridWidth;
  }
  if (lhs.bankGridHeight != rhs.bankGridHeight) {
    return lhs.bankGridHeight < rhs.bankGridHeight;
  }
  if (lhs.rankGridWidth != rhs.rankGridWidth) {
    return lhs.rankGridWidth < rhs.rankGridWidth;
  }
  return lhs.rankGridHeight < rhs.rankGridHeight;
}

struct GridLayouts {
  std::vector<LayoutPair> subarray;
  std::vector<LayoutPair> bank;
  std::vector<LayoutPair> rank;
};

GridLayouts getGridLayouts(const TotalMoveCostParams& params) {
  return {
      getPossibleLayouts(params.subarraysPerBank),
      getPossibleLayouts(params.banksPerRank),
      getPossibleLayouts(params.ranks),
  };
}

bool tryMultiply3(const uint64_t a, const uint64_t b, const uint64_t c, uint64_t& out) {
  if (a != 0 && b > std::numeric_limits<uint64_t>::max() / a) {
    return false;
  }
  const uint64_t ab = a * b;
  if (ab != 0 && c > std::numeric_limits<uint64_t>::max() / ab) {
    return false;
  }
  out = ab * c;
  return true;
}

template <typename ConsumeFn>
void forEachGridLayoutCandidate(const GridLayouts& layouts,
                                const TotalMoveCostParams& params,
                                ConsumeFn consume) {
  if (params.totalGridWidth == 0 || params.totalGridHeight == 0) {
    throw std::invalid_argument("total grid dimensions must be non-zero");
  }

  bool hasCandidate = false;
  for (const LayoutPair& subarrayLayout : layouts.subarray) {
    const uint64_t subarrayGridWidth = subarrayLayout.width;
    const uint64_t subarrayGridHeight = subarrayLayout.height;
    for (const LayoutPair& bankLayout : layouts.bank) {
      const uint64_t bankGridWidth = bankLayout.width;
      const uint64_t bankGridHeight = bankLayout.height;
      for (const LayoutPair& rankLayout : layouts.rank) {
        const uint64_t rankGridWidth = rankLayout.width;
        const uint64_t rankGridHeight = rankLayout.height;

        uint64_t totalWidthInSubarrays = 0;
        uint64_t totalHeightInSubarrays = 0;
        if (!tryMultiply3(subarrayGridWidth, bankGridWidth, rankGridWidth, totalWidthInSubarrays) ||
            !tryMultiply3(subarrayGridHeight, bankGridHeight, rankGridHeight, totalHeightInSubarrays)) {
          continue;
        }

        if (totalWidthInSubarrays < params.totalGridWidth ||
            totalHeightInSubarrays < params.totalGridHeight) {
          continue;
        }
        const uint64_t bankTileWidth = subarrayGridWidth;
        const uint64_t bankTileHeight = subarrayGridHeight;
        const uint64_t rankTileWidth = bankGridWidth * bankTileWidth;
        const uint64_t rankTileHeight = bankGridHeight * bankTileHeight;
        const double cost = totalMoveCost(subarrayGridWidth,
                                          subarrayGridHeight,
                                          bankGridWidth,
                                          bankGridHeight,
                                          rankGridWidth,
                                          rankGridHeight,
                                          params);
        consume(GridLayoutConfig{cost,
                                 subarrayGridWidth,
                                 subarrayGridHeight,
                                 bankGridWidth,
                                 bankGridHeight,
                                 rankGridWidth,
                                 rankGridHeight,
                                 bankTileWidth,
                                 bankTileHeight,
                                 rankTileWidth,
                                 rankTileHeight});
        hasCandidate = true;
      }
    }
  }

  if (!hasCandidate) {
    throw std::runtime_error("no valid configurations found");
  }
}

std::vector<GridLayoutConfig> rankGridLayouts(const TotalMoveCostParams& params) {
  const GridLayouts layouts = getGridLayouts(params);

  std::vector<GridLayoutConfig> allResults;
  allResults.reserve(layouts.subarray.size() * layouts.bank.size() * layouts.rank.size());
  forEachGridLayoutCandidate(layouts, params, [&](const GridLayoutConfig& candidate) {
    allResults.push_back(candidate);
  });

  std::sort(allResults.begin(), allResults.end(), lessByCostThenShape);
  return allResults;
}

//! @details heurestic. Assumes that optimal blocking will be rectangular.
GridLayoutConfig optimizeGridLayout(const TotalMoveCostParams& params) {
  const GridLayouts layouts = getGridLayouts(params);

  GridLayoutConfig best{};
  bool hasBest = false;
  forEachGridLayoutCandidate(layouts, params, [&](const GridLayoutConfig& candidate) {
    if (!hasBest || lessByCostThenShape(candidate, best)) {
      best = candidate;
      hasBest = true;
    }
  });

  return best;
}