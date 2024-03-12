//
// Created by koisi on 2024/2/22.
//
#include <stdlib.h>

#include "ffio.h"

static char str_buffer[128];
static char* get_str_time(){
  time_t raw_time;
  struct tm *time_info;
  time(&raw_time);
  time_info = localtime(&raw_time);
  strftime(str_buffer, sizeof(str_buffer), "%Y-%m-%d %H:%M:%S", time_info);
  return str_buffer;
}
static char* get_ffioMode(FFIO* ffio){
  return ffio->ffioMode == FFIO_MODE_DECODE ? "D" : "E";
}
static bool is_empty_string(const char *str) {
  return str == NULL || *str == '\0';
}

static int64_t pts_get_current_even(FFIO* ffio){
  /*
   * For live-streaming scenarios.
   * Will handle frame being sent too quickly or too slowly,
   * to push the live stream at a stable and smooth frame rate.
   */
  static int64_t last_time  = -1;
  static int     drop_count =  0;

  int64_t current_time = av_gettime();

  if(ffio->time_start_at == -1){ ffio->time_start_at = current_time; last_time = current_time; }
  int64_t dt_last  = current_time - last_time;
  int64_t dt_rltv  = current_time - ffio->time_start_at;
  int64_t pts_diff = av_rescale_q(dt_last, (AVRational){1, AV_TIME_BASE}, ffio->avCodecContext->time_base);
  int64_t pts_rltv = av_rescale_q(dt_rltv, (AVRational){1, AV_TIME_BASE}, ffio->avCodecContext->time_base);
  last_time        = current_time;

  if(ffio->pts_anchor > pts_rltv + FFIO_PTS_GAP_TOLERANCE_EVEN){
    ++drop_count;
    LOG_DEBUG_T("[%s] Sending too fast, exceeding the preset FPS, new pkt should be discarded. "
                "(pts: %d, duration: %.2fs, dropped: %d)",
                get_ffioMode(ffio), (int)ffio->pts_anchor, (float)dt_rltv/1000000.0, drop_count);
    return -1;
  }else if(ffio->pts_anchor < pts_rltv - FFIO_PTS_GAP_TOLERANCE_EVEN){
    ffio->pts_anchor = pts_rltv - FFIO_PTS_GAP_TOLERANCE_EVEN;
#ifdef DEBUG
    LOG_DEBUG_T("[%s] Sending too slow, this pkt will be set with tolerant pts. "
                "(tolerant_pts: %d, duration: %.2fs, pts_rltv: %d)",
                get_ffioMode(ffio), (int)ffio->pts_anchor, (float)dt_rltv/1000000.0, (int)pts_rltv);
#endif
    return ffio->pts_anchor;
  }

  if(ffio->pts_anchor == -1){
    ffio->pts_anchor = 0;
    return ffio->pts_anchor;
  }

  if(pts_diff<=FFIO_PTS_GAP_TOLERANCE_EVEN){ ffio->pts_anchor += 1; }
  else{
    ffio->pts_anchor += pts_diff - FFIO_PTS_GAP_TOLERANCE_EVEN;
    LOG_WARNING_T("[%s] get pts with large gap: %d ms.", get_ffioMode(ffio), (int)(dt_last/1000));
  }
  return ffio->pts_anchor;
}
static int64_t pts_get_current_relative(FFIO* ffio){
  /*
   * You can use this function,
   * if you sure you are calling encodeOneFrame() at a stable rate.
   */
  int64_t current_time = av_gettime();
  if (ffio->time_start_at == -1) { ffio->time_start_at = current_time; }
  int64_t dt = current_time - ffio->time_start_at;
  ffio->pts_anchor = av_rescale_q(dt, (AVRational){1, AV_TIME_BASE}, ffio->avCodecContext->time_base);
  return ffio->pts_anchor;
}
static int64_t pts_get_current_increase(FFIO* ffio){
  /*
   * Simply increase pts every time you call encodeOneFrame().
   * For example, when you are getting source images from local files.
   */
  ffio->pts_anchor = ffio->pts_anchor == -1 ? 0 : ++(ffio->pts_anchor);
  return ffio->pts_anchor;
}
static int64_t pts_get_current_direct(FFIO* ffio){
  /*
   * Use this function, you should take responsibility to
   * manually set `ffio->pts_anchor` before calling encodeOneFrame().
   */
  return ffio->pts_anchor;
}

static void hw_set_pix_fmt_according_avcodec(FFIO* ffio, const char* hw_device){
  if(ffio->hw_enabled){
    if( strncmp(hw_device, "cuda", strlen("cuda")) == 0 ){
      ffio->hw_pix_fmt = AV_PIX_FMT_CUDA;
      ffio->sw_pix_fmt = AV_PIX_FMT_NV12;
    } else if( strncmp(hw_device, "videotoolbox", strlen("videotoolbox")) == 0 ){
      ffio->hw_pix_fmt = AV_PIX_FMT_VIDEOTOOLBOX;
      ffio->sw_pix_fmt = AV_PIX_FMT_NV12;
    } else {
      ffio->hw_pix_fmt = find_avcodec_1st_hw_pix_fmt(ffio->avCodec);
      ffio->sw_pix_fmt = find_avcodec_1st_sw_pix_fmt(ffio->avCodec);
    }
  }
}
static enum AVPixelFormat hw_callback_for_get_pix_fmts(
    AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts){
  /*
   * Refers to the official FFmpeg examples: hw_decode.c
   */
  const enum AVPixelFormat target = *(enum AVPixelFormat*)(ctx->opaque);

  const enum AVPixelFormat *p;
  for (p = pix_fmts; *p != -1; p++) {
    if (*p == target) { return *p; }
  }
  fprintf(stderr, "Failed to get HW surface format.\n");
  return AV_PIX_FMT_NONE;
}

