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

#ifndef __PICACHU_RESERVED_H__
#define __PICACHU_RESERVED_H__

#include <linux/types.h>


struct picachu_reserve_mblock {
	u32 num;
	u64 start_phys;
	u64 start_virt;
	u64 size;
};

enum {
	PICACHU_PTP3_ID = 0,
	PICACHU_LOG_ID,
	PICACHU_BACKUP_LOG_ID,
	PICACHU_EEM_ID,
	PICACHU_AEE_ID,

	NUMS_MEM_ID,
};


static struct picachu_reserve_mblock picachu_reserve_mblock[NUMS_MEM_ID] = {
	{
		.num = PICACHU_PTP3_ID,
		.size = 0x40000,
	},
	{
		.num = PICACHU_LOG_ID,
		.size = 0x10000,  /* 64K */
	},
	{
		.num = PICACHU_BACKUP_LOG_ID,
		.size = 0x10000,  /* 64K bytes*/
	},
	{
		.num = PICACHU_EEM_ID,
		.size = 0x10000,  /* 64K bytes */
	},
	{
		.num = PICACHU_AEE_ID,
		.size = 0x10000,  /* 64K bytes */
	},
};
#endif

