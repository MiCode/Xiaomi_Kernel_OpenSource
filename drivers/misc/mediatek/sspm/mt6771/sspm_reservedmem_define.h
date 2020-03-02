/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef _SSPM_RESERVEDMEM_DEFINE_H_
#define _SSPM_RESERVEDMEM_DEFINE_H_
#include <v1/sspm_reservedmem.h>

enum {
	SSPM_MEM_ID = 0,
	PWRAP_MEM_ID,
	PMIC_MEM_ID,
	UPD_MEM_ID,
	QOS_MEM_ID,
	SWPM_MEM_ID,
#if defined(CONFIG_MTK_GMO_RAM_OPTIMIZE) || defined(CONFIG_MTK_MET_MEM_ALLOC)
#else
	MET_MEM_ID,
#endif
	NUMS_MEM_ID,
};

#define SSPM_PLT_LOGGER_BUF_LEN 0x100000

#ifdef _SSPM_INTERNAL_
/* The total size of sspm_reserve_mblock should less equal than
 * reserve-memory-sspm_share of device tree
 */
static struct sspm_reserve_mblock sspm_reserve_mblock[NUMS_MEM_ID] = {
	{
		.num = SSPM_MEM_ID,
		.size = 0x400 + SSPM_PLT_LOGGER_BUF_LEN,
		/* logger header + timesync header + 1M log buffer */
	},
	{
		.num = PWRAP_MEM_ID,
		.size = 0x300,  /* 768 bytes */
	},
	{
		.num = PMIC_MEM_ID,
		.size = 0xC00,  /* 3K */
	},
	{
		.num = UPD_MEM_ID,
		.size = 0x1800, /* 6K */
	},
	{
		.num = QOS_MEM_ID,
		.size = 0x1000, /* 4K */
	},
	{
		.num = SWPM_MEM_ID,
		.size = 0xC00,  /* 3K */
	},
#if defined(CONFIG_MTK_GMO_RAM_OPTIMIZE) || defined(CONFIG_MTK_MET_MEM_ALLOC)
#else
	{
		.num = MET_MEM_ID,
		.size = 0x400000, /* 4M */
	},
#endif

	/* TO align 64K, total is 1M+64K. The remaining size = 0xB900 */
};
#endif
#endif
