#include "ffio_util.h"

// this param is set to return a timeout error when waiting for more than 5 seconds.
#define WAIT_FOR_STREAM_TIMEOUT 5

#define OUTPUT_COLOR_BITS 24
#define OUTPUT_COLOR_BYTES 3 // color space number
#define OUTPUT_ALIGN_BYTES 1 // byte number per color
#define ALIGN_ROUND(a, b) ((a + b - 1) & ~(b - 1))
#define OUTPUT_RGB_PIX_FMT AV_PIX_FMT_RGB24
#define INPUT_RGB_PIX_FMT AV_PIX_FMT_YUV420P

typedef signed long int int64_tt;

#ifndef BYTE
#define BYTE unsigned char
#endif


int convertRGB2YUV(struct SwsContext *RGB2YUVContext, AVCodecContext *videoCodecContext,
                   AVFrame *inputFrame, AVFrame *RGBFrame,
                   unsigned char *RGBImage, int RGBImageSize);

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
    AVFrame *videoRGBFrame;
    AVCodec *videoCodec;
    struct SwsContext *swsContext;

    int frameNum;  // the current number of the video stream
    int streamEnd; // has already to the end of the stream

    unsigned char *extractedFrame; // the extracted frame

    struct Clicker *clicker;

} InputStreamObj;

InputStreamObj *newInputStreamObj();

int initializeInputStream(InputStreamObj *inputStreamObj, const char *streamPath);

InputStreamObj *finalizeInputStream(InputStreamObj *inputStreamObj);

/** decode one frame from the online video
 *
 * 1 means failed, 0 means success.
 * the result is stored at inputStreamObj->extractedFrame
 */
int decodeOneFrame(InputStreamObj *inputStreamObj);

int save_rgb_to_file(InputStreamObj *inputStreamObj, int frame_num);

int convertRGB2YUV(struct SwsContext *RGB2YUVContext, AVCodecContext *videoCodecContext,
                   AVFrame *inputFrame, AVFrame *RGBFrame,
                   unsigned char *RGBImage, int RGBImageSize);

typedef struct OutputStreamObj
{
    int outputframeNum;
    // output stream info
    int outputStreamStateFlag;  // to indicate that if the stream has been opened successfully
    char outputStreamPath[300]; // the path of mp4 or rtmp, rtsp

    int outputVideoStreamID; // which stream index to parse in the video, default 0.
    int outputVideoStreamWidth;
    int outputVideoStreamHeight;

    int outputvideoFramerateNum; // to compute the fps of the stream, duration / Den
    int outputvideoFramerateDen;

    AVFormatContext *outputFormatContext;
    AVCodecContext *videoEncoderContext;
    AVCodec *videoEncoder;
    AVFrame *videoEncoderFrame;
    AVFrame *videoRGBFrame;
    AVPacket *videoPacketOut;
    AVStream *outputVideoStream;

    AVRational outputVideoTimebase; // for computing pts and dts
    // AVRational inputVideoTimebase;  // for computing pts and dts for output

    struct SwsContext *RGB2YUVContext;

} OutputStreamObj;

OutputStreamObj *newOutputStreamObj();

int initializeOutputStream(
    OutputStreamObj *outputStreamObj,
    const char *outputStreamPath,
    int framerateNum, int framerateDen, int frameWidth, int frameHeight);

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
