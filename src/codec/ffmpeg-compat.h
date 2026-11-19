#pragma once
#include <stdbool.h>

#include <libavcodec/avcodec.h>
#include <libavcodec/version.h>

/* FFmpeg 8 removes the old FF_PROFILE_* aliases. */
#ifdef AV_PROFILE_MPEG4_SIMPLE
#define RETRO_MPEG4_SIMPLE_PROFILE AV_PROFILE_MPEG4_SIMPLE
#else
#define RETRO_MPEG4_SIMPLE_PROFILE FF_PROFILE_MPEG4_SIMPLE
#endif

/* The newer query API appeared in FFmpeg 7.1. Keep the public legacy
 * query for older SDKs, and avoid deprecated AVCodec fields on new ones. */
static inline bool retro_encoder_supports_format(const AVCodec *encoder, enum AVPixelFormat format)
{
    if (!encoder)
        return false;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 19, 100)
    const void *configs = NULL;
    int count = 0;
    if (avcodec_get_supported_config(NULL, encoder, AV_CODEC_CONFIG_PIX_FORMAT, 0, &configs, &count) < 0)
        return false;
    if (!configs)
        return true;
    const enum AVPixelFormat *formats = configs;
    for (int i = 0; i < count; ++i)
        if (formats[i] == format)
            return true;
#else
    if (!encoder->pix_fmts)
        return true;
    for (const enum AVPixelFormat *p = encoder->pix_fmts; *p != AV_PIX_FMT_NONE; ++p)
        if (*p == format)
            return true;
#endif
    return false;
}
