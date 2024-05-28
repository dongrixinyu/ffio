#include "main1.h"

InputStreamObj *newInputStreamObj()
{
    InputStreamObj *inputStreamObj = NULL;
    inputStreamObj = (InputStreamObj *)malloc(sizeof(InputStreamObj));
    if (inputStreamObj == NULL)
    {
        return NULL;
    }

    inputStreamObj->inputStreamStateFlag = 0; // 0 means close, 1 means stream context has been opened
    inputStreamObj->inputVideoStreamID = -1;  // which stream index to parse in the video
    inputStreamObj->inputVideoStreamWidth = 0;
    inputStreamObj->inputVideoStreamHeight = 0;
    inputStreamObj->inputFramerateNum = 0; // to compute the fps of the stream, Num / Den
    inputStreamObj->inputFramerateDen = 0;
    inputStreamObj->imageSize = 0;

    inputStreamObj->inputFormatContext = NULL;
    inputStreamObj->videoCodecContext = NULL;
    inputStreamObj->videoPacket = NULL;
    inputStreamObj->videoFrame = NULL;
    inputStreamObj->videoRGBFrame = NULL;
    inputStreamObj->swsContext = NULL;

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

int convertYUV2RGB(
    AVFrame *inputFrame, AVFrame *RGBFrame, struct SwsContext *swsContextObj,
    AVCodecContext *videoCodecContext)
{
    int ret;

    if (swsContextObj == NULL)
    {
        // replace SWS_FAST_BILINEAR with other options SWS_BICUBIC
        swsContextObj = sws_getCachedContext(
            swsContextObj,
            videoCodecContext->width, videoCodecContext->height, videoCodecContext->pix_fmt,
            videoCodecContext->width, videoCodecContext->height, OUTPUT_RGB_PIX_FMT,
            SWS_BICUBIC, NULL, NULL, NULL);
        if (swsContextObj == NULL)
            return 0;
    }

    RGBFrame->width = videoCodecContext->width;
    RGBFrame->height = videoCodecContext->height;
    RGBFrame->format = OUTPUT_RGB_PIX_FMT;

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

    return 1;
}

/**
 *  read the video context info, including format context and codec context.
 *
 *  ret: int
 *      0 means successfully start a video stream
 *      1 means failing to open the input stream
 *      2 means can not find the stream info
 *      3 means can not process params to context
 *      4 means can not open codec context
 *      5 means failing to allocate memory for swsContext
 */
int initializeInputStream(InputStreamObj *inputStreamObj, const char *streamPath)
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

    inputStreamObj->videoCodecContext = avcodec_alloc_context3(NULL);
    ret = avcodec_parameters_to_context(
        inputStreamObj->videoCodecContext,
        inputStreamObj->inputFormatContext->streams[videoStreamID]->codecpar);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_INFO, "%s\n", "can not process params to context.");
        return 3;
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

    if (inputStreamObj->swsContext == NULL)
    {
        inputStreamObj->swsContext = sws_getCachedContext(
            inputStreamObj->swsContext,
            inputStreamObj->videoCodecContext->width, inputStreamObj->videoCodecContext->height,
            inputStreamObj->videoCodecContext->pix_fmt,
            inputStreamObj->videoCodecContext->width, inputStreamObj->videoCodecContext->height, OUTPUT_RGB_PIX_FMT,
            SWS_BICUBIC, NULL, NULL, NULL);
        if (inputStreamObj->swsContext == NULL)
            return 5; // failed to allocate memory for swsContext
    }

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

    if (inputStreamObj->videoPacket)
    {
        av_packet_free(&(inputStreamObj->videoPacket));
        inputStreamObj->videoPacket = NULL;
    }

    if (inputStreamObj->videoCodecContext)
    {
        avcodec_free_context(&(inputStreamObj->videoCodecContext));
        inputStreamObj->videoCodecContext = NULL;
    }

    if (inputStreamObj->inputFormatContext)
    {
        avformat_close_input(&(inputStreamObj->inputFormatContext));
        inputStreamObj->inputFormatContext = NULL;
    }

    free(inputStreamObj->extractedFrame);
    inputStreamObj->extractedFrame = NULL;

    inputStreamObj->inputVideoStreamID = -1; // which stream index to parse in the video
    inputStreamObj->inputVideoStreamWidth = 0;
    inputStreamObj->inputVideoStreamHeight = 0;
    inputStreamObj->inputFramerateNum = 0;
    inputStreamObj->inputFramerateDen = 0;
    inputStreamObj->imageSize = 0;

    memset(inputStreamObj->inputStreamPath, '0', 300);

    av_log(NULL, AV_LOG_INFO, "%s", "finished unref this video stream context\n");

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

        // av_log(NULL, AV_LOG_INFO, "read one packet %d.\n", inputStreamObj->streamEnd);

        inputStreamObj->clicker->lasttime = time(NULL);                                       // get the currect time before av_read_frame
        ret = av_read_frame(inputStreamObj->inputFormatContext, inputStreamObj->videoPacket); // read a packet
        // av_log(NULL, AV_LOG_DEBUG, "%s", "### successfully read one packet.\n");
        // log_packet(inputStreamObj->inputFormatContext, inputStreamObj->videoPacket, "in");

        if (inputStreamObj->videoPacket->stream_index != inputStreamObj->inputVideoStreamID)
        {
            // filter unrelated packet
            av_packet_unref(inputStreamObj->videoPacket);

            if (ret == -5) // -5 means timeout, need to exit this function
            {
                char errbuf[200];
                av_strerror(ret, errbuf, sizeof(errbuf));
                av_log(NULL, AV_LOG_WARNING,
                       "timeout to waiting for the next packet data, ret: %d - %s.\n", ret, errbuf);
                return -5;
            }
            else if (ret < 0)
            {
                char errbuf[200];
                av_strerror(ret, errbuf, sizeof(errbuf));
                av_log(NULL, AV_LOG_WARNING,
                       "Probably Packet mismatch, unable to seek the next packet. ret: %d - %s.\n",
                       ret, errbuf);
                return -4;
            }
            else
            {
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
                    ret = convertYUV2RGB(
                        inputStreamObj->videoFrame, inputStreamObj->videoRGBFrame, inputStreamObj->swsContext,
                        inputStreamObj->videoCodecContext);

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
                    av_log(NULL, AV_LOG_ERROR, "%s", "an unknown error.\n");
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
                    ret = convertYUV2RGB(
                        inputStreamObj->videoFrame, inputStreamObj->videoRGBFrame,
                        inputStreamObj->swsContext,
                        inputStreamObj->videoCodecContext);

                    // end_time = clock();
                    // av_log(NULL, AV_LOG_INFO, "read one frame cost time=%f\n",
                    //     (double)(end_time - start_time) / CLOCKS_PER_SEC);
                    // inputStreamObj->streamEnd = 1; //  to the end

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

int convertRGB2YUV(
    struct SwsContext *RGB2YUVContext, AVCodecContext *videoCodecContext,
    AVFrame *YUVFrame, AVFrame *RGBFrame,
    unsigned char *RGBImage, int RGBImageSize)
{
    int ret;

    ret = av_frame_make_writable(RGBFrame);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_INFO, "av_frame_make_writable failed.\n");
        return -1;
    }

    memcpy(RGBFrame->data[0], RGBImage, RGBImageSize);

    ret = sws_scale(RGB2YUVContext, (const uint8_t *const)RGBFrame->data, RGBFrame->linesize, 0, YUVFrame->height, YUVFrame->data, YUVFrame->linesize);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_INFO, "Error convert pix format of frame: %d\n", ret);
        return 0;
    }

    // av_frame_unref(RGBFrame);

    return 1;
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

    outputStreamObj->outputframeNum = 1;        // this is used to record the encoded frame number.
    outputStreamObj->outputStreamStateFlag = 0; // to indicate that if the stream has been opened successfully

    outputStreamObj->outputVideoStreamID = -1; // which stream index to parse in the video, default 0.
    outputStreamObj->outputVideoStreamWidth = 0;
    outputStreamObj->outputVideoStreamHeight = 0;

    outputStreamObj->outputvideoFramerateNum = 0; // to compute the fps of the stream, duration / Den
    outputStreamObj->outputvideoFramerateDen = 0;

    // encoder info and remux info
    outputStreamObj->outputFormatContext = NULL;
    outputStreamObj->videoEncoderContext = NULL;
    outputStreamObj->videoEncoder = NULL;
    outputStreamObj->videoEncoderFrame = NULL;
    outputStreamObj->videoPacketOut = NULL;

    // rgb -> yuv format
    outputStreamObj->RGB2YUVContext = NULL;

    return outputStreamObj;
}

