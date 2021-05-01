#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

static void save_ppm_frame(unsigned char *buf, int wrap, int xsize, int ysize, char *filename);

int main(int argc, const char *argv[]) {
  if (argc < 2) {
    printf("You need to specify a media file.\n");
    return -1;
  }

  // Allocate avformat context.
  AVFormatContext *pFormatContext = avformat_alloc_context();
  if (!pFormatContext) {
    printf("ERROR could not allocate memory for Format Context\n");
    return -1;
  }

  // Open the file and read its header. The codecs are not opened.
  if (avformat_open_input(&pFormatContext, argv[1], NULL, NULL) != 0) {
    printf("ERROR could not open the file\n");
    return -1;
  }

  printf("format %s, duration %ld us, bit_rate %ld\n", pFormatContext->iformat->name, pFormatContext->duration, pFormatContext->bit_rate);
  printf("finding stream info from format");

  // Get the stream info so we can iterate the streams.
  if (avformat_find_stream_info(pFormatContext,  NULL) < 0) {
    printf("ERROR could not get the stream info\n");
    return -1;
  }

  AVCodec *pCodec = NULL;
  AVCodecParameters *pCodecParameters =  NULL;

  int video_stream_index = -1;

  // loop though all the streams and print its main information
  for (int i = 0; i < pFormatContext->nb_streams; i++) {
    AVCodecParameters *pLocalCodecParameters =  NULL;
    pLocalCodecParameters = pFormatContext->streams[i]->codecpar;

    printf("AVStream->time_base before open coded %d/%d \n", pFormatContext->streams[i]->time_base.num, pFormatContext->streams[i]->time_base.den);
    printf("AVStream->r_frame_rate before open coded %d/%d \n", pFormatContext->streams[i]->r_frame_rate.num, pFormatContext->streams[i]->r_frame_rate.den);
    printf("AVStream->start_time %" PRId64 "\n", pFormatContext->streams[i]->start_time);
    printf("AVStream->duration %" PRId64 "\n", pFormatContext->streams[i]->duration);

    AVCodec *pLocalCodec = NULL;

    // finds the registered decoder for a codec ID.
    pLocalCodec = avcodec_find_decoder(pLocalCodecParameters->codec_id);

    if (pLocalCodec == NULL) {
      printf("ERROR unsupported codec!\n");
      return -1;
    }

    // When the stream is a video we store its index, codec parameters and codec.
    if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
      if (video_stream_index == -1) {
        video_stream_index = i;
        pCodec = pLocalCodec;
        pCodecParameters = pLocalCodecParameters;
      }

      printf("Video Codec: resolution %d x %d\n", pLocalCodecParameters->width, pLocalCodecParameters->height);
    } else if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_AUDIO) {
      printf("Audio Codec: %d channels, sample rate %d\n", pLocalCodecParameters->channels, pLocalCodecParameters->sample_rate);
    }

    // Print its name, id and bitrate.
    printf("\tCodec %s ID %d bit_rate %ld\n", pLocalCodec->name, pLocalCodec->id, pLocalCodecParameters->bit_rate);
  }

  // Allocate for codec context.
  AVCodecContext *pCodecContext = avcodec_alloc_context3(pCodec);
  if (!pCodecContext) {
    printf("failed to allocated memory for AVCodecContext\n");
    return -1;
  }

  // Fill the codec context based on the values from the supplied codec parameters.
  if (avcodec_parameters_to_context(pCodecContext, pCodecParameters) < 0) {
    printf("failed to copy codec params to codec context\n");
    return -1;
  }

  // Initialize the AVCodecContext to use the given AVCodec.
  if (avcodec_open2(pCodecContext, pCodec, NULL) < 0) {
    printf("failed to open codec through avcodec_open2\n");
    return -1;
  }

  // Allocate for frames.
  AVFrame *pFrame = av_frame_alloc();
  if (!pFrame) {
    printf("failed to allocated memory for AVFrame\n");
    return -1;
  }

  // Allocate for RGB frames.
  AVFrame *pFrameRGB = av_frame_alloc();
  if (!pFrameRGB) {
    printf("failed to allocated memory for AVFrame\n");
    return -1;
  }

  // Get image buffer size.
  int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24,
      pCodecContext->width,
      pCodecContext->height,
      16);

  // Allocate image buffer with size from previous function.
  uint8_t *buffer = NULL;
  buffer = av_malloc(numBytes);

  // Fill the buffer with image data needed for swscale.
  av_image_fill_arrays(pFrameRGB->data,
    pFrameRGB->linesize,
    buffer,
    AV_PIX_FMT_RGB24,
    pCodecContext->width,
    pCodecContext->height, 1);

  // Create the swscale context for the images. We only need to set this once.
  struct SwsContext *sws_ctx = NULL;
  sws_ctx = sws_getContext(
    pCodecContext->width,
    pCodecContext->height,
    pCodecContext->pix_fmt,
    pCodecContext->width,
    pCodecContext->height,
    AV_PIX_FMT_RGB24,
    SWS_BILINEAR,
    NULL, NULL, NULL);

  // Allocate for compressed AVPacket.
  AVPacket *pPacket = av_packet_alloc();
  if (!pPacket) {
    printf("failed to allocated memory for AVPacket\n");
    return -1;
  }

  int response = 0;
  int how_many_packets_to_process = 8;

  // Fill the Packet with data from the Stream.
  while (av_read_frame(pFormatContext, pPacket) >= 0) {

    // If it's the video stream.
    if (pPacket->stream_index == video_stream_index) {
      printf("AVPacket->pts %" PRId64 "\n", pPacket->pts);

      // Decode the packets to frames and apply sws_scale to populate FrameRGB data.
      int response2 = 0;
      response2 = avcodec_send_packet(pCodecContext, pPacket);
      while (response2 >= 0) {
        response2 = avcodec_receive_frame(pCodecContext, pFrame);
        if (response2 == AVERROR(EAGAIN) || response2 == AVERROR_EOF) {
          break;
        } else if (response2 < 0) {
          printf("Error while receiving a frame from the decoder: %s\n", av_err2str(response));
        }

        sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
            pFrame->linesize, 0, pCodecContext->height,
            pFrameRGB->data, pFrameRGB->linesize);

        // Save the frame into PPM format (raw rgb).
        char frame_filename[1024];
        snprintf(frame_filename, sizeof(frame_filename), "%s-%d.ppm", "frame", pCodecContext->frame_number);
        save_ppm_frame(pFrame->data[0], pFrame->linesize[0], pCodecContext->width, pCodecContext->height, frame_filename);
      }

      if (response < 0)
        break;

      // Stop it, otherwise we'll be saving hundreds of frames.
      if (--how_many_packets_to_process <= 0) break;
    }

    av_packet_unref(pPacket);
  }

  printf("releasing all the resources\n");

  avformat_close_input(&pFormatContext);
  av_packet_free(&pPacket);
  av_frame_free(&pFrame);
  avcodec_free_context(&pCodecContext);
  return 0;
}

static void save_ppm_frame(unsigned char *buf, int wrap, int xsize, int ysize, char *filename)
{
    FILE *f;
    int i;
    f = fopen(filename,"w");
    // Writing the minimal required header for a ppm file format.
    // https://en.wikipedia.org/wiki/Netpbm#PPM_example
    fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);

    // Line by line.
    for (i = 0; i < ysize; i++)
        fwrite(buf + i * wrap, 1, xsize, f);
    fclose(f);
}
