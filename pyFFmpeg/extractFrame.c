#include "extractFrame.h"


StreamObj *newStreamObj() {
    StreamObj *streamObj = NULL;
    streamObj = (StreamObj *)malloc(sizeof(StreamObj));
    if (streamObj == NULL) {
        printf("out of memory. failed to new a StreamObj.\n");
        return NULL;
    }

    // streamObj->streamPath = {0}; // the path of mp4 or rtmp, rtsp
    streamObj->streamID = -1;     // which stream index to parse in the video
    streamObj->streamWidth = 0;
    streamObj->streamHeight = 0;
    streamObj->timebaseNum = 0;   // to compute the fps of the stream, Num / Den
    streamObj->timebaseDen = 0;
    streamObj->framerateNum = 0;
    streamObj->framerateDen = 0;

    streamObj->videoPacket = NULL;
    streamObj->videoFrame = NULL;

    streamObj->frameNum = -1; // the current number of the video stream
    streamObj->streamEnd = 0; // has already to the end of the stream

    streamObj->outputImage = NULL; // the extracted frame

}

int save_rgb_to_file(StreamObj *streamObj, int frame_num)
{
    // concatenate file name
    char pic_name[200] = {0};
    sprintf(pic_name, "./test_images/result_%d.yuv", frame_num);

    // write to file
    FILE *fp = NULL;
    fp = fopen(pic_name, "wb+");

    fwrite(streamObj->outputImage, 1, streamObj->imageSize*3, fp);
    fclose(fp);

    return 0;
}

int ConvertYUV2RBG(AVFrame *inputFrame, AVFrame *RGBFrame, unsigned char *outputImage,
                   AVCodecContext *videoCodecContext, int frameNum)
{
    int ret;
    // AVFrame *frame = av_frame_alloc();

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

    RGBFrame->width = videoCodecContext->width;
    RGBFrame->height = videoCodecContext->height;
    RGBFrame->format = OUTPUT_PIX_FMT;

    ret = av_frame_get_buffer(RGBFrame, 1);
    if (ret != 0)
    {
        printf("no memory for frame data buffer.\n");
    }

    ret = sws_scale(
        swsContextObj,
        (const uint8_t *const)inputFrame->data, inputFrame->linesize, 0, inputFrame->height,
        RGBFrame->data, RGBFrame->linesize);
    if (ret < 0)
    {
        // printf("Error convert video: %d\n", ret);
        return 0;
    }

    // save_rgb_to_file(frame, frame_num);
    printf("Successfully read one frame %d!\n", frameNum);
    // av_frame_unref(RGBFrame);

    return 1;
}

/**
 *  read the video context info
 *
 */
int Init(StreamObj *streamObj, const char *streamPath) {
    int ret;
    sprintf(streamObj->streamPath, "%s", streamPath); // the stream Path to be processed

    streamObj->videoFormatContext = avformat_alloc_context();
    ret = avformat_open_input(&streamObj->videoFormatContext, streamObj->streamPath, NULL, NULL);
    if (ret < 0)
    {
        printf("can not open stream path `%s`\n", streamObj->streamPath);
    }
    else
    {
        // printf("open stream path `%s` successfully.\n", sourcePath);
        av_log(NULL, AV_LOG_INFO, "open stream path `%s` successfully.\n",
               streamObj->streamPath);
    }

    ret = avformat_find_stream_info(streamObj->videoFormatContext, NULL);

    streamObj->videoCodec = NULL;
    int videoStreamID = av_find_best_stream(
        streamObj->videoFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, &streamObj->videoCodec, 0);

    streamObj->videoCodecContext = avcodec_alloc_context3(NULL);
    ret = avcodec_parameters_to_context(
        streamObj->videoCodecContext, streamObj->videoFormatContext->streams[videoStreamID]->codecpar);

    streamObj->videoCodec = avcodec_find_decoder(streamObj->videoCodecContext->codec_id);

    ret = avcodec_open2(streamObj->videoCodecContext, streamObj->videoCodec, NULL);

    // set params for decoder
    streamObj->streamID = videoStreamID;
    streamObj->timebaseNum = streamObj->videoFormatContext->streams[videoStreamID]->time_base.num;
    streamObj->timebaseDen = streamObj->videoFormatContext->streams[videoStreamID]->time_base.den;
    streamObj->streamWidth = streamObj->videoCodecContext->width;
    streamObj->streamHeight = streamObj->videoCodecContext->height;
    streamObj->framerateNum = streamObj->videoFormatContext->streams[videoStreamID]->avg_frame_rate.num;
    streamObj->framerateDen = streamObj->videoFormatContext->streams[videoStreamID]->avg_frame_rate.den;

    // just print the stream info on the screen
    av_dump_format(streamObj->videoFormatContext, videoStreamID, streamObj->streamPath, 0);

    streamObj->videoPacket = av_packet_alloc();
    streamObj->videoFrame = av_frame_alloc();

    streamObj->frameNum = 0;
    streamObj->streamEnd = 0; // 0 means not reaching the end, 1 means the stream is empty.

    int channelNum;
    if (OUTPUT_PIX_FMT == AV_PIX_FMT_RGB24)
    {
        channelNum = 3;
    }
    else if (OUTPUT_PIX_FMT == AV_PIX_FMT_RGBA)
    {
        channelNum = 4;
    }

    streamObj->imageSize = streamObj->streamWidth * streamObj->streamHeight * channelNum;
    streamObj->outputImage = (unsigned char *)malloc(streamObj->imageSize);
    printf("stream imagesize: %d\n", streamObj->imageSize);
    streamObj->videoRGBFrame = av_frame_alloc();

    return 0;
}

