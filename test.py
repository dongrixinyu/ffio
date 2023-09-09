import os
import pdb
import time
import ctypes

from PIL import Image
import numpy as np
import jionlp as jio

dir_path = "/home/cuichengyu/test_images"


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

lib_interface_api.getOneFrame.argtypes = [ctypes.c_void_p]
lib_interface_api.getOneFrame.restype = ctypes.py_object

stream_path = b'rtmp://live.uavcmlc.com:1935/live/DEV02001044?token=003caf430efb'

with jio.TimeIt('initialize a stream context'):
    streamObj = ctypes.c_void_p(lib_interface_api.init(stream_path))

video_image_height = lib_interface_api.getHeight(streamObj)
video_image_width = lib_interface_api.getWidth(streamObj)




pdb.set_trace()

for i in range(200):
    # with open(os.path.join(dir_path, 'result_{}.yuv'.format(i)), 'rb') as fr:
    #     content = fr.read()
    result_frame = lib_interface_api.getOneFrame(streamObj)

    # numpy method to convert the result buffer
    # np_buffer = np.frombuffer(frame_bytes, dtype=np.uint8)
    # np.reshape(np_buffer, (video_image_width, video_image_height, 3))
    rgb_image = Image.frombytes("RGB", (video_image_width, video_image_height), result_frame)

    # pdb.set_trace()
    rgb_image.save(os.path.join(dir_path, 'rgb_file_{}.jpg'.format(i)))
