// File: gridLogger.h
// PIMeval Simulator - Grid Logger
// Copyright (c) 2026 University of Virginia
// This file is licensed under the MIT License.
// See the LICENSE file in the root of this repository for more details.

#ifndef PIMEVAL_GRID_LOGGER_H
#define PIMEVAL_GRID_LOGGER_H

#include <cstddef>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

//! Tuple format: (rank, chip, bank, optional subarray)
using GridCoreLocationTuple = std::tuple<unsigned, unsigned, unsigned, std::optional<unsigned>>;

//! @brief Log core locations to a text file laid out as a 2D grid.
bool logGridCoreLocations2D(
	const std::vector<GridCoreLocationTuple>& coreLocations,
	size_t numRows,
	size_t numCols,
	const std::string& fileName);

#endif // PIMEVAL_GRID_LOGGER_H
