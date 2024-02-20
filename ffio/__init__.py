# -*- coding=utf-8 -*-
# Library: ffio
# Author: dongrixinyu
# License: MIT
# Email: dongrixinyu.66@gmail.com
# Github: https://github.com/dongrixinyu/ffio
# Description: An easy-to-use Python wrapper for FFmpeg-C-API.
# Website: http://www.jionlp.com

import os
import pdb
import time
import ctypes

__version__ = '1.0.3'

HOME_DIR = os.path.expanduser('~')
LOG_DIR = os.path.join(HOME_DIR, '.cache/ffio')

if not os.path.exists(LOG_DIR):
    os.makedirs(LOG_DIR)

# set logger
from ffio.util.logger import set_logger

logging = set_logger(level='INFO')

from ffio.util import *
from ffio.input_stream_parser import InputStreamParser
from ffio.output_stream_parser import OutputStreamParser