/**
 *  initialize video encoder context and format context info
 *
 *  ret: int
 *      0 means successfully start a video stream
 *      1 means can not denote the right output format
 *      2 means failed to allocate memory for `avformat_alloc_output_context2`
 *      3 means avcodec_new_stream failed.
 *      4 means avcodec_copy_context failed.
 *
 */
int initializeOutputStream(
    OutputStreamObj *outputStreamObj,
    const char *outputStreamPath,
    int framerateNum, int framerateDen, int frameWidth, int frameHeight)
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
    else if (strstr(outputStreamPath, ".flv"))
    {
        format_name = "flv";
    }
    else
    {
        // 1 means the outputStreamPath is illegal.
        return 1;
    }

    // step 1: find encoder, initialize encoder context
    if (outputStreamObj->videoEncoderContext == NULL)
    {
        // initialize codec object
        const char *codec_name = "libx264";
        outputStreamObj->videoEncoder = avcodec_find_encoder_by_name(codec_name);
        if (!outputStreamObj->videoEncoder)
        {
            fprintf(stderr, "Codec '%s' not found\n", codec_name);
            exit(1);
        }

        // initialize encoder context
        outputStreamObj->videoEncoderContext = avcodec_alloc_context3(
            outputStreamObj->videoEncoder);

        outputStreamObj->videoEncoderContext->codec_tag = 0;
        outputStreamObj->videoEncoderContext->codec_id = AV_CODEC_ID_H264;
        outputStreamObj->videoEncoderContext->codec_type = AVMEDIA_TYPE_VIDEO;
        outputStreamObj->videoEncoderContext->bit_rate = 400000;

        outputStreamObj->videoEncoderContext->gop_size = 12;
        outputStreamObj->videoEncoderContext->max_b_frames = 1;
        // outputStreamObj->videoEncoderContext->profile = FF_PROFILE_H264_HIGH;

        // get params from AVFormatContext
        // ensure frame rate of output is equal to input frame rate.
        // outputStreamObj->videoEncoderContext->framerate = inputStreamObj->inputFormatContext->streams[inputStreamObj->inputVideoStreamID]->avg_frame_rate;
        // set framerate and timebase
        outputStreamObj->outputvideoFramerateNum = framerateNum;
        outputStreamObj->outputvideoFramerateDen = framerateDen;
        outputStreamObj->videoEncoderContext->framerate.num = framerateNum;
        outputStreamObj->videoEncoderContext->framerate.den = framerateDen;

        outputStreamObj->videoEncoderContext->time_base = av_inv_q(outputStreamObj->videoEncoderContext->framerate);
        outputStreamObj->outputVideoTimebase = outputStreamObj->videoEncoderContext->time_base;
        // outputStreamObj->videoEncoderContext->width = inputStreamObj->inputFormatContext->streams[inputStreamObj->inputVideoStreamID]->codecpar->width;
        // outputStreamObj->videoEncoderContext->height = inputStreamObj->inputFormatContext->streams[inputStreamObj->inputVideoStreamID]->codecpar->height;
        outputStreamObj->videoEncoderContext->width = frameWidth;
        outputStreamObj->videoEncoderContext->height = frameHeight;

        // // get params from AVFrame
        // outputStreamObj->videoEncoderContext->pix_fmt = inputStreamObj->videoFrame->format;
        outputStreamObj->videoEncoderContext->pix_fmt = AV_PIX_FMT_YUV420P;
        // outputStreamObj->videoEncoderContext->color_range = inputStreamObj->videoFrame->color_range;
        // outputStreamObj->videoEncoderContext->color_primaries = inputStreamObj->videoFrame->color_primaries;
        // outputStreamObj->videoEncoderContext->color_trc = inputStreamObj->videoFrame->color_trc;
        // outputStreamObj->videoEncoderContext->colorspace = inputStreamObj->videoFrame->colorspace;
        // outputStreamObj->videoEncoderContext->chroma_sample_location = inputStreamObj->videoFrame->chroma_location;

        if (outputStreamObj->videoEncoder->id == AV_CODEC_ID_H264)
            av_opt_set(outputStreamObj->videoEncoderContext->priv_data, "preset", "slow", 0);

        /* 注意，这个 field_order 不同的视频的值是不一样的，这里我写死了。
         * 因为 本文的视频就是 AV_FIELD_PROGRESSIVE
         * 生产环境要对不同的视频做处理的
         */
        // outputStreamObj->videoEncoderContext->field_order = AV_FIELD_PROGRESSIVE;
        // AV_FIELD_UNKNOWN;
        // AV_FIELD_PROGRESSIVE;

        if (strcmp(format_name, "mp4") == 0)
        {
        }
        else if (strcmp(format_name, "flv") == 0)
        {

            av_log(NULL, AV_LOG_INFO, "%s", "set video GLOBAL_HEADER\n");
            outputStreamObj->videoEncoderContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        // AVDictionary *codec_options = NULL;
        // av_dict_set(&codec_options, "profile", "high", 0);
        // av_dict_set(&codec_options, "preset", "superfast", 0);
        // av_dict_set(&codec_options, "tune", "zerolatency", 0);

        ret = avcodec_open2(
            outputStreamObj->videoEncoderContext,
            outputStreamObj->videoEncoder, NULL); // &codec_options
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_INFO, "avcodec_open2 failed %d.\n", ret);
            return ret;
        }
    }

    // step 2: initialize packet and frame
    outputStreamObj->videoPacketOut = av_packet_alloc();
    outputStreamObj->videoEncoderFrame = av_frame_alloc();
    outputStreamObj->videoRGBFrame = av_frame_alloc();

    // initialize RGB -> YUV context
    // replace SWS_FAST_BILINEAR with other options SWS_BICUBIC
    outputStreamObj->RGB2YUVContext = sws_getCachedContext(
        outputStreamObj->RGB2YUVContext,
        outputStreamObj->videoEncoderContext->width,
        outputStreamObj->videoEncoderContext->height, OUTPUT_RGB_PIX_FMT,
        outputStreamObj->videoEncoderContext->width,
        outputStreamObj->videoEncoderContext->height,
        outputStreamObj->videoEncoderContext->pix_fmt,
        SWS_BICUBIC, NULL, NULL, NULL);
    if (outputStreamObj->RGB2YUVContext == NULL)
        return -10;

    // initialize frame info
    outputStreamObj->outputVideoStreamWidth = outputStreamObj->videoEncoderContext->width;
    outputStreamObj->outputVideoStreamHeight = outputStreamObj->videoEncoderContext->height;

    av_log(NULL, AV_LOG_INFO, "pix format: %d\n", AV_PIX_FMT_YUV420P);
    outputStreamObj->videoEncoderFrame->format = outputStreamObj->videoEncoderContext->pix_fmt;
    outputStreamObj->videoEncoderFrame->width = outputStreamObj->videoEncoderContext->width;
    outputStreamObj->videoEncoderFrame->height = outputStreamObj->videoEncoderContext->height;
    ret = av_frame_get_buffer(outputStreamObj->videoEncoderFrame, 0);
    if (ret < 0)
    {
        fprintf(stderr, "Could not allocate the video frame data\n");
        return 9;
    }

    // set params for videoRGBFrame
    outputStreamObj->videoRGBFrame->format = OUTPUT_RGB_PIX_FMT;
    outputStreamObj->videoRGBFrame->width = outputStreamObj->videoEncoderContext->width;
    outputStreamObj->videoRGBFrame->height = outputStreamObj->videoEncoderContext->height;
    ret = av_frame_get_buffer(outputStreamObj->videoRGBFrame, 0);
    if (ret < 0)
    {
        fprintf(stderr, "Could not allocate the video frame data\n");
        return 9;
    }

    // step 3: initialize output remux object, open it and then write header
    ret = avformat_alloc_output_context2(
        &(outputStreamObj->outputFormatContext), NULL, format_name, outputStreamPath);
    if (!outputStreamObj->outputFormatContext)
    {
        av_log(NULL, AV_LOG_INFO, "avformat_alloc_output_context2 failed %d \n", AVERROR(ENOMEM));
        return 2; // out of memory
    }

    // add a video stream to the context, streamID default by 0
    outputStreamObj->outputVideoStreamID = 0;

    // only build the video stream for the output
    for (int i = 0; i < 1; i++)
    {
        outputStreamObj->outputVideoStream = avformat_new_stream(
            outputStreamObj->outputFormatContext, NULL);
        if (!outputStreamObj->outputVideoStream)
        {
            av_log(NULL, AV_LOG_ERROR, "avformat_new_stream failed %d.\n", 3);
            ret = AVERROR_UNKNOWN;
            ret = 3;
            goto end;
        }

        // copy input stream params to output streams
        // ret = avcodec_parameters_copy(
        //     outputStreamObj->outputVideoStream->codecpar,
        //     input_codecpar);
        ret = avcodec_parameters_from_context(
            outputStreamObj->outputVideoStream->codecpar,
            outputStreamObj->videoEncoderContext);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "avcodec_parameters_copy failed %d.\n", ret);
            goto end;
        }
        // av_dict_copy(
        //     &(outputStreamObj->outputVideoStream)->metadata,
        //     inputStreamObj->inputFormatContext->streams[inputStreamObj->inputVideoStreamID]->metadata, 0);


        // if (outputStreamObj->outputFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
        // {
        //     outputStreamObj->outputVideoStream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        // }
        outputStreamObj->outputVideoStream->codecpar->codec_tag = 0;
    }

    av_dump_format(outputStreamObj->outputFormatContext, 0, outputStreamObj->outputStreamPath, 1);

    // open output file or stream format context
    if (!(outputStreamObj->outputFormatContext->flags & AVFMT_NOFILE))
    {
        ret = avio_open2(
            &(outputStreamObj->outputFormatContext->pb),
            outputStreamObj->outputStreamPath,
            AVIO_FLAG_READ_WRITE,
            // &(outputStreamObj->outputFormatContext->interrupt_callback),
            NULL,
            NULL);
        if (ret < 0)
        {
            char errbuf[200];
            av_strerror(ret, errbuf, sizeof(errbuf));
            av_log(NULL, AV_LOG_INFO, "avio_open2 failed %d - %s.\n", ret, errbuf);
            return ret;
        }
    }

    ret = avformat_write_header(outputStreamObj->outputFormatContext, NULL);
    if (ret < 0)
    {
        char errbuf[200];
        av_strerror(ret, errbuf, sizeof(errbuf));
        av_log(NULL, AV_LOG_INFO, "avformat_write_header failed %d - %s.\n", ret, errbuf);
        return ret;
    }

    outputStreamObj->outputStreamStateFlag = 1;

    return 0;

