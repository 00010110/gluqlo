#include "gluqlo_seasonal.h"

#include <algorithm>
#include <stdio.h>
#include <vector>

#include "sxtwl.h"

static const char *JIEQI_NAMES[] = {
	"冬至", "小寒", "大寒", "立春", "雨水", "惊蛰",
	"春分", "清明", "谷雨", "立夏", "小满", "芒种",
	"夏至", "小暑", "大暑", "立秋", "处暑", "白露",
	"秋分", "寒露", "霜降", "立冬", "小雪", "大雪"
};

static const char *HOU_NAMES[] = {
	"蚯蚓结", "麋角解", "水泉动",
	"雁北乡", "鹊始巢", "雉始雊",
	"鸡始乳", "征鸟厉疾", "水泽腹坚",
	"东风解冻", "蛰虫始振", "鱼陟负冰",
	"獭祭鱼", "鸿雁来", "草木萌动",
	"桃始华", "仓庚鸣", "鹰化为鸠",
	"玄鸟至", "雷乃发声", "始电",
	"桐始华", "田鼠化为鴽", "虹始见",
	"萍始生", "鸣鸠拂其羽", "戴胜降于桑",
	"蝼蝈鸣", "蚯蚓出", "王瓜生",
	"苦菜秀", "靡草死", "麦秋至",
	"螳螂生", "鵙始鸣", "反舌无声",
	"鹿角解", "蜩始鸣", "半夏生",
	"温风至", "蟋蟀居壁", "鹰始挚",
	"腐草为萤", "土润溽暑", "大雨时行",
	"凉风至", "白露降", "寒蝉鸣",
	"鹰乃祭鸟", "天地始肃", "禾乃登",
	"鸿雁来", "玄鸟归", "群鸟养羞",
	"雷始收声", "蛰虫坯户", "水始涸",
	"鸿雁来宾", "雀入大水为蛤", "菊有黄华",
	"豺乃祭兽", "草木黄落", "蛰虫咸俯",
	"水始冰", "地始冻", "雉入大水为蜃",
	"虹藏不见", "天气上升", "闭塞而成冬",
	"鹖鴠不鸣", "虎始交", "荔挺出"
};

static void copy_label(char *dst, size_t dstlen, const char *src) {
	if (!dst || dstlen == 0) return;
	snprintf(dst, dstlen, "%s", src ? src : "");
}

static bool compare_jieqi_info(const sxtwl::JieQiInfo &a,
	const sxtwl::JieQiInfo &b) {
	return a.jd < b.jd;
}

int gluqlo_compute_seasonal_display(int year, int month, int day,
	int hour, int min, int sec, struct seasonal_display *display) {
	if (!display || month < 1 || month > 12 || day < 1 || day > 31 ||
	    hour < 0 || hour > 23 || min < 0 || min > 59 ||
	    sec < 0 || sec > 60) {
		return 0;
	}

	std::vector<sxtwl::JieQiInfo> jieqi;
	for (int y = year - 1; y <= year + 1; y++) {
		std::vector<sxtwl::JieQiInfo> yearly = sxtwl::getJieQiByYear(y);
		jieqi.insert(jieqi.end(), yearly.begin(), yearly.end());
	}
	if (jieqi.size() < 3) return 0;

	std::sort(jieqi.begin(), jieqi.end(), compare_jieqi_info);

	Time now(year, month, day, hour, min, sec);
	double now_jd = sxtwl::toJD(now);
	int current = -1;
	for (int i = 0; i + 1 < (int) jieqi.size(); i++) {
		if (jieqi[i].jd <= now_jd && now_jd < jieqi[i + 1].jd) {
			current = i;
			break;
		}
	}
	if (current <= 0 || current + 1 >= (int) jieqi.size()) return 0;

	int prev_jieqi = jieqi[current - 1].jqIndex;
	int current_jieqi = jieqi[current].jqIndex;
	int next_jieqi = jieqi[current + 1].jqIndex;
	if (prev_jieqi < 0 || prev_jieqi >= 24 ||
	    current_jieqi < 0 || current_jieqi >= 24 ||
	    next_jieqi < 0 || next_jieqi >= 24) {
		return 0;
	}

	int first_hou = current_jieqi * 3;
	int second_hou = first_hou + 1;
	int third_hou = first_hou + 2;
	int current_hou_index = (int) ((now_jd - jieqi[current].jd) / 5.0);
	if (current_hou_index < 0) current_hou_index = 0;
	if (current_hou_index > 2) current_hou_index = 2;

	copy_label(display->prev_jieqi, sizeof(display->prev_jieqi),
		JIEQI_NAMES[prev_jieqi]);
	copy_label(display->current_jieqi, sizeof(display->current_jieqi),
		JIEQI_NAMES[current_jieqi]);
	copy_label(display->next_jieqi, sizeof(display->next_jieqi),
		JIEQI_NAMES[next_jieqi]);
	copy_label(display->first_hou, sizeof(display->first_hou),
		HOU_NAMES[first_hou]);
	copy_label(display->second_hou, sizeof(display->second_hou),
		HOU_NAMES[second_hou]);
	copy_label(display->third_hou, sizeof(display->third_hou),
		HOU_NAMES[third_hou]);
	display->current_hou_index = current_hou_index;

	return 1;
}
