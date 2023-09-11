#include "extractFrame.h"
#include "Python.h"

#ifdef _WIN32
#define API __declspec(dllexport)
#else
#define API
#endif

void *init(const char *sourceStreamPath);

int getWidth(void *streamObj);
int getHeight(void *streamObj);
float getAverageFPS(void *StreamObj);

int getFPS(void *streamObj);

PyObject *getOneFrame(void *streamObj);