static int hw_init_encoder(FFIO* ffio, const char* hw_device){
  /**
   * Refers to the official FFmpeg examples: vaapi_encode.c
   *
   * hw_device: "cuda"
   */
  enum AVHWDeviceType hw_type = av_hwdevice_find_type_by_name(hw_device);
  if(hw_type == AV_HWDEVICE_TYPE_NONE){ return FFIO_ERROR_HARDWARE_ACCELERATION; }

  hw_set_pix_fmt_according_avcodec(ffio, hw_device);
  LOG_INFO("[E][init][hw] force set codec_ctx pix_fmt to hw_pix_fmt: %s.",
           av_get_pix_fmt_name(ffio->hw_pix_fmt) );
  ffio->avCodecContext->pix_fmt = ffio->hw_pix_fmt;

  int ret = av_hwdevice_ctx_create(&(ffio->hwContext), hw_type,NULL, NULL, 0);
  if(ret != 0){
    LOG_ERROR("[E][init][hw] failed to init hw_ctx, ret: %d - %s.", ret, av_err2str(ret));
    return FFIO_ERROR_HARDWARE_ACCELERATION;
  }

  AVBufferRef* hw_frames_ref = av_hwframe_ctx_alloc(ffio->hwContext);
  if(hw_frames_ref==NULL){
    LOG_ERROR("[E][init][hw] failed to alloc hw frame_ctx.");
    return FFIO_ERROR_HARDWARE_ACCELERATION;
  }

  AVHWFramesContext* frames_ctx = (AVHWFramesContext *)(hw_frames_ref->data);
  frames_ctx->format    = ffio->hw_pix_fmt;
  frames_ctx->sw_format = ffio->sw_pix_fmt;
  frames_ctx->width     = ffio->codecParams->width;
  frames_ctx->height    = ffio->codecParams->height;
  frames_ctx->initial_pool_size = 20;
  LOG_INFO("[E][init][hw][pix_fmt]    format:  %s.",    av_get_pix_fmt_name(frames_ctx->format));
  LOG_INFO("[E][init][hw][pix_fmt] sw_format:  %s.", av_get_pix_fmt_name(frames_ctx->sw_format));

  ret = av_hwframe_ctx_init(hw_frames_ref);
  if(ret<0){
    LOG_ERROR("[E][init][hw] Failed to init hwFrame_ctx, ret: %d - %s.", ret, av_err2str(ret));
    av_buffer_unref(&hw_frames_ref);
    return FFIO_ERROR_HARDWARE_ACCELERATION;
  }

  ffio->avCodecContext->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
  av_buffer_unref(&hw_frames_ref);
  return ffio->avCodecContext->hw_frames_ctx==NULL ? AVERROR(ENOMEM) : 0;
}

static int hw_init_decoder(FFIO* ffio, const char* hw_device){
  enum AVHWDeviceType type = av_hwdevice_find_type_by_name(hw_device);
  if(type == AV_HWDEVICE_TYPE_NONE){ return FFIO_ERROR_HARDWARE_ACCELERATION; }

  hw_set_pix_fmt_according_avcodec(ffio, hw_device);
  ffio->avCodecContext->opaque     = &(ffio->hw_pix_fmt);
  ffio->avCodecContext->get_format = hw_callback_for_get_pix_fmts;

  int ret = av_hwdevice_ctx_create(&(ffio->hwContext), type,NULL, NULL, 0);
  if(ret != 0){
    LOG_ERROR("[D][init][hw] failed to create hw_ctx, ret: %d - %s.", ret, av_err2str(ret));
    return FFIO_ERROR_HARDWARE_ACCELERATION;
  }
  ffio->avCodecContext->hw_device_ctx = av_buffer_ref(ffio->hwContext);
  LOG_INFO("[D][init][hw] successfully initialize hw ctx.");
  return 0;
}

static void ffio_reset(FFIO* ffio){
  if(ffio==NULL){ return; }

  ffio->ffioState        = FFIO_STATE_INIT;
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

  ffio->pts_anchor       = -1;

  ffio->avFormatContext = NULL;
  ffio->avCodecContext  = NULL;
  ffio->avCodec         = NULL;
  ffio->avPacket        = NULL;
  ffio->avFrame         = NULL;
  ffio->hwFrame         = NULL;
  ffio->rgbFrame        = NULL;
  ffio->swsContext      = NULL;

  ffio->rawFrame        = NULL;
  ffio->rawFrameShm     = NULL;

  ffio->hwContext       = NULL;
  ffio->hw_pix_fmt      = AV_PIX_FMT_NONE;
  ffio->sw_pix_fmt      = AV_PIX_FMT_NONE;

  ffio->codecParams     = NULL;
  ffio->time_start_at   = -1;

  ffio->sei_buf[MAX_SEI_LENGTH-1] = '\0';
  ffio->frame = (FFIOFrame){0,0,0,0, NULL, NULL};

  LOG_INFO_T("[FFIO_STATE_INIT] set ffio contents to NULL.");
}

static int ffio_init_decode_avformat(FFIO* ffio){
  int ret;

  ret = avformat_open_input(&ffio->avFormatContext, ffio->targetUrl,NULL, NULL);
  if(ret < 0){
    LOG_ERROR("[D][init] failed to open stream.");
    return FFIO_ERROR_READ_OR_WRITE_TARGET;
  }

  ret = avformat_find_stream_info(ffio->avFormatContext, NULL);
  if(ret < 0){
    LOG_ERROR("[D][init] failed to find stream info.");
    return FFIO_ERROR_AVFORMAT_FAILURE;
  }

  LOG_INFO("[D][init] succeeded to init avformat.");
  return 0;
}
static int ffio_init_encode_avformat(FFIO* ffio){
  char* fmt_name = ffio->codecParams->format;
  if( fmt_name != NULL && fmt_name[0] == '\0'){ fmt_name = NULL; }
  if( fmt_name == NULL ){
    if     (strncmp(ffio->targetUrl, "rtmp://", strlen("rtmp://")) == 0){ fmt_name = "flv";    }
    else if(strncmp(ffio->targetUrl, "srt://",  strlen("srt://") ) == 0){ fmt_name = "mpegts"; }
  }

  int ret = avformat_alloc_output_context2(&ffio->avFormatContext, NULL, fmt_name, ffio->targetUrl);
  if( ret < 0 ){
    LOG_ERROR_T("[E][init] failed to open stream.");
    return FFIO_ERROR_READ_OR_WRITE_TARGET;
  }
  LOG_INFO("[E][init] succeeded to init avformat.");
  return 0;
}

