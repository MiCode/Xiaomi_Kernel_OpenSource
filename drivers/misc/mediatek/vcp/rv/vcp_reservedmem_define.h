/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __VCP_RESERVEDMEM_DEFINE_H__
#define __VCP_RESERVEDMEM_DEFINE_H__

static struct vcp_reserve_mblock vcp_reserve_mblock[] = {
	{
		.num = VDEC_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x0,
	},
	{
		.num = VENC_MEM_ID,
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
		.num = VDEC_SET_PROP_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x0,
	},
	{
		.num = VENC_SET_PROP_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x0,
	},
	{
		.num = VDEC_VCP_LOG_INFO_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x0,
	},
	{
		.num = VENC_VCP_LOG_INFO_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x0,
	},
	{
		.num = GCE_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x0,
	},

};


#endif

