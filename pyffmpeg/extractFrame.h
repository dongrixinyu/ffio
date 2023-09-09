#include <stdio.h>
#include <unistd.h>
// #include "libavutil/avconfig.h"

#ifndef BYTE
#define BYTE unsigned char
#endif

#define OUTPUT_COLOR_BITS 24
#define OUTPUT_COLOR_BYTES 3  // color space number
#define OUTPUT_ALIGN_BYTES 1  // byte number per color
#define ALIGN_ROUND(a, b) ((a + b - 1) & ~(b - 1))
#define OUTPUT_PIX_FMT AV_PIX_FMT_RGB24

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
#include "libavutil/opt.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
// #include "libswresample/swresample.h"

int ConvertYUV2RBG(AVFrame *inputFrame, AVFrame *RGBFrame, unsigned char *outputImage,
                   AVCodecContext *videoCodecContext, int frameNum);

typedef struct StreamObj {
    char streamPath[300]; // the path of mp4 or rtmp, rtsp
    int streamID;         // which stream index to parse in the video
    int streamWidth;
    int streamHeight;
    int imageSize;

    int timebaseNum;  // to compute the fps of the stream, duration / Den
    int timebaseDen;

    AVFormatContext *videoFormatContext;
    AVCodecContext *videoCodecContext;
    AVPacket *videoPacket;
    AVFrame *videoFrame;
    AVFrame *videoRGBFrame;
    AVCodec *videoCodec;

    int frameNum;  // the current number of the video stream
    int streamEnd; // has already to the end of the stream

    unsigned char *outputImage; // the extracted frame

} StreamObj;

StreamObj *newStreamObj();

int Init(StreamObj *streamObj, const char *streamPath);

/** decode one frame from the online video
 *
 * 1 means failed, 0 means success.
 * the result is stored at streamObj->outputImage
 */
int decodeOneFrame(StreamObj *streamObj);

int decodeFrame(StreamObj *streamObj);

int save_rgb_to_file(StreamObj *streamObj, int frame_num);
