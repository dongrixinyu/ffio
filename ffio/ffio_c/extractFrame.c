#include "extractFrame.h"

InputStreamObj *newInputStreamObj()
{
    InputStreamObj *inputStreamObj = NULL;
    inputStreamObj = (InputStreamObj *)malloc(sizeof(InputStreamObj));
    if (inputStreamObj == NULL)
    {
        return NULL;
    }

    inputStreamObj->inputStreamStateFlag = 0; // 0 means close, 1 means stream context has been opened
    inputStreamObj->inputVideoStreamID = -1;       // which stream index to parse in the video
    inputStreamObj->inputVideoStreamWidth = 0;
    inputStreamObj->inputVideoStreamHeight = 0;
    inputStreamObj->inputFramerateNum = 0; // to compute the fps of the stream, Num / Den
    inputStreamObj->inputFramerateDen = 0;
    inputStreamObj->imageSize = 0;

    inputStreamObj->inputFormatContext = NULL;
    inputStreamObj->videoCodecContext = NULL;
    inputStreamObj->videoPacket = NULL;
    inputStreamObj->videoFrame = NULL;
    inputStreamObj->videoTmpFrame = NULL;
    inputStreamObj->videoSWFrame = NULL;
    inputStreamObj->videoRGBFrame = NULL;
    inputStreamObj->swsContext = NULL;
    inputStreamObj->hw_device_ctx = NULL;
    inputStreamObj->hw_pix_fmt = -1;

    inputStreamObj->hw_flag = 0;   // 1 means to use hardware, 0 means not
    inputStreamObj->frameNum = -1; // the current number of the video stream
    inputStreamObj->streamEnd = 0; // has already to the end of the stream

    inputStreamObj->extractedFrame = NULL; // the extracted frame

    inputStreamObj->clicker = NULL;

    return inputStreamObj;
}

int save_rgb_to_file(InputStreamObj *inputStreamObj, int frame_num)
{
    // concatenate file name
    char pic_name[200] = {0};
    sprintf(pic_name, "/home/cuichengyu/test_images/result_%d.yuv", frame_num);

    // write to file
    FILE *fp = NULL;
    fp = fopen(pic_name, "wb+");

    fwrite(inputStreamObj->extractedFrame, 1, inputStreamObj->imageSize * 3, fp);
    fclose(fp);

    return 0;
}

int convertYUV2RGB(InputStreamObj *inputStreamObj)
{
    int ret;

    if (inputStreamObj->swsContext == NULL)
    {
        // replace SWS_FAST_BILINEAR with other options SWS_BICUBIC
        // several methods and its CPU %
        // SWS_BICUBIC 57.8 %
        // SWS_FAST_BILINEAR 35.7 %
        // SWS_GAUSS 56.3 %
        // SWS_BICUBLIN 38.9 %
        // the input frame pix_fmt should be changed according to the hw decode and cpu decode.
        inputStreamObj->swsContext = sws_getCachedContext(
            inputStreamObj->swsContext,
            inputStreamObj->videoCodecContext->width, inputStreamObj->videoCodecContext->height,
            inputStreamObj->videoTmpFrame->format, // videoCodecContext->pix_fmt,
            inputStreamObj->videoCodecContext->width, inputStreamObj->videoCodecContext->height,
            OUTPUT_RGB_PIX_FMT,
            SWS_FAST_BILINEAR, // SWS_BICUBIC,
            NULL, NULL, NULL);
        if (inputStreamObj->swsContext == NULL)
            return 0;
    }

    inputStreamObj->videoRGBFrame->width = inputStreamObj->videoCodecContext->width;
    inputStreamObj->videoRGBFrame->height = inputStreamObj->videoCodecContext->height;
    inputStreamObj->videoRGBFrame->format = OUTPUT_RGB_PIX_FMT;

    ret = av_frame_get_buffer(inputStreamObj->videoRGBFrame, 1);
    if (ret != 0)
    {
        av_log(NULL, AV_LOG_INFO, "%s", "no memory for frame data buffer.\n");
    }

    ret = sws_scale(
        inputStreamObj->swsContext, (const uint8_t *const)inputStreamObj->videoTmpFrame->data,
        inputStreamObj->videoTmpFrame->linesize, 0, inputStreamObj->videoTmpFrame->height,
        inputStreamObj->videoRGBFrame->data, inputStreamObj->videoRGBFrame->linesize);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_INFO, "Error convert video: %d\n", ret);
        return 0;
    }

    return 1;
}

