// File: gridLayoutOptimizer.h
// PIMeval Simulator - Grid Layout Optimization
// Copyright (c) 2026 University of Virginia
// This file is licensed under the MIT License.
// See the LICENSE file in the root of this repository for more details.

#ifndef PIMEVAL_GRID_LAYOUT_OPTIMIZER_H
#define PIMEVAL_GRID_LAYOUT_OPTIMIZER_H

#include <cstdint>
#include <vector>

//! @brief Additional parameters for totalMoveCost beyond grid widths
struct TotalMoveCostParams {
  //! The width of the block of data mapped to each subarray
  uint64_t subarrayBlockWidth;
  //! The height of the block of data mapped to each subarray
  uint64_t subarrayBlockHeight;
  //! The total number of subarrays in each bank
  uint64_t subarraysPerBank;
  //! The total number of banks in each rank
  uint64_t banksPerRank;
  //! The total number of ranks in the system
  uint64_t ranks;
  //! The target total grid width in subarray units
  uint64_t totalGridWidth;
  //! The target total grid height in subarray units
  uint64_t totalGridHeight;
  //! The halo radius for the stencil computation
  uint64_t radius;
  //! The cost to transfer one element between subarrays
  double transferCostSubarrayToSubarray;
  //! The cost to transfer one element between banks
  double transferCostBankToBank;
  //! The cost to transfer one element between ranks
  double transferCostRankToRank;
};

struct GridLayoutConfig {
  double cost;
  uint64_t subarrayGridWidth;
  uint64_t subarrayGridHeight;
  uint64_t bankGridWidth;
  uint64_t bankGridHeight;
  uint64_t rankGridWidth;
  uint64_t rankGridHeight;
  uint64_t bankTileWidth;
  uint64_t bankTileHeight;
  uint64_t rankTileWidth;
  uint64_t rankTileHeight;
};

//! @brief Enumerate all valid layouts sorted by cost in ascending order.
std::vector<GridLayoutConfig> rankGridLayouts(const TotalMoveCostParams& params);

//! @details heurestic. Assumes that optimal blocking will be rectangular.
GridLayoutConfig optimizeGridLayout(const TotalMoveCostParams& params);

#endif // PIMEVAL_GRID_LAYOUT_OPTIMIZER_H