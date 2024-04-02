/**
 * Library : ffio
 * Author : dongrixinyu, koisi
 * License : MIT
 * Email : dongrixinyu.66@gmail.com
 * Github : https://github.com/dongrixinyu/ffio
 * Description : An easy-to-use Python wrapper for FFmpeg-C-API.
 * Website : http://www.jionlp.com
 */

#include "ffio_cuda_check.h"

// gcc cuda_version_check.c -I /usr/local/cuda/include -L/usr/local/cuda/lib64 -lcudart -o cuda_version_check

int check_if_cuda_is_available()
{
#ifdef CHECK_IF_CUDA_IS_AVAILABLE
    struct cudaDeviceProp prop;
    int deviceCount;

    // get the number of device
    cudaGetDeviceCount(&deviceCount);

    if (deviceCount == 0)
    {
        printf("No CUDA devices available.\n");
        return -1;
    }

    for (int i = 0; i < deviceCount; i++){
        // get the attribute of first device
        cudaGetDeviceProperties(&prop, i);

        // get more attributes, computation, max threads
        printf("    > gpu %d - %s - %dMiB:\n",
            i, prop.name, (int)(prop.totalGlobalMem / 1048576));
    }

    return 0;
#endif
    return -2;
}
