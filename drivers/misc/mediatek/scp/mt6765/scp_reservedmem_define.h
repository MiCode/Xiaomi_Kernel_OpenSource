/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __SCP_RESERVEDMEM_DEFINE_H__
#define __SCP_RESERVEDMEM_DEFINE_H__

static struct scp_reserve_mblock scp_reserve_mblock[] = {
#ifdef CONFIG_MTK_VOW_SUPPORT
	{
		.num = VOW_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x1A000,/*104KB*/
	},
#endif
	{
		.num = SENS_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x800000,/*8MB*/
	},
#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
	{
		.num = MP3_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x400000,/*4MB*/
	},
#endif
	{
		.num = FLP_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x1000,/*4KB*/
	},
	{
		.num = SCP_A_LOGGER_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x200000,/*2MB*/
	},
#if defined(CONFIG_MTK_AUDIO_SCP_SPKPROTECT_SUPPORT)
	|| defined(CONFIG_MTK_AURISYS_PHONE_CALL_SUPPORT)
	|| defined(CONFIG_MTK_AUDIO_TUNNELING_SUPPORT)
	|| defined(CONFIG_MTK_VOW_SUPPORT)
	|| defined(CONFIG_MTK_AUDIO_SCP_SPKPROTECT_SUPPORT)
	{
		.num = AUDIO_IPI_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x200000,/*2MB*/
	},
#endif
#ifdef CONFIG_MTK_AUDIO_SCP_SPKPROTECT_SUPPORT
	{
		.num = SPK_PROTECT_MEM_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.size = 0x30000,/*192KB*/
	},
#endif
};


#endif
