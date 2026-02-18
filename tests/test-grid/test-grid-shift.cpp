// Test: Grid Shift
// Copyright (c) 2026 University of Virginia
// This file is licensed under the MIT License.
// See the LICENSE file in the root of this repository for more details.

#include "libpimeval.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <bitset>
#include <cassert>
#include <cstdlib>
#include <cinttypes>
#include <cstdio>

#if defined(_OPENMP)
#include <omp.h>
#endif

bool testGridShift(PimDeviceEnum deviceType, bool isLeftShift)
{
  std::cout << "Testing grid shift for device type " << deviceType << ", direction: " << (isLeftShift ? "left" : "right") << std::endl;

  bool ok = true;

  PimStatus status;

  unsigned numRanks = 1;
  unsigned numBankPerRank = 128; // 8 chips * 16 banks
  unsigned numSubarrayPerBank = 32;
  unsigned numRows = 2048;
  unsigned numCols = 8192;

  status = pimCreateDevice(deviceType, numRanks, numBankPerRank, numSubarrayPerBank, numRows, numCols);
  assert(status == PIM_OK);

  const uint64_t srcWidth = 20;
  const uint64_t srcHeight = 20;
  const uint64_t tileWidth = 5;
  const uint64_t tileHeight = 5;

  const uint64_t numCoresVertical = srcHeight / tileHeight;
  const uint64_t numCoresHorizontal = srcWidth / tileWidth;

  const size_t numElements = srcWidth * srcHeight;


  int* src = (int*) std::malloc(numElements * sizeof(int));
  int* dest = (int*) std::malloc(numElements * sizeof(int));

  for(uint64_t i = 0; i < numElements; ++i) {
    src[i] = static_cast<int>(i+1);
    dest[i] = static_cast<int>(i+1);
  }

  PimObjGrid grid = pimAllocGrid(PIM_ALLOC_AUTO, PIM_INT32, numCoresVertical, numCoresHorizontal,
                                  tileHeight, tileWidth, PIM_ALLOCATION_STRATEGY_STENCIL_9_POINT);
  assert(!grid.empty());
  assert(tileHeight == grid.size());

  // Copy to PIM
  status = pimCopyHostToGrid(src, grid);
  assert(status == PIM_OK);

  // Shift elements left or right within each core
  for(uint64_t y = 0; y < tileHeight; ++y) {
    if(isLeftShift) {
      status = pimShiftElementsLeft(grid[y]);
    } else {
      status = pimShiftElementsRight(grid[y]);
    }
    assert(status == PIM_OK);
  }

  // Copy back to host
  status = pimCopyGridToHost(grid, dest);
  assert(status == PIM_OK);

  // Shift elements in src array on host to verify against dest array
  if(isLeftShift) {
    for(uint64_t coreY = 0; coreY < numCoresVertical; ++coreY) {
      for (uint64_t coreX = 0; coreX < numCoresHorizontal; ++coreX) {
        for (uint64_t y = 0; y < tileHeight; ++y) {
          for (uint64_t x = 1; x < tileWidth; ++x) {
            // (x-1, y) <- (x,y)
            const size_t srcIdx = (coreY * tileHeight + y) * srcWidth + (coreX * tileWidth + x);
            const size_t destIdx = (coreY * tileHeight + y) * srcWidth + (coreX * tileWidth + x - 1);
            src[destIdx] = src[srcIdx];
          }
          const size_t destIdx = (coreY * tileHeight + y) * srcWidth + (coreX * tileWidth + tileWidth - 1);
          src[destIdx] = 0; // rightmost element becomes 0 after left shift
        }
      }
    }
  } else {
    for(uint64_t coreY = 0; coreY < numCoresVertical; ++coreY) {
      for (uint64_t coreX = 0; coreX < numCoresHorizontal; ++coreX) {
        for (uint64_t y = 0; y < tileHeight; ++y) {
          for (uint64_t x = tileWidth-1; x > 0; --x) {
            // (x, y) <- (x-1,y)
            const size_t srcIdx = (coreY * tileHeight + y) * srcWidth + (coreX * tileWidth + x - 1);
            const size_t destIdx = (coreY * tileHeight + y) * srcWidth + (coreX * tileWidth + x);
            src[destIdx] = src[srcIdx];
          }
          const size_t destIdx = (coreY * tileHeight + y) * srcWidth + (coreX * tileWidth);
          src[destIdx] = 0; // rightmost element becomes 0 after right shift
        }
      }
    }
  }

  for (uint64_t i = 0; i < numElements; ++i) {
    if (src[i] != dest[i]) {
      std::printf("ERROR: found mismatch at idx %" PRIu64 ": src %d dest %d\n", i, src[i], dest[i]);
      ok = false;
      break;
    }
  }

  std::free(src);
  std::free(dest);
  pimFreeGrid(grid);

  pimShowStats();
  pimDeleteDevice();

  std::fflush(stdout);
  return ok;
}

int main()
{
  std::cout << "PIM Regression Test: Grid Shift" << std::endl;

  bool ok = true;

  ok &= testGridShift(PIM_DEVICE_BANK_LEVEL, true);

  ok &= testGridShift(PIM_DEVICE_BITSIMD_V, true);

  ok &= testGridShift(PIM_DEVICE_FULCRUM, true);

  ok &= testGridShift(PIM_DEVICE_BANK_LEVEL, false);

  ok &= testGridShift(PIM_DEVICE_BITSIMD_V, false);

  ok &= testGridShift(PIM_DEVICE_FULCRUM, false);

  std::cout << "Grid Shift Test " << (ok ? "PASSED" : "FAILED") << std::endl;

  return 0;
}

