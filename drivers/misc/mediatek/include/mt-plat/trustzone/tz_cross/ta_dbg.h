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

/** Commands for TA Debug **/

#ifndef __TRUSTZONE_TA_DBG__
#define __TRUSTZONE_TA_DBG__

#define TZ_TA_DBG_UUID   "42a10730-f349-11e2-a99a-d4856458b228"

/* enable secure memory/chunk memory information debug */
#define MTEE_TA_DBG_ENABLE_MEMINFO

/* Command for Debug */
#define TZCMD_DBG_SECUREMEM_INFO      0
#define TZCMD_DBG_SECURECM_INFO       1

#endif				/* __TRUSTZONE_TA_DBG__ */
