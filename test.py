import os
import pdb
import time
import ctypes

import pyFFmpeg

dir_path = "/home/cuichengyu/test_images"

stream_path = "rtmp://live.uavcmlc.com:1935/live/DEV02001044?token=003caf430efb"

stream_obj = pyFFmpeg.StreamParser(stream_path)

for i in range(500):
    frame = stream_obj.get_one_frame(image_format='numpy')

    pdb.set_trace()
# rgb_image.save(os.path.join(dir_path, 'rgb_file_{}.jpg'.format(i)))
