/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __ARCHCOUNTER_TIMESYNC__
#define __ARCHCOUNTER_TIMESYNC__

void archcounter_timesync_init(uint8_t status);
uint64_t archcounter_timesync_to_monotonic(uint64_t hwclock);
uint64_t archcounter_timesync_to_boot(uint64_t hwclock);

#endif
