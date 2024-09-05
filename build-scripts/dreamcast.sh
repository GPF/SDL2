#!/bin/bash
# Define the path to the Dreamcast toolchain file
#export KOS_CMAKE_TOOLCHAIN="/opt/toolchains/dc/kos/utils/cmake/dreamcast.toolchain.cmake"
# Define the source directory and build directory
SOURCE_DIR="${PWD}/.."
BUILD_DIR="${PWD}/build"
# Default options
ENABLE_OPENGL=OFF
ENABLE_SDL_TESTS=ON
ENABLE_PTHREADS=ON

# Parse command-line arguments
while [[ "$#" -gt 0 ]]; do
case $1 in
        --enable-opengl) ENABLE_OPENGL=ON ;;
        --disable-opengl) ENABLE_OPENGL=OFF ;;
        --enable-sdl-tests) ENABLE_SDL_TESTS=ON ;;
        --disable-sdl-tests) ENABLE_SDL_TESTS=OFF ;;
        --enable-pthreads)  ENABLE_PTHREADS=ON ;;
        --disable-pthreads) ENABLE_PTHREADS=OFF ;;
        clean) 
            echo "Cleaning build directory..."
rm -rf CMakeFiles CMakeCache.txt Makefile
            exit 0
            ;;
        distclean)
            echo "Removing build directory..."
rm -rf "$BUILD_DIR"
exit 0
            ;;
        *) 
            echo "Unknown option: $1"
exit 1
            ;;
    esac
shift
done
# Create the build directory if it doesn't exist
mkdir -p "$BUILD_DIR"
# Navigate to the build directory
cd "$BUILD_DIR"
# Set CMake variables based on OpenGL and SDL_TESTS options
if [ "$ENABLE_OPENGL" == "ON" ]; then
CMAKE_OPTS="-DSDL_OPENGL=ON"
else
CMAKE_OPTS="-DSDL_OPENGL=OFF"
fi
if [ "$ENABLE_SDL_TESTS" == "ON" ]; then
CMAKE_OPTS="$CMAKE_OPTS -DSDL_TESTS=ON"
else
CMAKE_OPTS="$CMAKE_OPTS -DSDL_TESTS=OFF"
fi
if [ "$ENABLE_PTHREADS" == "ON" ]; then
    CMAKE_OPTS="$CMAKE_OPTS -DSDL_PTHREADS=ON"
else
    CMAKE_OPTS="$CMAKE_OPTS -DSDL_PTHREADS=OFF"
fi
# Run CMake to configure the project with the selected options
cmake -DCMAKE_TOOLCHAIN_FILE="$KOS_CMAKE_TOOLCHAIN" \
      -G "Unix Makefiles" \
      $CMAKE_OPTS \
      "$SOURCE_DIR"
# Build the project
make

# Optional: Run tests or other commands here
# Print a message indicating the build is complete
echo "Dreamcast build complete!"
