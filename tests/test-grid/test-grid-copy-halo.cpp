// Test: Grid Copy Halo
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

bool testGridCopyHalo(PimDeviceEnum deviceType, uint64_t numHalo)
{
  std::cout << "Testing grid halo for device type " << deviceType << " with numHalo = " << numHalo << std::endl;

  bool ok = true;

  PimStatus status;

  unsigned numRanks = 1;
  unsigned numBankPerRank = 128; // 8 chips * 16 banks
  unsigned numSubarrayPerBank = 32;
  unsigned numRows = 2048;
  unsigned numCols = 8192;

  status = pimCreateDevice(deviceType, numRanks, numBankPerRank, numSubarrayPerBank, numRows, numCols);
  assert(status == PIM_OK);

  const uint64_t srcWidth = 800;
  const uint64_t srcHeight = 800;
  const uint64_t tileWidth = 80;
  const uint64_t tileHeight = 80;

  const uint64_t numCoresVertical = srcHeight / tileHeight;
  const uint64_t numCoresHorizontal = srcWidth / tileWidth;
  const uint64_t numCores = numCoresVertical * numCoresHorizontal;

  const size_t numElements = srcWidth * srcHeight;
  const size_t numElementsDest = numCores * (tileWidth+2*numHalo) * (tileHeight+2*numHalo);

  const uint64_t coreHeight = tileHeight + 2*numHalo;
  const uint64_t coreWidth = tileWidth + 2*numHalo;
  const uint64_t totalWidth = numCoresHorizontal * coreWidth;


  int* src = (int*) std::malloc(numElements * sizeof(int));
  int* dest = (int*) std::malloc(numElementsDest * sizeof(int));
  int* check = (int*) std::malloc(numElementsDest * sizeof(int));

  for(uint64_t i = 0; i < numElements; ++i) {
    src[i] = static_cast<int>(i+1);
  }

  for(uint64_t i = 0; i < numElementsDest; ++i) {
    dest[i] = 0;
    check[i] = 0;
  }

  PimObjGrid grid = pimAllocGrid(PIM_ALLOC_AUTO, PIM_INT32, numCoresVertical, numCoresHorizontal,
                                  coreHeight, coreWidth, PIM_ALLOCATION_STRATEGY_STENCIL_9_POINT);
  assert(!grid.empty());
  assert(coreHeight == grid.size());

  // Copy to PIM
  status = pimCopyHostToGrid(src, grid, numHalo, tileHeight + numHalo, numHalo, tileWidth + numHalo);
  assert(status == PIM_OK);

  // Copy halo
  status = pimCopyGridHalo(grid, numHalo);
  assert(status == PIM_OK);

  // Copy back to host
  status = pimCopyGridToHost(grid, dest);
  assert(status == PIM_OK);

  // Setup check array, before halo copy
  for (uint64_t coreY = 0; coreY < numCoresVertical; ++coreY) {
    for (uint64_t coreX = 0; coreX < numCoresHorizontal; ++coreX) {
      for (uint64_t y = 0; y < tileHeight; ++y) {
        for (uint64_t x = 0; x < tileWidth; ++x) {
          const uint64_t srcIdx = (coreY * tileHeight + y) * srcWidth + (coreX * tileWidth + x);
          const uint64_t destIdx = (coreY * coreHeight + (y + numHalo)) * totalWidth
                                 + (coreX * coreWidth + (x + numHalo));
          check[destIdx] = src[srcIdx];
        }
      }
    }
  }

  // Perform halo copy in check array
  auto copyRectangle = [&](uint64_t srcCoreX, uint64_t srcCoreY,
                           uint64_t destCoreX, uint64_t destCoreY,
                           uint64_t srcX, uint64_t srcY,
                           uint64_t destX, uint64_t destY,
                           uint64_t width, uint64_t height) {
    const uint64_t srcBaseY = srcCoreY * coreHeight;
    const uint64_t srcBaseX = srcCoreX * coreWidth;
    const uint64_t destBaseY = destCoreY * coreHeight;
    const uint64_t destBaseX = destCoreX * coreWidth;

    for (uint64_t y = 0; y < height; ++y) {
      for (uint64_t x = 0; x < width; ++x) {
        const uint64_t srcIdx = (srcBaseY + srcY + y) * totalWidth + (srcBaseX + srcX + x);
        const uint64_t destIdx = (destBaseY + destY + y) * totalWidth + (destBaseX + destX + x);
        check[destIdx] = check[srcIdx];
      }
    }
  };

  for (uint64_t coreY = 0; coreY < numCoresVertical; ++coreY) {
    for (uint64_t coreX = 0; coreX < numCoresHorizontal; ++coreX) {
      if (coreX > 0) {
        copyRectangle(coreX - 1, coreY, coreX, coreY,
                      coreWidth - 2 * numHalo, numHalo,
                      0, numHalo,
                      numHalo, coreHeight - 2 * numHalo);
        copyRectangle(coreX, coreY, coreX - 1, coreY,
                      numHalo, numHalo,
                      coreWidth - numHalo, numHalo,
                      numHalo, coreHeight - 2 * numHalo);
      }
      if (coreY > 0) {
        copyRectangle(coreX, coreY - 1, coreX, coreY,
                      numHalo, coreHeight - 2 * numHalo,
                      numHalo, 0,
                      coreWidth - 2 * numHalo, numHalo);
        copyRectangle(coreX, coreY, coreX, coreY - 1,
                      numHalo, numHalo,
                      numHalo, coreHeight - numHalo,
                      coreWidth - 2 * numHalo, numHalo);
      }
      if (coreX > 0 && coreY > 0) {
        copyRectangle(coreX - 1, coreY - 1, coreX, coreY,
                      coreWidth - 2 * numHalo, coreHeight - 2 * numHalo,
                      0, 0,
                      numHalo, numHalo);
        copyRectangle(coreX, coreY, coreX - 1, coreY - 1,
                      numHalo, numHalo,
                      coreWidth - numHalo, coreHeight - numHalo,
                      numHalo, numHalo);
        copyRectangle(coreX, coreY - 1, coreX - 1, coreY,
                      numHalo, coreHeight - 2 * numHalo,
                      coreWidth - numHalo, 0,
                      numHalo, numHalo);
        copyRectangle(coreX - 1, coreY, coreX, coreY - 1,
                      coreWidth - 2 * numHalo, numHalo,
                      0, coreHeight - numHalo,
                      numHalo, numHalo);
      }
    }
  }

  for (uint64_t i = 0; i < numElementsDest; ++i) {
    if (check[i] != dest[i]) {
      std::printf("ERROR: found mismatch at idx %" PRIu64 ": check %d dest %d\n", i, check[i], dest[i]);
      ok = false;
      break;
    }
  }

  std::free(src);
  std::free(dest);
  std::free(check);
  pimFreeGrid(grid);

  pimShowStats();
  pimDeleteDevice();

  std::fflush(stdout);
  return ok;
}

int main()
{
  std::cout << "PIM Regression Test: Grid Copy Halo" << std::endl;

  bool ok = true;

  ok &= testGridCopyHalo(PIM_DEVICE_BANK_LEVEL, 1);

  ok &= testGridCopyHalo(PIM_DEVICE_BITSIMD_V, 1);

  ok &= testGridCopyHalo(PIM_DEVICE_FULCRUM, 1);

  std::cout << "Testing with numHalo = 2" << std::endl;

  ok &= testGridCopyHalo(PIM_DEVICE_BANK_LEVEL, 2);

  ok &= testGridCopyHalo(PIM_DEVICE_BITSIMD_V, 2);

  ok &= testGridCopyHalo(PIM_DEVICE_FULCRUM, 2);

  std::cout << "Grid Copy Halo Test " << (ok ? "PASSED" : "FAILED") << std::endl;

  return 0;
}

