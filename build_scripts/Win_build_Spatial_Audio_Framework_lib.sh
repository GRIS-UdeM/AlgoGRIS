#!/usr/bin/env bash

# Fix SAF memory leaks
echo "Fixing SAF memory leaks"
./_fix_saf_mem_leaks.sh

export LIBSAF_PATH=`pwd`"/../submodules/Spatial_Audio_Framework"

cd $LIBSAF_PATH

# Download OpenBLAS from github releases
rm -rf OpenBLAS
curl -L -O https://github.com/OpenMathLib/OpenBLAS/releases/download/v0.3.34/OpenBLAS-0.3.34-x64.zip
unzip OpenBLAS-0.3.34-x64.zip -d OpenBLAS
rm OpenBLAS-0.3.34-x64.zip

# Configure and Build
# libopenblas.lib bundles lapack/lapacke libs

cmake -S . -B build-debug \
  -DCMAKE_CXX_STANDARD=20 \
  -DCMAKE_CXX_STANDARD_REQUIRED=ON \
  -DCMAKE_CXX_EXTENSIONS=OFF \
  -DSAF_PERFORMANCE_LIB=SAF_USE_OPEN_BLAS_AND_LAPACKE \
  -DOPENBLAS_HEADER_PATH="$LIBSAF_PATH/OpenBLAS/include" \
  -DLAPACKE_HEADER_PATH="$LIBSAF_PATH/OpenBLAS/include" \
  -DOPENBLAS_LIBRARY="$LIBSAF_PATH/OpenBLAS/lib/libopenblas.lib" \
  -DLAPACKE_LIBRARY="$LIBSAF_PATH/OpenBLAS/lib/libopenblas.lib" \
  -DSAF_ENABLE_SOFA_READER_MODULE=1

cmake --build build-debug --config Debug

cmake -S . -B build-release \
  -DCMAKE_CXX_STANDARD=20 \
  -DCMAKE_CXX_STANDARD_REQUIRED=ON \
  -DCMAKE_CXX_EXTENSIONS=OFF \
  -DSAF_PERFORMANCE_LIB=SAF_USE_OPEN_BLAS_AND_LAPACKE \
  -DOPENBLAS_HEADER_PATH="$LIBSAF_PATH/OpenBLAS/include" \
  -DLAPACKE_HEADER_PATH="$LIBSAF_PATH/OpenBLAS/include" \
  -DOPENBLAS_LIBRARY="$LIBSAF_PATH/OpenBLAS/lib/libopenblas.lib" \
  -DLAPACKE_LIBRARY="$LIBSAF_PATH/OpenBLAS/lib/libopenblas.lib" \
  -DSAF_ENABLE_SOFA_READER_MODULE=1
 
cmake --build build-release --config Release

# Reset SAF code
echo "Resetting SAF code"
cd "$LIBSAF_PATH"
git checkout .
