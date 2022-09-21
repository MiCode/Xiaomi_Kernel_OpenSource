/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#include <linux/notifier.h>
#include "mi_hwconf_manager.h"
#include "dsi_panel.h"

#define HWCONPONENT_NAME "display"
#define HWCONPONENT_KEY_LCD "LCD"
//#define HWMON_CONPONENT_NAME "display"
#define HWMON_KEY_ACTIVE "panel_active"
#define HWMON_KEY_REFRESH "panel_refresh"
#define HWMON_KEY_BOOTTIME "kernel_boottime"
#define HWMON_KEY_DAYS "kernel_days"
#define HWMON_KEY_BL_AVG "bl_level_avg"
#define HWMON_KEY_BL_HIGH "bl_level_high"
#define HWMON_KEY_BL_LOW "bl_level_low"
#define HWMON_KEY_HBM_DRUATION "hbm_duration"
#define HWMON_KEY_HBM_TIMES "hbm_times"

static char *hwmon_key_fps[FPS_MAX_NUM] = {"30fps_times", "50fps_times",
	"60fps_times", "90fps_times", "120fps_times", "144fps_times"};
static const u32 dynamic_fps[FPS_MAX_NUM] = {30, 50, 60, 90, 120, 144};

char HWMON_CONPONENT_NAME[64];
int index = MI_DISP_PRIMARY;

#define DAY_SECS (60*60*24)

