/*
 * Copyright (c) 2015, 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "sdhci-msm-ice.h"

static void sdhci_msm_ice_error_cb(void *host_ctrl, u32 error)
{
	struct sdhci_msm_host *msm_host = (struct sdhci_msm_host *)host_ctrl;

	dev_err(&msm_host->pdev->dev, "%s: Error in ice operation 0x%x",
		__func__, error);

	if (msm_host->ice.state == SDHCI_MSM_ICE_STATE_ACTIVE)
		msm_host->ice.state = SDHCI_MSM_ICE_STATE_DISABLED;
}

static struct platform_device *sdhci_msm_ice_get_pdevice(struct device *dev)
{
	struct device_node *node;
	struct platform_device *ice_pdev = NULL;

	node = of_parse_phandle(dev->of_node, SDHC_MSM_CRYPTO_LABEL, 0);
	if (!node) {
		dev_dbg(dev, "%s: sdhc-msm-crypto property not specified\n",
			__func__);
		goto out;
	}
	ice_pdev = qcom_ice_get_pdevice(node);
out:
	return ice_pdev;
}

static
struct qcom_ice_variant_ops *sdhci_msm_ice_get_vops(struct device *dev)
{
	struct qcom_ice_variant_ops *ice_vops = NULL;
	struct device_node *node;

	node = of_parse_phandle(dev->of_node, SDHC_MSM_CRYPTO_LABEL, 0);
	if (!node) {
		dev_dbg(dev, "%s: sdhc-msm-crypto property not specified\n",
			__func__);
		goto out;
	}
	ice_vops = qcom_ice_get_variant_ops(node);
	of_node_put(node);
out:
	return ice_vops;
}

static
void sdhci_msm_enable_ice_hci(struct sdhci_host *host, bool enable)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	u32 config = 0;
	u32 ice_cap = 0;

	/*
	 * Enable the cryptographic support inside SDHC.
	 * This is a global config which needs to be enabled
	 * all the time.
	 * Only when it it is enabled, the ICE_HCI capability
	 * will get reflected in CQCAP register.
	 */
	config = readl_relaxed(host->ioaddr + HC_VENDOR_SPECIFIC_FUNC4);

	if (enable)
		config &= ~DISABLE_CRYPTO;
	else
		config |= DISABLE_CRYPTO;
	writel_relaxed(config, host->ioaddr + HC_VENDOR_SPECIFIC_FUNC4);

	/*
	 * CQCAP register is in different register space from above
	 * ice global enable register. So a mb() is required to ensure
	 * above write gets completed before reading the CQCAP register.
	 */
	mb();

	/*
	 * Check if ICE HCI capability support is present
	 * If present, enable it.
	 */
	ice_cap = readl_relaxed(msm_host->cryptoio + ICE_CQ_CAPABILITIES);
	if (ice_cap & ICE_HCI_SUPPORT) {
		config = readl_relaxed(msm_host->cryptoio + ICE_CQ_CONFIG);

		if (enable)
			config |= CRYPTO_GENERAL_ENABLE;
		else
			config &= ~CRYPTO_GENERAL_ENABLE;
		writel_relaxed(config, msm_host->cryptoio + ICE_CQ_CONFIG);
	}
}

int sdhci_msm_ice_get_dev(struct sdhci_host *host)
{
	struct device *sdhc_dev;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	if (!msm_host || !msm_host->pdev) {
		pr_err("%s: invalid msm_host %p or msm_host->pdev\n",
			__func__, msm_host);
		return -EINVAL;
	}

	sdhc_dev = &msm_host->pdev->dev;
	msm_host->ice.vops  = sdhci_msm_ice_get_vops(sdhc_dev);
	msm_host->ice.pdev = sdhci_msm_ice_get_pdevice(sdhc_dev);

	if (msm_host->ice.pdev == ERR_PTR(-EPROBE_DEFER)) {
		dev_err(sdhc_dev, "%s: ICE device not probed yet\n",
			__func__);
		msm_host->ice.pdev = NULL;
		msm_host->ice.vops = NULL;
		return -EPROBE_DEFER;
	}

	if (!msm_host->ice.pdev) {
		dev_dbg(sdhc_dev, "%s: invalid platform device\n", __func__);
		msm_host->ice.vops = NULL;
		return -ENODEV;
	}
	if (!msm_host->ice.vops) {
		dev_dbg(sdhc_dev, "%s: invalid ice vops\n", __func__);
		msm_host->ice.pdev = NULL;
		return -ENODEV;
	}
	msm_host->ice.state = SDHCI_MSM_ICE_STATE_DISABLED;
	return 0;
}

