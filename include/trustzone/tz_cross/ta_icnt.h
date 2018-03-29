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

/* An example test TA implementation.
 */

#ifndef __TRUSTZONE_TA_INC_CNT__
#define __TRUSTZONE_TA_INC_CNT__

#define TZ_TA_ICNT_UUID   "5bc52d1c-a07b-4373-8cab-d4db3e9eea5c"

/* Data Structure for INC-ONLY CNT TA */
/* You should define data structure used both in REE/TEE here
   N/A for Test TA */

/* Command for INC-ONLY CNT TA */
#define TZCMD_ICNT_COUNT        0
#define TZCMD_ICNT_RATE         1


#endif				/* __TRUSTZONE_TA_INC_CNT__ */
