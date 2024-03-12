//
// Created by koisi on 2024/2/22.
//
#include "ffioPyApi.h"

FFIO* api_newFFIO(){
  return newFFIO();
}

void api_initFFIO(
    FFIO* ffio, int mode, const char* streamUrl,
    bool hw_enabled, const char* hw_device,
    bool enableShm,  const char* shmName, int shmSize, int shmOffset,
    CodecParams* codecParams
){
  FFIOMode ffioMode = mode == 0 ? FFIO_MODE_DECODE : FFIO_MODE_ENCODE;
  int ret = initFFIO(ffio, ffioMode, streamUrl,
                     hw_enabled, hw_device,
                     enableShm, shmName, shmSize, shmOffset,
                     codecParams
  );
  if(ret != 0){ finalizeFFIO(ffio); }
}

void api_finalizeFFIO(FFIO* ffio){
  finalizeFFIO(ffio);
}

void api_deleteFFIO(FFIO* ffio){
  free( ffio );
}

FFIOFrame* api_decodeOneFrame(FFIO* ffio, const char* sei_filter){
  return decodeOneFrame(ffio, sei_filter);
}

FFIOFrame* api_decodeOneFrameToShm(FFIO* ffio, int shmOffset, const char* sei_filter){
  return decodeOneFrameToShm(ffio, shmOffset, sei_filter);
}

int api_encodeOneFrame(FFIO* ffio, PyObject *PyRGBImage, const char* seiMsg, int seiMsgSize){
  char *RGBImage = PyBytes_AsString(PyRGBImage);
  Py_ssize_t RGBImageSize = PyBytes_GET_SIZE(PyRGBImage);

  int ret = encodeOneFrame(ffio, (unsigned char *)RGBImage, seiMsg, (uint32_t)seiMsgSize);
  return ret;
}
bool api_encodeOneFrameFromShm(FFIO* ffio, int shmOffset, const char* seiMsg, int seiMsgSize){
  return encodeOneFrameFromShm(ffio, shmOffset, seiMsg, (uint32_t)seiMsgSize);
}