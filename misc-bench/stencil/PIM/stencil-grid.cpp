// Test: C++ version of the stencil using grid
// Copyright (c) 2025 University of Virginia
// This file is licensed under the MIT License.
// See the LICENSE file in the root of this repository for more details.

#include <iostream>
#include <vector>
#include <getopt.h>
#include <stdint.h>
#include <iomanip>
#include <cassert>
#include <random>
#include <limits>
#include <algorithm>
#include <cstring>
#include <span>
#if defined(_OPENMP)
#include <omp.h>
#endif

#include "util.h"
#include "libpimeval.h"


// Params ---------------------------------------------------------------------
typedef struct Params
{
  uint64_t iterations;
  uint64_t gridWidth;
  uint64_t gridHeight;
  uint64_t radius;
  const char *configFile;
  const char *inputFile;
  bool shouldVerify;
} Params;

void usage()
{
  fprintf(stderr,
          "\nUsage:  ./stencil.out [options]"
          "\n"
          "\n    -n    iterations (default=10 iteration)"
          "\n    -x    grid width (default=10000 elements)"
          "\n    -y    grid height (default=10000 elements)"
          "\n    -r    stencil radius (default=2)"
          "\n    -c    dramsim config file"
          "\n    -i    input file containing a 2d array (default=random)"
          "\n    -v    t = verifies PIM output with host output. (default=false)"
          "\n");
}

struct Params getInputParams(int argc, char **argv)
{
  struct Params p;
  p.iterations = 10;
  p.gridWidth = 10000;
  p.gridHeight = 10000;
  p.radius = 2;
  p.configFile = nullptr;
  p.inputFile = nullptr;
  p.shouldVerify = false;

  int opt;
  while ((opt = getopt(argc, argv, "h:n:x:y:r:c:i:v:")) >= 0)
  {
    switch (opt)
    {
    case 'h':
      usage();
      exit(0);
      break;
    case 'n':
      p.iterations = strtoull(optarg, NULL, 0);
      break;
    case 'x':
      p.gridWidth = strtoull(optarg, NULL, 0);
      break;
    case 'y':
      p.gridHeight = strtoull(optarg, NULL, 0);
      break;
    case 'r':
      p.radius= strtoull(optarg, NULL, 0);
      break;
    case 'c':
      p.configFile = optarg;
      break;
    case 'i':
      p.inputFile = optarg;
      break;
    case 'v':
      p.shouldVerify = (*optarg == 't');
      break;
    default:
      fprintf(stderr, "\nUnrecognized option!\n");
      usage();
      exit(0);
    }
  }
  return p;
}

//! @brief  Sums the neighbors of each element in a stencil row to compute the horizontal stencil sum
//!
//! Sums radius number of elemements to the left and right of center element, including center element
//! Puts each result pimRowSum[i] where i is the center index
//! Formula: pimRowSum[i] = Σ (j ∈ [i-radius, i+radius]) mid[j]
//! Works by shifting mid to the left and right and adding shifted versions
//! @param[in]  mid  PIM row to be summed
//! @param[out]  pimRowSum  The resultant PIM object to place the sum into
//! @param[in,out]  shiftBackup  Temporary PIM object used for calculations
//! @param[in]  radius  The stencil radius
void sumStencilRow(PimObjId mid, PimObjId pimRowSum, PimObjId shiftBackup, const uint64_t radius) {
  PimStatus status;

  if(radius == 0) {
    return;
  }

  status = pimCopyObjectToObject(mid, shiftBackup);
  assert (status == PIM_OK);

  status = pimShiftElementsRight(shiftBackup);
  assert (status == PIM_OK);

  status = pimAdd(mid, shiftBackup, pimRowSum);
  assert (status == PIM_OK);

  for(uint64_t shiftIter=1; shiftIter<radius; ++shiftIter) {
    status = pimShiftElementsRight(shiftBackup);
    assert (status == PIM_OK);

    status = pimAdd(pimRowSum, shiftBackup, pimRowSum);
    assert (status == PIM_OK);
  }

  status = pimCopyObjectToObject(mid, shiftBackup);
  assert (status == PIM_OK);

  for(uint64_t shiftIter=0; shiftIter<radius; ++shiftIter) {
    status = pimShiftElementsLeft(shiftBackup);
    assert (status == PIM_OK);

    status = pimAdd(pimRowSum, shiftBackup, pimRowSum);
    assert (status == PIM_OK);
  }
}

