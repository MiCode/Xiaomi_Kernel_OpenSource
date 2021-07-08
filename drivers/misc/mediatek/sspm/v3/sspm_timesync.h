/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _SSPM_TIMESYNC_H_
#define _SSPM_TIMESYNC_H_

#include "sspm_helper.h"

#define MBOX_SLOT_SZ                 4
#define MBOX_NOMCDI_OFF              20
#define SSPM_TS_MBOX_OFFSET_BASE     (sspmreg.mboxshare + (MBOX_NOMCDI_OFF * MBOX_SLOT_SZ))

/*
 * Shared MBOX: AP write, SSPM read
 * Unit for each offset: 4 bytes
 */

#define SSPM_TS_MBOX_TICK_H          (SSPM_TS_MBOX_OFFSET_BASE + 0)
#define SSPM_TS_MBOX_TICK_L          (SSPM_TS_MBOX_OFFSET_BASE + 4)
#define SSPM_TS_MBOX_TS_H            (SSPM_TS_MBOX_OFFSET_BASE + 8)
#define SSPM_TS_MBOX_TS_L            (SSPM_TS_MBOX_OFFSET_BASE + 12)
#define SSPM_TS_MBOX_DEBUG_TS_H      (SSPM_TS_MBOX_OFFSET_BASE + 14)
#define SSPM_TS_MBOX_DEBUG_TS_L      (SSPM_TS_MBOX_OFFSET_BASE + 16)

void sspm_timesync_suspend(void);
void sspm_timesync_resume(void);
unsigned int __init sspm_timesync_init(void);
#endif // _SSPM_TIMESYNC_H_
