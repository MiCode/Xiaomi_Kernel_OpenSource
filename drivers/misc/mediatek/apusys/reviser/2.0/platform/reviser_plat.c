// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include "reviser_mem_def.h"
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
		return -EINVAL;
	}

	rplat = (struct reviser_plat *)of_device_get_match_data(dev);
	if (!rplat) {
		LOG_ERR("No reviser_plat!\n");
		return -EINVAL;
	}


	LOG_DBG_RVR_FLW("=============================\n");
	LOG_DBG_RVR_FLW(" reviser platform info\n");
	LOG_DBG_RVR_FLW("-----------------------------\n");
	LOG_DBG_RVR_FLW("bank_size: 0x%x\n", rplat->bank_size);
	LOG_DBG_RVR_FLW("mdla_max: 0x%x\n", rplat->mdla_max);
	LOG_DBG_RVR_FLW("vpu_max: 0x%x\n", rplat->vpu_max);
	LOG_DBG_RVR_FLW("edma_max: 0x%x\n", rplat->edma_max);
	LOG_DBG_RVR_FLW("up_max: 0x%x\n", rplat->up_max);

	LOG_DBG_RVR_FLW("=============================\n");


	rdv->plat.bank_size = rplat->bank_size;

	memset(rdv->plat.device, 0, sizeof(rdv->plat.device[REVISER_DEVICE_MAX]));
	rdv->plat.device[REVISER_DEVICE_MDLA] = rplat->mdla_max;
	rdv->plat.device[REVISER_DEVICE_VPU] = rplat->vpu_max;
	rdv->plat.device[REVISER_DEVICE_EDMA] = rplat->edma_max;
	rdv->plat.device[REVISER_DEVICE_SECURE_MD32] = rplat->up_max;

	return rplat->init(pdev);
}
int reviser_plat_uninit(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct reviser_plat *rplat;
	struct reviser_dev_info *rdv = platform_get_drvdata(pdev);

	if (!rdv) {
		LOG_ERR("No reviser_dev_info!\n");
		return -EINVAL;
	}

	rplat = (struct reviser_plat *)of_device_get_match_data(dev);
	if (!rplat) {
		LOG_ERR("No reviser_plat!\n");
		return -EINVAL;
	}

	return rplat->uninit(pdev);
}
