# to build a pyFFmpeg from ubuntu:20.04

FROM ubuntu:20.04

WORKDIR /root

RUN apt update
RUN apt-get update

# install ffmpeg from source code and check its version
# set your time zone according to your location stored in tzdata
ENV TZ=Asia/Shanghai
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt install diffutils make yasm wget -y
RUN apt install pkg-config unzip gcc -y
RUN apt-get install libsdl2-2.0 libsdl2-dev -y
RUN wget https://github.com/FFmpeg/FFmpeg/archive/refs/tags/n4.4.1.zip
RUN unzip n4.4.1.zip

WORKDIR /root/FFmpeg-n4.4.1
RUN ./configure --enable-shared --enable-gpl --enable-optimizations \
    --disable-debug
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
RUN wget https://www.python.org/ftp/python/3.10.13/Python-3.10.13.tar.xz
RUN tar -xf Python-3.10.13.tar.xz
WORKDIR /root/Python-3.10.13
RUN ./configure --enable-optimizations --enable-shared
RUN make -j $(nproc)
# to make it overwrite the default python in system
RUN make install
RUN ln -s /usr/local/bin/python3.10 /usr/local/bin/python

# install pip for python3.10
RUN apt install curl -y
RUN curl -sS https://bootstrap.pypa.io/get-pip.py | python3.10
RUN rm /usr/local/bin/pip
RUN ln -s /usr/local/bin/pip3.10 /usr/local/bin/pip

ENV LD_LIBRARY_PATH=/usr/local/lib:/usr/local/lib64/:/root/Python-3.10.13/:/usr/local/lib/x86_64-linux-gnu

# install cmake
RUN apt install cmake git -y

# install pyFFmpeg
WORKDIR /root
RUN git clone https://github.com/dongrixinyu/pyFFmpeg
WORKDIR /root/pyFFmpeg