static
int sdhci_msm_ice_pltfm_init(struct sdhci_msm_host *msm_host)
{
	struct resource *ice_memres = NULL;
	struct platform_device *pdev = msm_host->pdev;
	int err = 0;

	if (!msm_host->ice_hci_support)
		goto out;
	/*
	 * ICE HCI registers are present in cmdq register space.
	 * So map the cmdq mem for accessing ICE HCI registers.
	 */
	ice_memres = platform_get_resource_byname(pdev,
						IORESOURCE_MEM, "cmdq_mem");
	if (!ice_memres) {
		dev_err(&pdev->dev, "Failed to get iomem resource for ice\n");
		err = -EINVAL;
		goto out;
	}
	msm_host->cryptoio = devm_ioremap(&pdev->dev,
					ice_memres->start,
					resource_size(ice_memres));
	if (!msm_host->cryptoio) {
		dev_err(&pdev->dev, "Failed to remap registers\n");
		err = -ENOMEM;
	}
out:
	return err;
}

int sdhci_msm_ice_init(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int err = 0;

	if (msm_host->ice.vops->init) {
		err = sdhci_msm_ice_pltfm_init(msm_host);
		if (err)
			goto out;

		if (msm_host->ice_hci_support)
			sdhci_msm_enable_ice_hci(host, true);

		err = msm_host->ice.vops->init(msm_host->ice.pdev,
					msm_host,
					sdhci_msm_ice_error_cb);
		if (err) {
			pr_err("%s: ice init err %d\n",
				mmc_hostname(host->mmc), err);
			sdhci_msm_ice_print_regs(host);
			if (msm_host->ice_hci_support)
				sdhci_msm_enable_ice_hci(host, false);
			goto out;
		}
		msm_host->ice.state = SDHCI_MSM_ICE_STATE_ACTIVE;
	}

out:
	return err;
}

void sdhci_msm_ice_cfg_reset(struct sdhci_host *host, u32 slot)
{
	writel_relaxed(SDHCI_MSM_ICE_ENABLE_BYPASS,
		host->ioaddr + CORE_VENDOR_SPEC_ICE_CTRL_INFO_3_n + 16 * slot);
}

static
int sdhci_msm_ice_get_cfg(struct sdhci_msm_host *msm_host, struct request *req,
			unsigned int *bypass, short *key_index)
{
	int err = 0;
	struct ice_data_setting ice_set;

	memset(&ice_set, 0, sizeof(struct ice_data_setting));
	if (msm_host->ice.vops->config_start) {
		err = msm_host->ice.vops->config_start(
						msm_host->ice.pdev,
						req, &ice_set, false);
		if (err) {
			pr_err("%s: ice config failed %d\n",
					mmc_hostname(msm_host->mmc), err);
			return err;
		}
	}
	/* if writing data command */
	if (rq_data_dir(req) == WRITE)
		*bypass = ice_set.encr_bypass ?
				SDHCI_MSM_ICE_ENABLE_BYPASS :
				SDHCI_MSM_ICE_DISABLE_BYPASS;
	/* if reading data command */
	else if (rq_data_dir(req) == READ)
		*bypass = ice_set.decr_bypass ?
				SDHCI_MSM_ICE_ENABLE_BYPASS :
				SDHCI_MSM_ICE_DISABLE_BYPASS;
	*key_index = ice_set.crypto_data.key_index;
	return err;
}

static
void sdhci_msm_ice_update_cfg(struct sdhci_host *host, u64 lba,
			u32 slot, unsigned int bypass, short key_index)
{
	unsigned int ctrl_info_val = 0;

	/* Configure ICE index */
	ctrl_info_val =
		(key_index &
		 MASK_SDHCI_MSM_ICE_CTRL_INFO_KEY_INDEX)
		 << OFFSET_SDHCI_MSM_ICE_CTRL_INFO_KEY_INDEX;

	/* Configure data unit size of transfer request */
	ctrl_info_val |=
		(SDHCI_MSM_ICE_TR_DATA_UNIT_512_B &
		 MASK_SDHCI_MSM_ICE_CTRL_INFO_CDU)
		 << OFFSET_SDHCI_MSM_ICE_CTRL_INFO_CDU;

	/* Configure ICE bypass mode */
	ctrl_info_val |=
		(bypass & MASK_SDHCI_MSM_ICE_CTRL_INFO_BYPASS)
		 << OFFSET_SDHCI_MSM_ICE_CTRL_INFO_BYPASS;

	writel_relaxed((lba & 0xFFFFFFFF),
		host->ioaddr + CORE_VENDOR_SPEC_ICE_CTRL_INFO_1_n + 16 * slot);
	writel_relaxed(((lba >> 32) & 0xFFFFFFFF),
		host->ioaddr + CORE_VENDOR_SPEC_ICE_CTRL_INFO_2_n + 16 * slot);
	writel_relaxed(ctrl_info_val,
		host->ioaddr + CORE_VENDOR_SPEC_ICE_CTRL_INFO_3_n + 16 * slot);
	/* Ensure ICE registers are configured before issuing SDHCI request */
	mb();
}

