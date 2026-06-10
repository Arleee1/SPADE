// Test: C++ version of stencil on CPU
// Copyright (c) 2026 University of Virginia
// This file is licensed under the MIT License.
// See the LICENSE file in the root of this repository for more details.

#include <chrono>
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

#include "utilBaselines.h"
#include "utilStencil.h"

// Params ---------------------------------------------------------------------
typedef struct Params
{
  uint64_t iterations;
  uint64_t gridWidth;
  uint64_t gridHeight;
  uint64_t radius;
  const char *inputFile;
  const char *stencilPatternName;
  StencilPattern stencilPattern;
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
          "\n    -i    input file containing a 2d array (default=random)"
          "\n    -p    stencil pattern (options: BOX, STAR) (default=BOX)"
          "\n");
}

struct Params getInputParams(int argc, char **argv)
{
  struct Params p;
  p.iterations = 10;
  p.gridWidth = 10000;
  p.gridHeight = 10000;
  p.radius = 2;
  p.inputFile = nullptr;
  p.stencilPatternName = "BOX";
  p.stencilPattern = STENCIL_PATTERN_BOX;

  int opt;
  while ((opt = getopt(argc, argv, "h:n:x:y:r:i:p:")) >= 0)
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
    case 'i':
      p.inputFile = optarg;
      break;
    case 'p':
      if (strcmp(optarg, "BOX") == 0) {
        p.stencilPattern = STENCIL_PATTERN_BOX;
      } else if (strcmp(optarg, "STAR") == 0) {
        p.stencilPattern = STENCIL_PATTERN_STAR;
      } else {
        fprintf(stderr, "\nUnrecognized stencil pattern!\n");
        usage();
        exit(0);
      }
      p.stencilPatternName = optarg;
      break;
    default:
      fprintf(stderr, "\nUnrecognized option!\n");
      usage();
      exit(0);
    }
  }
  return p;
}

int main(int argc, char* argv[])
{
  struct Params params = getInputParams(argc, argv);

  if(params.radius == 0) {
    std::cout << "Stencil radius must not be 0, please provide a different radius." << std::endl;
    return 1;
  }

  std::cout << "Running CPU stencil for grid: " << params.gridHeight << "x" << params.gridWidth << std::endl;
  std::cout << "Stencil Radius: " << params.radius << ", Number of Iterations: " << params.iterations << std::endl;
  std::cout << "Stencil Pattern Type: " << params.stencilPatternName << std::endl;

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

  const uint64_t stencilAreaInt = calculateStencilAreaInt(params.stencilPattern, params.radius);
  const float stencilAreaInverseFloat = 1.0f / static_cast<float>(stencilAreaInt);

  const auto stencilCpuKernelBox = [stencilAreaInverseFloat](const std::span<float> &stencilSrc, const uint64_t stencilWidth,
                                        const uint64_t gridX, const uint64_t gridY, const uint64_t stencilRadius) -> float {
              float resCPU = 0.0f;
              for(uint64_t stencilY=gridY-stencilRadius; stencilY<=gridY+stencilRadius; ++stencilY) {
                for(uint64_t stencilX=gridX-stencilRadius; stencilX<=gridX+stencilRadius; ++stencilX) {
                  resCPU += stencilSrc[stencilY * stencilWidth + stencilX];
                }
              }
              return resCPU * stencilAreaInverseFloat;
            };

  const auto stencilCpuKernelStar = [stencilAreaInverseFloat](const std::span<float> &stencilSrc, const uint64_t stencilWidth,
                                         const uint64_t gridX, const uint64_t gridY, const uint64_t stencilRadius) -> float {
               float resCPU = stencilSrc[gridY * stencilWidth + gridX];
               for(uint64_t offset=1; offset<=stencilRadius; ++offset) {
                 resCPU += stencilSrc[(gridY - offset) * stencilWidth + gridX];
                 resCPU += stencilSrc[(gridY + offset) * stencilWidth + gridX];
                 resCPU += stencilSrc[gridY * stencilWidth + (gridX - offset)];
                 resCPU += stencilSrc[gridY * stencilWidth + (gridX + offset)];
               }
               return resCPU * stencilAreaInverseFloat;
             };

  std::span<float> x(x_);
  std::span<float> y(y_);

  auto start = std::chrono::high_resolution_clock::now();

  for (int32_t i = 0; i < WARMUP; i++)
  {
    if(params.stencilPattern == STENCIL_PATTERN_BOX) {
      stencilCpu(x, y, params.iterations, params.radius, params.gridWidth, params.gridHeight, stencilCpuKernelBox);
    } else if(params.stencilPattern == STENCIL_PATTERN_STAR) {
      stencilCpu(x, y, params.iterations, params.radius, params.gridWidth, params.gridHeight, stencilCpuKernelStar);
    } else {
      std::cerr << "Unrecognized stencil pattern!" << std::endl;
      std::exit(1);
    }
  }

  auto end = std::chrono::high_resolution_clock::now();

  std::chrono::duration<double, std::milli> elapsedTime = (end - start)/WARMUP;
  std::cout << "Duration: " << std::fixed << std::setprecision(3) << elapsedTime.count() << " ms." << std::endl;

  return 0;
}