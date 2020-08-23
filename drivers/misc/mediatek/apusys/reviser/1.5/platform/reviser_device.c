// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/of_device.h>
#include "reviser_cmn.h"
#include "reviser_plat.h"
#include "reviser_device.h"


static struct reviser_plat mt6885_drv = {
	.init					= reviser_v1_0_init,
	.uninit					= reviser_v1_0_uninit,

	.bank_size				= 0x40000,
	.rmp_max				= 0xD,
	.ctx_max				= 32,
	.mdla_max				= 2,
	.vpu_max				= 3,
	.edma_max				= 2,
	.up_max					= 1,
};

static struct reviser_plat mt6873_drv = {
	.init					= reviser_v1_0_init,
	.uninit					= reviser_v1_0_uninit,

	.bank_size				= 0x40000,
	.rmp_max				= 0xD,
	.ctx_max				= 32,
	.mdla_max				= 1,
	.vpu_max				= 2,
	.edma_max				= 1,
	.up_max					= 1,
};

static const struct of_device_id reviser_of_match[] = {
	{ .compatible = "mediatek, mt6885-reviser", .data = &mt6885_drv},
	{ .compatible = "mediatek, mt6873-reviser", .data = &mt6873_drv},
	{/* end of list */},
};

const struct of_device_id *reviser_get_of_device_id(void)
{
	return reviser_of_match;
}

