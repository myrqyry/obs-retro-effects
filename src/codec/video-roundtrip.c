#include "video-roundtrip.h"

#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#include <stdlib.h>
#include <string.h>

struct retro_video {
	AVCodecContext *encoder;
	AVCodecContext *decoder;
	AVFrame *input;
	AVFrame *decoded;
	AVPacket *packet;
	struct SwsContext *rgba_to_yuv;
	struct SwsContext *yuv_to_rgba;
	enum AVPixelFormat encode_pixel_format;
	int64_t pts;
	uint64_t bytes;
	uint64_t packets;
	uint64_t keyframes;
};

struct retro_video_preset {
	enum retro_video_codec kind;
	const char *name;
	enum AVCodecID avcodec;
	enum AVPixelFormat input_format;
	int width, height, fps, bitrate, gop;
};

/* Capture targets are examples chosen by video era; not archival device claims.
 * Cinepak and MS Video 1 are RGB; SVQ1 uses YUV410P; MJPEG uses full-range
 * JPEG YUV422P. Do not flatten these to a shared YUV420P pipeline. */
static const struct retro_video_preset presets[] = {
	{RETRO_VIDEO_H263, "H.263 baseline", AV_CODEC_ID_H263, AV_PIX_FMT_YUV420P, 176, 144, 10, 64000, 30},
	{RETRO_VIDEO_MPEG4_PART2, "MPEG-4 Visual Simple Profile", AV_CODEC_ID_MPEG4, AV_PIX_FMT_YUV420P, 176, 144, 10,
	 64000, 30},
	{RETRO_VIDEO_CINEPAK, "Cinepak", AV_CODEC_ID_CINEPAK, AV_PIX_FMT_RGB24, 320, 240, 15, 300000, 15},
	{RETRO_VIDEO_SVQ1, "Sorenson Video 1 (SVQ1)", AV_CODEC_ID_SVQ1, AV_PIX_FMT_YUV410P, 320, 240, 15, 250000, 15},
	{RETRO_VIDEO_MSVIDEO1, "Microsoft Video 1", AV_CODEC_ID_MSVIDEO1, AV_PIX_FMT_RGB555LE, 160, 120, 15, 180000,
	 15},
	{RETRO_VIDEO_FLV1, "Sorenson Spark (FLV1)", AV_CODEC_ID_FLV1, AV_PIX_FMT_YUV420P, 320, 240, 15, 200000, 15},
	{RETRO_VIDEO_MPEG1, "MPEG-1 PAL CD video", AV_CODEC_ID_MPEG1VIDEO, AV_PIX_FMT_YUV420P, 352, 288, 25, 1150000,
	 15},
	{RETRO_VIDEO_MJPEG, "Motion JPEG 4:2:2", AV_CODEC_ID_MJPEG, AV_PIX_FMT_YUVJ422P, 320, 240, 15, 1500000, 15},
};

static const struct retro_video_preset *find_preset(enum retro_video_codec codec)
{
	for (size_t i = 0; i < sizeof(presets) / sizeof(presets[0]); ++i)
		if (presets[i].kind == codec)
			return &presets[i];
	return NULL;
}

struct retro_video_options retro_video_default_options(enum retro_video_codec codec)
{
	const struct retro_video_preset *preset = find_preset(codec);
	if (!preset)
		return (struct retro_video_options){0};
	return (struct retro_video_options){.codec = codec,
					    .width = preset->width,
					    .height = preset->height,
					    .fps = preset->fps,
					    .bitrate = preset->bitrate,
					    .gop_size = preset->gop};
}

const char *retro_video_codec_name(enum retro_video_codec codec)
{
	const struct retro_video_preset *preset = find_preset(codec);
	return preset ? preset->name : NULL;
}

bool retro_video_supported_size(enum retro_video_codec codec, int width, int height)
{
	if (codec == RETRO_VIDEO_H263)
		return (width == 128 && height == 96) || (width == 176 && height == 144) ||
		       (width == 352 && height == 288) || (width == 704 && height == 576) ||
		       (width == 1408 && height == 1152);
	if (codec == RETRO_VIDEO_MPEG4_PART2)
		return width >= 16 && height >= 16 && width <= 2048 && height <= 2048 && width % 16 == 0 &&
		       height % 16 == 0;
	if (find_preset(codec))
		return width >= 16 && height >= 16 && width <= 2048 && height <= 2048 && width % 4 == 0 &&
		       height % 4 == 0;
	return false;
}

