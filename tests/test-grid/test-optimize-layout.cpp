// Test: Optimize layout search helper
// Copyright (c) 2026 University of Virginia
// This file is licensed under the MIT License.
// See the LICENSE file in the root of this repository for more details.

#include <exception>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

constexpr bool kShowTop10Configurations = false;

// Exercise optimizer APIs and print ranked layout summaries for each scenario.
#include "gridLayoutOptimizer.h"

struct OptimizeScenario {
  std::string name;
  TotalMoveCostParams params;
};

int main()
{
  std::cout << "PIM Regression Test: Optimize layout" << std::endl;

  const std::vector<OptimizeScenario> scenarios{
      {
          "Baseline square blocks",
          {
              100,   // subarrayBlockWidth
              100,   // subarrayBlockHeight
              16,    // subarraysPerBank
              16,    // banksPerRank
              16,    // ranks
              64,    // totalGridWidth (in subarrays)
              64,    // totalGridHeight (in subarrays)
              1,     // radius
              1.0,   // transferCostSubarrayToSubarray
              20.0,  // transferCostBankToBank
              100.0  // transferCostRankToRank
          },
      },
      {
          "Rectangular subarray blocks",
          {
              256,
              64,
              32,
              16,
              8,
              64,
              64,
              1,
              1.0,
              12.0,
              80.0,
          },
      },
      {
          "Larger halo radius",
          {
              128,
              128,
              16,
              32,
              8,
              64,
              64,
              2,
              1.0,
              25.0,
              120.0,
          },
      },
      {
          "Rank-heavy communication penalty",
          {
              64,
              64,
              64,
              16,
              16,
              10,
              50,
              1,
              1.0,
              5.0,
              300.0,
          },
      },
      {
          "Constant communication penalty",
          {
              64,
              64,
              64,
              16,
              16,
              10,
              50,
              1,
              1.0,
              1.0,
              1.0,
          },
      },
        {
          "Non-divisible subarray packing",
          {
            64,
            64,
            16,
            1,
            1,
            5,
            3,
            1,
            1.0,
            20.0,
            100.0,
          },
        },
  };

  bool ok = true;

  for (std::size_t i = 0; i < scenarios.size(); ++i) {
    const OptimizeScenario& scenario = scenarios[i];
    std::cout << "\n===== Scenario " << (i + 1) << ": " << scenario.name << " =====" << std::endl;

    try {
      if (kShowTop10Configurations) {
        const std::vector<GridLayoutConfig> rankedLayouts = rankGridLayouts(scenario.params);
        if (rankedLayouts.empty()) {
          ok = false;
          std::cout << "No valid configurations found" << std::endl;
          continue;
        }

        const GridLayoutConfig& best = rankedLayouts.front();
        std::cout << "OPTIMAL CONFIGURATION:" << std::endl;
        std::cout << "  subarray_grid = " << best.subarrayGridWidth << "x" << best.subarrayGridHeight << std::endl;
        std::cout << "  bank_grid = " << best.bankGridWidth << "x" << best.bankGridHeight << std::endl;
        std::cout << "  rank_grid = " << best.rankGridWidth << "x" << best.rankGridHeight << std::endl;
        std::cout << "  Total move cost = " << best.cost << std::endl;

        std::cout << std::endl;
        std::cout << "Top 10 configurations:" << std::endl;
        const std::size_t topK = std::min<std::size_t>(10, rankedLayouts.size());
        for (std::size_t rank = 0; rank < topK; ++rank) {
          const GridLayoutConfig& config = rankedLayouts[rank];
          std::cout << (rank + 1) << ". Cost=" << config.cost
                    << "  subarray=" << config.subarrayGridWidth << "x" << config.subarrayGridHeight
                    << ", bank=" << config.bankGridWidth << "x" << config.bankGridHeight
                    << ", rank=" << config.rankGridWidth << "x" << config.rankGridHeight << std::endl;
        }
      } else {
        const GridLayoutConfig best = optimizeGridLayout(scenario.params);
        std::cout << "OPTIMAL CONFIGURATION:" << std::endl;
        std::cout << "  subarray_grid = " << best.subarrayGridWidth << "x" << best.subarrayGridHeight << std::endl;
        std::cout << "  bank_grid = " << best.bankGridWidth << "x" << best.bankGridHeight << std::endl;
        std::cout << "  rank_grid = " << best.rankGridWidth << "x" << best.rankGridHeight << std::endl;
        std::cout << "  Total move cost = " << best.cost << std::endl;
      }
    } catch (const std::exception& ex) {
      ok = false;
      std::cout << "Scenario failed with exception: " << ex.what() << std::endl;
    }
  }

  std::cout << "\nOptimize Layout Test " << (ok ? "PASSED" : "FAILED") << std::endl;
  return ok ? 0 : 1;
}
