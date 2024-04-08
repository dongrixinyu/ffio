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
    try:
        result = cuda_c_lib.check_if_cuda_is_available()
    except:
        print('nvidia-smi(nvcc) is not installed.')
        return False

    if result == 0:
        return True

    elif result == -1:
        print('nvidia-smi(nvcc) is installed, but detect no gpu.')
        return False

    elif result == -2:
        print('nvidia-smi(nvcc) is not installed.')
        return False

    else:
        return False
