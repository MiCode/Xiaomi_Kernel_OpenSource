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
		.larbid = MISC_PSEUDO_LARBID_DISP,
		.port = {
			M4U_PORT_DISP_FAKE0, -1},
		.name = "misc",
	},
	{
		.larbid = M4U_LARB_CCU,
		.port = {
			M4U_PORT_CCU0,
			M4U_PORT_CCU1, -1},
		.name = "ccu_disp",
	},
	{
		.larbid = M4U_LARB_APU,
		.port = {
			M4U_PORT_VPU, -1},
		.name = "apu_code",
	},
	{
		.larbid = APU_PSEUDO_LARBID_DATA,
		.port = {
			M4U_PORT_VPU_DATA, -1},
		.name = "apu_data",
	},
};

static char *pseudo_larbname[] = {
	"mediatek,smi_larb0", "mediatek,smi_larb1", "mediatek,smi_larb2",
	"mediatek,smi_larb3", "m4u_none", "mediatek,smi_larb5",
	"mediatek,smi_larb6",
};
char *pseudo_larb_clk_name[] = {
	"m4u_smi_larb0", "m4u_smi_larb1", "m4u_smi_larb2", "m4u_smi_larb3",
	"m4u_none", "m4u_smi_larb5", "m4u_smi_larb6",
};

unsigned int pseudo_acp_port_array[] = {M4U_PORT_APU};

static inline int  m4u_port_id_of_mdp(
		unsigned int larb, unsigned int port)
{
	if (larb == 1 &&
	    port >= 5 &&
	    port <= 12)
		return 1;
	else
		return 0;
}
#endif
