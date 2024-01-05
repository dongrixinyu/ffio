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

import os
import pdb
import time

import cv2
import ffio


print_gap = 200  # print log every `print_gap` frames

# replace these rtmp stream path with your custom one.
input_stream_path = "rtmp://ip:port/path/to/your/input/stream"
output_stream_path = "rtmp://ip:port/path/to/your/output/stream"

input_stream_path = "rtmp://live.demo.uavcmlc.com:1935/live/DEV02003179?token=8622d43632a1"
output_stream_path = "rtmp://live.demo.uavcmlc.com:1935/live/DEV02005245?token=efa390262de0"

input_stream_state = False  # to control whether to restart the input stream context
output_stream_state = False  # control whether to restart the output stream context

while True:

    # initialize the input stream context
    while True:
        print('init input stream context ... ')
        input_stream_obj = ffio.InputStreamParser(input_stream_path)
        if input_stream_obj.stream_state is True:
            # it means that the input stream context has been opened successfully.
            # otherwise, the input stream can not be reached,
            # probably the path is wrong or input stream is empty
            input_stream_state = input_stream_obj.stream_state
            break

    # initialize the output stream context
    while True:
        print('init output stream context ... ')
        output_stream_obj = ffio.OutputStreamParser(
            output_stream_path, input_stream_obj=input_stream_obj,
            preset="ultrafast")
        if output_stream_obj.stream_state is True:
            # it means that the output stream context has been opened successfully.
            # otherwise, the output stream can not be reached,
            # probably the path is wrong or output stream is empty
            output_stream_state = output_stream_obj.stream_state

            break


    # to get frames in a loop until encountering an error
    while True:
        frame = input_stream_obj.decode_one_frame(image_format='numpy')
        if type(frame) is int:
            if frame == -5:
                input_stream_state = input_stream_obj.stream_state
                break
            elif frame == -4:
                input_stream_state = input_stream_obj.stream_state
                break

        else:
            base_coor = output_stream_obj.output_frame_number
            while base_coor + 100 > output_stream_obj.height:
                base_coor -= output_stream_obj.height
            frame = cv2.rectangle(
                frame, (base_coor, base_coor), (base_coor + 99, base_coor + 99),
                (255, 0, 0), 3)

            # encode images to the output stream
            ret = output_stream_obj.encode_one_frame(frame)
            if ret == 0:
                if output_stream_obj.output_frame_number % print_gap == 0:
                    print('Successfully encode one frame {}.'.format(
                        output_stream_obj.output_frame_number))
                # pass
            else:
                break

    # then reconnect to the input and output stream and rebuild input and output
    # stream context in the next loop. You have to release the memory containing
    # input and output stream context manually.
    output_stream_obj.release_memory()
    input_stream_obj.release_memory()
    print('successfully release memory of input and output stream context.')
