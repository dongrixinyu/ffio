#ifndef RGB2YUV
#define RGB2YUV

#include <stdio.h>
// #include <unistd.h>
#include <sys/time.h>
// #include <cuda_runtime.h>

#ifdef __cplusplus
extern "C"
{
#endif
    int get_current_time();

    void initializeCuda(
        int width, int height,
        unsigned char **d_yuv_y, unsigned char **d_yuv_uv, unsigned char **d_rgb,
        int **d_width);

    void finalizeCuda(
        unsigned char *d_yuv_y, unsigned char *d_yuv_uv, unsigned char *d_rgb,
        int *d_width);

    int convertRGB2YUVbyCUDA(
        int width, int height,
        unsigned char *h_yuv_y, unsigned char *h_yuv_uv, unsigned char *h_rgb,
        unsigned char *d_yuv_y, unsigned char *d_yuv_uv, unsigned char *d_rgb,
        int *d_width);

    int convertYUV2RGBbyCUDA(
        int width, int height,
        unsigned char *h_yuv_y, unsigned char *h_yuv_uv, unsigned char *h_rgb,
        unsigned char *d_yuv_y, unsigned char *d_yuv_uv, unsigned char *d_rgb,
        int *d_width);

#ifdef __cplusplus
}
#endif
#endif
