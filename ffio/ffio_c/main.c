#include "extractFrame.h"
#include "insertFrame.h"

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
    const char *sourceStreamPath = "rtmp://live.demo.uavcmlc.com:1935/live/DEV02005245?token=efa390262de0";

    clock_t start_time, end_time;

    av_log_set_callback(av_log_pyFFmpeg_callback);
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
    OutputStreamObj *curOutputStreamObj = newOutputStreamObj();

    ret = decodeOneFrame(curInputStreamObj);

    for (int j = 0; j < 30; j++){
        memset(curInputStreamObj->extractedFrame + j * 2000, '0', 1500);
        start_time = clock();

        ret = initializeOutputStream(
            curOutputStreamObj, sourceStreamPath,
            framerateNum, framerateDen, frameWidth, frameHeight, "ultrafast");

        end_time = clock();
        printf("initializing encoder cost time=%f, %d\n", (double)(end_time - start_time) / CLOCKS_PER_SEC, ret);

        for (int k = 0; k < 300; k++){
            ret = encodeOneFrame(
                curOutputStreamObj,
                curInputStreamObj->extractedFrame, curInputStreamObj->imageSize,
                0);
            if (ret != 0){
                break;
            }
        }

        printf("has started and encoded %d, sleep 30s.\n", j + 1);
        sleep(30);

        curOutputStreamObj = finalizeOutputStream(curOutputStreamObj);
        printf("has finalized %d, sleep 30s.\n", j + 1);
        sleep(30);
    }

    curInputStreamObj = finalizeInputStream(curInputStreamObj);
    free(curInputStreamObj);
    free(curOutputStreamObj);
    printf("start waiting to get frame.\n");
    return 1;
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

        if (count > 300)
        {
            break;
            // write this to finish the mp4 file format
            // av_write_trailer(curOutputStreamObj->outputFormatContext);
            // avio_closep(&curOutputStreamObj->outputFormatContext->pb);
            // return 1;
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

    // curInputStreamObj = finalizeInputStream(curInputStreamObj);
    // curOutputStreamObj = finalizeOutputStream(curOutputStreamObj);

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
