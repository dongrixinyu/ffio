cmake_minimum_required(VERSION 3.11)
# 3.9 makes cuda compilation available.

# An easy-to-use Python wrapper for FFmpeg-C-API,
# aiming to facilitate computer-vision processing.
project(ffio)

# set to -DCHECK_CUDA=0 means not add definition
#        -DCHECK_CUDA=1 means add definition
if(${CHECK_CUDA})
    enable_language(CUDA)
    find_package(CUDA REQUIRED)
endif()

if(${DEBUG})
    add_compile_options(-O0 -g -DDEBUG)
    message("-- Project in [[ DEBUG ]] mode.")
else()
    add_compile_options(-O3)
    message("-- Project in [[ RELEASE ]] mode.")
endif()

if(${CHECK_CUDA})
    add_definitions(-DCHECK_IF_CUDA_IS_AVAILABLE)
    message("add global definition `CHECK_IF_CUDA_IS_AVAILABLE`")
endif()


set(CMAKE_BUILD_TYPE "Release")
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${FFMPEG_LIB_DIR_PATH})
if(${CHECK_CUDA})
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${CUDA_LIB_DIR_PATH})
endif()

# make sure all the ffmpeg libs are located in the default system lib directories
# otherwise you need to designate ffmpeg lib path via compile.sh
find_library(FFMPEG_AVCODEC_LIBRARIES     "avcodec")
find_library(FFMPEG_AVFORMAT_LIBRARIES    "avformat")
find_library(FFMPEG_AVDEVICE_LIBRARIES    "avdevice")
find_library(FFMPEG_AVFILTER_LIBRARIES    "avfilter")
find_library(FFMPEG_AVUTIL_LIBRARIES      "avutil")
find_library(FFMPEG_POSTPROC_LIBRARIES    "postproc")
find_library(FFMPEG_SWRESAMPLE_LIBRARIES  "swresample")
find_library(FFMPEG_SWSCALE_LIBRARIES     "swscale")
if(${CHECK_CUDA})
    find_library(CUDA_CUDART_LIBRARIES "cudart")
endif()

message("Project source directory:    ${PROJECT_SOURCE_DIR}")
message("Python_INCLUDE_DIRS:         ${PYTHON_INCLUDE_DIRS}")
message("Python_LIBRARIES:            ${PYTHON_LIBRARIES}")
message("FFmpeg_INCLUDE_DIRS:         ${FFMPEG_INCLUDE_DIRS}")

message("  FFmpeg_SWRESAMPLE_LIBRARIES: ${FFMPEG_SWRESAMPLE_LIBRARIES}")
message("  FFmpeg_SWSCALE_LIBRARIES:    ${FFMPEG_SWSCALE_LIBRARIES}")
message("  FFmpeg_POSTPROC_LIBRARIES:   ${FFMPEG_POSRPROC_LIBRARIES}")
message("  FFmpeg_AVUTIL_LIBRARIES:     ${FFMPEG_AVUTIL_LIBRARIES}")
message("  FFmpeg_AVFILTER_LIBRARIES:   ${FFMPEG_AVFILTER_LIBRARIES}")
message("  FFmpeg_AVDEVICE_LIBRARIES:   ${FFMPEG_AVDEVICE_LIBRARIES}")
message("  FFmpeg_AVCODEC_LIBRARIES:    ${FFMPEG_AVCODEC_LIBRARIES}")
message("  FFmpeg_AVFORMAT_LIBRARIES:   ${FFMPEG_AVFORMAT_LIBRARIES}")
if(${CHECK_CUDA})
    message("  cuda_INCLUDE_DIRS:           ${CUDA_INCLUDE_DIRS}")
    message("  cuda_cudart_LIBRARIES:       ${CUDA_CUDART_LIBRARIES}")
endif()

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

set(CUDA_LIBRARIES ${CUDA_CUDART_LIBRARIES})

