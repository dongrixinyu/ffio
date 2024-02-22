//
// Created by koisi on 2024/2/22.
//
#include <stdlib.h>

#include "ffio.h"

static void resetFFIO(FFIO* ffio){
  if(ffio==NULL){ return; }
  ffio->ffioState = FFIO_STATE_NOT_READY;
  ffio->ffioMode  = FFIO_MODE_DECODE;
  ffio->frameSeq  = -1;

  ffio->avFormatContext = NULL;
  ffio->avCodecContext  = NULL;
  ffio->avCodec         = NULL;
  ffio->avPacket        = NULL;
  ffio->yuvFrame        = NULL;
  ffio->rgbFrame        = NULL;
  ffio->swsContext      = NULL;
  ffio->hwContext       = NULL;

  ffio->videoStreamIndex = -1;
  ffio->imageWidth       = 0;
  ffio->imageHeight      = 0;
  ffio->imageByteSize    = 0;

  ffio->rawFrame    = NULL;
  ffio->rawFrameShm = NULL;
  ffio->shmEnabled  = false;
  ffio->shmFd       = -1;
  ffio->shmSize     = 0;

  ffio->hw_enabled  = false;
}

static int ffio_init_avformat(FFIO* ffio){
  int ret;

  if(ffio->ffioMode == FFIO_MODE_DECODE){
    ret = avformat_open_input(&ffio->avFormatContext, ffio->targetUrl,NULL, NULL);
    if(ret < 0){
      av_log(NULL, AV_LOG_WARNING, "can not open stream: `%s`\n", ffio->targetUrl);
      return 1;
    }

    ret = avformat_find_stream_info(ffio->avFormatContext, NULL);
    if(ret < 0){
      av_log(NULL, AV_LOG_WARNING, "can not find stream info\n");
      return 2;
    }
  }

  av_log(NULL, AV_LOG_WARNING, "succeed to init avformat.\n");
  return 0;
}

static int ffio_init_avcodec(FFIO* ffio, const char *hw_device){
  int ret;

  if(ffio->ffioMode == FFIO_MODE_DECODE){
    ffio->videoStreamIndex = av_find_best_stream(
        ffio->avFormatContext, AVMEDIA_TYPE_VIDEO,
        -1, -1,(const AVCodec **)&(ffio->avCodec), 0);
    if(ffio->videoStreamIndex < 0){
      av_log(NULL, AV_LOG_ERROR,"Failed to set stream index.\n");
      return 2;
    }

    ffio->avCodecContext = avcodec_alloc_context3(ffio->avCodec);
    if(!ffio->avCodecContext){
      av_log(NULL, AV_LOG_ERROR,"Failed to init codec.\n");
      return 4;
    }

    ret = avcodec_parameters_to_context(ffio->avCodecContext,
                                        ffio->avFormatContext->streams[ffio->videoStreamIndex]->codecpar);
    if(ret < 0){
      av_log(NULL, AV_LOG_ERROR,"Failed to set codec parameters.\n");
      return 4;
    }

    if(ffio->hw_enabled){
      ret = av_hwdevice_ctx_create(&(ffio->hwContext), AV_HWDEVICE_TYPE_CUDA,
                                   hw_device, NULL, 0);
      if(ret != 0){
        char errbuf[200];
        av_strerror(ret, errbuf, sizeof(errbuf));
        av_log(NULL, AV_LOG_ERROR,
               "initialize hardware context failed, ret: %d - %s.\n", ret, errbuf);
        return 4;
      }else{
        av_log(NULL, AV_LOG_INFO, "successfully initialize the nvidia cuda.\n");
        ffio->avCodecContext->hw_device_ctx = av_buffer_ref(ffio->hwContext);
      }
    } // end of hardware setting.

  }

  ret = avcodec_open2(ffio->avCodecContext, ffio->avCodec, NULL);
  if(ret < 0){
    av_log(NULL, AV_LOG_INFO, "%s\n", "can not open codec.");
    return 4;
  }

  return 0;
}

static void ffio_init_video_parameters(FFIO* ffio){
  ffio->imageWidth  = ffio->avCodecContext->width;
  ffio->imageHeight = ffio->avCodecContext->height;
}

