#!/bin/bash

# Define the path to the Dreamcast toolchain file
export KOS_CMAKE_TOOLCHAIN="/opt/toolchains/dc/kos/utils/cmake/dreamcast.toolchain.cmake"

# Define the source directory and build directory
SOURCE_DIR="${PWD}/.."
BUILD_DIR="${PWD}/build"

# Create the build directory if it doesn't exist
mkdir -p "$BUILD_DIR"

# Navigate to the build directory
cd "$BUILD_DIR"

# Optional: Run cleanup
if [ "$1" == "clean" ]; then
    echo "Cleaning build directory..."
    rm -rf CMakeFiles CMakeCache.txt Makefile
    exit 0
fi

# Run CMake to configure the project
cmake -DCMAKE_TOOLCHAIN_FILE="$KOS_CMAKE_TOOLCHAIN" "$SOURCE_DIR"
#cmake -DCMAKE_TOOLCHAIN_FILE="$KOS_CMAKE_TOOLCHAIN"  -DCMAKE_VERBOSE_MAKEFILE=ON --trace --debug-output "$SOURCE_DIR"

# Build the project
make

# Optional: Run tests or other commands here

# Print a message indicating the build is complete
echo "Dreamcast build complete!"
