// Test: Grid Ops
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


bool testGridOps(PimDeviceEnum deviceType)
{
  std::cout << "Testing grid ops for device type " << deviceType << std::endl;

  bool ok = true;

  PimStatus status;

  unsigned numRanks = 1;
  unsigned numBankPerRank = 128; // 8 chips * 16 banks
  unsigned numSubarrayPerBank = 32;
  unsigned numRows = 2048;
  unsigned numCols = 8192;

  status = pimCreateDevice(deviceType, numRanks, numBankPerRank, numSubarrayPerBank, numRows, numCols);
  assert(status == PIM_OK);

  const uint64_t srcWidth = 1024;
  const uint64_t srcHeight = 1024;
  const uint64_t tileWidth = 256;
  const uint64_t tileHeight = 128;

  const uint64_t numCoresVertical = srcHeight / tileHeight;
  const uint64_t numCoresHorizontal = srcWidth / tileWidth;

  const size_t numElements = srcWidth * srcHeight;


  int* src = (int*) std::malloc(numElements * sizeof(int));
  int* dest = (int*) std::malloc(numElements * sizeof(int));

  for(uint64_t i = 0; i < numElements; ++i) {
    src[i] = static_cast<int>(i);
    dest[i] = 0;
  }

  PimObjGrid grid = pimAllocGrid(PIM_ALLOC_AUTO, PIM_INT32, numCoresVertical, numCoresHorizontal,
                                  tileHeight, tileWidth, PIM_ALLOCATION_STRATEGY_STENCIL_9_POINT);
  assert(!grid.empty());
  assert(tileHeight == grid.size());

  // Copy to PIM
  status = pimCopyHostToGrid(src, grid);
  assert(status == PIM_OK);

  // Prefix Sum Test
  for(uint64_t i=1; i < tileHeight; ++i) {
    status = pimAdd(grid[i-1], grid[i], grid[i]);
  }

  // Copy back to host
  status = pimCopyGridToHost(grid, dest);
  assert(status == PIM_OK);

  // Compute Prefix Sum on CPU for verification
#if defined(_OPENMP)
#pragma omp parallel for collapse(2)
#endif
  for(uint64_t tileY=0; tileY < numCoresVertical; ++tileY) {
    for(uint64_t tileX=0; tileX < numCoresHorizontal; ++tileX) {
      uint64_t baseIdx = (tileY * tileHeight * srcWidth) + (tileX * tileWidth);
      for(uint64_t i=1; i < tileHeight; ++i) {
        for(uint64_t j=0; j < tileWidth; ++j) {
          src[baseIdx + i*srcWidth + j] += src[baseIdx + (i-1)*srcWidth + j];
        }
      }
    }
  }

  // Verify results
  for(uint64_t i = 0; i < numElements; ++i) {
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
  std::cout << "PIM Regression Test: Grid ops" << std::endl;

  bool ok = true;

  ok &= testGridOps(PIM_DEVICE_BANK_LEVEL);

  ok &= testGridOps(PIM_DEVICE_BITSIMD_V);

  ok &= testGridOps(PIM_DEVICE_FULCRUM);

  std::cout << "Grid Ops Test " << (ok ? "PASSED" : "FAILED") << std::endl;

  return 0;
}

