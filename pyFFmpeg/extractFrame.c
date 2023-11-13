#include "extractFrame.h"

static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt, const char *tag)
{
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

    printf("%s: pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
           tag,
           av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
           av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
           av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
           pkt->stream_index);
}

StreamObj *newStreamObj()
{
    StreamObj *streamObj = NULL;
    streamObj = (StreamObj *)malloc(sizeof(StreamObj));
    if (streamObj == NULL)
    {
        return NULL;
    }

    streamObj->streamStateFlag = 0; // 0 means close, 1 means stream context has been opened
    streamObj->streamID = -1;       // which stream index to parse in the video
    streamObj->streamWidth = 0;
    streamObj->streamHeight = 0;
    streamObj->framerateNum = 0; // to compute the fps of the stream, Num / Den
    streamObj->framerateDen = 0;
    streamObj->imageSize = 0;

    streamObj->videoFormatContext = NULL;
    streamObj->videoCodecContext = NULL;
    streamObj->videoPacket = NULL;
    streamObj->videoFrame = NULL;
    streamObj->videoRGBFrame = NULL;
    streamObj->swsContext = NULL;

    streamObj->frameNum = -1; // the current number of the video stream
    streamObj->streamEnd = 0; // has already to the end of the stream

    streamObj->outputImage = NULL; // the extracted frame

    streamObj->clicker = NULL;

    return streamObj;
}