/**
 *  read the video context info, including format context and codec context.
 *
 *  params:
 *      hw_flag: set to declare if use cuda gpu to accelarate. 1 means to use, 0 means not.
 *      hw_device: which cuda to use, e.g. cuda:0
 *
 *  ret: int
 *      0 means successfully start a video stream
 *      1 means failing to open the input stream
 *      2 means can not find the stream info
 *      3 means can not process params to context
 *      4 means can not open codec context
 *      5 means failing to allocate memory for swsContext(deleted)
 */
int initializeInputStream(
    InputStreamObj *inputStreamObj, const char *streamPath, int hw_flag, const char *hw_device)
{
    int ret;

    sprintf(inputStreamObj->inputStreamPath, "%s", streamPath); // copy the stream Path to be processed

    // nonblocking mode: set the auto timeout to avoid waiting for the stream forever.
    inputStreamObj->clicker = (struct Clicker *)calloc(1, sizeof(struct Clicker));

    inputStreamObj->inputFormatContext = avformat_alloc_context();
    inputStreamObj->inputFormatContext->interrupt_callback.callback = interrupt_callback;
    inputStreamObj->inputFormatContext->interrupt_callback.opaque = (void *)inputStreamObj->clicker;
    inputStreamObj->clicker->lasttime = time(NULL); // the current time

    ret = avformat_open_input(
        &inputStreamObj->inputFormatContext, inputStreamObj->inputStreamPath,
        NULL, NULL);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_INFO, "can not open stream path `%s`\n", inputStreamObj->inputStreamPath);
        if (inputStreamObj->inputFormatContext)
        {
            avformat_close_input(&inputStreamObj->inputFormatContext);
        }
        inputStreamObj->inputFormatContext = NULL;

        return 1;
    }
    else
    {
        av_log(NULL, AV_LOG_INFO, "open stream path `%s` successfully.\n",
               inputStreamObj->inputStreamPath);
    }

    ret = avformat_find_stream_info(inputStreamObj->inputFormatContext, NULL);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_INFO, "%s\n", "can not find stream info");
        return 2;
    }

    inputStreamObj->videoCodec = NULL;
    int videoStreamID = av_find_best_stream(
        inputStreamObj->inputFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, (const AVCodec **)&inputStreamObj->videoCodec, 0);
    av_log(NULL, AV_LOG_INFO, "video stream number is %d.\n", videoStreamID);

    inputStreamObj->hw_flag = hw_flag;
    if(inputStreamObj->hw_flag)
    {
        for (int i = 0;; i++)
        {
            const AVCodecHWConfig *config = avcodec_get_hw_config(
                inputStreamObj->videoCodec, i);
            if (!config)
            {
                av_log(NULL, AV_LOG_ERROR, "Decoder %s does not support device type %s.\n",
                       inputStreamObj->videoCodec->name, av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_CUDA));
                return -1;
            }
            if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
                config->device_type == AV_HWDEVICE_TYPE_CUDA)
            {
                inputStreamObj->hw_pix_fmt = config->pix_fmt;
                break;
            }
        }
    }

    inputStreamObj->videoCodecContext = avcodec_alloc_context3(NULL);
    ret = avcodec_parameters_to_context(
        inputStreamObj->videoCodecContext,
        inputStreamObj->inputFormatContext->streams[videoStreamID]->codecpar);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_INFO, "%s\n", "can not process params to context.");
        return 3;
    }

    if(inputStreamObj->hw_flag)
    {
        inputStreamObj->videoCodecContext->get_format = get_hw_format;

        // hw context initialization
        // const char hw_device[128] = "cuda:0"; // get it from user params
        ret = av_hwdevice_ctx_create(
            &(inputStreamObj->hw_device_ctx), AV_HWDEVICE_TYPE_CUDA,
            (const char *)hw_device, NULL, 0);
        if (ret != 0)
        {
            char errbuf[200];
            av_strerror(ret, errbuf, sizeof(errbuf));
            av_log(NULL, AV_LOG_ERROR,
                   "initialize hardware context failed, ret: %d - %s.\n", ret, errbuf);
        }
        else
        {
            av_log(NULL, AV_LOG_INFO, "successfully initialize the nvidia cuda.\n");
        }

        inputStreamObj->videoCodecContext->hw_device_ctx = av_buffer_ref(inputStreamObj->hw_device_ctx);
    }

    inputStreamObj->videoCodec = avcodec_find_decoder(inputStreamObj->videoCodecContext->codec_id);

    ret = avcodec_open2(inputStreamObj->videoCodecContext, inputStreamObj->videoCodec, NULL);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_INFO, "%s\n", "can not open codec context.");
        return 4;
    }

    // set params for decoder
    inputStreamObj->inputVideoStreamID = videoStreamID;
    inputStreamObj->inputFramerateNum = inputStreamObj->inputFormatContext->streams[videoStreamID]->avg_frame_rate.num;
    inputStreamObj->inputFramerateDen = inputStreamObj->inputFormatContext->streams[videoStreamID]->avg_frame_rate.den;
    inputStreamObj->inputVideoStreamWidth = inputStreamObj->videoCodecContext->width;
    inputStreamObj->inputVideoStreamHeight = inputStreamObj->videoCodecContext->height;

    // just print the stream info on the screen
    av_dump_format(inputStreamObj->inputFormatContext, videoStreamID, inputStreamObj->inputStreamPath, 0);

    inputStreamObj->videoPacket = av_packet_alloc();
    inputStreamObj->videoFrame = av_frame_alloc();
    if(inputStreamObj->hw_flag)
    {
        inputStreamObj->videoSWFrame = av_frame_alloc();
    }

    inputStreamObj->frameNum = 0;
    inputStreamObj->streamEnd = 0; // 0 means not reaching the end, 1 means the stream is empty.

    int channelNum;
    if (OUTPUT_RGB_PIX_FMT == AV_PIX_FMT_RGB24)
    {
        channelNum = 3;
    }
    else if (OUTPUT_RGB_PIX_FMT == AV_PIX_FMT_RGBA)
    {
        channelNum = 4;
    }

    inputStreamObj->imageSize = inputStreamObj->inputVideoStreamWidth * inputStreamObj->inputVideoStreamHeight * channelNum;
    inputStreamObj->extractedFrame = (unsigned char *)malloc(inputStreamObj->imageSize);
    av_log(NULL, AV_LOG_INFO, "stream imagesize: %d\n", inputStreamObj->imageSize);
    inputStreamObj->videoRGBFrame = av_frame_alloc();

    inputStreamObj->inputStreamStateFlag = 1;
    return 0;
}

