#include "extractFrame.h"
#include "insertFrame.h"
#include "Python.h"

#ifdef _WIN32
#define API __declspec(dllexport)
#else
#define API
#endif

// input stream functions
void *newInputStreamObject();
void *deleteInputStreamObject(void *inputStreamObj);

void *initializeInputStreamObject(
    void *inputStreamObj, const char *sourceStreamPath, int hw_flag,
    const bool enableShm, const char *shmName, const int shmSize, const int shmOffset
);
void *finalizeInputStreamObject(void *inputStreamObj);

int getInputStreamState(void *inputStreamObj);
int getInputVideoStreamWidth(void *inputStreamObj);
int getInputVideoStreamHeight(void *inputStreamObj);
int getInputVideoStreamFramerateNum(void *inputStreamObj);
int getInputVideoStreamFramerateDen(void *inputStreamObj);
float getInputVideoStreamAverageFPS(void *inputStreamObj);

int getFPS(void *inputStreamObj);

PyObject *decode1Frame(void *inputStreamObj);
bool getOneFrameToShm(void *inputStreamObj, int shmOffset);

// --------------------------------------------------------------
// output stream functions
void *newOutputStreamObject();
void *deleteOutputStreamObject(void *outputStreamObj);

void *initializeOutputStreamObject(
    void *outputStreamObj, const char *outputStreamPath,
    int framerateNum, int framerateDen, int frameWidth, int frameHeight,
    const char *preset, int hwFlag,
    const bool enableShm, const char *shmName, const int shmSize, const int shmOffset
);
void *finalizeOutputStreamObject(void *outputStreamObj);

int getOutputStreamState(void *outputStreamObj);

int encode1Frame(void *outputStreamObj, PyObject *PyRGBImage);
