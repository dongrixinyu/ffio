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

lib_interface_api.newInputStreamObject.argtypes = []
lib_interface_api.newInputStreamObject.restype = ctypes.c_void_p

lib_interface_api.deleteInputStreamObject.argtypes = [ctypes.c_void_p]
lib_interface_api.deleteInputStreamObject.restype = ctypes.c_void_p

# the source stream path must be in byte format, not Unicode
lib_interface_api.initializeInputStreamObject.argtypes = [
    ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int,
    ctypes.c_bool, ctypes.c_char_p, ctypes.c_int, ctypes.c_int
]
lib_interface_api.initializeInputStreamObject.restype = ctypes.c_void_p

lib_interface_api.finalizeInputStreamObject.argtypes = [ctypes.c_void_p]
lib_interface_api.finalizeInputStreamObject.restype = ctypes.c_void_p

lib_interface_api.getInputStreamState.argtypes = [ctypes.c_void_p]
lib_interface_api.getInputStreamState.restype = ctypes.c_int

lib_interface_api.getInputVideoStreamWidth.argtypes = [ctypes.c_void_p]
lib_interface_api.getInputVideoStreamWidth.restype = ctypes.c_int

lib_interface_api.getInputVideoStreamHeight.argtypes = [ctypes.c_void_p]
lib_interface_api.getInputVideoStreamHeight.restype = ctypes.c_int

lib_interface_api.getInputVideoStreamFramerateNum.argtypes = [ctypes.c_void_p]
lib_interface_api.getInputVideoStreamFramerateNum.restype = ctypes.c_int

lib_interface_api.getInputVideoStreamFramerateDen.argtypes = [ctypes.c_void_p]
lib_interface_api.getInputVideoStreamFramerateDen.restype = ctypes.c_int

lib_interface_api.getInputVideoStreamAverageFPS.argtypes = [ctypes.c_void_p]
lib_interface_api.getInputVideoStreamAverageFPS.restype = ctypes.c_float

lib_interface_api.decode1Frame.argtypes = [ctypes.c_void_p]
lib_interface_api.decode1Frame.restype = ctypes.py_object
lib_interface_api.getOneFrameToShm.argtypes = [ctypes.c_void_p, ctypes.c_int]
lib_interface_api.getOneFrameToShm.restype  = ctypes.c_bool


