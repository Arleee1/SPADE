// File: gridLogger.cpp
// PIMeval Simulator - Grid Logger
// Copyright (c) 2026 University of Virginia
// This file is licensed under the MIT License.
// See the LICENSE file in the root of this repository for more details.

#include "gridLogger.h"

#include <cstdio>
#include <fstream>

bool
logGridCoreLocations2D(
	const std::vector<GridCoreLocationTuple>& coreLocations,
	size_t numRows,
	size_t numCols,
	const std::string& fileName)
{
	const size_t expectedSize = numRows * numCols;
	if (coreLocations.size() != expectedSize) {
		std::printf(
			"PIM-Error: logGridCoreLocations2D: core location count mismatch (%zu provided vs %zu expected)\n",
			coreLocations.size(), expectedSize);
		return false;
	}

	std::ofstream output(fileName, std::ios::out | std::ios::trunc);
	if (!output.is_open()) {
		std::printf("PIM-Error: logGridCoreLocations2D: cannot open output file %s\n", fileName.c_str());
		return false;
	}

	for (size_t row = 0; row < numRows; ++row) {
		for (size_t col = 0; col < numCols; ++col) {
			const auto& [rank, chip, bank, subarray] = coreLocations[row * numCols + col];
      if(chip != 0) {
        std::printf("PIM-Error: logGridCoreLocations2D: Unsupported multi-chip configuration for stencil-based allocation strategy\n");
        return false;
      }
			output << "[rank=" << rank
						 << ", bank=" << bank
						 << ", chip=" << chip;
			if (subarray.has_value()) {
				output << ", subarray=" << *subarray;
			}
			output << "]";

			if (col + 1 < numCols) {
				output << ' ';
			}
		}
		output << '\n';
	}

	return true;
}
