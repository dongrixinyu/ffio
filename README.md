# ffio

<p align="left">
<img src="https://img.shields.io/badge/version-1.0.3-green" />
<img src="https://img.shields.io/docker/pulls/jionlp/pyffmpeg?color=brightgreen" />
</p>

<img src="https://github.com/dongrixinyu/ffio/blob/main/image/ffio_logo.jpg?raw=true" style="width:100px;height:70px" />


**ffio** is not just another wrapper of the FFmpeg executable.  
It directly integrates the FFmpeg C API, mainly designed for streaming, 
specifically deals with raw RGB data.


# Insight

```
[Video]       [FFIO]      [Raw-RGB-Image]    [FFIO]          [Video]
   ◎   ----  (decoding)  ---->   □
                                 ◆
                                 □   ---->  (encoding)  ---->   ◎
```

There are primarily two ways to utilize ffio:
1. **Decoding**: extract images in RGB format from a provided video, either from a local file or a live stream.
2. **Encoding**: encode provided RGB images, write them into a local video file or a live stream. 

Naturally, you can chain two ffio instances together to facilitate video transformation.
However, if further processing with raw RGB images is not required, 
consider simply using `ffmpeg` as a more straightforward solution. 

Just keep in mind, `images in raw format` are the main character of **ffio**.

# Features
- [x] **Easy to use**: You don't have to tackle the complex video processing problems concerning FFmpeg anymore.
Simply follow the [examples](https://github.com/dongrixinyu/ffio/blob/main/example/encode_frames.py).
- [x] **Stability**: Instead of forking a ffmpeg process and exchanging data via pipes, 
you can now interact directly with ffmpeg's C context.
- [x] **Hardware acceleration support**: Simply turn `hw_enabled` to `True` to enable hardware acceleration when creating ffio.
Nvidia CUDA is currently available.
- [x] **Shared memory support**: Interact with image data across multiple processes using shared memory, 
reducing redundant data copying.
- [ ] **Send or recv SEI packets**
- [ ] **Handle image with other formats**
- [ ] **Handles non-video data**. Audio, subtitle.

# Installation

**ffio** depends on `FFmpeg` dynamic linking libraries. So it's necessary to install FFmpeg before ffio.

We provide 3 methods to install ffio.

If you are not familiar with C, and not willing to deal with anything about C:

## 1. pull docker image from docker hub (**recommended**)
```
$ docker pull jionlp/ffio:latest
$ docker run -it jionlp/ffio:latest /bin/bash  # run into the container.
$ (in docker container) python
```

## 2. Quick start
- It takes only three lines of code to run a decoder to extract images from a video.

```python
import ffio
url     = "path_to_file or url_for_live_stream"
decoder = ffio.FFIO(url)
image   = decoder.decode_one_frame()
```

Or, if you wanna build a docker by yourself from a custom GitHub branch.

## 2. build docker image by yourself from GitHub

You can first clone this repo via git, and then build a docker with all libs installed. 
You do not need to configure compilation params anymore.

```
$ git clone https://github.com/dongrixinyu/ffio
$ cd ffio
$ docker build -t jionlp/ffio:(your custom version) .
```

If you wanna run ffio in your host OS, rather than a docker container, you can

## 3. Install ffio by yourself

This method is a little bit difficult if you are not familiar with `gcc`, `make`, `cmake` and trivial concerning `ffmpeg` compilation. 
But if you can configure `ffmpeg, python include path, dynamic library path` smoothly, just take a try.

#### Pre-Installation-requirements

- gcc, make, cmake tools etc.
- ffmpeg>=4.2.0 should have been installed correctly.
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


# Reference

- [FFmpeg Github](https://github.com/FFmpeg/FFmpeg)
- [FFmpeg Principle](https://github.com/lokenetwork/FFmpeg-Principle)
- [ffmepgRtmp](https://github.com/hurtnotbad/ffmepgRtmp)
