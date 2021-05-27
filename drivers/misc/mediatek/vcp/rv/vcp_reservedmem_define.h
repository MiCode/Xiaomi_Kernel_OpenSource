/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __VCP_RESERVEDMEM_DEFINE_H__
#define __VCP_RESERVEDMEM_DEFINE_H__

static struct vcp_reserve_mblock vcp_reserve_mblock[] = {
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
		.num = VCP_A_LOGGER_MEM_ID,
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
		.num = VOW_BARGEIN_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x0,
	},
	{
		.num = VCP_DRV_PARAMS_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x0,
	},
	{
		.num = ULTRA_MEM_ID,
		.start_phys = 0,
		.start_virt = 0,
		.size = 0x0,
	},
	{
		.num = SENS_SUPER_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x0,
	},
	{
		.num = SENS_LIST_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x0,
	},
	{
		.num = SENS_DEBUG_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x0,
	},
};


#endif

