// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include "reviser_cmn.h"
#include "reviser_plat.h"
#include "reviser_drv.h"


int reviser_plat_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct reviser_plat *rplat;
	struct reviser_dev_info *rdv = platform_get_drvdata(pdev);

	if (!rdv) {
		LOG_ERR("No reviser_dev_info!\n");
		return -1;
	}

	rplat = (struct reviser_plat *)of_device_get_match_data(dev);
	if (!rplat) {
		LOG_ERR("No reviser_plat!\n");
		return -1;
	}


	LOG_DEBUG("=============================\n");
	LOG_DEBUG(" reviser platform info\n");
	LOG_DEBUG("-----------------------------\n");
	LOG_DEBUG("bank_size: 0x%x\n", rplat->bank_size);
	LOG_DEBUG("rmp_max: 0x%x\n", rplat->rmp_max);
	LOG_DEBUG("ctx_max: 0x%x\n", rplat->ctx_max);
	LOG_DEBUG("mdla_max: 0x%x\n", rplat->mdla_max);
	LOG_DEBUG("vpu_max: 0x%x\n", rplat->vpu_max);
	LOG_DEBUG("edma_max: 0x%x\n", rplat->edma_max);
	LOG_DEBUG("up_max: 0x%x\n", rplat->up_max);

	LOG_DEBUG("=============================\n");


	rdv->plat.bank_size = rplat->bank_size;
	rdv->plat.rmp_max = rplat->rmp_max;
	rdv->plat.ctx_max = rplat->ctx_max;
	rdv->plat.mdla_max = rplat->mdla_max;
	rdv->plat.vpu_max = rplat->vpu_max;
	rdv->plat.edma_max = rplat->edma_max;
	rdv->plat.up_max = rplat->up_max;

	return rplat->init(pdev);
}
int reviser_plat_uninit(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct reviser_plat *rplat;
	struct reviser_dev_info *rdv = platform_get_drvdata(pdev);

	if (!rdv) {
		LOG_ERR("No reviser_dev_info!\n");
		return -1;
	}

	rplat = (struct reviser_plat *)of_device_get_match_data(dev);
	if (!rplat) {
		LOG_ERR("No reviser_plat!\n");
		return -1;
	}

	return rplat->uninit(pdev);
}
