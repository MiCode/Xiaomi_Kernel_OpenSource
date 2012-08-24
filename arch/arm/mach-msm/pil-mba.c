/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/ioport.h>
#include <linux/elf.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/of.h>

#include "peripheral-loader.h"

#define RMB_MBA_COMMAND			0x08
#define RMB_MBA_STATUS			0x0C
#define RMB_PMI_META_DATA		0x10
#define RMB_PMI_CODE_START		0x14
#define RMB_PMI_CODE_LENGTH		0x18

#define CMD_META_DATA_READY		0x1
#define CMD_LOAD_READY			0x2

#define STATUS_META_DATA_AUTH_SUCCESS	0x3
#define STATUS_AUTH_COMPLETE		0x4

#define PROXY_TIMEOUT_MS		10000
#define POLL_INTERVAL_US		50

static int modem_auth_timeout_ms = 10000;
module_param(modem_auth_timeout_ms, int, S_IRUGO | S_IWUSR);

struct mba_data {
	void __iomem *reg_base;
	void __iomem *metadata_base;
	unsigned long metadata_phys;
	struct pil_device *pil;
	struct clk *xo;
	u32 img_length;
};

static int pil_mba_make_proxy_votes(struct pil_desc *pil)
{
	int ret;
	struct mba_data *drv = dev_get_drvdata(pil->dev);

	ret = clk_prepare_enable(drv->xo);
	if (ret) {
		dev_err(pil->dev, "Failed to enable XO\n");
		return ret;
	}
	return 0;
}

static void pil_mba_remove_proxy_votes(struct pil_desc *pil)
{
	struct mba_data *drv = dev_get_drvdata(pil->dev);
	clk_disable_unprepare(drv->xo);
}

static int pil_mba_init_image(struct pil_desc *pil,
			      const u8 *metadata, size_t size)
{
	struct mba_data *drv = dev_get_drvdata(pil->dev);
	s32 status;
	int ret;

	/* Copy metadata to assigned shared buffer location */
	memcpy(drv->metadata_base, metadata, size);

	/* Initialize length counter to 0 */
	writel_relaxed(0, drv->reg_base + RMB_PMI_CODE_LENGTH);
	drv->img_length = 0;

	/* Pass address of meta-data to the MBA and perform authentication */
	writel_relaxed(drv->metadata_phys, drv->reg_base + RMB_PMI_META_DATA);
	writel_relaxed(CMD_META_DATA_READY, drv->reg_base + RMB_MBA_COMMAND);
	ret = readl_poll_timeout(drv->reg_base + RMB_MBA_STATUS, status,
		status == STATUS_META_DATA_AUTH_SUCCESS || status < 0,
		POLL_INTERVAL_US, modem_auth_timeout_ms * 1000);
	if (ret) {
		dev_err(pil->dev, "MBA authentication timed out\n");
	} else if (status < 0) {
		dev_err(pil->dev, "MBA returned error %d\n", status);
		ret = -EINVAL;
	}

	return ret;
}

static int pil_mba_verify_blob(struct pil_desc *pil, u32 phy_addr,
			       size_t size)
{
	struct mba_data *drv = dev_get_drvdata(pil->dev);
	s32 status;

	/* Begin image authentication */
	if (drv->img_length == 0) {
		writel_relaxed(phy_addr, drv->reg_base + RMB_PMI_CODE_START);
		writel_relaxed(CMD_LOAD_READY, drv->reg_base + RMB_MBA_COMMAND);
	}
	/* Increment length counter */
	drv->img_length += size;
	writel_relaxed(drv->img_length, drv->reg_base + RMB_PMI_CODE_LENGTH);

	status = readl_relaxed(drv->reg_base + RMB_MBA_STATUS);
	if (status < 0) {
		dev_err(pil->dev, "MBA returned error %d\n", status);
		return -EINVAL;
	}

	return 0;
}

static int pil_mba_auth(struct pil_desc *pil)
{
	struct mba_data *drv = dev_get_drvdata(pil->dev);
	int ret;
	s32 status;

	/* Wait for all segments to be authenticated or an error to occur */
	ret = readl_poll_timeout(drv->reg_base + RMB_MBA_STATUS, status,
			status == STATUS_AUTH_COMPLETE || status < 0,
			50, modem_auth_timeout_ms * 1000);
	if (ret) {
		dev_err(pil->dev, "MBA authentication timed out\n");
	} else if (status < 0) {
		dev_err(pil->dev, "MBA returned error %d\n", status);
		ret = -EINVAL;
	}

	return ret;
}

static int pil_mba_shutdown(struct pil_desc *pil)
{
	return 0;
}

static struct pil_reset_ops pil_mba_ops = {
	.init_image = pil_mba_init_image,
	.proxy_vote = pil_mba_make_proxy_votes,
	.proxy_unvote = pil_mba_remove_proxy_votes,
	.verify_blob = pil_mba_verify_blob,
	.auth_and_reset = pil_mba_auth,
	.shutdown = pil_mba_shutdown,
};

static int __devinit pil_mba_driver_probe(struct platform_device *pdev)
{
	struct mba_data *drv;
	struct resource *res;
	struct pil_desc *desc;
	int ret;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;
	platform_set_drvdata(pdev, drv);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -EINVAL;
	drv->reg_base = devm_ioremap(&pdev->dev, res->start,
				     resource_size(res));
	if (!drv->reg_base)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res) {
		drv->metadata_base = devm_ioremap(&pdev->dev, res->start,
						  resource_size(res));
		if (!drv->metadata_base)
			return -ENOMEM;
		drv->metadata_phys = res->start;
	}

	desc = devm_kzalloc(&pdev->dev, sizeof(*desc), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	ret = of_property_read_string(pdev->dev.of_node, "qcom,firmware-name",
				      &desc->name);
	if (ret)
		return ret;

	of_property_read_string(pdev->dev.of_node, "qcom,depends-on",
				      &desc->depends_on);

	drv->xo = devm_clk_get(&pdev->dev, "xo");
	if (IS_ERR(drv->xo))
		return PTR_ERR(drv->xo);

	desc->dev = &pdev->dev;
	desc->ops = &pil_mba_ops;
	desc->owner = THIS_MODULE;
	desc->proxy_timeout = PROXY_TIMEOUT_MS;

	drv->pil = msm_pil_register(desc);
	if (IS_ERR(drv->pil))
		return PTR_ERR(drv->pil);

	return 0;
}

static int __devexit pil_mba_driver_exit(struct platform_device *pdev)
{
	struct mba_data *drv = platform_get_drvdata(pdev);
	msm_pil_unregister(drv->pil);
	return 0;
}

static struct of_device_id mba_match_table[] = {
	{ .compatible = "qcom,pil-mba" },
	{}
};

struct platform_driver pil_mba_driver = {
	.probe = pil_mba_driver_probe,
	.remove = __devexit_p(pil_mba_driver_exit),
	.driver = {
		.name = "pil-mba",
		.of_match_table = mba_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init pil_mba_init(void)
{
	return platform_driver_register(&pil_mba_driver);
}
module_init(pil_mba_init);

static void __exit pil_mba_exit(void)
{
	platform_driver_unregister(&pil_mba_driver);
}
module_exit(pil_mba_exit);

MODULE_DESCRIPTION("Support for modem boot using the Modem Boot Authenticator");
MODULE_LICENSE("GPL v2");
