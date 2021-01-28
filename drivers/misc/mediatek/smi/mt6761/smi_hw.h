/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __SMI_HW_H__
#define __SMI_HW_H__

#include <clk-mt6761-pg.h>
#include <smi_port.h>

static const u32 smi_subsys_to_larbs[NR_SYSS] = {

	[SYS_DIS] = (1 << 3),
	[SYS_CAM] = (1 << 8),
	[SYS_VCODEC] = (1 << 9),

};

#if IS_ENABLED(CONFIG_MMPROFILE)
#include <mmprofile.h>

static const char *smi_mmp_name[NR_SYSS] = {
	[SYS_DIS] = "DIS",
	[SYS_CAM] = "CAM",
	[SYS_VCODEC] = "VCODEC",
};
static mmp_event smi_mmp_event[NR_SYSS];
#endif
#endif
