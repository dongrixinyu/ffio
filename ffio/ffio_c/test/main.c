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
  if(argc >= 2){ printf("Running with: %s %s\n", argv[0], argv[1]); }
  else{ printf("Failed to run with wrong arguments.\n"); exit(1); }

  if(strncmp(argv[1], "rtmp://", 7) != 0){
    printf("Please provide a correct rtmp url.\n");
    exit(1);
  }
  bool hw_enabled = false;
  if(argc>=3 && strncmp(argv[2], "gpu", 3)==0 ){ hw_enabled = true; }

  char* rtmp = argv[1];
  FFIO* ffio = newFFIO();

  CodecParams params;
  initFFIO(ffio, FFIO_MODE_DECODE, rtmp,
           hw_enabled, NULL,
           false, NULL, 0, 0, &params);

  int ret;
  for(int i=0; i<10; ++i){
    ret = decodeOneFrame(ffio);
    printf("decodeOneFrame returned %d, image seq: %d.\n", ret, ffio->frameSeq);
    if(ret == 0){
      saveRGB2File(ffio);
    }
  }

  finalizeFFIO(ffio);
  finalizeFFIO(ffio);

  return 0;
}
