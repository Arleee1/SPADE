// Test: C++ version of the game of life using grid
// Copyright (c) 2026 University of Virginia
// This file is licensed under the MIT License.
// See the LICENSE file in the root of this repository for more details.

#include <iostream>
#include <vector>
#include <getopt.h>
#include <stdint.h>
#include <iomanip>
#include <cassert>
#include <list>
#include <random>
#include <span>
#if defined(_OPENMP)
#include <omp.h>
#endif

#include "util.h"
#include "utilGrid.h"
#include "utilBaselines.h"
#include "libpimeval.h"

using namespace std;

// Params ---------------------------------------------------------------------
typedef struct Params
{
  uint64_t iterations;
  uint64_t width;
  uint64_t height;
  char *configFile;
  char *inputFile;
  bool shouldVerify;
} Params;

void usage()
{
  fprintf(stderr,
          "\nUsage:  ./game-of-life.out [options]"
          "\n"
          "\n    -n    iterations (default=10 iterations)"
          "\n    -x    board width (default=2048 elements)"
          "\n    -y    board height (default=2048 elements)"
          "\n    -c    dramsim config file"
          "\n    -i    input file containing a game board (default=generates board with random states)"
          "\n    -v    t = verifies PIM output with host output. (default=false)"
          "\n");
}

struct Params getInputParams(int argc, char **argv)
{
  struct Params p;
  p.iterations = 10;
  p.width = 2048;
  p.height = 2048;
  p.configFile = nullptr;
  p.inputFile = nullptr;
  p.shouldVerify = false;

  int opt;
  while ((opt = getopt(argc, argv, "h:n:x:y:c:i:v:")) >= 0)
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
      p.width = strtoull(optarg, NULL, 0);
      break;
    case 'y':
      p.height = strtoull(optarg, NULL, 0);
      break;
    case 'c':
      p.configFile = optarg;
      break;
    case 'i':
      p.inputFile = optarg;
      break;
    case 'v':
      p.shouldVerify = (*optarg == 't') ? true : false;
      break;
    default:
      fprintf(stderr, "\nUnrecognized option!\n");
      usage();
      exit(0);
    }
  }
  return p;
}

void computeGOLChunkIteration(PimObjGrid& workingPimMemory, PimObjGrid& rowsInSumCircularQueue, PimObjId tmpPimBool) {
  PimStatus status;

  uint64_t circularQueueBot = 0;
  uint64_t circularQueueTop = 0;

  sumStencilRow(workingPimMemory[0], rowsInSumCircularQueue[circularQueueTop], tmpPimBool, 1);
  ++circularQueueTop;
  sumStencilRow(workingPimMemory[1], rowsInSumCircularQueue[circularQueueTop], tmpPimBool, 1);
  ++circularQueueTop;

  // At this point:
  // circularQueueBot = 0
  // circularQueueTop = 2
  // rowsInSumCircularQueue[0] = workingPimMemory[0] horizontally summed
  // rowsInSumCircularQueue[1] = workingPimMemory[1] horizontally summed

  uint64_t nextRowToAdd = 2; // The index of the next row to add to the queue and to rowsInSumCircularQueue

  // Loops over the rest of the rows in the current chunk, vertically

  for(uint64_t row=1; row<workingPimMemory.size()-1; ++row) {
    sumStencilRow(workingPimMemory[nextRowToAdd], rowsInSumCircularQueue[circularQueueTop], tmpPimBool, 1);

    // Add up the three horizontally summed rows to get the total sum for the 3x3 stencil
    status = pimAdd(rowsInSumCircularQueue[(1+circularQueueBot) % rowsInSumCircularQueue.size()], rowsInSumCircularQueue[circularQueueBot], rowsInSumCircularQueue[circularQueueBot]);
    assert (status == PIM_OK);
    status = pimAdd(rowsInSumCircularQueue[circularQueueTop], rowsInSumCircularQueue[circularQueueBot], rowsInSumCircularQueue[circularQueueBot]);
    assert (status == PIM_OK);

    auto& sum = rowsInSumCircularQueue[circularQueueBot];
    status = pimEQScalar(sum, tmpPimBool, 4);
    assert (status == PIM_OK);

    status = pimAnd(tmpPimBool, workingPimMemory[row], workingPimMemory[row]);
    assert (status == PIM_OK);

    status = pimEQScalar(sum, tmpPimBool, 3);
    assert (status == PIM_OK);

    status = pimOr(workingPimMemory[row], tmpPimBool, workingPimMemory[row]);
    assert (status == PIM_OK);

    circularQueueTop = (1+circularQueueTop) % rowsInSumCircularQueue.size();
    ++nextRowToAdd;

    circularQueueBot = (1+circularQueueBot) % rowsInSumCircularQueue.size();

    // status = pimMulScalar(runningSum, workingPimMemory[row], stencilAreaToMultiplyPim);
    // assert (status == PIM_OK);

    // if(row+1<workingPimMemory.size()-radius) {
    //   status = pimSub(runningSum, rowsInSumCircularQueue[circularQueueBot], runningSum);
    //   assert (status == PIM_OK);
    //   circularQueueBot = (1+circularQueueBot) % rowsInSumCircularQueue.size();
    // }
  }
}

