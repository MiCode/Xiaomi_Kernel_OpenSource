/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
