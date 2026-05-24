#pragma once

struct seasonal_display {
	char prev_jieqi[32];
	char current_jieqi[32];
	char next_jieqi[32];
	char first_hou[32];
	char second_hou[32];
	char third_hou[32];
	int current_hou_index;
};

int gluqlo_compute_seasonal_display(int year, int month, int day,
	int hour, int min, int sec, struct seasonal_display *display);
