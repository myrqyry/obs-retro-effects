#pragma once
#include "video-roundtrip.h"
#ifdef __cplusplus
extern "C" {
#endif
struct codec_worker;
struct codec_worker_stats {
    uint64_t submitted, completed, dropped, errors, recovered;
    unsigned max_pending;
};
struct codec_worker *codec_worker_create(const struct retro_video_options *);
void codec_worker_destroy(struct codec_worker *);
bool codec_worker_submit(struct codec_worker *, const uint8_t *, int stride);
bool codec_worker_take(struct codec_worker *, uint8_t *);
bool codec_worker_faulted(struct codec_worker *);
struct codec_worker_stats codec_worker_stats(struct codec_worker *);
#ifdef __cplusplus
}
#endif
