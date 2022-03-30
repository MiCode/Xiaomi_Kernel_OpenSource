// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/module.h>       /* needed by all modules */
#include "vcp_feature_define.h"
#include "vcp_ipi_pin.h"
#include "vcp.h"

/*vcp feature list*/
struct vcp_feature_tb feature_table[NUM_FEATURE_ID] = {
	{
		.feature	= RTOS_FEATURE_ID,
		.freq		= 0,
		.enable	= 0,
		.sys_id	= VCPSYS_CORE0,
	},
	{
		.feature	= VDEC_FEATURE_ID,
		.freq		= 0,
		.enable	= 0,
		.sys_id	= VCPSYS_CORE0,
	},
	{
		.feature	= VENC_FEATURE_ID,
		.freq		= 0,
		.enable	= 0,
		.sys_id	= VCPSYS_CORE0,
	},
	{
		.feature	= GCE_FEATURE_ID,
		.freq		= 0,
		.enable	= 0,
		.sys_id	= VCPSYS_CORE0,
	},
};
EXPORT_SYMBOL(feature_table);
