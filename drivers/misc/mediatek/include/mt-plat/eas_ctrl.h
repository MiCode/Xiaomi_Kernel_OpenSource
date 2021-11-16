/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef _EAS_CTRL_H
#define _EAS_CTRL_H


enum {
	CGROUP_ROOT = 0,
	CGROUP_FG,
	CGROUP_BG,
	CGROUP_TA,
	CGROUP_RT,
	NR_CGROUP
};

enum {
	EAS_KIR_PERF = 0,
	EAS_KIR_BOOT,
	EAS_KIR_TOUCH,
	EAS_MAX_KIR
};

enum {
	EAS_UCLAMP_KIR_PERF = 0,
	EAS_UCLAMP_KIR_BOOT,
	EAS_UCLAMP_KIR_TOUCH,
	EAS_UCLAMP_KIR_FPSGO,
	EAS_UCLAMP_KIR_WIFI,
	EAS_UCLAMP_KIR_GBE,
	EAS_UCLAMP_KIR_BIG_TASK,
	EAS_UCLAMP_MAX_KIR
};

/* stune down thres */
enum {
	EAS_THRES_KIR_PERF = 0,
	EAS_THRES_KIR_FPSGO,
	EAS_THRES_MAX_KIR
};

enum {
	EAS_SYNC_FLAG_KIR_PERF = 0,
	EAS_SYNC_FLAG_KIR_FPSGO,
	EAS_SYNC_FLAG_MAX_KIR
};

enum {
	EAS_PREFER_IDLE_KIR_PERF = 0,
	EAS_PREFER_IDLE_KIR_FPSGO,
	EAS_PREFER_IDLE_MAX_KIR = 16
};

extern int boost_write_for_perf_idx(int group_idx, int boost_value);
extern int uclamp_min_for_perf_idx(int group_idx, int min_value);
extern int prefer_idle_for_perf_idx(int idx, int prefer_idle);

/* perfmgr */
extern int update_eas_boost_value(int kicker, int cgroup_idx, int value);
extern int update_eas_uclamp_min(int kicker, int cgroup_idx, int value);
extern int update_schedplus_down_throttle_ns(int kicker, int nsec);
extern int update_schedplus_up_throttle_ns(int kicker, int nsec);
extern int update_schedplus_sync_flag(int kicker, int enable);
extern int update_prefer_idle_value(int kicker, int cgroup_idx, int value);

#endif /* _EAS_CTRL_H */
