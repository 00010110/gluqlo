#include <stdio.h>
#include <string.h>

#include "SDL.h"
#include "gluqlo_seasonal.h"

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
int gluqlo_should_render_seasonal_on_monitor(const struct monitor_rect *monitors,
	int count, int index);
const char *gluqlo_choose_existing_font(const char **paths, int count);
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
	struct monitor_rect monitors[2] = {left, right};
	struct monitor_rect embedded_area;
	struct clock_layout layout;
	char pos[32];
	const char *font_paths[] = {"/definitely/not/a/font.ttf", "gluqlo.ttf"};
	const char *chosen_font;
	struct seasonal_display seasonal;
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

	failed |= expect_int("left monitor renders seasonal display",
		gluqlo_should_render_seasonal_on_monitor(monitors, 2, 0), 1);
	failed |= expect_int("right monitor renders time",
		gluqlo_should_render_seasonal_on_monitor(monitors, 2, 1), 0);
	failed |= expect_int("single monitor renders time",
		gluqlo_should_render_seasonal_on_monitor(monitors, 1, 0), 0);

	chosen_font = gluqlo_choose_existing_font(font_paths, 2);
	if (!chosen_font || strcmp(chosen_font, "gluqlo.ttf") != 0) {
		fprintf(stderr, "font fallback: got %s, expected gluqlo.ttf\n",
			chosen_font ? chosen_font : "(null)");
		failed = 1;
	}

	if (!gluqlo_compute_seasonal_display(2026, 5, 24, 12, 0, 0,
	    &seasonal)) {
		fprintf(stderr, "seasonal display calculation failed\n");
		failed = 1;
	} else {
		if (strcmp(seasonal.prev_jieqi, "立夏") != 0 ||
		    strcmp(seasonal.current_jieqi, "小满") != 0 ||
		    strcmp(seasonal.next_jieqi, "芒种") != 0) {
			fprintf(stderr, "jieqi display: got %s/%s/%s, expected 立夏/小满/芒种\n",
				seasonal.prev_jieqi, seasonal.current_jieqi,
				seasonal.next_jieqi);
			failed = 1;
		}
		if (strcmp(seasonal.first_hou, "苦菜秀") != 0 ||
		    strcmp(seasonal.second_hou, "靡草死") != 0 ||
		    strcmp(seasonal.third_hou, "麦秋至") != 0) {
			fprintf(stderr, "hou display: got %s/%s/%s, expected 苦菜秀/靡草死/麦秋至\n",
				seasonal.first_hou, seasonal.second_hou,
				seasonal.third_hou);
			failed = 1;
		}
		failed |= expect_int("小满 current hou index",
			seasonal.current_hou_index, 0);
	}

	if (!gluqlo_compute_seasonal_display(2026, 7, 13, 12, 0, 0,
	    &seasonal)) {
		fprintf(stderr, "summer hou calculation failed\n");
		failed = 1;
	} else if (strcmp(seasonal.current_jieqi, "小暑") != 0 ||
	    strcmp(seasonal.first_hou, "温风至") != 0 ||
	    strcmp(seasonal.second_hou, "蟋蟀居壁") != 0 ||
	    strcmp(seasonal.third_hou, "鹰始挚") != 0) {
		fprintf(stderr, "summer hou display: got %s/%s/%s/%s, expected 小暑/温风至/蟋蟀居壁/鹰始挚\n",
			seasonal.current_jieqi, seasonal.first_hou,
			seasonal.second_hou, seasonal.third_hou);
		failed = 1;
	} else {
		failed |= expect_int("小暑 current hou index",
			seasonal.current_hou_index, 1);
	}

	if (!gluqlo_compute_seasonal_display(2026, 12, 8, 12, 0, 0,
	    &seasonal)) {
		fprintf(stderr, "winter hou calculation failed\n");
		failed = 1;
	} else if (strcmp(seasonal.current_jieqi, "大雪") != 0 ||
	    strcmp(seasonal.first_hou, "鹖鴠不鸣") != 0 ||
	    strcmp(seasonal.second_hou, "虎始交") != 0 ||
	    strcmp(seasonal.third_hou, "荔挺出") != 0) {
		fprintf(stderr, "winter hou display: got %s/%s/%s/%s, expected 大雪/鹖鴠不鸣/虎始交/荔挺出\n",
			seasonal.current_jieqi, seasonal.first_hou,
			seasonal.second_hou, seasonal.third_hou);
		failed = 1;
	} else {
		failed |= expect_int("大雪 current hou index",
			seasonal.current_hou_index, 0);
	}

	return failed ? 1 : 0;
}
