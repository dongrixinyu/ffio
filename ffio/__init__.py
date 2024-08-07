# -*- coding=utf-8 -*-
# Library: ffio
# Author: dongrixinyu, koisi
# License: MIT
# Email: dongrixinyu.66@gmail.com
# Github: https://github.com/dongrixinyu/ffio
# Description: An easy-to-use Python wrapper for FFmpeg-C-API.
# Website: http://www.jionlp.com


__version__ = '2.0.3'

from ffio.ffio_check_cuda import cuda_is_available, available_gpu_memory
from ffio.ffio import FFIO, CCodecParams, FFIOMode
