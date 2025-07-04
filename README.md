# ffio

<p align="left">
<img src="https://img.shields.io/badge/version-2.0.4-green" />
<img src="https://img.shields.io/docker/pulls/jionlp/pyffmpeg?color=brightgreen" />
</p>

<img src="https://github.com/dongrixinyu/ffio/blob/main/image/ffio_logo.jpg?raw=true" style="width:100px;height:70px" />


**ffio** is not just another Python wrapper of the FFmpeg executable.
It directly integrates the FFmpeg C API, and is mainly designed for streaming
that specifically deals with raw RGB data.


# Insight

```
[Video]        [FFIO]      [Raw-RGB-Image]     [FFIO]          [Video]
   ● ------> (decoding)  ------>  □
                                  ◘  ----->  (may be AI processing, etc.)
                                  □  ----->  (encoding) ------->  ●
```

There are primarily two ways to utilize ffio:
1. **Decoding**: extract images in RGB format from a provided video, either from a local file or a live stream.
2. **Encoding**: encode provided RGB images, write them into a local video file or a live stream.

Naturally, you can chain two `ffio.FFio` instances together to facilitate video transformation.
However, if you don't have to do some stuff with raw RGB images, simply
consider using ffmpeg as a more straightforward solution.

Just keep in mind, the main character in **ffio** is `images in raw format`.

# Features
- [x] **Easy to use**: You don't have to tackle the complex video processing problems concerning FFmpeg anymore.
Simply follow the [examples](https://github.com/dongrixinyu/ffio/blob/main/example).
- [x] **Stability**: Instead of forking a ffmpeg process and exchanging data via pipes,
you can now interact directly with ffmpeg's C context in Python runtime.
- [x] **Hardware acceleration support**: Simply turn `hw_enabled` to `True` to enable hardware acceleration when creating ffio. `Nvidia CUDA` is currently available.
- [x] **Shared memory support**: Interact with image data across multiple processes using shared memory, reducing redundant data copying.(Currently, tests only passed on Linux.)
- [x] **Send or recv SEI packets**: easy to get access to customized SEI info. See example: [encode_frames with SEI](https://github.com/dongrixinyu/ffio/blob/main/example/encode_frames.py)
for detail.
- [x] **accelarate pix_fmt conversion via cuda**: pix_fmt conversion(namely yuv->rgb and rgb->yuv) is written in cuda.[statistics](https://github.com/dongrixinyu/ffio/wiki/CPU-GPU-utilization-of-ffio#GPU-usage-statistics)
- [x] **handle gdr video via `flags='showall'`**: you can set `flags='showall'` in decoder to get rgb frame from a gdr(as opposed to idr) video
- [ ] **Handle non-video data**. Audio, or subtitle.


# Installation

**ffio** depends on `FFmpeg` dynamic linking libraries. So it's necessary to install FFmpeg before ffio.

We provide 3 methods to install ffio.

If you are not familiar with C, and not willing to deal with anything about C:

## 1. Pull docker image from docker hub (**recommended**)
```
$ docker pull jionlp/ffio:latest
$ docker run -it jionlp/ffio:latest /bin/bash  # run into the container.
$ (in docker container) python
```

#### Quick start
- It takes only two lines of code to run a decoder to extract images from a video.

```python
import ffio
decoder = ffio.FFIO("/path/to/target")
image   = decoder.decode_one_frame()
```

Or, if you wanna build a docker by yourself from a custom GitHub branch:

## 2. Build docker image by yourself from GitHub

You can first clone this repo via git, and then build a docker with all libs installed.
You do not need to configure compilation params anymore.

```
$ git clone https://github.com/dongrixinyu/ffio
$ cd ffio
$ docker build -t jionlp/ffio:(your custom version) .
```

If you wanna run ffio in your host OS, rather than a docker container, you can:

## 3. Install ffio by yourself

This method is a little bit difficult if you are not familiar with `gcc`, `make`, `cmake` and trivial concerning `ffmpeg` compilation.
But if you can configure `ffmpeg, python include path, dynamic library path` smoothly, just take a try.

#### Pre-Installation-requirements

- gcc, make, cmake tools etc.
- ffmpeg>=4.2.0 should have been installed correctly.
- Python>=3.8
- nvcc (if intend to use `hw_enabled` and `pix_fmt_hw_enabled`)

#### Installation method

- install FFmpeg from source code - [refer to Dockerfile](https://github.com/dongrixinyu/ffio/blob/main/Dockerfile) or documents on other websites.

- install ffio via github + pip
```
$ git clone https://github.com/dongrixinyu/ffio
$ cd ffio
$ ./compiler.sh [debug|release] # you should configure all kinds of paths according to your OS environment, otherwise you would encounter errors.
$ pip install -e .
```

# Usage

Here is [Documentation](https://github.com/dongrixinyu/ffio/wiki/Documentation). Examples of how to use ffio are given in the hyperlinks:

| function       | description    |
|----------------| ---------------|
| [decode video frames](https://github.com/dongrixinyu/ffio/blob/main/example/decode_frames.py) | To grab frames from an online stream or a video file <br/>suffixed by `mp4` or `flv`, etc. |
| [decode video frames to shm](example/decode_frames_shm.py) | Decoded rgb bytes will be written to SharedMemory.  |
| [encode video frames](https://github.com/dongrixinyu/ffio/blob/main/example/encode_frames.py) | To insert frames in Numpy format into a video stream  |
| [encode video frames from shm](example/encode_frames_shm.py) | Encode rgb bytes from SharedMemory. |
|`ffio.cuda_is_available()` | returns a bool value, indicating that if cuda is available, and print info of GPU. |
|`ffio.available_gpu_memory()` | returns a list, indicating how much GPU memory is available to use for every GPU card, excluding that been occupied. It is measured in `M`(mega) unit. It is helpful for deciding whether to set `hw_enabled` and `pix_fmt_hw_enabled` to `True`.|

# Reference

- [FFmpeg Github](https://github.com/FFmpeg/FFmpeg)
- [FFmpeg Principle](https://github.com/lokenetwork/FFmpeg-Principle)
- [ffmepgRtmp](https://github.com/hurtnotbad/ffmepgRtmp)
