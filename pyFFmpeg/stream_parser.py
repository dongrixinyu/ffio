# -*- coding=utf-8 -*-
# Library: pyFFmpeg
# Author: dongrixinyu
# License: MIT
# Email: dongrixinyu.89@163.com
# Github: https://github.com/dongrixinyu/pyFFmpeg
# Description: a simple Python wrapper for FFmpeg.'
# Website: http://www.jionlp.com

import os
import pdb
import sys
import time
import ctypes
import traceback

from PIL import Image
import numpy as np


DIR_PATH = os.path.dirname(os.path.abspath(__file__))
lib_interface_api = ctypes.PyDLL(
    os.path.join(DIR_PATH, 'build', 'libinterfaceAPI.so'))

# the source stream path must be in byte format, not Unicode
lib_interface_api.init.argtypes = [ctypes.c_char_p]
lib_interface_api.init.restype = ctypes.c_void_p

lib_interface_api.getWidth.argtypes = [ctypes.c_void_p]
lib_interface_api.getWidth.restype = ctypes.c_int

lib_interface_api.getHeight.argtypes = [ctypes.c_void_p]
lib_interface_api.getHeight.restype = ctypes.c_int

lib_interface_api.getAverageFPS.argtypes = [ctypes.c_void_p]
lib_interface_api.getAverageFPS.restype = ctypes.c_float

lib_interface_api.getOneFrame.argtypes = [ctypes.c_void_p]
lib_interface_api.getOneFrame.restype = ctypes.py_object


class StreamParser(object):
    """StreamParser: """
    def __init__(self, stream_path):

        self.stream_path = stream_path
        # the input stream must be in byte format
        try:
            start_time = time.time()
            self.streamObj = ctypes.c_void_p(lib_interface_api.init(stream_path.encode()))
            end_time = time.time()
            print("initialization cost {:.4f} seconds".format(end_time-start_time))

        except:
            print("failed to initialize the stream.")
            print(traceback.format_exc())
            self.streamObj = None
            sys.exit()

        self.stream_video_width = lib_interface_api.getWidth(self.streamObj)
        self.stream_video_height = lib_interface_api.getHeight(self.streamObj)
        self.stream_video_average_fps = lib_interface_api.getAverageFPS(self.streamObj)
        print("stream width: {}, height: {}, average fps: {}".format(
            self.stream_video_width, self.stream_video_height,
            self.stream_video_average_fps))

        self.frame_number = 0

    @property
    def width(self):
        return self.stream_video_width

    @property
    def height(self):
        return self.stream_video_height

    @property
    def fps(self):
        return self.stream_video_average_fps


    def get_one_frame(self, image_format='numpy'):
        """ image_format: numpy, Image, base64, None
        """

        frame_bytes = lib_interface_api.getOneFrame(self.streamObj)
        self.frame_number += 1

        if image_format is None:
            return frame_bytes

        elif image_format == 'numpy':
            # numpy method to convert the result buffer
            np_buffer = np.frombuffer(frame_bytes, dtype=np.uint8)
            np_frame = np.reshape(np_buffer, (self.stream_video_width, self.stream_video_height, 3))
            return np_frame

        elif image_format == 'Image':
            rgb_image = Image.frombytes(
                "RGB", (self.stream_video_width, self.stream_video_height), frame_bytes)
            return rgb_image

        elif image_format == 'base64':
            return None
