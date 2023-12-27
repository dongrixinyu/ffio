#include "interfaceAPI.h"

void *newInputStreamObject() {
    InputStreamObj *curInputStreamObj = newInputStreamObj();
    if (curInputStreamObj == NULL)
    {
        return 0;
    }

    return curInputStreamObj;
}

void *deleteInputStreamObject(void *inputStreamObj)
{
    InputStreamObj *curInputStreamObj = (InputStreamObj *)inputStreamObj;
    free(curInputStreamObj);
    curInputStreamObj = NULL;

    return NULL;
}

void *initializeInputStreamObject(void *inputStreamObj, const char *sourceStreamPath)
{
    int ret;
    InputStreamObj *curInputStreamObj = (InputStreamObj *)inputStreamObj;

    // initialize log callback
    av_log_set_callback(av_log_pyFFmpeg_callback);

    ret = initializeInputStream(curInputStreamObj, sourceStreamPath);
    if (ret != 0) // failed to open stream context
    {
        curInputStreamObj = finalizeInputStream(curInputStreamObj);
    }

    return curInputStreamObj;
}

void *finalizeInputStreamObject(void *inputStreamObj) {
    InputStreamObj *curInputStreamObj = (InputStreamObj *)inputStreamObj;

    curInputStreamObj = finalizeInputStream(curInputStreamObj);

    return curInputStreamObj;
}

int getInputVideoStreamWidth(void *inputStreamObj)
{
    InputStreamObj *curInputStreamObj = (InputStreamObj *)inputStreamObj;

    int width = curInputStreamObj->inputVideoStreamWidth;
    return width;
}

int getInputVideoStreamHeight(void *inputStreamObj)
{
    InputStreamObj *curInputStreamObj = (InputStreamObj *)inputStreamObj;

    int height = curInputStreamObj->inputVideoStreamHeight;
    return height;
}

int getInputStreamState(void *inputStreamObj)
{
    InputStreamObj *curInputStreamObj = (InputStreamObj *)inputStreamObj;

    int streamStateFlag = curInputStreamObj->inputStreamStateFlag;
    return streamStateFlag;
}

float getInputVideoStreamAverageFPS(void *inputStreamObj)
{
    InputStreamObj *curInputStreamObj = (InputStreamObj *)inputStreamObj;

    int framerateNum = curInputStreamObj->inputFramerateNum;
    int framerateDen = curInputStreamObj->inputFramerateDen;

    float fps = (float)framerateNum / framerateDen;

    return fps;
}

int getInputVideoStreamFramerateNum(void *inputStreamObj)
{
    InputStreamObj *curInputStreamObj = (InputStreamObj *)inputStreamObj;

    int framerateNum = curInputStreamObj->inputFramerateNum;

    return framerateNum;
}

int getInputVideoStreamFramerateDen(void *inputStreamObj)
{
    InputStreamObj *curInputStreamObj = (InputStreamObj *)inputStreamObj;

    int framerateDen = curInputStreamObj->inputFramerateDen;

    return framerateDen;
}

PyObject *decode1Frame(void *inputStreamObj)
{
    int ret;

    InputStreamObj *curInputStreamObj = (InputStreamObj *)inputStreamObj;

    // this code below it for test the speed of decodeOneFrame
    // struct timeval start_tv, end_tv;
    // gettimeofday(&start_tv, NULL);
    ret = decodeOneFrame(curInputStreamObj);
    // gettimeofday(&end_tv, NULL);
    // av_log(NULL, AV_LOG_INFO, "decodeOneFrame in C cost time = %.5f, %d\n",
    //        (((double)end_tv.tv_sec + (double)end_tv.tv_usec / 1000000) - ((double)start_tv.tv_sec + (double)start_tv.tv_usec / 1000000)),
    //        curInputStreamObj->frameNum);

    if (ret == 0) {
        // get the RGB frame correctly
        PyObject *outputImageBuffer = PyBytes_FromStringAndSize(
            (char *)curInputStreamObj->extractedFrame, curInputStreamObj->imageSize);

        return outputImageBuffer;
    } else {

        PyObject *outputNum = PyLong_FromSsize_t((Py_ssize_t)ret);
        return outputNum;
    }

    // return (char *)curStreamObj->outputImage; // return the result RGB image bytes
}

void *newOutputStreamObject() {
    OutputStreamObj *curOutputStreamObj = newOutputStreamObj();
    if (curOutputStreamObj == NULL)
    {
        return 0;
    }

    return curOutputStreamObj;
}

void *deleteOutputStreamObject(void *outputStreamObj) {
    InputStreamObj *curOutputStreamObj = (InputStreamObj *)outputStreamObj;
    free(curOutputStreamObj);
    curOutputStreamObj = NULL;

    return NULL;
}

void *initializeOutputStreamObject(
    void *outputStreamObj, const char *outputStreamPath,
    int framerateNum, int framerateDen, int frameWidth, int frameHeight)
{
    int ret;
    OutputStreamObj *curOutputStreamObj = (OutputStreamObj *)outputStreamObj;

    // initialize log callback
    av_log_set_callback(av_log_pyFFmpeg_callback);

    ret = initializeOutputStream(
        curOutputStreamObj, outputStreamPath,
        framerateNum, framerateDen, frameWidth, frameHeight);
    if (ret != 0) // failed to open stream context
    {
        curOutputStreamObj = finalizeOutputStream(curOutputStreamObj);
    }

    return curOutputStreamObj;
}

void *finalizeOutputStreamObject(void *outputStreamObj){
    OutputStreamObj *curOutputStreamObj = (OutputStreamObj *)outputStreamObj;

    curOutputStreamObj = finalizeOutputStream(curOutputStreamObj);

    return curOutputStreamObj;
}

int getOutputStreamState(void *outputStreamObj) {
    OutputStreamObj *curOutputStreamObj = (OutputStreamObj *)outputStreamObj;

    int streamStateFlag = curOutputStreamObj->outputStreamStateFlag;
    return streamStateFlag;
}

/**
 *
 *
 *
 *
 */
int encode1Frame(void *outputStreamObj, PyObject *PyRGBImage)
{
    int ret;

    OutputStreamObj *curOutputStreamObj = (OutputStreamObj *)outputStreamObj;
    char *RGBImage = PyBytes_AsString(PyRGBImage);
    Py_ssize_t RGBImageSize = PyBytes_GET_SIZE(PyRGBImage);

    // this code below it for test the speed of decodeOneFrame
    // struct timeval start_tv, end_tv;
    // gettimeofday(&start_tv, NULL);
    ret = encodeOneFrame(curOutputStreamObj, (unsigned char *)RGBImage, (int)RGBImageSize,
                         0);
    // gettimeofday(&end_tv, NULL);
    // av_log(NULL, AV_LOG_INFO, "encodeOneFrame in C cost time = %.5f, %d\n",
    //        (((double)end_tv.tv_sec + (double)end_tv.tv_usec / 1000000) - ((double)start_tv.tv_sec + (double)start_tv.tv_usec / 1000000)),
    //        curOutputStreamObj->outputframeNum);

    return ret;
}
