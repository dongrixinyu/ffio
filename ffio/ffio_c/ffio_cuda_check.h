/**
 * Library : ffio
 * Author : dongrixinyu, koisi
 * License : MIT
 * Email : dongrixinyu.66@gmail.com
 * Github : https://github.com/dongrixinyu/ffio
 * Description : An easy-to-use Python wrapper for FFmpeg-C-API.
 * Website : http://www.jionlp.com
 */

#ifdef CHECK_IF_CUDA_IS_AVAILABLE
#include "cuda_runtime_api.h"
#endif
#include <stdio.h>

int check_if_cuda_is_available();

#ifdef CHECK_IF_CUDA_IS_AVAILABLE
int available_gpu_memory();
#endif
