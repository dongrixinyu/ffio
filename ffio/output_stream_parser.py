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
import sys
import time
import ctypes
import traceback

import cv2
import base64
from PIL import Image
import numpy as np

from ffio import logging


DIR_PATH = os.path.dirname(os.path.abspath(__file__))
lib_interface_api = ctypes.PyDLL(
    os.path.join(DIR_PATH, 'build', 'libinterfaceAPI.so'))

lib_interface_api.newOutputStreamObject.argtypes = []
lib_interface_api.newOutputStreamObject.restype = ctypes.c_void_p

lib_interface_api.deleteOutputStreamObject.argtypes = [ctypes.c_void_p]
lib_interface_api.deleteOutputStreamObject.restype = ctypes.c_void_p

# the source stream path must be in byte format, not Unicode
lib_interface_api.initializeOutputStreamObject.argtypes = [
    ctypes.c_void_p, ctypes.c_char_p,
    ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int,
    ctypes.c_char_p, ctypes.c_int,
    ctypes.c_bool, ctypes.c_char_p, ctypes.c_int, ctypes.c_int
]
lib_interface_api.initializeOutputStreamObject.restype = ctypes.c_void_p

lib_interface_api.finalizeOutputStreamObject.argtypes = [ctypes.c_void_p]
lib_interface_api.finalizeOutputStreamObject.restype = ctypes.c_void_p

lib_interface_api.getOutputStreamState.argtypes = [ctypes.c_void_p]
lib_interface_api.getOutputStreamState.restype = ctypes.c_int

# lib_interface_api.getOutputVideoStreamWidth.argtypes = [ctypes.c_void_p]
# lib_interface_api.getOutputVideoStreamWidth.restype = ctypes.c_int

# lib_interface_api.getOutputVideoStreamHeight.argtypes = [ctypes.c_void_p]
# lib_interface_api.getOutputVideoStreamHeight.restype = ctypes.c_int

# lib_interface_api.getOutputVideoStreamAverageFPS.argtypes = [ctypes.c_void_p]
# lib_interface_api.getOutputVideoStreamAverageFPS.restype = ctypes.c_float

lib_interface_api.encode1Frame.argtypes = [ctypes.c_void_p, ctypes.py_object]
lib_interface_api.encode1Frame.restype = ctypes.c_int


