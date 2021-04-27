/*
 * Copyright (C) 2020 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __FPSGO_THERMAL_AWARE_H__
#define __FPSGO_THERMAL_AWARE_H__

#ifdef CONFIG_FPSGO_THERMAL_SUPPORT
void __init thrm_aware_init(struct kobject *dir_kobj);
void __exit thrm_aware_exit(void);
void thrm_aware_frame_start(int perf_hint, int target_fps);
void thrm_aware_switch(int enable);
#else
static inline void thrm_aware_init(struct kobject *dir_kobj) { }
static inline void thrm_aware_exit(void) { }
static inline void thrm_aware_frame_start(int perf_hint, int target_fps) { }
static inline void thrm_aware_switch(int enable) { }
#endif

#endif