/**
 * build an output object to restore infos
 *
*/
OutputStreamObj *newOutputStreamObj()
{
    OutputStreamObj *outputStreamObj = NULL;
    outputStreamObj = (OutputStreamObj *)malloc(sizeof(OutputStreamObj));
    if (outputStreamObj == NULL)
    {
        return NULL;
    }

    outputStreamObj->outputStreamStateFlag = 0; // to indicate that if the stream has been opened successfully

    outputStreamObj->outputVideoStreamID = -1; // which stream index to parse in the video, default 0.
    outputStreamObj->outputVideoStreamWidth = 0;
    outputStreamObj->outputVideoStreamHeight = 0;

    outputStreamObj->outputvideoFramerateNum = 0; // to compute the fps of the stream, duration / Den
    outputStreamObj->outputvideoFramerateDen = 0;

    // encoder info
    outputStreamObj->outputVideoFormatContext = NULL;
    outputStreamObj->videoEncoderContext = NULL;
    outputStreamObj->videoEncoder = NULL;
    outputStreamObj->videoEncoderFrame = NULL;
    outputStreamObj->videoPacketOut = NULL;

    return outputStreamObj;
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
        swsContextObj, (const uint8_t *const)inputFrame->data,
        inputFrame->linesize, 0, inputFrame->height,
        RGBFrame->data, RGBFrame->linesize);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_INFO, "Error convert video: %d\n", ret);
        return 0;
    }

    // save_rgb_to_file(frame, frame_num);

    // print log once every 20 frames
    if (frameNum % PRINT_FRAME_GAP == 0)
    {
        av_log(NULL, AV_LOG_INFO, "Successfully read one frame, num = %d!\n", frameNum);
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
    if (level > AV_LOG_INFO) // set level
    {
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

    // add log info
    const char *homeDir;
    struct passwd *pw = getpwuid(getuid());
    homeDir = pw->pw_dir;

    // const char *homedir = getpwuid(getuid())->pw_dir;

    // get the pid of the current process
    pid_t pid = getpid();

    sprintf(fileName, "%s/.cache/pyFFmpeg/clog-%d.txt.%s",
            homeDir, pid, fileNameBuffer); // copy the log file path

    // printf("full path: %s\n", fileName);

    FILE *fp = fopen(fileName, "a");
    if (fp != NULL)
    {
        if (level == 8)
        {
            fprintf(fp, "[ %s ] %s %s", buffer, "FATAL", bPrint.str);
        }
        else if (level == 16)
        {
            fprintf(fp, "[ %s ] %s %s", buffer, "ERROR", bPrint.str);
        }
        else if (level == 24)
        {
            fprintf(fp, "[ %s ] %s %s", buffer, "WARNING", bPrint.str);
        }
        else if (level == 32)
        {
            fprintf(fp, "[ %s ] %s %s", buffer, "INFO", bPrint.str);
            // fprintf(fp, "%s", bPrint.str);
        }

        // #define AV_LOG_FATAL     8
        // #define AV_LOG_ERROR    16
        // #define AV_LOG_WARNING  24
        // #define AV_LOG_INFO     32

        fclose(fp);
        fp = NULL;
    }
    printf("%s", bPrint.str);

    av_bprint_finalize(&bPrint, NULL);
};

/**
 *  read the video context info
 *
 *  ret: int
 *      0 means successfully start a video stream
 *      1 means failing to open the input stream
 *      2 means can not find the stream info
 *      3 means can not process params to context
 *      4 means can not open codec context
 *      5 means failing to allocate memory for swsContext
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
        NULL, NULL);
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
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_INFO, "%s\n", "can not find stream info");
        return 2;
    }

    streamObj->videoCodec = NULL;
    int videoStreamID = av_find_best_stream(
        streamObj->videoFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, (const AVCodec **)&streamObj->videoCodec, 0);
    av_log(NULL, AV_LOG_INFO, "video stream number is %d.\n", videoStreamID);

    streamObj->videoCodecContext = avcodec_alloc_context3(NULL);
    ret = avcodec_parameters_to_context(
        streamObj->videoCodecContext,
        streamObj->videoFormatContext->streams[videoStreamID]->codecpar);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_INFO, "%s\n", "can not process params to context.");
        return 3;
    }

    streamObj->videoCodec = avcodec_find_decoder(streamObj->videoCodecContext->codec_id);

    ret = avcodec_open2(streamObj->videoCodecContext, streamObj->videoCodec, NULL);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_INFO, "%s\n", "can not open codec context.");
        return 4;
    }

    // set params for decoder
    streamObj->streamID = videoStreamID;
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
            return 5;  // failed to allocate memory for swsContext
    }

    streamObj->streamStateFlag = 1;
    return 0;
}

/**
 *  initialize video encoder context info
 */
int initOutputStream(OutputStreamObj *outputStreamObj,
        StreamObj *streamObj, const char *outputStreamPath)
{
    int ret;
    sprintf(outputStreamObj->outputStreamPath, "%s", outputStreamPath);

    // open an output format context
    const char *format_name = NULL;
    if (strstr(outputStreamPath, "rtmp://"))
    {
        format_name = "flv";
    }
    else if (strstr(outputStreamPath, "srt://"))
    {
        format_name = "mpegts";
    }
    else if (strstr(outputStreamPath, ".mp4"))
    {
        format_name = "mp4";
    }
    else
    {
        // -3 means the outputStreamPath is illegal.
        return -3;
    }

    ret = avformat_alloc_output_context2(
        &(outputStreamObj->outputVideoFormatContext), NULL, format_name, outputStreamPath);
    if (!outputStreamObj->outputVideoFormatContext)
    {
        av_log(NULL, AV_LOG_INFO, "failed to open an output format context %d \n", AVERROR(ENOMEM));
        return ENOMEM;  // out of memory
    }

    // add a video stream to the context, streamID default by 0
    outputStreamObj->outputVideoStreamID = 0;
    outputStreamObj->inputVideoTimebase = streamObj->videoFormatContext->streams[streamObj->streamID]->time_base;

    outputStreamObj->outputVideoStream = avformat_new_stream(
        outputStreamObj->outputVideoFormatContext, NULL);
    outputStreamObj->outputVideoStream->time_base = outputStreamObj->inputVideoTimebase;

    // set outputVideoStream width and height
    // outputStreamObj->outputVideoStream->codecpar->width = streamObj->videoFormatContext->streams[streamObj->streamID]->codecpar->width;
    // outputStreamObj->outputVideoStream->codecpar->height = streamObj->videoFormatContext->streams[streamObj->streamID]->codecpar->height;
    outputStreamObj->outputVideoStream->sample_aspect_ratio = streamObj->videoFormatContext->streams[streamObj->streamID]->sample_aspect_ratio;
    outputStreamObj->outputVideoStream->avg_frame_rate.num = streamObj->videoFormatContext->streams[streamObj->streamID]->avg_frame_rate.num;
    outputStreamObj->outputVideoStream->avg_frame_rate.den = streamObj->videoFormatContext->streams[streamObj->streamID]->avg_frame_rate.den;

    // copy input stream params to output streams
    ret = avcodec_parameters_copy(
        outputStreamObj->outputVideoStream->codecpar,
        streamObj->videoFormatContext->streams[streamObj->streamID]->codecpar);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "avcodec_parameters_copy failed %d.\n", ret);
        return ret;
    }
    outputStreamObj->outputVideoStream->codecpar->codec_tag = 0;

    // copy metadata
    av_dict_copy(
        &outputStreamObj->outputVideoStream->metadata,
        streamObj->videoFormatContext->streams[streamObj->streamID]->metadata, 0);

    // outputStreamObj->videoEncoderFrame = av_frame_alloc();  // 此数据帧较为关键，即输入流的数据帧
    outputStreamObj->videoPacketOut = av_packet_alloc();

    // ensure frame rate of output is equal to input frame rate.
    outputStreamObj->inputFramerate = streamObj->videoFormatContext->streams[streamObj->streamID]->avg_frame_rate;

    return 0;
}

