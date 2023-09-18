#include "extractFrame.h"

StreamObj *newStreamObj()
{
    StreamObj *streamObj = NULL;
    streamObj = (StreamObj *)malloc(sizeof(StreamObj));
    if (streamObj == NULL)
    {
        return NULL;
    }

    // streamObj->streamPath = {0}; // the path of mp4 or rtmp, rtsp
    streamObj->streamID = -1; // which stream index to parse in the video
    streamObj->streamWidth = 0;
    streamObj->streamHeight = 0;
    streamObj->framerateNum = 0; // to compute the fps of the stream, Num / Den
    streamObj->framerateDen = 0;
    streamObj->imageSize = 0;

    streamObj->videoFormatContext = NULL;
    streamObj->videoCodecContext = NULL;
    streamObj->videoDict = NULL;
    streamObj->videoPacket = NULL;
    streamObj->videoFrame = NULL;
    streamObj->swsContext = NULL;

    streamObj->frameNum = -1; // the current number of the video stream
    streamObj->streamEnd = 0; // has already to the end of the stream

    streamObj->outputImage = NULL; // the extracted frame

    streamObj->clicker = NULL;
}

int save_rgb_to_file(StreamObj *streamObj, int frame_num)
{
    // concatenate file name
    char pic_name[200] = {0};
    sprintf(pic_name, "./test_images/result_%d.yuv", frame_num);

    // write to file
    FILE *fp = NULL;
    fp = fopen(pic_name, "wb+");

    fwrite(streamObj->outputImage, 1, streamObj->imageSize * 3, fp);
    fclose(fp);

    return 0;
}

int ConvertYUV2RBG(
    AVFrame *inputFrame, AVFrame *RGBFrame, struct SwsContext *swsContextObj,
    unsigned char *outputImage, AVCodecContext *videoCodecContext, int frameNum)
{
    int ret;
    // AVFrame *frame = av_frame_alloc();

    // struct SwsContext *swsContextObj = NULL;
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
        av_log(NULL, AV_LOG_INFO, "%s", "no memory for frame data buffer.\n");
    }

    ret = sws_scale(
        swsContextObj,
        (const uint8_t *const)inputFrame->data, inputFrame->linesize, 0, inputFrame->height,
        RGBFrame->data, RGBFrame->linesize);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_INFO, "Error convert video: %d\n", ret);
        return 0;
    }

    // save_rgb_to_file(frame, frame_num);
    // print log once every 20 frames
    if (frameNum % 20 == 0)
    {
        av_log(NULL, AV_LOG_INFO, "Successfully read one frame %d!\n", frameNum);
    }

    // av_frame_unref(RGBFrame);

    return 1;
}

static int interrupt_callback(void *p)
{
    Clicker *cl = (Clicker *)p;
    if (cl->lasttime > 0)
    {
        if (time(NULL) - cl->lasttime > WAIT_FOR_STREAM_TIMEOUT)
        { // 5 seconds waiting for an empty stream
            return 1;
        }
    }

    return 0;
}

void av_log_pyFFmpeg_callback(void *avClass, int level, const char *fmt, va_list vl)
{
    if (level > AV_LOG_INFO)
    { // set level
        return;
    }

    AVBPrint bPrint;
    av_bprint_init(&bPrint, 1024, 10240);
    av_vbprintf(&bPrint, fmt, vl);

    time_t rawtime;
    struct tm *info;
    char buffer[80];
    char fileNameBuffer[9];

    time(&rawtime);

    info = localtime(&rawtime);

    strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", info);
    strftime(fileNameBuffer, 9, "%Y%m%d", info);

    char fileName[200];
    // clog.txt.20230918

    // const char *logDir = "/home/cuichengyu/.cache/";
    const char *homedir;

    if ((homedir = getenv("HOME")) == NULL)
    {
        homedir = getpwuid(getuid())->pw_dir;
    }
    sprintf(fileName, "%s/clog.txt.%s", homedir, fileNameBuffer); // copy the stream Path to be processed

    FILE *fp = fopen(fileName, "a");
    if (fp != NULL)
    {
        if (level == 8)
        {
            fprintf(fp, "%s %s %s", buffer, "FATAL", bPrint.str);
        }
        else if (level == 16)
        {
            fprintf(fp, "%s %s %s", buffer, "ERROR", bPrint.str);
        }
        else if (level == 24)
        {
            fprintf(fp, "%s %s %s", buffer, "WARNING", bPrint.str);
        }
        else if (level == 32)
        {
            fprintf(fp, "%s %s %s", buffer, "INFO", bPrint.str);
            // fprintf(fp, "%s", bPrint.str);
        }

        // #define AV_LOG_FATAL     8
        // #define AV_LOG_ERROR    16
        // #define AV_LOG_WARNING  24
        // #define AV_LOG_INFO     32

        fclose(fp);
        fp = NULL;
    }
    printf("log: %d, %s", level, bPrint.str);

    av_bprint_finalize(&bPrint, NULL);
};

