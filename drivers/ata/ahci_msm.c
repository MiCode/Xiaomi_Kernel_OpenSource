/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/ahci_platform.h>
#include <linux/phy/phy.h>
#include <linux/of.h>
#include <linux/of_device.h>

struct msm_ahci_clk_info {
	struct list_head list;
	struct clk *clk;
	const char *name;
	u32 max_freq;
	bool enabled;
};

struct msm_ahci_host {
	struct platform_device *ahci_pdev;
	struct list_head clk_list_head;
	void __iomem *ahci_base;
	struct phy *phy;
	bool phy_powered_on;
};

static int msm_ahci_enable_clk(struct device *dev,
		struct msm_ahci_clk_info *clki)
{
	int ret = 0;

	if (clki->enabled)
		goto out;

	ret = clk_prepare_enable(clki->clk);
	if (ret) {
		dev_err(dev, "%s: %s prepare enable failed, %d\n",
				__func__, clki->name, ret);
	} else {
		clki->enabled = true;
		dev_dbg(dev, "%s: clk: %s enabled\n",
				__func__, clki->name);
	}
out:
	return ret;
}

static void msm_ahci_disable_clk(struct device *dev,
		struct msm_ahci_clk_info *clki)
{
	if (!clki->enabled)
		return;

	clk_disable_unprepare(clki->clk);
	clki->enabled = false;
	dev_dbg(dev, "%s: clk: %s disabled\n",
				__func__, clki->name);
}

static int msm_ahci_setup_asic_rbc_clks(struct msm_ahci_host *host, bool on)
{
	int ret = 0;
	struct device *dev = host->ahci_pdev->dev.parent;
	struct msm_ahci_clk_info *clki;
	struct list_head *head = &host->clk_list_head;

	if (!head || list_empty(head))
		goto out;

	/*
	 * asic0_clk and rbc0_clk should be enabled/disabled
	 * only when PHY is powered on.
	 */
	if (!host->phy_powered_on)
		goto out;

gate_ungate:
	list_for_each_entry(clki, head, list) {
		if (IS_ERR_OR_NULL(clki->clk))
			continue;

		if (!strcmp(clki->name, "asic0_clk") ||
				!strcmp(clki->name, "rbc0_clk")) {
			if (on)
				ret = msm_ahci_enable_clk(dev, clki);
			else
				msm_ahci_disable_clk(dev, clki);
		}
	}
out:
	if (ret) {
		on = false;
		goto gate_ungate;
	}

	return ret;
}

static int msm_ahci_setup_clocks(struct msm_ahci_host *host, bool on)
{
	int ret = 0;
	struct device *dev = host->ahci_pdev->dev.parent;
	struct msm_ahci_clk_info *clki;
	struct list_head *head = &host->clk_list_head;

	if (!head || list_empty(head))
		goto out;

gate_ungate:
	list_for_each_entry(clki, head, list) {
		if (IS_ERR_OR_NULL(clki->clk))
			continue;

		/*
		 * asic0_clk and rbc0_clk are handled separately
		 * see msm_ahci_setup_asic_rbc_clks().
		 */
		if (!strcmp(clki->name, "asic0_clk") ||
				!strcmp(clki->name, "rbc0_clk"))
				continue;
		if (on)
			ret = msm_ahci_enable_clk(dev, clki);
		else
			msm_ahci_disable_clk(dev, clki);
	}
out:
	if (ret) {
		on = false;
		goto gate_ungate;
	}

	return ret;
}

static int msm_ahci_get_clocks(struct msm_ahci_host *host)
{
	int ret = 0;
	struct msm_ahci_clk_info *clki;
	struct device *dev = host->ahci_pdev->dev.parent;
	struct list_head *head = &host->clk_list_head;

	if (!head || list_empty(head))
		goto out;

	list_for_each_entry(clki, head, list) {
		if (!clki->name)
			continue;

		clki->clk = devm_clk_get(dev, clki->name);
		if (IS_ERR(clki->clk)) {
			ret = PTR_ERR(clki->clk);
			dev_err(dev, "%s: %s clk get failed, %d\n",
					__func__, clki->name, ret);
			goto out;
		}

		if (clki->max_freq) {
			ret = clk_set_rate(clki->clk, clki->max_freq);
			if (ret) {
				dev_err(dev, "%s: %s clk set rate(%dHz) failed, %d\n",
					__func__, clki->name,
					clki->max_freq, ret);
				goto out;
			}
		}
		dev_dbg(dev, "%s: clk: %s, rate: %lu\n", __func__,
				clki->name, clk_get_rate(clki->clk));
	}
out:
	return ret;
}

