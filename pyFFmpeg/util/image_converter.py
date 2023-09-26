# -*- coding=utf-8 -*-
# Library: pyFFmpeg
# Author: dongrixinyu
# License: MIT
# Email: dongrixinyu.66@gmail.com
# Github: https://github.com/dongrixinyu/pyFFmpeg
# Description: a simple Python wrapper for FFmpeg.'
# Website: http://www.jionlp.com


import base64
import numpy as np
from PIL import Image


def rawrgb_2_numpy(rgb_bytes, width, height):
    # convert rgb frame bytes to numpy format
    # rgb_bytes does not contain width and height info

    # numpy method to convert the result buffer
    np_buffer = np.frombuffer(frame_bytes, dtype=np.uint8)
    np_frame = np.reshape(np_buffer, (width, height, 3))

    return np_frame


def rawrgb_2_Image(rgb_bytes, width, height):
    # convert rgb frame bytes to PIL.Image format
    # rgb_bytes does not contain width and height info
    rgb_image = Image.frombytes("RGB", (width, height), frame_bytes)

    return rgb_image


def rawrgb_2_jpg(rgb_bytes, width, height, file_path):
    # convert rgb frame bytes to a jgp format file saved in `file_path`
    # rgb_bytes does not contain width and height info
    rgb_image = Image.frombytes("RGB", (width, height), frame_bytes)

    rgb_image.save(file_path)


def rawrgb_2_base64(rgb_bytes, width, height):
    # convert rgb frame bytes to base64 format
    # rgb_bytes does not contain width and height info
    np_buffer = np.frombuffer(frame_bytes, dtype=np.uint8)
    np_frame = np.reshape(np_buffer, (width, height, 3))

    np_image = cv2.imencode('.jpg', np_frame)[1]
    base64_image_code = str(base64.b64encode(np_image))

    return base64_image_code
