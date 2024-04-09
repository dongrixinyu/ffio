#ifndef YUV2RGB
#define YUV2RGB

#include <stdio.h>

void __global__ yuv_2_rgb(
    const int *width, // int height,
    const unsigned char *d_yuv_y, const unsigned char *d_yuv_uv, unsigned char *d_rgb);

#endif
