#!/bin/bash
set -e

build_type="sequential"
mkl_interface="lp64"

# Check if MKL build directory is provided
if [ ! -z "$3" ]; then
    mkl_builder_dir=${3}
else
    mkl_builder_dir="/opt/intel/oneapi/mkl/latest/share/mkl/tools/builder/"
    echo "Using default MKL builder path (${mkl_builder_dir})"
fi

# Define output dir
output_dir="`pwd`/../saf_custom_libs/"
mkdir -p $output_dir

# Define output and MKL build directories
if [[ "$OSTYPE" == "linux"* ]]; then
    if ! [ -d ${mkl_builder_dir} ]; then
        echo "Error: Intel MKL not installed"
        exit 1
    fi

else
    echo "Error: unknown OS"
    exit 1
fi

# Current path
parent_path="../submodules/Spatial_Audio_Framework/scripts"

# copy saf_mkl_list
cp ${parent_path}/saf_mkl_list ${mkl_builder_dir}

echo "Configuration: Builder ${mkl_builder_dir}, ${build_type} ${mkl_interface}"

# build custom library
(cd ${mkl_builder_dir} && make libintel64 interface=${mkl_interface} threading=sequential name="libsaf_mkl_custom_${mkl_interface}" export=saf_mkl_list)

# copy library
(cd ${mkl_builder_dir} && mv "libsaf_mkl_custom_${mkl_interface}.so" ${output_dir})

echo "Installed libsaf_mkl_custom_${mkl_interface} into ${output_dir}"

set +e