void mi_dsi_panel_state_count(struct dsi_panel *panel, PANEL_COUNT_EVENT event, int value)
{
	char ch[64] = {0};

	if (!strcmp(panel->type, "primary")) {
		index = MI_DISP_PRIMARY;
		strcpy(HWMON_CONPONENT_NAME, "display");
	} else {
		index = MI_DISP_SECONDARY;
		strcpy(HWMON_CONPONENT_NAME, "display2");
	}

	switch (event)
	{
	case PANEL_ACTIVE:
	{
		static u32 on_times[MI_DISP_MAX];
		static u32 off_times[MI_DISP_MAX];
		static u64 timestamp_panelon[MI_DISP_MAX];
		if (value && !panel->mi_count.panel_active_count_enable) {
			timestamp_panelon[index] = get_jiffies_64();
			on_times[index]++;
			panel->mi_count.panel_active_count_enable = true;
		} else if (!value && panel->mi_count.panel_active_count_enable){
			ktime_t boot_time;
			u32 delta_days = 0;
			u64 jiffies_time = 0;
			struct timespec rtctime;

			off_times[index]++;
			pr_info("%s: %s on_times[%llu] off_times[%llu]\n", __func__, HWMON_CONPONENT_NAME, on_times[index], off_times[index]);

			jiffies_time = get_jiffies_64();
			if (time_after64(jiffies_time, timestamp_panelon[index]))
				panel->mi_count.panel_active += (jiffies_time - timestamp_panelon[index]);
			memset(ch, 0, sizeof(ch));
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
	}
	break;
	case PANEL_BACKLIGHT:
	{
		static u32 last_bl_lvl[MI_DISP_MAX];
		static u64 last_bl_start[MI_DISP_MAX];
		u64 bl_level_end = 0;

		bl_level_end = get_jiffies_64();

		if (last_bl_start[index] == 0) {
			last_bl_lvl[index] = value;
			last_bl_start[index] = bl_level_end;
			return;
		}

		if (last_bl_lvl[index] > 0) {
			panel->mi_count.bl_level_integral += last_bl_lvl[index] * (bl_level_end - last_bl_start[index]);
			panel->mi_count.bl_duration += (bl_level_end - last_bl_start[index]);
		}

		/* backlight level 2047*0.75 ==> 1535 */
		if (last_bl_lvl[index] > 1535) {
			panel->mi_count.bl_highlevel_duration += (bl_level_end - last_bl_start[index]);
		} else if (last_bl_lvl[index] > 0) {
		/* backlight level (0, 1535] */
		panel->mi_count.bl_lowlevel_duration += (bl_level_end - last_bl_start[index]);
		}

		last_bl_lvl[index] = value;
		last_bl_start[index] = bl_level_end;

		if (value == 0) {
			memset(ch, 0, sizeof(ch));
			if (panel->mi_count.bl_duration > 0) {
				snprintf(ch, sizeof(ch), "%llu", 	panel->mi_count.bl_level_integral / panel->mi_count.bl_duration);
				update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BL_AVG, ch);
			}

			memset(ch, 0, sizeof(ch));
			snprintf(ch, sizeof(ch), "%llu", panel->mi_count.bl_highlevel_duration / HZ);
			update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BL_HIGH, ch);

			memset(ch, 0, sizeof(ch));
			snprintf(ch, sizeof(ch), "%llu", panel->mi_count.bl_lowlevel_duration / HZ);
			update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BL_LOW, ch);
		}
	}
	break;
	case PANEL_HBM:
	{
		static u64 time_hbm_on[MI_DISP_MAX]={0};
		u64 time_hbm_off = 0;
		static int enter_flag[MI_DISP_MAX] = {[0 ... MI_DISP_MAX-1] = 1};
		static int exit_flag[MI_DISP_MAX] = {0};

		if (value > panel->mi_cfg.hbm_backlight_threshold && enter_flag[index]) {
			time_hbm_on[index] = get_jiffies_64();
			enter_flag[index] = 0;
			exit_flag[index] = 1;
		} else if (value <= panel->mi_cfg.hbm_backlight_threshold && exit_flag[index]) {
			time_hbm_off = get_jiffies_64();
			if (time_after64(time_hbm_off, time_hbm_on[index])) {
				panel->mi_count.hbm_duration += (time_hbm_off-time_hbm_on[index]);
				panel->mi_count.hbm_times++;
			}
			enter_flag[index] = 1;
			exit_flag[index] = 0;

			memset(ch, 0, sizeof(ch));
			snprintf(ch, sizeof(ch), "%llu", panel->mi_count.hbm_duration / HZ);
			update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_HBM_DRUATION, ch);

			memset(ch, 0, sizeof(ch));
			snprintf(ch, sizeof(ch), "%llu", panel->mi_count.hbm_times);
			update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_HBM_TIMES, ch);
		}
	}
	break;
	case PANEL_FPS:
	{
		static u32 cur_fps_index[MI_DISP_MAX] = {0};
		static bool first_into_flag[MI_DISP_MAX] = {[0 ... MI_DISP_MAX-1]=true};
		int change_fps_index = 0;
		static u64 timestamp_fps[MI_DISP_MAX];
		u64 jiffies_time = 0;

		for (change_fps_index = 0; change_fps_index < FPS_MAX_NUM; change_fps_index++) {
			if (value == dynamic_fps[change_fps_index])
				break;
		}

		if (first_into_flag[index]) {
			timestamp_fps[index] = get_jiffies_64();
			first_into_flag[index] = false;
			cur_fps_index[index] = change_fps_index;
			return;
		}

		if (change_fps_index != cur_fps_index[index]) {
			jiffies_time = get_jiffies_64();
			if (cur_fps_index[index] != FPS_MAX_NUM) {
				if (time_after64(jiffies_time, timestamp_fps[index]))
					panel->mi_count.fps_times[cur_fps_index[index]] += (jiffies_time - timestamp_fps[index]);
				snprintf(ch, sizeof(ch), "%llu", panel->mi_count.fps_times[cur_fps_index[index]] / HZ);
				update_hw_monitor_info(HWMON_CONPONENT_NAME, hwmon_key_fps[cur_fps_index[index]], ch);
			}
			timestamp_fps[index] = get_jiffies_64();
		}
		cur_fps_index[index] = change_fps_index;
	}
	break;
	default:
		break;
	}
	return;
}

