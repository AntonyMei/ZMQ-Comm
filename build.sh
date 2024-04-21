#!/bin/bash

# Stop execution if any command fails
set -e

# Remove the existing build directory completely
echo "Removing existing build directory..."
rm -rf build

# Create a fresh build directory
echo "Creating a new build directory..."
mkdir -p build
cd build

# Run CMake to configure the build environment
echo "Configuring build with CMake..."
cmake -DCMAKE_BUILD_TYPE=Release .. || { echo "CMake configuration failed"; exit 1; }
#cmake -DCMAKE_BUILD_TYPE=Debug .. || { echo "CMake configuration failed"; exit 1; }

# Run Make to build the project with 8 threads
echo "Building with Make..."
make -j8 || { echo "Make failed"; exit 1; }

# Return to the original directory
cd ..

# Install the Python module
echo "Installing the Python module..."
python setup.py install || { echo "Python module installation failed"; exit 1; }

echo "Build and installation completed successfully."
