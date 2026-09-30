# Parallel Image Processing Using Top-Hat Transform

## Overview
This project demonstrates the implementation of a Top-Hat morphological filter for image processing, written in C++. The main objective is not just the computer vision algorithm itself, but rather to showcase the impact of different parallel computing architectures on program performance and execution speed.

The algorithm is implemented in four variants:
1. Sequential (CPU) – The classic, single-threaded approach (Baseline).
2. Multi-threaded (OpenMP) – Parallelization at the shared-memory level utilizing multi-core CPUs.
3. Distributed (MPI) – Splitting the image into blocks and distributing tasks to independent processes/nodes in a compute cluster.
4. GPU Accelerated (CUDA) – SIMT-style parallelization utilizing thousands of cores on an NVIDIA graphics card.

## Algorithm
Every variant performs the same sequence of pixel-level operations on the input image:
1. Padding - Adding a border to protect image edges during kernel operations.
2. Erosion - Applying a minimum filter within a 5x5 pixel window.
3. Dilation - Applying a maximum filter within a 5x5 pixel window on the eroded image.
4. Top-Hat Transform - Subtracting the result of the morphological opening (erosion followed by dilation) from the original image. This highlights small elements and adjusts local contrast.

The code features a built-in timing mechanism that measures the execution timeand calculates the speedup provided by each parallelization technology.
