//
// Created by koisi on 2024/2/22.
//

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

PyObject* api_decodeOneFrame(FFIO* ffio);
bool      api_decodeOneFrameToShm(FFIO* ffio, int shmOffset);

int  api_encodeOneFrame(FFIO* ffio, PyObject *PyRGBImage);
bool api_encodeOneFrameFromShm(FFIO* ffio, int shmOffset);

#endif // FFIO_PYAPI_H