static int msm_ahci_parse_clock_info(struct msm_ahci_host *host)
{
	int ret = 0;
	int cnt;
	int i;
	struct device *dev = host->ahci_pdev->dev.parent;
	struct device_node *np = dev->of_node;
	char *name;
	u32 *clkfreq = NULL;
	struct msm_ahci_clk_info *clki;

	if (!np)
		goto out;

	INIT_LIST_HEAD(&host->clk_list_head);

	cnt = of_property_count_strings(np, "clock-names");
	if (!cnt || (cnt == -EINVAL)) {
		dev_info(dev, "%s: Unable to find clocks, assuming enabled\n",
				__func__);
	} else if (cnt < 0) {
		dev_err(dev, "%s: count clock strings failed, err %d\n",
				__func__, cnt);
		ret = cnt;
	}

	if (cnt <= 0)
		goto out;

	clkfreq = kzalloc(cnt * sizeof(*clkfreq), GFP_KERNEL);
	if (!clkfreq) {
		ret = -ENOMEM;
		dev_err(dev, "%s: memory alloc failed\n", __func__);
		goto out;
	}

	ret = of_property_read_u32_array(np,
			"max-clock-frequency-hz", clkfreq, cnt);
	if (ret && (ret != -EINVAL)) {
		dev_err(dev, "%s: invalid max-clock-frequency-hz property, %d\n",
				__func__, ret);
		goto out;
	}

	for (i = 0; i < cnt; i++) {
		ret = of_property_read_string_index(np,
				"clock-names", i, (const char **)&name);
		if (ret)
			goto out;

		clki = devm_kzalloc(dev, sizeof(*clki), GFP_KERNEL);
		if (!clki) {
			ret = -ENOMEM;
			goto out;
		}

		clki->max_freq = clkfreq[i];
		clki->name = kstrdup(name, GFP_KERNEL);
		list_add_tail(&clki->list, &host->clk_list_head);
	}
out:
	kfree(clkfreq);
	return ret;
}

static int msm_ahci_init_clocks(struct msm_ahci_host *host)
{
	int ret = 0;

	ret = msm_ahci_parse_clock_info(host);
	if (ret)
		goto out;

	ret = msm_ahci_get_clocks(host);
	if (ret)
		goto out;

	ret = msm_ahci_setup_clocks(host, true);
	if (ret)
		goto out;
out:
	return ret;
}

static void msm_ahci_exit_clocks(struct msm_ahci_host *host)
{
	msm_ahci_setup_clocks(host, false);
}

static int msm_ahci_init_phy(struct msm_ahci_host *host)
{
	int ret = 0;
	struct device *dev = host->ahci_pdev->dev.parent;

	host->phy = devm_phy_get(dev, "sata-6g");
	if (IS_ERR(host->phy)) {
		ret = PTR_ERR(host->phy);
		dev_err(dev, "PHY get failed %d\n", ret);
		goto out;
	}

	ret = phy_init(host->phy);
	if (ret) {
		dev_err(dev, "PHY initialization failed %d\n", ret);
		goto out;
	}

	ret = phy_power_on(host->phy);
	if (ret) {
		dev_err(dev, "PHY power on failed %d\n", ret);
		goto out;
	}

	host->phy_powered_on = true;

	/* asic0 and rbc0 clks needs to be ungated only after phy power on */
	ret = msm_ahci_setup_asic_rbc_clks(host, true);
	if (ret) {
		dev_err(dev, "failed to enable asic0/rbc0 clks %d", ret);
		goto out;
	}

out:
	return ret;
}

static void msm_ahci_exit_phy(struct msm_ahci_host *host)
{
	msm_ahci_setup_asic_rbc_clks(host, false);
	phy_power_off(host->phy);
	host->phy_powered_on = false;
}

static int msm_ahci_init(struct device *ahci_dev, void __iomem *addr)
{
	int ret;
	struct device *dev = ahci_dev->parent;
	struct msm_ahci_host *host = dev_get_drvdata(dev);

	/* Save ahci mmio to access vendor specific registers */
	host->ahci_base = addr;

	ret = msm_ahci_init_clocks(host);
	if (ret) {
		dev_err(dev, "AHCI clk init failed with err=%d\n", ret);
		goto out;
	}

	ret = msm_ahci_init_phy(host);
	if (ret) {
		dev_err(dev, "SATA PHY init failed with err=%d\n", ret);
		goto out_exit_clks;
	}

	return 0;

out_exit_clks:
	msm_ahci_exit_clocks(host);
out:
	return ret;
}

static void msm_ahci_exit(struct device *ahci_dev)
{
	struct device *dev = ahci_dev->parent;
	struct msm_ahci_host *host = dev_get_drvdata(dev);


	msm_ahci_exit_phy(host);
	msm_ahci_exit_clocks(host);
}

