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

#ifndef __TRUSTZONE_TA_VDEC__
#define __TRUSTZONE_TA_VDEC__

#define TZ_TA_VDEC_UUID   "ff33a6e0-8635-11e2-9e96-0800200c9a66"

#define UT_ENABLE 0
#define DONT_USE_BS_VA 1	/* for VP path integration set to 1, */
				/* for mfv_ut set to 0 */
#define USE_MTEE_M4U
#define USE_MTEE_DAPC


/* Command for VDEC TA */
#define TZCMD_VDEC_AVC_INIT       0
#define TZCMD_VDEC_AVC_DECODE     1
#define TZCMD_VDEC_AVC_DEINIT      2

#define TZCMD_VDEC_TEST        100
#endif				/* __TRUSTZONE_TA_VDEC__ */
