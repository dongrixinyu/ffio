# pyFFmpeg

<img src="https://img.shields.io/badge/version-1.0.1-green" />

A simple Python wrapper for FFmpeg.

- This repo is to provide an easy way for Python users to grab frames from a video stream. You do not need to tackle many complex audio-video problems concerning FFmpeg any more.


# Installation

## Pre-Installation-requirements

- gcc, make, cmake tools etc.
- ffmpeg>=4.2.0 should been installed correctly.
- A Python>=3.7 intepreter

## Installation method 1:

This repo is now still in experiment and may not be that stable, so I recommend you to install pyFFmpeg in method 1:

- github + pip
```
$ git clone https://github.com/dongrixinyu/pyFFmpeg
$ cd pyFFmpeg
$ ./compile.sh
$ pip install -e .
```

## Installation method 2:

This method is still in progress and not implemented now.

- dockerfile build
```
$ git clone https://github.com/dongrixinyu/pyFFmpeg
$ docker build .
```

# Usage

- To grab frames from an online stream or a video file suffixed by `mp4` or `flv`, etc.

```
import pyFFmpeg

# to build and initialize a stream context.
stream_path = 'rtmp://ip:port/xxxx'
stream_path = 'xxxx.mp4'
stream_obj = pyFFmpeg.StreamParser(stream_path)

# stream_state indicates if the stream context has been built and initialized successfully.
print(stream_obj.stream_state)

# to get an RGB frame in various format.
frame = stream_obj.get_one_frame(image_format='numpy')
# image_format includes `numpy`, `Image` in PIL, `base64`, or `None` which means just bytes.
# in this example, you get a frame in numpy format with a shape like (width, height, 3).

```

- The [`test.py`](https://github.com/dongrixinyu/pyFFmpeg/blob/main/test.py) file provides a complete version of how to decode an online video stream continually.


# Reference

- [FFmpeg Principle](https://github.com/lokenetwork/FFmpeg-Principle)
