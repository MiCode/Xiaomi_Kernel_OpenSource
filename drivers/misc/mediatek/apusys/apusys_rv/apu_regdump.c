// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/platform_device.h>

#include "apusys_secure.h"
#include "apu_regdump.h"

static struct platform_device *g_apu_pdev;
static struct mutex regdump_lock;

static uint32_t apusys_rv_smc_call(struct device *dev, uint32_t smc_id,
	uint32_t a2)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_APUSYS_CONTROL, smc_id,
				a2, 0, 0, 0, 0, 0, &res);
	if (((int) res.a0) < 0)
		dev_info(dev, "%s: smc call %d return error(%d)\n",
			__func__,
			smc_id, res.a0);

	return res.a0;
}

void apu_regdump(void)
{
	dev_info(&g_apu_pdev->dev, "%s\n", __func__);

	mutex_lock(&regdump_lock);

	apusys_rv_smc_call(&g_apu_pdev->dev,
		MTK_APUSYS_KERNEL_OP_APUSYS_REGDUMP, 0);

	mutex_unlock(&regdump_lock);
}

void apu_regdump_init(struct platform_device *pdev)
{
	g_apu_pdev = pdev;
	mutex_init(&regdump_lock);
}
