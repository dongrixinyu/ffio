#ifndef YUV2RGB
#define YUV2RGB

#include <stdio.h>
#include <unistd.h>

void __global__ rgb_2_yuv(
    const int *width,
    const unsigned char *d_rgb, unsigned char *d_yuv_y, unsigned char *d_yuv_uv);

#endif
