/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __SMI_HW_H__
#define __SMI_HW_H__

#include <clk-mt6768-pg.h>
#include <smi_port.h>

static const u32 smi_subsys_to_larbs[NR_SYSS] = {
	[SYS_DIS] = ((1 << 0) | (1 << (SMI_LARB_NUM))),
	[SYS_VDEC] = (1 << 1),
	[SYS_ISP] = (1 << 2),
	[SYS_CAM] = (1 << 3),
	[SYS_VENC] = (1 << 4),
};

#if IS_ENABLED(CONFIG_MMPROFILE)
#include <mmprofile_function.h>

static const char *smi_mmp_name[NR_SYSS] = {
	[SYS_DIS] = "DIS", [SYS_VDEC] = "VDE",
	[SYS_ISP] = "ISP", [SYS_CAM] = "CAM", [SYS_VENC] = "VEN",
};
static mmp_event smi_mmp_event[NR_SYSS];
#endif
#endif
