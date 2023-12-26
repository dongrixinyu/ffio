#include "ffio_util.h"

int interrupt_callback(void *p)
{
    Clicker *cl = (Clicker *)p;
    if (cl->lasttime > 0)
    {
        // 5 seconds waiting for an empty stream
        if (time(NULL) - cl->lasttime > WAIT_FOR_STREAM_TIMEOUT)
        {
            return 1;
        }
    }

    return 0;
}

void av_log_pyFFmpeg_callback(void *avClass, int level, const char *fmt, va_list vl)
{
    if (level > AV_LOG_INFO) // set level
    {
        return;
    }

    AVBPrint bPrint;
    av_bprint_init(&bPrint, 1024, 10240);
    av_vbprintf(&bPrint, fmt, vl);

    time_t rawtime;
    struct tm *info;
    char buffer[80];
    char fileNameBuffer[9];

    time(&rawtime);

    info = localtime(&rawtime);

    strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", info);
    strftime(fileNameBuffer, 9, "%Y%m%d", info);

    char fileName[200];

    // add log info
    const char *homeDir;
    struct passwd *pw = getpwuid(getuid());
    homeDir = pw->pw_dir;

    // const char *homedir = getpwuid(getuid())->pw_dir;

    // get the pid of the current process
    pid_t pid = getpid();

    sprintf(fileName, "%s/.cache/ffio/clog-%d.txt.%s",
            homeDir, pid, fileNameBuffer); // copy the log file path

    // printf("full path: %s\n", fileName);

    FILE *fp = fopen(fileName, "a");
    if (fp != NULL)
    {
        if (level == 8)
        {
            fprintf(fp, "[ %s ] %s %s", buffer, "FATAL", bPrint.str);
        }
        else if (level == 16)
        {
            fprintf(fp, "[ %s ] %s %s", buffer, "ERROR", bPrint.str);
        }
        else if (level == 24)
        {
            fprintf(fp, "[ %s ] %s %s", buffer, "WARNING", bPrint.str);
        }
        else if (level == 32)
        {
            fprintf(fp, "[ %s ] %s %s", buffer, "INFO", bPrint.str);
            // fprintf(fp, "%s", bPrint.str);
        }

        // #define AV_LOG_FATAL     8
        // #define AV_LOG_ERROR    16
        // #define AV_LOG_WARNING  24
        // #define AV_LOG_INFO     32

        fclose(fp);
        fp = NULL;
    }
    printf("%s", bPrint.str);

    av_bprint_finalize(&bPrint, NULL);
};

static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt, const char *tag)
{
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

    printf("%s: pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
           tag,
           av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
           av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
           av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
           pkt->stream_index);
}