class OutputStreamParser(object):
    """OutputStreamParser: a Python wrapper for FFmpeg to initialize an output video stream.


    Args:
        output_stream_path: path you must designate. rtmp, mp4, rtsp, srt, flv format etc.
        preset: param concerning encoding:
            ultrafast,superfast,veryfast,faster,fast,medium,slow,slower,veryslow,placebo
            `ultrafast` default.

    example:
        >>> import ffio
        >>> output_stream_obj = ffio.OutputStreamParser('rtmp://ip:port/path/to/your/stream',
        >>> ... )
        >>> image_np = np.array([[1,2,3],[4,6,9]], dtype=np.uint8)
        >>> output_stream_obj.encode_one_frame(image_np)

    """
    def __init__(self, output_stream_path, input_stream_obj=None,
                 framerate_num=0, framerate_den=0, image_width=0, image_height=0,
                 preset="ultrafast", use_cuda=False,
                 shm_name: str = None, shm_size: int = 0, shm_offset: int = 0):

        # allocate memory to new a streamObj
        self.output_stream_obj = ctypes.c_void_p(lib_interface_api.newOutputStreamObject())

        self.output_stream_path = output_stream_path
        if use_cuda:
            self.use_cuda = 1
        else:
            self.use_cuda = 0

        start_time = time.time()
        if input_stream_obj is not None:
            self.output_stream_video_width         = input_stream_obj.width
            self.output_stream_video_height        = input_stream_obj.height
            self.output_stream_video_framerate_num = input_stream_obj.framerate_num
            self.output_stream_video_framerate_den = input_stream_obj.framerate_den
            self.output_stream_video_average_fps   = \
                self.output_stream_video_framerate_num / self.output_stream_video_framerate_den

        else:
            if image_width == 0:
                raise ValueError('the `image_width` should not be 0')
            self.output_stream_video_width = image_width

            if image_height == 0:
                raise ValueError('the `image_height` should not be 0')
            self.output_stream_video_height = image_height

            if framerate_num == 0:
                raise ValueError('the `framerate_num` should not be 0')
            self.output_stream_video_framerate_num = framerate_num

            if framerate_den == 0:
                raise ValueError('the `framerate_den` should not be 0')
            self.output_stream_video_framerate_den = framerate_den

            self.output_stream_video_average_fps = \
                self.output_stream_video_framerate_num / self.output_stream_video_framerate_den

        self.output_stream_video_preset = preset

        # the output stream must be in byte format
        self.output_stream_obj = ctypes.c_void_p(
            lib_interface_api.initializeOutputStreamObject(
                self.output_stream_obj, self.output_stream_path.encode(),
                self.output_stream_video_framerate_num,
                self.output_stream_video_framerate_den,
                self.output_stream_video_width,
                self.output_stream_video_height,
                self.output_stream_video_preset.encode(),
                self.use_cuda,
                False       if shm_name is None else True,
                "".encode() if shm_name is None else shm_name.encode(),
                shm_size, shm_offset
            )
        )
        end_time = time.time()

        self.output_stream_state_flag = lib_interface_api.getOutputStreamState(
            self.output_stream_obj)

        if self.output_stream_state_flag == 1:  # 1 means successfully opening a stream context
            print("initialization of output stream cost {:.4f} seconds".format(end_time-start_time))
            print("output stream \n\twidth: {}, height: {}, average fps: {}\n"
                  "\tpreset: {},".format(
                self.output_stream_video_width, self.output_stream_video_height,
                self.output_stream_video_average_fps,
                self.output_stream_video_preset))

            self.output_stream_buffer_size = self.output_stream_video_height * self.output_stream_video_width * 3
            # self.image_buffer = ctypes.create_string_buffer(
            #     b"\0", self.stream_buffer_size)
            print("\timage buffer size = {} * {} * 3 = {}".format(
                self.output_stream_video_width, self.output_stream_video_height,
                3, self.output_stream_buffer_size))

            self.output_frame_number = 0

        else:
            print("failed to initialize the output stream. cost {:.4f} seconds".format(
                end_time-start_time))

            self.output_stream_obj = lib_interface_api.finalizeOutputStreamObject(
                self.output_stream_obj)

            self.output_stream_obj = lib_interface_api.deleteOutputStreamObject(
                self.output_stream_obj)

            # all these params should be set to 0 when failed to initialize
            self.output_stream_video_width = 0
            self.output_stream_video_height = 0
            self.output_stream_video_average_fps = 0
            self.output_stream_buffer_size = 0
            self.output_frame_number = 0
            self.output_stream_video_framerate_num = 0
            self.output_stream_video_framerate_den = 0

    @property
    def width(self):
        return self.output_stream_video_width

    @property
    def height(self):
        return self.output_stream_video_height

    @property
    def fps(self):
        return self.output_stream_video_average_fps

    @property
    def preset(self):
        return self.output_stream_video_preset

    @property
    def stream_state(self):
        # 1 means successfully opening a stream context
        # 0 means failed to initialize it
        if self.output_stream_state_flag == 1:
            return True
        else:
            return False

    def encode_one_frame(self, rgb_image):
        """ image_format: numpy, Image, base64, None
        """

        rgb_image_type = type(rgb_image)
        # pdb.set_trace()
        if rgb_image_type is bytes:
            ret = lib_interface_api.encode1Frame(self.output_stream_obj, rgb_image)
            self.output_frame_number += 1

            return ret

        elif rgb_image_type == np.ndarray:
            # numpy method to convert the result buffer
            # print(rgb_image.size, rgb_image.dtype)
            rgb_image_bytes = rgb_image.tobytes()

            ret = lib_interface_api.encode1Frame(self.output_stream_obj, rgb_image_bytes)
            if ret == 0:
                self.output_frame_number += 1

            return ret

        elif rgb_image_type == Image:
            pass

        elif rgb_image_type == base64.base64:
            pass

        else:
            raise ValueError('the type of the given `rgb_image` is invalid.')

        return ret

    def release_memory(self):
        # have to release memory manually cause this piece of code can not be released
        # by Python automatically via ref count, otherwise the memory would explode if
        # initialize and finalize several times.

        # finalize
        self.output_stream_obj = lib_interface_api.finalizeOutputStreamObject(
            self.output_stream_obj)

        # delete the object
        self.output_stream_obj = lib_interface_api.deleteOutputStreamObject(
            self.output_stream_obj)

        self.output_stream_video_width = 0
        self.output_stream_video_height = 0
        self.output_stream_video_average_fps = 0
        self.output_stream_buffer_size = 0
        self.output_frame_number = 0
        self.output_stream_video_framerate_num = 0
        self.output_stream_video_framerate_den = 0
