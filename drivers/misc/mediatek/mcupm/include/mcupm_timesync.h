/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MCUPM_TIMESYNC_H_
#define _MCUPM_TIMESYNC_H_

#define SHAREMBOX_ON_SUSPEND         4
#define SHAREMBOX_OFFSET             0
#define SHAREMBOX_SIZE_SUSPEND          12
#define SHAREMBOX_OFFSET_TIMESTAMP   (SHAREMBOX_OFFSET + \
					SHAREMBOX_SIZE_SUSPEND)
#define SHAREMBOX_SIZE_TIMESTAMP     6

#define MCUPM_TS_MBOX                     SHAREMBOX_ON_SUSPEND
#define MCUPM_TS_MBOX_OFFSET_BASE         SHAREMBOX_OFFSET_TIMESTAMP

/*
 * Shared MBOX: AP write, MCUPM read
 * Unit for each offset: 4 bytes
 */

#define MCUPM_TS_MBOX_TICK_H               (MCUPM_TS_MBOX_OFFSET_BASE + 0)
#define MCUPM_TS_MBOX_TICK_L               (MCUPM_TS_MBOX_OFFSET_BASE + 1)
#define MCUPM_TS_MBOX_TS_H                 (MCUPM_TS_MBOX_OFFSET_BASE + 2)
#define MCUPM_TS_MBOX_TS_L                 (MCUPM_TS_MBOX_OFFSET_BASE + 3)
#define MCUPM_TS_MBOX_DEBUG_TS_H           (MCUPM_TS_MBOX_OFFSET_BASE + 4)
#define MCUPM_TS_MBOX_DEBUG_TS_L           (MCUPM_TS_MBOX_OFFSET_BASE + 5)

void mcupm_timesync_suspend(void);
void mcupm_timesync_resume(void);
unsigned int mcupm_timesync_init(void);
#endif // _MCUM_TIMESYNC_H_