StreamObj *unInit(StreamObj *streamObj)
{
    streamObj->streamStateFlag = 0; // stream has been closed.

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

    return streamObj;
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

        ret = av_read_frame(streamObj->videoFormatContext, streamObj->videoPacket); // read a packet

        if (streamObj->videoPacket->stream_index != streamObj->streamID)
        {
            // only accept video packets, excluding audio and other packets.
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

/**
 * to get one frame from the stream
 *
 * ret: int
 *     0 means getting a frame successfully.
 *     1 means the stream has been to the end
 *     -5 means timeout to get the next packet, need to exit this function
 *     -1 means other error concerning avcodec_receive_frame.
 *     -2 means other error concerning av_read_frame.
 *     -4 means packet mismatch & unable to seek to the next packet.
 */
int decodeOneFrame(StreamObj *streamObj)
{
    int ret;
    streamObj->streamEnd = 0;

    for (;;)
    {
        if (streamObj->streamEnd == 1)
        {
            av_log(NULL, AV_LOG_INFO, "%s", "It's been to the end of the video stream.\n");
            return 1;
        }

        // av_log(NULL, AV_LOG_INFO, "read one packet %d.\n", streamObj->streamEnd);

        streamObj->clicker->lasttime = time(NULL);                                  // get the currect time before av_read_frame
        ret = av_read_frame(streamObj->videoFormatContext, streamObj->videoPacket); // read a packet
        // av_log(NULL, AV_LOG_DEBUG, "%s", "### successfully read one packet.\n");
        log_packet(streamObj->videoFormatContext, streamObj->videoPacket, "in");
        // av_log(NULL, AV_LOG_INFO, "read a packet. end, get stream id: %d, expected id: %d, ret: %d, EOF: %d\n",
        //    streamObj->videoPacket->stream_index, streamObj->streamID, ret, AVERROR_EOF);
        if (streamObj->videoPacket->stream_index != streamObj->streamID)
        {
            // filter unrelated packet
            av_packet_unref(streamObj->videoPacket);

            if (ret == -5) // -5 means timeout, need to exit this function
            {
                av_log(NULL, AV_LOG_WARNING,
                       "timeout to waiting for the next packet data, ret = %d.\n", ret);
                return -5;
            }
            else if (ret < 0) {
                av_log(NULL, AV_LOG_WARNING,
                       "Probably Packet mismatch, unable to seek the next packet. ret: %d.\n", ret);
                return -4;
            }
            else{
                if (streamObj->frameNum % (PRINT_FRAME_GAP * 2) == 0)
                {
                    av_log(NULL, AV_LOG_WARNING,
                           "Only accept packet from streamID: %d, ret: %d, eof: %d.\n",
                           streamObj->streamID, ret, AVERROR_EOF);
                }
            }

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

                    return -1;
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
                    return 1;
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

                    return -1;
                }
            }
        }
        else
        {
            av_log(NULL, AV_LOG_ERROR, "%s", "unknown error 2.\n");
            return -2;
        }
    }

    return 1;
}