void game_of_life(const std::span<uint8_t> &src_host, std::span<uint8_t> &dst_host, const uint64_t width,
                  const uint64_t iterations, const uint64_t maxAvailableCores, const uint64_t coreHeight,
                  const uint64_t coreWidth, const bool isHLayoutDevice)
{
  assert(src_host.size() == dst_host.size());

  PimStatus status;
  const size_t height = src_host.size() / width;
  const uint64_t extraRowsBool = 2 + 1; // halo + tmp_pim_bool
  const uint64_t extraRowsUint8 = 3; // rowsInSumCircularQueue
  const uint64_t extraColsBool = 2; // halo

  uint64_t maxTileHeight;
  uint64_t maxTileWidth;
  if(isHLayoutDevice) {
    maxTileHeight = coreHeight - extraRowsBool - extraRowsUint8;
    maxTileWidth = coreWidth/8 - extraColsBool;
  } else {
    maxTileHeight = coreHeight - extraRowsBool - 8*extraRowsUint8;
    maxTileWidth = coreWidth - extraColsBool;
  }

  const GridPartitioning partitioning = calculateGridPartitioning(width, height, maxAvailableCores, maxTileWidth, maxTileHeight);

  assert(partitioning.totalCores > 0);
  const uint64_t rowsToAllocateBool = partitioning.tileHeight + extraRowsBool;
  const uint64_t rowsToAllocateUint8 = extraRowsUint8;
  const uint64_t colsToAllocate = partitioning.tileWidth + extraColsBool;

  assert(src_host.size() == partitioning.numCoresVertical * partitioning.tileHeight * partitioning.numCoresHorizontal * partitioning.tileWidth);

  std::cout << "PIM Game of Life for " << height << "x" << width << " for " << iterations << " iterations" << std::endl;
  std::cout << "Using " << partitioning.totalCores << "/" << maxAvailableCores << " cores in a grid of " << partitioning.numCoresVertical << "x" << partitioning.numCoresHorizontal << " cores" << std::endl;
  std::cout << "Tile size: " << partitioning.tileHeight << "x" << partitioning.tileWidth << std::endl;

  //! @todo allocation strategy
  PimObjGrid rowsInSumCircularQueue = pimAllocGrid(PIM_ALLOC_AUTO, PIM_UINT8, partitioning.numCoresVertical, partitioning.numCoresHorizontal,
                                  rowsToAllocateUint8, colsToAllocate, PIM_ALLOCATION_STRATEGY_STENCIL_9_POINT);
  assert(!rowsInSumCircularQueue.empty());
  assert(rowsToAllocateUint8 == rowsInSumCircularQueue.size());

  PimObjGrid workingPimMemory = pimAllocGridAssociated(rowsInSumCircularQueue[0], PIM_BOOL, rowsToAllocateBool - 1);
  assert(!workingPimMemory.empty());
  assert(rowsToAllocateBool - 1 == workingPimMemory.size());

  PimObjGrid tmpPimBoolGrid = pimAllocGridAssociated(rowsInSumCircularQueue[0], PIM_BOOL, 1);
  assert(!tmpPimBoolGrid.empty());
  PimObjId tmpPimBool = tmpPimBoolGrid[0];

  status = pimCopyHostToGrid(src_host.data(), workingPimMemory, 1, partitioning.tileWidth + 1, 1, partitioning.tileHeight + 1);
  assert(status == PIM_OK);

  status = pimCopyGridHalo(workingPimMemory, 1);
  assert(status == PIM_OK);


  for(size_t iter = 0; iter < iterations; ++iter) {
    computeGOLChunkIteration(workingPimMemory, rowsInSumCircularQueue, tmpPimBool);

    if(iter < iterations - 1) { // Only need to copy halo if not the last iteration
      status = pimCopyGridHalo(workingPimMemory, 1);
      assert(status == PIM_OK);
    }
  }

  // Only copy back the non-halo region
  status = pimCopyGridToHost(workingPimMemory, dst_host.data(), 1, partitioning.tileWidth + 1, 1, partitioning.tileHeight + 1);
  assert(status == PIM_OK);

  // dest should now have the results of the stencil computation

  status = pimFreeGrid(workingPimMemory);
  assert(status == PIM_OK);

  status = pimFreeGrid(rowsInSumCircularQueue);
  assert(status == PIM_OK);

  status = pimFreeGrid(tmpPimBoolGrid);
  assert(status == PIM_OK);
}

