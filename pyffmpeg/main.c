#include <stdio.h>
#include <time.h>
#include <unistd.h>
// #include "libavutil/avconfig.h"

#ifndef BYTE
#define BYTE unsigned char
#endif

// #define AV_NUM_DATA_POINTERS 8 // frame data array number
#define OUTPUT_COLOR_BITS 24
#define OUTPUT_COLOR_BYTES 3 // color space number
#define OUTPUT_ALIGN_BYTES 1 // byte number per color
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
// #include "libswscale/swscale_internal.h"
// #include "libswresample/swresample.h"


int save_rgb_to_file(AVFrame *frame, int num)
{
    // concatenate file name
    char pic_name[200] = {0};
    sprintf(pic_name, "./test_images/rgba_8888_%d.yuv", num);

    // write to file
    FILE *fp = NULL;
    fp = fopen(pic_name, "wb+");

    // printf("frame num: %I4d. pix: %I4d, %I4d, %I4d,\t\t%I4d, %I4d, %I4d\n",
    //        num, frame->data[0][0], frame->data[0][1], frame->data[0][2],
    //        frame->data[0][3000], frame->data[0][3001], frame->data[0][3002]);
    // printf("frame linesize: %d, height: %d\n", frame->linesize[0], frame->height);
    fwrite(frame->data[0], 1, frame->linesize[0] * frame->height, fp);
    fclose(fp);

    return 0;
}


int ConvertYUV2RBG(AVFrame *inputFrame, unsigned char *outputImage,
                   AVCodecContext *videoCodecContext, int frame_num)
{
    int ret;
    AVFrame *frame = av_frame_alloc();

    struct SwsContext *swsContextObj = NULL;
    if (swsContextObj == NULL)
    {
        // replace SWS_FAST_BILINEAR with other options SWS_BICUBIC
        swsContextObj = sws_getCachedContext(
            swsContextObj,
            videoCodecContext->width, videoCodecContext->height, videoCodecContext->pix_fmt,
            videoCodecContext->width, videoCodecContext->height, OUTPUT_PIX_FMT,
            SWS_BICUBIC, NULL, NULL, NULL);
        if (swsContextObj == NULL)
            return 0;
    }

    frame->width = videoCodecContext->width;
    frame->height = videoCodecContext->height;
    frame->format = OUTPUT_PIX_FMT;


    ret = av_frame_get_buffer(frame, 1);
    if (ret != 0) {
        printf("no memory for frame data buffer.\n", NULL);
    }

    ret = sws_scale(
        swsContextObj,
        (const uint8_t *const)inputFrame->data, inputFrame->linesize, 0, inputFrame->height,
        frame->data, frame->linesize);
    if (ret < 0)
    {
        // printf("Error convert video: %d\n", ret);
        return 0;
    }

    save_rgb_to_file(frame, frame_num);
    printf("Successfully read one frame %d!\n", frame_num);
    av_frame_unref(frame);

    return 1;
}


