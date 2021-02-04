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


#ifndef __TRUSTZONE_TA_GCPU__
#define __TRUSTZONE_TA_GCPU__

#define TZ_TA_GCPU_UUID   "7b6c51b8-2994-4a32-be68-d840df0281bb"

/* Data Structure for GCPU TA
 * You should define data structure used both in REE/TEE here
 */
/* N/A for GCPU TA */

/* Command for GCPU TA */
#define TZCMD_GCPU_SELFTEST                 0
#define TZCMD_GCPU_SUSPEND                  1
#define TZCMD_GCPU_KERNEL_INIT_DONE         2
#define TZCMD_GCPU_AES_CBC_PERFORMANCE      3
#define TZCMD_GCPU_RESUME                   4

#endif	/* __TRUSTZONE_TA_GCPU__ */
