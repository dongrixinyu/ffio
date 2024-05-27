# -*- coding=utf-8 -*-
# Library: ffio
# Author: dongrixinyu, koisi
# License: MIT
# Email: dongrixinyu.66@gmail.com
# Github: https://github.com/dongrixinyu/ffio
# Description: An easy-to-use Python wrapper for FFmpeg-C-API.
# Website: http://www.jionlp.com


from ctypes  import POINTER, byref

from ffio.ffio_c import cuda_c_lib


def cuda_is_available():
    # check if cuda is available for ffio
    try:
        result = cuda_c_lib.check_if_cuda_is_available()
    except:
        print('[ffio][py] nvidia-smi(nvcc) is not installed.')
        return False

    if result == 0:
        return True

    elif result == -1:
        print('[ffio][py] nvidia-smi(nvcc) is installed, but detect no gpu.')
        return False

    elif result == -2:
        print('[ffio][py] nvidia-smi(nvcc) is not installed.')
        return False

    else:
        print('[ffio][py] other exceptions.')
        return False


def available_gpu_memory():
    # check the available gpu memory left for ffio
    # the unit of result is `MB`, if cuda is not available or memory exhausted, return 0.
    try:
        result = cuda_c_lib.available_gpu_memory()
        return result
    except:
        print('[ffio][py] nvidia-smi(nvcc) is not installed.')
        return 0
