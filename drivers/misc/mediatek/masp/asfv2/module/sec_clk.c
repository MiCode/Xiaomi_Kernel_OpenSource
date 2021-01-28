// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011 MediaTek Inc.
 */

/******************************************************************************
 *  INCLUDE LINUX HEADER
 ******************************************************************************/
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/clk.h>
#include <linux/of_platform.h>
#include "sec_clk.h"

int sec_clk_enable(struct platform_device *dev)
{
	pr_debug("[sec] Need not to get hacc clock\n");
	return 0;
}

