/*
* Gluqlo: Fliqlo for Linux
* https://github.com/alexanderk23/gluqlo
*
* Copyright (c) 2010-2012 Kuźniarski Jacek
* Copyright (c) 2014 Alexander Kovalenko
*
* Permission to use, copy, modify, and distribute this software for any
* purpose with or without fee is hereby granted, provided that the above
* copyright notice and this permission notice appear in all copies.
*
* THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
* WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
* ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
* WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
* ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
* OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xinerama.h>
#include <time.h>

#include "SDL.h"
#include "SDL_ttf.h"
#include "SDL_syswm.h"
#include "SDL_gfxPrimitives.h"
#include "SDL_rotozoom.h"
#include "gluqlo_seasonal.h"

struct monitor_rect { int x, y, w, h; };
struct clock_layout {
	SDL_Rect hour;
	SDL_Rect minute;
	int rectsize;
	int spacing;
	int radius;
};

#ifndef GLUQLO_NO_MAIN
static void set_bounds_from_rect(struct monitor_rect *bounds,
	const struct monitor_rect *rect) {
	bounds->x = rect->x;
	bounds->y = rect->y;
	bounds->w = rect->w;
	bounds->h = rect->h;
}

static void expand_bounds(struct monitor_rect *bounds,
	const struct monitor_rect *rect) {
	int x1 = bounds->x < rect->x ? bounds->x : rect->x;
	int y1 = bounds->y < rect->y ? bounds->y : rect->y;
	int x2 = bounds->x + bounds->w > rect->x + rect->w ?
		bounds->x + bounds->w : rect->x + rect->w;
	int y2 = bounds->y + bounds->h > rect->y + rect->h ?
		bounds->y + bounds->h : rect->y + rect->h;
	bounds->x = x1;
	bounds->y = y1;
	bounds->w = x2 - x1;
	bounds->h = y2 - y1;
}

/* Returns physical monitor rectangles from Xinerama, falling back to the root
 * screen as a single area if Xinerama is unavailable. */
static int find_monitor_rects(struct monitor_rect *out, int max_out,
	struct monitor_rect *bounds) {
	Display *dpy = XOpenDisplay(NULL);
	if (!dpy) return 0;

	int n = 0;
	XineramaScreenInfo *si = NULL;
	int xinerama_active = XineramaIsActive(dpy);
	if (xinerama_active) si = XineramaQueryScreens(dpy, &n);

	if (!si || n <= 0) {
		int s = DefaultScreen(dpy);
		out[0].x = 0;
		out[0].y = 0;
		out[0].w = DisplayWidth(dpy, s);
		out[0].h = DisplayHeight(dpy, s);
		set_bounds_from_rect(bounds, &out[0]);
		if (si) XFree(si);
		XCloseDisplay(dpy);
		return 1;
	}

	int count = n < max_out ? n : max_out;
	for (int i = 0; i < count; i++) {
		out[i].x = si[i].x_org;
		out[i].y = si[i].y_org;
		out[i].w = si[i].width;
		out[i].h = si[i].height;
		if (i == 0) set_bounds_from_rect(bounds, &out[i]);
		else expand_bounds(bounds, &out[i]);
	}

	XFree(si);
	XCloseDisplay(dpy);
	return count;
}
#endif

int gluqlo_compute_clock_layout(const struct monitor_rect *area,
	double display_scale_factor, struct clock_layout *layout) {
	if (!area || !layout || area->w <= 0 || area->h <= 0 ||
	    display_scale_factor <= 0) {
		return 0;
	}

	int width = area->w * display_scale_factor;
	int height = area->h * display_scale_factor;
	bool is_horizontal = width > height;

	if (is_horizontal) {
		layout->rectsize = height * 0.6;
		layout->spacing = width * .031;
		layout->radius = height * .05714;
	} else {
		layout->rectsize = width * 0.6;
		layout->spacing = height * .031;
		layout->radius = width * .05714;
	}

	int jitter_width  = 1;
	int jitter_height = 1;
	if (display_scale_factor != 1) {
		jitter_width  = (area->w - width) * 0.5;
		jitter_height = (area->h - height) * 0.5;
	}

	layout->hour.w = layout->rectsize;
	layout->hour.h = layout->rectsize;
	layout->minute.w = layout->rectsize;
	layout->minute.h = layout->rectsize;

	if (is_horizontal) {
		layout->hour.x = area->x +
			0.5 * (width - (0.031 * width) - (1.2 * height)) +
			jitter_width;
		layout->hour.y = area->y + 0.2 * height + jitter_height;
		layout->minute.x = layout->hour.x + (0.6 * height) +
			layout->spacing;
		layout->minute.y = layout->hour.y;
	} else {
		layout->hour.y = area->y +
			0.5 * (height - (0.031 * height) - (1.2 * width)) +
			jitter_height;
		layout->hour.x = area->x + 0.2 * width + jitter_width;
		layout->minute.y = layout->hour.y + (0.6 * width) +
			layout->spacing;
		layout->minute.x = layout->hour.x;
	}

	return 1;
}

