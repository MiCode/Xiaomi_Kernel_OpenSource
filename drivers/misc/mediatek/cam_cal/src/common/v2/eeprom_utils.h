/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __EEPROM_UTILS_H
#define __EEPROM_UTILS_H

#include <linux/ktime.h>
#include <linux/time64.h>
#include <linux/timekeeping.h>

void EEPROM_PROFILE_INIT(struct timespec64 *ptv);
void EEPROM_PROFILE(struct timespec64 *ptv, char *tag);

#endif