class InputStreamParser(object):
    """InputStreamParser: a Python wrapper for FFmpeg to initialize an video stream.

    """
    def __init__(self, input_stream_path, use_cuda=False,
                 shm_name: str = None, shm_size: int = 0, shm_offset: int = 0):

        if use_cuda:
            self.use_cuda = 1
        else:
            self.use_cuda = 0

        # allocate memory to new a streamObj
        self.input_stream_obj = ctypes.c_void_p(lib_interface_api.newInputStreamObject())

        self.input_stream_path = input_stream_path

        start_time = time.time()
        # the input stream must be in byte format
        if shm_name is None:
            self.shm_enabled = False
            self.input_stream_obj = ctypes.c_void_p(
                lib_interface_api.initializeInputStreamObject(
                    self.input_stream_obj, self.input_stream_path.encode(), self.use_cuda,
                    False, "".encode(), 0, 0
                )
            )
        else:
            self.shm_enabled = True
            self.input_stream_obj = ctypes.c_void_p(
                lib_interface_api.initializeInputStreamObject(
                    self.input_stream_obj, self.input_stream_path.encode(), self.use_cuda,
                    True, shm_name.encode(), shm_size, shm_offset
                )
            )
        end_time = time.time()

        self.input_stream_state_flag = lib_interface_api.getInputStreamState(
            self.input_stream_obj)

        if self.input_stream_state_flag == 1:  # 1 means successfully opening a stream context
            print("initialization of input stream cost {:.4f} seconds".format(end_time-start_time))

            self.input_stream_video_width = lib_interface_api.getInputVideoStreamWidth(
                self.input_stream_obj)
            self.input_stream_video_height = lib_interface_api.getInputVideoStreamHeight(
                self.input_stream_obj)
            self.input_stream_video_framerate_num = lib_interface_api.getInputVideoStreamFramerateNum(
                self.input_stream_obj)
            self.input_stream_video_framerate_den = lib_interface_api.getInputVideoStreamFramerateDen(
                self.input_stream_obj)
            self.input_stream_video_average_fps = lib_interface_api.getInputVideoStreamAverageFPS(
                self.input_stream_obj)
            print("input stream width: {}, height: {}, average fps: {}".format(
                self.input_stream_video_width, self.input_stream_video_height,
                self.input_stream_video_average_fps))

            self.input_stream_buffer_size = self.input_stream_video_height * self.input_stream_video_width * 3
            # self.image_buffer = ctypes.create_string_buffer(
            #     b"\0", self.stream_buffer_size)
            print("image buffer size = {} * {} * 3 = {}".format(
                self.input_stream_video_width, self.input_stream_video_height,
                3, self.input_stream_buffer_size))

            self.input_frame_number = 0

        else:
            print("failed to initialize the input stream. cost {:.4f} seconds".format(
                end_time-start_time))

            self.input_stream_obj = lib_interface_api.deleteInputStreamObject(
                self.input_stream_obj)

            # all these params should be set to 0 when failed to initialize
            self.input_stream_video_width = 0
            self.input_stream_video_height = 0
            self.input_stream_video_framerate_num = 0
            self.input_stream_video_framerate_den = 0
            self.input_stream_video_average_fps = 0
            self.input_stream_buffer_size = 0
            self.input_frame_number = 0

            # print(traceback.format_exc())
            # raise ValueError("Can not initialize the stream.")

    @property
    def width(self):
        return self.input_stream_video_width

    @property
    def height(self):
        return self.input_stream_video_height

    @property
    def fps(self):
        return self.input_stream_video_average_fps

    @property
    def framerate_num(self):
        return self.input_stream_video_framerate_num

    @property
    def framerate_den(self):
        return self.input_stream_video_framerate_den

    @property
    def stream_state(self):
        # 1 means successfully opening a stream context
        # 0 means failed to initialize it
        if self.input_stream_state_flag == 1:
            return True
        else:
            return False

    def decode_one_frame(self, image_format='numpy'):
        """ image_format: numpy, Image, base64, None
        """

        frame_bytes = lib_interface_api.decode1Frame(self.input_stream_obj)
        frame_bytes_type = type(frame_bytes)
        if frame_bytes_type is bytes:
            # has already get an image in python bytes format

            self.input_frame_number += 1

            if image_format is None:
                return frame_bytes

            elif image_format == 'numpy':
                # numpy method to convert the result buffer
                np_buffer = np.frombuffer(frame_bytes, dtype=np.uint8)
                np_frame = np.reshape(
                    np_buffer, (self.input_stream_video_height, self.input_stream_video_width, 3))
                return np_frame

            elif image_format == 'Image':
                rgb_image = Image.frombytes(
                    "RGB", (self.input_stream_video_width, self.input_stream_video_height), frame_bytes)
                return rgb_image

            elif image_format == 'base64':
                np_buffer = np.frombuffer(frame_bytes, dtype=np.uint8)
                np_frame = np.reshape(
                    np_buffer, (self.input_stream_video_height, self.input_stream_video_width, 3))
                bgr_image = cv2.cvtColor(np_frame, cv2.COLOR_RGB2BGR)

                np_image = cv2.imencode('.jpg', bgr_image)[1]
                base64_image_code = base64.b64encode(np_image).decode()

                return base64_image_code

        elif frame_bytes_type is int:
            if frame_bytes == -5:
                # it means that the stream is empty,
                # now you should close the stream context object manually
                return -5

            elif frame_bytes == -4:
                # Packet mismatch. Unable to seek to the next packet
                # now you should close the stream context object manually
                return -4

            else:
                # other errors
                return 1

    def get_one_frame_to_shm(self, offset=0) -> bool:
        # get RGB bytes to shm.
        return lib_interface_api.getOneFrameToShm(self.input_stream_obj, offset)

    def release_memory(self):
        # have to release memory manually cause this piece of code can not be released
        # by Python automatically via ref count, otherwise the memory would explode if
        # initialize and finalize several times.

        # uninitialize
        self.input_streamObj = lib_interface_api.finalizeInputStreamObject(
            self.input_stream_obj)

        # delete the object
        self.input_stream_obj = lib_interface_api.deleteInputStreamObject(
            self.input_stream_obj)

        self.input_stream_video_width = 0
        self.input_stream_video_height = 0
        self.input_stream_video_framerate_num = 0
        self.input_stream_video_framerate_den = 0
        self.input_stream_video_average_fps = 0
        self.input_stream_buffer_size = 0
        self.input_frame_number = 0
