/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

/* Power management TA functions
 */

#ifndef __TRUSTZONE_TA_PM__
#define __TRUSTZONE_TA_PM__

#define TZ_TA_PM_UUID   "387389fa-b2cf-11e2-856d-d485645c4310"

/* Command for PM TA */
#define TZCMD_PM_CPU_LOWPOWER     0
#define TZCMD_PM_CPU_DORMANT      1
#define TZCMD_PM_DEVICE_OPS       2
#define TZCMD_PM_CPU_ERRATA_802022_WA    3

enum eMTEE_PM_State {
	MTEE_NONE,
	MTEE_SUSPEND,
	MTEE_SUSPEND_LATE,
	MTEE_RESUME,
	MTEE_RESUME_EARLY,
};

#endif				/* __TRUSTZONE_TA_PM__ */
