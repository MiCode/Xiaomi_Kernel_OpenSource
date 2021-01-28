/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#ifndef __ARCHCOUNTER_TIMESYNC__
#define __ARCHCOUNTER_TIMESYNC__

void archcounter_timesync_init(uint8_t status);
uint64_t archcounter_timesync_to_monotonic(uint64_t hwclock);
uint64_t archcounter_timesync_to_boot(uint64_t hwclock);
u64 mtk_get_archcounter_time(u64 cyc);

#endif
