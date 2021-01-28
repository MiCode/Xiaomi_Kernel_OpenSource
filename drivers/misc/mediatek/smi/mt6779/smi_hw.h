/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __SMI_HW_H__
#define __SMI_HW_H__

#include <clk-mt6779-pg.h>
#include <smi_port.h>

static const u32 smi_subsys_to_larbs[NR_SYSS] = {

	[SYS_DIS] = (1 << 0) | (1 << 1) | (1 << 12),
	[SYS_VDE] = (1 << 2),
	[SYS_VEN] = (1 << 3),
	[SYS_ISP] = (1 << 5) | (1 << 6),
	[SYS_IPE] = (1 << 7) | (1 << 8),
	[SYS_CAM] = (1 << 9) | (1 << 10) | (1 << 11),

};


#if IS_ENABLED(CONFIG_MMPROFILE)
#include <mmprofile.h>

static const char *smi_mmp_name[NR_SYSS] = {
	[SYS_DIS] = "DIS",
	[SYS_VDE] = "VDE",
	[SYS_VEN] = "VEN",
	[SYS_ISP] = "ISP",
	[SYS_IPE] = "IPE",
	[SYS_CAM] = "CAM",
};
static mmp_event smi_mmp_event[NR_SYSS];
#endif
#endif