static int ffio_init_decode_avcodec(FFIO* ffio, const char *hw_device) {
  int ret;

  ffio->videoStreamIndex = av_find_best_stream(
      ffio->avFormatContext, AVMEDIA_TYPE_VIDEO,
      -1, -1,(const AVCodec **)&(ffio->avCodec), 0);
  if(ffio->videoStreamIndex < 0){
    LOG_ERROR("[D][init] failed to find best stream.");
    return FFIO_ERROR_AVFORMAT_FAILURE;
  }

  ffio->avCodecContext = avcodec_alloc_context3(ffio->avCodec);
  if(!ffio->avCodecContext){
    LOG_ERROR("[D][init] failed to alloc codec ctx.");
    return FFIO_ERROR_AVCODEC_FAILURE;
  }

  ret = avcodec_parameters_to_context(ffio->avCodecContext,
                                      ffio->avFormatContext->streams[ffio->videoStreamIndex]->codecpar);
  if(ret < 0){
    LOG_ERROR("[D][init] failed to set codec parameters.");
    return FFIO_ERROR_AVCODEC_FAILURE;
  }

  if(ffio->hw_enabled){
    ret = hw_init_decoder(ffio, hw_device);
    if(ret!=0){ return FFIO_ERROR_AVCODEC_FAILURE; }
  }

  ret = avcodec_open2(ffio->avCodecContext, ffio->avCodec, NULL);
  if(ret < 0){
    LOG_ERROR("[D][init] failed to open codec.");
    return FFIO_ERROR_AVCODEC_FAILURE;
  }else{
    LOG_INFO("[D][init] succeeded to init codec.");
    return 0;
  }
}
static int ffio_init_encode_avcodec(FFIO* ffio, const char* hw_device) {
  /*
   * Refers to the official FFmpeg examples: encoder_video.c and transcoding.c
   *
   * Returns:
   *   success - 0
   *
   * Steps 1: codec     = avcodec_find_encoder_by_name
   * Steps 2: codec_ctx = avcodec_alloc_context3(codec)
   * Steps 3: set codec_ctx params
   * Steps 4: avcodec_open2(codec_ctx, codec)
   */
  const char *codec_name = ffio->codecParams->codec;
  ffio->avCodec = (AVCodec*)avcodec_find_encoder_by_name(codec_name);
  if (!ffio->avCodec){
    LOG_ERROR("[E][init] Codec '%s' not found.", codec_name);
    return FFIO_ERROR_AVCODEC_FAILURE;
  }

  ffio->avCodecContext = avcodec_alloc_context3(ffio->avCodec);
  if (!ffio->avCodecContext){
    LOG_ERROR("[E][init] Could not allocate video codec context.");
    return FFIO_ERROR_AVCODEC_FAILURE;
  }

  ffio->avCodecContext->width        = ffio->codecParams->width;
  ffio->avCodecContext->height       = ffio->codecParams->height;
  ffio->avCodecContext->bit_rate     = ffio->codecParams->bitrate;
  ffio->avCodecContext->gop_size     = ffio->codecParams->gop;
  ffio->avCodecContext->max_b_frames = ffio->codecParams->b_frames;
  ffio->avCodecContext->time_base    = ffio->codecParams->pts_trick == FFIO_PTS_TRICK_RELATIVE ?
      FFIO_TIME_BASE_MILLIS : (AVRational){1, ffio->codecParams->fps, };
  ffio->avCodecContext->framerate    = (AVRational){ffio->codecParams->fps, 1};

  av_opt_set(ffio->avCodecContext->priv_data, "profile", ffio->codecParams->profile, 0);
  av_opt_set(ffio->avCodecContext->priv_data, "preset",  ffio->codecParams->preset,  0);
  if(!is_empty_string(ffio->codecParams->tune)){
    av_opt_set(ffio->avCodecContext->priv_data, "tune", ffio->codecParams->tune, 0);
  }
  ffio->avCodecContext->pix_fmt = is_empty_string(ffio->codecParams->pix_fmt) ?
      find_avcodec_1st_sw_pix_fmt(ffio->avCodec) : av_get_pix_fmt(ffio->codecParams->pix_fmt);

  // for compatibility: if GLOBAL_HEADER is needed by target format.
  if (ffio->avFormatContext->oformat->flags & AVFMT_GLOBALHEADER){
    ffio->avCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }

  int ret;
  if(ffio->hw_enabled){
    ret = hw_init_encoder(ffio, hw_device);
    if(ret!=0){ LOG_ERROR("[E][init][hw] failed to init hw ctx."); return ret; }
    LOG_INFO("[E][init][hw] succeeded to init hw ctx.");
  }

  ret = avcodec_open2(ffio->avCodecContext, ffio->avCodec, NULL);
  if(ret < 0){
    LOG_ERROR("[E][init] can not open codec.");
    return FFIO_ERROR_AVCODEC_FAILURE;
  }
  LOG_INFO("[E][init] succeeded to init codec ctx.");
  return 0;
}
static int ffio_init_encode_create_stream(FFIO* ffio){
  int ret;
  ffio->videoStreamIndex = 0;   // Only create one stream to output format.
  AVStream *stream = avformat_new_stream(ffio->avFormatContext, NULL);
  if (!stream) {
    LOG_ERROR("[E][init] failed to create new AVStream.");
    return FFIO_ERROR_AVFORMAT_FAILURE;
  }
  ret = avcodec_parameters_from_context(stream->codecpar, ffio->avCodecContext);
  if (ret < 0) {
    LOG_ERROR("[E][init] failed to copy encoder parameters to AVStream.");
    return FFIO_ERROR_AVFORMAT_FAILURE;
  }
  stream->time_base = ffio->avCodecContext->time_base;

  av_dump_format(ffio->avFormatContext, 0, ffio->targetUrl, 1);

  if (!(ffio->avFormatContext->oformat->flags & AVFMT_NOFILE)) {
    ret = avio_open(&(ffio->avFormatContext->pb), ffio->targetUrl, AVIO_FLAG_WRITE);
    if (ret < 0) {
      LOG_ERROR("[E][init] avio_open failed %d - %s.", ret, av_err2str(ret));
      return FFIO_ERROR_READ_OR_WRITE_TARGET;
    }
  }

  // stream->time_base may be changed after this.
  ret = avformat_write_header(ffio->avFormatContext, NULL);
  if (ret < 0) {
    LOG_ERROR("[E][init] avformat_write_header failed %d - %s.", ret, av_err2str(ret));
    return FFIO_ERROR_READ_OR_WRITE_TARGET;
  }

  LOG_INFO("[E][init] succeeded to create video stream and write header to target.");
  return 0;
}

