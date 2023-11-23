# pyFFmpeg

<p align="left">
<img src="https://img.shields.io/badge/version-1.0.1-green" />
<img src="https://img.shields.io/docker/pulls/jionlp/pyffmpeg?color=brightgreen" />
</p>


A simple Python wrapper for FFmpeg.

- This repo is to provide an easy way for Python users to grab frames from a video stream. You do not need to tackle many complex audio-video problems concerning FFmpeg any more.


# Installation

We provide 2 methods to install pyFFmpeg.

## 1. Docker

You can first pull this repo via git, and then build a docker with all libs installed. You do not need to configure compilation params any more.

```
$ git clone https://github.com/dongrixinyu/pyFFmpeg
$ cd pyFFmpeg
$ docker build -t jionlp/pyffmpeg:1.0 .
$ docker run -it jionlp/pyffmpeg:1.0 /bin/bash  # run into the container.
$ (in docker container) python
```

and then you can type these scripts in the `Python Console`:
```python
>>> import pyFFmpeg
>>> stream_obj = pyFFmpeg.StreamParser('rtmp://ip:port/xxxx')
>>> print(stream_obj.width, stream_obj.height, stream_obj.fps)
```

**Or**, **you can pull image from docker hub**.(recommended)
```
docker pull jionlp/pyffmpeg:1.0
```

## 2. Install pyFFmpeg by yourself

This method is a little bit difficult if you are not familiar with GCC and trivials concerning compilation. But if you can configure `ffmpeg, python include path, dynamic library path` smoothly, just take a try.

#### Pre-Installation-requirements

- gcc, make, cmake tools etc.
- ffmpeg>=4.2.0 should been installed correctly.
- Python>=3.7

#### Installation method

This repo is now still in experiment and may not be that stable, so I recommend you to install pyFFmpeg in method 1:

- github + pip
```
$ git clone https://github.com/dongrixinyu/pyFFmpeg
$ cd pyFFmpeg
$ ./compiler.sh  # you should configure all kinds of paths according to your system environment.
$ pip install -e .
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
