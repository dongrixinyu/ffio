# pyffmpeg
A simple Python wrapper for FFmpeg with .so files

- This repo is to provide an easy way for Python user to grab frames from a video stream. You do not need to tackle many complex audio-video problems concerning ffmpeg any more.


# Installation

## Pre-Installation-requirements

- gcc, cmake tools etc.
- ffmpeg should been installed correctly.
- A Python intepreter with version>3.6

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

This method is still in progress.

- dockerfile build
```
$ git clone https://github.com/dongrixinyu/pyFFmpeg
$ docker build .
```

# Usage

- To grab frames from an online stream or a video file suffixed by `mp4` or `flv`, etc.

```
import pyFFmpeg

# to build a stream context.
stream_obj = pyFFmpeg.StreamParser(stream_path)

# stream_state indicats if the stream context has been built successfully.
print(stream_obj.stream_state)

# to get an RGB frame in various format.
frame = stream_obj.get_one_frame(image_format='numpy')
# image_format includes `numpy`, `Image` in PIL, `base64`, or `None` which means just bytes.
# in this example, you get a frame in numpy format with a shape like (width, height, 3).

```

- The `test.py` file provides a complete version of how to decode an online video stream.


# Reference

- [FFmpeg Principle](https://github.com/lokenetwork/FFmpeg-Principle)
