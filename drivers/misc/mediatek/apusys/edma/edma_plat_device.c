// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/of_device.h>

#include "edma_cmd_hnd.h"
#include "edma_plat_internal.h"


static struct edma_plat_drv edma_v20_drv = {
	.exe_sub           = edma_exe_v20,
	.prt_error		   = print_error_status,
	.edma_isr         = edma_isr_handler,
	.cmd_timeout_ms     = 3000,
	.delay_power_off_ms   = 2000,
};


static struct edma_plat_drv edma_v30_drv = {
	.exe_sub           = edma_exe_v30,
	.prt_error		   = printV30_error_status,
	.edma_isr         = edmaV30_isr_handler,
	.cmd_timeout_ms     = 3000,
	.delay_power_off_ms   = 2000,
};



static const struct of_device_id mtk_edma_sub_of_ids[] = {
	{.compatible = "mtk,edma-sub", .data = &edma_v20_drv},
	{.compatible = "mtk,edma-sub-v20", .data = &edma_v20_drv},
	{.compatible = "mtk,edma-sub-v30", .data = &edma_v30_drv},
	{}
};

MODULE_DEVICE_TABLE(of, mtk_edma_sub_of_ids);

const struct of_device_id *edma_plat_get_device(void)
{
	return mtk_edma_sub_of_ids;
}
