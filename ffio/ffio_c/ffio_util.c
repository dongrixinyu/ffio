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

void av_log_ffio_callback(void *avClass, int level, const char *fmt, va_list vl)
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
}

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

void print_avcodec_supported_pix_fmt(AVCodec *codec){
  LOG_INFO("[pix_fmt] codec %s supports pix_fmt: ", codec->name);
  for(int i=0;;++i){
    if( codec->pix_fmts==NULL || codec->pix_fmts[i]==AV_PIX_FMT_NONE ) { break; }
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(codec->pix_fmts[i]);
    char* hw_support = (desc->flags & AV_PIX_FMT_FLAG_HWACCEL) ? "HWACCEL" : "NON-HW";
    LOG_INFO("[pix_fmt]    %-4d  %-20s  %s.", i, av_get_pix_fmt_name(codec->pix_fmts[i]), hw_support);
  }
}
enum AVPixelFormat find_avcodec_1st_sw_pix_fmt(AVCodec *codec){
  for(int i=0;;++i){
    if( codec->pix_fmts==NULL || codec->pix_fmts[i]==AV_PIX_FMT_NONE ) {
      LOG_WARNING("[pix_fmt] auto find sw_pix_fmt for codec: %s", av_get_pix_fmt_name(AV_PIX_FMT_NONE));
      return AV_PIX_FMT_NONE;
    }
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(codec->pix_fmts[i]);
    if( !(desc->flags & AV_PIX_FMT_FLAG_HWACCEL) ){
      LOG_WARNING("[pix_fmt] auto find sw_pix_fmt for codec: %s.", av_get_pix_fmt_name(codec->pix_fmts[i]));
      return codec->pix_fmts[i];
    }
  }
}
enum AVPixelFormat find_avcodec_1st_hw_pix_fmt(AVCodec *codec){
  for(int i=0;;++i){
    if( codec->pix_fmts==NULL || codec->pix_fmts[i]==AV_PIX_FMT_NONE ) {
      LOG_WARNING("[pix_fmt] auto find hw_pix_fmt for codec: %s.", av_get_pix_fmt_name(AV_PIX_FMT_NONE));
      return AV_PIX_FMT_NONE;
    }
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(codec->pix_fmts[i]);
    if( (desc->flags & AV_PIX_FMT_FLAG_HWACCEL) ){
      LOG_WARNING("[pix_fmt] auto find hw_pix_fmt for codec: %s.", av_get_pix_fmt_name(codec->pix_fmts[i]));
      return codec->pix_fmts[i];
    }
  }
}

bool extend_sei_to_av_packet(bool useAnnexB, AVPacket* pkt,const uint8_t* uuid,
                             const char* message, uint32_t sei_message_size){
  /**
   * Description:
   *   Extend sei NALU byte stream to an existing AVPacket.
   */
  uint32_t sei_payload_size                 = sei_message_size + 16;   // 16 bytes for uuid.
  // sei_payload_size_size: the bytes to store the size of sei payload.
  uint32_t sei_payload_size_size            = sei_payload_size / 0xFF + 1;
  // first 2 bytes of sei NALU: "0x06 0x05".
  //   0x06 indicates that is a sei NALU.
  //   0x05 indicates the type of sei is: user_data_unregistered.
  uint32_t sei_size_without_header          = 2 + sei_payload_size + sei_payload_size_size;
  uint32_t sei_tail_size                    = sei_size_without_header%2 == 0 ? 2 : 1 ;
      sei_size_without_header              += sei_tail_size;
  uint32_t sei_size_without_header_reversed = __builtin_bswap32(sei_size_without_header);
  uint32_t sei_total_size                   = 4 + sei_size_without_header;

  if (sei_total_size > MAX_SEI_LENGTH ){
    LOG_WARNING("[sei] skip to insert sei with large size: %d", sei_total_size);
    return false;
  }

  int origin_pkt_size = pkt->size;
  int ret = av_grow_packet(pkt, (int)sei_total_size);
  if(ret < 0){
    LOG_WARNING("[sei] failed to grow av packet.");
    return false;
  }
  // Todo: maybe there are some ways to avoid moving origin video data for improving performance.
  memmove(pkt->data + sei_total_size, pkt->data, origin_pkt_size);
  uint8_t* p_sei = pkt->data;

  // set sei header.
  if(useAnnexB){
    memcpy(p_sei, (unsigned char[]){0x00, 0x00, 0x00, 0x01, 0x06, 0x05}, 6);
  }else{
    memcpy(p_sei, &sei_size_without_header_reversed, sizeof(sei_size_without_header_reversed));
    p_sei[4]=0x06; p_sei[5]=0x05;
  }

  // set sei contents size.
  for(int i=1; i<sei_payload_size_size; ++i){ p_sei[5+i] = 0xFF; }
  p_sei[5+sei_payload_size_size] = sei_payload_size % 0xFF;

  // set uuid and message to sei.
  memcpy(p_sei+5+sei_payload_size_size+1, uuid, 16);
  memcpy(p_sei+5+sei_payload_size_size+1+16, message, sei_message_size);

  // set the tail of sei.
  if(sei_tail_size==2){ p_sei[sei_total_size-2]=0x00; }
  p_sei[sei_total_size-1]=0x80;

  return true;
}
bool get_sei_from_av_frame(AVFrame* avFrame, unsigned char* dst, const char* filter){
  /*
   * Description:
   *   Set sei message from avFrame to (unsigned char*)dst.
   *   This function will only fill one SEI side date to dst.
   *   If a video frame has multiple side data,
   *   a filter can be provided, which will fill the first SEI side date
   *   that contains the filter string to the dst buffer.
   */
  AVFrameSideData* side_data;
  for(int i=0; i < avFrame->nb_side_data; ++i){
    side_data = avFrame->side_data[i];

    if(side_data->type == AV_FRAME_DATA_SEI_UNREGISTERED){
      // side_data->data+16: ignore 16 bytes of uuid.
      strncpy((char*)dst, (const char*)side_data->data+16,
              MAX_SEI_LENGTH-1 < side_data->size-16 ? MAX_SEI_LENGTH-1 :  side_data->size-16);
      if(filter == NULL
          || strstr((const char*)dst, filter) != NULL){
        return true;
      }
    }

  }
  return false;
}