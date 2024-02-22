//
// Created by koisi on 2024/2/22.
//
#include <stdlib.h>
#include <string.h>

#include "../ffio.h"

int main(int argc, char *argv[]){
  if(argc == 2){
    printf("Running with: %s %s\n", argv[0], argv[1]);
  }else{
    printf("Failed to run with wrong arguments.\n");
    exit(1);
  }
  if(strncmp(argv[1], "rtmp://", 7) != 0){
    printf("Please provide a correct rtmp url.\n");
    exit(1);
  }

  char* rtmp = argv[1];
  FFIO* ffio = newFFIO();

  initFFIO(ffio, FFIO_MODE_DECODE, rtmp,
           false, NULL,
           false, NULL, 0, 0);

  finalizeFFIO(ffio);
  finalizeFFIO(ffio);

  return 0;
}
