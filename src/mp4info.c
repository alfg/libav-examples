#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/log.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "mp4info.h"

int main(int argc, const char *argv[])
{
  if (argc < 2) {
    printf("Please specify a media file.\n");
    return -1;
  }

  av_log_set_level(AV_LOG_QUIET); // No logging output for libav.

  logger("initializing all the containers, codecs and protocols.");

  // Allocate for the format (container).
  AVFormatContext *pFormatContext = avformat_alloc_context();
  if (!pFormatContext) {
    logger("ERROR: could not allocate memory for Format Context");
    return -1;
  }

  logger("opening the input file: %s and loading format (container) header", argv[1]);

  // Open the file and read header.
  int ret;
  if ((ret = avformat_open_input(&pFormatContext, argv[1], NULL, NULL)) < 0) {
      logger("ERROR: could not open the file. Error: %d", ret);
      printf("%s", av_err2str(ret));
      return ret;
  }

  // av_dump_format(pFormatContext, 0, argv[1], 0);

  // Read container data.
  logger("format: %s, duration: %lld us, bit_rate: %lld",
    pFormatContext->iformat->name,
    pFormatContext->duration,
    pFormatContext->bit_rate);

  printf("url: %s\n", pFormatContext->url);
  printf("nb_streams: %d\n", pFormatContext->nb_streams);
  printf("flags: %d\n", pFormatContext->flags);

  // Find stream info from format.
  logger("getting stream info from format");

  if (avformat_find_stream_info(pFormatContext, NULL) < 0) {
    logger("ERROR: could not get stream info");
    return -1;
  }

  // Initialize the AVCodec.
  AVCodec  *pCodec = NULL;
  AVCodecParameters *pCodecParameters = NULL;
  int video_stream_index = -1;

  // Loop through the streams and print its information.
  for (int i = 0; i < pFormatContext->nb_streams; i++) {
    AVCodecParameters *pLocalCodecParameters = NULL;
    pLocalCodecParameters = pFormatContext->streams[i]->codecpar;
    logger("AVStream->time_base before open coded %d/%d", pFormatContext->streams[i]->time_base.num, pFormatContext->streams[i]->time_base.den);
    logger("AVStream->r_frame_rate before open coded %d/%d", pFormatContext->streams[i]->r_frame_rate.num, pFormatContext->streams[i]->r_frame_rate.den);
    logger("AVStream->start_time %" PRId64, pFormatContext->streams[i]->start_time);
    logger("AVStream->duration %" PRId64, pFormatContext->streams[i]->duration);

    printf("id: %d\n", pFormatContext->streams[i]->id);
    printf("sample_aspect_ratio: %d\n", pFormatContext->streams[i]->sample_aspect_ratio.den);

    printf("AVMediaType Unknown: %d\n", AVMEDIA_TYPE_UNKNOWN);
    printf("AVMediaType Video: %d\n", AVMEDIA_TYPE_VIDEO);
    printf("AVMediaType Audio: %d\n", AVMEDIA_TYPE_AUDIO);

    printf("codec_type: %d\n", pLocalCodecParameters->codec_type);
    printf("codec_id: %d\n", pLocalCodecParameters->codec_id);
    printf("codec_tag: %02x\n", pLocalCodecParameters->codec_tag);

    // Convert to char byte array.
    uint32_t n = pLocalCodecParameters->codec_tag;
    unsigned char bytes[4];
    for (int j = 0; j < 4; ++j) {
      bytes[j] = (n >> (j * 8) & 0xFF);
    }
    printf("0x%x%x%x%x\n", bytes[0], bytes[1], bytes[2], bytes[3]);
    printf("%s\n", bytes);

    printf("format: %d\n", pLocalCodecParameters->format);
    printf("bitrate: %ld\n", pLocalCodecParameters->bit_rate);
    printf("profile: %d\n", pLocalCodecParameters->profile);
    printf("level: %d\n", pLocalCodecParameters->level);
    printf("width: %d\n", pLocalCodecParameters->width);
    printf("height: %d\n", pLocalCodecParameters->height);
    printf("sample_aspect_ratio: %d/%d\n", pLocalCodecParameters->sample_aspect_ratio.num, pLocalCodecParameters->sample_aspect_ratio.den);

    printf("channels: %d\n", pLocalCodecParameters->channels);
    printf("sample_rate: %d\n", pLocalCodecParameters->sample_rate);
    printf("frame_size: %d\n", pLocalCodecParameters->frame_size);


    // Print out the decoded frame info.
    AVCodec *pLocalCodec = avcodec_find_decoder(pLocalCodecParameters->codec_id);
    if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
      if (video_stream_index == -1) {
        video_stream_index = i;
        pCodec = pLocalCodec;
        pCodecParameters = pLocalCodecParameters;
      }

      printf("Video Codec: resolution %d x %d\n",
        pLocalCodecParameters->width, pLocalCodecParameters->height);
    } else if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_AUDIO) {
      printf("Audio Codec: %d channels, sample rate %d\n",
        pLocalCodecParameters->channels, pLocalCodecParameters->sample_rate);
    }
    AVCodecContext *pCodecContext = avcodec_alloc_context3(pCodec);
    avcodec_parameters_to_context(pCodecContext, pCodecParameters);
    avcodec_open2(pCodecContext, pCodec, NULL);

    AVPacket *pPacket = av_packet_alloc();
    AVFrame *pFrame = av_frame_alloc();
    int frameCount = 0;

    // av_packet_unref(pPacket);

    int how_many_packets_to_process = 100;
    av_seek_frame(pFormatContext, 1, 1001*30, AVSEEK_FLAG_ANY);
    while (av_read_frame(pFormatContext, pPacket) >= 0) {

      avcodec_send_packet(pCodecContext, pPacket);
      avcodec_receive_frame(pCodecContext, pFrame);

      printf("pict_type: %c\n", (char) av_get_picture_type_char(pFrame->pict_type));
      printf("frame_number: %d\n", pCodecContext->frame_number);
      printf("pkt_size: %d\n", pFrame->pkt_size);

      // https://stackoverflow.com/questions/60496654/seeking-to-nearest-keyframe-in-libav-ffmpeg
      // https://stackoverflow.com/questions/39983025/how-to-read-any-frame-while-having-frame-number-using-ffmpeg-av-seek-frame
      // int64_t bytepos = pPacket->dts + 1001*30; // increase by 30 frames
      // av_seek_frame(pFormatContext, 1, bytepos, AVSEEK_FLAG_ANY);
      // av_seek_frame(pFormatContext, 1, 1001*30, AVSEEK_FLAG_ANY);
      if (--how_many_packets_to_process <= 0) break;
   }
    // avformat_close_input(&pFormatContext);
    // av_packet_free(&pPacket);
    // av_frame_free(&pFrame);
    // avcodec_free_context(&pCodecContext);
  }


  return 0;
}

static void logger(const char *fmt, ...)
{
  va_list args;
  fprintf( stderr, "LOG: " );
  va_start( args, fmt );
  vfprintf( stderr, fmt, args );
  va_end( args );
  fprintf( stderr, "\n" );
}
