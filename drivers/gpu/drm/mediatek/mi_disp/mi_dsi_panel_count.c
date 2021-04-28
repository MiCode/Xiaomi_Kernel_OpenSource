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

#define HWCONPONENT_NAME "display"
#define HWCONPONENT_KEY_LCD "LCD"
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

static char *hwmon_key_hbm[FPS_MAX_NUM] = {"30fps_times", "50fps_times",
	"60fps_times", "90fps_times", "120fps_times", "144fps_times"};
static const u32 dynamic_fps[FPS_MAX_NUM] = {30, 50, 60, 90, 120, 144};

#define DAY_SECS (60*60*24)

void dsi_panel_state_count(struct dsi_panel *panel, int enable)
{
	static u64 timestamp_panelon;
	static u64 on_times;
	static u64 off_times;
	char ch[64] = {0};

	if (enable) {
		/* get panel on timestamp */
		timestamp_panelon = get_jiffies_64();
		on_times++;
		panel->mi_count.panel_active_count_enable = true;
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
			panel->mi_count.panel_active += jiffies_time - timestamp_panelon;
		snprintf(ch, sizeof(ch), "%llu", panel->mi_count.panel_active / HZ);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_ACTIVE, ch);

		memset(ch, 0, sizeof(ch));
		snprintf(ch, sizeof(ch), "%llu", panel->mi_count.kickoff_count / 60);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_REFRESH, ch);

		boot_time = ktime_get_boottime();
		do_div(boot_time, NSEC_PER_SEC);
		memset(ch, 0, sizeof(ch));
		snprintf(ch, sizeof(ch), "%llu", panel->mi_count.boottime + boot_time);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BOOTTIME, ch);

		getnstimeofday(&rtctime);
		if (panel->mi_count.bootRTCtime != 0) {
			if (rtctime.tv_sec > panel->mi_count.bootRTCtime) {
				if (rtctime.tv_sec - panel->mi_count.bootRTCtime > 10 * 365 * DAY_SECS) {
					panel->mi_count.bootRTCtime = rtctime.tv_sec;
				} else {
					if (rtctime.tv_sec - panel->mi_count.bootRTCtime > DAY_SECS) {
						delta_days = (rtctime.tv_sec - panel->mi_count.bootRTCtime) / DAY_SECS;
						panel->mi_count.bootdays += delta_days;
						panel->mi_count.bootRTCtime = rtctime.tv_sec -
							((rtctime.tv_sec - panel->mi_count.bootRTCtime) % DAY_SECS);
					}
				}
			} else {
				pr_err("RTC time rollback!\n");
				panel->mi_count.bootRTCtime = rtctime.tv_sec;
			}
		} else {
			pr_info("panel_info.bootRTCtime init!\n");
			panel->mi_count.bootRTCtime = rtctime.tv_sec;
		}
		memset(ch, 0, sizeof(ch));
		snprintf(ch, sizeof(ch), "%llu", panel->mi_count.bootdays);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_DAYS, ch);

		panel->mi_count.panel_active_count_enable = false;
	}

	return;
}

void dsi_panel_HBM_count(struct dsi_panel *panel, int enable, int off)
{
	static u64 timestamp_hbmon;
	static int last_HBM_status;
	u64 jiffies_time = 0;
	char ch[64] = {0};
	bool record = false;

	pr_info("hbm_times[%lld],en[%d] off[%d]\n", panel->mi_count.hbm_times, enable, off);

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
			panel->mi_count.hbm_duration += (jiffies_time - timestamp_hbmon) / HZ;
		snprintf(ch, sizeof(ch), "%llu", panel->mi_count.hbm_duration);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_HBM_DRUATION, ch);

		/* caculate panel hbm times */
		memset(ch, 0, sizeof(ch));
		panel->mi_count.hbm_times++;
		snprintf(ch, sizeof(ch), "%llu", panel->mi_count.hbm_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_HBM_TIMES, ch);
	}

	last_HBM_status = enable;

	return;
}