//! @brief  Computes one iteration of one chunk of the stencil
//!
//! Uses circular queue to compute window sums
//! Adds the next row to the front of the queue and to the sum
//! Takes the sum (divided by the stencil area) as the result from the row
//! Subtracts the back of the queue from the sum
//! Pops from the queue back of the queue
//! Repeats until done
//! @param[in]  workingPimMemory  PIM rows in the stencil chunk
//! @param[in]  rowsInSumCircularQueue  Queue used for keeping track of running sum of rows vertically
//! @param[in,out]  tmpPim  Temporary PIM object used for calculations
//! @param[in,out]  runningSum Temporary PIM object used for keeping track of the current running (vertical) sum
//! @param[in]  stencilAreaToMultiplyPim This algorithm computes stencil average, thus each element in the result must be divided by the stencil area. This is done by multiplying by the inverse.
//! @param[in]  radius  The stencil radius
void computeStencilChunkIteration(std::span<PimObjId> workingPimMemory, std::span<PimObjId> rowsInSumCircularQueue, PimObjId tmpPim, PimObjId runningSum, const uint64_t stencilAreaToMultiplyPim, const uint64_t radius) {
  PimStatus status;

  uint64_t circularQueueBot = 0;
  uint64_t circularQueueTop = 0;

  sumStencilRow(workingPimMemory[0], rowsInSumCircularQueue[circularQueueTop], tmpPim, radius);
  ++circularQueueTop;
  sumStencilRow(workingPimMemory[1], rowsInSumCircularQueue[circularQueueTop], tmpPim, radius);
  ++circularQueueTop;
  status = pimAdd(rowsInSumCircularQueue[0], rowsInSumCircularQueue[1], runningSum);
  assert (status == PIM_OK);

  // At this point:
  // circularQueueBot = 0
  // circularQueueTop = 2
  // rowsInSumCircularQueue[0] = workingPimMemory[0] horizontally summed
  // rowsInSumCircularQueue[1] = workingPimMemory[1] horizontally summed
  // runningSum = sum of first two rows horizontally summed

  for(uint64_t i=2; i<2*radius; ++i) {
    sumStencilRow(workingPimMemory[i], rowsInSumCircularQueue[circularQueueTop], tmpPim, radius);
    status = pimAdd(runningSum, rowsInSumCircularQueue[circularQueueTop], runningSum);
    assert (status == PIM_OK);
    ++circularQueueTop;
  }

  // At this point:
  // circularQueueBot = 0
  // circularQueueTop = 2*radius
  // rowsInSumCircularQueue[0...2*radius] are occupied with workingPimMemory[0...2*radius] horizontally summed
  // runningSum = sum of rows [0...2*radius] horizontally summed

  uint64_t nextRowToAdd = 2*radius; // The index of the next row to add to the queue and to the running sum

  // Loops over the rest of the rows in the current chunk, vertically
  // Each iteration, finds horizontal sum of the next row (nextRowToAdd)
  // Places this horizontal sum at the front of the queue (at position circularQueueTop)
  // Adds the horizontal sum to the runningSum
  // Places runningSum/stencilArea into the workingPimMemory as the final result for the row
  // If neccessary, subtracts the row from the back of the queue from the runningSum

  for(uint64_t row=radius; row<workingPimMemory.size()-radius; ++row) {
    sumStencilRow(workingPimMemory[nextRowToAdd], rowsInSumCircularQueue[circularQueueTop], tmpPim, radius);

    status = pimAdd(runningSum, rowsInSumCircularQueue[circularQueueTop], runningSum);
    assert (status == PIM_OK);

    circularQueueTop = (1+circularQueueTop) % rowsInSumCircularQueue.size();
    ++nextRowToAdd;

    status = pimMulScalar(runningSum, workingPimMemory[row], stencilAreaToMultiplyPim);
    assert (status == PIM_OK);

    if(row+1<workingPimMemory.size()-radius) {
      status = pimSub(runningSum, rowsInSumCircularQueue[circularQueueBot], runningSum);
      assert (status == PIM_OK);
      circularQueueBot = (1+circularQueueBot) % rowsInSumCircularQueue.size();
    }
  }
}

