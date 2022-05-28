// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/module.h>

#include "mtk_cam-debug_option.h"

static unsigned int debug_opts;
module_param(debug_opts, uint, 0644);
MODULE_PARM_DESC(debug_opts, "debug options");

unsigned int cam_debug_opts(void)
{
	return debug_opts;
}
