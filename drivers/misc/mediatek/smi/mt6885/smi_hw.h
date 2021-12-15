/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __SMI_HW_H__
#define __SMI_HW_H__

#include <clk-mt6885-pg.h>
#include <smi_port.h>

static const u32 smi_subsys_to_larbs[NR_SYSS] = {
	[SYS_ISP] = (1 << 9),
	[SYS_ISP2] = (1 << 11),
	[SYS_IPE] = (1 << 19) | (1 << 20) | (1 << 28),
	[SYS_VDE] = (1 << 5),
	[SYS_VDE2] = (1 << 4),
	[SYS_VEN] = (1 << 7),
	[SYS_VEN_CORE1] = (1 << 8),
	[SYS_MDP] = (1 << 2) | (1 << 3) |
		(1 << 22) | (1 << 23) | (1 << 26) | (1 << 27),
	[SYS_DIS] = (1 << 0) | (1 << 1) | (1 << 21) | (1 << 24) | (1 << 25),
	[SYS_CAM] = (1 << 13) | (1 << 14) | (1 << 15) |
		(1 << 29) | (1 << 30) | (1 << 31),
	[SYS_CAM_RAWA] = (1 << 16),
	[SYS_CAM_RAWB] = (1 << 17),
	[SYS_CAM_RAWC] = (1 << 18),
};

#if IS_ENABLED(CONFIG_MMPROFILE)
#include <mmprofile.h>

static const char *smi_mmp_name[NR_SYSS] = {
	[SYS_ISP] = "ISP",
	[SYS_ISP2] = "ISP2",
	[SYS_IPE] = "IPE",
	[SYS_VDE] = "VDE",
	[SYS_VDE2] = "VDE2",
	[SYS_VEN] = "VEN",
	[SYS_VEN_CORE1] = "VEN_CORE1",
	[SYS_MDP] = "MDP",
	[SYS_DIS] = "DIS",
	[SYS_CAM] = "CAM",
	[SYS_CAM_RAWA] = "CAM_RAWA",
	[SYS_CAM_RAWB] = "CAM_RAWB",
	[SYS_CAM_RAWC] = "CAM_RAWC",
};
static mmp_event smi_mmp_event[NR_SYSS];
#endif
#endif
