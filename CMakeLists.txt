cmake_minimum_required(VERSION 3.0)

# An easy-to-use Python wrapper for FFmpeg-C-API,
# aiming to facilitate computer-vision processing.
project(ffio)

add_compile_options(-O3)
#add_compile_options(-O0 -g -DDEBUG)

set(CMAKE_BUILD_TYPE "Release")
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${FFMPEG_LIB_DIR_PATH})

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

set(FFIO_LIBRARIES
    ${FFMPEG_AVUTIL_LIBRARIES}
    ${FFMPEG_AVFILTER_LIBRARIES}
    ${FFMPEG_AVDEVICE_LIBRARIES}
    ${FFMPEG_AVCODEC_LIBRARIES}
    ${FFMPEG_AVFORMAT_LIBRARIES}
    ${FFMPEG_SWSCALE_LIBRARIES})

message("CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(FFIO_LIBRARIES ${FFIO_LIBRARIES})
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  set(FFIO_LIBRARIES ${FFIO_LIBRARIES} rt)
endif()

set(LIB_SOURCE_FILES ${LIB_SOURCE_FILES} ${PROJECT_SOURCE_DIR}/ffio/ffio_c/ffio.c)
set(LIB_SOURCE_FILES ${LIB_SOURCE_FILES} ${PROJECT_SOURCE_DIR}/ffio/ffio_c/ffio_util.c)
set(LIB_SOURCE_FILES ${LIB_SOURCE_FILES} ${PROJECT_SOURCE_DIR}/ffio/ffio_c/ffioPyApi.c)
add_library(interfaceAPI SHARED ${LIB_SOURCE_FILES})

target_include_directories(interfaceAPI PUBLIC ${PYTHON_INCLUDE_DIRS})
target_include_directories(interfaceAPI PUBLIC ${FFMPEG_INCLUDE_DIRS})
target_include_directories(interfaceAPI PUBLIC /usr/include)
target_link_libraries(interfaceAPI ${PYTHON_LIBRARIES} ${FFIO_LIBRARIES})

# for test.
AUX_SOURCE_DIRECTORY(${PROJECT_SOURCE_DIR}/ffio/ffio_c/test    TEST_SOURCE_FILES)
set(TEST_SOURCE_FILES ${TEST_SOURCE_FILES} ${PROJECT_SOURCE_DIR}/ffio/ffio_c/ffio_util.c)
set(TEST_SOURCE_FILES ${TEST_SOURCE_FILES} ${PROJECT_SOURCE_DIR}/ffio/ffio_c/ffio.c)
add_executable(ffio.test ${TEST_SOURCE_FILES})
target_include_directories(ffio.test PUBLIC ${FFMPEG_INCLUDE_DIRS})
target_include_directories(ffio.test PUBLIC ${PROJECT_SOURCE_DIR}/ffio/ffio_c)
target_link_libraries(ffio.test ${FFIO_LIBRARIES})
