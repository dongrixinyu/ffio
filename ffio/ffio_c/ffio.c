//
// Created by koisi on 2024/2/22.
//
#include <stdlib.h>

#include "ffio.h"

static void resetFFIO(FFIO* ffio){
  if(ffio==NULL){ return; }

  ffio->ffioState        = FFIO_STATE_NOT_READY;
  ffio->ffioMode         = FFIO_MODE_DECODE;
  ffio->frameSeq         = -1;
  ffio->hw_enabled       = false;

  ffio->shmEnabled       = false;
  ffio->shmFd            = -1;
  ffio->shmSize          = 0;

  ffio->videoStreamIndex = -1;
  ffio->imageWidth       = 0;
  ffio->imageHeight      = 0;
  ffio->imageByteSize    = 0;

  ffio->avFormatContext = NULL;
  ffio->avCodecContext  = NULL;
  ffio->avCodec         = NULL;
  ffio->avPacket        = NULL;
  ffio->yuvFrame        = NULL;
  ffio->rgbFrame        = NULL;
  ffio->swsContext      = NULL;
  ffio->hwContext       = NULL;

  ffio->rawFrame        = NULL;
  ffio->rawFrameShm     = NULL;
}

static int ffio_init_avformat(FFIO* ffio){
  int ret;

  if(ffio->ffioMode == FFIO_MODE_DECODE){
    ret = avformat_open_input(&ffio->avFormatContext, ffio->targetUrl,NULL, NULL);
    if(ret < 0){
      av_log(NULL, AV_LOG_ERROR, "can not open stream: `%s`.\n", ffio->targetUrl);
      return 1;
    }

    ret = avformat_find_stream_info(ffio->avFormatContext, NULL);
    if(ret < 0){
      av_log(NULL, AV_LOG_ERROR, "can not find stream info.\n");
      return 2;
    }
  }

  av_log(NULL, AV_LOG_INFO, "succeed to init avformat.\n");
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
    av_log(NULL, AV_LOG_ERROR, "can not open codec.\n");
    return 4;
  }

  return 0;
}

static int ffio_init_video_parameters(FFIO* ffio){
  ffio->imageWidth  = ffio->avCodecContext->width;
  ffio->imageHeight = ffio->avCodecContext->height;

  ffio->rgbFrame->width  = ffio->imageWidth;
  ffio->rgbFrame->height = ffio->imageHeight;
  ffio->rgbFrame->format = AV_PIX_FMT_RGB24;

  int ret = av_frame_get_buffer(ffio->rgbFrame, 0);
  if(ret != 0){
    av_log(NULL, AV_LOG_ERROR, "Failed to allocate memory for rgbFrame.\n");
    return 5;
  }

  return 0;
}

static int ffio_init_sws_context(FFIO* ffio){
  if(ffio->ffioMode == FFIO_MODE_DECODE){
    ffio->swsContext = sws_getContext(
        ffio->avCodecContext->width, ffio->avCodecContext->height, ffio->avCodecContext->pix_fmt,
        ffio->avCodecContext->width, ffio->avCodecContext->height, AV_PIX_FMT_RGB24,
        SWS_FAST_BILINEAR, NULL, NULL, NULL
    );
  }

  if(!ffio->swsContext){
    av_log(NULL, AV_LOG_ERROR, "can not open sws codec.\n");
    return 4;
  }else{
    return 0;
  }
}