static inline
void sdhci_msm_ice_hci_update_cmdq_cfg(u64 dun, unsigned int bypass,
				short key_index, u64 *ice_ctx)
{
	/*
	 * The naming convention got changed between ICE2.0 and ICE3.0
	 * registers fields. Below is the equivalent names for
	 * ICE3.0 Vs ICE2.0:
	 *   Data Unit Number(DUN) == Logical Base address(LBA)
	 *   Crypto Configuration index (CCI) == Key Index
	 *   Crypto Enable (CE) == !BYPASS
	 */
	 if (ice_ctx)
		*ice_ctx = DATA_UNIT_NUM(dun) |
			CRYPTO_CONFIG_INDEX(key_index) |
			CRYPTO_ENABLE(!bypass);
}

static
void sdhci_msm_ice_hci_update_noncq_cfg(struct sdhci_host *host,
		u64 dun, unsigned int bypass, short key_index)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	unsigned int crypto_params = 0;
	/*
	 * The naming convention got changed between ICE2.0 and ICE3.0
	 * registers fields. Below is the equivalent names for
	 * ICE3.0 Vs ICE2.0:
	 *   Data Unit Number(DUN) == Logical Base address(LBA)
	 *   Crypto Configuration index (CCI) == Key Index
	 *   Crypto Enable (CE) == !BYPASS
	 */
	/* Configure ICE bypass mode */
	crypto_params |=
		(!bypass & MASK_SDHCI_MSM_ICE_HCI_PARAM_CE)
			<< OFFSET_SDHCI_MSM_ICE_HCI_PARAM_CE;
	/* Configure Crypto Configure Index (CCI) */
	crypto_params |= (key_index &
			 MASK_SDHCI_MSM_ICE_HCI_PARAM_CCI)
			 << OFFSET_SDHCI_MSM_ICE_HCI_PARAM_CCI;

	writel_relaxed((crypto_params & 0xFFFFFFFF),
		msm_host->cryptoio + ICE_NONCQ_CRYPTO_PARAMS);

	/* Update DUN */
	writel_relaxed((dun & 0xFFFFFFFF),
		msm_host->cryptoio + ICE_NONCQ_CRYPTO_DUN);
	/* Ensure ICE registers are configured before issuing SDHCI request */
	mb();
}

int sdhci_msm_ice_cfg(struct sdhci_host *host, struct mmc_request *mrq,
			u32 slot)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int err = 0;
	short key_index = 0;
	sector_t lba = 0;
	unsigned int bypass = SDHCI_MSM_ICE_ENABLE_BYPASS;
	struct request *req;

	if (msm_host->ice.state != SDHCI_MSM_ICE_STATE_ACTIVE) {
		pr_err("%s: ice is in invalid state %d\n",
			mmc_hostname(host->mmc), msm_host->ice.state);
		return -EINVAL;
	}

	WARN_ON(!mrq);
	if (!mrq)
		return -EINVAL;
	req = mrq->req;
	if (req) {
		lba = req->__sector;
		err = sdhci_msm_ice_get_cfg(msm_host, req, &bypass, &key_index);
		if (err)
			return err;
		pr_debug("%s: %s: slot %d bypass %d key_index %d\n",
				mmc_hostname(host->mmc),
				(rq_data_dir(req) == WRITE) ? "WRITE" : "READ",
				slot, bypass, key_index);
	}

	if (msm_host->ice_hci_support) {
		/* For ICE HCI / ICE3.0 */
		sdhci_msm_ice_hci_update_noncq_cfg(host, lba, bypass,
						key_index);
	} else {
		/* For ICE versions earlier to ICE3.0 */
		sdhci_msm_ice_update_cfg(host, lba, slot, bypass, key_index);
	}
	return 0;
}

