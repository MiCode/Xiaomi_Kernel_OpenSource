/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/ioport.h>
#include <linux/elf.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/clk.h>

#include <mach/msm_iomap.h>

#include "peripheral-loader.h"
#include "scm-pas.h"

#define MARM_BOOT_CONTROL		0x0010
#define MARM_RESET			(MSM_CLK_CTL_BASE + 0x2BD4)
#define MAHB0_SFAB_PORT_RESET		(MSM_CLK_CTL_BASE + 0x2304)
#define MARM_CLK_BRANCH_ENA_VOTE	(MSM_CLK_CTL_BASE + 0x3000)
#define MARM_CLK_SRC0_NS		(MSM_CLK_CTL_BASE + 0x2BC0)
#define MARM_CLK_SRC1_NS		(MSM_CLK_CTL_BASE + 0x2BC4)
#define MARM_CLK_SRC_CTL		(MSM_CLK_CTL_BASE + 0x2BC8)
#define MARM_CLK_CTL			(MSM_CLK_CTL_BASE + 0x2BCC)
#define SFAB_MSS_S_HCLK_CTL		(MSM_CLK_CTL_BASE + 0x2C00)
#define MSS_MODEM_CXO_CLK_CTL		(MSM_CLK_CTL_BASE + 0x2C44)
#define MSS_SLP_CLK_CTL			(MSM_CLK_CTL_BASE + 0x2C60)
#define MSS_MARM_SYS_REF_CLK_CTL	(MSM_CLK_CTL_BASE + 0x2C64)
#define MAHB0_CLK_CTL			(MSM_CLK_CTL_BASE + 0x2300)
#define MAHB1_CLK_CTL			(MSM_CLK_CTL_BASE + 0x2BE4)
#define MAHB2_CLK_CTL			(MSM_CLK_CTL_BASE + 0x2C20)
#define MAHB1_NS			(MSM_CLK_CTL_BASE + 0x2BE0)
#define MARM_CLK_FS			(MSM_CLK_CTL_BASE + 0x2BD0)
#define MAHB2_CLK_FS			(MSM_CLK_CTL_BASE + 0x2C24)
#define PLL_ENA_MARM			(MSM_CLK_CTL_BASE + 0x3500)
#define PLL8_STATUS			(MSM_CLK_CTL_BASE + 0x3158)
#define CLK_HALT_MSS_SMPSS_MISC_STATE	(MSM_CLK_CTL_BASE + 0x2FDC)

struct modem_data {
	void __iomem *base;
	unsigned long start_addr;
	struct pil_device *pil;
	struct clk *xo;
};

static int make_modem_proxy_votes(struct pil_desc *pil)
{
	int ret;
	struct modem_data *drv = dev_get_drvdata(pil->dev);

	ret = clk_prepare_enable(drv->xo);
	if (ret) {
		dev_err(pil->dev, "Failed to enable XO\n");
		return ret;
	}
	return 0;
}

static void remove_modem_proxy_votes(struct pil_desc *pil)
{
	struct modem_data *drv = dev_get_drvdata(pil->dev);
	clk_disable_unprepare(drv->xo);
}

static int modem_init_image(struct pil_desc *pil, const u8 *metadata,
		size_t size)
{
	const struct elf32_hdr *ehdr = (struct elf32_hdr *)metadata;
	struct modem_data *drv = dev_get_drvdata(pil->dev);
	drv->start_addr = ehdr->e_entry;
	return 0;
}

