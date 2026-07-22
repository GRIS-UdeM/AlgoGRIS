#!/usr/bin/env bash

# Fix SAF memory leaks
echo "Fixing SAF memory leaks"
./_fix_saf_mem_leaks.sh

export LIBSAF_PATH=`pwd`"/../submodules/Spatial_Audio_Framework"
export LIBSAF_BUILD_PATH="$LIBSAF_PATH/build"

cd $LIBSAF_PATH

# Configure and Build

# If building with oneMKL
# This will build for VS 2026
# cmake -S . -B build \
	# -DCMAKE_CXX_STANDARD=20 \
	# -DCMAKE_CXX_STANDARD_REQUIRED=ON \
	# -DCMAKE_CXX_EXTENSIONS=OFF \
	# -DSAF_PERFORMANCE_LIB=SAF_USE_INTEL_MKL_LP64 \
	# -DSAF_ENABLE_SOFA_READER_MODULE="1"

# OpenBLAS/LAPACKE
# This will build for VS 2026
cmake -S . -B build-debug \
  -DCMAKE_CXX_STANDARD=20 \
  -DCMAKE_CXX_STANDARD_REQUIRED=ON \
  -DCMAKE_CXX_EXTENSIONS=OFF \
  -DSAF_PERFORMANCE_LIB=SAF_USE_OPEN_BLAS_AND_LAPACKE \
  -DOPENBLAS_HEADER_PATH="$LIBSAF_PATH/lapack/CBLAS/include" \
  -DLAPACKE_HEADER_PATH="$LIBSAF_PATH/lapack/LAPACKE/include" \
  -DOPENBLAS_LIBRARY="$LIBSAF_PATH/lapack/build-debug/lib/libblas.lib" \
  -DLAPACKE_LIBRARY="$LIBSAF_PATH/lapack/build-debug/lib/liblapacke.lib" \
  -DSAF_ENABLE_SOFA_READER_MODULE=1

cmake --build build-debug --config Debug

cmake -S . -B build-release \
  -DCMAKE_CXX_STANDARD=20 \
  -DCMAKE_CXX_STANDARD_REQUIRED=ON \
  -DCMAKE_CXX_EXTENSIONS=OFF \
  -DSAF_PERFORMANCE_LIB=SAF_USE_OPEN_BLAS_AND_LAPACKE \
  -DOPENBLAS_HEADER_PATH="$LIBSAF_PATH/lapack/CBLAS/include" \
  -DLAPACKE_HEADER_PATH="$LIBSAF_PATH/lapack/LAPACKE/include" \
  -DOPENBLAS_LIBRARY="$LIBSAF_PATH/lapack/build/lib/libblas.lib" \
  -DLAPACKE_LIBRARY="$LIBSAF_PATH/lapack/build/lib/liblapacke.lib" \
  -DSAF_ENABLE_SOFA_READER_MODULE=1
 
cmake --build build-release --config Release

# Reset SAF code
echo "Resetting SAF code"
cd "$LIBSAF_PATH"
git checkout .