static int ffio_init_packet_and_frame(FFIO* ffio){
  ffio->avFrame  = av_frame_alloc();
  ffio->hwFrame  = av_frame_alloc();
  ffio->rgbFrame = av_frame_alloc();
  ffio->avPacket = av_packet_alloc();
  if( !(ffio->avFrame) || !(ffio->hwFrame) || !(ffio->rgbFrame) || !(ffio->avPacket) ){
    av_log(NULL, AV_LOG_ERROR, "failed to allocate avPacket or avFrame.\n");
    finalizeFFIO(ffio);
    return FFIO_ERROR_AVFRAME_ALLOCATION;
  }

  ffio->imageWidth   = ffio->avCodecContext->width;
  ffio->imageHeight  = ffio->avCodecContext->height;
  ffio->frame.width  = ffio->imageWidth;
  ffio->frame.height = ffio->imageHeight;

  int ret;
  ffio->rgbFrame->width  = ffio->imageWidth;
  ffio->rgbFrame->height = ffio->imageHeight;
  ffio->rgbFrame->format = AV_PIX_FMT_RGB24;
  ret = av_frame_get_buffer(ffio->rgbFrame, 0);
  if(ret != 0){
    LOG_ERROR("[%s][init] failed to alloc rgbFrame.", get_ffioMode(ffio));
    return FFIO_ERROR_AVFRAME_ALLOCATION;
  }

  if(ffio->ffioMode == FFIO_MODE_ENCODE){
    ffio->avFrame->width  = ffio->imageWidth;
    ffio->avFrame->height = ffio->imageHeight;
    ffio->avFrame->format = ffio->hw_enabled ? ffio->sw_pix_fmt : ffio->avCodecContext->pix_fmt;
    ret = av_frame_get_buffer(ffio->avFrame, 0);
    if(ret != 0){
      LOG_ERROR("[E][init] failed to alloc avFrame.");
      return FFIO_ERROR_AVFRAME_ALLOCATION;
    }

    if(ffio->hw_enabled){
      ret = av_hwframe_get_buffer(ffio->avCodecContext->hw_frames_ctx, ffio->hwFrame, 0);
      if(ret<0){
        LOG_ERROR("[E][init] failed to alloc hwFrame.");
        return FFIO_ERROR_AVFRAME_ALLOCATION;
      }
    }
  }

  return 0;
}

static int ffio_init_sws_context(FFIO* ffio){
  LOG_INFO("[%s][init][pix_fmt]      ffio->avFrame    : %s.", get_ffioMode(ffio), av_get_pix_fmt_name(ffio->avFrame->format));
  LOG_INFO("[%s][init][pix_fmt]      ffio->hwFrame    : %s.", get_ffioMode(ffio), av_get_pix_fmt_name(ffio->hwFrame->format));
  LOG_INFO("[%s][init][pix_fmt]      ffio->rgbFrame   : %s.", get_ffioMode(ffio), av_get_pix_fmt_name(ffio->rgbFrame->format));
  LOG_INFO("[%s][init][pix_fmt]      ffio->hw_pix_fmt : %s.", get_ffioMode(ffio), av_get_pix_fmt_name(ffio->hw_pix_fmt));
  LOG_INFO("[%s][init][pix_fmt]      ffio->sw_pix_fmt : %s.", get_ffioMode(ffio), av_get_pix_fmt_name(ffio->sw_pix_fmt));
  LOG_INFO("[%s][init][pix_fmt] codec_ctx->sw_pix_fmt : %s.", get_ffioMode(ffio), av_get_pix_fmt_name(ffio->avCodecContext->sw_pix_fmt));
  LOG_INFO("[%s][init][pix_fmt] codec_ctx->pix_fmt    : %s.", get_ffioMode(ffio), av_get_pix_fmt_name(ffio->avCodecContext->pix_fmt));

  if(ffio->ffioMode == FFIO_MODE_DECODE){
    enum AVPixelFormat srcFormat = ffio->hw_enabled ? ffio->sw_pix_fmt : ffio->avCodecContext->pix_fmt;
    LOG_INFO("[D][init][pix_fmt]    swscale src_fmt    : %s.", av_get_pix_fmt_name(srcFormat));
    ffio->swsContext = sws_getContext(
        ffio->avCodecContext->width, ffio->avCodecContext->height, srcFormat,
        ffio->avCodecContext->width, ffio->avCodecContext->height, AV_PIX_FMT_RGB24,
        SWS_FAST_BILINEAR, NULL, NULL, NULL
    );
  }

  if(ffio->ffioMode == FFIO_MODE_ENCODE){
    enum AVPixelFormat dstFormat = ffio->hw_enabled ? ffio->sw_pix_fmt : ffio->avCodecContext->pix_fmt;
    LOG_INFO("[E][init][pix_fmt]    swscale dst_fmt    : %s.", av_get_pix_fmt_name(dstFormat));
    ffio->swsContext = sws_getContext(
        ffio->avCodecContext->width, ffio->avCodecContext->height, AV_PIX_FMT_RGB24,
        ffio->avCodecContext->width, ffio->avCodecContext->height, dstFormat,
        SWS_FAST_BILINEAR, NULL, NULL, NULL
    );
  }

  if(!ffio->swsContext){
    LOG_ERROR("[%s][init] can not open sws codec.", get_ffioMode(ffio));
    return FFIO_ERROR_SWS_FAILURE;
  }else{
    LOG_INFO("[%s][init] succeeded to init sws ctx.", get_ffioMode(ffio));
    return 0;
  }
}

