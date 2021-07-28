// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/platform_device.h>

#include "reviser_mem_def.h"
#include "reviser_drv.h"
#include "reviser_cmn.h"
#include "reviser_plat.h"

#include "reviser_hw_vrv.h"
#include "reviser_hw_mgt.h"
#include "reviser_hw_cmn.h"

int reviser_vrv_init(struct platform_device *pdev)
{
	struct reviser_dev_info *rdv = platform_get_drvdata(pdev);
	struct reviser_hw_ops *hw_cb;

	hw_cb = (struct reviser_hw_ops *) reviser_hw_mgt_get_cb();

	hw_cb->dmp_boundary = reviser_print_rvr_boundary;
	hw_cb->dmp_ctx = reviser_print_rvr_context_ID;
	hw_cb->dmp_rmp = reviser_print_rvr_remap_table;
	hw_cb->dmp_default = reviser_print_rvr_default_iova;
	hw_cb->dmp_exception = reviser_print_rvr_exception;


	//Set TCM Info
	rdv->plat.pool_type[REVSIER_POOL_TCM] = REVISER_MEM_TYPE_TCM;

	return 0;
}
int reviser_vrv_uninit(struct platform_device *pdev)
{
	return 0;
}
