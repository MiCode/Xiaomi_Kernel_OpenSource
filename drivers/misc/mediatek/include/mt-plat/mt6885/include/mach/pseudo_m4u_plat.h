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

#ifndef __PSEUDO_M4U_PLAT_H__
#define __PSEUDO_M4U_PLAT_H__

#define PSEUDO_LARB_NR (4)
struct pseudo_device {
	struct device *dev;	/* record the device for config mmu use. */
	int larbid;		/* the larb id of this device*/
	bool mmuen;		/* whether this larb have been configed. */
	const char *name;
	unsigned int port[32];
};

struct pseudo_device pseudo_dev_larb[SMI_LARB_NR];
struct pseudo_device pseudo_dev_larb_fake[PSEUDO_LARB_NR] = {
	{
		.larbid = CCU_PSEUDO_LARBID_DISP,
		.port = {
			M4U_PORT_L14_CAM_CCUI_DISP,
			M4U_PORT_L14_CAM_CCUO_DISP,
			M4U_PORT_L22_CCU_DISP},
		.name = "ccu_disp",
	},
	{
		.larbid = CCU_PSEUDO_LARBID_MDP,
		.port = {
			M4U_PORT_L13_CAM_CCUI_MDP,
			M4U_PORT_L13_CAM_CCUO_MDP,
			M4U_PORT_L23_CCU_MDP},
		.name = "ccu_mdp",
	},
	{
		.larbid = APU_PSEUDO_LARBID_CODE,
		.port = {
			M4U_PORT_L21_APU_FAKE_CODE},
		.name = "apu_code",
	},
	{
		.larbid = APU_PSEUDO_LARBID_DATA,
		.port = {
			M4U_PORT_L21_APU_FAKE_DATA},
		.name = "apu_data",
	},
#if (CONFIG_MTK_IOMMU_PGTABLE_EXT == 32)
	{
		.larbid = MISC_PSEUDO_LARBID_DISP,
		.port = {
			M4U_PORT_L0_DISP_FAKE0},
		.name = "misc",
	},
#endif
};

unsigned int pseudo_acp_port_array[] = {
	M4U_PORT_L21_APU_FAKE_CODE,
	M4U_PORT_L21_APU_FAKE_DATA,
};

#endif