static int msm_ahci_suspend(struct device *ahci_dev)
{
	int ret;
	struct device *dev = ahci_dev->parent;
	struct msm_ahci_host *host = dev_get_drvdata(dev);

	msm_ahci_setup_asic_rbc_clks(host, false);
	ret = phy_power_off(host->phy);
	if (ret) {
		dev_err(dev, "%s: PHY power off failed %d\n", __func__, ret);
		goto out;
	} else {
		host->phy_powered_on = false;
	}

	msm_ahci_setup_clocks(host, false);
out:
	return ret;
}

static int msm_ahci_resume(struct device *ahci_dev)
{
	int ret;
	struct device *dev = ahci_dev->parent;
	struct msm_ahci_host *host = dev_get_drvdata(dev);

	ret = msm_ahci_setup_clocks(host, true);
	if (ret)
		goto out;

	ret = phy_power_on(host->phy);
	if (ret) {
		dev_err(dev, "%s: PHY power on failed %d\n", __func__, ret);
		goto out;
	} else {
		host->phy_powered_on = true;
	}

	/* asic0 and rbc0 clks needs to be ungated only after phy power on */
	ret = msm_ahci_setup_asic_rbc_clks(host, true);
out:
	return ret;
}

static struct ahci_platform_data msm_ahci_pdata = {
	.init = msm_ahci_init,
	.exit = msm_ahci_exit,
	.suspend = msm_ahci_suspend,
	.resume = msm_ahci_resume,
};

static const struct of_device_id msm_ahci_of_match[] = {
	{ .compatible = "qcom,msm-ahci", .data = &msm_ahci_pdata },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, msm_ahci_of_match);

static int msm_ahci_probe(struct platform_device *pdev)
{
	int err;
	struct msm_ahci_host *host;
	struct device *dev = &pdev->dev;
	const struct of_device_id *of_id;
	const struct ahci_platform_data *pdata = NULL;
	struct platform_device *ahci_pdev;
	struct device *ahci_dev;
	int ret = 0;

	host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);
	if (!host) {
		dev_err(dev, "failed to allocate memory for msm host\n");
		ret = -ENOMEM;
		goto err;
	}

	ahci_pdev = platform_device_alloc("ahci", -1);
	if (!ahci_pdev) {
		dev_err(dev, "failed to allocate ahci platform device\n");
		ret = -ENODEV;
		goto err;
	}

	ahci_dev = &ahci_pdev->dev;
	ahci_dev->parent = dev;

	if (!dev->dma_mask)
		dev->dma_mask = &dev->coherent_dma_mask;

	if (!dma_set_mask(dev, DMA_BIT_MASK(64))) {
		dma_set_coherent_mask(dev, DMA_BIT_MASK(64));
	} else if (!dma_set_mask(dev, DMA_BIT_MASK(32))) {
		dma_set_coherent_mask(dev, DMA_BIT_MASK(32));
	} else {
		err = -EIO;
		dev_err(dev, "unable to set dma mask\n");
		goto err_put_device;
	}

	ahci_dev->dma_mask = dev->dma_mask;
	ahci_dev->dma_parms = dev->dma_parms;
	dma_set_coherent_mask(ahci_dev, dev->coherent_dma_mask);

	host->ahci_pdev = ahci_pdev;

	platform_set_drvdata(pdev, host);

	of_id = of_match_device(msm_ahci_of_match, dev);
	if (of_id) {
		pdata = of_id->data;
	} else {
		ret = -EINVAL;
		dev_err(dev, "pdata is required to initialze ahci %d\n", ret);
		goto err;
	}

	ahci_dev->of_node = dev->of_node;

	ret = platform_device_add_resources(ahci_pdev,
			pdev->resource, pdev->num_resources);
	if (ret) {
		dev_err(dev, "failed to add resources %d\n", ret);
		goto err_put_device;
	}

	ret = platform_device_add_data(ahci_pdev, pdata, sizeof(*pdata));
	if (ret) {
		dev_err(dev, "failed to add pdata %d\n", ret);
		goto err_put_device;
	}

	ret = platform_device_add(ahci_pdev);
	if (ret) {
		dev_err(dev, "failed to register ahci device %d\n", ret);
		goto err_put_device;
	}

	return 0;

err_put_device:
	platform_device_put(ahci_pdev);
err:
	return ret;
}

static int msm_ahci_remove(struct platform_device *pdev)
{
	struct msm_ahci_host *host = platform_get_drvdata(pdev);

	platform_device_unregister(host->ahci_pdev);

	return 0;
}

static struct platform_driver msm_ahci_driver = {
	.probe		= msm_ahci_probe,
	.remove		= msm_ahci_remove,
	.driver		= {
		.name	= "ahci-msm",
		.owner = THIS_MODULE,
		.of_match_table = msm_ahci_of_match,
	},
};
module_platform_driver(msm_ahci_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("AHCI platform MSM Glue Layer");
