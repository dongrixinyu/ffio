// #include "libavutil/avconfig.h"

#include "ffio_util.h"

#ifndef BYTE
#define BYTE unsigned char
#endif

// this param is set to return a timeout error when waiting for more than 5 seconds.
#define WAIT_FOR_STREAM_TIMEOUT 5

// write log to file in every PRINT_FRAME_GAP frames to make sure correctness.
#define PRINT_FRAME_GAP 200

#define OUTPUT_COLOR_BITS 24
#define OUTPUT_COLOR_BYTES 3 // color space number
#define OUTPUT_ALIGN_BYTES 1 // byte number per color
#define ALIGN_ROUND(a, b) ((a + b - 1) & ~(b - 1))
#define OUTPUT_RGB_PIX_FMT AV_PIX_FMT_RGB24
#define INPUT_RGB_PIX_FMT AV_PIX_FMT_YUV420P


typedef signed long int int64_tt;

typedef struct InputStreamObj
{
    int inputStreamStateFlag;  // to indicate that if the stream has been opened successfully
    char inputStreamPath[300]; // the path of mp4 or rtmp, rtsp
    int inputVideoStreamID;    // which stream index to parse in the video
    int inputVideoStreamWidth;
    int inputVideoStreamHeight;
    int imageSize;

    int inputFramerateNum; // to compute the fps of the stream, duration / Den
    int inputFramerateDen;

    AVFormatContext *inputFormatContext;
    AVCodecContext *videoCodecContext;
    AVPacket *videoPacket;
    AVFrame *videoFrame;
    AVFrame *videoTmpFrame;
    AVFrame *videoSWFrame;
    AVFrame *videoRGBFrame;
    AVCodec *videoCodec;
    struct SwsContext *swsContext;
    AVBufferRef *hw_device_ctx;
    enum AVPixelFormat hw_pix_fmt;

    int hw_flag;   // indicate if using the hardware accelaration
    int frameNum;  // the current number of the video stream
    int streamEnd; // has already to the end of the stream

    unsigned char *extractedFrame; // the extracted frame

    unsigned char *extractedFrameShm;
    bool shmEnabled;
    int  shmFd;
    int  shmSize;

    struct Clicker *clicker;

} InputStreamObj;

int convertYUV2RGB(
    InputStreamObj *inputStreamObj);

InputStreamObj *newInputStreamObj();

int initializeInputStream(
    InputStreamObj *inputStreamObj, const char *streamPath,
    int hw_flag, const char *hw_device,
    const bool enableShm, const char *shmName, const int shmSize, const int shmOffset);

InputStreamObj *finalizeInputStream(InputStreamObj *inputStreamObj);

/** decode one frame from the online video
 *
 * 1 means failed, 0 means success.
 * the result is stored at inputStreamObj->extractedFrame
 */
int decodeOneFrame(InputStreamObj *inputStreamObj);
int decodeOneFrameToShm(InputStreamObj *inputStreamObj, int shmOffset);
int _decodeOneFrame(InputStreamObj *inputStreamObj);

int save_rgb_to_file(InputStreamObj *inputStreamObj, int frame_num);

static enum AVPixelFormat get_hw_format(
    AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts);
