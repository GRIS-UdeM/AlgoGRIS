#!/usr/bin/env bash

# Fix SAF memory leaks
echo "Fixing SAF memory leaks"
./_fix_saf_mem_leaks.sh

export LIBSAF_PATH=`pwd`"/../submodules/Spatial_Audio_Framework"

CBLAS_H_PATH="$(find /usr -type f -name 'cblas.h' 2>/dev/null | head -n 1)"
if [[ -z "$CBLAS_H_PATH" ]]; then
  echo "Error: cblas.h not found under /usr"
  exit 1
fi

CBLAS_INCLUDE_DIR="$(dirname "$CBLAS_H_PATH")"
echo "Found cblas.h at: $CBLAS_H_PATH"
echo "Using include dir: $CBLAS_INCLUDE_DIR"

cd $LIBSAF_PATH

cmake -S . -B build/build-debug \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_STANDARD=20 \
  -DCMAKE_CXX_STANDARD_REQUIRED=ON \
  -DCMAKE_CXX_EXTENSIONS=OFF \
  -DSAF_PERFORMANCE_LIB=SAF_USE_OPEN_BLAS_AND_LAPACKE \
  -DOPENBLAS_HEADER_PATH="$CBLAS_INCLUDE_DIR" \
  -DSAF_USE_FFTW="1" \
  -DSAF_ENABLE_SIMD="1" \
  -DSAF_ENABLE_SOFA_READER_MODULE="1"

cmake --build build/build-debug

cmake -S . -B build/build-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_STANDARD=20 \
  -DCMAKE_CXX_STANDARD_REQUIRED=ON \
  -DCMAKE_CXX_EXTENSIONS=OFF \
  -DSAF_PERFORMANCE_LIB=SAF_USE_OPEN_BLAS_AND_LAPACKE \
  -DOPENBLAS_HEADER_PATH="$CBLAS_INCLUDE_DIR" \
  -DSAF_USE_FFTW="1" \
  -DSAF_ENABLE_SIMD="1" \
  -DSAF_ENABLE_SOFA_READER_MODULE="1"

cmake --build build/build-release

# Reset SAF code
echo "Resetting SAF code"
cd "$LIBSAF_PATH"
git checkout .
