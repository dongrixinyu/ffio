# ffio

<p align="left">
<img src="https://img.shields.io/badge/version-1.0.3-green" />
<img src="https://img.shields.io/docker/pulls/jionlp/pyffmpeg?color=brightgreen" />
</p>

<img src="https://github.com/dongrixinyu/ffio/blob/main/image/ffio_logo.jpg?raw=true" style="width:100px;height:70px">

**ffio**, which means using **FF**mpeg to process **io** audio-video streams, is a stable and easy-to-use Python wrapper for FFmpeg-C-API, providing for Python users to handle video streams smoothly.

# Features

For Python users:
- 1. [**Easy to use**](https://github.com/dongrixinyu/ffio/tree/main/example). You do not need to tackle many complex audio-video problems concerning FFmpeg any more.
- 2. [**Stable subprocess management**](). When fork a Python subprocess to process an online video stream, you have no need to worry about the memory leak, error of online stream, abnormal suspension of subprocess, etc.

- 3. **support nvidia GPU**. You can choose if to use nvidia GPU by setting `use_cuda` to `True` or `False`.
- 4. [**support shared memory**](https://github.com/dongrixinyu/ffio/blob/main/example/decode_frames_shm.py). When decoding one frame from a video stream, the decoded RGB frame(usually in numpy format) could be stored in a shared memory that can be accessed by other process. Same to the encoded frame.
- 5. **read and write SEI info**.

# Installation

- **ffio** depends on `FFmpeg` dynamic linking libraries. So it's nessesary to install FFmpeg before ffio.

We provide 3 methods to install ffio.

If you are not familiar with C, and not willing to deal with anything about C:

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
$ docker build -t jionlp/ffio:(your custom version) .
```

If you wanna run ffio in your host OS, rather than a docker container, you can

## 3. Install ffio by yourself

This method is a little bit difficult if you are not familiar with `gcc`, `make`, `cmake` and trivials concerning `ffmpeg` compilation. But if you can configure `ffmpeg, python include path, dynamic library path` smoothly, just take a try.

#### Pre-Installation-requirements

- gcc, make, cmake tools etc.
- ffmpeg>=4.2.0 should been installed correctly.
- Python>=3.8

#### Installation method

- install FFmpeg from source code - [refer to Dockerfile](https://github.com/dongrixinyu/ffio/blob/main/Dockerfile) or documents on other websites.

- install ffio via github + pip
```
$ git clone https://github.com/dongrixinyu/ffio
$ cd ffio
$ ./compiler.sh  # you should configure all kinds of paths according to your OS environment, otherwise you would encounter errors.
$ pip install -e .
```

# Usage

Examples of how to use ffio are given in the hyperlinks:

| function                                                                                      | description                                                                               |
|-----------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------|
| [decode video frames](https://github.com/dongrixinyu/ffio/blob/main/example/decode_frames.py) | To grab frames from an online stream or a video file <br/>suffixed by `mp4` or `flv`, etc. |
| [decode video frames to shm](example/decode_frames_shm.py)                                    | Decoded rgb bytes will be written to SharedMemory.                                        |
| [encode video frames](https://github.com/dongrixinyu/ffio/blob/main/example/encode_frames.py) | To insert frames in Numpy format into a video stream                                      |
| [encode video frames from shm](example/encode_frames_shm.py)                                  | Encode rgb bytes from SharedMemory.                                                       |

# TODO
- read and insert SEI info.
- enable Nvidia cuda for encoding and decoding, pixel format converting.
- functions concerning audio and subtitle.

# Reference

- [FFmpeg Github](https://github.com/FFmpeg/FFmpeg)
- [FFmpeg Principle](https://github.com/lokenetwork/FFmpeg-Principle)
- [ffmepgRtmp](https://github.com/hurtnotbad/ffmepgRtmp)