set(FFIO_SOURCE_FILES ${FFIO_SOURCE_FILES} ${PROJECT_SOURCE_DIR}/ffio/ffio_c/ffio.c)
# if(${CHECK_CUDA})
#     set(FFIO_SOURCE_FILES ${FFIO_SOURCE_FILES} ${PROJECT_SOURCE_DIR}/ffio/ffio_c/ffio_cuda_check.c)
# endif()
set(FFIO_SOURCE_FILES ${FFIO_SOURCE_FILES} ${PROJECT_SOURCE_DIR}/ffio/ffio_c/ffio_util.c)


if(${CHECK_CUDA})


    cuda_add_library(cudaAPI SHARED
        ${PROJECT_SOURCE_DIR}/ffio/ffio_c/ffio_cuda_check.c)
    message("cuda_INCLUDE_DIRS:           ${CUDA_INCLUDE_DIRS}")
    message("cuda_cudart_LIBRARIES:       ${CUDA_LIBRARIES}")
    target_include_directories(cudaAPI PUBLIC
        ${CUDA_INCLUDE_DIRS})
    target_link_libraries(cudaAPI
        ${CUDA_LIBRARIES})


    set(FFIO_SOURCE_FILES ${FFIO_SOURCE_FILES} ${PROJECT_SOURCE_DIR}/ffio/ffio_c/pix_fmt_conversion.cu)
    if(${DEBUG})
        set(CUDA_NVCC_FLAGS "-O0 -g -G")
    else()
        set(CUDA_NVCC_FLAGS "-O3")
    endif()
    cuda_add_library(interfaceAPI SHARED
        ${PROJECT_SOURCE_DIR}/ffio/ffio_c/ffioPyApi.c
        ${FFIO_SOURCE_FILES})
    target_include_directories(interfaceAPI PUBLIC
        ${PROJECT_SOURCE_DIR}/ffio/ffio_c/
        ${FFMPEG_INCLUDE_DIRS}
        ${PYTHON_INCLUDE_DIRS})
    target_link_libraries(interfaceAPI
        ${FFIO_LIBRARIES}
        ${PYTHON_LIBRARIES}
        )

    set(FFIO_DEBUG_FILES ${FFIO_SOURCE_FILES} ${PROJECT_SOURCE_DIR}/ffio/ffio_c/test/main.c)
    if(${DEBUG})
        set(CUDA_NVCC_FLAGS "-O0 -g -G")
    else()
        set(CUDA_NVCC_FLAGS "-O3")
    endif()
    cuda_add_executable(main
        ${FFIO_DEBUG_FILES})

    set_target_properties(main
        PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/ffio/ffio_c/test)
    # set_target_properties(main
    #     PROPERTIES CUDA_GENERATION_DEBUG ON)
    target_include_directories(main PUBLIC
        ${PROJECT_SOURCE_DIR}/ffio/ffio_c/
        ${FFMPEG_INCLUDE_DIRS}
        ${CUDA_INCLUDE_DIRS})
    target_link_libraries(main
        ${FFIO_LIBRARIES}
        ${CUDA_LIBRARIES})

else()
    add_library(interfaceAPI SHARED
        ${PROJECT_SOURCE_DIR}/ffio/ffio_c/ffioPyApi.c
        ${FFIO_SOURCE_FILES})
    target_include_directories(interfaceAPI PUBLIC
        ${FFMPEG_INCLUDE_DIRS}
        ${PYTHON_INCLUDE_DIRS})
    target_link_libraries(interfaceAPI
        ${FFIO_LIBRARIES}
        ${PYTHON_LIBRARIES})
endif()


# for test.
AUX_SOURCE_DIRECTORY(${PROJECT_SOURCE_DIR}/ffio/ffio_c/test    TEST_SOURCE_FILES)
if(${CHECK_CUDA})
    cuda_add_executable(ffio.test
        ${FFIO_SOURCE_FILES}
        ${TEST_SOURCE_FILES})
else()
    add_executable(ffio.test
        ${FFIO_SOURCE_FILES}
        ${TEST_SOURCE_FILES})
endif()

target_include_directories(ffio.test PUBLIC
    ${FFMPEG_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/ffio/ffio_c)
target_link_libraries(ffio.test
    ${FFIO_LIBRARIES})