void retro_video_destroy(struct retro_video *ctx)
{
	if (!ctx)
		return;
	sws_freeContext(ctx->rgba_to_yuv);
	sws_freeContext(ctx->yuv_to_rgba);
	av_packet_free(&ctx->packet);
	av_frame_free(&ctx->decoded);
	av_frame_free(&ctx->input);
	avcodec_free_context(&ctx->decoder);
	avcodec_free_context(&ctx->encoder);
	free(ctx);
}

struct retro_video *retro_video_create(const struct retro_video_options *options)
{
	if (!options || !retro_video_supported_size(options->codec, options->width, options->height) ||
	    options->fps < 1 || options->fps > 60 || options->bitrate < 1000 || options->bitrate > 20000000 ||
	    options->gop_size < 1 || options->gop_size > 1000)
		return NULL;

	const struct retro_video_preset *preset = find_preset(options->codec);
	if (!preset)
		return NULL;
	const AVCodec *encoder = avcodec_find_encoder(preset->avcodec);
	const AVCodec *decoder = avcodec_find_decoder(preset->avcodec);
	if (encoder && encoder->pix_fmts) {
		bool format_supported = false;
		for (const enum AVPixelFormat *fmt = encoder->pix_fmts; *fmt != AV_PIX_FMT_NONE; ++fmt)
			if (*fmt == preset->input_format)
				format_supported = true;
		if (!format_supported)
			return NULL;
	}
	if (!encoder || !decoder)
		return NULL;

	struct retro_video *ctx = calloc(1, sizeof(*ctx));
	if (!ctx)
		return NULL;

	ctx->encoder = avcodec_alloc_context3(encoder);
	ctx->decoder = avcodec_alloc_context3(decoder);
	ctx->input = av_frame_alloc();
	ctx->decoded = av_frame_alloc();
	ctx->packet = av_packet_alloc();
	if (!ctx->encoder || !ctx->decoder || !ctx->input || !ctx->decoded || !ctx->packet)
		goto error;

	ctx->encoder->width = options->width;
	ctx->encoder->height = options->height;
	ctx->encoder->pix_fmt = preset->input_format;
	ctx->encode_pixel_format = preset->input_format;
	ctx->encoder->time_base = (AVRational){1, options->fps};
	ctx->encoder->framerate = (AVRational){options->fps, 1};
	ctx->encoder->bit_rate = options->bitrate;
	ctx->encoder->gop_size = options->gop_size;
	ctx->encoder->max_b_frames = 0;
	/* MJPEG has full-range image chroma; RGB-family codecs do not use YUV
	 * conversion at all. This is actual encoder input, not a lookalike pass. */
	if (options->codec == RETRO_VIDEO_MJPEG)
		ctx->encoder->color_range = AVCOL_RANGE_JPEG;
	if (options->codec == RETRO_VIDEO_MPEG1)
		ctx->encoder->sample_aspect_ratio = (AVRational){12, 11};
	if (options->codec == RETRO_VIDEO_MPEG4_PART2) {
		ctx->encoder->profile = FF_PROFILE_MPEG4_SIMPLE;
		/* An explicit QCIF 4:3 sample aspect (12:11) in MPEG-4 VOL.
		 * Larger square-pixel modes use 1:1. */
		ctx->encoder->sample_aspect_ratio =
			options->width == 176 && options->height == 144 ? (AVRational){12, 11} : (AVRational){1, 1};
	}

	/* AVI-era codecs such as Cinepak can require frame dimensions from the
	 * container header; our in-memory round-trip has no container. */
	ctx->decoder->width = options->width;
	ctx->decoder->height = options->height;
	if (avcodec_open2(ctx->encoder, encoder, NULL) < 0 || avcodec_open2(ctx->decoder, decoder, NULL) < 0)
		goto error;

