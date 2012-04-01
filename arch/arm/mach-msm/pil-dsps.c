/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include <linux/elf.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/delay.h>

#include <mach/msm_iomap.h>

#include "peripheral-loader.h"
#include "scm-pas.h"

#define PPSS_RESET			(MSM_CLK_CTL_BASE + 0x2594)
#define PPSS_RESET_PROC_RESET		0x2
#define PPSS_RESET_RESET		0x1
#define PPSS_PROC_CLK_CTL		(MSM_CLK_CTL_BASE + 0x2588)
#define CLK_BRANCH_ENA			0x10
#define PPSS_HCLK_CTL			(MSM_CLK_CTL_BASE + 0x2580)
#define CLK_HALT_DFAB_STATE		(MSM_CLK_CTL_BASE + 0x2FC8)

static int init_image_dsps(struct pil_desc *pil, const u8 *metadata,
				     size_t size)
{
	/* Bring memory and bus interface out of reset */
	writel_relaxed(PPSS_RESET_PROC_RESET, PPSS_RESET);
	writel_relaxed(CLK_BRANCH_ENA, PPSS_HCLK_CTL);
	mb();
	return 0;
}

static int reset_dsps(struct pil_desc *pil)
{
	writel_relaxed(CLK_BRANCH_ENA, PPSS_PROC_CLK_CTL);
	while (readl_relaxed(CLK_HALT_DFAB_STATE) & BIT(18))
		cpu_relax();
	/* Bring DSPS out of reset */
	writel_relaxed(0x0, PPSS_RESET);
	return 0;
}

static int shutdown_dsps(struct pil_desc *pil)
{
	writel_relaxed(PPSS_RESET_PROC_RESET | PPSS_RESET_RESET, PPSS_RESET);
	usleep_range(1000, 2000);
	writel_relaxed(PPSS_RESET_PROC_RESET, PPSS_RESET);
	writel_relaxed(0x0, PPSS_PROC_CLK_CTL);
	return 0;
}

struct pil_reset_ops pil_dsps_ops = {
	.init_image = init_image_dsps,
	.auth_and_reset = reset_dsps,
	.shutdown = shutdown_dsps,
};

static int init_image_dsps_trusted(struct pil_desc *pil, const u8 *metadata,
				   size_t size)
{
	return pas_init_image(PAS_DSPS, metadata, size);
}

static int reset_dsps_trusted(struct pil_desc *pil)
{
	return pas_auth_and_reset(PAS_DSPS);
}

static int shutdown_dsps_trusted(struct pil_desc *pil)
{
	return pas_shutdown(PAS_DSPS);
}

struct pil_reset_ops pil_dsps_ops_trusted = {
	.init_image = init_image_dsps_trusted,
	.auth_and_reset = reset_dsps_trusted,
	.shutdown = shutdown_dsps_trusted,
};

static int __devinit pil_dsps_driver_probe(struct platform_device *pdev)
{
	struct pil_desc *desc;
	struct pil_device *pil;

	desc = devm_kzalloc(&pdev->dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	desc->name = pdev->dev.platform_data;
	desc->dev = &pdev->dev;
	desc->owner = THIS_MODULE;
	if (pas_supported(PAS_DSPS) > 0) {
		desc->ops = &pil_dsps_ops_trusted;
		dev_info(&pdev->dev, "using secure boot\n");
	} else {
		desc->ops = &pil_dsps_ops;
		dev_info(&pdev->dev, "using non-secure boot\n");
	}
	pil = msm_pil_register(desc);
	if (IS_ERR(pil))
		return PTR_ERR(pil);
	platform_set_drvdata(pdev, pil);
	return 0;
}

static int __devexit pil_dsps_driver_exit(struct platform_device *pdev)
{
	struct pil_device *pil = platform_get_drvdata(pdev);
	msm_pil_unregister(pil);
	return 0;
}

static struct platform_driver pil_dsps_driver = {
	.probe = pil_dsps_driver_probe,
	.remove = __devexit_p(pil_dsps_driver_exit),
	.driver = {
		.name = "pil_dsps",
		.owner = THIS_MODULE,
	},
};

static int __init pil_dsps_init(void)
{
	return platform_driver_register(&pil_dsps_driver);
}
module_init(pil_dsps_init);

static void __exit pil_dsps_exit(void)
{
	platform_driver_unregister(&pil_dsps_driver);
}
module_exit(pil_dsps_exit);

MODULE_DESCRIPTION("Support for booting sensors (DSPS) images");
MODULE_LICENSE("GPL v2");