static int ffio_init_memory_for_rgb_bytes(
    FFIO* ffio, FFIOMode mode,
    bool enableShm, const char* shmName, int shmSize, int shmOffset
){
  ffio->imageByteSize = ffio->imageHeight * ffio->imageWidth * FFIO_COLOR_DEPTH;
  ffio->rawFrame = (unsigned char *)malloc(ffio->imageByteSize);
  LOG_INFO("[%s][init] stream image size: %dx%dx%d = %d.",
           get_ffioMode(ffio),
           ffio->imageWidth, ffio->imageHeight, FFIO_COLOR_DEPTH, ffio->imageByteSize
  );
  if(enableShm){
    ffio->shmEnabled = true;
    ffio->shmSize    = shmSize;

    int _shm_open_mode = mode == FFIO_MODE_DECODE ? O_RDWR     : O_RDONLY;
    int _shm_map_port  = mode == FFIO_MODE_DECODE ? PROT_WRITE : PROT_READ;
    ffio->shmFd        = shm_open(shmName, _shm_open_mode, 0666);
    if(ffio->shmFd == -1){
      LOG_ERROR("[%s][init] failed to bind shm fd with name: %s.", get_ffioMode(ffio), shmName);
      return FFIO_ERROR_SHM_FAILURE;
    }
    ffio->rawFrameShm = mmap(0, shmSize, _shm_map_port, MAP_SHARED, ffio->shmFd, shmOffset);
    if(ffio->rawFrameShm == MAP_FAILED){
      LOG_ERROR("[%s][init] failed to map shm with name: %s.", get_ffioMode(ffio), shmName);
      return FFIO_ERROR_SHM_FAILURE;
    }
    LOG_INFO("[%s][init] succeeded to bind shm with name: %s.", get_ffioMode(ffio), shmName);
  }else{
    ffio->shmEnabled = false;
  }

  return 0;
}

static int ffio_init_check_and_set_codec_params(FFIO* ffio, CodecParams* params, const char* hw_device){
  if(params==NULL){
    LOG_ERROR("[%s] Please provide a non-NULL codec params.", get_ffioMode(ffio));
    return FFIO_ERROR_WRONG_CODEC_PARAMS;
  }

  if(   params->width       <=   0 ){ params->width    = 1920;      }
  if(   params->height      <=   0 ){ params->height   = 1080;      }
  if(   params->bitrate     <=   0 ){ params->bitrate  = 2400*1024; }
  if(   params->fps         <=   0 ){ params->fps      = 24;        }
  if(   params->gop         <    0 ){ params->gop      = 48;        }
  if(   params->b_frames    <    0 ){ params->b_frames = 0;         }
  if(  (params->profile)[0] == '\0'){ snprintf(params->profile, sizeof(params->profile), "%s", "main"       ); }
  if(  (params->preset )[0] == '\0'){ snprintf(params->preset,  sizeof(params->preset ), "%s", "veryfast"   ); }

  if(  (params->codec  )[0] == '\0'){

    if(ffio->hw_enabled){
      if( strncmp(hw_device, "cuda", strlen("cuda")) == 0 ){
        snprintf(params->codec, sizeof(params->codec), "%s", "h264_nvenc");
      }else if( strncmp(hw_device, "videotoolbox", strlen("videotoolbox")) == 0 ){
        snprintf(params->codec, sizeof(params->codec), "%s", "h264_videotoolbox");
      }else{
        LOG_ERROR("[%s] hw acceleration for [%s] is currently not supported for ffio.",
                  get_ffioMode(ffio), hw_device);
        return FFIO_ERROR_HARDWARE_ACCELERATION;
      }
    }else{
      snprintf(params->codec, sizeof(params->codec), "%s", "libx264");
    }

  }
  
  switch(params->pts_trick){
    case FFIO_PTS_TRICK_DIRECT:
      LOG_INFO("[%s] using FFIO_PTS_TRICK_DIRECT.",   get_ffioMode(ffio));
      ffio->get_current_pts = pts_get_current_direct;              break;
    case FFIO_PTS_TRICK_RELATIVE:
      LOG_INFO("[%s] using FFIO_PTS_TRICK_RELATIVE.", get_ffioMode(ffio));
      ffio->get_current_pts = pts_get_current_relative;            break;
    case FFIO_PTS_TRICK_INCREASE:
      LOG_INFO("[%s] using FFIO_PTS_TRICK_INCREASE.", get_ffioMode(ffio));
      ffio->get_current_pts = pts_get_current_increase;            break;
    case FFIO_PTS_TRICK_EVEN:
    default:
      LOG_INFO("[%s] using FFIO_PTS_TRICK_EVEN.",     get_ffioMode(ffio));
      ffio->get_current_pts = pts_get_current_even;
  }

  ffio->codecParams = params;
  return 0;
}

