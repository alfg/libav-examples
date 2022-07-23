#include <stdio.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>

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
  AVCodec *video_in_avc;
  AVCodecContext *video_in_avcc;
  AVCodecContext *video_out_avcc;
  AVCodec *audio_in_avc;
  AVCodecContext *audio_in_avcc;
  AVCodecContext *audio_out_avcc;

  for (int i = 0; i < input_fmt_ctx->nb_streams; i++) {
	if (input_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
	  video_in_stream = input_fmt_ctx->streams[i];
	  video_in_avc = avcodec_find_decoder(video_in_stream->codecpar->codec_id);
	  video_in_avcc = avcodec_alloc_context3(video_in_avc);
	  avcodec_parameters_to_context(video_in_avcc, video_in_stream->codecpar);
	  avcodec_open2(video_in_avcc, video_in_avc, NULL);

	  video_out_stream = avformat_new_stream(output_fmt_ctx, NULL);
	  AVRational input_framerate = av_guess_frame_rate(input_fmt_ctx, video_in_stream, NULL);
//	  AVStream *video_avs = avformat_new_stream

	  char *codec_name = "libx265";
	  char *codec_priv_key = "x265-params";
	  char *codec_priv_value = "keyint=60:min-keyint=60:scenecut=0";

	  AVCodec *video_out_avc = avcodec_find_encoder_by_name(codec_name);
	  video_out_avcc = avcodec_alloc_context3(video_out_avc);

	  av_opt_set(video_out_avcc->priv_data, codec_priv_key, codec_priv_value, 0);
	  video_out_avcc->height = video_in_avcc->height;
	  video_out_avcc->width = video_in_avcc->width;
	  video_out_avcc->pix_fmt = video_out_avc->pix_fmts[0];
	  video_out_avcc->bit_rate = 2 * 1000 * 1000;
	  video_out_avcc->rc_buffer_size = 4 * 1000 * 1000;
	  video_out_avcc->rc_max_rate = 2 * 1000 * 1000;
	  video_out_avcc->rc_min_rate = 2.5 * 1000 * 1000;
	  video_out_avcc->time_base = av_inv_q(input_framerate);
	  video_out_stream->time_base = video_out_avcc->time_base;

	  int ret = avcodec_open2(video_out_avcc, video_out_avc, NULL);
	  ret = avcodec_parameters_from_context(video_out_stream->codecpar, video_out_avcc);

//	  if (avcodec_parameters_copy(video_out_stream->codecpar, video_in_stream->codecpar) < 0) {
//		return -1;
//	  }
	} else if (input_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
	  audio_in_stream = input_fmt_ctx->streams[i];
	  audio_in_avc = avcodec_find_decoder(audio_in_stream->codecpar->codec_id);
	  audio_in_avcc = avcodec_alloc_context3(audio_in_avc);
	  avcodec_parameters_to_context(audio_in_avcc, audio_in_stream->codecpar);
	  avcodec_open2(audio_in_avcc, audio_in_avc, NULL);

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

  int video_frame_count = 0;
  int audio_frame_count = 0;
  while (av_read_frame(input_fmt_ctx, packet) >= 0) {
	if (input_fmt_ctx->streams[packet->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {

	  // decode
	  int response = avcodec_send_packet(video_in_avcc, packet);
	  if (response >= 0) {
		response = avcodec_receive_frame(video_in_avcc, frame);
		if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
		  break;
		} else if (response < 0) {
		  return response;
		}
//		av_frame_unref(frame);
	  }
//	  av_packet_unref(packet);

	  // encode
	  AVPacket *output_packet = av_packet_alloc();
	  response = avcodec_send_frame(video_out_avcc, frame);
	  while (response >= 0) {
		response = avcodec_receive_packet(video_out_avcc, output_packet);
		if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
		  break;
		} else if (response < 0) {
		  return -1;
		}

		output_packet->stream_index = packet->stream_index;
		output_packet->duration = video_out_stream->time_base.den / video_out_stream->time_base.num / video_in_stream->avg_frame_rate.num * video_in_stream->avg_frame_rate.den;
		av_packet_rescale_ts(packet, video_in_stream->time_base, video_out_stream->time_base);
		av_interleaved_write_frame(output_fmt_ctx, output_packet);
	  }

	  av_packet_unref(output_packet);
	  av_packet_free(&output_packet);

	  av_frame_unref(frame);
	  av_packet_unref(packet);

	  video_frame_count++;
	} else if (input_fmt_ctx->streams[packet->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
	  av_packet_rescale_ts(packet, audio_in_stream->time_base, audio_out_stream->time_base);
	  av_interleaved_write_frame(output_fmt_ctx, packet);
	  audio_frame_count++;
	}
  }
  printf("wrote %d video frames\n", video_frame_count);
  printf("wrote %d audio samples\n", audio_frame_count);

  // Write stream trailer to the output media file.
  if (av_write_trailer(output_fmt_ctx) < 0){
	return -1;
  };
}
