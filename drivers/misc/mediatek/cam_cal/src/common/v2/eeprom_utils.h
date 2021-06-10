/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef __EEPROM_UTILS_H
#define __EEPROM_UTILS_H

#include <linux/time.h>

void EEPROM_PROFILE_INIT(struct timeval *ptv);
void EEPROM_PROFILE(struct timeval *ptv, char *tag);

#endif
