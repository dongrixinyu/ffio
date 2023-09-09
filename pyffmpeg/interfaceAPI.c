#include "interfaceAPI.h"


void *init(const char *sourceStreamPath) {
    int ret;

    StreamObj *curStreamObj = newStreamObj();
    if (curStreamObj == NULL) {
        return 0;
    }
    ret = Init(curStreamObj, sourceStreamPath);

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

PyObject *getOneFrame(void *streamObj)
{
    int ret;

    StreamObj *curStreamObj = (StreamObj *)streamObj;
    ret = decodeOneFrame(curStreamObj);

    PyObject *outputImageBuffer = PyBytes_FromStringAndSize(
        (char *) curStreamObj->outputImage, curStreamObj->imageSize);

    return outputImageBuffer;
    // return curStreamObj->outputImage; // return the result RGB image bytes
}
