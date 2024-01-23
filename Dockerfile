# to build a pyFFmpeg from ubuntu:20.04

FROM ubuntu:20.04

# cuda version should be modified in accordance with the actual GPU card
ENV NVIDIA_BASE_URL=https://us.download.nvidia.com/tesla
ENV DRIVER_VERSION=470.223.02
ENV CUDA_VERSION=12.1.0
ENV FFMPEG_VERSION=6.1.1
ENV PYTHON_VERSION=3.10.13
ENV NV_CODEC_HEADERS_VERSION=n11.1.5.2

WORKDIR /root

RUN apt update
RUN apt-get update


# set your time zone according to your location stored in tzdata
ENV TZ=Asia/Shanghai
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt install diffutils make yasm wget -y
RUN apt install pkg-config unzip gcc -y
RUN apt-get install libsdl2-2.0 libsdl2-dev -y
# install cmake
RUN apt install cmake git -y


# install nvidia-cuda

# from package manager
RUN wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2004/x86_64/cuda-keyring_1.1-1_all.deb
RUN dpkg -i cuda-keyring_1.1-1_all.deb
RUN apt-get update
# the command must be run seperatedly
RUN apt-get install cuda-toolkit -y
RUN apt-get install nvidia-gds -y
ENV PATH=/usr/local/cuda-12/bin${PATH:+:${PATH}}

# install ffmpeg from source code and check its version

# dependency step: install nv-codec-headers
RUN git clone -b $NV_CODEC_HEADERS_VERSION https://gitee.com/ak17/nv-codec-headers.git nv-codec-headers
WORKDIR /root/nv-codec-headers
RUN make -j$(nproc) && make install

RUN apt-get install libx264-dev -y

WORKDIR /root
# download ffmpeg source code from https://ffmpeg.org/download.html#releases
RUN wget https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz --no-check-certificate
RUN tar -xJf ffmpeg-${FFMPEG_VERSION}.tar.xz
# RUN wget https://github.com/FFmpeg/FFmpeg/archive/refs/tags/n4.4.1.zip
# RUN unzip n4.4.1.zip

WORKDIR /root/ffmpeg-${FFMPEG_VERSION}
# RUN ./configure --enable-shared --enable-gpl --enable-optimizations \
#     --disable-debug --enable-libx264
RUN ./configure \
    --enable-gpl \
    --enable-shared \
    --enable-optimizations \
    --enable-libx264 \
    --enable-nonfree \
    --enable-cuda-nvcc \
    --enable-libnpp \
    --disable-debug \
    --disable-asm \
    --disable-stripping \
    --extra-cflags=-I/usr/local/cuda/include --extra-ldflags=-L/usr/local/cuda/lib64
RUN make -j $(nproc)
# install in the default directory /usr/local
RUN make install

# install ffmpeg from apt command and its corresponding dev libs.
# RUN apt install libavutil-dev libavcodec-dev libavformat-dev libavdevice-dev \
#     libavfilter-dev libpostproc-dev libswresample-dev libswscale-dev -y

# ./include/x86_64-linux-gnu/libavcodec/avcodec.h
# root@1e61361abac7:/usr# find . -name libavcodec.so
# ./lib/x86_64-linux-gnu/libavcodec.so

# install python 3.10
WORKDIR /root/
RUN apt install build-essential zlib1g-dev libncurses5-dev libgdbm-dev \
    libnss3-dev libssl-dev libreadline-dev libffi-dev libsqlite3-dev libbz2-dev -y
RUN wget https://www.python.org/ftp/python/${PYTHON_VERSION}/Python-${PYTHON_VERSION}.tar.xz
RUN tar -xf Python-${PYTHON_VERSION}.tar.xz
WORKDIR /root/Python-${PYTHON_VERSION}
RUN ./configure --enable-optimizations --enable-shared
RUN make -j $(nproc)
# to make it overwrite the default python in system
RUN make install
RUN ln -s /usr/local/bin/python3.10 /usr/local/bin/python

# install pip for python3.10
RUN apt install curl -y
ENV LD_LIBRARY_PATH=/usr/local/lib:/root/Python-$PYTHON_VERSION
RUN curl -sS https://bootstrap.pypa.io/get-pip.py | python3.10
RUN rm /usr/local/bin/pip
RUN ln -s /usr/local/bin/pip3.10 /usr/local/bin/pip

ENV LD_LIBRARY_PATH=/usr/local/lib:/usr/local/lib64/:/root/Python-${PYTHON_VERSION}/:/usr/local/lib/x86_64-linux-gnu

# koisi-io config
# ENV http_proxy=http://192.168.126.62:6152
# ENV https_proxy=$http_proxy

# install ffio
WORKDIR /root
RUN git clone https://github.com/dongrixinyu/ffio
WORKDIR /root/ffio
RUN ./compiler.sh
RUN pip install -e .

WORKDIR /workspace