/**
 * to encode one frame to the output stream
 *
 * ret: int
 *     0 means encoding a frame successfully.
 *
 *
 */
int encodeOneFrame(OutputStreamObj *outputStreamObj, StreamObj *streamObj, int end_of_stream) {
    int ret;

    if (outputStreamObj->videoEncoderContext == NULL)
    {
        // initialize packet_out
        // streamObj->videoPacketOut = av_packet_alloc();

        // initialize encoder context
        outputStreamObj->videoEncoder = (AVCodec *)avcodec_find_encoder(AV_CODEC_ID_H264);

        outputStreamObj->videoEncoderContext = avcodec_alloc_context3(outputStreamObj->videoEncoder);
        outputStreamObj->videoEncoderContext->codec_type = AVMEDIA_TYPE_VIDEO;
        outputStreamObj->videoEncoderContext->bit_rate = 2511836;

        outputStreamObj->videoEncoderContext->framerate = outputStreamObj->inputFramerate;
        outputStreamObj->videoEncoderContext->gop_size = 12;
        outputStreamObj->videoEncoderContext->max_b_frames = 12;
        outputStreamObj->videoEncoderContext->profile = FF_PROFILE_H264_HIGH;

        // get params from AVFormatContext
        outputStreamObj->videoEncoderContext->time_base = streamObj->videoFormatContext->streams[streamObj->streamID]->time_base;
        outputStreamObj->videoEncoderContext->width = streamObj->videoFormatContext->streams[streamObj->streamID]->codecpar->width;
        outputStreamObj->videoEncoderContext->height = streamObj->videoFormatContext->streams[streamObj->streamID]->codecpar->height;

        // get params from AVFrame
        outputStreamObj->videoEncoderContext->sample_aspect_ratio = streamObj->videoFrame->sample_aspect_ratio;
        outputStreamObj->outputVideoStream->sample_aspect_ratio = streamObj->videoFrame->sample_aspect_ratio;

        // outputStreamObj->videoEncoderContext->sample_aspect_ratio.num = 1;
        outputStreamObj->videoEncoderContext->pix_fmt = streamObj->videoFrame->format;
        outputStreamObj->videoEncoderContext->color_range = streamObj->videoFrame->color_range;
        outputStreamObj->videoEncoderContext->color_primaries = streamObj->videoFrame->color_primaries;
        outputStreamObj->videoEncoderContext->color_trc = streamObj->videoFrame->color_trc;
        outputStreamObj->videoEncoderContext->colorspace = streamObj->videoFrame->colorspace;
        outputStreamObj->videoEncoderContext->chroma_sample_location = streamObj->videoFrame->chroma_location;

        /* 注意，这个 field_order 不同的视频的值是不一样的，这里我写死了。
         * 因为 本文的视频就是 AV_FIELD_PROGRESSIVE
         * 生产环境要对不同的视频做处理的
         */
        outputStreamObj->videoEncoderContext->field_order = AV_FIELD_PROGRESSIVE;
        // AV_FIELD_UNKNOWN;
        // AV_FIELD_PROGRESSIVE;

        // copy codec from videoEncoderContext to outputVideoFormatContext->VideoStream
        // outputStreamObj->outputVideoStream[outputStreamObj->outputVideoStreamID].codec->codec = outputStreamObj->videoEncoderContext->codec;
        // outputStreamObj->outputVideoStream[outputStreamObj->outputVideoStreamID].codec->codec_type = outputStreamObj->videoEncoderContext->codec_type;
        // outputStreamObj->outputVideoStream[outputStreamObj->outputVideoStreamID].codec->codec_id = outputStreamObj->videoEncoderContext->codec_id;

        if (outputStreamObj->outputVideoFormatContext->oformat->flags & AVFMT_GLOBALHEADER) {
            av_log(NULL, AV_LOG_INFO, "%s", "set video GLOBAL_HEADER\n");
            outputStreamObj->videoEncoderContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            // may cause memory leak
            outputStreamObj->videoEncoderContext->extradata = streamObj->videoCodecContext->extradata;
            outputStreamObj->videoEncoderContext->extradata_size = streamObj->videoCodecContext->extradata_size;
        }

        // copy params form video encoder context to stream
        ret = avcodec_parameters_from_context(
            outputStreamObj->outputVideoStream->codecpar,
            outputStreamObj->videoEncoderContext);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_INFO, "avcodec_parameters_from_context failed %d.\n", ret);
            return ret;
        }

        AVDictionary *codec_options = NULL;
        av_dict_set(&codec_options, "profile", "high", 0);
        av_dict_set(&codec_options, "preset", "superfast", 0);
        av_dict_set(&codec_options, "tune", "zerolatency", 0);

        ret = avcodec_open2(
            outputStreamObj->videoEncoderContext,
            outputStreamObj->videoEncoder, NULL); // &codec_options
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_INFO, "avcodec_open2 failed %d.\n", ret);
            return ret;
        }

        // open output file or stream format context
        ret = avio_open2(
            &(outputStreamObj->outputVideoFormatContext->pb),
            outputStreamObj->outputStreamPath,
            AVIO_FLAG_WRITE,
            // &(outputStreamObj->outputVideoFormatContext->interrupt_callback),
            NULL,
            NULL);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_INFO, "avio_open2 failed %d.\n", ret);
            return ret;
        }

        ret = avformat_write_header(outputStreamObj->outputVideoFormatContext, NULL);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_INFO, "avformat_write_header failed %d.\n", ret);
            return ret;
        }

        printf("\n");
        av_dump_format(outputStreamObj->outputVideoFormatContext, 0, outputStreamObj->outputStreamPath, 1);
        // outputStreamObj->outputVideoStream->time_base = outputStreamObj->inputVideoTimebase;
    }

    // frame -> encoder context
    if (end_of_stream == 1) {
        ret = avcodec_send_frame(
            outputStreamObj->videoEncoderContext, NULL);
    } else {
        ret = avcodec_send_frame(
            outputStreamObj->videoEncoderContext, streamObj->videoFrame);
    }

    if (ret < 0)
    {
        av_log(NULL, AV_LOG_INFO, "avcodec_send_frame failed %d %d.\n", ret, AVERROR(EINVAL));
        return ret;
    }

    // encoder context -> packet
    for (;;)
    {
        ret = avcodec_receive_packet(
            outputStreamObj->videoEncoderContext, outputStreamObj->videoPacketOut);
        if (ret == AVERROR(EAGAIN))
        {
            // av_log(NULL, AV_LOG_INFO, "avcodec_receive_pachet need to try again %d\n", ret);
            // break;
            return 2;  // try again
        }

        // 前面没有往 编码器发 NULL, 所以正常情况 ret 不会小于 0
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_INFO, "avcodec_receive_packet failed %d.\n", ret);
            return ret;
        }
        // 编码出 AVPacket ，打印一些信息。
        av_log(NULL, AV_LOG_INFO, "encoder packet_out size: %d.\n", outputStreamObj->videoPacketOut->size);
        log_packet(outputStreamObj->outputVideoFormatContext, outputStreamObj->videoPacketOut, "out_1");
        // 设置 AVPacket 的 stream_index ，这样才知道是哪个流的。
        outputStreamObj->videoPacketOut->stream_index = outputStreamObj->outputVideoStream->index;
        // 转换 AVPacket 的时间基为 输出流的时间基。
        // outputStreamObj->videoPacketOut->pts = av_rescale_q_rnd(
        //     outputStreamObj->videoPacketOut->pts,          // int
        //     outputStreamObj->inputVideoTimebase,           // AVRational
        //     outputStreamObj->outputVideoStream->time_base, // AVRational
        //     AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
        // outputStreamObj->videoPacketOut->dts = av_rescale_q_rnd(
        //     outputStreamObj->videoPacketOut->dts,          // int
        //     outputStreamObj->inputVideoTimebase,           // AVRational
        //     outputStreamObj->outputVideoStream->time_base, // AVRational
        //     AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
        // outputStreamObj->videoPacketOut->duration = av_rescale_q_rnd(
        //     outputStreamObj->videoPacketOut->duration,
        //     outputStreamObj->inputVideoTimebase,           // AVRational
        //     outputStreamObj->outputVideoStream->time_base, // AVRational
        //     AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);

        // av_packet_rescale_ts(outputStreamObj->videoPacketOut, in_stream->time_base, out_stream->time_base);
        outputStreamObj->videoPacketOut->duration = 33;
        outputStreamObj->videoPacketOut->pos = -1;


        log_packet(outputStreamObj->outputVideoFormatContext, outputStreamObj->videoPacketOut, "out_2");

        ret = av_interleaved_write_frame(
            outputStreamObj->outputVideoFormatContext, outputStreamObj->videoPacketOut);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_INFO, "av_interleaved_write_frame failed %d.\n", ret);
            return ret;
        }

        av_packet_unref(outputStreamObj->videoPacketOut);
    }

    return 0;
}

