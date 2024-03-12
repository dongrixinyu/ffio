# -*- coding=utf-8 -*-
# Library: ffio
# Author: dongrixinyu
# License: MIT
# Email: dongrixinyu.66@gmail.com
# Github: https://github.com/dongrixinyu/ffio
# Description: An easy-to-use Python wrapper for FFmpeg-C-API.
# Website: http://www.jionlp.com

# Strongly recommend to use this piece of code in a sub-process.
# Cause decoding an online video stream consumes about 100M memory and 40% CPU.

import sys
import time
import cv2
import numpy as np
import ffio

if len(sys.argv) < 3:
  print("Usage: ")
  print("  python3 example/encode_frames.py [src_path] [dst_path]")
  sys.exit(1)
i_path = sys.argv[1]
o_path = sys.argv[2]


def main():
  params = ffio.CCodecParams()
  params.width    = 1920
  params.height   = 1080
  params.bitrate  = 2400*1024
  params.fps      = 24
  params.gop      = 48
  params.b_frames = 0
  params.profile  = b"baseline"
  params.preset   = b"fast"
  encoder = ffio.FFIO( o_path, mode=ffio.FFIOMode.ENCODE, hw_enabled=True, codec_params=params)
  decoder = ffio.FFIO( i_path, mode=ffio.FFIOMode.DECODE, hw_enabled=True)

  if decoder and encoder:
    time_total = 0
    idx        = 0
    while idx < 100:
      time_before = time.time()
      if frame := decoder.decode_one_frame(sei_filter="ffio"):
        if frame.sei_msg:
          print(f"sei: {frame.sei_msg.decode()}")
        frame = _draw(frame.as_numpy(), idx)
        if encoder.encode_one_frame(frame, sei_msg=f"[{idx}] ffio sei msg."):
          dt          = time.time() - time_before
          time_total += dt
          idx        += 1
          avg         = time_total * 1000 / idx
          fps         = 1000 / avg
          print(f"{idx}: dt:{dt * 1000:.2f}ms, avg:{avg:.2f}ms, {fps:.2f}fps, "
                f"total: {time_total:.2f}s, shape:{frame.shape}")
      else:
        print(f"decode error: {frame.err}.")
        break

    # Attention !!
    # Force quitting this script will result in a memory leak.
    # Ensure the following process is executed.
  decoder.release_memory()
  encoder.release_memory()


def _draw(frame: np.ndarray, seq: int):
  diff    = seq % 100
  coord_x = 20 + diff*4
  coord_y = 10 + diff*2
  cv2.rectangle(frame, (coord_x, coord_y), (coord_x+200, coord_y+150), (0, 255, 0), 2)
  return frame


main()