void stencil(const std::span<float> srcHost, std::span<float> dstHost, const uint64_t gridWidth, const uint64_t iterations, const uint64_t radius,
              const uint64_t maxAvailableCores, const uint64_t coreHeight, const uint64_t coreWidth) {
  assert(srcHost.size() == dstHost.size());

  const uint64_t gridHeight = srcHost.size() / gridWidth;
  const uint64_t extraRows = (2*radius) + (2*radius+1) + (1) + (1); // halo + rowsInSumCircularQueue + tmpPim + runningSum
  const uint64_t extraCols = 2*radius; // halo
  const uint64_t maxTileHeight = coreHeight - extraRows;
  const uint64_t maxTileWidth = coreWidth - extraCols;

  //! @todo grid: verify + cleanup -- what if grid doesn't fit nicely?
  uint64_t tileHeight = 0;
  uint64_t tileWidth = 0;
  uint64_t numCoresVertical = 0;
  uint64_t numCoresHorizontal = 0;
  uint64_t totalCores = 0;

  for (uint64_t candidateTotalCores = maxAvailableCores; candidateTotalCores > 0; --candidateTotalCores) {
    bool foundForThisCoreCount = false;
    uint64_t bestCoreShapeDiff = std::numeric_limits<uint64_t>::max();
    for (uint64_t candidateCoresHorizontal = 1; candidateCoresHorizontal <= candidateTotalCores; ++candidateCoresHorizontal) {
      if (candidateTotalCores % candidateCoresHorizontal != 0) {
        continue;
      }

      const uint64_t candidateCoresVertical = candidateTotalCores / candidateCoresHorizontal;
      if (gridWidth % candidateCoresHorizontal != 0 || gridHeight % candidateCoresVertical != 0) {
        continue;
      }

      const uint64_t candidateTileWidth = gridWidth / candidateCoresHorizontal;
      const uint64_t candidateTileHeight = gridHeight / candidateCoresVertical;
      if (candidateTileWidth > maxTileWidth || candidateTileHeight > maxTileHeight) {
        continue;
      }

      const uint64_t candidateCoreShapeDiff =
          (candidateCoresVertical > candidateCoresHorizontal)
              ? (candidateCoresVertical - candidateCoresHorizontal)
              : (candidateCoresHorizontal - candidateCoresVertical);

      if (!foundForThisCoreCount ||
          candidateCoreShapeDiff < bestCoreShapeDiff ||
          (candidateCoreShapeDiff == bestCoreShapeDiff && candidateCoresHorizontal > numCoresHorizontal)) {
        foundForThisCoreCount = true;
        bestCoreShapeDiff = candidateCoreShapeDiff;
        totalCores = candidateTotalCores;
        numCoresHorizontal = candidateCoresHorizontal;
        numCoresVertical = candidateCoresVertical;
        tileWidth = candidateTileWidth;
        tileHeight = candidateTileHeight;
      }
    }

    if (foundForThisCoreCount) {
      break;
    }
  }

  assert(totalCores > 0);
  const uint64_t rowsToAllocate = tileHeight + extraRows;
  const uint64_t colsToAllocate = tileWidth + extraCols;

  assert(gridWidth == numCoresHorizontal * tileWidth);
  assert(gridHeight == numCoresVertical * tileHeight);
  assert(tileHeight <= maxTileHeight);
  assert(tileWidth <= maxTileWidth);
  assert(gridHeight % tileHeight == 0);
  assert(gridWidth % tileWidth == 0);
  assert(totalCores == numCoresVertical * numCoresHorizontal);
  assert(srcHost.size() == numCoresVertical * tileHeight * numCoresHorizontal * tileWidth);


  const uint64_t stencilAreaInt = (2 * radius + 1) * (2 * radius + 1);
  const float stencilAreaFloat = 1.0f / static_cast<float>(stencilAreaInt);
  uint32_t tmp;
  std::memcpy(&tmp, &stencilAreaFloat, sizeof(float));
  const uint64_t stencilAreaToMultiplyPim = static_cast<uint64_t>(tmp);

  std::cout << "PIM Stencil for " << gridHeight << "x" << gridWidth << " grid with radius " << radius << " for " << iterations << " iterations" << std::endl;
  std::cout << "Using " << totalCores << "/" << maxAvailableCores << " cores in a grid of " << numCoresVertical << "x" << numCoresHorizontal << " cores" << std::endl;
  std::cout << "Tile size: " << tileHeight << "x" << tileWidth << std::endl;

  // Hard code sizes for now
  // (2*radius+1) + (1) + (1) -> represents arguments to stencilChunk function
  PimObjGrid grid = pimAllocGrid(PIM_ALLOC_AUTO, PIM_FP32, numCoresVertical, numCoresHorizontal,
                                  rowsToAllocate, colsToAllocate, PIM_ALLOCATION_STRATEGY_STENCIL_9_POINT);
  assert(!grid.empty());
  assert(rowsToAllocate == grid.size());

  PimStatus status = pimCopyHostToGrid(srcHost.data(), grid, radius, tileWidth + radius, radius, tileHeight + radius);
  assert(status == PIM_OK);

  auto it = grid.begin();
  std::span<PimObjId> workingPimMemory(it, it + tileHeight + 2*radius);
  it += tileHeight + 2*radius;
  std::span<PimObjId> rowsInSumCircularQueue(it, it + (2*radius+1));
  it += (2*radius+1);
  PimObjId tmpPim = *it;
  ++it;
  PimObjId runningSum = *it;
  assert(it+1 == grid.end());

  //! @todo grid: need to remove
  std::vector<PimObjId> workingPimMemoryVec(workingPimMemory.begin(), workingPimMemory.end());
  status = pimCopyGridHalo(workingPimMemoryVec, radius);
  assert(status == PIM_OK);


  for(size_t iter = 0; iter < iterations; ++iter) {
    computeStencilChunkIteration(workingPimMemory, rowsInSumCircularQueue, tmpPim, runningSum, stencilAreaToMultiplyPim, radius);

    if(iter < iterations - 1) { // Only need to copy halo if not the last iteration
      status = pimCopyGridHalo(workingPimMemoryVec, radius);
      assert(status == PIM_OK);
    }
  }

  // Only copy back the non-halo region
  status = pimCopyGridToHost(grid, dstHost.data(), radius, tileWidth + radius, radius, tileHeight + radius);
  assert(status == PIM_OK);

  // dest should now have the results of the stencil computation

  status = pimFreeGrid(grid);
  assert(status == PIM_OK);
}

