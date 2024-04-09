#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

void __global__ rgb_2_yuv(
    const int *width,
    const unsigned char *d_rgb, unsigned char *d_yuv_y, unsigned char *d_yuv_uv);

const char *dir_name = "/home/cuichengyu/github/ffio/ffio/";

int get_str_time()
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

void __global__ rgb_2_yuv(
    const int *width, // int height,
    const unsigned char *d_rgb, unsigned char *d_yuv_y, unsigned char *d_yuv_uv)
{
    // int width = 1280;
    int height = 720;
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

int convertRGB2YUV(
    int width, int height,
    unsigned char *h_rgb, unsigned char *h_yuv_y, unsigned char *h_yuv_uv)
{
    int ret;

    // const int width = 1280;
    // const int height = 720;
    int base_size = width * height / 2;

    const int block_size = 128; // choose between 128 and 256
    const int grid_size = base_size * 2 / block_size;

    unsigned char *d_yuv_y, *d_yuv_uv, *d_rgb;
    int *d_width;
    cudaMalloc((void **)&d_yuv_y, base_size * 2 * sizeof(unsigned char));
    cudaMalloc((void **)&d_yuv_uv, base_size * sizeof(unsigned char));
    cudaMalloc((void **)&d_rgb, base_size * 6 * sizeof(unsigned char));
    cudaMalloc((void **)&d_width, sizeof(int));

    cudaMemcpy(d_width, &width, 4, cudaMemcpyHostToDevice);
    cudaMemcpy(d_rgb, h_rgb, base_size * 6 * sizeof(unsigned char), cudaMemcpyHostToDevice);

    ret = get_str_time();
    rgb_2_yuv<<<grid_size, block_size>>>(d_width, d_rgb, d_yuv_y, d_yuv_uv);
    ret = get_str_time();

    cudaMemcpy(h_yuv_y, d_yuv_y, base_size * 2 * sizeof(unsigned char), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_yuv_uv, d_yuv_uv, base_size * sizeof(unsigned char), cudaMemcpyDeviceToHost);
    ret = get_str_time();

    free(h_yuv_y);
    free(h_yuv_uv);
    free(h_rgb);

    cudaFree(d_yuv_y);
    cudaFree(d_yuv_uv);
    cudaFree(d_rgb);

    return 0;
}

int main(void)
{
    int ret;

    const int width = 1280;
    const int height = 720;
    int base_size = width * height / 2;
    unsigned char *h_yuv_y = (unsigned char *)malloc(base_size * 2);
    unsigned char *h_yuv_uv = (unsigned char *)malloc(base_size);
    unsigned char *h_rgb = (unsigned char *)malloc(base_size * 6);
    unsigned char *h_orig_yuv_y = (unsigned char *)malloc(base_size * 2);
    unsigned char *h_orig_yuv_uv = (unsigned char *)malloc(base_size);

    FILE *f_uv = fopen("/home/cuichengyu/github/ffio/ffio/nv12_uv", "r");
    fread((char *)h_orig_yuv_uv, base_size, 1, f_uv);
    fclose(f_uv);
    FILE *f_y = fopen("/home/cuichengyu/github/ffio/ffio/nv12_y", "r");
    fread((char *)h_orig_yuv_y, base_size * 2, 1, f_y);
    fclose(f_y);

    FILE *f_rgb = fopen("/home/cuichengyu/github/ffio/ffio/rgb", "r");
    fread((char *)h_rgb, base_size * 6, 1, f_rgb);
    fclose(f_rgb);

    ret = get_str_time();

    const int block_size = 128; // choose between 128 and 256
    const int grid_size = base_size * 2 / block_size;

    unsigned char *d_yuv_y, *d_yuv_uv, *d_rgb;
    int *d_width, d_height;
    cudaMalloc((void **)&d_yuv_y, base_size * 2 * sizeof(unsigned char));
    cudaMalloc((void **)&d_yuv_uv, base_size * sizeof(unsigned char));
    cudaMalloc((void **)&d_rgb, base_size * 6 * sizeof(unsigned char));
    cudaMalloc((void **)&d_width, sizeof(int));
    ret = get_str_time();

    cudaMemcpy(d_width, &width, 4, cudaMemcpyHostToDevice);
    // cudaMemcpy(d_yuv_y, h_yuv_y, base_size * 2 * sizeof(unsigned char), cudaMemcpyHostToDevice);
    // cudaMemcpy(d_yuv_uv, h_yuv_uv, base_size * sizeof(unsigned char), cudaMemcpyHostToDevice);
    cudaMemcpy(d_rgb, h_rgb, base_size * 6 * sizeof(unsigned char), cudaMemcpyHostToDevice);

    printf("compute cuda func\n");
    ret = get_str_time();
    rgb_2_yuv<<<grid_size, block_size>>>(d_width, d_rgb, d_yuv_y, d_yuv_uv);

    ret = get_str_time();
    // sleep(15);

    cudaMemcpy(h_yuv_y, d_yuv_y, base_size * 2 * sizeof(unsigned char), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_yuv_uv, d_yuv_uv, base_size * sizeof(unsigned char), cudaMemcpyDeviceToHost);
    ret = get_str_time();

    // FILE *fw = NULL;
    // fw = fopen("/home/cuichengyu/github/ffio/ffio/yuv_y_res", "w");
    // fwrite(h_yuv_y, base_size * 2 * sizeof(unsigned char), 1, fw);
    // fclose(fw);

    // FILE *fw_uv = NULL;
    // fw_uv = fopen("/home/cuichengyu/github/ffio/ffio/yuv_uv_res", "w");
    // fwrite(h_yuv_uv, base_size * sizeof(unsigned char), 1, fw_uv);
    // fclose(fw_uv);

    // int i = 4;
    // int j = 243;
    // int index = i * width + j;
    // printf("index: %d\n", index);
    // printf("width: %d, height: %d.    rgb2yuv: %d, orig_yuv: %d.\n",
    //        i, j, h_yuv_y[index], h_orig_yuv_y[index]);
    // printf("width: %d, height: %d.    rgb2yuv: %d, orig_yuv: %d.\n",
    //        i, j, h_yuv_uv[index], h_orig_yuv_uv[index]);
    // printf("width: %d, height: %d.    rgb2yuv: %d, orig_yuv: %d.\n",
    //        i, j, h_yuv_uv[index + 1], h_orig_yuv_uv[index + 1]);

    free(h_yuv_y);
    free(h_yuv_uv);
    free(h_rgb);
    cudaFree(d_yuv_y);
    cudaFree(d_yuv_uv);
    cudaFree(d_rgb);
    ret = get_str_time();

    return 0;
}