static AVFrame* convertFromRGBFrame(FFIO* ffio, unsigned char* rgbBytes){
  /**
   * If hw not enabled, directly return ffio->avFrame.
   * Else, convert ffio->avFrame to ffio->hwFrame, then return ffio->hwFrame.
   */
  av_image_fill_arrays(ffio->rgbFrame->data, ffio->rgbFrame->linesize,
                       rgbBytes, AV_PIX_FMT_RGB24, ffio->imageWidth, ffio->imageHeight, 1);

  sws_scale(
      ffio->swsContext, (const uint8_t* const*)ffio->rgbFrame->data,
      ffio->rgbFrame->linesize, 0, ffio->avCodecContext->height,
      ffio->avFrame->data, ffio->avFrame->linesize
  );

  AVFrame *avFrame = ffio->avFrame;
  if(ffio->hw_enabled){
    av_hwframe_transfer_data(ffio->hwFrame, ffio->avFrame, 0);
    avFrame = ffio->hwFrame;
  }
  return avFrame;
}

static int convertToRgbFrame(FFIO* ffio){
  AVFrame *src_frame = ffio->avFrame;
  if( ffio->hw_enabled && (src_frame->format == ffio->hw_pix_fmt) ){
    int ret = av_hwframe_transfer_data(ffio->hwFrame, ffio->avFrame, 0);
    if(ret<0) {
      av_log(NULL, AV_LOG_ERROR, "failed to transfer avframe from hardware.\n");
      return -1;
    }
    src_frame=ffio->hwFrame;
  }

  return sws_scale(
      ffio->swsContext, (uint8_t const* const*)src_frame->data,
      src_frame->linesize, 0, ffio->avCodecContext->height,
      ffio->rgbFrame->data, ffio->rgbFrame->linesize
  );
}