InputStreamObj *finalizeInputStream(InputStreamObj *inputStreamObj)
{
    inputStreamObj->inputStreamStateFlag = 0; // stream has been closed.

    if (inputStreamObj->clicker)
    {
        free(inputStreamObj->clicker);
        inputStreamObj->clicker = NULL;
    }

    if (inputStreamObj->swsContext)
    {
        sws_freeContext(inputStreamObj->swsContext);
        inputStreamObj->swsContext = NULL;
    }

    if (inputStreamObj->videoRGBFrame)
    {
        av_frame_free(&(inputStreamObj->videoRGBFrame));
        inputStreamObj->videoRGBFrame = NULL;
    }

    if (inputStreamObj->videoFrame)
    {
        av_frame_free(&(inputStreamObj->videoFrame));
        inputStreamObj->videoFrame = NULL;
    }

    if (inputStreamObj->videoSWFrame)
    {
        av_frame_free(&(inputStreamObj->videoSWFrame));
        inputStreamObj->videoSWFrame = NULL;
    }

    if (inputStreamObj->videoTmpFrame)
    {
        // av_frame_free(&(inputStreamObj->videoTmpFrame));
        inputStreamObj->videoTmpFrame = NULL;
    }

    if (inputStreamObj->videoPacket)
    {
        av_packet_free(&(inputStreamObj->videoPacket));
        inputStreamObj->videoPacket = NULL;
    }

    if (inputStreamObj->videoCodecContext)
    {
        avcodec_close(inputStreamObj->videoCodecContext);
        avcodec_free_context(&(inputStreamObj->videoCodecContext));
        inputStreamObj->videoCodecContext = NULL;
    }

    if (inputStreamObj->inputFormatContext)
    {
        avformat_close_input(&(inputStreamObj->inputFormatContext));
        inputStreamObj->inputFormatContext = NULL;
    }

    if(inputStreamObj->hw_device_ctx)
    {
        av_buffer_unref(&(inputStreamObj->hw_device_ctx));
        inputStreamObj->hw_device_ctx = NULL;
    }

    free(inputStreamObj->extractedFrame);
    inputStreamObj->extractedFrame = NULL;
    inputStreamObj->videoCodec = NULL;

    inputStreamObj->inputVideoStreamID = -1; // which stream index to parse in the video
    inputStreamObj->inputVideoStreamWidth = 0;
    inputStreamObj->inputVideoStreamHeight = 0;
    inputStreamObj->inputFramerateNum = 0;
    inputStreamObj->inputFramerateDen = 0;
    inputStreamObj->imageSize = 0;
    inputStreamObj->frameNum = 0;
    inputStreamObj->hw_pix_fmt = AV_PIX_FMT_NONE;

    memset(inputStreamObj->inputStreamPath, '0', 300);

    av_log(NULL, AV_LOG_INFO, "%s", "finished unref input video stream context.\n");

    return inputStreamObj;
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
int decodeOneFrame(InputStreamObj *inputStreamObj)
{
    int ret;
    inputStreamObj->streamEnd = 0;

    for (;;)
    {
        if (inputStreamObj->streamEnd == 1)
        {
            av_log(NULL, AV_LOG_INFO, "%s", "It's been to the end of the video stream.\n");
            return 1;
        }

        inputStreamObj->clicker->lasttime = time(NULL);
        ret = av_read_frame(inputStreamObj->inputFormatContext, inputStreamObj->videoPacket); // read a packet
        // log_packet(inputStreamObj->inputFormatContext, inputStreamObj->videoPacket, "in");

        if (inputStreamObj->videoPacket->stream_index != inputStreamObj->inputVideoStreamID)
        {
            // filter unrelated packet
            av_packet_unref(inputStreamObj->videoPacket);

            if (ret == -5)  // -5 means timeout, need to exit this function
            {
                char errbuf[200];
                av_strerror(ret, errbuf, sizeof(errbuf));
                av_log(NULL, AV_LOG_WARNING,
                       "timeout to waiting for the next packet data, ret: %d - %s.\n", ret, errbuf);
                return -5;
            }
            else if (ret < 0) {
                char errbuf[200];
                av_strerror(ret, errbuf, sizeof(errbuf));
                av_log(NULL, AV_LOG_WARNING,
                       "Probably Packet mismatch, unable to seek the next packet. ret: %d - %s.\n",
                       ret, errbuf);
                return -4;
            }
            else{
                if (inputStreamObj->frameNum % (PRINT_FRAME_GAP * 2) == 0)
                {
                    av_log(NULL, AV_LOG_WARNING,
                           "Only accept packet from streamID: %d, ret: %d.\n",
                           inputStreamObj->inputVideoStreamID, ret);
                }
            }

            continue;
        }

        if (AVERROR_EOF == ret)
        {
            av_log(NULL, AV_LOG_INFO, "has already to the end of the file: %d\n", ret);
            avcodec_send_packet(inputStreamObj->videoCodecContext, inputStreamObj->videoPacket);
            av_packet_unref(inputStreamObj->videoPacket);

            for (;;)
            {
                ret = avcodec_receive_frame(inputStreamObj->videoCodecContext, inputStreamObj->videoFrame);

                if (ret == AVERROR(EAGAIN))
                {
                    break; // need more AVPacket
                }
                else if (ret == AVERROR_EOF)
                {
                    // to the end of the stream
                    inputStreamObj->streamEnd = 1;
                    return 1;
                }
                else if (ret >= 0)
                {
                    if (inputStreamObj->hw_flag)
                    {
                        if (inputStreamObj->videoFrame->format == inputStreamObj->hw_pix_fmt)
                        {
                            ret = av_hwframe_transfer_data(
                                inputStreamObj->videoSWFrame, inputStreamObj->videoFrame, 0);
                            inputStreamObj->videoTmpFrame = inputStreamObj->videoSWFrame;
                        }
                        else
                        {
                            inputStreamObj->videoTmpFrame = inputStreamObj->videoFrame;
                        }
                    }
                    else
                    {
                        inputStreamObj->videoTmpFrame = inputStreamObj->videoFrame;
                    }

                    ret = convertYUV2RGB(inputStreamObj);

                    // end_time = clock();
                    // av_log(NULL, AV_LOG_INFO, "read one frame cost time=%f\n",
                    //     (double)(end_time - start_time) / CLOCKS_PER_SEC);

                    inputStreamObj->streamEnd = 1; //  to the end
                    memcpy(inputStreamObj->extractedFrame, inputStreamObj->videoRGBFrame->data[0], inputStreamObj->imageSize);
                    av_frame_unref(inputStreamObj->videoRGBFrame);

                    // print log once every PRINT_FRAME_GAP frames
                    if (inputStreamObj->frameNum % PRINT_FRAME_GAP == 0)
                    {
                        av_log(NULL, AV_LOG_INFO, "Successfully read one frame, num = %d.\n", inputStreamObj->frameNum);
                    }
                    inputStreamObj->frameNum += 1;

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
                ret = avcodec_send_packet(inputStreamObj->videoCodecContext, inputStreamObj->videoPacket);
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
                    char errbuf[200];
                    av_strerror(ret, errbuf, sizeof(errbuf));
                    av_log(NULL, AV_LOG_ERROR,
                           "an unknown error, ret: %d - %s.\n", ret, errbuf);

                    break;
                }
            }

            av_packet_unref(inputStreamObj->videoPacket);

            for (;;)
            {
                ret = avcodec_receive_frame(
                    inputStreamObj->videoCodecContext, inputStreamObj->videoFrame);

                if (ret == AVERROR(EAGAIN))
                {
                    break; // need more AVPacket
                }
                else if (ret == AVERROR_EOF)
                {
                    // to the end of the video stream
                    inputStreamObj->streamEnd = 1;
                    return 1;
                }
                else if (ret >= 0)
                {
                    if(inputStreamObj->hw_flag)
                    {
                        if (inputStreamObj->videoFrame->format == inputStreamObj->hw_pix_fmt)
                        {
                            ret = av_hwframe_transfer_data(
                                inputStreamObj->videoSWFrame, inputStreamObj->videoFrame, 0);
                            inputStreamObj->videoTmpFrame = inputStreamObj->videoSWFrame;
                        }
                        else
                        {
                            inputStreamObj->videoTmpFrame = inputStreamObj->videoFrame;
                        }
                    }
                    else
                    {
                        inputStreamObj->videoTmpFrame = inputStreamObj->videoFrame;
                    }

                    ret = convertYUV2RGB(inputStreamObj);

                    // memcpy might be dismissed to accelerate.
                    memcpy(inputStreamObj->extractedFrame, inputStreamObj->videoRGBFrame->data[0], inputStreamObj->imageSize);
                    av_frame_unref(inputStreamObj->videoRGBFrame);

                    // print log once every PRINT_FRAME_GAP frames
                    if (inputStreamObj->frameNum % PRINT_FRAME_GAP == 0)
                    {
                        av_log(NULL, AV_LOG_INFO, "Successfully read one frame, num = %d.\n", inputStreamObj->frameNum);
                    }
                    inputStreamObj->frameNum += 1;

                    return 0;
                }
                else
                {
                    char errbuf[200];
                    av_strerror(ret, errbuf, sizeof(errbuf));
                    av_log(NULL, AV_LOG_ERROR,
                           "an unknown error -1, ret: %d - %s.\n", ret, errbuf);
                    return -1;
                }
            }
        }
        else
        {
            char errbuf[200];
            av_strerror(ret, errbuf, sizeof(errbuf));
            av_log(NULL, AV_LOG_ERROR,
                   "an unknown error -2, ret: %d - %s.\n", ret, errbuf);
            return -2;
        }
    }

    return 1;
}

static enum AVPixelFormat get_hw_format(
    AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts)
{
    const enum AVPixelFormat *p;

    for (p = pix_fmts; *p != -1; p++)
    {
        if (*p == AV_PIX_FMT_CUDA)
            return *p;
    }

    fprintf(stderr, "Failed to get HW surface format.\n");
    return AV_PIX_FMT_NONE;
}
