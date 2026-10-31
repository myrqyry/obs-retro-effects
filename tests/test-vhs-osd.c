#include "../src/filters/vhs-osd.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned count_alpha(const unsigned char *data)
{
	unsigned n = 0;
	for (int i = 0; i < VHS_OSD_WIDTH * VHS_OSD_HEIGHT; i++)
		n += data[i * 4 + 3] != 0;
	return n;
}
int main(void)
{
	unsigned char out[VHS_OSD_WIDTH * VHS_OSD_HEIGHT * 4];
	struct vhs_osd_state s = {.mode = VHS_OSD_HIDDEN,
				  .style = VHS_OSD_VCR,
				  .transport = VHS_OSD_PLAY,
				  .speed = VHS_OSD_SP,
				  .channel = 3,
				  .seconds = 75,
				  .full_status = true};
	assert(vhs_osd_render(out, sizeof(out), &s));
	assert(count_alpha(out) == 0);
	s.mode = VHS_OSD_ALWAYS;
	assert(vhs_osd_render(out, sizeof(out), &s));
	unsigned full = count_alpha(out);
	assert(full > 100 && full < 10000);
	/* Status at upper-left and counter at lower area. */
	unsigned upper = 0, lower = 0;
	for (int y = 0; y < VHS_OSD_HEIGHT; y++)
		for (int x = 0; x < VHS_OSD_WIDTH; x++)
			if (out[(y * VHS_OSD_WIDTH + x) * 4 + 3]) {
				upper += y < 60;
				lower += y > 185;
			}
	assert(upper && lower);
	s.mode = VHS_OSD_TIMED;
	s.full_status = false;
	assert(vhs_osd_render(out, sizeof(out), &s));
	unsigned counter_only = count_alpha(out);
	assert(counter_only > 0 && counter_only < full);
	s.style = VHS_OSD_CAMCORDER;
	s.full_status = true;
	s.transport = VHS_OSD_REC;
	assert(vhs_osd_render(out, sizeof(out), &s));
	unsigned cam = count_alpha(out);
	assert(cam > 100 && cam != full);
	/* No circular or blinking record widget: bitmap characters only. */
	s.mode = VHS_OSD_HIDDEN;
	assert(vhs_osd_render(out, sizeof(out), &s));
	assert(count_alpha(out) == 0);
	assert(vhs_osd_advance(14., 2., VHS_OSD_STOP) == 14.);
	assert(vhs_osd_advance(14., 2., VHS_OSD_PAUSE) == 14.);
	assert(vhs_osd_advance(14., 2., VHS_OSD_PLAY) == 16.);
	assert(vhs_osd_advance(14., 2., VHS_OSD_REC) == 16.);
	assert(vhs_osd_advance(14., 2., VHS_OSD_FAST_FORWARD) == 22.);
	assert(vhs_osd_advance(14., 2., VHS_OSD_REWIND) == 6.);
	assert(vhs_osd_advance(3., 2., VHS_OSD_REWIND) == 0.);
	assert(vhs_osd_status_visible(VHS_OSD_TIMED, 0.));
	assert(vhs_osd_status_visible(VHS_OSD_TIMED, 4.99));
	assert(!vhs_osd_status_visible(VHS_OSD_TIMED, 5.));
	assert(!vhs_osd_status_visible(VHS_OSD_TIMED, 19.));
	assert(vhs_osd_status_visible(VHS_OSD_ALWAYS, 999.));

	puts("VHS_OSD_BITMAP_PASS STATUS_TIMING_PASS TRANSPORT_COUNTER_PASS");
	return 0;
}
