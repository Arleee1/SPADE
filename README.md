# SPADE (Stencil PIM And Data-layout framEwork)

![License](https://img.shields.io/badge/license-MIT-green.svg)

## Abstract
Stencil computing is a widely used high performance computing task that iteratively updates elements in a grid based on their neighbors. Stencil computing has a wide range of applications including image processing and solving differential equations. Stencil computations are typically memory-bound, meaning their runtime is dominated by memory access time, rather than compute time. This happens because the arithmetic intensity, or ratio of computation operations per memory operation, of stencil computing is typically low, which can cause the CPU to stall while waiting for memory accesses. Processing in Memory (PIM) is a computing architecture that places compute logic in DRAM. PIM can potentially accelerate memory-bound workloads. Because the compute logic is near to the data in memory, the memory access time is greatly reduced. Thus, this motivates using PIM for stencil computations.

Stencil computing is also highly parallelizable, as the updates for each element in the grid can be computed in parallel. PIM can also offer a high degree of parallel compute, as DRAM has a large number of potential places for compute logic. This is another motivation for performing stencil computations using PIM. When computing stencil in parallel, not specifically for PIM, it is often necessary to have frequent data transfers between Processing Elements (PEs), so that the updated values can be exchanged. Depending on the data-layout in PIM, the performance cost of these data transfers can vary. However, existing PIM APIs lack support for implementing stencil-like computations with these data transfer patterns.

Therefore we introduce SPADE. SPADE includes a framework for reducing inter-PE transfer costs for kernels with stencil-like data transfer patterns. SPADE also includes an API for creating stencil-like PIM kernels. Finally, SPADE provides an implementation of stencil computing for PIM using the SPADE API. We open source our work here.

## Acknowledgment
This project is a fork of [PIMeval](https://github.com/UVA-LavaLab/PIMeval-PIMbench) by F. A. Siddique et al., and it is infeasable to separate SPADE from PIMeval. PIMeval is liscenced under the MIT license, which allows the PIMeval code to be distributed here.

## References
F. A. Siddique et al., “Architectural modeling and benchmarking for digital dram
pim,” in 2024 IEEE International Symposium on Workload Characterization (IISWC),
2024, pp. 247–261. doi: 10.1109/IISWC63097.2024.00030.