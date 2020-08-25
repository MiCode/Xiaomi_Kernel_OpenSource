// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/platform_device.h>

#include "reviser_drv.h"
#include "reviser_cmn.h"
#include "reviser_plat.h"
#include "reviser_reg_v1_0.h"
#include "reviser_hw_v1_0.h"
#include "reviser_hw_mgt.h"
#include "reviser_hw_cmn.h"

int reviser_v1_0_init(struct platform_device *pdev)
{
	struct reviser_dev_info *rdv = platform_get_drvdata(pdev);
	struct reviser_hw_ops *cb;

	cb = (struct reviser_hw_ops *) reviser_mgt_get_cb();

	cb->dmp_boundary = reviser_print_boundary;
	cb->dmp_ctx = reviser_print_context_ID;
	cb->dmp_rmp = reviser_print_remap_table;
	cb->dmp_default = reviser_print_default_iova;
	cb->dmp_exception = reviser_print_exception;



	cb->set_boundary = reviser_boundary_init;
	cb->set_default = reviser_set_default_iova;
	cb->set_ctx = reviser_set_context_ID;
	cb->set_rmp = reviser_set_remap_table;

	cb->isr_cb = reviser_isr;
	cb->set_int = reviser_enable_interrupt;

	rdv->plat.hw_ver = 0x100;

	return 0;
}
int reviser_v1_0_uninit(struct platform_device *pdev)
{
	return 0;
}
