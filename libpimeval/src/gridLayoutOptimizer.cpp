// File: gridLayoutOptimizer.cpp
// PIMeval Simulator - Grid Layout Optimization
// Copyright (c) 2026 University of Virginia
// This file is licensed under the MIT License.
// See the LICENSE file in the root of this repository for more details.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "gridLayoutOptimizer.h"

std::vector<uint64_t> getDivisors(const uint64_t n) {
  if (n == 0) {
    throw std::invalid_argument("cannot compute divisors for zero");
  }

  std::vector<uint64_t> divisors;
  for (uint64_t i = 1; i <= n / i; ++i) {
    if (n % i == 0) {
      divisors.push_back(i);
      const uint64_t pair = n / i;
      if (pair != i) {
        divisors.push_back(pair);
      }
    }
  }
  std::sort(divisors.begin(), divisors.end());
  return divisors;
}

//! @brief Calculate number of transfers
//! @param gridBlockWidth The number of blocks in the horizontal direction of the grid
//! @param gridBlockHeight The number of blocks in the vertical direction of the grid
//! @param blockWidth The width of each block in elements
//! @param blockHeight The height of each block in elements
//! @param radius The halo radius in elements
uint64_t
getStats(const uint64_t gridBlockWidth,
  const uint64_t gridBlockHeight,
  const uint64_t blockWidth,
  const uint64_t blockHeight,
  const uint64_t radius
) {
  // Elements to transfer between cores at the same y level, e.g., core (i, j) to core (i+1, j)
  // Logic behind formula:
  //    2: At each boundary, we need to transfer data in both directions, e.g., from core (i, j) to core (i+1, j) and from core (i+1, j) to core (i, j)
  //    gridBlockHeight: The total horizontal transfer cost is gridBlockHeight times the cost for one y level
  //    (gridBlockWidth - 1): Each core at y level j needs to transfer data with the core at y level j+1, and there are gridBlockWidth cores at each y level, so there are gridBlockWidth - 1 boundaries between cores at different y levels
  //    radius * (blockHeight - 2 * radius): At each boundary, we need to transfer data within the halo region of width radius, and the height of the halo region is blockHeight - 2 * radius, so the total number of elements to transfer at each boundary is radius * (blockHeight - 2 * radius)
  const uint64_t toMoveHorizontal = 2 * gridBlockHeight * (gridBlockWidth - 1) * radius * (blockHeight - 2 * radius);
  // Elements to transfer between cores at the same x level, e.g., core (i, j) to core (i, j+1)
  // Similar logic as toMoveHorizontal, but flipped directions
  const uint64_t toMoveVertical = 2 * gridBlockWidth * (gridBlockHeight - 1) * radius * (blockWidth - 2 * radius);
  // Elements to transfer between cores diagonalally, e.g., core (i, j) to core (i+1, j+1)
  // Logic behind formula:
  //    4: At each diagonal boundary, we need to transfer data in four directions
  //    (gridBlockWidth - 1) * (gridBlockHeight - 1): There are (gridBlockWidth - 1) * (gridBlockHeight - 1) diagonal boundaries in total
  //    radius * radius: At each diagonal boundary, transfer a square region of size radius * radius
  const uint64_t toMoveDiagonal = 4 * (gridBlockWidth-1) * (gridBlockHeight - 1) * radius * radius;
  const uint64_t toMoveTotal = toMoveHorizontal + toMoveVertical + toMoveDiagonal;
  return toMoveTotal;
}

