// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Description: CoreSight TMC Ethernet driver
 */

#include <linux/of_address.h>
#include "coresight-common.h"
#include "coresight-tmc.h"

static bool tmc_etr_support_eth_mode(struct device *dev)
{
	return fwnode_property_present(dev->fwnode, "qcom,eth_support");
}

int tmc_eth_enable(struct tmc_eth_data *eth_data)
{
	int rc;
	u32 axictl;
	struct tmc_drvdata *tmcdrvdata = eth_data->tmcdrvdata;

	CS_UNLOCK(tmcdrvdata->base);
	rc = tmc_wait_for_tmcready(tmcdrvdata);
	if (rc) {
		CS_LOCK(tmcdrvdata->base);
		return rc;
	}

	writel_relaxed(ETR_ETH_SIZE / 4, tmcdrvdata->base + TMC_RSZ);
	writel_relaxed(TMC_MODE_CIRCULAR_BUFFER, tmcdrvdata->base + TMC_MODE);

	axictl = readl_relaxed(tmcdrvdata->base + TMC_AXICTL);
	axictl &= ~TMC_AXICTL_CLEAR_MASK;
	axictl |= TMC_AXICTL_PROT_CTL_B1;
	axictl |= TMC_AXICTL_WR_BURST(tmcdrvdata->max_burst_size);
	axictl |= TMC_AXICTL_CACHE_CTL_B0;
	writel_relaxed(axictl, tmcdrvdata->base + TMC_AXICTL);

	tmc_write_dba(tmcdrvdata, 0);

	writel_relaxed(TMC_FFCR_EN_FMT | TMC_FFCR_EN_TI
			| TMC_FFCR_FONFLIN_BIT | TMC_FFCR_STOP_ON_FLUSH,
			tmcdrvdata->base + TMC_FFCR);

	msm_qdss_csr_enable_eth(tmcdrvdata->csr);

	tmc_enable_hw(tmcdrvdata);

	CS_LOCK(tmcdrvdata->base);

	pr_info("Enable ETR ethernet mode.\n");
	return 0;
}

void tmc_eth_disable(struct tmc_eth_data *eth_data)
{
	struct tmc_drvdata *tmcdrvdata = eth_data->tmcdrvdata;

	tmc_disable_hw(tmcdrvdata);
	msm_qdss_csr_disable_eth(tmcdrvdata->csr);
	pr_info("Disable ETR ethernet mode.\n");
}

int tmc_etr_eth_init(struct amba_device *adev,
		     struct tmc_drvdata *drvdata)
{
	struct device *dev = &adev->dev;
	struct tmc_eth_data *eth_data = NULL;

	if (tmc_etr_support_eth_mode(dev)) {
		eth_data = devm_kzalloc(dev, sizeof(*eth_data), GFP_KERNEL);
		if (!eth_data)
			return -ENOMEM;

		drvdata->eth_data = eth_data;
		drvdata->eth_data->tmcdrvdata = drvdata;
		drvdata->mode_support |= BIT(TMC_ETR_OUT_MODE_ETH);
	} else {
		pr_debug("%s doesn't support ethrenet mode.\n", dev_name(dev));
		drvdata->eth_data = NULL;
	}

	return 0;
}
