#!/bin/bash

if [ ! -d ffio/build ]; then
    mkdir ffio/build
else
    rm -rf ffio/build/*
fi

# we assume that the include directory and the dynamic lib path is located under /usr.
# This ffmepg_include_path will be in effect:
# if you install ffmpeg lib by `apt install libavutil-dev` etc.
# or if you install ffmpeg by the given Dockerfile located in this repo.
avutil_header_file_path=`find /usr -name avutil.h`
avutil_include_path=`dirname ${avutil_header_file_path}`
ffmpeg_include_path=`dirname ${avutil_include_path}`
echo "FFMPEG_INCLUDE_PATH: ${ffmpeg_include_path}"

avutil_lib_file_path=`find /usr -name libavutil.so`
ffmpeg_lib_dir_path=`dirname ${avutil_lib_file_path}`
echo "FFMPEG_LIB_DIR_PATH: ${ffmpeg_lib_dir_path}"

cd ffio/build
cmake ../.. \
    -DPYTHON_INCLUDE_DIRS=$(python -c "from distutils.sysconfig import get_python_inc; print(get_python_inc())")  \
    -DPYTHON_LIBRARIES=$(python -c "import distutils.sysconfig as sysconfig; print(sysconfig.get_config_var('LIBDIR') + '/' + sysconfig.get_config_var('LDLIBRARY'))") \
    -DFFMPEG_INCLUDE_DIRS=${ffmpeg_include_path} \
    -DFFMPEG_LIB_DIR_PATH=${ffmpeg_lib_dir_path}
    # -DFFMPEG_LIBRARIES=${ffmpeg_base_dir}/lib

make
