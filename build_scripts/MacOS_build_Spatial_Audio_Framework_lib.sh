#!/bin/sh

# Fix SAF memory leaks
echo "Fixing SAF memory leaks"
./_fix_saf_mem_leaks.sh

export LIBSAF_PATH=`pwd`"/../submodules/Spatial_Audio_Framework"
export LIBSAF_BUILD_PATH="$LIBSAF_PATH/build"

cd $LIBSAF_PATH
mkdir -p $LIBSAF_BUILD_PATH

# Targetting MacOS 11.5
sed -i '' -E 's/Targeting MacOS [0-9.]+/Targeting MacOS 11.5/' CMakeLists.txt
sed -i '' -E 's/CMAKE_OSX_DEPLOYMENT_TARGET "[0-9.]+"/CMAKE_OSX_DEPLOYMENT_TARGET "11.5"/' CMakeLists.txt

# configure
cmake -S . -B build/build-debug \
  -DCMAKE_CXX_STANDARD=20 \
  -DCMAKE_CXX_STANDARD_REQUIRED=ON \
  -DCMAKE_CXX_EXTENSIONS=OFF \
  -DSAF_PERFORMANCE_LIB=SAF_USE_APPLE_ACCELERATE_LP64 \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DSAF_ENABLE_SOFA_READER_MODULE="1"

# build
cmake --build build/build-debug --config Debug

# configure
cmake -S . -B build/build-release \
  -DCMAKE_CXX_STANDARD=20 \
  -DCMAKE_CXX_STANDARD_REQUIRED=ON \
  -DCMAKE_CXX_EXTENSIONS=OFF \
  -DSAF_PERFORMANCE_LIB=SAF_USE_APPLE_ACCELERATE_LP64 \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DSAF_ENABLE_SOFA_READER_MODULE="1"

# build
cmake --build build/build-release --config Release

# Reset SAF code
echo "Resetting SAF code"
cd "$LIBSAF_PATH"
git checkout .

# Resetting CMakeLists.txt
#sed -i '' -E 's/(Targeting MacOS )[0-9.]+/\112.0./; s/(CMAKE_OSX_DEPLOYMENT_TARGET ")[0-9.]+(")/\112.0\2/' CMakeLists.txt