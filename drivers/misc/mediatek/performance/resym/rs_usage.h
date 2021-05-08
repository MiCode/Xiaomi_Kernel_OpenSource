/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef RS_USAGE_H
#define RS_USAGE_H

enum  {
	USAGE_DEVTYPE_CPU  = 0,
	USAGE_DEVTYPE_GPU  = 1,
	USAGE_DEVTYPE_APU  = 2,
	USAGE_DEVTYPE_MDLA = 3,
	USAGE_DEVTYPE_VPU  = 4,
	USAGE_DEVTYPE_MAX  = 5,
};

extern void (*rsu_cpufreq_notifier_fp)(int cluster_id, unsigned long freq);
extern void (*rsu_getusage_fp)(__s32 *devusage, __u32 *bwusage, __u32 pid);

int __init rs_usage_init(void);
void __exit rs_usage_exit(void);

extern unsigned int __attribute__((weak)) mt_ppm_userlimit_freq_limit_by_others(
	unsigned int cluster);

#endif

