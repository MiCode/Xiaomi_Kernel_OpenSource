/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __SMI_HW_H__
#define __SMI_HW_H__

#include <clk-mt6779-pg.h>
#include <smi_port.h>

static const u32 smi_subsys_to_larbs[NR_SYSS] = {

	[SYS_DIS] = (1 << 9) | (1 << 14),
	[SYS_VDE] = (1 << 12),
	[SYS_VEN] = (1 << 19),
	[SYS_ISP] = (1 << 26),
	[SYS_IPU] = (1 << 3) | (1 << 5),
	[SYS_IPE] = (1 << 4) | (1 << 10),
	[SYS_CAM] = (1 << 24) | (1 << 31),

};


#if IS_ENABLED(CONFIG_MMPROFILE)
#include <mmprofile.h>

static const char *smi_mmp_name[NR_SYSS] = {
	[SYS_DIS] = "DIS",
	[SYS_VDE] = "VDE",
	[SYS_VEN] = "VEN",
	[SYS_ISP] = "ISP",
	[SYS_IPU] = "IPU",
	[SYS_IPE] = "IPE",
	[SYS_CAM] = "CAM",
};
static mmp_event smi_mmp_event[NR_SYSS];
#endif
#endif