int gluqlo_format_window_position(char *buf, size_t buflen, int x, int y) {
	if (!buf || buflen == 0) return 0;
	int written = snprintf(buf, buflen, "%d,%d", x, y);
	return written > 0 && (size_t) written < buflen;
}

void gluqlo_choose_render_area(int has_window, int window_w, int window_h,
	int surface_w, int surface_h, struct monitor_rect *area) {
	area->x = 0;
	area->y = 0;
	if (has_window && window_w > 0 && window_h > 0) {
		area->w = window_w;
		area->h = window_h;
	} else {
		area->w = surface_w;
		area->h = surface_h;
	}
}

int gluqlo_should_render_seasonal_on_monitor(const struct monitor_rect *monitors,
	int count, int index) {
	if (!monitors || count < 2 || index < 0 || index >= count) return 0;

	int leftmost = 0;
	for (int i = 1; i < count; i++) {
		if (monitors[i].x < monitors[leftmost].x ||
		    (monitors[i].x == monitors[leftmost].x &&
		     monitors[i].y < monitors[leftmost].y)) {
			leftmost = i;
		}
	}

	return index == leftmost;
}

const char *gluqlo_choose_existing_font(const char **paths, int count) {
	if (!paths || count <= 0) return NULL;

	for (int i = 0; i < count; i++) {
		if (paths[i] && access(paths[i], R_OK) == 0) return paths[i];
	}

	return NULL;
}

#ifndef FONT
#define FONT "/usr/share/gluqlo/gluqlo.ttf"
#endif

const char* TITLE = "Gluqlo 1.1";
const int DEFAULT_WIDTH = 1024;
const int DEFAULT_HEIGHT = 768;

bool twentyfourh = true;
bool leadingzero = false;
bool fullscreen = false;
bool animate = true;
bool anykeyclose = false;

int past_h = -1, past_m = -1;

int width = DEFAULT_WIDTH;
int height = DEFAULT_HEIGHT;

TTF_Font *font_time = NULL;
TTF_Font *font_mode = NULL;
TTF_Font *font_season_current = NULL;
TTF_Font *font_season_blur = NULL;
TTF_Font *font_hou_current = NULL;
TTF_Font *font_hou_blur = NULL;

const SDL_Color FONT_COLOR = { 0xb7, 0xb7, 0xb7 };
const SDL_Color BACKGROUND_COLOR = { 0x0f, 0x0f, 0x0f };

SDL_Surface *screen;

SDL_Rect hourBackground;
SDL_Rect minBackground;

SDL_Rect bgrect;

#define MAX_CLOCK_LAYOUTS 16
#define GLUQLO_CONTENT_TIME 0
#define GLUQLO_CONTENT_SEASONAL 1
struct monitor_rect clock_areas[MAX_CLOCK_LAYOUTS];
struct clock_layout clock_layouts[MAX_CLOCK_LAYOUTS];
SDL_Surface *clock_backgrounds[MAX_CLOCK_LAYOUTS];
int clock_content[MAX_CLOCK_LAYOUTS];
int clock_layout_count = 0;

