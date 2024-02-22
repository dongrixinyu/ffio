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
  FFIO_STATE_NOT_READY = 0,
  FFIO_STATE_READY,
  FFIO_STATE_RUNNING,
  FFIO_STATE_END,
  FFIO_STATE_CLOSED
} FFIOState;

typedef struct FFIO{
  char      targetUrl[MAX_URL_LENGTH];       // the path of mp4 or rtmp, rtsp
  FFIOState ffioState;                       // to indicate that if the stream has been opened successfully
  FFIOMode  ffioMode;                        // encode or decode
  int       frameSeq;                        // the sequence number of the current video frame

  AVFormatContext   *avFormatContext;
  AVCodecContext    *avCodecContext;
  AVCodec           *avCodec;
  AVPacket          *avPacket;
  AVFrame           *yuvFrame;
  AVFrame           *rgbFrame;
  struct SwsContext *swsContext;
  AVBufferRef       *hwContext;

  int videoStreamIndex;                      // which stream index to parse in the video
  int imageWidth;
  int imageHeight;
  int imageByteSize;

  unsigned char *rawFrame;
  unsigned char *rawFrameShm;
  bool           shmEnabled;
  int            shmFd;
  int            shmSize;

  bool hw_enabled;                           // indicate if using the hardware acceleration
} FFIO;

// Functions of FFIO lifecycle.
FFIO* newFFIO();
int initFFIO(
    FFIO* ffio, FFIOMode mode, const char* streamUrl,
    bool hw_enabled, const char* hw_device,
    bool enableShm,  const char* shmName, int shmSize, int shmOffset
);
FFIO* finalizeFFIO(FFIO* ffio);

/** decode one frame from the online video
 *
 * 1 means failed, 0 means success.
 * the result is stored at ffio->rawFrame or rawFrameShm.
 */
int decodeOneFrame(FFIO* ffio);
int decodeOneFrameToShm(FFIO* ffio, int shmOffset);

#endif //FFIO_C_FFIO_H