/**
 *  read the video context info
 *
 */
int Init(StreamObj *streamObj, const char *streamPath)
{
    int ret;

    sprintf(streamObj->streamPath, "%s", streamPath); // copy the stream Path to be processed

    // nonblocking mode: set the auto timeout to avoid waiting for the stream forever.
    streamObj->clicker = (struct Clicker *)calloc(1, sizeof(struct Clicker));

    streamObj->videoFormatContext = avformat_alloc_context();
    streamObj->videoFormatContext->interrupt_callback.callback = interrupt_callback;
    streamObj->videoFormatContext->interrupt_callback.opaque = (void *)streamObj->clicker;
    streamObj->clicker->lasttime = time(NULL); // the current time

    ret = avformat_open_input(
        &streamObj->videoFormatContext, streamObj->streamPath,
        NULL, &(streamObj->videoDict));
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_INFO, "can not open stream path `%s`\n", streamObj->streamPath);
        if (streamObj->videoFormatContext)
        {
            avformat_close_input(&streamObj->videoFormatContext);
        }
        streamObj->videoFormatContext = NULL;
        return 1;
    }
    else
    {
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
    // streamObj->timebaseNum = streamObj->videoFormatContext->streams[videoStreamID]->time_base.num;
    // streamObj->timebaseDen = streamObj->videoFormatContext->streams[videoStreamID]->time_base.den;
    streamObj->framerateNum = streamObj->videoFormatContext->streams[videoStreamID]->avg_frame_rate.num;
    streamObj->framerateDen = streamObj->videoFormatContext->streams[videoStreamID]->avg_frame_rate.den;
    streamObj->streamWidth = streamObj->videoCodecContext->width;
    streamObj->streamHeight = streamObj->videoCodecContext->height;

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
    av_log(NULL, AV_LOG_INFO, "stream imagesize: %d\n", streamObj->imageSize);
    streamObj->videoRGBFrame = av_frame_alloc();

    if (streamObj->swsContext == NULL)
    {
        streamObj->swsContext = sws_getCachedContext(
            streamObj->swsContext,
            streamObj->videoCodecContext->width, streamObj->videoCodecContext->height,
            streamObj->videoCodecContext->pix_fmt,
            streamObj->videoCodecContext->width, streamObj->videoCodecContext->height, OUTPUT_PIX_FMT,
            SWS_BICUBIC, NULL, NULL, NULL);
        if (streamObj->swsContext == NULL)
            return 0;
    }

    return 0;
}

int unInit(StreamObj *streamObj)
{
    if (streamObj->clicker)
    {
        free(streamObj->clicker);
        streamObj->clicker = NULL;
    }

    if (streamObj->swsContext)
    {
        sws_freeContext(streamObj->swsContext);
        streamObj->swsContext = NULL;
    }

    if (streamObj->videoRGBFrame)
    {
        av_frame_free(&(streamObj->videoRGBFrame));
        streamObj->videoRGBFrame = NULL;
    }

    if (streamObj->videoFrame)
    {
        // av_frame_unref(streamObj->videoFrame);
        av_frame_free(&(streamObj->videoFrame));
        streamObj->videoFrame = NULL;
    }

    if (streamObj->videoPacket)
    {
        av_packet_free(&(streamObj->videoPacket));
        streamObj->videoPacket = NULL;
    }

    if (streamObj->videoCodecContext)
    {
        avcodec_free_context(&(streamObj->videoCodecContext));
        streamObj->videoCodecContext = NULL;
    }

    if (streamObj->videoFormatContext)
    {
        avformat_close_input(&(streamObj->videoFormatContext));
        streamObj->videoFormatContext = NULL;
    }

    free(streamObj->outputImage);
    streamObj->outputImage = NULL;

    streamObj->streamID = -1; // which stream index to parse in the video
    streamObj->streamWidth = 0;
    streamObj->streamHeight = 0;
    streamObj->framerateNum = 0;
    streamObj->framerateDen = 0;
    streamObj->imageSize = 0;

    memset(streamObj->streamPath, '0', 300);

    av_log(NULL, AV_LOG_INFO, "%s", "finished unref this video stream context\n");
}

