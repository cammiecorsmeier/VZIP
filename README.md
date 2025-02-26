# vzip - Multithreaded Compression Tool

## Overview
This project enhances the performance of a file compression tool using multithreading with the `pthread` library. The goal is to optimize the execution while ensuring correctness and compliance with project constraints.

## Requirements
- **Operating System**: Linux (Ubuntu 22.04 recommended)
- **Compiler**: `gcc` (version 11.4.0)
- **Libraries**:
  - `pthread`
  - `zlib` (install using `sudo apt install zlib1g-dev` if needed)

## Installation & Compilation
1. **Download and extract the package:**
   ```sh
   unzip project2.zip
   cd project2
   ```
2. **Build the tool:**
   ```sh
   make
   ```
3. **Run tests to verify correctness:**
   ```sh
   make test
   ```

## Implementation Details
- The original `vzip` code is optimized using `pthread` to introduce parallel processing.
- The number of active threads is capped at **20** (including the main thread).
- Synchronization mechanisms such as **mutex locks** are implemented to ensure thread safety.
- The input data is divided among multiple threads, each performing compression independently before merging results.

## Usage
To compress a file:
```sh
./vzip input.txt > output.vz
```
To decompress:
```sh
./vunzip output.vz > decompressed.txt
```

## Performance Optimization
- Data is split dynamically among threads based on system load.
- Threads execute independently and merge results efficiently.
- Mutex locking is used only where necessary to avoid unnecessary overhead.

## Evaluation
- **Speeding Factor**: The implementation aims for at least a 3x speed improvement over the original.
- **Code Style**: Code follows best practices with clear comments and modular functions.
- **Presentation**: A recorded video explains the approach, optimizations, and team contributions.

## Notes
- Any submission exceeding **20 threads** at any point is considered invalid.
- All code must be clean, properly commented, and structured according to best practices.

