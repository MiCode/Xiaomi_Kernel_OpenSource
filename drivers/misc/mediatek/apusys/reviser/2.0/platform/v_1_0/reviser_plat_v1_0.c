// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/platform_device.h>

#include "reviser_mem_def.h"
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
	struct reviser_hw_ops *hw_cb;

	hw_cb = (struct reviser_hw_ops *) reviser_hw_mgt_get_cb();

	hw_cb->dmp_boundary = reviser_print_boundary;
	hw_cb->dmp_ctx = reviser_print_context_ID;
	hw_cb->dmp_rmp = reviser_print_remap_table;
	hw_cb->dmp_default = reviser_print_default_iova;
	hw_cb->dmp_exception = reviser_print_exception;



	hw_cb->set_boundary = reviser_boundary_init;
	hw_cb->set_default = reviser_set_default_iova;
	hw_cb->set_ctx = reviser_set_context_ID;
	hw_cb->set_rmp = reviser_set_remap_table;

	hw_cb->isr_cb = reviser_isr;
	hw_cb->set_int = reviser_enable_interrupt;

	//Set TCM Info
	rdv->plat.pool_type[REVSIER_POOL_TCM] = REVISER_MEM_TYPE_TCM;
	rdv->plat.pool_base[REVSIER_POOL_TCM] = 0;
	rdv->plat.pool_step[REVSIER_POOL_TCM] = 1;
	//Set DRAM fallback
	rdv->plat.dram[0] = REMAP_DRAM_BASE;

	//Set remap max
	rdv->plat.rmp_max = VLM_REMAP_TABLE_MAX;
	//Set ctx max
	rdv->plat.ctx_max = VLM_CTXT_CTX_ID_MAX;

	rdv->plat.hw_ver = 0x100;

	return 0;
}
int reviser_v1_0_uninit(struct platform_device *pdev)
{
	return 0;
}
