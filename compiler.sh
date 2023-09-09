#!/bin/bash

if [ ! -d build ]; then
    mkdir build
else
    rm -rf build/*
fi


ffmpeg_base_dir=/home/cuichengyu/FFmpeg-n4.4.1

cd build
cmake .. \
    -DPYTHON_INCLUDE_DIRS=$(python -c "from distutils.sysconfig import get_python_inc; print(get_python_inc())")  \
    -DPYTHON_LIBRARIES=$(python -c "import distutils.sysconfig as sysconfig; print(sysconfig.get_config_var('LIBDIR') + '/' + sysconfig.get_config_var('LDLIBRARY'))") \
    -DFFMPEG_INCLUDE_DIRS=${ffmpeg_base_dir}
    # -DFFMPEG_LIBRARIES=${ffmpeg_base_dir}/lib

make

# cd .. && python test_cpp.py
