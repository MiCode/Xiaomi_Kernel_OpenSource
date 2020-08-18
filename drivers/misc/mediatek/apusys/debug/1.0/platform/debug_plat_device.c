// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/of_device.h>

#include "debug_plat_internal.h"

static struct debug_plat_drv mt6853_drv = {
	.dump_show = dump_show_mt6853,
	.reg_dump  = apusys_reg_dump_mt6853,

	.apusys_base          = 0x19000000,
	.apusys_reg_size      = 0x100000,
	.total_dbg_mux_count  = 28,
};

static struct debug_plat_drv mt6873_drv = {
	.dump_show = dump_show_mt6873,
	.reg_dump  = apusys_reg_dump_mt6873,

	.apusys_base          = 0x19000000,
	.apusys_reg_size      = 0x100000,
	.total_dbg_mux_count  = 22,
};

static struct debug_plat_drv mt6885_drv = {
	.dump_show = dump_show_mt6885,
	.reg_dump  = apusys_reg_dump_mt6885,

	.apusys_base          = 0x19000000,
	.apusys_reg_size      = 0x100000,
	.total_dbg_mux_count  = 38,
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
