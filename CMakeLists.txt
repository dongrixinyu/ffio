cmake_minimum_required(VERSION 3.0)

# a simple python wrapper for ffmpeg,
# aiming to facilitate computer-vision processing.
project(pyFFmpeg)

add_compile_options(-O3)

set(CMAKE_BUILD_TYPE "Release")

# make sure all the ffmpeg libs are located in the default system lib directories
# otherwise you need to designate ffmpeg lib path via compile.sh
find_library(FFMPEG_AVCODEC_LIBRARIES "avcodec")
find_library(FFMPEG_AVFORMAT_LIBRARIES "avformat")
find_library(FFMPEG_AVDEVICE_LIBRARIES "avdevice")
find_library(FFMPEG_AVFILTER_LIBRARIES "avfilter")
find_library(FFMPEG_AVUTIL_LIBRARIES "avutil")
find_library(FFMPEG_POSTPROC_LIBRARIES "postproc")
find_library(FFMPEG_SWRESAMPLE_LIBRARIES "swresample")
find_library(FFMPEG_SWSCALE_LIBRARIES "swscale")

message("Project source directory: ${PROJECT_SOURCE_DIR}")
message("Python_INCLUDE_DIRS: ${PYTHON_INCLUDE_DIRS}")
message("Python_LIBRARIES: ${PYTHON_LIBRARIES}")
message("FFmpeg_INCLUDE_DIRS: ${FFMPEG_INCLUDE_DIRS}")

message("FFmpeg_SWRESAMPLE_LIBRARIES: ${FFMPEG_SWRESAMPLE_LIBRARIES}")
message("FFmpeg_SWSCALE_LIBRARIES: ${FFMPEG_SWSCALE_LIBRARIES}")
message("FFmpeg_POSTPROC_LIBRARIES: ${FFMPEG_POSRPROC_LIBRARIES}")
message("FFmpeg_AVUTIL_LIBRARIES: ${FFMPEG_AVUTIL_LIBRARIES}")
message("FFmpeg_AVFILTER_LIBRARIES: ${FFMPEG_AVFILTER_LIBRARIES}")
message("FFmpeg_AVDEVICE_LIBRARIES: ${FFMPEG_AVDEVICE_LIBRARIES}")
message("FFmpeg_AVCODEC_LIBRARIES: ${FFMPEG_AVCODEC_LIBRARIES}")
message("FFmpeg_AVFORMAT_LIBRARIES: ${FFMPEG_AVFORMAT_LIBRARIES}")

include_directories(${PYTHON_INCLUDE_DIRS})
include_directories(${FFMPEG_INCLUDE_DIRS})
include_directories(${FFMPEG_LIBRARIES})
include_directories(/usr/include)

add_library(interfaceAPI SHARED
            ${PROJECT_SOURCE_DIR}/pyFFmpeg/interfaceAPI.c
            ${PROJECT_SOURCE_DIR}/pyFFmpeg/extractFrame.c)

target_link_libraries(interfaceAPI
    ${PYTHON_LIBRARIES}
    ${FFMPEG_AVUTIL_LIBRARIES}
    ${FFMPEG_AVFILTER_LIBRARIES}
    ${FFMPEG_AVDEVICE_LIBRARIES}
    ${FFMPEG_AVCODEC_LIBRARIES}
    ${FFMPEG_AVFORMAT_LIBRARIES}
    ${FFMPEG_SWSCALE_LIBRARIES})

# message("Successfully build `*.so` file.")