	ctx->input->width = options->width;
	ctx->input->height = options->height;
	ctx->input->format = preset->input_format;
	ctx->input->color_range = ctx->encoder->color_range;
	if (av_frame_get_buffer(ctx->input, 32) < 0)
		goto error;

	ctx->rgba_to_yuv = sws_getContext(options->width, options->height, AV_PIX_FMT_RGBA, options->width,
					  options->height, preset->input_format, SWS_BILINEAR, NULL, NULL, NULL);
	if (!ctx->rgba_to_yuv)
		goto error;

	return ctx;
error:
	retro_video_destroy(ctx);
	return NULL;
}

int retro_video_process(struct retro_video *ctx, const uint8_t *rgba, int input_stride, uint8_t *decoded,
			int decoded_stride)
{
	if (!ctx || !rgba || !decoded || input_stride < ctx->encoder->width * 4 ||
	    decoded_stride < ctx->encoder->width * 4)
		return -1;

	if (av_frame_make_writable(ctx->input) < 0)
		return -1;

	const uint8_t *source[4] = {rgba, NULL, NULL, NULL};
	int strides[4] = {input_stride, 0, 0, 0};
	if (sws_scale(ctx->rgba_to_yuv, source, strides, 0, ctx->encoder->height, ctx->input->data,
		      ctx->input->linesize) != ctx->encoder->height)
		return -1;

	ctx->input->pts = ctx->pts++;
	if (avcodec_send_frame(ctx->encoder, ctx->input) < 0)
		return -1;

	int have_frame = 0;
	int code;
	while ((code = avcodec_receive_packet(ctx->encoder, ctx->packet)) == 0) {
		ctx->bytes += ctx->packet->size;
		ctx->packets++;
		if (ctx->packet->flags & AV_PKT_FLAG_KEY)
			ctx->keyframes++;

		if (avcodec_send_packet(ctx->decoder, ctx->packet) < 0) {
			av_packet_unref(ctx->packet);
			return -1;
		}
		av_packet_unref(ctx->packet);

		while ((code = avcodec_receive_frame(ctx->decoder, ctx->decoded)) == 0) {
			ctx->yuv_to_rgba = sws_getCachedContext(ctx->yuv_to_rgba, ctx->decoded->width,
								ctx->decoded->height,
								(enum AVPixelFormat)ctx->decoded->format,
								ctx->decoded->width, ctx->decoded->height,
								AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL, NULL, NULL);
			if (!ctx->yuv_to_rgba) {
				av_frame_unref(ctx->decoded);
				return -1;
			}
			uint8_t *target[4] = {decoded, NULL, NULL, NULL};
			int target_stride[4] = {decoded_stride, 0, 0, 0};
			if (sws_scale(ctx->yuv_to_rgba, (const uint8_t *const *)ctx->decoded->data,
				      ctx->decoded->linesize, 0, ctx->decoded->height, target,
				      target_stride) != ctx->decoded->height) {
				av_frame_unref(ctx->decoded);
				return -1;
			}
			av_frame_unref(ctx->decoded);
			have_frame = 1;
		}
		if (code != AVERROR(EAGAIN) && code != AVERROR_EOF)
			return -1;
	}

	return code == AVERROR(EAGAIN) || code == AVERROR_EOF ? have_frame : -1;
}

uint64_t retro_video_encoded_bytes(const struct retro_video *ctx)
{
	return ctx ? ctx->bytes : 0;
}

uint64_t retro_video_encoded_packets(const struct retro_video *ctx)
{
	return ctx ? ctx->packets : 0;
}

uint64_t retro_video_encoded_keyframes(const struct retro_video *ctx)
{
	return ctx ? ctx->keyframes : 0;
}

int retro_video_decoded_profile(const struct retro_video *video)
{
	return video ? video->decoder->profile : -1;
}

int retro_video_decoded_sar_num(const struct retro_video *video)
{
	return video ? video->decoder->sample_aspect_ratio.num : 0;
}

int retro_video_decoded_sar_den(const struct retro_video *video)
{
	return video ? video->decoder->sample_aspect_ratio.den : 0;
}