// draw rounded box
// see http://lists.libsdl.org/pipermail/sdl-libsdl.org/2006-December/058868.html
void fill_rounded_box_b(SDL_Surface* dst, SDL_Rect *coords, int r, SDL_Color color) {
	Uint32 pixcolor = SDL_MapRGB(dst->format, color.r, color.g, color.b);

	int i, j;
	int rpsqrt2 = (int) (r / sqrt(2));
	int yd = dst->pitch / dst->format->BytesPerPixel;
	int w = coords->w / 2 - 1;
	int h = coords->h / 2 - 1;
	int xo = coords->x + w;
	int yo = coords->y + h;

	w -= r;
	h -= r;

	if(w <= 0 || h <= 0) return;

	SDL_LockSurface(dst);
	Uint32 *pixels = (Uint32*)(dst->pixels);

	int sy = (yo - h) * yd;
	int ey = (yo + h) * yd;
	int sx = xo - w;
	int ex = xo + w;

	for(i = sy; i <= ey; i += yd)
		for(j = sx - r; j <= ex + r; j++)
			pixels[i + j] = pixcolor;

	int d = -r;
	int x2m1 = -1;
	int y = r;

	for(int x = 0; x <= rpsqrt2; x++) {
		x2m1 += 2;
		d += x2m1;
		if(d >= 0) {
			y--;
			d -= y * 2;
		}

		for(i = sx - x; i <= ex + x; i++) {
			pixels[sy - y * yd + i] = pixcolor;
		}

		for(i = sx - y; i <= ex + y; i++) {
			pixels[sy - x * yd + i] = pixcolor;
		}

		for(i = sx - y; i <= ex + y; i++) {
			pixels[ey + x * yd + i] = pixcolor;
		}

		for(i = sx - x; i <= ex + x; i++) {
			pixels[ey + y * yd + i] = pixcolor;
		}
	}

	SDL_UnlockSurface(dst);
}

void render_ampm(SDL_Surface *surface, SDL_Rect *rect, int pm) {
	char mode[3];
	SDL_Rect coords;
	snprintf(mode, 3, "%cM", pm ? 'P' : 'A');
	SDL_Surface *ampm = TTF_RenderText_Blended(font_mode, mode, FONT_COLOR);
	int offset = rect->h * 0.127;
	coords.x = rect->x + rect->h * 0.07;
	coords.y = rect->y + (pm ? rect->h - offset - ampm->h : offset);
	SDL_BlitSurface(ampm, 0, surface, &coords);
	SDL_FreeSurface(ampm);
}



void blit_digits(SDL_Surface *surface, SDL_Rect *rect, int spc, char digits[], SDL_Color color) {
	int min_x, max_x, min_y, max_y, advance;
	int adjust_x = (digits[0] == '1') ? 2.5 * spc : 0; // special case
	int center_x = rect->x + rect->w / 2 - adjust_x;

	SDL_Surface *glyph;
	SDL_Rect coords;

	if(digits[1]) {
		// first digit
		TTF_GlyphMetrics(font_time, digits[0], &min_x, &max_x, &min_y, &max_y, &advance);
		glyph = TTF_RenderGlyph_Blended(font_time, digits[0], color);
		coords.x = center_x - max_x + min_x - spc - (adjust_x ? spc : 0);
		coords.y = rect->y + (rect->h - glyph->h) / 2;
		SDL_BlitSurface(glyph, 0, surface, &coords);
		SDL_FreeSurface(glyph);
		// second digit
		TTF_GlyphMetrics(font_time, digits[1], &min_x, &max_x, &min_y, &max_y, &advance);
		glyph = TTF_RenderGlyph_Blended(font_time, digits[1], color);
		coords.y = rect->y + (rect->h - glyph->h) / 2;
		coords.x = center_x + spc / 2;
		SDL_BlitSurface(glyph, 0, surface, &coords);
		SDL_FreeSurface(glyph);
	} else {
		// single digit
		glyph = TTF_RenderGlyph_Blended(font_time, digits[0], color);
		coords.x = center_x - glyph->w / 2;
		coords.y = rect->y + (rect->h - glyph->h) / 2;
		SDL_BlitSurface(glyph, 0, surface, &coords);
		SDL_FreeSurface(glyph);
	}
}


