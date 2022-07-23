#include <stdio.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

//static int open_input_file(const char *filename) {
//  int ret;
//  AVCodec *dec;
//
//  // Open the file and read header.
//  if ((ret = avformat_open_input(&fmt_ctx, filename, NULL, NULL)) != 0) {
//	printf("ERROR: could not open the file\n");
//	av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
//	return ret;
//  }
//
//  // Get the stream info to select the video stream.
//  if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
//	av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
//	return ret;
//  }
//
//  ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
//  if (ret < 0) {
//	av_log(NULL, AV_LOG_ERROR,
//		   "Cannot find a video stream in the input file\n");
//	return ret;
//  }
//  video_stream_index = ret;
//
//  // Create the decoding context.
//  dec_ctx = avcodec_alloc_context3(dec);
//  if (!dec_ctx) return AVERROR(ENOMEM);
//  avcodec_parameters_to_context(dec_ctx,
//								fmt_ctx->streams[video_stream_index]->codecpar);
//
//  // Init the video decoder.
//  if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0) {
//	av_log(NULL, AV_LOG_ERROR, "Cannot open video decoder\n");
//	return ret;
//  }
//  return 0;
//}

static AVFormatContext *input_fmt_ctx;
static AVFormatContext *output_fmt_ctx;

static AVCodecContext *dec_ctx;
static int video_stream_index = -1;

int main(int argc, const char *argv[]) {
  if (argc < 3) {
	printf("Please specify an input and output media file.\n");
	return -1;
  }

  // In and out files from cli args.
  const char *in_filename, *out_filename;
  in_filename = argv[1];
  out_filename = argv[2];

  // Open the file and read its header. The codecs are not opened.
  if (avformat_open_input(&input_fmt_ctx, in_filename, NULL, NULL) != 0) {
	printf("ERROR could not open the file\n");
	return -1;
  }

  // Read media file to get stream information.
  if (avformat_find_stream_info(input_fmt_ctx, NULL) < 0) {
	return -1;
  }

  // Prepare output file.
  avformat_alloc_output_context2(&output_fmt_ctx, NULL, NULL, out_filename);

  // Decoder setup for each track.
  AVStream *video_in_stream;
  AVStream *video_out_stream;
  AVStream *audio_in_stream;
  AVStream *audio_out_stream;
  for (int i = 0; i < input_fmt_ctx->nb_streams; i++) {
	if (input_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
	  video_in_stream = input_fmt_ctx->streams[i];
	  AVCodec *avc = avcodec_find_decoder(video_in_stream->codecpar->codec_id);
	  AVCodecContext *avcc = avcodec_alloc_context3(avc);
	  avcodec_parameters_to_context(avcc, video_in_stream->codecpar);
	  avcodec_open2(avcc, avc, NULL);

	  video_out_stream = avformat_new_stream(output_fmt_ctx, NULL);
	  if (avcodec_parameters_copy(video_out_stream->codecpar, video_in_stream->codecpar) < 0) {
		return -1;
	  }
	} else if (input_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
	  audio_in_stream = input_fmt_ctx->streams[i];
	  AVCodec *avc = avcodec_find_decoder(audio_in_stream->codecpar->codec_id);
	  AVCodecContext *avcc = avcodec_alloc_context3(avc);
	  avcodec_parameters_to_context(avcc, audio_in_stream->codecpar);
	  avcodec_open2(avcc, avc, NULL);

	  audio_out_stream = avformat_new_stream(output_fmt_ctx, NULL);
	  if (avcodec_parameters_copy(audio_out_stream->codecpar, audio_in_stream->codecpar) < 0) {
		return -1;
	  }
	}
  }


  // Write global header.
  if(output_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
	output_fmt_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }

  // Open output file for writing.
  avio_open(&output_fmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);

  // Set any muxer flags (if fragmented).
  AVDictionary* muxer_opts = NULL;
//  av_dict_set(&muxer_opts, "movflags", "frag_keyframe+empty_moov+delay_moov+default_base_moof", 0);
  if (avformat_write_header(output_fmt_ctx, &muxer_opts) < 0) return -1;


  // Write frames/packets to the AVStreams.
  AVPacket *packet = av_packet_alloc();
  AVFrame *frame = av_frame_alloc();
  if (!packet || !frame) {
	perror("could not allocate frame");
	return -1;
  }

  while (av_read_frame(input_fmt_ctx, packet) >= 0) {
	printf("write_frame\n");
	if (input_fmt_ctx->streams[packet->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
	  av_packet_rescale_ts(packet, video_in_stream->time_base, video_out_stream->time_base);
	  av_interleaved_write_frame(output_fmt_ctx, packet);
	} else if (input_fmt_ctx->streams[packet->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
	  av_packet_rescale_ts(packet, audio_in_stream->time_base, audio_out_stream->time_base);
	  av_interleaved_write_frame(output_fmt_ctx, packet);
	}
  }

  // Write stream trailer to the output media file.
  if (av_write_trailer(output_fmt_ctx) < 0){
	return -1;
  };
}