static FFIOFrame* decodeOneFrameToAVFrame(FFIO* ffio){
  /*
   * Returns:
   *   success - 0
   *
   * Process:
   **               read             send            recv
   **     AVFormat  ---->  AVPacket  ---->  AVCodec  ---->  AVFrame
   **                   (encoded data)     [decoding]    (decoded data)
   */
  if(ffio->ffioState == FFIO_STATE_READY){ ffio->ffioState = FFIO_STATE_RUNNING; }
  if(ffio->ffioState != FFIO_STATE_RUNNING){
    LOG_ERROR_T("[D] ffio not ready.");
    ffio->frame.err = FFIO_ERROR_FFIO_NOT_AVAILABLE; goto ENDPOINT_DECODE_ERROR;
  }

  int read_ret, send_ret, recv_ret;
  while(true) {
    av_packet_unref(ffio->avPacket);
    read_ret = av_read_frame(ffio->avFormatContext, ffio->avPacket);
    if(read_ret<0){ break; }

    if(ffio->avPacket->stream_index == ffio->videoStreamIndex) {
ENDPOINT_RESEND_PACKET:
      send_ret = avcodec_send_packet(ffio->avCodecContext, ffio->avPacket);
      if(send_ret == AVERROR_EOF){
        goto ENDPOINT_DECODE_EOF;
      } else if (send_ret < 0 && send_ret != AVERROR(EAGAIN)){
        LOG_ERROR("[D] error occurred while send packet to codec: %d - %s.",
                  send_ret, av_err2str(send_ret));
        ffio->frame.err = FFIO_ERROR_SEND_TO_CODEC; goto ENDPOINT_DECODE_ERROR;
      }

      // send_ret == 0 or AVERROR(EAGAIN):
      recv_ret = avcodec_receive_frame(ffio->avCodecContext, ffio->avFrame);
      if(recv_ret == 0){
        if( convertToRgbFrame(ffio) < 0 ){ ffio->frame.err = FFIO_ERROR_SWS_FAILURE; goto ENDPOINT_DECODE_ERROR; }
        if( (ffio->frameSeq)%LOG_PRINT_FRAME_GAP == 0){
          LOG_INFO_T("[D] decoded %d frames.", ffio->frameSeq);
        }
        ffio->frameSeq  += 1;
        ffio->frame.type = FFIO_FRAME_TYPE_RGB;
        ffio->frame.err  = FFIO_ERROR_SUCCESS;
        return &(ffio->frame);
      } else if (recv_ret == AVERROR_EOF){
        goto ENDPOINT_DECODE_EOF;
      } else if (recv_ret == AVERROR(EAGAIN)){
        if(send_ret == AVERROR(EAGAIN)){
          LOG_WARNING_T("[D] both send_packet and recv_frame get AVERROR(EAGAIN).");
          usleep(10000);
          goto ENDPOINT_RESEND_PACKET;
        } else { continue; }
      } else {
        LOG_ERROR("[D] error occurred while avcodec_receive_frame: %d - %s.",
                  recv_ret, av_err2str(recv_ret));
        ffio->frame.err = FFIO_ERROR_RECV_FROM_CODEC; goto ENDPOINT_DECODE_ERROR;
      }

    } // if this is a packet of the desired stream.

  }

  if(read_ret == AVERROR_EOF){
    avcodec_send_packet(ffio->avCodecContext, NULL);
ENDPOINT_DECODE_EOF:
    LOG_INFO_T("[D] reached the end of this stream.");
    ffio->ffioState  = FFIO_STATE_END;
    ffio->frame.type = FFIO_FRAME_TYPE_EOF;
    ffio->frame.err  = FFIO_ERROR_STREAM_EOF;
    ffio->frame.data = NULL; ffio->frame.sei_msg = NULL;
    return &(ffio->frame);
  }else{
    LOG_ERROR("[D] error occurred while av_read_frame: %d - %s.", read_ret, av_err2str(read_ret));
    ffio->frame.err  = FFIO_ERROR_READ_OR_WRITE_TARGET;
ENDPOINT_DECODE_ERROR:
    ffio->frame.type = FFIO_FRAME_TYPE_ERROR;
    ffio->frame.data = NULL; ffio->frame.sei_msg = NULL;
    return &(ffio->frame);
  }

}
static FFIOError encodeOneFrameFromRGBFrame(FFIO* ffio, unsigned char* rgbBytes,
                                            const char* seiMsg, uint32_t seiMsgSize){
  /*
   * Refers to the official FFmpeg examples: transcoding.c and vaapi_transcode.c
   *
   * Description:
   *   Only ensures that one frame is successfully sent to the codec, then attempt to fetch
   * the encoded avPacket. Returns true even if the attempt fails.
   *   This means the avPacket might actually be written to the target file on next or later
   * call to `encodeOneFrameFromRGBFrame()`.
   *
   * Returns:
   *   success - FFIO_ERROR_SUCCESS - 0
   *
   * Process:
   **              send            recv             write
   **     AVFrame  ---->  AVCodec  ---->  AVPacket  ---->  AVFormat
   **   (raw data)       [encoding]    (encoded data)
   */
  if(ffio->ffioState == FFIO_STATE_READY){ ffio->ffioState = FFIO_STATE_RUNNING; }
  if(ffio->ffioState != FFIO_STATE_RUNNING){
    LOG_INFO("[E] ffio not ready.");
    return FFIO_ERROR_FFIO_NOT_AVAILABLE;
  }

  AVFrame  *srcFrame = convertFromRGBFrame(ffio, rgbBytes);
  AVStream *stream   = ffio->avFormatContext->streams[ffio->videoStreamIndex];
  srcFrame->pts      = ffio->get_current_pts(ffio);
  if(srcFrame->pts == -1){
    // skip this frame, maybe get frame too fast.
    return 0;
  }

  int write_ret, send_ret, recv_ret;
  while(true) {
    send_ret = avcodec_send_frame(ffio->avCodecContext, srcFrame);
    if(send_ret<0 && send_ret!=AVERROR(EAGAIN)){ break; }

    av_packet_unref(ffio->avPacket);
    recv_ret = avcodec_receive_packet(ffio->avCodecContext, ffio->avPacket);
    if(recv_ret == AVERROR_EOF){
      goto ENDPOINT_ENCODE_EOF;
    } else if (recv_ret==AVERROR(EAGAIN)){
      if(send_ret==AVERROR(EAGAIN)){
        LOG_WARNING("both receive_packet and send_frame get AVERROR(EAGAIN), so resend frame to codec.");
        usleep(10000);
        continue;
      }else{
        /*
         * Send a frame to codec successfully(send_ret == 0),
         * but this frame not encoded yet, will receive it again when next time
         * this encodeOneFrameFromRGBFrame() called.
         */
        return 0;
      }
    } else if (recv_ret<0){
      LOG_ERROR("[E] error occurred while recv packet from codec: %d - %s.", recv_ret, av_err2str(recv_ret));
      return FFIO_ERROR_RECV_FROM_CODEC;
    }

    // recv_ret == 0 and send_ret == 0
    ffio->avPacket->stream_index = ffio->videoStreamIndex;
    av_packet_rescale_ts(ffio->avPacket,
                         ffio->avCodecContext->time_base,
                         stream->time_base);

    if(seiMsg!=NULL){
      extend_sei_to_av_packet(ffio->codecParams->use_h264_AnnexB_sei,
                              ffio->avPacket,
                              ffio->codecParams->sei_uuid, seiMsg, seiMsgSize);
    }

    // Our program runs synchronously,
    // so using av_write_frame() is sufficient, compared to av_interleaved_write_frame()
    write_ret = av_write_frame(ffio->avFormatContext, ffio->avPacket);
    if(write_ret==0){
      if( (ffio->frameSeq)%LOG_PRINT_FRAME_GAP == 0){
        LOG_INFO_T("[E] encoded %d frames.", ffio->frameSeq);
      }
      ffio->frameSeq += 1;
#ifdef DEBUG
      LOG_DEBUG("[E][%d] (frame){pts: %d, dts: %d} (packet){pts: %d, dts: %d}.",
                ffio->frameSeq,
                (int)srcFrame->pts,       (int)srcFrame->pkt_dts,
                (int)ffio->avPacket->pts, (int)ffio->avPacket->dts);
#endif
      return FFIO_ERROR_SUCCESS;
    } else {
      LOG_ERROR("[E] error occurred while av_interleaved_write_frame: %d - %s.", write_ret, av_err2str(write_ret));
      return FFIO_ERROR_READ_OR_WRITE_TARGET;
    }
  }

  if(send_ret == AVERROR_EOF){
ENDPOINT_ENCODE_EOF:
    LOG_INFO("[E] reached the end of this stream.");
    ffio->ffioState = FFIO_STATE_END;
    return FFIO_ERROR_STREAM_EOF;
  }else{
    LOG_ERROR("[E] error while send frame to codec: %d - %s.", send_ret, av_err2str(send_ret));
    return FFIO_ERROR_SEND_TO_CODEC;
  }
}

FFIO *newFFIO(){
  FFIO* ffio = (FFIO*)malloc(sizeof(FFIO));
  LOG_INFO_T(" allocate a new FFIO.");
  ffio_reset(ffio);
  return ffio;
}

/**
 *  Returns:
 *    success - 0
 */
