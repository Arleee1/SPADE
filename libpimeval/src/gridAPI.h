// File: gridAPI.h
// PIMeval Simulator - grid API Interface
// Copyright (c) 2026 University of Virginia
// This file is licensed under the MIT License.
// See the LICENSE file in the root of this repository for more details.

#include <cstdint>
#include <vector>
#include <cstdlib>
#include <cassert>
#include "libpimeval.h"


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Example usage of the above APIs
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void copyHaloToAdjacent(PimObjGrid& grid) {
    //! @todo Would copy halo regions between adjacent cores in the grid
    //! Could be implemented using fixed strategies/enums
}

void stencilIteration(PimObjGrid& grid) {
    //! @todo Would perform a stencil iteration across all cores in parallel, would use existing stencil code
}

void gameOfLifeIteration(PimObjGrid& grid) {
    //! @todo Would perform a Game of Life iteration across all cores in parallel, would use existing Game of Life code
}

void exampleStencilUsage() {
    PimStatus status;

    const uint64_t srcWidth = 1024;
    const uint64_t srcHeight = 1024;
    const uint64_t tileWidth = 256;
    const uint64_t tileHeight = 256;
    const uint64_t numIterations = 10;

    const uint64_t numCoresVertical = srcHeight / tileHeight; // 4
    const uint64_t numCoresHorizontal = srcWidth / tileWidth; // 4


    float* src = (float*) std::malloc(srcWidth * srcHeight * sizeof(float)); // Example source data
    float* dest = (float*) std::malloc(srcWidth * srcHeight * sizeof(float)); // Example destination data

    // 4x4 grid of cores, each core with 256x256 elements plus 1-element halo on each side
    PimObjGrid grid = pimAllocGrid(PIM_ALLOC_AUTO, PIM_FP32, numCoresVertical, numCoresHorizontal,
                                    tileHeight + 2, tileWidth + 2, PIM_ALLOCATION_STRATEGY_STENCIL_9_POINT);
    // Copy to PIM, skipping halo regions
    PimStatus status = pimCopyHostToGrid(src, grid, 1, tileWidth + 1, 1, tileHeight + 1);
    // Fill the halo regions with data from adjacent cores
    pimCopyGridHalo(grid, 1);
    for(size_t iter = 0; iter < numIterations; ++iter) {
        stencilIteration(grid); // (Nearly) identical to existing stencil code, but parallel across cores
        if(iter < numIterations - 1) { // Only need to copy halo if not the last iteration
            pimCopyGridHalo(grid, 1);
        }
    }
    pimCopyGridToHost(grid, dest, 1, tileWidth + 1, 1, tileHeight + 1);

    // Only copy back the non-halo region
    status = pimCopyGridToHost(grid, dest, 1, tileWidth + 1, 1, tileHeight + 1);
    assert(status == PIM_OK);

    // dest should now have the results of the stencil computation

    status = pimFreeGrid(grid);
    assert(status == PIM_OK);

    std::free(src);
    std::free(dest);
}

void exampleGameOfLifeUsage() {
    PimStatus status;

    const uint64_t srcWidth = 1024;
    const uint64_t srcHeight = 1024;
    const uint64_t tileWidth = 256;
    const uint64_t tileHeight = 256;
    const uint64_t numIterations = 10;

    const uint64_t numCoresVertical = srcHeight / tileHeight; // 4
    const uint64_t numCoresHorizontal = srcWidth / tileWidth; // 4

    // Allocate 1-element halo on each side
    // Use PIM_BOOL for Game of Life
    PimObjGrid grid = pimAllocGrid(PIM_ALLOC_AUTO, PIM_BOOL, numCoresVertical, numCoresHorizontal,
                                    tileHeight + 2, tileWidth + 2, PIM_ALLOCATION_STRATEGY_GAME_OF_LIFE);
    assert(grid.size() > 0);

    // Stores sums during GOL computation
    PimObjGrid sumAccumulator = pimAllocGridAssociated(grid[0], PIM_UINT8, 1);
    assert(sumAccumulator.size() > 0);

    float* src = (float*) std::malloc(srcWidth * srcHeight * sizeof(float)); // Example source data

    // Copy to PIM, skipping halo regions
    PimStatus status = pimCopyHostToGrid(src, grid, 1, tileWidth + 1, 1, tileHeight + 1);

    // Fill the halo regions with data from adjacent cores
    copyHaloToAdjacent(grid);

    for(size_t iter = 0; iter < numIterations; ++iter) {
        gameOfLifeIteration(grid);

        if(iter < numIterations - 1) { // Only need to copy halo if not the last iteration
            copyHaloToAdjacent(grid);
        }
    }

    float* dest = (float*) std::malloc(srcWidth * srcHeight * sizeof(float)); // Example destination data

    // Only copy back the non-halo region
    status = pimCopyGridToHost(grid, dest, 1, tileWidth + 1, 1, tileHeight + 1);
    assert(status == PIM_OK);

    status = pimFreeGrid(grid);
    assert(status == PIM_OK);

    std::free(src);
    std::free(dest);
}