// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define PFX "CAM_CAL"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__

#include <linux/moduleparam.h>

#include "eeprom_utils.h"

static unsigned int cam_eeprom_debug;

module_param(cam_eeprom_debug, uint, 0644);
MODULE_PARM_DESC(cam_eeprom_debug, "camera_eeprom_debug");

unsigned int debug_flag(void)
{
	return cam_eeprom_debug;
}

#define EEPROM_PROF 1

#if EEPROM_PROF
void EEPROM_PROFILE_INIT(struct timespec64 *ptv)
{
	ktime_get_real_ts64(ptv);
}

void EEPROM_PROFILE(struct timespec64 *ptv, char *tag)
{
	struct timespec64 now, diff;

	ktime_get_real_ts64(&now);
	diff = timespec64_sub(now, *ptv);

	pr_info("[%s]Profile = %llu ns\n", tag, timespec64_to_ns(&diff));
}

#else
void EEPROM_PROFILE_INIT(struct timespec64 *ptv) {}
void EEPROM_PROFILE(struct timespec64 *ptv, char *tag) {}
#endif

