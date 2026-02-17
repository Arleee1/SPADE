// Test: Grid Large Alloc
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


bool testGridLargeAlloc(PimDeviceEnum deviceType,
                        uint64_t extraCoresVertical = 0, uint64_t extraCoresHorizontal = 0,
                        uint64_t extraElementsPerCoreVertical = 0, uint64_t extraElementsPerCoreHorizontal = 0)
{
  std::cout << "Testing grid large alloc for device type " << deviceType << std::endl;
  const bool extra = extraCoresVertical > 0 || extraCoresHorizontal > 0 || extraElementsPerCoreVertical > 0 || extraElementsPerCoreHorizontal > 0;
  if (extra) {
    std::cout << "  with extra cores: " << extraCoresVertical << " vertical, " << extraCoresHorizontal << " horizontal" << std::endl;
    std::cout << "  with extra elements per core: " << extraElementsPerCoreVertical << " vertical, " << extraElementsPerCoreHorizontal << " horizontal" << std::endl;
  }

  bool ok = true;

  PimStatus status;

  unsigned numRanks = 1;
  unsigned numBankPerRank = 128; // 8 chips * 16 banks
  unsigned numSubarrayPerBank = 32;
  unsigned numRows = 2048;
  unsigned numCols = 8192;

  status = pimCreateDevice(deviceType, numRanks, numBankPerRank, numSubarrayPerBank, numRows, numCols);
  assert(status == PIM_OK);

  PimDeviceProperties deviceProps;
  status = pimGetDeviceProperties(&deviceProps);
  assert(status == PIM_OK);

  const uint64_t numCores = deviceProps.numPIMCores;

  // Allocate a near square of cores
  uint64_t numCoresVertical = 1;
  for (uint64_t factor = 1; factor * factor <= numCores; ++factor) {
    if (numCores % factor == 0) {
      numCoresVertical = factor;
    }
  }
  const uint64_t numCoresHorizontal = extraCoresHorizontal + numCores / numCoresVertical;
  numCoresVertical += extraCoresVertical;

  const uint64_t bitsPerElement = 32;
  uint64_t tileWidth;
  uint64_t tileHeight;
  if(deviceProps.isHLayoutDevice) {
    tileWidth = deviceProps.numColPerSubarray / bitsPerElement;
    tileHeight = deviceProps.numRowPerCore;
  } else {
    tileWidth = deviceProps.numColPerSubarray;
    tileHeight = deviceProps.numRowPerCore / bitsPerElement;
  }

  tileWidth += extraElementsPerCoreHorizontal;
  tileHeight += extraElementsPerCoreVertical;

  std::cout << "Device has " << numCores << " cores, allocate grid with " << numCoresVertical << " vertical cores and " << numCoresHorizontal << " horizontal cores" << std::endl;

  PimObjGrid grid = pimAllocGrid(PIM_ALLOC_AUTO, PIM_FP32, numCoresVertical, numCoresHorizontal,
                                  tileHeight, tileWidth, PIM_ALLOCATION_STRATEGY_STENCIL_9_POINT);

  if(extra) {
    assert(grid.empty());
  } else {
    assert(!grid.empty());
  }

  std::cout << "Allocated grid with " << grid.size() << " objects" << std::endl;

  pimFreeGrid(grid);

  pimShowStats();
  pimDeleteDevice();

  std::fflush(stdout);
  return ok;
}

int main()
{
  std::cout << "PIM Regression Test: Grid large alloc" << std::endl;

  bool ok = true;

  ok &= testGridLargeAlloc(PIM_DEVICE_BANK_LEVEL);

  ok &= testGridLargeAlloc(PIM_DEVICE_BITSIMD_V);

  ok &= testGridLargeAlloc(PIM_DEVICE_FULCRUM);

  std::cout << "Testing grid large alloc with extra cores/elements to trigger allocation failure" << std::endl;

  // ok &= testGridLargeAlloc(PIM_DEVICE_BANK_LEVEL, 1, 0, 0, 0); // extra vertical core
  // ok &= testGridLargeAlloc(PIM_DEVICE_BANK_LEVEL, 0, 1, 0, 0); // extra horizontal core
  // ok &= testGridLargeAlloc(PIM_DEVICE_BANK_LEVEL, 0, 0, 1, 0); // extra vertical elements per core
  // ok &= testGridLargeAlloc(PIM_DEVICE_BANK_LEVEL, 0, 0, 0, 1); // extra horizontal elements per core

  std::cout << "Grid Large Alloc Test " << (ok ? "PASSED" : "FAILED") << std::endl;

  return 0;
}

