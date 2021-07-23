/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "linux/notifier.h"
#include <asm/hwconf_manager.h>
#include "dsi_panel_mi_count.h"

#define HWCONPONENT_NAME "display"
#define HWCONPONENT_KEY_LCD "OLED"
#define HWMON_CONPONENT_NAME "display"
#define HWMON_KEY_ACTIVE "panel_active"
#define HWMON_KEY_REFRESH "panel_refresh"
#define HWMON_KEY_BOOTTIME "kernel_boottime"
#define HWMON_KEY_DAYS "kernel_days"
#define HWMON_KEY_BL_AVG "bl_level_avg"
#define HWMON_KEY_BL_HIGH "bl_level_high"
#define HWMON_KEY_BL_LOW "bl_level_low"
#define HWMON_KEY_HBM_DRUATION "hbm_duration"
#define HWMON_KEY_HBM_TIMES "hbm_times"
#define DAY_SECS (60*60*24)

static char *hwmon_key_fps[FPS_MAX_NUM] = {"30fps_times", "50fps_times",
	"60fps_times", "90fps_times", "120fps_times", "144fps_times"};
static const u32 dynamic_fps[FPS_MAX_NUM] = {30, 50, 60, 90, 120, 144};
atomic_t panel_active_count_enable;

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_gpio;

	bool prepared;
	bool enabled;

	int error;

	bool hbm_en;
	bool hbm_wait;

	u32 unset_doze_brightness;
	u32 doze_brightness_state;
	bool in_aod;
	bool high_refresh_rate;
	char *panel_info;
	bool doze_enable;
	int skip_dimmingon;
	struct delayed_work dimmingon_delayed_work;
	ktime_t set_backlight_time;
	struct dsi_panel_mi_count mi_count;
};

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

void dsi_panel_fps_count(struct drm_panel *panel, u32 fps, u32 enable)
{
	struct lcm *ctx = panel_to_lcm(panel);

	static u32 timming_index = FPS_MAX_NUM;
	static u64 timestamp_fps;
	static u32 cur_fps = 60;
	u64 jiffies_time = 0;
	int i = 0;
	char ch[64] = {0};

	if (!panel || (!enable && timming_index == FPS_MAX_NUM))
		return;

	pr_info("%s fps = %d, cur_fps = %d, enable = %d\n", __func__, fps, cur_fps, enable);
	if (enable && !fps)
		fps = cur_fps;

	for(i = 0; i < FPS_MAX_NUM; i++) {
		if (fps == dynamic_fps[i])
			break;
	}

	if (i < FPS_MAX_NUM || !fps) {
		if (i != timming_index && timming_index < FPS_MAX_NUM) {
			jiffies_time = get_jiffies_64();
			if (time_after64(jiffies_time, timestamp_fps))
				ctx->mi_count.fps_times[timming_index] += (jiffies_time - timestamp_fps) / HZ;
			snprintf(ch, sizeof(ch), "%llu", ctx->mi_count.fps_times[timming_index]);
			update_hw_monitor_info(HWMON_CONPONENT_NAME, hwmon_key_fps[timming_index], ch);
			timestamp_fps = jiffies_time;
		} else if (timming_index == FPS_MAX_NUM)
			timestamp_fps = get_jiffies_64();

		timming_index = i;
	}

	cur_fps = fps ? fps : cur_fps;
}

void dsi_panel_state_count(struct drm_panel *panel, int enable)
{
	struct lcm *ctx = panel_to_lcm(panel);

	static u64 timestamp_panelon;
	static u64 on_times;
	static u64 off_times;
	char ch[64] = {0};

	if (!panel) {
		pr_err("invalid params\n");
		return;
	}

	if (enable) {
		/* get panel on timestamp */
		timestamp_panelon = get_jiffies_64();
		on_times++;
		atomic_set(&panel_active_count_enable, 1);
	} else {
		ktime_t boot_time;
		u32 delta_days = 0;
		u64 jiffies_time = 0;
		struct timespec rtctime;

		off_times++;
		pr_info("%s: on_times[%llu] off_times[%llu]\n", __func__, on_times, off_times);

		/* caculate panel active duration */
		jiffies_time = get_jiffies_64();
		if (time_after64(jiffies_time, timestamp_panelon))
			ctx->mi_count.panel_active += jiffies_time - timestamp_panelon;
		snprintf(ch, sizeof(ch), "%llu", ctx->mi_count.panel_active / HZ);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_ACTIVE, ch);

		boot_time = ktime_get_boottime();
		do_div(boot_time, NSEC_PER_SEC);
		memset(ch, 0, sizeof(ch));
		snprintf(ch, sizeof(ch), "%llu", ctx->mi_count.boottime + boot_time);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BOOTTIME, ch);

		getnstimeofday(&rtctime);
		if (ctx->mi_count.bootRTCtime != 0) {
			if (rtctime.tv_sec > ctx->mi_count.bootRTCtime) {
				if (rtctime.tv_sec - ctx->mi_count.bootRTCtime > 10 * 365 * DAY_SECS) {
					ctx->mi_count.bootRTCtime = rtctime.tv_sec;
				} else {
					if (rtctime.tv_sec - ctx->mi_count.bootRTCtime > DAY_SECS) {
						delta_days = (rtctime.tv_sec - ctx->mi_count.bootRTCtime) / DAY_SECS;
						ctx->mi_count.bootdays += delta_days;
						ctx->mi_count.bootRTCtime = rtctime.tv_sec -
							((rtctime.tv_sec - ctx->mi_count.bootRTCtime) % DAY_SECS);
					}
				}
			} else {
				pr_err("RTC time rollback!\n");
				ctx->mi_count.bootRTCtime = rtctime.tv_sec;
			}
		} else {
			pr_info("panel_info.bootRTCtime init!\n");
			ctx->mi_count.bootRTCtime = rtctime.tv_sec;
		}
		memset(ch, 0, sizeof(ch));
		snprintf(ch, sizeof(ch), "%llu", ctx->mi_count.bootdays);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_DAYS, ch);
		atomic_set(&panel_active_count_enable, 0);
	}

	return;
}