void render_digits(SDL_Surface *surface, SDL_Surface *bg_surface,
	SDL_Rect *background, char digits[], char prevdigits[], int maxsteps,
	int step) {
	SDL_Rect rect, dstrect;
	SDL_Color color;
	double scale;
	Uint8 c;

	// int spc = surface->h * .0125;
	bool is_h = surface->h < surface->w;
	int spc = is_h ? surface->h * .0125 : surface->w * .0125;

	// blit upper halves of current digits
	rect.x = background->x;
	rect.y = background->y;
	rect.w = background->w;
	rect.h = background->h/2;
	SDL_SetClipRect(surface, &rect);
	SDL_BlitSurface(bg_surface, 0, surface, &rect);
	blit_digits(surface, background, spc, digits, FONT_COLOR);
	SDL_SetClipRect(surface, NULL);

	int halfsteps = maxsteps / 2;
	int upperhalf = (step+1) <= halfsteps;
	if(upperhalf) {
		scale = 1.0 - (1.0 * step) / (halfsteps - 1);
		c = 0xb7 - 0xb7 * (1.0 * step) / (halfsteps - 1);
	} else {
		scale = ((1.0 * step) - halfsteps + 1) / halfsteps;
		c = 0xb7 * ((1.0 * step) - halfsteps + 1) / halfsteps;
	}
	color.r = color.g = color.b = c;

	// create surface to scale from filled background surface
	// bgcopy is using for blit text, so need use same format with screen, for avoid any alpha rendering problem
	SDL_Surface *bgcopy = SDL_ConvertSurface(bg_surface, surface->format, surface->flags);
	rect.x = 0;
	rect.y = 0;
	rect.w = bgcopy->w;
	rect.h = bgcopy->h;
	blit_digits(bgcopy, &rect, spc, upperhalf ? prevdigits : digits, color);

	// scale and blend it to dest
	SDL_Surface *scaled = zoomSurface(bgcopy, 1.0, scale, 1);
	rect.x = 0;
	rect.y = upperhalf ? 0 : scaled->h / 2;
	rect.w = scaled->w;
	rect.h = scaled->h / 2;
	dstrect.x = background->x;
	dstrect.y = background->y + ( upperhalf ? ((background->h - scaled->h) / 2) : background->h / 2);
	dstrect.w = rect.w;
	dstrect.h = rect.h;	
	SDL_SetClipRect(surface, &dstrect);
	SDL_BlitSurface(scaled, &rect, surface, &dstrect);
	SDL_SetClipRect(surface, NULL);
	SDL_FreeSurface(scaled);
	SDL_FreeSurface(bgcopy);

	if(!animate) return;
	// draw divider
	rect.h = (is_h ? surface->h : surface->w) * 0.005;
	rect.w = background->w;
	rect.x = background->x;
	rect.y = background->y + (background->h - rect.h) / 2;
	SDL_FillRect(surface, &rect, SDL_MapRGB(surface->format, 0, 0, 0));
	rect.y += rect.h;
	rect.h = 1;
	SDL_FillRect(surface, &rect, SDL_MapRGB(surface->format, 0x1a, 0x1a, 0x1a));
}

static void render_centered_utf8(SDL_Surface *surface, TTF_Font *font,
	const char *text, SDL_Color color, int center_x, int center_y) {
	SDL_Surface *text_surface = TTF_RenderUTF8_Blended(font, text, color);
	if (!text_surface) return;

	SDL_Rect coords;
	coords.x = center_x - text_surface->w / 2;
	coords.y = center_y - text_surface->h / 2;
	SDL_BlitSurface(text_surface, 0, surface, &coords);
	SDL_FreeSurface(text_surface);
}

