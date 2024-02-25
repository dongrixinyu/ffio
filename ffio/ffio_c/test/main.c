//
// Created by koisi on 2024/2/22.
//
#include <stdlib.h>
#include <string.h>

#include "../ffio.h"

static void saveRGB2File(const FFIO* ffio){
  FILE *pFile;
  char filename[32];

  snprintf(filename, sizeof(filename), "frame%d.ppm", ffio->frameSeq);
  pFile = fopen(filename, "wb");
  if (pFile == NULL) { return; }
  fprintf(pFile, "P6\n%d %d\n255\n", ffio->imageWidth, ffio->imageHeight);

  fwrite(ffio->rawFrame, 1, ffio->imageByteSize, pFile);
  fclose(pFile);
}

int main(int argc, char *argv[]){
  if( argc == 1 ){ printf("Please run with args: i_url [o_url] [gpu|cpu].\n"); exit(1); }
  printf("Running with: %s %s %s.\n", argv[0], argv[1], argc == 3 ? argv[2] : "" );

  char *i_url = argv[1];
  char *o_url = NULL;
  bool hw_enabled = false;
  if(argc==3){
    if( strncmp(argv[2], "gpu", 3) == 0 ){ hw_enabled = true; }
    else{ o_url = argv[2]; }
  }else if(argc>3){
    o_url = argv[2];
    if( strncmp(argv[3], "gpu", 3) == 0 ){ hw_enabled = true; }
  }

  FFIO* i_ffio = newFFIO();
  FFIO* o_ffio = newFFIO();

  CodecParams params = {
      0, 0, 0, 0, 0, 0,
      "", "", "", "", "", ""
  };
  initFFIO(i_ffio, FFIO_MODE_DECODE, i_url,
           hw_enabled, NULL,
           false, NULL, 0, 0, &params);
  if(o_url!=NULL){
    initFFIO(o_ffio, FFIO_MODE_ENCODE, o_url,
             hw_enabled, NULL,
             false, NULL, 0, 0, &params);
  }

  int ret;
  for(int i=0; i<200; ++i){
    ret = decodeOneFrame(i_ffio);
    printf("[%d] decodeOneFrame returned %d.\n", i_ffio->frameSeq, ret);
    if( ret==0 && i<10){ saveRGB2File(i_ffio); }
    if( ret==0 && o_url!=NULL){
      ret = encodeOneFrame(o_ffio, i_ffio->rawFrame);
      printf("[%d] encodeOneFrame returned %d.\n", o_ffio->frameSeq, ret);
    }
  }

  finalizeFFIO(i_ffio);
  finalizeFFIO(o_ffio);

  return 0;
}
