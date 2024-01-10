#pragma once

// this param is set to return a timeout error when waiting for more than 5 seconds.
#define WAIT_FOR_STREAM_TIMEOUT 5

// write log to file in every PRINT_FRAME_GAP frames to make sure correctness.
#define PRINT_FRAME_GAP 200

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <pwd.h>

#include "libavutil/opt.h"
#include "libavutil/bprint.h"
#include "libavutil/imgutils.h"
#include "libavutil/time.h"
#include "libavutil/timestamp.h"

#include "libavutil/hwcontext.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
#include "libavutil/mathematics.h"
#include "libswscale/swscale.h"

typedef struct Clicker
{
    time_t lasttime;
} Clicker;

int interrupt_callback(void *p);

void av_log_ffio_callback(void *avClass, int level, const char *fmt, va_list vl);