void render_seasonal_display(int layout_index, const struct tm *time_i) {
	struct seasonal_display seasonal;
	SDL_Rect area_rect;
	SDL_Color current_color = {0xd0, 0xd0, 0xd0};
	SDL_Color blur_color = {0x55, 0x55, 0x55};
	SDL_Color side_jieqi_color = {0x18, 0x18, 0x18};

	if (!font_season_current || !font_season_blur ||
	    !font_hou_current || !font_hou_blur ||
	    !gluqlo_compute_seasonal_display(time_i->tm_year + 1900,
	    time_i->tm_mon + 1, time_i->tm_mday, time_i->tm_hour,
	    time_i->tm_min, time_i->tm_sec, &seasonal)) {
		return;
	}

	const struct monitor_rect *area = &clock_areas[layout_index];
	area_rect.x = area->x;
	area_rect.y = area->y;
	area_rect.w = area->w;
	area_rect.h = area->h;
	SDL_FillRect(screen, &area_rect, SDL_MapRGB(screen->format, 0, 0, 0));

	int left_x = area->x + area->w * 0.20;
	int center_x = area->x + area->w * 0.50;
	int right_x = area->x + area->w * 0.80;
	int jieqi_y = area->y + area->h * 0.34;
	int hou_y = area->y + area->h * 0.66;

	render_centered_utf8(screen, font_season_blur, seasonal.prev_jieqi,
		side_jieqi_color, left_x, jieqi_y);
	render_centered_utf8(screen, font_season_current, seasonal.current_jieqi,
		current_color, center_x, jieqi_y);
	render_centered_utf8(screen, font_season_blur, seasonal.next_jieqi,
		side_jieqi_color, right_x, jieqi_y);
	render_centered_utf8(screen,
		font_hou_blur, seasonal.first_hou,
		seasonal.current_hou_index == 0 ? current_color : blur_color,
		left_x, hou_y);
	render_centered_utf8(screen,
		font_hou_blur, seasonal.second_hou,
		seasonal.current_hou_index == 1 ? current_color : blur_color,
		center_x, hou_y);
	render_centered_utf8(screen,
		font_hou_blur, seasonal.third_hou,
		seasonal.current_hou_index == 2 ? current_color : blur_color,
		right_x, hou_y);
}

void render_clock(int maxsteps, int step) {
	char buffer[3], buffer2[3];
	struct tm *_time;
	time_t rawtime;
	int old_h = past_h;
	int old_m = past_m;

	time(&rawtime);
	_time = localtime(&rawtime);

	for (int i = 0; i < clock_layout_count; i++) {
		if (clock_content[i] == GLUQLO_CONTENT_SEASONAL) {
			if (step == maxsteps - 1) render_seasonal_display(i, _time);
			continue;
		}

		// draw hours
		if(_time->tm_hour != old_h) {
			int h = twentyfourh ? _time->tm_hour : (_time->tm_hour + 11) % 12 + 1;
			if(leadingzero) {
				snprintf(buffer, 3, "%02d", h);
				snprintf(buffer2, 3, "%02d", old_h);
			} else {
				snprintf(buffer, 3, "%d", h);
				snprintf(buffer2, 3, "%d", old_h);
			}
			render_digits(screen, clock_backgrounds[i],
				&clock_layouts[i].hour, buffer, buffer2, maxsteps, step);
			// draw am/pm
			if(!twentyfourh) render_ampm(screen, &clock_layouts[i].hour,
				_time->tm_hour >= 12);
		}

		// draw minutes
		if(_time->tm_min != old_m) {
			snprintf(buffer, 3, "%02d", _time->tm_min);
			snprintf(buffer2, 3, "%02d", old_m);
			render_digits(screen, clock_backgrounds[i],
				&clock_layouts[i].minute, buffer, buffer2, maxsteps, step);
		}
	}

	// flip backbuffer
	SDL_Flip(screen);

	if(step == maxsteps-1) {
		past_h = _time->tm_hour;
		past_m = _time->tm_min;
	}
}

void render_animation() {
	if(!animate) {
		render_clock(20, 19);
		return;
	}

	const int DURATION = 260;
	int start_tick = SDL_GetTicks();
	int end_tick = start_tick + DURATION;
	int current_tick;
	int frame;
	int done = 0;

	while(!done) {
		current_tick = SDL_GetTicks();
		if(current_tick >= end_tick) {
			done = 1;
			current_tick = end_tick;
		}
		frame = 99 * (current_tick-start_tick) / (end_tick-start_tick);
		render_clock(100, frame);
	}
}

Uint32 update_time(Uint32 interval, void *param) {
	SDL_Event e;
	time_t rawtime;
	struct tm *time_i;

	time(&rawtime);
	time_i = localtime(&rawtime);

	if(time_i->tm_min != past_m) {
		e.type = SDL_USEREVENT;
		e.user.code = 0;
		e.user.data1 = NULL;
		e.user.data2 = NULL;
		SDL_PushEvent(&e);
		interval = 1000 * (60 - time_i->tm_sec) - 250;
	} else {
		interval = 250;
	}

	return interval;
}

