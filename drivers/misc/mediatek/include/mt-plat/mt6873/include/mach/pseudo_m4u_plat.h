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

struct pseudo_device {
	struct device *dev;	/* record the device for config mmu use. */
	int larbid;		/* the larb id of this device*/
	bool mmuen;		/* whether this larb have been configed. */
	const char *name;
	int port[32];
};

struct pseudo_device pseudo_dev_larb[SMI_LARB_NR];
struct pseudo_device pseudo_dev_larb_fake[] = {
	{
		.larbid = MISC_PSEUDO_LARBID,
		.port = {
			M4U_PORT_L0_DISP_FAKE0, -1},
		.name = "misc",
	},
	{
		.larbid = CCU0_PSEUDO_LARBID,
		.port = {
			M4U_PORT_L13_CAM_CCUI,
			M4U_PORT_L13_CAM_CCUO,
			M4U_PORT_L22_CCU0, -1},
		.name = "ccu0",
	},
	{
		.larbid = CCU1_PSEUDO_LARBID,
		.port = {
			M4U_PORT_L14_CAM_CCUI,
			M4U_PORT_L14_CAM_CCUO,
			M4U_PORT_L23_CCU1, -1},
		.name = "ccu1",
	},
	{
		.larbid = APU_PSEUDO_LARBID_CODE,
		.port = {
			M4U_PORT_L21_APU_FAKE_CODE, -1},
		.name = "apu_code",
	},
	{
		.larbid = APU_PSEUDO_LARBID_DATA,
		.port = {
			M4U_PORT_L21_APU_FAKE_DATA, -1},
		.name = "apu_data",
	},
	{
		.larbid = APU_PSEUDO_LARBID_VLM,
		.port = {
			M4U_PORT_L21_APU_FAKE_VLM, -1},
		.name = "apu_vlm",
	},
};

unsigned int pseudo_acp_port_array[] = {
	M4U_PORT_L21_APU_FAKE_CODE,
	M4U_PORT_L21_APU_FAKE_DATA,
	M4U_PORT_L21_APU_FAKE_VLM,
};

static char *pseudo_larbname[] = {
	"mediatek,smi_larb0", "mediatek,smi_larb1", "mediatek,smi_larb2",
	"m4u_none", "mediatek,smi_larb4", "mediatek,smi_larb5",
	"m4u_none", "mediatek,smi_larb7", "m4u_none",
	"mediatek,smi_larb9", "m4u_none", "mediatek,smi_larb11",
	"m4u_none", "mediatek,smi_larb13", "mediatek,smi_larb14",
	"m4u_none", "mediatek,smi_larb16", "mediatek,smi_larb17",
	"mediatek,smi_larb18", "mediatek,smi_larb19", "mediatek,smi_larb20"
};
char *pseudo_larb_clk_name[] = {
	"m4u_smi_larb0", "m4u_smi_larb1", "m4u_smi_larb2", "m4u_none",
	"m4u_smi_larb4", "m4u_smi_larb5", "m4u_none", "m4u_smi_larb7",
	"m4u_none", "m4u_smi_larb9", "m4u_none", "m4u_smi_larb11",
	"m4u_none", "m4u_smi_larb13", "m4u_smi_larb14", "m4u_none",
	"m4u_smi_larb16", "m4u_smi_larb17", "m4u_smi_larb18", "m4u_smi_larb19",
	"m4u_smi_larb20"
};

static inline int  m4u_port_id_of_mdp(
		unsigned int larb, unsigned int port)
{
	if (larb == 2 && port <= 4)
		return 1;
	else
		return 0;
}
#endif
