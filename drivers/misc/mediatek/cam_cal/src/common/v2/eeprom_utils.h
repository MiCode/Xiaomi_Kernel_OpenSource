/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __EEPROM_UTILS_H
#define __EEPROM_UTILS_H

#include <linux/ktime.h>
#include <linux/time64.h>
#include <linux/timekeeping.h>

#define must_log(...) pr_debug(__VA_ARGS__)

#define error_log(...) pr_debug("error: " __VA_ARGS__)

#define debug_log(...) \
do { \
	if (unlikely(debug_flag())) \
		pr_debug(__VA_ARGS__); \
} while (0)

unsigned int debug_flag(void);

void EEPROM_PROFILE_INIT(struct timespec64 *ptv);
void EEPROM_PROFILE(struct timespec64 *ptv, char *tag);

#endif
