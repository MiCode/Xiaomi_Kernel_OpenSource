/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _SSPM_TIMESYNC_H_
#define _SSPM_TIMESYNC_H_

#define SHAREMBOX_NO_MCDI            3
#define SHAREMBOX_OFFSET_MCDI        0
#define SHAREMBOX_SIZE_MCDI          20
#define SHAREMBOX_OFFSET_TIMESTAMP   (SHAREMBOX_OFFSET_MCDI + \
					SHAREMBOX_SIZE_MCDI)
#define SHAREMBOX_SIZE_TIMESTAMP     6

#define SSPM_TS_MBOX                      SHAREMBOX_NO_MCDI
#define SSPM_TS_MBOX_OFFSET_BASE          SHAREMBOX_OFFSET_TIMESTAMP

/*
 * Shared MBOX: AP write, SSPM read
 * Unit for each offset: 4 bytes
 */

#define SSPM_TS_MBOX_TICK_H               (SSPM_TS_MBOX_OFFSET_BASE + 0)
#define SSPM_TS_MBOX_TICK_L               (SSPM_TS_MBOX_OFFSET_BASE + 1)
#define SSPM_TS_MBOX_TS_H                 (SSPM_TS_MBOX_OFFSET_BASE + 2)
#define SSPM_TS_MBOX_TS_L                 (SSPM_TS_MBOX_OFFSET_BASE + 3)
#define SSPM_TS_MBOX_DEBUG_TS_H           (SSPM_TS_MBOX_OFFSET_BASE + 4)
#define SSPM_TS_MBOX_DEBUG_TS_L           (SSPM_TS_MBOX_OFFSET_BASE + 5)

void sspm_timesync_suspend(void);
void sspm_timesync_resume(void);
unsigned int __init sspm_timesync_init(void);
#endif // _SSPM_TIMESYNC_H_