int main(int argc, char* argv[])
{
  struct Params params = getInputParams(argc, argv);
  std::cout << "Running PIM game of life for board: " << params.width << "x" << params.height << "\n";
  std::cout << "Number of Iterations: " << params.iterations << std::endl;

  std::vector<uint8_t> x_(params.height * params.width);
  std::vector<uint8_t> y_(x_.size());

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
      std::uniform_int_distribution<uint8_t> dist(0, 1);
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

  uint64_t coreHeight = deviceProp.numRowPerCore;

  uint64_t coreWidth = deviceProp.numColPerSubarray;

  std::span<uint8_t> x(x_);
  std::span<uint8_t> y(y_);

  game_of_life(x, y, params.width, params.iterations, deviceProp.numPIMCores, coreHeight, coreWidth, deviceProp.isHLayoutDevice);

  if (params.shouldVerify)
  {
    std::vector<uint8_t> cpuY_(y.size());
    std::span<uint8_t> cpuY(cpuY_);

    const auto golCpuKernel = [](const std::span<uint8_t> &stencilSrc, const uint64_t stencilWidth,
                                       const uint64_t gridX, const uint64_t gridY, const uint64_t stencilRadius) -> uint8_t {
        uint8_t sum = 0;
        for(uint64_t stencilY=gridY-stencilRadius; stencilY<=gridY+stencilRadius; ++stencilY) {
          for(uint64_t stencilX=gridX-stencilRadius; stencilX<=gridX+stencilRadius; ++stencilX) {
            sum += stencilSrc[stencilY * stencilWidth + stencilX];
          }
        }
        uint8_t res = (sum == 3) | (sum == 4 && stencilSrc[gridY * stencilWidth + gridX] == 1);
        return res;
      };

    stencilCpu(x, cpuY, params.iterations, 1, params.width, params.height, golCpuKernel);
    bool ok = true;

    // Only compute when stencil is fully in range
    const uint64_t startY = params.iterations;
    const uint64_t endY = params.height - startY;
    const uint64_t startX = params.iterations;
    const uint64_t endX = params.width - startX;

#if defined(_OPENMP)
#pragma omp parallel for collapse(2)
#endif
    for(uint64_t gridY=startY; gridY<endY; ++gridY) {
      for(uint64_t gridX=startX; gridX<endX; ++gridX) {
        if (cpuY[gridY * params.width + gridX] != y[gridY * params.width + gridX])
        {
#if defined(_OPENMP)
#pragma omp critical
#endif
          {
            std::cout << "Wrong answer: " << static_cast<unsigned>(y[gridY * params.width + gridX]) << " (expected " << static_cast<unsigned>(cpuY[gridY * params.width + gridX]) << ") at position (" << gridX << ", " << gridY << ")" << std::endl;
            ok = false;
            assert(0);
          }
        }
      }
    }
    if(ok) {
      std::cout << "Correct for Game of Life!" << std::endl;
    }
  }

  pimShowStats();

  return 0;
}