int initFFIO(
    FFIO* ffio, FFIOMode mode, const char* streamUrl,
    bool hw_enabled, const char* hw_device,
    bool enableShm,  const char* shmName, int shmSize, int shmOffset,
    CodecParams* codecParams
){
  int ret;

  if(ffio==NULL || ffio->ffioState != FFIO_STATE_INIT){ return FFIO_ERROR_FFIO_NOT_AVAILABLE; }

  ffio->ffioMode    = mode;
  ffio->hw_enabled  = hw_enabled;
  ffio_init_check_and_set_codec_params(ffio, codecParams, hw_device);
  snprintf(ffio->targetUrl, sizeof(ffio->targetUrl), "%s", streamUrl);
  LOG_INFO_T("[%s][init] codec params is checked. Target: %s.", get_ffioMode(ffio), streamUrl);

  ret = ffio->ffioMode == FFIO_MODE_DECODE ?
          ffio_init_decode_avformat(ffio) : ffio_init_encode_avformat(ffio);
  if(ret != 0){ finalizeFFIO(ffio); return ret; }

  ret = ffio->ffioMode == FFIO_MODE_DECODE ?
          ffio_init_decode_avcodec(ffio, hw_device) : ffio_init_encode_avcodec(ffio, hw_device);
  if(ret != 0){ finalizeFFIO(ffio); return ret; }

  ret = ffio->ffioMode == FFIO_MODE_DECODE ? 0 : ffio_init_encode_create_stream(ffio);
  if(ret != 0){ finalizeFFIO(ffio); return ret; }

  ret = ffio_init_packet_and_frame(ffio);
  if(ret != 0){ finalizeFFIO(ffio); return ret; }
  LOG_INFO("[%s][init] succeeded to alloc AVFrame and AVPacket.", get_ffioMode(ffio));

  ret = ffio_init_memory_for_rgb_bytes(ffio, mode, enableShm, shmName, shmSize, shmOffset);
  if(ret != 0){ finalizeFFIO(ffio); return ret; }
  LOG_INFO("[%s][init] succeeded to bind rawFrame and rawFrameShm.", get_ffioMode(ffio));

  ret = ffio_init_sws_context(ffio);
  if(ret != 0){ finalizeFFIO(ffio); return ret; }

  ffio->ffioState = FFIO_STATE_READY;
  LOG_INFO_T("[%s][init] succeeded to initialize ffio.", get_ffioMode(ffio));
  return 0;
}

FFIO* finalizeFFIO(FFIO* ffio){

  if(ffio->swsContext)     { sws_freeContext(ffio->swsContext);                }
  if(ffio->avFrame)        { av_frame_free( &(ffio->avFrame) );                }
  if(ffio->hwFrame)        { av_frame_free( &(ffio->hwFrame) );                }
  if(ffio->rgbFrame)       { av_frame_free( &(ffio->rgbFrame) );               }
  if(ffio->avPacket)       { av_packet_free( &(ffio->avPacket) );              }
  if(ffio->avCodecContext) { avcodec_free_context( &(ffio->avCodecContext) );  }
  if(ffio->hwContext)      { av_buffer_unref(&(ffio->hwContext));              }

  if(ffio->avFormatContext){
    if(ffio->ffioMode == FFIO_MODE_DECODE){
      avformat_close_input( &(ffio->avFormatContext) );
    }else{
      LOG_INFO("[E] write trailer to target: %s.", ffio->targetUrl);
      av_write_trailer(ffio->avFormatContext);
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

  ffio_reset(ffio);
  ffio->ffioState = FFIO_STATE_CLOSED;
  LOG_INFO_T("[%s] finished to free ffio context.", get_ffioMode(ffio));
  return ffio;
}

/*
 * Returns:
 *   success - 0
 *
 * Decode:
 **               read             send            recv
 **     AVFormat  ---->  AVPacket  ---->  AVCodec  ---->  AVFrame
 **                   (encoded data)     [decoding]    (decoded data)
 *
 * Encode:
 **              send            recv             write
 **     AVFrame  ---->  AVCodec  ---->  AVPacket  ---->  AVFormat
 **   (raw data)       [encoding]    (encoded data)
 *
 */
FFIOFrame* decodeOneFrame(FFIO* ffio, const char* sei_filter){
  FFIOFrame* frame = decodeOneFrameToAVFrame(ffio);
  if(frame->type == FFIO_FRAME_TYPE_RGB){
    memcpy(ffio->rawFrame, ffio->rgbFrame->data[0], ffio->imageByteSize);
    ffio->frame.data    = ffio->rawFrame;
    ffio->frame.sei_msg =
        get_sei_from_av_frame(ffio->avFrame, ffio->sei_buf, sei_filter) ?
        (char*)ffio->sei_buf : NULL;
  }
  return frame;
}
FFIOFrame* decodeOneFrameToShm(FFIO* ffio, int shmOffset, const char* sei_filter){
  if(!ffio->shmEnabled){
    ffio->frame.type = FFIO_FRAME_TYPE_ERROR;
    ffio->frame.err  = FFIO_ERROR_SHM_FAILURE;
    ffio->frame.data = NULL; ffio->frame.sei_msg = NULL;
    return &(ffio->frame);
  }

  FFIOFrame* frame = decodeOneFrameToAVFrame(ffio);
  if(frame->type == FFIO_FRAME_TYPE_RGB){
    memcpy(ffio->rawFrameShm + shmOffset, ffio->rgbFrame->data[0], ffio->imageByteSize);
    ffio->frame.data    = ffio->rawFrameShm + shmOffset;
    ffio->frame.sei_msg =
        get_sei_from_av_frame(ffio->avFrame, ffio->sei_buf, sei_filter) ?
        (char*)ffio->sei_buf : NULL;
  }

  return frame;
}
int encodeOneFrame(FFIO* ffio, unsigned char* rgbBytes,
                   const char* seiMsg, uint32_t seiMsgSize){
  return encodeOneFrameFromRGBFrame(ffio, rgbBytes, seiMsg, seiMsgSize);
}
bool encodeOneFrameFromShm(FFIO* ffio, int shmOffset,
                           const char* seiMsg, uint32_t seiMsgSize){
  int ret = encodeOneFrameFromRGBFrame(ffio, ffio->rawFrameShm + shmOffset, seiMsg, seiMsgSize);
  return ret == 0 ? true : false;
}
