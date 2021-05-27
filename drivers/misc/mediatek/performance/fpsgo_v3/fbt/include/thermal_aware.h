// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __FPSGO_THERMAL_AWARE_H__
#define __FPSGO_THERMAL_AWARE_H__

void __init thrm_aware_init(struct kobject *dir_kobj);
void __exit thrm_aware_exit(void);
void thrm_aware_frame_start(int perf_hint, int target_fps);
void thrm_aware_switch(int enable);

#endif
