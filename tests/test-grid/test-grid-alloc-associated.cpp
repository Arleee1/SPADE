// Test: Grid Alloc Associated
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


bool testGridAllocAssociated(PimDeviceEnum deviceType)
{
  std::cout << "Testing grid alloc associated for device type " << deviceType << std::endl;

  bool ok = true;

  PimStatus status;

  unsigned numRanks = 1;
  unsigned numBankPerRank = 128; // 8 chips * 16 banks
  unsigned numSubarrayPerBank = 32;
  unsigned numRows = 2048;
  unsigned numCols = 8192;

  status = pimCreateDevice(deviceType, numRanks, numBankPerRank, numSubarrayPerBank, numRows, numCols);
  assert(status == PIM_OK);

  const uint64_t tileWidth = 256;
  const uint64_t elementsPerCoreVertical = 3;
  const uint64_t assocElementsPerCoreVertical = 2;

  const uint64_t numCoresVertical = 5;
  const uint64_t numCoresHorizontal = 5;

  const size_t elementsPerObject = numCoresVertical * numCoresHorizontal * tileWidth;
  const size_t numElementsSrc = elementsPerObject * elementsPerCoreVertical;
  const size_t numElementsDest = elementsPerObject * assocElementsPerCoreVertical;

  std::vector<int> src(numElementsSrc);
  std::vector<uint8_t> dest(numElementsDest);


  for(uint64_t i = 0; i < numElementsSrc; ++i) {
    src[i] = static_cast<int>(i % 13);
  }

  PimObjGrid grid = pimAllocGrid(PIM_ALLOC_AUTO, PIM_INT32, numCoresVertical, numCoresHorizontal,
                                  elementsPerCoreVertical, tileWidth);
  assert(!grid.empty());
  assert(elementsPerCoreVertical == grid.size());

  PimObjGrid associatedGrid = pimAllocGridAssociated(grid[0], PIM_BOOL, assocElementsPerCoreVertical);
  assert(!associatedGrid.empty());
  assert(assocElementsPerCoreVertical == associatedGrid.size());

  // Copy to PIM
  status = pimCopyHostToGrid(src.data(), grid);
  assert(status == PIM_OK);

  // Bool ops on associated grid
  status = pimLT(grid[0], grid[1], associatedGrid[0]);
  assert(status == PIM_OK);

  status = pimEQ(grid[1], grid[2], associatedGrid[1]);
  assert(status == PIM_OK);

  // Copy back to host
  status = pimCopyGridToHost(associatedGrid, dest.data());
  assert(status == PIM_OK);

  // Verify results
  const uint64_t totalElementsHorizontal = numCoresHorizontal * tileWidth;
  for(uint64_t i = 0; i < totalElementsHorizontal; ++i) {
    bool expectedLT = src[i] < src[totalElementsHorizontal + i];
    bool expectedEQ = src[totalElementsHorizontal + i] == src[2*totalElementsHorizontal + i];
    bool actualLT = static_cast<bool>(dest[i]);
    bool actualEQ = static_cast<bool>(dest[totalElementsHorizontal + i]);
    if (actualLT != expectedLT) {
      std::printf("ERROR: found mismatch at idx %" PRIu64 ": expectedLT %d actualLT %d\n", i, expectedLT, actualLT);
      ok = false;
      break;
    }
    if (actualEQ != expectedEQ) {
      std::printf("ERROR: found mismatch at idx %" PRIu64 ": expectedEQ %d actualEQ %d\n", i, expectedEQ, actualEQ);
      ok = false;
      break;
    }
  }

  pimFreeGrid(grid);
  pimFreeGrid(associatedGrid);

  pimShowStats();
  pimDeleteDevice();

  std::fflush(stdout);
  return ok;
}

int main()
{
  std::cout << "PIM Regression Test: Grid alloc associated" << std::endl;

  bool ok = true;

  ok &= testGridAllocAssociated(PIM_DEVICE_BANK_LEVEL);

  ok &= testGridAllocAssociated(PIM_DEVICE_BITSIMD_V);

  ok &= testGridAllocAssociated(PIM_DEVICE_FULCRUM);

  std::cout << "Grid Alloc Associated Test " << (ok ? "PASSED" : "FAILED") << std::endl;

  return 0;
}

