#include "extractFrame.h"
#include "Python.h"

#ifdef _WIN32
#define API __declspec(dllexport)
#else
#define API
#endif

void *newStreamObject();

void *deleteStreamObject(void *streamObj);

void *init(void *streamObj, const char *sourceStreamPath, const bool enableShm,
           const char *shmName, const int shmSize, const int shmOffset);
void *finalize(void *streamObj);

int getStreamState(void *streamObj);
int getWidth(void *streamObj);
int getHeight(void *streamObj);
float getAverageFPS(void *StreamObj);

int getFPS(void *streamObj);

PyObject *getOneFrame(void *streamObj);
int getOneFrameToShm(void *streamObj, int shmOffset);
