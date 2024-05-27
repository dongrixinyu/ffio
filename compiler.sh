#!/bin/bash

global_ffmpeg_include_path=""
global_ffmpeg_lib_path=""
global_python_include_path=""
global_python_lib_path=""
global_cuda_flag=0
global_cuda_include_path=""
global_cuda_lib_path=""

check_and_set_ffmpeg_path() {
  if command -v pkg-config &> /dev/null            \
    && pkg-config --cflags libavcodec &> /dev/null \
    && pkg-config --libs   libavcodec &> /dev/null ; then
      echo "[ffio_build] checking ffmpeg includes and libs path by pkg-config..."
      global_ffmpeg_include_path=$(pkg-config --cflags libavcodec 2> /dev/null | sed 's/-I//g')
      global_ffmpeg_lib_path=$(    pkg-config --libs   libavcodec 2> /dev/null | sed 's/-L//g' | awk '{print $1}')
  else
    echo "[ffio_build] checking ffmpeg includes path by searching the path of avutil.h..."
    local avutil_header_file_path=`find /usr -name avutil.h`
    local avutil_include_path=`dirname ${avutil_header_file_path}`
    global_ffmpeg_include_path=`dirname ${avutil_include_path}`

    echo "[ffio_build] checking ffmpeg libs path by searching the path of libavutil.so..."
    local avutil_lib_file_path=`find /usr -name libavutil.so`
    global_ffmpeg_lib_path=`dirname ${avutil_lib_file_path}`
  fi

  echo "  - find ffmpeg includes path: ${global_ffmpeg_include_path}"
  echo "  - find ffmpeg libs     path: ${global_ffmpeg_lib_path}"
}

check_and_set_python_path(){
  if command -v python3-config &> /dev/null   \
    && python3-config --includes &> /dev/null \
    && python3-config --ldflags  &> /dev/null ; then
      echo "[ffio_build] checking python3 includes and libs path by python3-config..."
      global_python_include_path=$(python3-config --includes 2> /dev/null | sed 's/-I//g' | awk '{print $1}')
      local python_lib_dir_path=$(python3-config --ldflags  2> /dev/null | sed 's/ /\n/g' | grep "\-L" | sed 's/-L//g')

      for vari in ${python_lib_dir_path}
      do
          res=$(ls -lht ${vari} | grep -P "libpython.+so")
          if [[ "$res" != "" ]]; then
              # echo "res: ${res}"
              global_python_lib_path=${vari}
              break
          fi;

      done

      # global_python_lib_path=`find $python_lib_dir_path -name 'libpython*' | grep -E "\.(dylib|so)$" | head -n 1`
  else
    echo "[ffio_build] checking python3 includes and libs path by trying python3 commands..."
    PYTHON_CMD=$(command -v python3 &> /dev/null && echo "python3" || echo "python")
    global_python_include_path=$($PYTHON_CMD -c "from distutils.sysconfig import get_python_inc; print(get_python_inc())")
    global_python_lib_path=$($PYTHON_CMD -c "import distutils.sysconfig as sysconfig;print(sysconfig.get_config_var('LIBDIR') + '/' + sysconfig.get_config_var('LDLIBRARY'))")
  fi

  echo "  - find python3 includes path: ${global_python_include_path}"
  echo "  - find python3 libs     path: ${global_python_lib_path}"
}

check_and_set_cuda_path(){

  if command -v nvcc &> /dev/null ; then
    echo "[ffio_build] checking cuda includes and libs path by nvcc ..."
    local nvcc_bin_path=`which nvcc`
    local nvcc_bin_dir_path=`dirname ${nvcc_bin_path}`
    local nvcc_base_dir_path=`dirname ${nvcc_bin_dir_path}`

    global_cuda_include_path="${nvcc_base_dir_path}/include"
    global_cuda_lib_path="${nvcc_base_dir_path}/lib64"
    # echo "#### ${global_cuda_include_path}"
    global_cuda_flag=1

  else
    echo "[ffio_build] checking cuda includes path by searching the path of cuda_runtime.h ..."
    # local nvcc_version=`nvcc --version | grep 'release' | awk -F '[ ,]' '{print $6}'`
    local cuda_runtime_lib_file_path=`find /usr -name libcudart.so`
    local cuda_runtime_header_file_path=`find /usr -name cuda_runtime.h`
    if [ -n "${cuda_runtime_header_file_path}" ]; then
      local cuda_runtime_include_path=`dirname ${cuda_runtime_header_file_path}`
      global_cuda_include_path=cuda_runtime_include_path
      local cuda_runtime_lib_path=`dirname ${cuda_runtime_lib_file_path}`
      global_cuda_lib_path=cuda_runtime_lib_path
      global_cuda_flag=1
    else
      global_cuda_flag=0
    fi
  fi

  echo "  - find cuda includes path: ${global_cuda_include_path}"
  echo "  - find cuda libs     path: ${global_cuda_lib_path}"
}

check_and_set_ffmpeg_path
check_and_set_python_path
check_and_set_cuda_path

# custom setting
global_ffmpeg_include_path=/home/cuichengyu/pyffmpeg_workspace/ffmpeg-6.0/include
global_ffmpeg_lib_path=/home/cuichengyu/pyffmpeg_workspace/ffmpeg-6.0/lib
# global_cuda_flag=0

if [ ! -d ffio/build ]; then
  mkdir ffio/build
else
  rm -rf ffio/build/*
fi

cd ffio/build
cmake ../.. \
    -DPYTHON_INCLUDE_DIRS=${global_python_include_path}  \
    -DPYTHON_LIBRARIES=${global_python_lib_path}         \
    -DFFMPEG_INCLUDE_DIRS=${global_ffmpeg_include_path}  \
    -DFFMPEG_LIB_DIR_PATH=${global_ffmpeg_lib_path} \
    -DCUDA_INCLUDE_DIRS=${global_cuda_include_path} \
    -DCUDA_LIB_DIR_PATH=${global_cuda_lib_path} \
    -DCHECK_CUDA=${global_cuda_flag}
make