int decodeFrame(StreamObj *streamObj)
{
    int ret;

    for (;;)
    {
        if (streamObj->streamEnd == 1)
        {
            av_log(NULL, AV_LOG_INFO, "%s", "# to the end of the video stream.\n");
            break;
        }

        ret = av_read_frame(streamObj->videoFormatContext, streamObj->videoPacket); // 读取一个 packet

        if (streamObj->videoPacket->stream_index != streamObj->streamID)
        {
            // 仅处理该条视频流的包
            av_packet_unref(streamObj->videoPacket);
            av_log(NULL, AV_LOG_WARNING, "%s", "# didnt accept audio packet.\n");
            continue;
        }

        // av_log(NULL, AV_LOG_ERROR, "AVERROR_EOF: %d, %d\n", AVERROR_EOF, ret);
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
                    ret = ConvertYUV2RBG(
                        streamObj->videoFrame, streamObj->videoRGBFrame, streamObj->swsContext,
                        streamObj->outputImage,
                        streamObj->videoCodecContext, streamObj->frameNum);
                    // end_time = clock();
                    // av_log(NULL, AV_LOG_INFO, "read one frame cost time=%f\n",
                    //     (double)(end_time - start_time) / CLOCKS_PER_SEC);
                    streamObj->frameNum += 1;
                    streamObj->streamEnd = 1; //  to the end
                }
                else
                {
                    av_log(NULL, AV_LOG_ERROR, "%s", "other error.\n");

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
                    av_log(NULL, AV_LOG_INFO, "%s",
                           "Receive_frame and send_packet both returned EAGAIN, which is an API violation.\n");
                    sleep(0.04); // sleep for about a duration of one frame
                }
                else if (ret == 0)
                {
                    break;
                }
                else if (ret < 0)
                {
                    av_log(NULL, AV_LOG_ERROR, "%s", "an unknown error concerning send packet.\n");
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
                    ret = ConvertYUV2RBG(
                        streamObj->videoFrame, streamObj->videoRGBFrame,
                        streamObj->swsContext, streamObj->outputImage,
                        streamObj->videoCodecContext, streamObj->frameNum);
                    // end_time = clock();
                    // av_log(NULL, AV_LOG_INFO, "read one frame cost time=%f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);
                    streamObj->frameNum += 1;
                    streamObj->streamEnd = 1; //  to the end
                }
                else
                {
                    av_log(NULL, AV_LOG_ERROR, "%s", "other error concerning receive frame.\n");

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
            av_log(NULL, AV_LOG_INFO, "%s", "# to the end of the video stream.\n");
            return 1;
            // break;
        }

        // av_log(NULL, AV_LOG_INFO, "read a packet %d.\n", streamObj->streamEnd);

        streamObj->clicker->lasttime = time(NULL);                                  // to get the currect frame info before av_read_frame
        ret = av_read_frame(streamObj->videoFormatContext, streamObj->videoPacket); // read a packet

        // av_log(NULL, AV_LOG_INFO, "read a packet. end, get stream id: %d, expected id: %d, ret: %d, EOF: %d\n",
        //    streamObj->videoPacket->stream_index, streamObj->streamID, ret, AVERROR_EOF);
        if (streamObj->videoPacket->stream_index != streamObj->streamID)
        {
            // filter unrelated packet
            av_packet_unref(streamObj->videoPacket);

            if (ret == -5) // -5 means timeout, need to exit this function
            {
                av_log(NULL, AV_LOG_WARNING,
                       "timeout to wating for the next packet data, ret = %d.\n", ret);
                return -5;
            }

            av_log(NULL, AV_LOG_WARNING, "# Only accept packet from streamID: %d.\n", streamObj->streamID);
            continue;
        }

        if (AVERROR_EOF == ret)
        {
            av_log(NULL, AV_LOG_INFO, "has already to the end of the file: %d\n", ret);
            avcodec_send_packet(streamObj->videoCodecContext, streamObj->videoPacket);
            av_packet_unref(streamObj->videoPacket);

            for (;;)
            {
                ret = avcodec_receive_frame(streamObj->videoCodecContext, streamObj->videoFrame);

                if (ret == AVERROR(EAGAIN))
                {
                    break; // need more AVPacket
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
                    ret = ConvertYUV2RBG(
                        streamObj->videoFrame, streamObj->videoRGBFrame, streamObj->swsContext,
                        streamObj->outputImage,
                        streamObj->videoCodecContext, streamObj->frameNum);
                    // end_time = clock();
                    // av_log(NULL, AV_LOG_INFO, "read one frame cost time=%f\n",
                    //     (double)(end_time - start_time) / CLOCKS_PER_SEC);
                    streamObj->frameNum += 1;
                    streamObj->streamEnd = 1; //  to the end
                    memcpy(streamObj->outputImage, streamObj->videoRGBFrame->data[0], streamObj->imageSize);
                    av_frame_unref(streamObj->videoRGBFrame);

                    return 0;
                }
                else
                {
                    av_log(NULL, AV_LOG_ERROR, "%s", "other error concerning receive frame.\n");

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
                    av_log(NULL, AV_LOG_INFO, "%s",
                           "Receive_frame and send_packet both returned EAGAIN, which is an API violation.\n");
                    sleep(0.03); // sleep for about a duration of one frame
                }
                else if (ret == 0)
                {
                    break;
                }
                else if (ret < 0)
                {
                    av_log(NULL, AV_LOG_ERROR, "%s", "an unknown error.\n");
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
                    ret = ConvertYUV2RBG(
                        streamObj->videoFrame, streamObj->videoRGBFrame,
                        streamObj->swsContext, streamObj->outputImage,
                        streamObj->videoCodecContext, streamObj->frameNum);

                    // end_time = clock();
                    // av_log(NULL, AV_LOG_INFO, "read one frame cost time=%f\n",
                    //     (double)(end_time - start_time) / CLOCKS_PER_SEC);
                    streamObj->frameNum += 1;
                    streamObj->streamEnd = 1; //  to the end
                    // av_log(NULL, AV_LOG_INFO, "Successfully read 1 frame %d!\n", streamObj->frameNum);
                    memcpy(streamObj->outputImage, streamObj->videoRGBFrame->data[0], streamObj->imageSize);
                    av_frame_unref(streamObj->videoRGBFrame);

                    return 0;
                }
                else
                {
                    av_log(NULL, AV_LOG_ERROR, "%s", "unknown error 1.\n");

                    return 1;
                }
            }
        }
        else
        {
            av_log(NULL, AV_LOG_ERROR, "%s", "unknown error 2.\n");
            return 1;
        }
    }

    return 1;
}

