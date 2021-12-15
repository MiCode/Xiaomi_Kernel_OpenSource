/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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