static int modem_reset(struct pil_desc *pil)
{
	u32 reg;
	const struct modem_data *drv = dev_get_drvdata(pil->dev);

	/* Put modem AHB0,1,2 clocks into reset */
	writel_relaxed(BIT(0) | BIT(1), MAHB0_SFAB_PORT_RESET);
	writel_relaxed(BIT(7), MAHB1_CLK_CTL);
	writel_relaxed(BIT(7), MAHB2_CLK_CTL);

	/* Vote for pll8 on behalf of the modem */
	reg = readl_relaxed(PLL_ENA_MARM);
	reg |= BIT(8);
	writel_relaxed(reg, PLL_ENA_MARM);

	/* Wait for PLL8 to enable */
	while (!(readl_relaxed(PLL8_STATUS) & BIT(16)))
		cpu_relax();

	/* Set MAHB1 divider to Div-5 to run MAHB1,2 and sfab at 79.8 Mhz*/
	writel_relaxed(0x4, MAHB1_NS);

	/* Vote for modem AHB1 and 2 clocks to be on on behalf of the modem */
	reg = readl_relaxed(MARM_CLK_BRANCH_ENA_VOTE);
	reg |= BIT(0) | BIT(1);
	writel_relaxed(reg, MARM_CLK_BRANCH_ENA_VOTE);

	/* Source marm_clk off of PLL8 */
	reg = readl_relaxed(MARM_CLK_SRC_CTL);
	if ((reg & 0x1) == 0) {
		writel_relaxed(0x3, MARM_CLK_SRC1_NS);
		reg |= 0x1;
	} else {
		writel_relaxed(0x3, MARM_CLK_SRC0_NS);
		reg &= ~0x1;
	}
	writel_relaxed(reg | 0x2, MARM_CLK_SRC_CTL);

	/*
	 * Force core on and periph on signals to remain active during halt
	 * for marm_clk and mahb2_clk
	 */
	writel_relaxed(0x6F, MARM_CLK_FS);
	writel_relaxed(0x6F, MAHB2_CLK_FS);

	/*
	 * Enable all of the marm_clk branches, cxo sourced marm branches,
	 * and sleep clock branches
	 */
	writel_relaxed(0x10, MARM_CLK_CTL);
	writel_relaxed(0x10, MAHB0_CLK_CTL);
	writel_relaxed(0x10, SFAB_MSS_S_HCLK_CTL);
	writel_relaxed(0x10, MSS_MODEM_CXO_CLK_CTL);
	writel_relaxed(0x10, MSS_SLP_CLK_CTL);
	writel_relaxed(0x10, MSS_MARM_SYS_REF_CLK_CTL);

	/* Wait for above clocks to be turned on */
	while (readl_relaxed(CLK_HALT_MSS_SMPSS_MISC_STATE) & (BIT(7) | BIT(8) |
				BIT(9) | BIT(10) | BIT(4) | BIT(6)))
		cpu_relax();

	/* Take MAHB0,1,2 clocks out of reset */
	writel_relaxed(0x0, MAHB2_CLK_CTL);
	writel_relaxed(0x0, MAHB1_CLK_CTL);
	writel_relaxed(0x0, MAHB0_SFAB_PORT_RESET);
	mb();

	/* Setup exception vector table base address */
	writel_relaxed(drv->start_addr | 0x1, drv->base + MARM_BOOT_CONTROL);

	/* Wait for vector table to be setup */
	mb();

	/* Bring modem out of reset */
	writel_relaxed(0x0, MARM_RESET);

	return 0;
}