void mi_dsi_panel_HBM_count(struct dsi_panel *panel, int enable, int off)
{
	static u64 timestamp_hbmon[MI_DISP_MAX];
	static int last_HBM_status[MI_DISP_MAX];
	u64 jiffies_time = 0;
	char ch[64] = {0};
	bool record = false;

	if (!strcmp(panel->type, "primary")) {
		index = MI_DISP_PRIMARY;
		strcpy(HWMON_CONPONENT_NAME, "display");
	} else {
		index = MI_DISP_SECONDARY;
		strcpy(HWMON_CONPONENT_NAME, "display2");
	}

	pr_info("%s, hbm_times[%lld],en[%d] off[%d]\n", HWMON_CONPONENT_NAME, panel->mi_count.hbm_times, enable, off);

	if (off) {
		if (last_HBM_status[index] == 1)
			record = true;
	} else {
		if (enable) {
			if (!last_HBM_status[index]) {
				/* get HBM on timestamp */
				timestamp_hbmon[index] = get_jiffies_64();
			}
		} else if (last_HBM_status[index]){
			record = true;
		}
	}

	if (record) {
		/* caculate panel hbm duration */
		jiffies_time = get_jiffies_64();
		if (time_after64(jiffies_time, timestamp_hbmon[index]))
			panel->mi_count.hbm_duration += (jiffies_time - timestamp_hbmon[index]) / HZ;
		snprintf(ch, sizeof(ch), "%llu", panel->mi_count.hbm_duration);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_HBM_DRUATION, ch);

		/* caculate panel hbm times */
		memset(ch, 0, sizeof(ch));
		panel->mi_count.hbm_times++;
		snprintf(ch, sizeof(ch), "%llu", panel->mi_count.hbm_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_HBM_TIMES, ch);
	}

	last_HBM_status[index] = enable;

	return;
}

void mi_dsi_panel_HBM_count_bl(struct dsi_panel *panel, u32 bl_lvl)
{
	char ch[64] = {0};
	static u64 time_hbm_on[MI_DISP_MAX]={0};
	static u64 time_hbm_off[MI_DISP_MAX]={0};
	static int enter_flag[MI_DISP_MAX] = {1,1};
	static int exit_flag[MI_DISP_MAX] = {0,0};

	if (!strcmp(panel->type, "primary")) {
		index = MI_DISP_PRIMARY;
		strcpy(HWMON_CONPONENT_NAME, "display");
	} else {
		index = MI_DISP_SECONDARY;
		strcpy(HWMON_CONPONENT_NAME, "display2");
	}
	if(bl_lvl > panel->mi_cfg.hbm_backlight_threshold && enter_flag[index]) {
		time_hbm_on[index] = get_jiffies_64();
		enter_flag[index] = 0;
		exit_flag[index] = 1;
	} else if (bl_lvl <= panel->mi_cfg.hbm_backlight_threshold && exit_flag[index]) {
		time_hbm_off[index] = get_jiffies_64();
		if (time_after64(time_hbm_off[index], time_hbm_on[index])) {
			panel->mi_count.hbm_duration += (time_hbm_off[index] - time_hbm_on[index]) / HZ;
			panel->mi_count.hbm_times++;
			enter_flag[index] = 1;
			exit_flag[index] = 0;
		}
	}

	if (bl_lvl == 0) {
		memset(ch, 0, sizeof(ch));
		snprintf(ch, sizeof(ch), "%llu", panel->mi_count.hbm_duration);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_HBM_DRUATION, ch);

		/* caculate panel hbm times */
		memset(ch, 0, sizeof(ch));
		snprintf(ch, sizeof(ch), "%llu", panel->mi_count.hbm_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_HBM_TIMES, ch);
	}

	return;
}

