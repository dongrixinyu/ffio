#include "pix_fmt_conversion.cuh"
#include "cuda_runtime.h"

int get_current_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL); // get milliseconds

    int milliseconds = (tv.tv_sec * 1000 + tv.tv_usec / 1000) % 1000;

    char str_buffer[30];

    time_t raw_time;
    struct tm *time_info;
    time(&raw_time);
    time_info = localtime(&raw_time);
    strftime(str_buffer, sizeof(str_buffer), "%Y-%m-%d %H:%M:%S", time_info);
    sprintf(str_buffer + strlen(str_buffer), ".%03d", milliseconds);

    // return str_buffer;
    printf("%s\n", str_buffer);
    return 0;
}

void __global__ yuv_2_rgb(
    const int *width, // int height,
    const unsigned char *d_yuv_y, const unsigned char *d_yuv_uv, unsigned char *d_rgb)
{

    const int n = blockDim.x * blockIdx.x + threadIdx.x;

    int i = n / (*width);
    int j = n % (*width);

    int u_index = 2 * (((i / 2) * (*width) / 2) + j / 2);
    int v_index = u_index + 1;

    int r = (d_yuv_y[n] - 16) * 1.164f + (d_yuv_uv[v_index] - 128) * 1.793f;
    r = (r > 255) ? 255 : r;
    r = (r > 0) ? r : 0;
    d_rgb[n * 3] = (unsigned char)r;

    int g = (d_yuv_y[n] - 16) * 1.164f - (d_yuv_uv[u_index] - 128) * 0.213f - (d_yuv_uv[v_index] - 128) * 0.533f;
    g = (g > 255) ? 255 : g;
    g = (g > 0) ? g : 0;
    d_rgb[n * 3 + 1] = (unsigned char)g;

    int b = (d_yuv_y[n] - 16) * 1.164f + (d_yuv_uv[u_index] - 128) * 2.112f;
    b = (b > 255) ? 255 : b;
    b = (b > 0) ? b : 0;
    d_rgb[n * 3 + 2] = (unsigned char)(b);
}

void __global__ rgb_2_yuv(
    const int *width, // int height,
    const unsigned char *d_rgb, unsigned char *d_yuv_y, unsigned char *d_yuv_uv)
{
    // int width = 1280;
    // int height = 720;
    const int n = blockDim.x * blockIdx.x + threadIdx.x;

    d_yuv_y[n] = (unsigned char)(16 + 0.183 * d_rgb[3 * n + 2] + 0.614 * d_rgb[3 * n + 1] + 0.062 * d_rgb[3 * n]);

    // index of width(i) and height(j)
    int i = n / (*width);
    int j = n % (*width);

    int u_index = 2 * (((i / 2) * (*width) / 2) + j / 2);
    int v_index = u_index + 1;

    d_yuv_uv[v_index] = (unsigned char)(128 - 0.101 * d_rgb[3 * n + 2] - 0.339 * d_rgb[3 * n + 1] + 0.439 * d_rgb[3 * n]);
    d_yuv_uv[u_index] = (unsigned char)(128 + 0.439 * d_rgb[3 * n + 2] - 0.399 * d_rgb[3 * n + 1] - 0.04 * d_rgb[3 * n]);
}

void initializeCuda(
    int width, int height,
    unsigned char **d_yuv_y, unsigned char **d_yuv_uv, unsigned char **d_rgb,
    int **d_width)
{
    int base_size = width * height / 2;
    // printf("init 1 malloc ... %p\n", (void **)d_yuv_y);
    cudaMalloc((void **)d_yuv_y, base_size * 2 * sizeof(unsigned char));
    cudaMalloc((void **)d_yuv_uv, base_size * sizeof(unsigned char));
    cudaMalloc((void **)d_rgb, base_size * 6 * sizeof(unsigned char));
    cudaMalloc((void **)d_width, sizeof(int));
    // printf("init 2 malloc ... %p\n", (void **)d_yuv_y);

    // cudaMemcpy(d_width, &width, 4, cudaMemcpyHostToDevice);
}

void finalizeCuda(
    unsigned char *d_yuv_y, unsigned char *d_yuv_uv, unsigned char *d_rgb,
    int *d_width)
{
    cudaFree(d_yuv_y);
    cudaFree(d_yuv_uv);
    cudaFree(d_rgb);
    cudaFree(d_width);
}

int convertRGB2YUVbyCUDA(
    int width, int height,
    unsigned char *h_yuv_y, unsigned char *h_yuv_uv, unsigned char *h_rgb,
    unsigned char *d_yuv_y, unsigned char *d_yuv_uv, unsigned char *d_rgb,
    int *d_width)
{

    int base_size = width * height / 2;

    const int block_size = 128; // choose between 128 and 256
    const int grid_size = base_size * 2 / block_size;

    cudaMemcpy(d_width, &width, 4, cudaMemcpyHostToDevice);
    cudaMemcpy(d_rgb, h_rgb, base_size * 6 * sizeof(unsigned char), cudaMemcpyHostToDevice);

    rgb_2_yuv<<<grid_size, block_size>>>(d_width, d_rgb, d_yuv_y, d_yuv_uv);

    cudaMemcpy(h_yuv_y, d_yuv_y, base_size * 2 * sizeof(unsigned char), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_yuv_uv, d_yuv_uv, base_size * sizeof(unsigned char), cudaMemcpyDeviceToHost);

    return 0;
}

int convertYUV2RGBbyCUDA(
    int width, int height,
    unsigned char *h_yuv_y, unsigned char *h_yuv_uv, unsigned char *h_rgb,
    unsigned char *d_yuv_y, unsigned char *d_yuv_uv, unsigned char *d_rgb,
    int *d_width)
{

    int base_size = width * height / 2;

    const int block_size = 128; // choose between 128 and 256
    const int grid_size = base_size * 2 / block_size;

    // printf("re malloc ... %p\n", (void **)(&d_yuv_y));

    cudaMemcpy(d_width, &width, 4, cudaMemcpyHostToDevice);
    cudaMemcpy(d_yuv_y, h_yuv_y, base_size * 2 * sizeof(unsigned char),
               cudaMemcpyHostToDevice);
    cudaMemcpy(d_yuv_uv, h_yuv_uv, base_size * sizeof(unsigned char),
               cudaMemcpyHostToDevice);

    // ret = get_str_time();
    yuv_2_rgb<<<grid_size, block_size>>>(d_width, d_yuv_y, d_yuv_uv, d_rgb);
    // ret = get_str_time();

    cudaMemcpy(h_rgb, d_rgb, base_size * 6 * sizeof(unsigned char),
               cudaMemcpyDeviceToHost);
    // ret = get_str_time();

    return 0;
}
