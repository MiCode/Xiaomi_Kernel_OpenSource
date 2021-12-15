/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __EEPROM_UTILS_H
#define __EEPROM_UTILS_H

#include <linux/time.h>

void EEPROM_PROFILE_INIT(struct timeval *ptv);
void EEPROM_PROFILE(struct timeval *ptv, char *tag);

#endif
