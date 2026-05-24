#include <stdio.h>

#include "SDL.h"

struct monitor_rect { int x, y, w, h; };
struct clock_layout {
	SDL_Rect hour;
	SDL_Rect minute;
	int rectsize;
	int spacing;
	int radius;
};

int gluqlo_compute_clock_layout(const struct monitor_rect *area,
	double display_scale_factor, struct clock_layout *layout);
int gluqlo_format_window_position(char *buf, size_t buflen, int x, int y);
void gluqlo_choose_render_area(int has_window, int window_w, int window_h,
	int surface_w, int surface_h, struct monitor_rect *area);

static int expect_int(const char *name, int actual, int expected) {
	if (actual != expected) {
		fprintf(stderr, "%s: got %d, expected %d\n", name, actual, expected);
		return 1;
	}
	return 0;
}

static int expect_rect_inside(const char *name, SDL_Rect rect,
	const struct monitor_rect *area) {
	if (rect.x < area->x || rect.y < area->y ||
	    rect.x + rect.w > area->x + area->w ||
	    rect.y + rect.h > area->y + area->h) {
		fprintf(stderr,
			"%s: rect (%d,%d %dx%d) outside area (%d,%d %dx%d)\n",
			name, rect.x, rect.y, rect.w, rect.h,
			area->x, area->y, area->w, area->h);
		return 1;
	}
	return 0;
}

int main(void) {
	struct monitor_rect left = {0, 0, 2560, 1440};
	struct monitor_rect right = {2560, 0, 2560, 1440};
	struct monitor_rect embedded_area;
	struct clock_layout layout;
	char pos[32];
	int failed = 0;

	if (!gluqlo_compute_clock_layout(&left, 1.0, &layout)) {
		fprintf(stderr, "left monitor layout failed\n");
		return 1;
	}
	failed |= expect_int("left hour x", layout.hour.x, 377);
	failed |= expect_int("left minute x", layout.minute.x, 1320);
	failed |= expect_rect_inside("left hour", layout.hour, &left);
	failed |= expect_rect_inside("left minute", layout.minute, &left);

	if (!gluqlo_compute_clock_layout(&right, 1.0, &layout)) {
		fprintf(stderr, "right monitor layout failed\n");
		return 1;
	}
	failed |= expect_int("right hour x", layout.hour.x, 2937);
	failed |= expect_int("right minute x", layout.minute.x, 3880);
	failed |= expect_rect_inside("right hour", layout.hour, &right);
	failed |= expect_rect_inside("right minute", layout.minute, &right);

	if (!gluqlo_format_window_position(pos, sizeof(pos), 2560, 0)) {
		fprintf(stderr, "right monitor SDL position formatting failed\n");
		return 1;
	}
	for (const char *actual = pos, *expected = "2560,0";
	    *actual || *expected; actual++, expected++) {
		if (*actual != *expected) {
			fprintf(stderr, "SDL position: got %s, expected 2560,0\n", pos);
			failed = 1;
			break;
		}
	}

	gluqlo_choose_render_area(1, 2560, 1440, 5120, 1440, &embedded_area);
	failed |= expect_int("embedded area x", embedded_area.x, 0);
	failed |= expect_int("embedded area y", embedded_area.y, 0);
	failed |= expect_int("embedded area width", embedded_area.w, 2560);
	failed |= expect_int("embedded area height", embedded_area.h, 1440);

	return failed ? 1 : 0;
}
