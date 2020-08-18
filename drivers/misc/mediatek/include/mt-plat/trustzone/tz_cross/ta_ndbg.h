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

#ifndef __TRUSTZONE_TA_NDBG__
#define __TRUSTZONE_TA_NDBG__

#define TZ_TA_NDBG_UUID   "820b5780-dd5b-11e2-a28f-0800200c9a66"

/* Data Structure for NDBG TA
 * You should define data structure used both in REE/TEE here
 */
/* N/A for NDBG TA */
#define NDBG_BAT_ST_SIZE    16
#define URAN_SIZE           16
#define NDBG_REE_ENTROPY_SZ (NDBG_BAT_ST_SIZE + URAN_SIZE)

/* Command for DAPC TA */

#define TZCMD_NDBG_INIT           0
#define TZCMD_NDBG_WAIT_RESEED    1
#define TZCMD_NDBG_RANDOM         2

#endif	/* __TRUSTZONE_TA_NDBG__ */