static int ffio_init_memory_for_rgb_bytes(
    FFIO* ffio, FFIOMode mode,
    bool enableShm, const char* shmName, int shmSize, int shmOffset
){
  ffio->imageByteSize = ffio->imageHeight * ffio->imageWidth * FFIO_COLOR_DEPTH;
  ffio->rawFrame = (unsigned char *)malloc(ffio->imageByteSize);
  av_log(NULL, AV_LOG_INFO,
         "stream image size: %dx%dx%d = %d\n",
         ffio->imageWidth, ffio->imageHeight, FFIO_COLOR_DEPTH, ffio->imageByteSize);
  if(enableShm){
    ffio->shmEnabled = true;
    ffio->shmSize    = shmSize;

    int _shm_open_mode = mode == FFIO_MODE_DECODE ? O_RDWR     : O_RDONLY;
    int _shm_map_port  = mode == FFIO_MODE_DECODE ? PROT_WRITE : PROT_READ;
    ffio->shmFd        = shm_open(shmName, _shm_open_mode, 0666);
    if(ffio->shmFd == -1){
      av_log(NULL, AV_LOG_ERROR, "%s\n", "can not create shm fd.");
      return 5;
    }
    ffio->rawFrameShm = mmap(0, shmSize, _shm_map_port, MAP_SHARED, ffio->shmFd, shmOffset);
    if(ffio->rawFrameShm == MAP_FAILED){
      av_log(NULL, AV_LOG_ERROR, "%s\n", "can not map shm.");
      return 5;
    }
  }else{
    ffio->shmEnabled = false;
  }

  return 0;
}

FFIO *newFFIO(){
  FFIO* ffio = (FFIO*)malloc(sizeof(FFIO));
  resetFFIO(ffio);
  return ffio;
}

/**
 *  read the video context info, including format context and codec context.
 *
 *  params:
 *      hw_enabled: set to declare if use cuda gpu to accelerate.
 *      hw_device: which cuda to use, e.g. cuda:0
 *
 *  ret: int
 *      0: successfully start a video stream
 *      1: failing to open the target stream
 *      2: can not find the stream info
 *      3: can not process params to context
 *      4: can not open codec context
 *      5: failing to allocate memory
 */
int initFFIO(
    FFIO* ffio, FFIOMode mode, const char* streamUrl,
    bool hw_enabled, const char* hw_device,
    bool enableShm,  const char* shmName, int shmSize, int shmOffset
){
  int ret;

  ffio->ffioMode = mode;
  snprintf(ffio->targetUrl, sizeof(ffio->targetUrl), "%s", streamUrl);

  ret = ffio_init_avformat(ffio);
  if(ret != 0){ finalizeFFIO(ffio); return ret; }

  ffio->hw_enabled = hw_enabled;
  ret = ffio_init_avcodec(ffio, hw_device);
  if(ret != 0){ finalizeFFIO(ffio); return ret; }

  ffio_init_video_parameters(ffio);

  ffio->yuvFrame = av_frame_alloc();
  ffio->rgbFrame = av_frame_alloc();
  ffio->avPacket = av_packet_alloc();
  if( !(ffio->yuvFrame) || !(ffio->rgbFrame) || !(ffio->avPacket) ){
    av_log(NULL, AV_LOG_ERROR, "failed to allocate avPacket or avFrame.\n");
    finalizeFFIO(ffio);
    return 5;
  }

  ret = ffio_init_memory_for_rgb_bytes(ffio, mode, enableShm, shmName, shmSize, shmOffset);
  if(ret != 0){ finalizeFFIO(ffio); return ret; }

  ffio->ffioState = FFIO_STATE_READY;
  av_log(NULL, AV_LOG_INFO, "succeeded to initialize ffio.\n");
  return 0;
}

FFIO* finalizeFFIO(FFIO* ffio){
  ffio->ffioState = FFIO_STATE_NOT_READY;
  ffio->frameSeq  = -1;

  if(ffio->swsContext)     { sws_freeContext(ffio->swsContext);                }
  if(ffio->yuvFrame)       { av_frame_free( &(ffio->yuvFrame) );               }
  if(ffio->rgbFrame)       { av_frame_free( &(ffio->rgbFrame) );               }
  if(ffio->avPacket)       { av_packet_free( &(ffio->avPacket) );              }
  if(ffio->avCodecContext) { avcodec_free_context( &(ffio->avCodecContext) );  }

  if(ffio->avFormatContext){
    if(ffio->ffioMode == FFIO_MODE_DECODE){
      avformat_close_input( &(ffio->avFormatContext) );
    }else{
      if( !(ffio->avFormatContext->oformat->flags & AVFMT_NOFILE) ){
        avio_closep(&(ffio->avFormatContext->pb) );
      }
      avformat_free_context(ffio->avFormatContext);
    }
  }

  if(ffio->rawFrame  ){ free(ffio->rawFrame); }
  if(ffio->shmEnabled){
    if(ffio->rawFrameShm){ munmap(ffio->rawFrameShm, ffio->shmSize); }
    if(ffio->shmFd != -1){ close (ffio->shmFd); }
  }

  memset(ffio->targetUrl, '0', MAX_URL_LENGTH);

  resetFFIO(ffio);
  av_log(NULL, AV_LOG_INFO, "%s", "finished to unref video stream context.\n");
  return ffio;
}