/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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

#include <mach/msm_iomap.h>
#include <mach/msm_xo.h>

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

#define PROXY_VOTE_TIMEOUT		10000

struct modem_data {
	void __iomem *base;
	unsigned long start_addr;
	struct msm_xo_voter *pxo;
	struct timer_list timer;
};

static int nop_verify_blob(struct pil_desc *pil, u32 phy_addr, size_t size)
{
	return 0;
}

static void remove_proxy_votes(unsigned long data)
{
	struct modem_data *drv = (struct modem_data *)data;
	msm_xo_mode_vote(drv->pxo, MSM_XO_MODE_OFF);
}

static void make_modem_proxy_votes(struct device *dev)
{
	int ret;
	struct modem_data *drv = dev_get_drvdata(dev);

	ret = msm_xo_mode_vote(drv->pxo, MSM_XO_MODE_ON);
	if (ret)
		dev_err(dev, "Failed to enable PXO\n");
	mod_timer(&drv->timer, jiffies + msecs_to_jiffies(PROXY_VOTE_TIMEOUT));
}

static void remove_modem_proxy_votes_now(struct modem_data *drv)
{
	/* If the proxy vote hasn't been removed yet, remove it immediately. */
	if (del_timer(&drv->timer))
		remove_proxy_votes((unsigned long)drv);
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

	make_modem_proxy_votes(pil->dev);

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
	struct modem_data *drv = dev_get_drvdata(pil->dev);

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

	remove_modem_proxy_votes_now(drv);

	return 0;
}

static struct pil_reset_ops pil_modem_ops = {
	.init_image = modem_init_image,
	.verify_blob = nop_verify_blob,
	.auth_and_reset = modem_reset,
	.shutdown = modem_shutdown,
};

static int modem_init_image_trusted(struct pil_desc *pil, const u8 *metadata,
		size_t size)
{
	return pas_init_image(PAS_MODEM, metadata, size);
}

static int modem_reset_trusted(struct pil_desc *pil)
{
	int ret;
	struct modem_data *drv = dev_get_drvdata(pil->dev);

	make_modem_proxy_votes(pil->dev);

	ret = pas_auth_and_reset(PAS_MODEM);
	if (ret)
		remove_modem_proxy_votes_now(drv);

	return ret;
}

static int modem_shutdown_trusted(struct pil_desc *pil)
{
	int ret;
	struct modem_data *drv = dev_get_drvdata(pil->dev);

	ret = pas_shutdown(PAS_MODEM);
	if (ret)
		return ret;

	remove_modem_proxy_votes_now(drv);
	return 0;
}

static struct pil_reset_ops pil_modem_ops_trusted = {
	.init_image = modem_init_image_trusted,
	.verify_blob = nop_verify_blob,
	.auth_and_reset = modem_reset_trusted,
	.shutdown = modem_shutdown_trusted,
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

	drv->pxo = msm_xo_get(MSM_XO_PXO, dev_name(&pdev->dev));
	if (IS_ERR(drv->pxo))
		return PTR_ERR(drv->pxo);

	desc = devm_kzalloc(&pdev->dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	setup_timer(&drv->timer, remove_proxy_votes, (unsigned long)drv);
	desc->name = "modem";
	desc->depends_on = "q6";
	desc->dev = &pdev->dev;

	if (pas_supported(PAS_MODEM) > 0) {
		desc->ops = &pil_modem_ops_trusted;
		dev_info(&pdev->dev, "using secure boot\n");
	} else {
		desc->ops = &pil_modem_ops;
		dev_info(&pdev->dev, "using non-secure boot\n");
	}

	if (msm_pil_register(desc)) {
		msm_xo_put(drv->pxo);
		return -EINVAL;
	}
	return 0;
}

static int __devexit pil_modem_driver_exit(struct platform_device *pdev)
{
	struct modem_data *drv = platform_get_drvdata(pdev);
	del_timer_sync(&drv->timer);
	msm_xo_put(drv->pxo);
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