static int modem_shutdown(struct pil_desc *pil)
{
	u32 reg;

	/* Put modem into reset */
	writel_relaxed(0x1, MARM_RESET);
	mb();

	/* Put modem AHB0,1,2 clocks into reset */
	writel_relaxed(BIT(0) | BIT(1), MAHB0_SFAB_PORT_RESET);
	writel_relaxed(BIT(7), MAHB1_CLK_CTL);
	writel_relaxed(BIT(7), MAHB2_CLK_CTL);
	mb();

	/*
	 * Disable all of the marm_clk branches, cxo sourced marm branches,
	 * and sleep clock branches
	 */
	writel_relaxed(0x0, MARM_CLK_CTL);
	writel_relaxed(0x0, MAHB0_CLK_CTL);
	writel_relaxed(0x0, SFAB_MSS_S_HCLK_CTL);
	writel_relaxed(0x0, MSS_MODEM_CXO_CLK_CTL);
	writel_relaxed(0x0, MSS_SLP_CLK_CTL);
	writel_relaxed(0x0, MSS_MARM_SYS_REF_CLK_CTL);

	/* Disable marm_clk */
	reg = readl_relaxed(MARM_CLK_SRC_CTL);
	reg &= ~0x2;
	writel_relaxed(reg, MARM_CLK_SRC_CTL);

	/* Clear modem's votes for ahb clocks */
	writel_relaxed(0x0, MARM_CLK_BRANCH_ENA_VOTE);

	/* Clear modem's votes for PLLs */
	writel_relaxed(0x0, PLL_ENA_MARM);

	return 0;
}

static struct pil_reset_ops pil_modem_ops = {
	.init_image = modem_init_image,
	.auth_and_reset = modem_reset,
	.shutdown = modem_shutdown,
	.proxy_vote = make_modem_proxy_votes,
	.proxy_unvote = remove_modem_proxy_votes,
};

static int modem_init_image_trusted(struct pil_desc *pil, const u8 *metadata,
		size_t size)
{
	return pas_init_image(PAS_MODEM, metadata, size);
}

static int modem_reset_trusted(struct pil_desc *pil)
{
	return pas_auth_and_reset(PAS_MODEM);
}

static int modem_shutdown_trusted(struct pil_desc *pil)
{
	return pas_shutdown(PAS_MODEM);
}

static struct pil_reset_ops pil_modem_ops_trusted = {
	.init_image = modem_init_image_trusted,
	.auth_and_reset = modem_reset_trusted,
	.shutdown = modem_shutdown_trusted,
	.proxy_vote = make_modem_proxy_votes,
	.proxy_unvote = remove_modem_proxy_votes,
};

static int __devinit pil_modem_driver_probe(struct platform_device *pdev)
{
	struct modem_data *drv;
	struct resource *res;
	struct pil_desc *desc;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -EINVAL;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;
	platform_set_drvdata(pdev, drv);

	drv->base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!drv->base)
		return -ENOMEM;

	drv->xo = devm_clk_get(&pdev->dev, "xo");
	if (IS_ERR(drv->xo))
		return PTR_ERR(drv->xo);

	desc = devm_kzalloc(&pdev->dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	desc->name = "modem";
	desc->depends_on = "q6";
	desc->dev = &pdev->dev;
	desc->owner = THIS_MODULE;
	desc->proxy_timeout = 10000;

	if (pas_supported(PAS_MODEM) > 0) {
		desc->ops = &pil_modem_ops_trusted;
		dev_info(&pdev->dev, "using secure boot\n");
	} else {
		desc->ops = &pil_modem_ops;
		dev_info(&pdev->dev, "using non-secure boot\n");
	}
	drv->pil = msm_pil_register(desc);
	if (IS_ERR(drv->pil)) {
		return PTR_ERR(drv->pil);
	}
	return 0;
}

static int __devexit pil_modem_driver_exit(struct platform_device *pdev)
{
	struct modem_data *drv = platform_get_drvdata(pdev);
	msm_pil_unregister(drv->pil);
	return 0;
}

static struct platform_driver pil_modem_driver = {
	.probe = pil_modem_driver_probe,
	.remove = __devexit_p(pil_modem_driver_exit),
	.driver = {
		.name = "pil_modem",
		.owner = THIS_MODULE,
	},
};

static int __init pil_modem_init(void)
{
	return platform_driver_register(&pil_modem_driver);
}
module_init(pil_modem_init);

static void __exit pil_modem_exit(void)
{
	platform_driver_unregister(&pil_modem_driver);
}
module_exit(pil_modem_exit);

MODULE_DESCRIPTION("Support for booting modem processors");
MODULE_LICENSE("GPL v2");
