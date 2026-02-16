// Test: Grid Copy
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


bool testGridCopy(PimDeviceEnum deviceType)
{
  std::cout << "Testing grid copy for device type " << deviceType << std::endl;

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


  float* src = (float*) std::malloc(numElements * sizeof(float));
  float* dest = (float*) std::malloc(numElements * sizeof(float));

  PimObjGrid grid = pimAllocGrid(PIM_ALLOC_AUTO, PIM_FP32, numCoresVertical, numCoresHorizontal,
                                  tileHeight, tileWidth, PIM_ALLOCATION_STRATEGY_STENCIL_9_POINT);
  assert(!grid.empty());

  // Copy to PIM
  status = pimCopyHostToGrid(src, grid);
  assert(status == PIM_OK);

  // Copy back to host
  status = pimCopyGridToHost(grid, dest);
  assert(status == PIM_OK);

  // Verify results
  for(uint64_t i = 0; i < numElements; ++i) {
    if (src[i] != dest[i]) {
      std::printf("ERROR: found mismatch at idx %" PRIu64 ": src %f dest %f\n", i, src[i], dest[i]);
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
  std::cout << "PIM Regression Test: Grid copy" << std::endl;

  bool ok = true;

  ok &= testGridCopy(PIM_DEVICE_BANK_LEVEL);

  ok &= testGridCopy(PIM_DEVICE_BITSIMD_V);

  ok &= testGridCopy(PIM_DEVICE_FULCRUM);

  std::cout << "Grid Copy Test " << (ok ? "PASSED" : "FAILED") << std::endl;

  return 0;
}

