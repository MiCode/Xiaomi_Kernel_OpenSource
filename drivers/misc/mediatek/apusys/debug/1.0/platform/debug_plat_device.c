// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/of_device.h>

#include "debug_plat_internal.h"
#include "debug_platform.h"

static struct debug_plat_drv mt6853_drv = {
	.platform_idx = 0,
	.apusys_base			= APUSYS_BASE,
	.apusys_reg_size		= APUSYS_REG_SIZE,
	.total_dbg_mux_count	= TOTAL_MUX_COUNT_MT6853,
};

static struct debug_plat_drv mt6873_drv = {
	.platform_idx = 1,
	.apusys_base			= APUSYS_BASE,
	.apusys_reg_size		= APUSYS_REG_SIZE,
	.total_dbg_mux_count	= TOTAL_MUX_COUNT_MT6873,
};

static struct debug_plat_drv mt6885_drv = {
	.platform_idx = 2,
	.apusys_base			= APUSYS_BASE,
	.apusys_reg_size		= APUSYS_REG_SIZE,
	.total_dbg_mux_count	= TOTAL_MUX_COUNT_MT6885,
};

static const struct of_device_id debug_of_match[] = {
	{ .compatible = "mediatek,mt6853-debug", .data = &mt6853_drv},
	{ .compatible = "mediatek,mt6873-debug", .data = &mt6873_drv},
	{ .compatible = "mediatek,mt6885-debug", .data = &mt6885_drv},
	{ /* end of list */},
};
MODULE_DEVICE_TABLE(of, debug_of_match);

const struct of_device_id *debug_plat_get_device(void)
{
	return debug_of_match;
}