void mi_dsi_panel_backlight_count(struct dsi_panel *panel, u32 bl_lvl)
{
	static u32 last_bl_level[MI_DISP_MAX];
	static u64 last_bl_start[MI_DISP_MAX];
	u64 bl_level_end = 0;
	char ch[64] = {0};

	if (!strcmp(panel->type, "primary")) {
		index = MI_DISP_PRIMARY;
		strcpy(HWMON_CONPONENT_NAME, "display");
	} else {
		index = MI_DISP_SECONDARY;
		strcpy(HWMON_CONPONENT_NAME, "display2");
	}

	bl_level_end = get_jiffies_64();

	if (last_bl_start[index] == 0) {
		last_bl_level[index] = bl_lvl;
		last_bl_start[index] = bl_level_end;
		return;
	}

	if (last_bl_level[index] > 0) {
		panel->mi_count.bl_level_integral += last_bl_level[index] * (bl_level_end - last_bl_start[index]);
		panel->mi_count.bl_duration += (bl_level_end - last_bl_start[index]);
	}

	/* backlight level 2047*0.75 ==> 1535 */
	if (last_bl_level[index] > 1535) {
		panel->mi_count.bl_highlevel_duration += (bl_level_end - last_bl_start[index]);
	} else if (last_bl_level[index] > 0) {
		/* backlight level (0, 1535] */
		panel->mi_count.bl_lowlevel_duration += (bl_level_end - last_bl_start[index]);
	}

	last_bl_level[index] = bl_lvl;
	last_bl_start[index] = bl_level_end;

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

void mi_dsi_panel_fps_count(struct dsi_panel *panel, u32 fps, u32 enable)
{
	static u32 timming_index[MI_DISP_MAX] = {FPS_MAX_NUM, FPS_MAX_NUM}; //TODO
	static u64 timestamp_fps[MI_DISP_MAX];
	static u32 cur_fps[MI_DISP_MAX] = {0};
	u64 jiffies_time = 0;
	int i = 0;
	char ch[64] = {0};

	if (!strcmp(panel->type, "primary")) {
		index = MI_DISP_PRIMARY;
		strcpy(HWMON_CONPONENT_NAME, "display");
	} else {
		index = MI_DISP_SECONDARY;
		strcpy(HWMON_CONPONENT_NAME, "display2");
	}

	if (!panel || (!enable && timming_index[index] == FPS_MAX_NUM))
		return;

	if (enable && !fps)
		fps = cur_fps[index];

	for(i = 0; i < FPS_MAX_NUM; i++) {
		if (fps == dynamic_fps[i])
			break;
	}

	if (i < FPS_MAX_NUM || !fps) {
		if (i != timming_index[index] && timming_index[index] < FPS_MAX_NUM) {
			jiffies_time = get_jiffies_64();
			if (time_after64(jiffies_time, timestamp_fps[index]))
				panel->mi_count.fps_times[timming_index[index]] += (jiffies_time - timestamp_fps[index]) / HZ;
			snprintf(ch, sizeof(ch), "%llu", panel->mi_count.fps_times[timming_index[index]]);
			update_hw_monitor_info(HWMON_CONPONENT_NAME, hwmon_key_fps[timming_index[index]], ch);
			timestamp_fps[index] = jiffies_time;
		} else if (timming_index[index] == FPS_MAX_NUM)
			timestamp_fps[index] = get_jiffies_64();

		timming_index[index] = i;
	}

	cur_fps[index] = fps ? fps : cur_fps[index];
}

void mi_dsi_panel_fps_count_lock(struct dsi_panel *panel, u32 fps, u32 enable)
{
	if (!panel)
		return;

	mutex_lock(&panel->panel_lock);
	mi_dsi_panel_fps_count(panel, fps, enable);
	mutex_unlock(&panel->panel_lock);
}

void mi_dsi_panel_count_init(struct dsi_panel *panel)
{
	int i = 0;
	if (!strcmp(panel->type, "primary")) {
		index = MI_DISP_PRIMARY;
		strcpy(HWMON_CONPONENT_NAME, "display");
	} else {
		index = MI_DISP_SECONDARY;
		strcpy(HWMON_CONPONENT_NAME, "display2");
	}

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
		add_hw_monitor_info(HWMON_CONPONENT_NAME, hwmon_key_fps[i], "0");

	mi_dsi_panel_state_count(panel, PANEL_ACTIVE, 1);

	return;
}

int mi_dsi_panel_set_disp_count(struct dsi_panel *panel, const char *buf)
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

	// if (!strcmp(panel->type, "primary")) {
	// 	index = MI_DISP_PRIMARY;
	// 	strcpy(HWMON_CONPONENT_NAME, "display");
	// } else {
	// 	index = MI_DISP_SECONDARY;
	// 	strcpy(HWMON_CONPONENT_NAME, "display2");
	// }

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
		add_hw_monitor_info(HWMON_CONPONENT_NAME, hwmon_key_fps[i], count_str);
	}

	pr_info("[LCD] %s: end\n", __func__);

	return 0;
}

ssize_t mi_dsi_panel_get_disp_count(struct dsi_panel *panel,
			char *buf, size_t size)
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

	ret = scnprintf(buf, size,
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
