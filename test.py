# Strongly recommend to use this piece of code in a sub-process.
# Cause decoding an online video stream consumes about 100M memory and 40% CPU.

import os
import pdb

import pyFFmpeg


stream_path = "rtmp://live.demo.uavcmlc.com:1935/live/DEV02001270?token=03c7986c15e0"

stream_state = False  # to control whether to restart the stream context
while True:

    # initialize the stream context
    while True:
        print('init ... ')
        stream_obj = pyFFmpeg.StreamParser(stream_path)
        if stream_obj.stream_state is True:
            # it means that the stream context has been opened successfully.
            # otherwise, the stream can not be reached,
            # probably the path is wrong or stream is empty
            stream_state = stream_obj.stream_state
            break

    # to get frames in a loop until encountering an error
    while True:
        frame = stream_obj.get_one_frame(image_format='numpy')
        if type(frame) is int:
            if frame == -5:
                stream_state = stream_obj.stream_state
                break
            elif frame == -4:
                stream_state = stream_obj.stream_state
                break

        else:
            pass
            # process this frame according to your needs.
            # rgb_image.save(os.path.join(dir_path, 'rgb_file_{}.jpg'.format(i)))

    # then reconnect to the stream and rebuild a stream context in the next loop.
    # you have to release the memory containing stream context manually
    stream_obj.release_memory()
    print('successfully release memory of stream context.')
