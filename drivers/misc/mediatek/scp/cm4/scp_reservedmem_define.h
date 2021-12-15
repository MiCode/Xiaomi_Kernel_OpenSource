/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __SCP_RESERVEDMEM_DEFINE_H__
#define __SCP_RESERVEDMEM_DEFINE_H__

static struct scp_reserve_mblock scp_reserve_mblock[] = {
	{
		.num = VOW_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x0,
	},
	{
		.num = SENS_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x0,
	},
	{
		.num = MP3_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x0,
	},
	{
		.num = FLP_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x0,
	},
	{
		.num = SCP_A_LOGGER_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x0,
	},
	{
		.num = AUDIO_IPI_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x0,
	},
	{
		.num = SPK_PROTECT_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x0,
	},
	{
		.num = SPK_PROTECT_DUMP_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x0,
	},
	{
		.num = VOW_BARGEIN_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x0,
	},
	{
		.num = SCP_DRV_PARAMS_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x0,
	},
	{
		.num = ULTRA_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x0,
	},
};


#endif
