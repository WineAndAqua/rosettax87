# RosettaHack x87

## Overview

This is an experimental project that modifies Apple's Rosetta technology to use less precise but significantly faster x87 instruction handlers. The benchmarks show approximately 4-5x performance improvement for x87 floating-point operations.

**⚠️ Warning: This is not ready for end-users. Use at your own risk.**

## Prerequisites

- macOS 15.5 or compatible
- C compiler (clang)
- CMake

## Building

### Main Project

```
cmake -B build
cmake --build build
```

### Sample Test Program

```clang -v -arch x86_64 -mno-sse -mfpmath=387 ./sample/math.c -o ./build/math```

## Running

Run the target program from the build folder:
```
./rosettax87 ./math
```

You might get a popup to confirm that the process is allowed to debug. This is a one time thing.

## Performance Benchmark

### Default Rosetta x87 handlers:
```log
╰─$ ./math
benchmark run_fsqrt
Result: 40800000
Run 1 time: 246664 ticks
Result: 40800000
Run 2 time: 233811 ticks
Result: 40800000
Run 3 time: 227264 ticks
Result: 40800000
Run 4 time: 225907 ticks
Result: 40800000
Run 5 time: 228230 ticks
Result: 40800000
Run 6 time: 227629 ticks
Result: 40800000
Run 7 time: 226741 ticks
Result: 40800000
Run 8 time: 227249 ticks
Result: 40800000
Run 9 time: 227047 ticks
Result: 40800000
Run 10 time: 226771 ticks

Average time: 229731 ticks
```

### Custom rosetta x87 handlers:
```log
╰─$ ./rosettax87 ./math
launching into program
RosettaRuntimex87 built Apr 23 2025 18:43:47
benchmark run_fsqrt
Result: 40800000
Run 1 time: 48682 ticks
Result: 40800000
Run 2 time: 48292 ticks
Result: 40800000
Run 3 time: 48306 ticks
Result: 40800000
Run 4 time: 48369 ticks
Result: 40800000
Run 5 time: 48230 ticks
Result: 40800000
Run 6 time: 48561 ticks
Result: 40800000
Run 7 time: 49638 ticks
Result: 40800000
Run 8 time: 48371 ticks
Result: 40800000
Run 9 time: 48313 ticks
Result: 40800000
Run 10 time: 48410 ticks

Average time: 48517 ticks
```

## Technical Details

### Research Notes

If you want to examine `runtime` and `libRosettaRuntime` using `IDA PRO`, you need to use `chain_fixup.py`.
- `libRosettaRuntime` is located at `/Library/Apple/usr/libexec/oah/libRosettaRuntime`.
- `runtime` is located at `/usr/libexec/rosetta/runtime`.

### Windows Applications Through Wine

You can use the brew `wine@devel` cask with RosettaHack x87. It supports launching Windows applications through Wine with an environment variable `ROSETTA_X87_PATH`.

1. Install `wine@devel` using [Homebrew](https://brew.sh/)

```bash
brew install --cask wine@devel
```

2. To permanently set the environment variable, add the following to your `~/.bashrc` or `~/.zshrc` file:
```bash
export ROSETTA_X87_PATH=/Path/To/rosettax87
```

3. Run the Windows application
```bash
wine PATH_TO_BINARY.exe
```

## License

This project is licensed under `MIT`.
