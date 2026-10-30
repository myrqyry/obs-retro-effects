#include "vhs-osd.h"
#include <stdio.h>
#include <string.h>

/* Five columns and seven rows, MSB at the left. Not a generic UI font:
 * each character is drawn as a discrete monochrome device bitmap. */
struct glyph {
	char c;
	uint8_t row[7];
};
static const struct glyph glyphs[] = {
	{'A', {14, 17, 17, 31, 17, 17, 17}}, {'B', {30, 17, 17, 30, 17, 17, 30}}, {'C', {14, 17, 16, 16, 16, 17, 14}},
	{'D', {30, 17, 17, 17, 17, 17, 30}}, {'E', {31, 16, 16, 30, 16, 16, 31}}, {'F', {31, 16, 16, 30, 16, 16, 16}},
	{'G', {14, 17, 16, 23, 17, 17, 14}}, {'H', {17, 17, 17, 31, 17, 17, 17}}, {'I', {31, 4, 4, 4, 4, 4, 31}},
	{'J', {7, 2, 2, 2, 18, 18, 12}},     {'K', {17, 18, 20, 24, 20, 18, 17}}, {'L', {16, 16, 16, 16, 16, 16, 31}},
	{'M', {17, 27, 21, 21, 17, 17, 17}}, {'N', {17, 25, 21, 19, 17, 17, 17}}, {'O', {14, 17, 17, 17, 17, 17, 14}},
	{'P', {30, 17, 17, 30, 16, 16, 16}}, {'Q', {14, 17, 17, 17, 21, 18, 13}}, {'R', {30, 17, 17, 30, 20, 18, 17}},
	{'S', {15, 16, 16, 14, 1, 1, 30}},   {'T', {31, 4, 4, 4, 4, 4, 4}},       {'U', {17, 17, 17, 17, 17, 17, 14}},
	{'V', {17, 17, 17, 17, 17, 10, 4}},  {'W', {17, 17, 17, 21, 21, 21, 10}}, {'X', {17, 17, 10, 4, 10, 17, 17}},
	{'Y', {17, 17, 10, 4, 4, 4, 4}},     {'Z', {31, 1, 2, 4, 8, 16, 31}},     {'0', {14, 17, 19, 21, 25, 17, 14}},
	{'1', {4, 12, 4, 4, 4, 4, 14}},      {'2', {14, 17, 1, 2, 4, 8, 31}},     {'3', {30, 1, 1, 14, 1, 1, 30}},
	{'4', {2, 6, 10, 18, 31, 2, 2}},     {'5', {31, 16, 30, 1, 1, 17, 14}},   {'6', {6, 8, 16, 30, 17, 17, 14}},
	{'7', {31, 1, 2, 4, 8, 8, 8}},       {'8', {14, 17, 17, 14, 17, 17, 14}}, {'9', {14, 17, 17, 15, 1, 2, 12}},
	{':', {0, 4, 4, 0, 4, 4, 0}},        {'-', {0, 0, 0, 31, 0, 0, 0}},       {'/', {1, 1, 2, 4, 8, 16, 16}},
	{' ', {0, 0, 0, 0, 0, 0, 0}}};

static const uint8_t *bitmap(char c)
{
	for (size_t i = 0; i < sizeof(glyphs) / sizeof(glyphs[0]); ++i)
		if (glyphs[i].c == c)
			return glyphs[i].row;
	return glyphs[sizeof(glyphs) / sizeof(glyphs[0]) - 1].row;
}

static void dot(uint8_t *rgba, int x, int y, uint8_t bright)
{
	if (x < 0 || y < 0 || x >= VHS_OSD_WIDTH || y >= VHS_OSD_HEIGHT)
		return;
	size_t idx = ((size_t)y * VHS_OSD_WIDTH + x) * 4;
	rgba[idx] = rgba[idx + 1] = rgba[idx + 2] = bright;
	rgba[idx + 3] = 255;
}

static void text(uint8_t *rgba, int x, int y, const char *word)
{
	for (; *word; ++word, x += 6) {
		const uint8_t *g = bitmap(*word);
		for (int row = 0; row < 7; row++)
			for (int col = 0; col < 5; col++)
				if (g[row] & (16u >> col)) {
					/* A single dark stroke on right/bottom simulates the
					 * black-edged, high-contrast OSD lettering. */
					dot(rgba, x + col + 1, y + row + 1, 0);
					dot(rgba, x + col, y + row, 244);
				}
	}
}

bool vhs_osd_status_visible(int mode, double seconds_since_control_change)
{
	return mode != VHS_OSD_TIMED || seconds_since_control_change < 5.;
}

double vhs_osd_advance(double position, double delta, int transport)
{
	if (delta <= 0)
		return position;
	double rate = transport == VHS_OSD_PLAY || transport == VHS_OSD_REC ? 1.
		      : transport == VHS_OSD_FAST_FORWARD                   ? 4.
		      : transport == VHS_OSD_REWIND                         ? -4.
									    : 0.;
	double next = position + rate * delta;
	return next > 359999. ? 359999. : next < 0 ? 0 : next;
}

bool vhs_osd_render(uint8_t *rgba, size_t capacity, const struct vhs_osd_state *s)
{
	if (!rgba || capacity < VHS_OSD_BYTES || !s)
		return false;
	memset(rgba, 0, VHS_OSD_BYTES);
	if (s->mode == VHS_OSD_HIDDEN)
		return true;

	unsigned sec = (unsigned)(s->seconds > 0. ? s->seconds : 0.);
	if (sec > 359999u)
		sec = 359999u;
	unsigned hour = sec / 3600u, minute = (sec / 60u) % 60u, second = sec % 60u;
	char counter[20];
	snprintf(counter, sizeof(counter), "%u:%02u:%02u", hour, minute, second);

	if (s->mode == VHS_OSD_COUNTER || (s->mode == VHS_OSD_TIMED && !s->full_status)) {
		text(rgba, 116, 207, counter);
		return true;
	}
	const char *motion = s->transport == VHS_OSD_PLAY           ? "PLAY"
			     : s->transport == VHS_OSD_REC          ? "REC"
			     : s->transport == VHS_OSD_PAUSE        ? "PAUSE"
			     : s->transport == VHS_OSD_REWIND       ? "REW"
			     : s->transport == VHS_OSD_FAST_FORWARD ? "FF"
								    : "STOP";
	const char *speed = s->speed == VHS_OSD_LP ? "LP" : s->speed == VHS_OSD_EP ? "EP" : "SP";
	char channel[16];
	int ch = s->channel < 1 ? 1 : s->channel > 125 ? 125 : s->channel;
	snprintf(channel, sizeof(channel), "CH %02d", ch);

	if (s->style == VHS_OSD_CAMCORDER) {
		/* Separate viewfinder layout: only text indicators, no recording
		 * dot, circle, reticle, blinking border, or fake camera HUD. */
		text(rgba, 18, 18, speed);
		text(rgba, 245, 18, s->transport == VHS_OSD_STOP ? "STBY" : motion);
		text(rgba, 18, 207, counter);
	} else {
		text(rgba, 18, 18, motion);
		text(rgba, 265, 18, channel);
		text(rgba, 18, 207, speed);
		text(rgba, 116, 207, counter);
	}
	return true;
}
