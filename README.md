# Ring Buffer Project

A C99 implementation of a File Control Block (FCB) system with flash memory operations, including CRC generation and comprehensive unit tests.

## Features

- **File Control Block (FCB)** - Manages file metadata and operations
- **Flash Memory Abstraction** - Simulated flash memory interface
- **Flash Operations** - Read, write, and append operations on flash storage
- **CRC Generation** - Cyclic redundancy check for data integrity
- **Comprehensive Tests** - Unit tests for initialization, read/write, and append operations

## Prerequisites

- C99-compatible compiler (GCC, Clang, MSVC)
- CMake 4.2 or higher
- Git (for dependency fetching)

## Building

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Running Tests

Tests are automatically run as part of the build process. To run tests manually:

```bash
cd build
ctest --output-on-failure
```

Or run the test executable directly:

```bash
./test_fcb
```

## Project Structure

```
├── fcb/                 # File Control Block module
├── flash_mem/           # Flash memory abstraction
├── flash_ops/           # Flash operations (read, write, append)
├── crc_gen/             # CRC generation utilities
├── main.c               # Test runner
├── test_fcb_*.c/h       # Unit test implementations
└── CMakeLists.txt       # Build configuration
```

## Testing

The project uses the [Unity](https://github.com/ThrowTheSwitch/Unity) test framework. Tests cover:

- **FCB Initialization** - File Control Block setup and configuration
- **Read/Write Operations** - Flash memory data operations
- **Append Operations** - Sequential data appending

## License

This project is licensed under the **GNU General Public License v2.0 (GPL-2.0)** — the same license as the Linux kernel. See [LICENSE](LICENSE) file for details.

Under GPL-2.0, any derivative work or distribution must be released under the same license and include source code. This ensures the project remains open and free for everyone.
