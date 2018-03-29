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

#ifndef __MT_PARTITIONINFO_H__
#define __MT_PARTITIONINFO_H__

/* #define PT_ABTC_ATAG */
/* #define ATAG_OTP_INFO       0x54430004 */
/* #define ATAG_BMT_INFO       0x54430005 */

struct tag_pt_info {
	unsigned long long size;	/* partition size */
	unsigned long long start_address;	/* partition start */
};
#endif
