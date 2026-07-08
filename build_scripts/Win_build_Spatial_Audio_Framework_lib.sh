#!/usr/bin/env bash

# Fix SAF memory leaks
echo "Fixing SAF memory leaks"
./_fix_saf_mem_leaks.sh

export LIBSAF_PATH=`pwd`"/../submodules/Spatial_Audio_Framework"
export LIBSAF_BUILD_PATH="$LIBSAF_PATH/build"

cd $LIBSAF_PATH
mkdir -p $LIBSAF_BUILD_PATH

# Building

# configure
# This will build for VS 2026
# To build for VS 2022 use the commented line instead.
# cmake -S . -B build -G "Visual Studio 17 2022" -A x64 \
cmake -S . -B build \
	-DCMAKE_CXX_STANDARD=20 \
	-DCMAKE_CXX_STANDARD_REQUIRED=ON \
	-DCMAKE_CXX_EXTENSIONS=OFF \
	-DSAF_PERFORMANCE_LIB=SAF_USE_INTEL_MKL_LP64 \
	-DSAF_ENABLE_SOFA_READER_MODULE="1"
 
cmake --build build --config Debug
cmake --build build --config Release

# Reset SAF code
echo "Resetting SAF code"
cd "$LIBSAF_PATH"
git checkout .