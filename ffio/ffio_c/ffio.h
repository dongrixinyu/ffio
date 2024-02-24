//
// Created by koisi on 2024/2/22.
//

#ifndef FFIO_C_FFIO_H
#define FFIO_C_FFIO_H

#include "ffio_util.h"

#define MAX_URL_LENGTH   256
#define FFIO_COLOR_DEPTH 3

typedef enum FFIOMode {
  FFIO_MODE_DECODE = 0,
  FFIO_MODE_ENCODE
} FFIOMode;

typedef enum FFIOState {
  FFIO_STATE_INIT = 0,
  FFIO_STATE_READY,
  FFIO_STATE_RUNNING,
  FFIO_STATE_END,
  FFIO_STATE_CLOSED
} FFIOState;

typedef enum FFIOError {
  FFIO_ERROR_FFIO_NOT_AVAILABLE = -100,
  FFIO_ERROR_RECV_FROM_CODEC,
  FFIO_ERROR_SEND_TO_CODEC,
  FFIO_ERROR_READ_OR_WRITE_TARGET,
  FFIO_ERROR_STREAM_ENDING,
  FFIO_ERROR_AVFRAME_ALLOCATION,
  FFIO_ERROR_AVFORMAT_FAILURE,
  FFIO_ERROR_AVCODEC_FAILURE,
  FFIO_ERROR_SHM_FAILURE,
  FFIO_ERROR_SWS_FAILURE,
  FFIO_ERROR_HARDWARE_ACCELERATION
} FFIOError;

typedef struct CodecParams {
  int  width;
  int  height;
  int  bitrate;
  int  fps;
  int  gop;
  int  b_frames;
  char profile[24];
  char preset[24];
  char tune[24];
  char pix_fmt[24];
  char format[24];
} CodecParams;

typedef struct FFIO{
  FFIOState ffioState;                       // to indicate that if the stream has been opened successfully
  FFIOMode  ffioMode;                        // encode or decode
  int       frameSeq;                        // the sequence number of the current video frame
  bool      hw_enabled;                      // indicate if using the hardware acceleration

  bool      shmEnabled;
  int       shmFd;
  int       shmSize;

  int       videoStreamIndex;                // which stream index to parse in the video
  int       imageWidth;
  int       imageHeight;
  int       imageByteSize;

  char      targetUrl[MAX_URL_LENGTH];       // the path of mp4 or rtmp, rtsp

  AVFormatContext     *avFormatContext;
  AVCodecContext      *avCodecContext;
  AVCodec             *avCodec;
  AVPacket            *avPacket;
  AVFrame             *avFrame;              // Decode:  codec    -> avFrame -> (hw_enabled? hwFrame) -> rgbFrame
  AVFrame             *hwFrame;              // Encode:  rgbFrame -> avFrame -> (hw_enabled? hwFrame) -> codec
  AVFrame             *rgbFrame;
  struct SwsContext   *swsContext;

  unsigned char       *rawFrame;
  unsigned char       *rawFrameShm;

  AVBufferRef         *hwContext;
  enum AVPixelFormat   hw_pix_fmt;

  CodecParams         *codecParams;
} FFIO;

// Functions of FFIO lifecycle.
FFIO* newFFIO();
int initFFIO(
    FFIO* ffio, FFIOMode mode, const char* streamUrl,
    bool hw_enabled, const char* hw_device,
    bool enableShm,  const char* shmName, int shmSize, int shmOffset,
    CodecParams* codecParams
);
FFIO* finalizeFFIO(FFIO* ffio);

/** decode one frame from the online video
 *
 * 1 means failed, 0 means success.
 * the result is stored at ffio->rawFrame or rawFrameShm.
 */
int decodeOneFrame(FFIO* ffio);
int decodeOneFrameToShm(FFIO* ffio, int shmOffset);

int encodeOneFrame(FFIO* ffio, unsigned char *RGBImage);
bool encodeOneFrameFromShm(FFIO* ffio, int shmOffset);

#endif //FFIO_C_FFIO_H
