#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Actual FFmpeg encoder -> decoder for early 3GP video payloads.
 * Both use YUV420P, no B frames; no muxing, fake artifacts or packet loss.
 */
enum retro_video_codec {
	RETRO_VIDEO_H263 = 1,        /* ITU-T H.263 baseline */
	RETRO_VIDEO_MPEG4_PART2 = 2, /* MPEG-4 Visual Part 2, Simple Profile */
	RETRO_VIDEO_CINEPAK = 3,     /* RGB vector quantization, CD-ROM video */
	RETRO_VIDEO_SVQ1 = 4,        /* Sorenson Video 1, YUV410P */
	RETRO_VIDEO_MSVIDEO1 = 5,    /* Microsoft Video 1, RGB555LE */
	RETRO_VIDEO_FLV1 = 6,        /* Sorenson Spark / Flash H.263 variant */
	RETRO_VIDEO_MPEG1 = 7,       /* MPEG-1 video */
	RETRO_VIDEO_MJPEG = 8,       /* Intra-frame JPEG, 4:2:2 */
};

struct retro_video;

struct retro_video_options {
	enum retro_video_codec codec;
	int width;
	int height;
	int fps;
	int bitrate;                 /* target bits/second */
	int gop_size;                /* keyframe distance in frames */
	int cinepak_codebook_passes; /* 0..5, encoder codebook iterations */
	int svq1_motion_estimation;  /* 0..2, zero/EPZS/X1 */
	int mjpeg_quantizer;         /* 2..31, lower is higher quality, 0=default */
};

/* Distinct, illustrative starting profiles. Codec-specific historical defaults
 * vary by capture device; they are not claims about every old video file.
 * All currently map to 4:3 display, including non-square-pixel QCIF/PAL.
 */
struct retro_video_options retro_video_default_options(enum retro_video_codec codec);
const char *retro_video_codec_name(enum retro_video_codec codec);
bool retro_video_supported_size(enum retro_video_codec codec, int width, int height);
struct retro_video *retro_video_create(const struct retro_video_options *options);
void retro_video_destroy(struct retro_video *video);

/* RGBA top-to-bottom; stride >= 4 * width.
 * 1 decoded frame available, 0 codec still buffering, -1 error.
 * Caller must provide input in temporal order at the selected fps.
 */
int retro_video_process(struct retro_video *video, const uint8_t *rgba, int rgba_stride, uint8_t *decoded,
			int decoded_stride);
uint64_t retro_video_encoded_bytes(const struct retro_video *video);
uint64_t retro_video_encoded_packets(const struct retro_video *video);
uint64_t retro_video_encoded_keyframes(const struct retro_video *video);

/* Parsed from the decoder's actual video bitstream after frame decoding.
 * Diagnostic metadata: FFmpeg profile ID and sample-aspect numerator/denominator.
 */
int retro_video_decoded_profile(const struct retro_video *video);
int retro_video_decoded_sar_num(const struct retro_video *video);
int retro_video_decoded_sar_den(const struct retro_video *video);