int main()
{
    int err;
    int ret;
    int height, width;
    int time_base_num, time_base_den;

    const char *sourcePath = "rtmp://live.uavcmlc.com:1935/live/DEV02001044?token=003caf430efb";

    AVFormatContext *sourceContext = avformat_alloc_context();
    ret = avformat_open_input(&sourceContext, sourcePath, NULL, NULL);
    if (ret < 0)
    {
        printf("can not open stream path `%s`\n", sourcePath);
    }
    else
    {
        // printf("open stream path `%s` successfully.\n", sourcePath);
        av_log(NULL, AV_LOG_INFO, "open stream path `%s` successfully.\n", sourcePath);
    }

    ret = avformat_find_stream_info(sourceContext, NULL);

    AVCodec *sourceVideoCodec = NULL;
    int videoStreamID = av_find_best_stream(
        sourceContext, AVMEDIA_TYPE_VIDEO, -1, -1, &sourceVideoCodec, 0);

    AVCodecContext *videoCodecContext = avcodec_alloc_context3(NULL);
    ret = avcodec_parameters_to_context(videoCodecContext, sourceContext->streams[videoStreamID]->codecpar);

    sourceVideoCodec = avcodec_find_decoder(videoCodecContext->codec_id);

    ret = avcodec_open2(videoCodecContext, sourceVideoCodec, NULL);

    av_dump_format(sourceContext, videoStreamID, sourcePath, 0);

    AVPacket *videoPacket = av_packet_alloc();
    AVFrame *videoFrame = av_frame_alloc();
    int frame_num = 0;
    int streamEnd = 0;
    unsigned char *outputImage = (unsigned char *)malloc(videoFrame->width * videoFrame->height * 3 * 2);

    clock_t start_time, end_time;
    double total_cost_time;
    start_time = clock();

    for (;;)
    {
        if (streamEnd == 1)
        {
            printf("# to the end of the video stream.\n");
            break;
        }

        ret = av_read_frame(sourceContext, videoPacket); // 读取一个 packet

        if (videoPacket->stream_index != videoStreamID)
        {
            // 仅处理该条视频流的包
            av_packet_unref(videoPacket);
            printf("# didnt accept audio packet.\n");
            continue;
        }

        // printf("AVERROR_EOF: %d, %d\n", AVERROR_EOF, ret);
        if (AVERROR_EOF == ret)
        {
            avcodec_send_packet(videoCodecContext, videoPacket);
            av_packet_unref(videoPacket);

            for (;;)
            {
                ret = avcodec_receive_frame(videoCodecContext, videoFrame);

                if (ret == AVERROR(EAGAIN))
                {
                    break; // need more AVPacket
                }
                else if (ret == AVERROR_EOF)
                {
                    // to the end of the stream
                    streamEnd = 1;
                    break;
                }
                else if (ret >= 0)
                {
                    // has to the end
                    // decode one YUV frame, convert it to rgb format
                    ret = ConvertYUV2RBG(videoFrame, outputImage,
                                         videoCodecContext, frame_num);
                    // end_time = clock();
                    // printf("read one frame cost time=%f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);
                    frame_num += 1;
                    streamEnd = 1; //  to the end
                }
                else
                {
                    printf("other error when generating one frame.\n");

                    return ret;
                }
            }
        }
        else if (ret == 0)
        {
            while (1)
            {
                ret = avcodec_send_packet(videoCodecContext, videoPacket);
                if (ret == AVERROR(EAGAIN))
                {
                    printf("Receive_frame and send_packet both returned EAGAIN, which is an API violation.\n");
                    sleep(0.02); // sleep for about a duration of one frame
                }
                else if (ret == 0)
                {
                    break;
                }
                else if (ret < 0)
                {
                    printf("an unknown error.\n");
                    break;
                }
            }

            av_packet_unref(videoPacket);

            for (;;)
            {
                ret = avcodec_receive_frame(videoCodecContext, videoFrame);

                if (ret == AVERROR(EAGAIN))
                {
                    break; // need more AVPacket
                }
                else if (ret == AVERROR_EOF)
                {

                    streamEnd = 1; // to the end of the video stream
                    break;
                }
                else if (ret >= 0)
                {
                    // decode one YUV frame, convert it to rgb format
                    ret = ConvertYUV2RBG(videoFrame, outputImage,
                                         videoCodecContext, frame_num);
                    // end_time = clock();
                    // printf("read one frame cost time=%f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);
                    frame_num += 1;
                    // if (frame_num > 200){
                    //     return 10;
                    // }
                    // start_time = clock();
                }
                else
                {
                    printf("other error when generating one frame.\n");

                    return ret;
                }
            }
        } else {
            printf("read error code %d \n", ret);
            return ENOMEM;
        }
    }

    printf("# %d, %d\n", ret, AVERROR_EOF);

    return 0;
}
