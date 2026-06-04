#!/bin/sh

export LIBSAF_PATH=`pwd`"/submodules/Spatial_Audio_Framework"
export LIBSAF_BUILD_PATH="$LIBSAF_PATH/build"

cd $LIBSAF_PATH
mkdir $LIBSAF_BUILD_PATH

# Targetting MacOS 11.5
sed -i '' -E 's/(Targeting MacOS )[0-9.]+/\111.5./; s/(CMAKE_OSX_DEPLOYMENT_TARGET ")[0-9.]+(")/\111.5\2/' CMakeLists.txt

# configure
cmake -S . -B build \
  -DCMAKE_CXX_STANDARD=20 \
  -DCMAKE_CXX_STANDARD_REQUIRED=ON \
  -DCMAKE_CXX_EXTENSIONS=OFF \
  -DSAF_PERFORMANCE_LIB=SAF_USE_APPLE_ACCELERATE_ILP64 \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DSAF_ENABLE_SOFA_READER_MODULE=1

# build
cmake --build build --config Release

# Resetting CMakeLists.txt
sed -i '' -E 's/(Targeting MacOS )[0-9.]+/\112.0./; s/(CMAKE_OSX_DEPLOYMENT_TARGET ")[0-9.]+(")/\112.0\2/' CMakeLists.txt