void dsi_panel_backlight_count(struct dsi_panel *panel, u32 bl_lvl)
{
	static u32 last_bl_level;
	static u64 last_bl_start;
	u64 bl_level_end = 0;
	char ch[64] = {0};

	bl_level_end = get_jiffies_64();

	if (last_bl_start == 0) {
		last_bl_level = bl_lvl;
		last_bl_start = bl_level_end;
		return;
	}

	if (last_bl_level > 0) {
		panel->mi_count.bl_level_integral += last_bl_level * (bl_level_end - last_bl_start);
		panel->mi_count.bl_duration += (bl_level_end - last_bl_start);
	}

	/* backlight level 3071 ==> 450 nit */
	if (last_bl_level > (panel->bl_config.bl_max_level*3/4)) {
		panel->mi_count.bl_highlevel_duration += (bl_level_end - last_bl_start);
	} else if (last_bl_level > 0) {
		/* backlight level (0, 3071] */
		panel->mi_count.bl_lowlevel_duration += (bl_level_end - last_bl_start);
	}

	last_bl_level = bl_lvl;
	last_bl_start = bl_level_end;

	if (bl_lvl == 0) {
		memset(ch, 0, sizeof(ch));
		if (panel->mi_count.bl_duration > 0) {
			snprintf(ch, sizeof(ch), "%llu",
				panel->mi_count.bl_level_integral / panel->mi_count.bl_duration);
			update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BL_AVG, ch);
		}

		memset(ch, 0, sizeof(ch));
		snprintf(ch, sizeof(ch), "%llu", panel->mi_count.bl_highlevel_duration / HZ);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BL_HIGH, ch);

		memset(ch, 0, sizeof(ch));
		snprintf(ch, sizeof(ch), "%llu", panel->mi_count.bl_lowlevel_duration / HZ);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BL_LOW, ch);
	}

	return;
}

void dsi_panel_fps_count(struct dsi_panel *panel, u32 fps, u32 enable)
{
	static u32 timming_index = FPS_MAX_NUM;
	static u64 timestamp_fps;
	static u32 cur_fps = 0;
	u64 jiffies_time = 0;
	int i = 0;
	char ch[64] = {0};

	if (!panel || (!enable && timming_index == FPS_MAX_NUM))
		return;

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
				panel->mi_count.fps_times[timming_index] += (jiffies_time - timestamp_fps) / HZ;
			snprintf(ch, sizeof(ch), "%llu", panel->mi_count.fps_times[timming_index]);
			update_hw_monitor_info(HWMON_CONPONENT_NAME, hwmon_key_hbm[timming_index], ch);
			timestamp_fps = jiffies_time;
		} else if (timming_index == FPS_MAX_NUM)
			timestamp_fps = get_jiffies_64();

		timming_index = i;
	}

	cur_fps = fps ? fps : cur_fps;
}

void dsi_panel_fps_count_lock(struct dsi_panel *panel, u32 fps, u32 enable)
{
	if (!panel)
		return;

	mutex_lock(&panel->panel_lock);
	dsi_panel_fps_count(panel, fps, enable);
	mutex_unlock(&panel->panel_lock);
}

void dsi_panel_count_init(struct dsi_panel *panel)
{
	int i = 0;

	panel->mi_count.panel_active_count_enable = false;
	panel->mi_count.panel_active = 0;
	panel->mi_count.kickoff_count = 0;
	panel->mi_count.bl_duration = 0;
	panel->mi_count.bl_level_integral = 0;
	panel->mi_count.bl_highlevel_duration = 0;
	panel->mi_count.bl_lowlevel_duration = 0;
	panel->mi_count.hbm_duration = 0;
	panel->mi_count.hbm_times = 0;
	memset((void *)panel->mi_count.fps_times, 0, sizeof(panel->mi_count.fps_times));

	register_hw_monitor_info(HWMON_CONPONENT_NAME);
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_ACTIVE, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_REFRESH, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BOOTTIME, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_DAYS, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BL_AVG, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BL_HIGH, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BL_LOW, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_HBM_DRUATION, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_HBM_TIMES, "0");

	for (i = 0; i < FPS_MAX_NUM; i++)
		add_hw_monitor_info(HWMON_CONPONENT_NAME, hwmon_key_hbm[i], "0");

	dsi_panel_state_count(panel, 1);

	return;
}

