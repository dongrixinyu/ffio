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

lib_interface_api.newStreamObject.argtypes = []
lib_interface_api.newStreamObject.restype = ctypes.c_void_p

lib_interface_api.deleteStreamObject.argtypes = [ctypes.c_void_p]
lib_interface_api.deleteStreamObject.restype = ctypes.c_void_p

# the source stream path must be in byte format, not Unicode
lib_interface_api.init.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
lib_interface_api.init.restype = ctypes.c_void_p

lib_interface_api.finalize.argtypes = [ctypes.c_void_p]
lib_interface_api.finalize.restype = ctypes.c_void_p

lib_interface_api.getStreamState.argtypes = [ctypes.c_void_p]
lib_interface_api.getStreamState.restype = ctypes.c_int

lib_interface_api.getWidth.argtypes = [ctypes.c_void_p]
lib_interface_api.getWidth.restype = ctypes.c_int

lib_interface_api.getHeight.argtypes = [ctypes.c_void_p]
lib_interface_api.getHeight.restype = ctypes.c_int

lib_interface_api.getAverageFPS.argtypes = [ctypes.c_void_p]
lib_interface_api.getAverageFPS.restype = ctypes.c_float

lib_interface_api.getOneFrame.argtypes = [ctypes.c_void_p]
lib_interface_api.getOneFrame.restype = ctypes.py_object
# lib_interface_api.getOneFrame.restype = ctypes.c_int


class StreamParser(object):
    """StreamParser: """
    def __init__(self, stream_path):

        # allocate memory to new a streamObj
        self.streamObj = ctypes.c_void_p(lib_interface_api.newStreamObject())

        self.stream_path = stream_path

        start_time = time.time()
        # the input stream must be in byte format
        self.streamObj = ctypes.c_void_p(
            lib_interface_api.init(self.streamObj, stream_path.encode()))
        end_time = time.time()

        self.stream_state_flag = lib_interface_api.getStreamState(self.streamObj)

        if self.stream_state_flag == 1:  # 1 means successfully opening a stream context
            print("initialization cost {:.4f} seconds".format(end_time-start_time))

            self.stream_video_width = lib_interface_api.getWidth(self.streamObj)
            self.stream_video_height = lib_interface_api.getHeight(self.streamObj)
            self.stream_video_average_fps = lib_interface_api.getAverageFPS(self.streamObj)
            print("stream width: {}, height: {}, average fps: {}".format(
                self.stream_video_width, self.stream_video_height,
                self.stream_video_average_fps))

            self.stream_buffer_size = self.stream_video_height * self.stream_video_width * 3
            # self.image_buffer = ctypes.create_string_buffer(
            #     b"\0", self.stream_buffer_size)
            print("image buffer size = {} * {} * 3 = {}".format(
                self.stream_video_width, self.stream_video_height, 3, self.stream_buffer_size))

            self.frame_number = 0

        else:
            print("failed to initialize the stream. cost {:.4f} seconds".format(
                end_time-start_time))

            self.streamObj = lib_interface_api.deleteStreamObject(self.streamObj)

            # all these params should be set to 0 when failed to initialize
            self.stream_video_width = 0
            self.stream_video_height = 0
            self.stream_video_average_fps = 0
            self.stream_buffer_size = 0
            self.frame_number = 0

            # print(traceback.format_exc())
            # raise ValueError("Can not initialize the stream.")

    @property
    def width(self):
        return self.stream_video_width

    @property
    def height(self):
        return self.stream_video_height

    @property
    def fps(self):
        return self.stream_video_average_fps

    @property
    def stream_state(self):
        # 1 means successfully opening a stream context
        # 0 means failed to initialize it
        if self.stream_state_flag == 1:
            return True
        else:
            return False

    def get_one_frame(self, image_format='numpy'):
        """ image_format: numpy, Image, base64, None
        """

        frame_bytes = lib_interface_api.getOneFrame(self.streamObj)
        frame_bytes_type = type(frame_bytes)
        if frame_bytes_type is bytes:
            # has already get an image in python bytes format

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

        elif frame_bytes_type is int:
            if frame_bytes == -5:
                # it means that the stream is empty,
                # now you should close the stream context manually
                return -5
            elif frame_bytes == -4:
                # Packet mismatch. Unable to seek to the next packet
                # now you should close the stream context manually
                return -4
            else:
                # other errorsf
                return 1

    def release_memory(self):
        # have to release memory manually

        # uninitialize
        self.streamObj = lib_interface_api.finalize(self.streamObj)

        # delete the object
        self.streamObj = lib_interface_api.deleteStreamObject(self.streamObj)