void stencilCpu(std::span<float> &src, std::span<float> &dst, const uint64_t iterations, const uint64_t radius, uint64_t width, uint64_t height) {
  const uint64_t stencilAreaInt = (2 * radius + 1) * (2 * radius + 1);
  const float stencilAreaInverseFloat = 1.0f / static_cast<float>(stencilAreaInt);

  for(uint64_t iter=1; iter<=iterations; ++iter) {
    // Only compute when stencil is fully in range
    const uint64_t startY = radius*iter;
    const uint64_t endY = height - startY;
    const uint64_t startX = radius*iter;
    const uint64_t endX = width - startX;
#if defined(_OPENMP)
#pragma omp parallel for collapse(2)
#endif
    for(uint64_t gridY=startY; gridY<endY; ++gridY) {
      for(uint64_t gridX=startX; gridX<endX; ++gridX) {
        float resCPU = 0.0f;
        for(uint64_t stencilY=gridY-radius; stencilY<=gridY+radius; ++stencilY) {
          for(uint64_t stencilX=gridX-radius; stencilX<=gridX+radius; ++stencilX) {
            resCPU += src[stencilY * width + stencilX];
          }
        }
        dst[gridY * width + gridX] = resCPU * stencilAreaInverseFloat;
      }
    }
    std::swap(src, dst);
  }
  std::swap(src, dst);
}