int decodeFrame(StreamObj *streamObj) {
    int ret;

    for (;;)
    {
        if (streamObj->streamEnd == 1)
        {
            printf("# to the end of the video stream.\n");
            break;
        }

        ret = av_read_frame(streamObj->videoFormatContext, streamObj->videoPacket); // 读取一个 packet

        if (streamObj->videoPacket->stream_index != streamObj->streamID)
        {
            // 仅处理该条视频流的包
            av_packet_unref(streamObj->videoPacket);
            printf("# didnt accept audio packet.\n");
            continue;
        }

        // printf("AVERROR_EOF: %d, %d\n", AVERROR_EOF, ret);
        if (AVERROR_EOF == ret)
        {
            avcodec_send_packet(streamObj->videoCodecContext, streamObj->videoPacket);
            av_packet_unref(streamObj->videoPacket);

            for (;;)
            {
                ret = avcodec_receive_frame(streamObj->videoCodecContext, streamObj->videoFrame);

                if (ret == AVERROR(EAGAIN))
                {
                    // need more AVPacket
                    break;
                }
                else if (ret == AVERROR_EOF)
                {
                    // to the end of the stream
                    streamObj->streamEnd = 1;
                    break;
                }
                else if (ret >= 0)
                {
                    ret = ConvertYUV2RBG(streamObj->videoFrame, streamObj->videoRGBFrame, streamObj->outputImage,
                                         streamObj->videoCodecContext, streamObj->frameNum);
                    // end_time = clock();
                    // printf("read one frame cost time=%f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);
                    streamObj->frameNum += 1;
                    streamObj->streamEnd = 1; //  to the end
                }
                else
                {
                    printf("other error.\n");

                    return ret;
                }
            }
        }
        else if (ret == 0)
        {
            while (1)
            {
                ret = avcodec_send_packet(streamObj->videoCodecContext, streamObj->videoPacket);
                if (ret == AVERROR(EAGAIN))
                {
                    printf("Receive_frame and send_packet both returned EAGAIN, which is an API violation.\n");
                    sleep(0.04); // sleep for about a duration of one frame
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

            av_packet_unref(streamObj->videoPacket);

            for (;;)
            {
                ret = avcodec_receive_frame(
                    streamObj->videoCodecContext, streamObj->videoFrame);

                if (ret == AVERROR(EAGAIN))
                {
                    // need more AVPacket
                    break;
                }
                else if (ret == AVERROR_EOF)
                {
                    // to the end of the video stream
                    streamObj->streamEnd = 1;
                    break;
                }
                else if (ret >= 0)
                {
                    ret = ConvertYUV2RBG(streamObj->videoFrame, streamObj->videoRGBFrame,
                                         streamObj->outputImage,
                                         streamObj->videoCodecContext, streamObj->frameNum);
                    // end_time = clock();
                    // printf("read one frame cost time=%f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);
                    streamObj->frameNum += 1;
                    streamObj->streamEnd = 1; //  to the end
                }
                else
                {
                    printf("other error.\n");

                    return ret;
                }
            }
        }
    }
    return 1;
}

int decodeOneFrame(StreamObj *streamObj)
{
    int ret;
    streamObj->streamEnd = 0;

    for (;;)
    {
        if (streamObj->streamEnd == 1)
        {
            printf("# to the end of the video stream.\n");
            return 1;
            // break;
        }

        ret = av_read_frame(streamObj->videoFormatContext, streamObj->videoPacket); // 读取一个 packet

        if (streamObj->videoPacket->stream_index != streamObj->streamID)
        {
            // filter unrelated packet
            av_packet_unref(streamObj->videoPacket);
            printf("# Only accept packet from streamID: %d.\n", streamObj->streamID);
            continue;
        }

        if (AVERROR_EOF == ret)
        {
            printf("has already to the end of the file: %d\n", ret);
            avcodec_send_packet(streamObj->videoCodecContext, streamObj->videoPacket);
            av_packet_unref(streamObj->videoPacket);

            for (;;)
            {
                ret = avcodec_receive_frame(streamObj->videoCodecContext, streamObj->videoFrame);

                if (ret == AVERROR(EAGAIN))
                {
                    // need more AVPacket
                    break;
                }
                else if (ret == AVERROR_EOF)
                {
                    // to the end of the stream
                    streamObj->streamEnd = 1;
                    // break;
                    return 1;
                }
                else if (ret >= 0)
                {
                    ret = ConvertYUV2RBG(streamObj->videoFrame, streamObj->videoRGBFrame,
                                         streamObj->outputImage,
                                         streamObj->videoCodecContext, streamObj->frameNum);
                    // end_time = clock();
                    // printf("read one frame cost time=%f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);
                    streamObj->frameNum += 1;
                    streamObj->streamEnd = 1; //  to the end
                    memcpy(streamObj->outputImage, streamObj->videoRGBFrame->data[0], streamObj->imageSize);
                    av_frame_unref(streamObj->videoRGBFrame);
                    return 0;
                }
                else
                {
                    printf("other error.\n");

                    return 1;
                }
            }
        }
        else if (ret == 0)
        {
            while (1)
            {
                ret = avcodec_send_packet(streamObj->videoCodecContext, streamObj->videoPacket);
                if (ret == AVERROR(EAGAIN))
                {
                    printf("Receive_frame and send_packet both returned EAGAIN, which is an API violation.\n");
                    sleep(0.04); // sleep for about a duration of one frame
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

            av_packet_unref(streamObj->videoPacket);

            for (;;)
            {
                ret = avcodec_receive_frame(
                    streamObj->videoCodecContext, streamObj->videoFrame);

                if (ret == AVERROR(EAGAIN))
                {
                    break; // need more AVPacket
                }
                else if (ret == AVERROR_EOF)
                {
                    // to the end of the video stream
                    streamObj->streamEnd = 1;
                    break;
                }
                else if (ret >= 0)
                {
                    ret = ConvertYUV2RBG(streamObj->videoFrame, streamObj->videoRGBFrame, streamObj->outputImage,
                                         streamObj->videoCodecContext, streamObj->frameNum);
                    // end_time = clock();
                    // printf("read one frame cost time=%f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);
                    streamObj->frameNum += 1;
                    streamObj->streamEnd = 1; //  to the end
                    memcpy(streamObj->outputImage, streamObj->videoRGBFrame->data[0], streamObj->imageSize);
                    av_frame_unref(streamObj->videoRGBFrame);
                    return 0;
                }
                else
                {
                    printf("other error.\n");

                    return 1;
                }
            }
        }
    }
    return 1;
}

int main()
{
    int err;
    int ret;

    const char *sourceStreamPath = "rtmp://live.uavcmlc.com:1935/live/DEV02001044?token=003caf430efb";
    // const char *sourcePath = "/home/cuichengyu/ffmpeg/qingdao_city_20min_384p_25fps.mp4";
    clock_t start_time, end_time;

    // build a new struct object
    start_time = clock();
    printf("cur time: %ld\n", start_time);
    StreamObj *curStreamObj = newStreamObj();
    ret = Init(curStreamObj, sourceStreamPath); // initialize a new stream
    end_time = clock();
    printf("cur time: %ld\n", end_time);
    printf("initializing cost time=%f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    start_time = clock();
    ret = decodeOneFrame(curStreamObj);
    end_time = clock();
    printf("read one frame cost time=%f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    ret = save_rgb_to_file(curStreamObj, 0);
    return 0;
    sleep(20);

    start_time = clock();
    ret = decodeOneFrame(curStreamObj);
    end_time = clock();
    printf("read one frame cost time=%f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    ret = save_rgb_to_file(curStreamObj, 1);
    return 0;
}