#ifndef GLUQLO_NO_MAIN
int main(int argc, char** argv ) {
	char *wid_env;
	static char sdlwid[100];
	double display_scale_factor = 1;

	Uint32 wid = 0;
	Display *display;
	XWindowAttributes windowAttributes;
	int embedded_x = 0, embedded_y = 0;
	int have_embedded_position = 0;
	struct monitor_rect monitors[MAX_CLOCK_LAYOUTS];
	struct monitor_rect desktop_bounds = {0, 0, DEFAULT_WIDTH, DEFAULT_HEIGHT};
	int monitor_count = find_monitor_rects(monitors, MAX_CLOCK_LAYOUTS,
		&desktop_bounds);

	for(int i = 1; i < argc; i++) {
		if(strcmp("--help",argv[i]) == 0 || strcmp("-help", argv[i]) == 0) {
			printf("Usage: %s [OPTION...]\nOptions:\n", argv[0]);
			printf("  -help\t\tDisplay this\n");
			printf("  -root, -f\tFullscreen\n");
			printf("  -noflip\tDisable the flip animation (change time in one frame)\n");
			printf("  -anykeyclose\tClose app when mouse move or any key pressed\n");
			printf("  -ampm\t\tUse 12-hour clock format (AM/PM)\n");
			printf("  -leadingzero\tAlways display hour with two digits\n");
			printf("  -w\t\tCustom width\n");
			printf("  -h\t\tCustom height\n");
			printf("  -r\t\tCustom resolution in WxH format\n");
			printf("  -s\t\tCustom display scale factor\n");
			return 0;
		} else if(strcmp("-root", argv[i]) == 0 || strcmp("--root", argv[i]) == 0 || strcmp("-f", argv[i]) == 0 || strcmp("--fullscreen", argv[i]) == 0) {
			fullscreen = true;
		} else if(strcmp("-noflip", argv[i]) == 0) {
			animate = false;
		} else if(strcmp("-anykeyclose", argv[i]) == 0) {
			anykeyclose = true;
		} else if(strcmp("-ampm", argv[i]) == 0) {
			twentyfourh = false;
		} else if(strcmp("-leadingzero", argv[i]) == 0) {
			leadingzero = true;
		} else if(strcmp("-r", argv[i]) == 0 || strcmp("--resolution", argv[i]) == 0) {
			char *resolution = argv[i+1];
			char *val = strtok(resolution, "x");
			width = atoi(val);
			val = strtok(NULL, "x");
			height = atoi(val);
			i++;
		} else if(strcmp("-w", argv[i]) == 0) {
			width = atoi(argv[i+1]);
			i++;
		} else if(strcmp("-h", argv[i]) == 0) {
			height = atoi(argv[i+1]);
			i++;
		} else if(strcmp("-s", argv[i]) == 0) {
			display_scale_factor = atof(argv[i+1]);
			i++;
		} else if(strcmp("-window-id", argv[i]) == 0) {
			wid = strtol(argv[i+1], (char **) NULL, 0);
			i++;
		} else {
			printf("Invalid option -- %s\n", argv[i]);
			printf("Try --help for more information.\n");
			return 0;
		}
	}

	/* If no window argument, check environment */
	if(wid == 0) {
		if ((wid_env = getenv("XSCREENSAVER_WINDOW")) != NULL ) {
			wid = strtol(wid_env, (char **) NULL, 0); /* Base 0 autodetects hex/dec */
		}
	}

	/* Get win attrs if we've been given a window, otherwise we'll use our own */
	if(wid != 0) {
		if ((display = XOpenDisplay(NULL)) != NULL) { /* Use the default display */
			XGetWindowAttributes(display, (Window) wid, &windowAttributes);
			Window child;
			have_embedded_position = XTranslateCoordinates(display,
				(Window) wid, DefaultRootWindow(display), 0, 0,
				&embedded_x, &embedded_y, &child);
			XCloseDisplay(display);
			snprintf(sdlwid, 100, "SDL_WINDOWID=0x%X", wid);
			putenv(sdlwid); /* Tell SDL to use this window */
			char poshint[64];
			if (gluqlo_format_window_position(poshint, sizeof(poshint),
			    embedded_x, embedded_y)) {
				setenv("SDL_VIDEO_WINDOW_POS", poshint, 1);
			}
			width = windowAttributes.width;
			height = windowAttributes.height;
		}
	}

	/* Disable SDL 1.2's XRandR mode-switching path. On Xinerama desktops
	 * (multi-monitor) SDL will otherwise try to switch modelines, which can
	 * leave a monitor stuck at 640x480 after we exit. */
	setenv("SDL_VIDEO_X11_XRANDR", "0", 1);
	setenv("SDL_VIDEO_X11_VIDMODE", "0", 1);
	setenv("SDL_VIDEO_CENTERED", "0", 1);

	if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0) {
		fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
		return 1;
	}
	atexit(SDL_Quit);

	if(fullscreen && (!wid)) {
		/* Avoid SDL_FULLSCREEN mode switching; cover the virtual desktop
		 * with one borderless window and render once per physical monitor. */
		char poshint[64];
		snprintf(poshint, sizeof poshint, "%d,%d",
			desktop_bounds.x, desktop_bounds.y);
		setenv("SDL_VIDEO_WINDOW_POS", poshint, 1);
		screen = SDL_SetVideoMode(desktop_bounds.w, desktop_bounds.h, 32,
			SDL_SWSURFACE|SDL_NOFRAME);
	} else if (wid) {
		int sw = desktop_bounds.w > width ? desktop_bounds.w : width;
		screen = SDL_SetVideoMode(sw, height, 32, SDL_SWSURFACE);
	} else {
		screen = SDL_SetVideoMode(width, height, 32, SDL_HWSURFACE|SDL_DOUBLEBUF);
	}

	if (!screen) {
		fprintf(stderr, "Unable to set video mode: %s\n", SDL_GetError());
		return 1;
	}

	if (wid && have_embedded_position &&
	    (display = XOpenDisplay(NULL)) != NULL) {
		XMoveWindow(display, (Window) wid, embedded_x, embedded_y);
		XSync(display, False);
		XCloseDisplay(display);
	}

	if(fullscreen || wid) {
		SDL_ShowCursor(SDL_DISABLE);
	}

	SDL_WM_SetCaption(TITLE, TITLE);

	struct monitor_rect render_areas[MAX_CLOCK_LAYOUTS];
	if (fullscreen && !wid && monitor_count > 0) {
		clock_layout_count = monitor_count;
		for (int i = 0; i < clock_layout_count; i++) {
			render_areas[i].x = monitors[i].x - desktop_bounds.x;
			render_areas[i].y = monitors[i].y - desktop_bounds.y;
			render_areas[i].w = monitors[i].w;
			render_areas[i].h = monitors[i].h;
		}
	} else {
		/* XScreenSaver provides one window per monitor via
		 * XSCREENSAVER_WINDOW, so each subprocess renders its full window. */
		clock_layout_count = 1;
		gluqlo_choose_render_area(wid != 0, width, height, screen->w,
			screen->h, &render_areas[0]);
	}

	for (int i = 0; i < clock_layout_count; i++) {
		clock_areas[i] = render_areas[i];
		clock_content[i] = GLUQLO_CONTENT_TIME;
	}

	if (fullscreen && !wid) {
		for (int i = 0; i < clock_layout_count; i++) {
			if (gluqlo_should_render_seasonal_on_monitor(render_areas,
			    clock_layout_count, i)) {
				clock_content[i] = GLUQLO_CONTENT_SEASONAL;
			}
		}
	} else if (wid && have_embedded_position && monitor_count > 1) {
		for (int i = 0; i < monitor_count; i++) {
			if (embedded_x >= monitors[i].x &&
			    embedded_x < monitors[i].x + monitors[i].w &&
			    embedded_y >= monitors[i].y &&
			    embedded_y < monitors[i].y + monitors[i].h &&
			    gluqlo_should_render_seasonal_on_monitor(monitors,
			    monitor_count, i)) {
				clock_content[0] = GLUQLO_CONTENT_SEASONAL;
				break;
			}
		}
	}

	int font_base = render_areas[0].w > render_areas[0].h ?
		render_areas[0].h * display_scale_factor :
		render_areas[0].w * display_scale_factor;
	int season_current_font_size = font_base / 4;
	int season_blur_font_size = font_base / 7;
	int hou_current_font_size = font_base / 12;
	int hou_blur_font_size = font_base / 16;
	if (season_current_font_size < 18) season_current_font_size = 18;
	if (season_blur_font_size < 12) season_blur_font_size = 12;
	if (hou_current_font_size < 12) hou_current_font_size = 12;
	if (hou_blur_font_size < 12) hou_blur_font_size = 12;
	const char *seasonal_font_candidates[] = {
		"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
		"/usr/share/fonts/todesk/NotoSansCJK-Regular.ttc",
		"/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
		"/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
		FONT
	};
	const char *seasonal_font_path = gluqlo_choose_existing_font(
		seasonal_font_candidates,
		sizeof(seasonal_font_candidates) / sizeof(seasonal_font_candidates[0]));
	if (!seasonal_font_path) seasonal_font_path = FONT;

	TTF_Init();
	atexit(TTF_Quit);
	font_time = TTF_OpenFont(FONT, font_base / 1.68 );
	font_mode = TTF_OpenFont(FONT, font_base / 16.5);
	font_season_current = TTF_OpenFont(seasonal_font_path, season_current_font_size);
	font_season_blur = TTF_OpenFont(seasonal_font_path, season_blur_font_size);
	font_hou_current = TTF_OpenFont(seasonal_font_path, hou_current_font_size);
	font_hou_blur = TTF_OpenFont(seasonal_font_path, hou_blur_font_size);
	if (!font_time || !font_mode || !font_season_current ||
	    !font_season_blur || !font_hou_current || !font_hou_blur) {
		fprintf(stderr, "TTF_OpenFont: %s\n", TTF_GetError());
		return 1;
	}

	// clear screen
	SDL_FillRect(screen, 0, SDL_MapRGB(screen->format, 0, 0, 0));

	for (int i = 0; i < clock_layout_count; i++) {
		if (!gluqlo_compute_clock_layout(&render_areas[i],
		    display_scale_factor, &clock_layouts[i])) {
			fprintf(stderr, "Unable to calculate clock layout\n");
			return 1;
		}
		bgrect.x = 0;
		bgrect.y = 0;
		bgrect.w = clock_layouts[i].rectsize;
		bgrect.h = clock_layouts[i].rectsize;
		clock_backgrounds[i] = SDL_CreateRGBSurface(
			SDL_HWSURFACE|SDL_SRCALPHA,
			clock_layouts[i].rectsize,
			clock_layouts[i].rectsize, 32, 0xff000000,
			0x00ff0000, 0x0000ff00, 0x000000ff);
		if (!clock_backgrounds[i]) {
			fprintf(stderr, "SDL_CreateRGBSurface: %s\n", SDL_GetError());
			return 1;
		}
		fill_rounded_box_b(clock_backgrounds[i], &bgrect,
			clock_layouts[i].radius, BACKGROUND_COLOR);
	}

	// draw current time
	render_clock(20, 19);

	// main loop
	bool done = false;
	SDL_Event event;
	SDL_TimerID timer = SDL_AddTimer(60, update_time, NULL);

	int mouse_x = -1;
	int mouse_y = -1;	

	while(!done && SDL_WaitEvent(&event)) {
		switch(event.type) {
			case SDL_USEREVENT:
				render_animation();
				break;
			case SDL_KEYDOWN:
				if(anykeyclose){
					done = true;
					break;
				}
				switch(event.key.keysym.sym) {
					case SDLK_ESCAPE:
					case SDLK_q:
						done = true;
						break;
					default:
						break;
				}
				break;

			case SDL_MOUSEMOTION:
				if ( (mouse_x == -1) || (mouse_y == -1) ) //lifehack
					{
						mouse_x = event.motion.x;
						mouse_y = event.motion.y;

					}

				if(((mouse_x != event.motion.x) || (mouse_y != event.motion.y)) && anykeyclose)
					done = true;
				break;

			case SDL_QUIT:
				done = true;
				break;
		}
	}

	SDL_RemoveTimer(timer);

	for (int i = 0; i < clock_layout_count; i++) {
		SDL_FreeSurface(clock_backgrounds[i]);
	}

	SDL_FreeSurface(screen);

	TTF_CloseFont(font_time);
	TTF_CloseFont(font_mode);
	TTF_CloseFont(font_season_current);
	TTF_CloseFont(font_season_blur);
	TTF_CloseFont(font_hou_current);
	TTF_CloseFont(font_hou_blur);

	return 0;
}
#endif
