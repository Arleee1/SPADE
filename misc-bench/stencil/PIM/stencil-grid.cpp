// Test: C++ version of the stencil using grid
// Copyright (c) 2025 University of Virginia
// This file is licensed under the MIT License.
// See the LICENSE file in the root of this repository for more details.

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <getopt.h>
#include <iomanip>
#include <iostream>
#include <random>
#include <span>
#include <vector>
#if defined(_OPENMP)
#include <omp.h>
#endif

#include "util.h"
#include "utilGrid.h"
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

  const GridPartitioning partitioning = calculateGridPartitioning(gridWidth, gridHeight, maxAvailableCores, maxTileWidth, maxTileHeight);

  assert(partitioning.totalCores > 0);
  const uint64_t colsToAllocate = partitioning.tileWidth + extraCols;

  assert(srcHost.size() == partitioning.numCoresVertical * partitioning.tileHeight * partitioning.numCoresHorizontal * partitioning.tileWidth);


  const uint64_t stencilAreaInt = (2 * radius + 1) * (2 * radius + 1);
  const float stencilAreaFloat = 1.0f / static_cast<float>(stencilAreaInt);
  uint32_t tmp;
  std::memcpy(&tmp, &stencilAreaFloat, sizeof(float));
  const uint64_t stencilAreaToMultiplyPim = static_cast<uint64_t>(tmp);

  std::cout << "PIM Stencil for " << gridHeight << "x" << gridWidth << " grid with radius " << radius << " for " << iterations << " iterations" << std::endl;
  std::cout << "Using " << partitioning.totalCores << "/" << maxAvailableCores << " cores in a grid of " << partitioning.numCoresVertical << "x" << partitioning.numCoresHorizontal << " cores" << std::endl;
  std::cout << "Tile size: " << partitioning.tileHeight << "x" << partitioning.tileWidth << std::endl;

  PimObjGrid workingPimMemory = pimAllocGrid(PIM_ALLOC_AUTO, PIM_FP32, partitioning.numCoresVertical, partitioning.numCoresHorizontal,
                                  partitioning.tileHeight + 2*radius, colsToAllocate, PIM_ALLOCATION_STRATEGY_STENCIL_9_POINT);
  assert(partitioning.tileHeight + 2*radius == workingPimMemory.size());

  PimObjGrid rowsInSumCircularQueue = pimAllocGridAssociated(workingPimMemory[0], PIM_FP32, 2*radius+1);
  assert(2*radius+1 == rowsInSumCircularQueue.size());

  PimObjGrid tmpObjsGrid = pimAllocGridAssociated(workingPimMemory[0], PIM_FP32, 2);
  assert(2 == tmpObjsGrid.size());
  PimObjId tmpPim = tmpObjsGrid[0];
  PimObjId runningSum = tmpObjsGrid[1];

  PimStatus status = pimCopyHostToGrid(srcHost.data(), workingPimMemory, radius, partitioning.tileWidth + radius, radius, partitioning.tileHeight + radius);
  assert(status == PIM_OK);

  status = pimCopyGridHalo(workingPimMemory, radius);
  assert(status == PIM_OK);


  for(size_t iter = 0; iter < iterations; ++iter) {
    computeStencilChunkIteration(workingPimMemory, rowsInSumCircularQueue, tmpPim, runningSum, stencilAreaToMultiplyPim, radius);

    if(iter < iterations - 1) { // Only need to copy halo if not the last iteration
      status = pimCopyGridHalo(workingPimMemory, radius);
      assert(status == PIM_OK);
    }
  }

  // Only copy back the non-halo region
  status = pimCopyGridToHost(workingPimMemory, dstHost.data(), radius, partitioning.tileWidth + radius, radius, partitioning.tileHeight + radius);
  assert(status == PIM_OK);

  // dest should now have the results of the stencil computation

  status = pimFreeGrid(workingPimMemory);
  assert(status == PIM_OK);

  status = pimFreeGrid(rowsInSumCircularQueue);
  assert(status == PIM_OK);

  status = pimFreeGrid(tmpObjsGrid);
  assert(status == PIM_OK);
}

int main(int argc, char* argv[])
{
  struct Params params = getInputParams(argc, argv);

  if(params.radius == 0) {
    std::cout << "Stencil radius must not be 0, please provide a different radius." << std::endl;
    return 1;
  }

  std::cout << "Running PIM stencil for grid: " << params.gridHeight << "x" << params.gridWidth << std::endl;
  std::cout << "Stencil Radius: " << params.radius << ", Number of Iterations: " << params.iterations << std::endl;

  std::vector<float> x_(params.gridHeight * params.gridWidth);
  std::vector<float> y_(x_.size());

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
      for(size_t idx = 0; idx < x_.size(); ++idx) {
        x_[idx] = dist(gen);
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

  uint64_t coreHeight = deviceProp.numRowPerCore;
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

    const uint64_t stencilAreaInt = (2 * params.radius + 1) * (2 * params.radius + 1);
    const float stencilAreaInverseFloat = 1.0f / static_cast<float>(stencilAreaInt);

    const auto stencilCpuKernel = [stencilAreaInverseFloat](const std::span<float> &stencilSrc, const uint64_t stencilWidth,
                                       const uint64_t gridX, const uint64_t gridY, const uint64_t stencilRadius) -> float {
               float resCPU = 0.0f;
               for(uint64_t stencilY=gridY-stencilRadius; stencilY<=gridY+stencilRadius; ++stencilY) {
                 for(uint64_t stencilX=gridX-stencilRadius; stencilX<=gridX+stencilRadius; ++stencilX) {
                   resCPU += stencilSrc[stencilY * stencilWidth + stencilX];
                 }
               }
               return resCPU * stencilAreaInverseFloat;
             };

    stencilCpu(x, cpuY, params.iterations, params.radius, params.gridWidth, params.gridHeight, stencilCpuKernel);
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