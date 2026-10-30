#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* A deliberately small, hardware-like 5x7 character generator at a fixed
 * low-resolution canvas; texture is point-sampled after the VHS tape shader. */
#define VHS_OSD_WIDTH 320
#define VHS_OSD_HEIGHT 240
#define VHS_OSD_BYTES (VHS_OSD_WIDTH * VHS_OSD_HEIGHT * 4)
enum vhs_osd_mode { VHS_OSD_HIDDEN = 0, VHS_OSD_TIMED = 1, VHS_OSD_ALWAYS = 2, VHS_OSD_COUNTER = 3 };
enum vhs_osd_style { VHS_OSD_VCR = 0, VHS_OSD_CAMCORDER = 1 };
enum vhs_osd_transport {
	VHS_OSD_STOP = 0,
	VHS_OSD_PLAY = 1,
	VHS_OSD_PAUSE = 2,
	VHS_OSD_REWIND = 3,
	VHS_OSD_FAST_FORWARD = 4,
	VHS_OSD_REC = 5
};
enum vhs_osd_speed { VHS_OSD_SP = 0, VHS_OSD_LP = 1, VHS_OSD_EP = 2 };

struct vhs_osd_state {
	int mode, style, transport, speed, channel;
	double seconds;
	bool full_status;
};

bool vhs_osd_render(uint8_t *rgba, size_t capacity, const struct vhs_osd_state *state);
double vhs_osd_advance(double position, double delta, int transport);
bool vhs_osd_status_visible(int mode, double seconds_since_control_change);