int sdhci_msm_ice_cmdq_cfg(struct sdhci_host *host,
			struct mmc_request *mrq, u32 slot, u64 *ice_ctx)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int err = 0;
	short key_index;
	sector_t lba = 0;
	unsigned int bypass = SDHCI_MSM_ICE_ENABLE_BYPASS;
	struct request *req;

	if (msm_host->ice.state != SDHCI_MSM_ICE_STATE_ACTIVE) {
		pr_err("%s: ice is in invalid state %d\n",
			mmc_hostname(host->mmc), msm_host->ice.state);
		return -EINVAL;
	}

	WARN_ON(!mrq);
	if (!mrq)
		return -EINVAL;
	req = mrq->req;
	if (req) {
		lba = req->__sector;
		err = sdhci_msm_ice_get_cfg(msm_host, req, &bypass, &key_index);
		if (err)
			return err;
		pr_debug("%s: %s: slot %d bypass %d key_index %d\n",
				mmc_hostname(host->mmc),
				(rq_data_dir(req) == WRITE) ? "WRITE" : "READ",
				slot, bypass, key_index);
	}

	if (msm_host->ice_hci_support) {
		/* For ICE HCI / ICE3.0 */
		sdhci_msm_ice_hci_update_cmdq_cfg(lba, bypass, key_index,
						ice_ctx);
	} else {
		/* For ICE versions earlier to ICE3.0 */
		sdhci_msm_ice_update_cfg(host, lba, slot, bypass, key_index);
	}
	return 0;
}

int sdhci_msm_ice_reset(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int err = 0;

	if (msm_host->ice.state != SDHCI_MSM_ICE_STATE_ACTIVE) {
		pr_err("%s: ice is in invalid state before reset %d\n",
			mmc_hostname(host->mmc), msm_host->ice.state);
		return -EINVAL;
	}

	if (msm_host->ice.vops->reset) {
		err = msm_host->ice.vops->reset(msm_host->ice.pdev);
		if (err) {
			pr_err("%s: ice reset failed %d\n",
					mmc_hostname(host->mmc), err);
			sdhci_msm_ice_print_regs(host);
			return err;
		}
	}

	/* If ICE HCI support is present then re-enable it */
	if (msm_host->ice_hci_support)
		sdhci_msm_enable_ice_hci(host, true);

	if (msm_host->ice.state != SDHCI_MSM_ICE_STATE_ACTIVE) {
		pr_err("%s: ice is in invalid state after reset %d\n",
			mmc_hostname(host->mmc), msm_host->ice.state);
		return -EINVAL;
	}
	return 0;
}

int sdhci_msm_ice_resume(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int err = 0;

	if (msm_host->ice.state !=
			SDHCI_MSM_ICE_STATE_SUSPENDED) {
		pr_err("%s: ice is in invalid state before resume %d\n",
			mmc_hostname(host->mmc), msm_host->ice.state);
		return -EINVAL;
	}

	if (msm_host->ice.vops->resume) {
		err = msm_host->ice.vops->resume(msm_host->ice.pdev);
		if (err) {
			pr_err("%s: ice resume failed %d\n",
					mmc_hostname(host->mmc), err);
			return err;
		}
	}

	msm_host->ice.state = SDHCI_MSM_ICE_STATE_ACTIVE;
	return 0;
}

int sdhci_msm_ice_suspend(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int err = 0;

	if (msm_host->ice.state !=
			SDHCI_MSM_ICE_STATE_ACTIVE) {
		pr_err("%s: ice is in invalid state before resume %d\n",
			mmc_hostname(host->mmc), msm_host->ice.state);
		return -EINVAL;
	}

	if (msm_host->ice.vops->suspend) {
		err = msm_host->ice.vops->suspend(msm_host->ice.pdev);
		if (err) {
			pr_err("%s: ice suspend failed %d\n",
					mmc_hostname(host->mmc), err);
			return -EINVAL;
		}
	}
	msm_host->ice.state = SDHCI_MSM_ICE_STATE_SUSPENDED;
	return 0;
}

int sdhci_msm_ice_get_status(struct sdhci_host *host, int *ice_status)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int stat = -EINVAL;

	if (msm_host->ice.state != SDHCI_MSM_ICE_STATE_ACTIVE) {
		pr_err("%s: ice is in invalid state %d\n",
			mmc_hostname(host->mmc), msm_host->ice.state);
		return -EINVAL;
	}

	if (msm_host->ice.vops->status) {
		*ice_status = 0;
		stat = msm_host->ice.vops->status(msm_host->ice.pdev);
		if (stat < 0) {
			pr_err("%s: ice get sts failed %d\n",
					mmc_hostname(host->mmc), stat);
			return -EINVAL;
		}
		*ice_status = stat;
	}
	return 0;
}

void sdhci_msm_ice_print_regs(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	if (msm_host->ice.vops->debug)
		msm_host->ice.vops->debug(msm_host->ice.pdev);
}
