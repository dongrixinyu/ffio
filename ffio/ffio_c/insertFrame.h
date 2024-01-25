#include "ffio_util.h"

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

typedef struct OutputStreamObj
{
    int outputframeNum;
    // output stream info
    int outputStreamStateFlag;  // to indicate that if the stream has been opened successfully
    char outputStreamPath[300]; // the path of mp4 or rtmp, rtsp

    int outputVideoStreamID; // which stream index to parse in the video, default 0.
    int outputVideoStreamWidth;
    int outputVideoStreamHeight;
    int sizeOfImageBytes;

    int outputvideoFramerateNum; // to compute the fps of the stream, duration / Den
    int outputvideoFramerateDen;

    int hw_flag; // indicate if using the hardware accelaration

    AVBufferRef *hw_device_ctx;
    // enum AVPixelFormat hw_pix_fmt;

    AVFormatContext *outputFormatContext;
    AVCodecContext *videoEncoderContext;
    AVCodec *videoEncoder;
    AVFrame *videoEncoderFrame;    // cpu frame
    AVFrame *videoRGBFrame;        // cpu rgb frame
    AVFrame *videoHWEncoderFrame;  // gpu hw frame
    AVPacket *videoPacketOut;
    AVStream *outputVideoStream;

    struct SwsContext *RGB2YUVContext;

    unsigned char *shmForFrame;
    bool shmEnabled;
    int  shmFd;
    int  shmSize;

} OutputStreamObj;

OutputStreamObj *newOutputStreamObj();

int convertRGB2YUV(
    OutputStreamObj *outputStreamObj,
    unsigned char *RGBImage, int RGBImageSize);

int convertRGB2NV12(
    OutputStreamObj *outputStreamObj,
    unsigned char *RGBImage, int RGBImageSize);

/**
 *  initialize video encoder context and format context info
 *
 *  ret: int
 *      0 means successfully start a video stream.
 *      1 means can not parse the right output format.
 *      2 means can not find the video encoder `libx264` default.
 *      3 means failed to run avcodec_open2.
 *      4 means failed to run sws_getCachedContext.
 *      5 means av_frame_get_buffer failed.
 *      6 means failed to allocate memory for `avformat_alloc_output_context2`.
 *      7 means avcodec_new_stream failed.
 *      8 means avcodec_copy_context failed.
 *      9 means avio_open2 failed.
 *      10 means avformat_write_header failed
 */
int initializeOutputStream(
    OutputStreamObj *outputStreamObj,
    const char *outputStreamPath,
    int framerateNum, int framerateDen, int frameWidth, int frameHeight,
    const char *preset, int hw_flag,
    const bool enableShm, const char *shmName, const int shmSize, const int shmOffset);

static int set_hwframe_ctx(
    AVCodecContext *ctx, AVBufferRef *hw_device_ctx, int width, int height);

/**
 * to finalize the output stream context
 *
 * ret: OutputStreamObj * , which need to be deleted.
 *
 */
OutputStreamObj *finalizeOutputStream(OutputStreamObj *outputStreamObj);

/**
 * encode one frame
 *
 * params:
 *     end_of_stream: (int) indicate if the frame is the last one.
 *
 */
int encodeOneFrame(
    OutputStreamObj *outputStreamObj,
    unsigned char *RGBImage, int RGBImageSize, int end_of_stream);
