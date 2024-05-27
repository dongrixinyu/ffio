#ifndef RGB2YUV
#define RGB2YUV

#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
// #include <cuda_runtime.h>

void __global__ rgb_2_yuv(
    const int *width,
    const unsigned char *d_rgb, unsigned char *d_yuv_y, unsigned char *d_yuv_uv);

#endif