end:
    avformat_free_context(outputStreamObj->outputFormatContext);
    outputStreamObj->outputFormatContext = NULL;

    return ret;
}

/**
 * to finalize the output stream context
 *
 * ret: int
 *     0 means encoding a frame successfully.
 *
 *
 */
OutputStreamObj *finalizeOutputStream(OutputStreamObj *outputStreamObj)
{
    // set stream state to 0, which means the stream context has been closed.
    outputStreamObj->outputStreamStateFlag = 0;

    if (outputStreamObj->videoRGBFrame)
    {
        av_frame_free(&(outputStreamObj->videoRGBFrame));
        outputStreamObj->videoRGBFrame = NULL;
    }
}

/**
 * to encode one frame to the output stream
 *
 * ret: int
 *     0 means encoding a frame successfully.
 *
 *
 */
int encodeOneFrame(
    OutputStreamObj *outputStreamObj,
    unsigned char *RGBImage, int RGBImageSize,
    int end_of_stream)
{
    int ret;
    clock_t start_time, end_time;

    // make frame data
    ret = av_frame_make_writable(outputStreamObj->videoEncoderFrame);

    // this code below is to construct a fake image for testing.
    // it comes from the example of FFmpeg.
    if (0)
    {
        int x, y;
        for (y = 0; y < outputStreamObj->videoEncoderFrame->height; y++)
        {
            for (x = 0; x < outputStreamObj->videoEncoderFrame->width; x++)
            {
                outputStreamObj->videoEncoderFrame->data[0][y * outputStreamObj->videoEncoderFrame->linesize[0] + x] = x + y + outputStreamObj->outputframeNum * 3;
            }
        }
        for (y = 0; y < outputStreamObj->videoEncoderFrame->height / 2; y++)
        {
            for (x = 0; x < outputStreamObj->videoEncoderFrame->width / 2; x++)
            {
                outputStreamObj->videoEncoderFrame->data[1][y * outputStreamObj->videoEncoderFrame->linesize[1] + x] = 128 + y + outputStreamObj->outputframeNum * 2;
                outputStreamObj->videoEncoderFrame->data[2][y * outputStreamObj->videoEncoderFrame->linesize[2] + x] = 64 + x + outputStreamObj->outputframeNum * 5;
            }
        }
    }

    // copy the input image to output in yuv format
    // if (0) {
    //     for (int i = 0; i <= 2; i++)
    //     {
    //         if (i == 0)
    //         {
    //             memcpy(
    //                 outputStreamObj->videoEncoderFrame->data[i],
    //                 inputStreamObj->videoFrame->data[i],
    //                 outputStreamObj->videoEncoderFrame->linesize[i] * outputStreamObj->outputVideoStreamHeight);
    //         }
    //         else
    //         {
    //             memcpy(
    //                 outputStreamObj->videoEncoderFrame->data[i],
    //                 inputStreamObj->videoFrame->data[i],
    //                 (outputStreamObj->videoEncoderFrame->linesize[i] * outputStreamObj->outputVideoStreamHeight) / 2);
    //         }
    //     }
    // }

    // convert the input RGB to YUV format
    ret = convertRGB2YUV(
        outputStreamObj->RGB2YUVContext,
        outputStreamObj->videoEncoderContext,
        outputStreamObj->videoEncoderFrame, outputStreamObj->videoRGBFrame,
        RGBImage, RGBImageSize);

    // 1000 means the default time_base of the video stream, namely 1000ms
    // you can check it via outputStreamObj->outputVideoStream->time_base
    int64_tt curPTS = (outputStreamObj->outputframeNum * 1000 * outputStreamObj->outputvideoFramerateDen) / outputStreamObj->outputvideoFramerateNum;
    // outputStreamObj->videoEncoderFrame->pts = inputStreamObj->videoFrame->pts;
    outputStreamObj->videoEncoderFrame->pts = curPTS;
    // av_log(NULL, AV_LOG_INFO, "---> frame->pts: %d\n", outputStreamObj->videoEncoderFrame->pts);
    // outputStreamObj->videoEncoderFrame->pkt_dts = inputStreamObj->videoFrame->pts;
    // outputStreamObj->videoEncoderFrame->pkt_pts = inputStreamObj->videoFrame->pts;

    // send the frame to the encoder
    ret = avcodec_send_frame(
        outputStreamObj->videoEncoderContext,
        outputStreamObj->videoEncoderFrame);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_INFO, "avcodec_send_frame failed %d %d.\n", ret, AVERROR(EINVAL));
        return ret;
    }
    outputStreamObj->outputframeNum += 1; // count number + 1

    while (ret >= 0)
    {
        ret = avcodec_receive_packet(
            outputStreamObj->videoEncoderContext, outputStreamObj->videoPacketOut);
        if (ret == AVERROR(EAGAIN))
        { // -11 means we need to send frame again.
            break;
        }
        else if (ret == AVERROR_EOF) // -541478725 means end of file
        {
            av_log(NULL, AV_LOG_INFO, "avcodec_receive_packet to the end %d\n", ret);
            return 9;
        }
        else if (ret < 0)
        {
            av_log(NULL, AV_LOG_INFO, "avcodec_receive_packet failed %d.\n", ret);
            return ret;
        }

        // av_log(NULL, AV_LOG_DEBUG, "---> Receive packet %3" PRId64 " (size=%5d)\n",
        //        outputStreamObj->videoPacketOut->pts, outputStreamObj->videoPacketOut->size);
        // log_packet(outputStreamObj->outputFormatContext, outputStreamObj->videoPacketOut, "out_1");

        // set the stream_index of one AVPacket
        outputStreamObj->videoPacketOut->stream_index = outputStreamObj->outputVideoStream->index;

        // convert the timebase of AVPacket
        // ret = av_rescale_q(
        //     1,
        //     outputStreamObj->inputVideoTimebase,
        //     outputStreamObj->outputVideoStream->time_base);
        // outputStreamObj->videoPacketOut->pts += ret;
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

        // AVStream *inputStream = inputStreamObj->inputFormatContext->streams[inputStreamObj->inputVideoStreamID];

        // AVRational input_time_base = {1, 500};
        // av_packet_rescale_ts(
        //     outputStreamObj->videoPacketOut,
        //     // inputStream->time_base,
        //     input_time_base,
        //     outputStreamObj->outputVideoStream->time_base);

        outputStreamObj->videoPacketOut->pos = -1;

        // log_packet(outputStreamObj->outputFormatContext, outputStreamObj->videoPacketOut, "out_2");
        // av_log(NULL, AV_LOG_INFO, "packet info pts: %d, dts: %d, size: %d\n",
        //        outputStreamObj->videoPacketOut->pts,
        //        outputStreamObj->videoPacketOut->dts,
        //        outputStreamObj->videoPacketOut->size);

        start_time = clock();
        ret = av_interleaved_write_frame(
            outputStreamObj->outputFormatContext, outputStreamObj->videoPacketOut);
        end_time = clock();
        av_log(NULL, AV_LOG_INFO, "encode one frame cost time = %f, %d\n",
               (double)(end_time - start_time) / CLOCKS_PER_SEC,
               outputStreamObj->outputframeNum);

        if (ret < 0)
        {
            char errbuf[200];
            av_strerror(ret, errbuf, sizeof(errbuf));
            av_log(NULL, AV_LOG_INFO, "av_interleaved_write_frame failed %d - %s.\n", ret, errbuf);

            return ret;
        }
        av_packet_unref(outputStreamObj->videoPacketOut);

        if (outputStreamObj->outputframeNum % PRINT_FRAME_GAP == 0)
        {
            av_log(NULL, AV_LOG_INFO, "Successfully send one frame to %s.\n",
                   outputStreamObj->outputStreamPath);
        }
    }

    // frame -> encoder context
    // if (end_of_stream == 1) {
    //     ret = avcodec_send_frame(
    //         outputStreamObj->videoEncoderContext, NULL);
    // } else {
    //     ret = avcodec_send_frame(
    //         outputStreamObj->videoEncoderContext, inputStreamObj->videoFrame);
    // }
    // if (ret < 0)
    // {
    //     av_log(NULL, AV_LOG_INFO, "avcodec_send_frame failed %d %d.\n", ret, AVERROR(EINVAL));
    //     return ret;
    // }

    return 0;
}

