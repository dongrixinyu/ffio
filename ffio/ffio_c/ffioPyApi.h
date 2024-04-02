/**
 * Library : ffio
 * Author : koisi, dongrixinyu
 * License : MIT
 * Email : dongrixinyu .66 @gmail.com
 * Github : https://github.com/dongrixinyu/ffio
 * Description : An easy-to-use Python wrapper for FFmpeg-C-API.
 * Website : http://www.jionlp.com
 */

#ifndef FFIO_PYAPI_H
#define FFIO_PYAPI_H

#include "Python.h"

#include "ffio.h"


FFIO* api_newFFIO();
void api_initFFIO(
    FFIO* ffio, int mode, const char* streamUrl,
    bool hw_enabled, const char* hw_device,
    bool enableShm,  const char* shmName, int shmSize, int shmOffset,
    CodecParams* codecParams
);
void api_finalizeFFIO(FFIO* ffio);
void api_deleteFFIO(FFIO* ffio);

FFIOFrame* api_decodeOneFrame(FFIO* ffio, const char* sei_filter);
FFIOFrame* api_decodeOneFrameToShm(FFIO* ffio, int shmOffset, const char* sei_filter);

int  api_encodeOneFrame(FFIO* ffio, PyObject *PyRGBImage, const char* seiMsg, int seiMsgSize);
bool api_encodeOneFrameFromShm(FFIO* ffio, int shmOffset, const char* seiMsg, int seiMsgSize);

#endif // FFIO_PYAPI_H