int dsi_panel_disp_count_set(struct dsi_panel *panel, const char *buf)
{
	char count_str[64] = {0};
	int i = 0;
	u64 panel_active = 0;
	u64 kickoff_count = 0;
	u64 kernel_boottime = 0;
	u64 kernel_rtctime = 0;
	u64 kernel_days = 0;
	u64 record_end = 0;
	u32 delta_days = 0;
	u64 bl_duration = 0;
	u64 bl_level_integral = 0;
	u64 bl_highlevel_duration = 0;
	u64 bl_lowlevel_duration = 0;
	u64 hbm_duration = 0;
	u64 hbm_times = 0;
	u64 fps_times[FPS_MAX_NUM] = {0};

	ssize_t result;
	struct timespec rtctime;

	pr_info("[LCD] %s: begin\n", __func__);

	if (!panel) {
		pr_err("invalid panel\n");
		return -EINVAL;
	}

	result = sscanf(buf,
		"panel_active=%llu\n"
		"panel_kickoff_count=%llu\n"
		"kernel_boottime=%llu\n"
		"kernel_rtctime=%llu\n"
		"kernel_days=%llu\n"
		"bl_duration=%llu\n"
		"bl_level_integral=%llu\n"
		"bl_highlevel_duration=%llu\n"
		"bl_lowlevel_duration=%llu\n"
		"hbm_duration=%llu\n"
		"hbm_times=%llu\n"
		"fps30_times=%llu\n"
		"fps50_times=%llu\n"
		"fps60_times=%llu\n"
		"fps90_times=%llu\n"
		"fps120_times=%llu\n"
		"fps144_times=%llu\n"
		"record_end=%llu\n",
		&panel_active,
		&kickoff_count,
		&kernel_boottime,
		&kernel_rtctime,
		&kernel_days,
		&bl_duration,
		&bl_level_integral,
		&bl_highlevel_duration,
		&bl_lowlevel_duration,
		&hbm_duration,
		&hbm_times,
		&fps_times[FPS_30],
		&fps_times[FPS_50],
		&fps_times[FPS_60],
		&fps_times[FPS_90],
		&fps_times[FPS_120],
		&fps_times[FPS_144],
		&record_end);

	if (result != 18) {
		pr_err("sscanf buf error!\n");
		return -EINVAL;
	}
#if 0
	if (panel_active < panel.panel_active) {
		pr_err("Current panel_active < panel_info.panel_active!\n");
		return -EINVAL;
	}

	if (kickoff_count < panel.kickoff_count) {
		pr_err("Current kickoff_count < panel_info.kickoff_count!\n");
		return -EINVAL;
	}
#endif

	getnstimeofday(&rtctime);
	if (rtctime.tv_sec > kernel_rtctime) {
		if (rtctime.tv_sec - kernel_rtctime > 10 * 365 * DAY_SECS) {
			panel->mi_count.bootRTCtime = rtctime.tv_sec;
		} else {
			if (rtctime.tv_sec - kernel_rtctime > DAY_SECS) {
				delta_days = (rtctime.tv_sec - kernel_rtctime) / DAY_SECS;
				panel->mi_count.bootRTCtime = rtctime.tv_sec - ((rtctime.tv_sec - kernel_rtctime) % DAY_SECS);
			} else {
				panel->mi_count.bootRTCtime = kernel_rtctime;
			}
		}
	} else {
		pr_err("RTC time rollback!\n");
		panel->mi_count.bootRTCtime = kernel_rtctime;
	}

	panel->mi_count.panel_active = panel_active;
	snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.panel_active/HZ);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_ACTIVE, count_str);

	memset(count_str, 0, sizeof(count_str));
	panel->mi_count.kickoff_count = kickoff_count;
	snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.kickoff_count/60);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_REFRESH, count_str);

	memset(count_str, 0, sizeof(count_str));
	panel->mi_count.boottime = kernel_boottime;
	snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.boottime);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BOOTTIME, count_str);

	memset(count_str, 0, sizeof(count_str));
	panel->mi_count.bootdays = kernel_days + delta_days;
	snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.bootdays);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_DAYS, count_str);

	memset(count_str, 0, sizeof(count_str));
	panel->mi_count.bl_level_integral = bl_level_integral;
	panel->mi_count.bl_duration = bl_duration;
	if (panel->mi_count.bl_duration > 0) {
		snprintf(count_str, sizeof(count_str), "%llu",
			panel->mi_count.bl_level_integral / panel->mi_count.bl_duration);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BL_AVG, count_str);
	}

	memset(count_str, 0, sizeof(count_str));
	panel->mi_count.bl_highlevel_duration = bl_highlevel_duration;
	snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.bl_highlevel_duration);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BL_HIGH, count_str);

	memset(count_str, 0, sizeof(count_str));
	panel->mi_count.bl_lowlevel_duration = bl_lowlevel_duration;
	snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.bl_lowlevel_duration);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BL_LOW, count_str);

	memset(count_str, 0, sizeof(count_str));
	panel->mi_count.hbm_duration = hbm_duration;
	snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.hbm_duration);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_HBM_DRUATION, count_str);

	memset(count_str, 0, sizeof(count_str));
	panel->mi_count.hbm_times = hbm_times;
	snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.hbm_times);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_HBM_TIMES, count_str);

	for (i = 0; i < FPS_MAX_NUM; i++) {
		memset(count_str, 0, sizeof(count_str));
		panel->mi_count.fps_times[i] = fps_times[i];
		snprintf(count_str, sizeof(count_str), "%llu", panel->mi_count.fps_times[i]);
		add_hw_monitor_info(HWMON_CONPONENT_NAME, hwmon_key_hbm[i], count_str);
	}

	pr_info("[LCD] %s: end\n", __func__);

	return 0;
}

