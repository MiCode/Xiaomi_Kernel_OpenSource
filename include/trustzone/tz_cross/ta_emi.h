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

#ifndef __TRUSTZONE_TA_EMI__
#define __TRUSTZONE_TA_EMI__

#define TZ_TA_EMI_UUID   "f80dab1a-a33f-4a48-a015-a16845d351f3"


/* Data Structure for EMI TA */
/* You should define data structure used both in REE/TEE here
   N/A for Test TA */


/* Command for EMI TA */

#define TZCMD_EMI_REG    0
#define TZCMD_EMI_CLR    1
#define TZCMD_EMI_RD     2
#define TZCMD_EMI_WR     3


#endif				/* __TRUSTZONE_TA_EMI__ */