int main(int argc, char* argv[])
{
  struct Params params = getInputParams(argc, argv);

  std::cout << "Running PIM stencil for grid: " << params.gridHeight << "x" << params.gridWidth << std::endl;
  std::cout << "Stencil Radius: " << params.radius << ", Number of Iterations: " << params.iterations << std::endl;

  std::vector<float> x_(params.gridHeight * params.gridWidth);
  std::vector<float> y_(params.gridHeight * params.gridWidth);

  if (params.inputFile == nullptr)
  {
    // Fill in random grid
#if defined(_OPENMP)
#pragma omp parallel
#endif
    {
      constexpr uint32_t baseSeed = 12345u;
      uint32_t threadSeed = baseSeed;
    #if defined(_OPENMP)
      threadSeed += static_cast<uint32_t>(omp_get_thread_num());
    #endif
      std::mt19937 gen(threadSeed);
      std::uniform_real_distribution<float> dist(0.0f, 10000.0f);
#if defined(_OPENMP)
#pragma omp for
#endif
      for(size_t i=0; i<params.gridHeight; ++i) {
        for(size_t j=0; j<params.gridWidth; ++j) {
          x_[i * params.gridWidth + j] = static_cast<float>(dist(gen));
        }
      }
    }
  }
  else
  {
    std::cout << "Reading from input file is not implemented yet." << std::endl;
    return 1;
  }

  if (!createDevice(params.configFile))
  {
    return 1;
  }

  PimDeviceProperties deviceProp;
  PimStatus status = pimGetDeviceProperties(&deviceProp);
  assert(status == PIM_OK);

  constexpr uint64_t bitsPerElement = 32;

  uint64_t coreHeight = 2 * deviceProp.numRowPerSubarray;
  if(!deviceProp.isHLayoutDevice) {
    coreHeight /= bitsPerElement;
  }

  uint64_t coreWidth;
  if(deviceProp.isHLayoutDevice) {
    switch(deviceProp.simTarget) {
      case PIM_DEVICE_FULCRUM:
      case PIM_DEVICE_BANK_LEVEL:
        coreWidth = deviceProp.numColPerSubarray / bitsPerElement;
        break;
      default:
        std::cerr << "Stencil unimplemented for simulation target: " << deviceProp.simTarget << std::endl;
        std::exit(1);
    }
  } else {
    coreWidth = deviceProp.numColPerSubarray;
  }

  std::span<float> x(x_);
  std::span<float> y(y_);

  stencil(x, y, params.gridWidth, params.iterations, params.radius, deviceProp.numPIMCores, coreHeight, coreWidth);

  if (params.shouldVerify)
  {
    std::vector<float> cpuY_(y.size());
    std::span<float> cpuY(cpuY_);

    stencilCpu(x, cpuY, params.iterations, params.radius, params.gridWidth, params.gridHeight);
    bool ok = true;

    // Only compute when stencil is fully in range
    const uint64_t startY = params.radius * params.iterations;
    const uint64_t endY = params.gridHeight - startY;
    const uint64_t startX = params.radius * params.iterations;
    const uint64_t endX = params.gridWidth - startX;

    std::cout << std::fixed << std::setprecision(10);
#if defined(_OPENMP)
#pragma omp parallel for collapse(2)
#endif
    for(uint64_t gridY=startY; gridY<endY; ++gridY) {
      for(uint64_t gridX=startX; gridX<endX; ++gridX) {
        constexpr float acceptableDelta = 0.1f;
        if (std::abs(cpuY[gridY * params.gridWidth + gridX] - y[gridY * params.gridWidth + gridX]) > acceptableDelta)
        {
#if defined(_OPENMP)
#pragma omp critical
#endif
          {
            std::cout << "Wrong answer: " << y[gridY * params.gridWidth + gridX] << " (expected " << cpuY[gridY * params.gridWidth + gridX] << ") at position (" << gridX << ", " << gridY << ")" << std::endl;
            ok = false;
            assert(0);
          }
        }
      }
    }
    if(ok) {
      std::cout << "Correct for stencil!" << std::endl;
    }
  }

  pimShowStats();

  return 0;
}