ssize_t dsi_panel_disp_count_get(struct dsi_panel *panel, char *buf)
{
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

	boot_time = ktime_get_boottime();
	do_div(boot_time, NSEC_PER_SEC);
	/* getnstimeofday(&rtctime); */

	ret = scnprintf(buf, PAGE_SIZE,
		"panel_active=%llu\n"
		"panel_kickoff_count=%llu\n"
		"kernel_boottime=%llu\n"
		"kernel_rtctime=%llu\n"
		"kernel_days=%llu\n"
		"bl_duration=%llu\n"
		"bl_level_integral=%llu\n"
		"bl_highlevel_duration=%llu\n"
		"bl_lowlevel_duration=%llu\n"
		"hbm_duration=%llu\n"
		"hbm_times=%llu\n"
		"fps30_times=%llu\n"
		"fps50_times=%llu\n"
		"fps60_times=%llu\n"
		"fps90_times=%llu\n"
		"fps120_times=%llu\n"
		"fps144_times=%llu\n"
		"record_end=%llu\n",
		panel->mi_count.panel_active,
		panel->mi_count.kickoff_count,
		panel->mi_count.boottime + boot_time,
		panel->mi_count.bootRTCtime,
		panel->mi_count.bootdays,
		panel->mi_count.bl_duration,
		panel->mi_count.bl_level_integral,
		panel->mi_count.bl_highlevel_duration,
		panel->mi_count.bl_lowlevel_duration,
		panel->mi_count.hbm_duration,
		panel->mi_count.hbm_times,
		panel->mi_count.fps_times[FPS_30],
		panel->mi_count.fps_times[FPS_50],
		panel->mi_count.fps_times[FPS_60],
		panel->mi_count.fps_times[FPS_90],
		panel->mi_count.fps_times[FPS_120],
		panel->mi_count.fps_times[FPS_144],
		record_end);

	return ret;
}