//! @brief Calculate the total data movement cost for a given blocking configuration and hardware configuration
//! @param subarrayGridWidth The number of subarrays in the horizontal direction of the grid (per bank)
//! @param bankGridWidth The number of banks in the horizontal direction of the grid (per rank)
//! @param rankGridWidth The number of ranks in the horizontal direction of the grid
//! @param params Grouped block, hardware topology, halo, and transfer cost parameters
double totalMoveCost(const uint64_t subarrayGridWidth,
                     const uint64_t bankGridWidth,
                     const uint64_t rankGridWidth,
                     const TotalMoveCostParams& params) {
  if (subarrayGridWidth == 0 || bankGridWidth == 0 || rankGridWidth == 0) {
    throw std::invalid_argument("grid widths must be non-zero");
  }
  if (params.subarraysPerBank % subarrayGridWidth != 0) {
    throw std::invalid_argument("subarraysPerBank must be divisible by subarrayGridWidth");
  }
  if (params.banksPerRank % bankGridWidth != 0) {
    throw std::invalid_argument("banksPerRank must be divisible by bankGridWidth");
  }
  if (params.ranks % rankGridWidth != 0) {
    throw std::invalid_argument("ranks must be divisible by rankGridWidth");
  }

  const uint64_t subarrayGridHeight = params.subarraysPerBank / subarrayGridWidth;
  const uint64_t bankGridHeight = params.banksPerRank / bankGridWidth;
  const uint64_t rankGridHeight = params.ranks / rankGridWidth;

  // Shape of elements at each layer, e.g., bankBlockWidth is the width of elements in each bank
  const uint64_t bankBlockWidth = params.subarrayBlockWidth * subarrayGridWidth;
  const uint64_t bankBlockHeight = params.subarrayBlockHeight * subarrayGridHeight;
  const uint64_t rankBlockWidth = bankBlockWidth * bankGridWidth;
  const uint64_t rankBlockHeight = bankBlockHeight * bankGridHeight;

  const uint64_t toMoveS2S = getStats(subarrayGridWidth, subarrayGridHeight, params.subarrayBlockWidth, params.subarrayBlockHeight, params.radius);
  const uint64_t toMoveB2B = getStats(bankGridWidth, bankGridHeight, bankBlockWidth, bankBlockHeight, params.radius);
  const uint64_t toMoveR2R = getStats(rankGridWidth, rankGridHeight, rankBlockWidth, rankBlockHeight, params.radius);

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
  if (lhs.bankGridWidth != rhs.bankGridWidth) {
    return lhs.bankGridWidth < rhs.bankGridWidth;
  }
  return lhs.rankGridWidth < rhs.rankGridWidth;
}

struct GridDivisors {
  std::vector<uint64_t> subarray;
  std::vector<uint64_t> bank;
  std::vector<uint64_t> rank;
};

GridDivisors getGridDivisors(const TotalMoveCostParams& params) {
  return {
      getDivisors(params.subarraysPerBank),
      getDivisors(params.banksPerRank),
      getDivisors(params.ranks),
  };
}

template <typename ConsumeFn>
void forEachGridLayoutCandidate(const GridDivisors& divisors,
                                const TotalMoveCostParams& params,
                                ConsumeFn consume) {
  bool hasCandidate = false;
  for (const uint64_t subarrayGridWidth : divisors.subarray) {
    for (const uint64_t bankGridWidth : divisors.bank) {
      for (const uint64_t rankGridWidth : divisors.rank) {
        const double cost = totalMoveCost(subarrayGridWidth, bankGridWidth, rankGridWidth, params);
        consume(GridLayoutConfig{cost, subarrayGridWidth, bankGridWidth, rankGridWidth});
        hasCandidate = true;
      }
    }
  }

  if (!hasCandidate) {
    throw std::runtime_error("no valid configurations found");
  }
}

std::vector<GridLayoutConfig> rankGridLayouts(const TotalMoveCostParams& params) {
  const GridDivisors divisors = getGridDivisors(params);

  std::vector<GridLayoutConfig> allResults;
  allResults.reserve(divisors.subarray.size() * divisors.bank.size() * divisors.rank.size());
  forEachGridLayoutCandidate(divisors, params, [&](const GridLayoutConfig& candidate) {
    allResults.push_back(candidate);
  });

  std::sort(allResults.begin(), allResults.end(), lessByCostThenShape);
  return allResults;
}

//! @details heurestic. Assumes that optimal blocking will be rectangular.
GridLayoutConfig optimizeGridLayout(const TotalMoveCostParams& params) {
  const GridDivisors divisors = getGridDivisors(params);

  GridLayoutConfig best{};
  bool hasBest = false;
  forEachGridLayoutCandidate(divisors, params, [&](const GridLayoutConfig& candidate) {
    if (!hasBest || lessByCostThenShape(candidate, best)) {
      best = candidate;
      hasBest = true;
    }
  });

  return best;
}