#include "interfaceAPI.h"

void *newStreamObject() {
    StreamObj *curStreamObj = newStreamObj();
    if (curStreamObj == NULL)
    {
        return 0;
    }

    return curStreamObj;
}

void *deleteStreamObject(void *streamObj) {
    StreamObj *curStreamObj = (StreamObj *)streamObj;
    free(curStreamObj);
    curStreamObj = NULL;

    return NULL;
}

void *init(void *streamObj, const char *sourceStreamPath)
{
    int ret;
    StreamObj *curStreamObj = (StreamObj *)streamObj;

    // initialize log callback
    av_log_set_callback(av_log_pyFFmpeg_callback);

    ret = Init(curStreamObj, sourceStreamPath);
    if (ret != 0) // failed to open stream context
    {
        curStreamObj = unInit(curStreamObj);
    }

    return curStreamObj;
}

void *finalize(void *streamObj) {
    StreamObj *curStreamObj = (StreamObj *)streamObj;

    curStreamObj = unInit(curStreamObj);

    return curStreamObj;
}

int getWidth(void *streamObj)
{
    StreamObj *curStreamObj = (StreamObj *)streamObj;

    int width = curStreamObj->streamWidth;
    return width;
}

int getHeight(void *streamObj)
{
    StreamObj *curStreamObj = (StreamObj *)streamObj;

    int height = curStreamObj->streamHeight;
    return height;
}

int getStreamState(void *streamObj)
{
    StreamObj *curStreamObj = (StreamObj *)streamObj;

    int streamStateFlag = curStreamObj->streamStateFlag;
    return streamStateFlag;
}

float getAverageFPS(void *streamObj)
{
    StreamObj *curStreamObj = (StreamObj *)streamObj;

    int framerateNum = curStreamObj->framerateNum;
    int framerateDen = curStreamObj->framerateDen;

    float fps = (float)framerateNum / framerateDen;

    return fps;
}

PyObject *getOneFrame(void *streamObj)
{
    int ret;

    StreamObj *curStreamObj = (StreamObj *)streamObj;
    ret = decodeOneFrame(curStreamObj);

    if (ret == 0) {
        PyObject *outputImageBuffer = PyBytes_FromStringAndSize(
            (char *)curStreamObj->outputImage, curStreamObj->imageSize);

        return outputImageBuffer;
    } else {

        PyObject *outputNum = PyLong_FromSsize_t((Py_ssize_t)ret);
        return outputNum;
    }

    // return (char *)curStreamObj->outputImage; // return the result RGB image bytes
}
