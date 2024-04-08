#include <math.h>
#include <stdio.h>
#include <unistd.h>

void __global__ yuv_2_rgb(
    // int width, int height,
    const unsigned char *d_yuv_y, const unsigned char *d_yuv_uv, unsigned char *d_rgb);
void check(const double *z, const int N);

const char *dir_name = "/home/cuichengyu/github/ffio/ffio/";

// BT.709, which is the standard for HDTV.
float kColorConversion709Default[] = {
    1.164,
    1.164,
    1.164,
    0.0,
    -0.213,
    2.112,
    1.793,
    -0.533,
    0.0,
};

static char str_buffer[128];

int get_str_time()
{
    time_t raw_time;
    struct tm *time_info;
    time(&raw_time);
    time_info = localtime(&raw_time);
    strftime(str_buffer, sizeof(str_buffer), "%Y-%m-%d %H:%M:%S", time_info);
    // return str_buffer;
    printf("%s\n", str_buffer);
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
    unsigned char *h_orig_rgb = (unsigned char *)malloc(base_size * 6);

    FILE *f_uv = fopen("/home/cuichengyu/github/ffio/ffio/nv12_uv", "r");
    fread((char *)h_yuv_uv, base_size, 1, f_uv);
    fclose(f_uv);
    FILE *f_y = fopen("/home/cuichengyu/github/ffio/ffio/nv12_y", "r");
    fread((char *)h_yuv_y, base_size * 2, 1, f_y);
    fclose(f_y);

    FILE *f_rgb = fopen("/home/cuichengyu/github/ffio/ffio/rgb", "r");
    fread((char *)h_orig_rgb, base_size * 6, 1, f_rgb);
    fclose(f_rgb);

    const int block_size = 128; // choose between 128 and 256
    const int grid_size = base_size * 2 / block_size;

    unsigned char *d_yuv_y, *d_yuv_uv, *d_rgb;
    // int *d_width, d_height;
    cudaMalloc((void **)&d_yuv_y, base_size * 2 * sizeof(unsigned char));
    cudaMalloc((void **)&d_yuv_uv, base_size * sizeof(unsigned char));
    cudaMalloc((void **)&d_rgb, base_size * 6 * sizeof(unsigned char));
    // cudaMalloc((void **)&d_width, sizeof(int));

    cudaMemcpy(d_yuv_y, h_yuv_y, base_size * 2 * sizeof(unsigned char), cudaMemcpyHostToDevice);
    cudaMemcpy(d_yuv_uv, h_yuv_uv, base_size * sizeof(unsigned char), cudaMemcpyHostToDevice);

    ret = get_str_time();
    yuv_2_rgb<<<grid_size, block_size>>>(d_yuv_y, d_yuv_uv, d_rgb);

    ret = get_str_time();
    // sleep(15);

    cudaMemcpy(h_rgb, d_rgb, base_size * 6 * sizeof(unsigned char), cudaMemcpyDeviceToHost);
    ret = get_str_time();

    FILE *fw = NULL;
    fw = fopen("/home/cuichengyu/github/ffio/ffio/rgb_res", "w");
    fwrite(h_rgb, base_size * 6 * sizeof(unsigned char), 1, fw);
    fclose(fw);

    int i = 0;
    int j = 1;
    int index = i * width + j;
    printf("index: %d\n", index);
    printf("width: %d, height: %d.    yuv2rgb: %d, orig_rgb: %d.\n",
           i, j, h_rgb[3 * index], h_orig_rgb[3 * index]);
    printf("width: %d, height: %d.    yuv2rgb: %d, orig_rgb: %d.\n",
           i, j, h_rgb[3 * index + 1], h_orig_rgb[3 * index + 1]);
    printf("width: %d, height: %d.    yuv2rgb: %d, orig_rgb: %d.\n",
           i, j, h_rgb[3 * index + 2], h_orig_rgb[3 * index + 2]);

    free(h_yuv_y);
    free(h_yuv_uv);
    free(h_rgb);
    cudaFree(d_yuv_y);
    cudaFree(d_yuv_uv);
    cudaFree(d_rgb);

    return 0;
}

void __global__ yuv_2_rgb(
    // int width, int height,
    const unsigned char *d_yuv_y, const unsigned char *d_yuv_uv, unsigned char *d_rgb)
{
    int width = 1280;
    int height = 720;
    const int n = blockDim.x * blockIdx.x + threadIdx.x;

    int i = n / width;
    int j = n % width;

    // d_yuv_y[n] -= 16;
    // d_yuv_uv[_n] -= 128;

    int u_index = 2 * (((i / 2) * width / 2) + j / 2);
    int v_index = u_index + 1;

    d_rgb[n * 3 + 2] = (unsigned char)((d_yuv_y[n] - 16) * 1.164f + (d_yuv_uv[v_index] - 128) * 1.793f);
    d_rgb[n * 3 + 1] = (unsigned char)((d_yuv_y[n] - 16) * 1.164f - (d_yuv_uv[u_index] - 128) * 0.213f - (d_yuv_uv[v_index] - 128) * 0.533f);
    d_rgb[n * 3] = (unsigned char)((d_yuv_y[n] - 16) * 1.164f + (d_yuv_uv[u_index] - 128) * 2.112f);

}
