# -*- coding=utf-8 -*-
# Library: ffio
# Author: dongrixinyu
# License: MIT
# Email: dongrixinyu.66@gmail.com
# Github: https://github.com/dongrixinyu/ffio
# Description: An easy-to-use Python wrapper for FFmpeg-C-API.
# Website: http://www.jionlp.com

#  Usage:
#     python3 decode_frames.py test.flv

# Strongly recommend to use this piece of code in a sub-process.
# Cause decoding an online video stream consumes about 100M memory and 40% CPU.

import sys
import ffio
from PIL import Image


if len(sys.argv) < 2:
  print("Please provide a target to read.")
  sys.exit(1)

video_path = sys.argv[1]
read_num   = 10
read_idx   = 0
while read_idx < read_num:

  """
    Infinite trying to open ffio, until ffio_state returns success.
  """
  while True:
    decoder = ffio.FFIO( video_path, mode=ffio.FFIOMode.DECODE, hw_enabled=False)
    if decoder.ffio_state is True:
      break

  """
    Loop to decode frames from video.
  """
  while read_idx < read_num:
    rgb_bytes = decoder.decode_one_frame(image_format='numpy')
    if type(rgb_bytes) is int:
      """
        When `decode_one_frame()` failed to get frame, 
        you should release current FFIO instance by calling `release_memory()`.
        Than retry to create new instance as you need.
      """
      break

    else:
      read_idx += 1
      print(f"success get frame: [{read_idx}].")
      image = Image.fromarray(rgb_bytes, 'RGB')
      image.save(f"output-{read_idx}.jpeg", 'JPEG')

  decoder.release_memory()