int main()
{
    int err;
    int ret;
    // av_register_all();
    // avformat_network_init();

    // const char *sourceStreamPath = "rtmp://live.demo.uavcmlc.com:1935/live/DEV02001270?token=03c7986c15e0";
    // const char *sourcePath = "/home/cuichengyu/github/pyFFmpeg/pyFFmpeg/output.mp4";
    // const char *sourcePath = "/home/cuichengyu/pyffmpeg_workspace/fast_moving_output.mp4";
    const char *sourcePath = "rtmp://live.demo.uavcmlc.com:1935/live/DEV02003179?token=8622d43632a1";
    // const char *sourceStreamPath = "rtmp://live.demo.uavcmlc.com:1935/live/DEV02001270?token=03c7986c15e0";
    // const char *sourceStreamPath = "/home/cuichengyu/github/pyFFmpeg/pyFFmpeg/res1.mp4";
    const char *sourceStreamPath = "rtmp://192.168.35.30:30107/live/cuichengyu4";

    clock_t start_time, end_time;

    av_log_set_callback(av_log_pyFFmpeg_callback);
    printf("start waiting to new a stream obj.\n");
    // sleep(60);

    // build a new struct object
    start_time = clock();
    StreamObj *curStreamObj = newStreamObj();
    ret = Init(curStreamObj, sourcePath); // initialize a new stream
    end_time = clock();
    printf("initializing decoder cost time=%f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    sleep(1);
    start_time = clock();
    OutputStreamObj *curOutputStreamObj = newOutputStreamObj();
    ret = initOutputStream(curOutputStreamObj, curStreamObj, sourceStreamPath); // initialize a new stream
    end_time = clock();
    printf("initializing encoder cost time=%f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    printf("start waiting to get frame.\n");
    // sleep(60);
    if (ret != 0 ) {
        return 1;
    }

    start_time = clock();
    int count = 0;
    int file_end = 0;
    while (1)
    {
        ret = decodeOneFrame(curStreamObj);
        // if (count > 100)
        // {
        //     break;
        // }
        count++;
        if (ret == 1) {
            // which means to the end of the file
            file_end = ret;
        }

        ret = encodeOneFrame(curOutputStreamObj, curStreamObj, file_end);
        if (ret < 0) {
            return 1;
            // break;
        }

        if (ret == 10)
        {
            printf("timeout to get a frame.\n");
            // break;
        }

        if (file_end == 1) {
            // break;
            return 0;
        }
    }


    end_time = clock();
    printf("read one frame cost time=%f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    curStreamObj = unInit(curStreamObj);

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
    curStreamObj = unInit(curStreamObj);

    return 0;
}