int main()
{
    int err;
    int ret;

    const char *sourceStreamPath = "rtmp://live.demo.uavcmlc.com:1935/live/DEV02001270?token=03c7986c15e0";
    // const char *sourcePath = "/home/cuichengyu/ffmpeg/qingdao_city_20min_384p_25fps.mp4";
    clock_t start_time, end_time;

    av_log_set_callback(av_log_pyFFmpeg_callback);
    printf("start waiting to new a stream obj.\n");
    // sleep(60);
    // build a new struct object
    start_time = clock();
    printf("cur time: %ld\n", start_time);
    StreamObj *curStreamObj = newStreamObj();
    ret = Init(curStreamObj, sourceStreamPath); // initialize a new stream
    end_time = clock();
    printf("cur time: %ld\n", end_time);
    printf("initializing cost time=%f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    printf("start waiting to get frame.\n");
    // sleep(60);

    start_time = clock();
    int count = 0;
    while (1)
    {
        ret = decodeOneFrame(curStreamObj);
        // if (count > 100)
        // {
        //     break;
        // }
        count++;

        if (ret == 10)
        {
            printf("timeout to get a frame.\n");
            // break;
        }
    }

    end_time = clock();
    printf("read one frame cost time=%f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    ret = unInit(curStreamObj);

    printf("finished 1st period\n");
    sleep(60);
    // ret = save_rgb_to_file(curStreamObj, 0);

    ret = Init(curStreamObj, sourceStreamPath); // initialize a new stream
    end_time = clock();
    printf("cur time: %ld\n", end_time);
    printf("initializing cost time=%f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    start_time = clock();
    while (1)
    {
        ret = decodeOneFrame(curStreamObj);
        if (count > 300)
        {
            break;
        }
        count++;

        if (ret == 10)
        {
            break;
        }
    }

    end_time = clock();
    printf("read one frame cost time=%f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);
    ret = unInit(curStreamObj);

    return 0;
}
