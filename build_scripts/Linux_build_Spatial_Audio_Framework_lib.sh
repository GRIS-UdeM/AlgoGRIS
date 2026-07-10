#!/usr/bin/env bash

# Fix SAF memory leaks
echo "Fixing SAF memory leaks"
./_fix_saf_mem_leaks.sh

export LIBSAF_PATH=`pwd`"/../submodules/Spatial_Audio_Framework"
export LIBSAFMKL_PATH=`pwd`"/../saf_custom_libs"

cd $LIBSAF_PATH

cmake -S . -B build/build-debug \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_STANDARD=20 \
  -DCMAKE_CXX_STANDARD_REQUIRED=ON \
  -DCMAKE_CXX_EXTENSIONS=OFF \
  -DINTEL_MKL_LIB="$LIBSAFMKL_PATH/libsaf_mkl_custom_lp64.so" \
  -DSAF_PERFORMANCE_LIB=SAF_USE_INTEL_MKL_LP64 \
  -DSAF_ENABLE_SOFA_READER_MODULE="1"

cmake --build build/build-debug

cmake -S . -B build/build-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_STANDARD=20 \
  -DCMAKE_CXX_STANDARD_REQUIRED=ON \
  -DCMAKE_CXX_EXTENSIONS=OFF \
  -DINTEL_MKL_LIB="$LIBSAFMKL_PATH/libsaf_mkl_custom_lp64.so" \
  -DSAF_PERFORMANCE_LIB=SAF_USE_INTEL_MKL_LP64 \
  -DSAF_ENABLE_SOFA_READER_MODULE="1"

cmake --build build/build-release

# Reset SAF code
echo "Resetting SAF code"
cd "$LIBSAF_PATH"
git checkout .
