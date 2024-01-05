# ffio

<p align="left">
<img src="https://img.shields.io/badge/version-1.0.1-green" />
<img src="https://img.shields.io/docker/pulls/jionlp/pyffmpeg?color=brightgreen" />
</p>

<img src="https://github.com/dongrixinyu/ffio/blob/main/ffio_logo.jpg?raw=true" />

An easy-to-use Python wrapper for FFmpeg-C-API.

- This repo **ffio**(which means FFmpeg as an io) is to provide an easy way for Python users to use handle video streams. Now this repo includes functions below:
    - grab frames from a video stream.
    - insert frames in Python format into an output video stream.

# Features

For Python users:
- 1. You do not need to tackle many complex audio-video problems concerning FFmpeg any more.
- 2. When fork a Python subprocess to process an online video stream, you have no need to worry about the memory leak, error of online stream, abnormal suspension of subprocess, etc.

# Installation

We provide 3 methods to install ffio.

If you are not familiar with C, not willing to deal with anything about C:

## 1. pull docker image from docker hub (**recommended**)
```
$ docker pull jionlp/ffio:latest
$ docker run -it jionlp/ffio:latest /bin/bash  # run into the container.
$ (in docker container) python
```

and then you can type these scripts in the `Python Console`:
```python
>>> import ffio
>>> input_stream_obj = ffio.InputStreamParser('rtmp://ip:port/xxxx')
>>> print(input_stream_obj.width, input_stream_obj.height, input_stream_obj.fps)
```

Or, if you wanna build a docker by yourself from a custom Github branch.

## 2. build docker image by yourself from Github

You can first clone this repo via git, and then build a docker with all libs installed. You do not need to configure compilation params any more.

```
$ git clone https://github.com/dongrixinyu/ffio
$ cd ffio
$ docker build -t jionlp/ffio:vv .
```

If you wanna run ffio in your host OS, rather than a docker, you can

## 3. Install ffio by yourself

This method is a little bit difficult if you are not familiar with `gcc`, `make`, `cmake` and trivials concerning compilation. But if you can configure `ffmpeg, python include path, dynamic library path` smoothly, just take a try.

#### Pre-Installation-requirements

- gcc, make, cmake tools etc.
- ffmpeg>=4.2.0 should been installed correctly.
- Python>=3.7

#### Installation method

This repo is now still in experiment and may not be that stable, so I recommend you to install ffio in method 1:

- github + pip
```
$ git clone https://github.com/dongrixinyu/ffio
$ cd ffio
$ ./compiler.sh  # you should configure all kinds of paths according to your OS environment, otherwise you would encounter errors.
$ pip install -e .
```


# Usage

Examples of how to use ffio are given in the hyperlinks:

| function | description |
| [decode video frames](https://github.com/dongrixinyu/ffio/blob/main/example/decode_frames.py) | To grab frames from an online stream or a video file suffixed by `mp4` or `flv`, etc. |
| [encode video frames](https://github.com/dongrixinyu/ffio/blob/main/example/encode_frames.py) | To insert frames in Numpy format into a video stream |

# TODO
- read and insert SEI info.
- enable Nvidia cuda for encoding and decoding.
- audio functions.

# Reference

- [FFmpeg Github](https://github.com/FFmpeg/FFmpeg)
- [FFmpeg Principle](https://github.com/lokenetwork/FFmpeg-Principle)
- [ffmepgRtmp](https://github.com/hurtnotbad/ffmepgRtmp)