static int ffio_init_memory_for_rgb_bytes(
    FFIO* ffio, FFIOMode mode,
    bool enableShm, const char* shmName, int shmSize, int shmOffset
){
  ffio->imageByteSize = ffio->imageHeight * ffio->imageWidth * FFIO_COLOR_DEPTH;
  ffio->rawFrame = (unsigned char *)malloc(ffio->imageByteSize);
  av_log(NULL, AV_LOG_INFO,
         "stream image size: %dx%dx%d = %d.\n",
         ffio->imageWidth, ffio->imageHeight, FFIO_COLOR_DEPTH, ffio->imageByteSize
  );
  if(enableShm){
    ffio->shmEnabled = true;
    ffio->shmSize    = shmSize;

    int _shm_open_mode = mode == FFIO_MODE_DECODE ? O_RDWR     : O_RDONLY;
    int _shm_map_port  = mode == FFIO_MODE_DECODE ? PROT_WRITE : PROT_READ;
    ffio->shmFd        = shm_open(shmName, _shm_open_mode, 0666);
    if(ffio->shmFd == -1){
      av_log(NULL, AV_LOG_ERROR, "can not create shm fd.\n");
      return 5;
    }
    ffio->rawFrameShm = mmap(0, shmSize, _shm_map_port, MAP_SHARED, ffio->shmFd, shmOffset);
    if(ffio->rawFrameShm == MAP_FAILED){
      av_log(NULL, AV_LOG_ERROR, "can not map shm.\n");
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

  ffio->yuvFrame = av_frame_alloc();
  ffio->rgbFrame = av_frame_alloc();
  ffio->avPacket = av_packet_alloc();
  if( !(ffio->yuvFrame) || !(ffio->rgbFrame) || !(ffio->avPacket) ){
    av_log(NULL, AV_LOG_ERROR, "failed to allocate avPacket or avFrame.\n");
    finalizeFFIO(ffio);
    return 5;
  }

  ret = ffio_init_video_parameters(ffio);
  if(ret != 0){ finalizeFFIO(ffio); return ret; }

  ret = ffio_init_sws_context(ffio);
  if(ret != 0){ finalizeFFIO(ffio); return ret; }

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
  ffio->ffioState = FFIO_STATE_CLOSED;
  av_log(NULL, AV_LOG_INFO, "finished to unref video stream context.\n");
  return ffio;
}

static int convertYUV2RGB(FFIO* ffio){
  return sws_scale(
      ffio->swsContext, (uint8_t const* const*)ffio->yuvFrame->data,
      ffio->yuvFrame->linesize, 0, ffio->avCodecContext->height,
      ffio->rgbFrame->data, ffio->rgbFrame->linesize
  );
}

static int decodeOneFrameToAVFrame(FFIO* ffio){
  if(ffio->ffioState == FFIO_STATE_READY){ ffio->ffioState = FFIO_STATE_RUNNING; }
  if(ffio->ffioState != FFIO_STATE_RUNNING){
    av_log(NULL, AV_LOG_ERROR,"ffio not ready.\n");
    return -7;
  }

  int read_ret, send_ret, recv_ret;
  while( (read_ret = av_read_frame(ffio->avFormatContext, ffio->avPacket)) >= 0) {

    if(ffio->avPacket->stream_index == ffio->videoStreamIndex) {
ENDPOINT_RESEND_PACKET:
      send_ret = avcodec_send_packet(ffio->avCodecContext, ffio->avPacket);
      if(send_ret == AVERROR_EOF){
        goto ENDPOINT_AV_ERROR_EOF;
      } else if (send_ret < 0){
        char errbuf[200];
        av_strerror(send_ret, errbuf, sizeof(errbuf));
        av_log(NULL, AV_LOG_ERROR,
               "an error while avcodec_send_packet: %d - %s.\n", send_ret, errbuf);
        return -2;
      }

      // send_ret == 0 or AVERROR(EAGAIN):
      recv_ret = avcodec_receive_frame(ffio->avCodecContext, ffio->yuvFrame);
      if(recv_ret == 0){
        convertYUV2RGB(ffio);
        ffio->frameSeq += 1;
        return 0;
      } else if (recv_ret == AVERROR_EOF){
        goto ENDPOINT_AV_ERROR_EOF;
      } else if (recv_ret == AVERROR(EAGAIN)){
        if(send_ret == AVERROR(EAGAIN)){ usleep(10000); goto ENDPOINT_RESEND_PACKET; }
        else{ continue; }
      } else {
        char errbuf[200];
        av_strerror(recv_ret, errbuf, sizeof(errbuf));
        av_log(NULL, AV_LOG_ERROR,
               "an error while avcodec_receive_frame: %d - %s.\n", recv_ret, errbuf);
        return -1;
      }

    } // if this is a packet of the desired stream.

  }

  if(read_ret == AVERROR_EOF){
    avcodec_send_packet(ffio->avCodecContext, NULL);
ENDPOINT_AV_ERROR_EOF:
    av_log(NULL, AV_LOG_INFO, "here is the end of this stream.\n");
    ffio->ffioState = FFIO_STATE_END;
    return 1;
  }else{
    char errbuf[200];
    av_strerror(read_ret, errbuf, sizeof(errbuf));
    av_log(NULL, AV_LOG_ERROR,
           "an error while av_read_frame: %d - %s.\n", read_ret, errbuf);
    return -6;
  }

}

/**
 * to get one frame from the stream
 *
 * ret: int
 *     0  means getting a frame successfully.
 *     1  means the stream has been to the end
 *     -1 means other error concerning avcodec_receive_frame.
 *     -2 means other error concerning avcodec_send_packet.
 *     -4 means packet mismatch & unable to seek to the next packet.
 *     -5 means timeout to get the next packet, need to exit this function.
 *     -6 means other error concerning av_read_frame.
 *     -7 means ffio not get ready.
 *     -9 means shm not enabled.
 */
int decodeOneFrame(FFIO* ffio){
  int ret = decodeOneFrameToAVFrame(ffio);
  if(ret == 0){
    // memcpy might be dismissed to accelerate.
    memcpy(ffio->rawFrame, ffio->rgbFrame->data[0], ffio->imageByteSize);
  }
  return ret;
}

int decodeOneFrameToShm(FFIO* ffio, int shmOffset){
  if(!ffio->shmEnabled){ return -9; }

  int ret = decodeOneFrameToAVFrame(ffio);
  if(ret == 0){
    memcpy(ffio->rgbFrame + shmOffset, ffio->rgbFrame->data[0], ffio->imageByteSize);
  }
  return ret;
}
