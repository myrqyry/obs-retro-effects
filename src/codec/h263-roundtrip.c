#include "h263-roundtrip.h"

#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#include <stdlib.h>

struct retro_h263 {
	AVCodecContext *encoder;
	AVCodecContext *decoder;
	AVFrame *input;
	AVFrame *decoded;
	AVPacket *packet;
	struct SwsContext *rgba_to_yuv;
	struct SwsContext *yuv_to_rgba;
	int64_t pts;
	uint64_t bytes;
	uint64_t packets;
};

bool retro_h263_supported_size(int width, int height)
{
	return (width == 128 && height == 96) || (width == 176 && height == 144) || (width == 352 && height == 288) ||
	       (width == 704 && height == 576) || (width == 1408 && height == 1152);
}

void retro_h263_destroy(struct retro_h263 *ctx)
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

struct retro_h263 *retro_h263_create(const struct retro_h263_options *options)
{
	if (!options || !retro_h263_supported_size(options->width, options->height) || options->fps < 1 ||
	    options->fps > 60 || options->bitrate < 1000 || options->bitrate > 20000000 || options->gop_size < 1 ||
	    options->gop_size > 1000)
		return NULL;

	const AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_H263);
	const AVCodec *decoder = avcodec_find_decoder(AV_CODEC_ID_H263);
	if (!encoder || !decoder)
		return NULL;

	struct retro_h263 *ctx = calloc(1, sizeof(*ctx));
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
	ctx->encoder->pix_fmt = AV_PIX_FMT_YUV420P;
	ctx->encoder->time_base = (AVRational){1, options->fps};
	ctx->encoder->framerate = (AVRational){options->fps, 1};
	ctx->encoder->bit_rate = options->bitrate;
	ctx->encoder->gop_size = options->gop_size;
	ctx->encoder->max_b_frames = 0;

	if (avcodec_open2(ctx->encoder, encoder, NULL) < 0 || avcodec_open2(ctx->decoder, decoder, NULL) < 0)
		goto error;

	ctx->input->width = options->width;
	ctx->input->height = options->height;
	ctx->input->format = AV_PIX_FMT_YUV420P;
	if (av_frame_get_buffer(ctx->input, 32) < 0)
		goto error;

	ctx->rgba_to_yuv = sws_getContext(options->width, options->height, AV_PIX_FMT_RGBA, options->width,
					  options->height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);
	ctx->yuv_to_rgba = sws_getContext(options->width, options->height, AV_PIX_FMT_YUV420P, options->width,
					  options->height, AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL, NULL, NULL);
	if (!ctx->rgba_to_yuv || !ctx->yuv_to_rgba)
		goto error;

	return ctx;
error:
	retro_h263_destroy(ctx);
	return NULL;
}

int retro_h263_process(struct retro_h263 *ctx, const uint8_t *rgba, int input_stride, uint8_t *decoded,
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

		if (avcodec_send_packet(ctx->decoder, ctx->packet) < 0) {
			av_packet_unref(ctx->packet);
			return -1;
		}
		av_packet_unref(ctx->packet);

		while ((code = avcodec_receive_frame(ctx->decoder, ctx->decoded)) == 0) {
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

uint64_t retro_h263_encoded_bytes(const struct retro_h263 *ctx)
{
	return ctx ? ctx->bytes : 0;
}

uint64_t retro_h263_encoded_packets(const struct retro_h263 *ctx)
{
	return ctx ? ctx->packets : 0;
}