void dsi_panel_HBM_count(struct drm_panel *panel, int enable, int off)
{
	struct lcm *ctx = panel_to_lcm(panel);

	static u64 timestamp_hbmon;
	static int last_HBM_status;
	u64 jiffies_time = 0;
	char ch[64] = {0};
	bool record = false;

	if (!panel) {
		pr_err("invalid params\n");
		return;
	}

	pr_info("hbm_times[%lld],en[%d] off[%d]\n", ctx->mi_count.hbm_times, enable, off);

	if (off) {
		if (last_HBM_status == 1)
			record = true;
	} else {
		if (enable) {
			if (!last_HBM_status) {
				/* get HBM on timestamp */
				timestamp_hbmon = get_jiffies_64();
			}
		} else if (last_HBM_status){
			record = true;
		}
	}

	if (record) {
		/* caculate panel hbm duration */
		jiffies_time = get_jiffies_64();
		if (time_after64(jiffies_time, timestamp_hbmon))
			ctx->mi_count.hbm_duration += (jiffies_time - timestamp_hbmon) / HZ;
		snprintf(ch, sizeof(ch), "%llu", ctx->mi_count.hbm_duration);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_HBM_DRUATION, ch);

		/* caculate panel hbm times */
		memset(ch, 0, sizeof(ch));
		ctx->mi_count.hbm_times++;
		snprintf(ch, sizeof(ch), "%llu", ctx->mi_count.hbm_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_HBM_TIMES, ch);
	}

	last_HBM_status = enable;

	return;
}

void dsi_panel_count_init(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	int i = 0;
	atomic_set(&panel_active_count_enable, 0);
	ctx->mi_count.panel_active = 0;
	ctx->mi_count.kickoff_count = 0;
	ctx->mi_count.hbm_duration = 0;
	ctx->mi_count.hbm_times = 0;

	for (i = 0; i < FPS_MAX_NUM; i++) {
		ctx->mi_count.fps_times[i] = 0;
	}
	i = 0;

	register_hw_monitor_info(HWMON_CONPONENT_NAME);

	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_ACTIVE, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BOOTTIME, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_DAYS, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_HBM_DRUATION, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_HBM_TIMES, "0");
	for (i = 0; i < FPS_MAX_NUM; i++)
		add_hw_monitor_info(HWMON_CONPONENT_NAME, hwmon_key_fps[i], "0");

	dsi_panel_state_count(panel, 1);
	dsi_panel_fps_count(panel, 0, 1);
	return;
}

#if 0
int dsi_panel_disp_count_set(struct drm_panel *panel, const char *buf)
{
	struct lcm *ctx = panel_to_lcm(panel);

	char count_str[64] = {0};
	int i = 0;
	u64 fps_times[FPS_MAX_NUM] = {0};
	u64 record_end = 0;

	ssize_t result;
	struct timespec rtctime;

	pr_info("[LCD] %s: begin\n", __func__);

	if (!panel) {
		pr_err("invalid panel\n");
		return -EINVAL;
	}

	result = sscanf(buf,
		"fps60_times=%llu\n"
		"fps120_times=%llu\n"
		"record_end=%llu\n",
		&fps_times[FPS_60],
		&fps_times[FPS_120],
		&record_end);

	if (result != 3) {
		pr_err("sscanf buf error!\n");
		return -EINVAL;
	}

	for (i = 0; i < FPS_MAX_NUM; i++) {
		memset(count_str, 0, sizeof(count_str));
		ctx->mi_count.fps_times[i] = fps_times[i];
		snprintf(count_str, sizeof(count_str), "%llu", ctx->mi_count.fps_times[i]);
		add_hw_monitor_info(HWMON_CONPONENT_NAME, hwmon_key_fps[i], count_str);
	}

	pr_info("[LCD] %s: end\n", __func__);

	return 0;
}

ssize_t dsi_panel_disp_count_get(struct drm_panel *panel, char *buf)
{
	struct lcm *ctx = panel_to_lcm(panel);

	int ret = -1;
	ktime_t boot_time;
	u64 record_end = 0;
	/* struct timespec rtctime; */

	if (!panel) {
		pr_err("invalid panel\n");
		return -EINVAL;
	}

	if (buf == NULL) {
		pr_err("dsi_panel_disp_count_get buffer is NULL!\n");
		return -EINVAL;
	}

	ret = scnprintf(buf, PAGE_SIZE,
		"fps60_times=%llu\n"
		"fps120_times=%llu\n"
		"record_end=%llu\n",
		ctx->mi_count.fps_times[FPS_60],
		ctx->mi_count.fps_times[FPS_120],
		record_end);

	return ret;
}
#endif

