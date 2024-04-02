# -*- coding=utf-8 -*-
# Library: ffio
# Author: dongrixinyu, koisi
# License: MIT
# Email: dongrixinyu.66@gmail.com
# Github: https://github.com/dongrixinyu/ffio
# Description: An easy-to-use Python wrapper for FFmpeg-C-API.
# Website: http://www.jionlp.com


__version__ = '2.0.2'

from ffio.ffio import FFIO, CCodecParams, FFIOMode
from ffio.ffio_check_cuda import cuda_is_available