int main()
{
    int err;
    int ret;
    // avformat_network_init();

    // const char *sourceStreamPath = "rtmp://live.demo.uavcmlc.com:1935/live/DEV02001270?token=03c7986c15e0";
    // const char *sourcePath = "/home/cuichengyu/github/pyFFmpeg/pyFFmpeg/output.mp4";
    // const char *sourcePath = "/home/cuichengyu/pyffmpeg_workspace/fast_moving_output.mp4";
    const char *sourcePath = "rtmp://live.demo.uavcmlc.com:1935/live/DEV02003179?token=8622d43632a1";
    // const char *sourceStreamPath = "rtmp://live.demo.uavcmlc.com:1935/live/DEV02001270?token=03c7986c15e0";
    // const char *sourceStreamPath = "/home/cuichengyu/github/pyFFmpeg/pyFFmpeg/res1.mp4";
    // const char *sourceStreamPath = "rtmp://192.168.35.30:30107/live/cuichengyu3";
    const char *sourceStreamPath = "rtmp://live.demo.uavcmlc.com:1935/live/DEV02005245?token=efa390262de0";

    clock_t start_time, end_time;

    av_log_set_callback(av_log_ffio_callback);
    printf("start waiting to new a stream obj.\n");
    // sleep(60);

    // build a new struct object
    start_time = clock();
    InputStreamObj *curInputStreamObj = newInputStreamObj();
    ret = initializeInputStream(curInputStreamObj, sourcePath); // initialize a new stream
    end_time = clock();
    printf("initializing decoder cost time=%f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);
    if (ret != 0)
    {
        return 1;
    }
    sleep(1);

    int framerateNum = curInputStreamObj->inputFormatContext->streams[1]->avg_frame_rate.num;
    int framerateDen = curInputStreamObj->inputFormatContext->streams[1]->avg_frame_rate.den;
    int frameWidth = curInputStreamObj->inputVideoStreamWidth;
    int frameHeight = curInputStreamObj->inputVideoStreamHeight;

    start_time = clock();
    OutputStreamObj *curOutputStreamObj = newOutputStreamObj();
    ret = initializeOutputStream(
        curOutputStreamObj, sourceStreamPath,
        framerateNum, framerateDen, frameWidth, frameHeight);

    end_time = clock();
    printf("initializing encoder cost time=%f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    printf("start waiting to get frame.\n");
    // sleep(60);
    if (ret != 0)
    {
        return 1;
    }

    start_time = clock();
    int count = 0;
    int file_end = 0;
    while (1)
    {
        ret = decodeOneFrame(curInputStreamObj);
        // if (count > 100)
        // {
        //     break;
        // }

        count++;
        if (ret == 1)
        {
            // which means to the end of the file
            file_end = ret;
        }
        if (ret == -5)
        {
            av_write_trailer(curOutputStreamObj->outputFormatContext);
            avio_closep(&curOutputStreamObj->outputFormatContext->pb);
            return 1;
        }

        ret = encodeOneFrame(
            curOutputStreamObj,
            curInputStreamObj->extractedFrame, curInputStreamObj->imageSize,
            file_end);

        if (count > 10000)
        {
            // write this to finish the mp4 file format
            av_write_trailer(curOutputStreamObj->outputFormatContext);
            avio_closep(&curOutputStreamObj->outputFormatContext->pb);
            return 1;
        }

        if (ret == 10)
        {
            printf("timeout to get a frame.\n");
            // break;
        }

        if (file_end == 1)
        {
            return 0;
        }
    }

    end_time = clock();
    printf("read one frame cost time=%f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    curInputStreamObj = finalizeInputStream(curInputStreamObj);

    printf("finished 1st period\n");
    sleep(60);
    // ret = save_rgb_to_file(curInputStreamObj, 0);

    ret = initializeInputStream(curInputStreamObj, sourceStreamPath); // initialize a new stream
    end_time = clock();
    printf("cur time: %ld\n", end_time);
    printf("initializing cost time=%f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    start_time = clock();
    while (1)
    {
        ret = decodeOneFrame(curInputStreamObj);
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
    curInputStreamObj = finalizeInputStream(curInputStreamObj);

    return 0;
}
