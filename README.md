# RosettaHack x87

## Overview

This is an experimental project that modifies Apple's Rosetta technology to use less precise but significantly faster x87 instruction handlers.

**⚠️ Warning: This is not ready for end-users. Use at your own risk.**

## Prerequisites

- macOS 26 or later
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

You will see a popup asking you to authorize debugging. Once approved, the process granted debug session.
Reference: [Debugging tool entitlement](https://developer.apple.com/documentation/bundleresources/entitlements/com.apple.security.cs.debugger)

Alternatively, you can disable `Debugging Restrictions` in System Integrity Protection (SIP) by running `csrutil enable --without debug` in macOS Recovery.

## Technical Details

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
