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

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/elf.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/smp.h>

#include <mach/msm_iomap.h>
#include <mach/msm_xo.h>
#include <mach/socinfo.h>
#include <mach/msm_bus_board.h>
#include <mach/msm_bus.h>

#include "peripheral-loader.h"
#include "scm-pas.h"

#define GSS_CSR_AHB_CLK_SEL	0x0
#define GSS_CSR_RESET		0x4
#define GSS_CSR_CLK_BLK_CONFIG	0x8
#define GSS_CSR_CLK_ENABLE	0xC
#define GSS_CSR_BOOT_REMAP	0x14
#define GSS_CSR_POWER_UP_DOWN	0x18
#define GSS_CSR_CFG_HID		0x2C

#define GSS_SLP_CLK_CTL		(MSM_CLK_CTL_BASE + 0x2C60)
#define GSS_RESET		(MSM_CLK_CTL_BASE + 0x2C64)
#define GSS_CLAMP_ENA		(MSM_CLK_CTL_BASE + 0x2C68)
#define GSS_CXO_SRC_CTL		(MSM_CLK_CTL_BASE + 0x2C74)

#define PLL5_STATUS		(MSM_CLK_CTL_BASE + 0x30F8)
#define PLL_ENA_GSS		(MSM_CLK_CTL_BASE + 0x3480)

#define PLL5_VOTE		BIT(5)
#define PLL_STATUS		BIT(16)
#define REMAP_ENABLE		BIT(16)
#define A5_POWER_STATUS		BIT(4)
#define A5_POWER_ENA		BIT(0)
#define NAV_POWER_ENA		BIT(1)
#define XO_CLK_BRANCH_ENA	BIT(0)
#define SLP_CLK_BRANCH_ENA	BIT(4)
#define A5_RESET		BIT(0)

struct gss_data {
	void __iomem *base;
	void __iomem *qgic2_base;
	unsigned long start_addr;
	struct clk *xo;
	struct pil_device *pil;
};

static int pil_gss_init_image(struct pil_desc *pil, const u8 *metadata,
		size_t size)
{
	const struct elf32_hdr *ehdr = (struct elf32_hdr *)metadata;
	struct gss_data *drv = dev_get_drvdata(pil->dev);
	drv->start_addr = ehdr->e_entry;
	return 0;
}

static int make_gss_proxy_votes(struct pil_desc *pil)
{
	int ret;
	struct gss_data *drv = dev_get_drvdata(pil->dev);

	ret = clk_prepare_enable(drv->xo);
	if (ret) {
		dev_err(pil->dev, "Failed to enable XO\n");
		return ret;
	}
	return 0;
}

static void remove_gss_proxy_votes(struct pil_desc *pil)
{
	struct gss_data *drv = dev_get_drvdata(pil->dev);
	clk_disable_unprepare(drv->xo);
}

static void gss_init(struct gss_data *drv)
{
	void __iomem *base = drv->base;

	/* Supply clocks to GSS. */
	writel_relaxed(XO_CLK_BRANCH_ENA, GSS_CXO_SRC_CTL);
	writel_relaxed(SLP_CLK_BRANCH_ENA, GSS_SLP_CLK_CTL);

	/* Deassert GSS reset and clamps. */
	writel_relaxed(0x0, GSS_RESET);
	writel_relaxed(0x0, GSS_CLAMP_ENA);
	mb();

	/*
	 * Configure clock source and dividers for 288MHz core, 144MHz AXI and
	 * 72MHz AHB, all derived from the 288MHz PLL.
	 */
	writel_relaxed(0x341, base + GSS_CSR_CLK_BLK_CONFIG);
	writel_relaxed(0x1, base + GSS_CSR_AHB_CLK_SEL);

	/* Assert all GSS resets. */
	writel_relaxed(0x7F, base + GSS_CSR_RESET);

	/* Enable all bus clocks and wait for resets to propagate. */
	writel_relaxed(0x1F, base + GSS_CSR_CLK_ENABLE);
	mb();
	udelay(1);

	/* Release subsystem from reset, but leave A5 in reset. */
	writel_relaxed(A5_RESET, base + GSS_CSR_RESET);
}

static void cfg_qgic2_bus_access(void *data)
{
	struct gss_data *drv = data;
	int i;

	/*
	 * Apply a 8064 v1.0 workaround to configure QGIC bus access.
	 * This must be done from Krait 0 to configure the Master ID
	 * correctly.
	 */
	writel_relaxed(0x2, drv->base + GSS_CSR_CFG_HID);
	for (i = 0; i <= 3; i++)
		readl_relaxed(drv->qgic2_base);
}

static int pil_gss_shutdown(struct pil_desc *pil)
{
	struct gss_data *drv = dev_get_drvdata(pil->dev);
	void __iomem *base = drv->base;
	u32 regval;
	int ret;

	ret = clk_prepare_enable(drv->xo);
	if (ret) {
		dev_err(pil->dev, "Failed to enable XO\n");
		return ret;
	}

	/* Make sure bus port is halted. */
	msm_bus_axi_porthalt(MSM_BUS_MASTER_GSS_NAV);

	/*
	 * Vote PLL on in GSS's voting register and wait for it to enable.
	 * The PLL must be enable to switch the GFMUX to a low-power source.
	 */
	writel_relaxed(PLL5_VOTE, PLL_ENA_GSS);
	while ((readl_relaxed(PLL5_STATUS) & PLL_STATUS) == 0)
		cpu_relax();

	/* Perform one-time GSS initialization. */
	gss_init(drv);

	/* Assert A5 reset. */
	regval = readl_relaxed(base + GSS_CSR_RESET);
	regval |= A5_RESET;
	writel_relaxed(regval, base + GSS_CSR_RESET);

	/* Power down A5 and NAV. */
	regval = readl_relaxed(base + GSS_CSR_POWER_UP_DOWN);
	regval &= ~(A5_POWER_ENA|NAV_POWER_ENA);
	writel_relaxed(regval, base + GSS_CSR_POWER_UP_DOWN);

	/* Select XO clock source and increase dividers to save power. */
	regval = readl_relaxed(base + GSS_CSR_CLK_BLK_CONFIG);
	regval |= 0x3FF;
	writel_relaxed(regval, base + GSS_CSR_CLK_BLK_CONFIG);

	/* Disable bus clocks. */
	writel_relaxed(0x1F, base + GSS_CSR_CLK_ENABLE);

	/* Clear GSS PLL votes. */
	writel_relaxed(0, PLL_ENA_GSS);
	mb();

	clk_disable_unprepare(drv->xo);

	return 0;
}

static int pil_gss_reset(struct pil_desc *pil)
{
	struct gss_data *drv = dev_get_drvdata(pil->dev);
	void __iomem *base = drv->base;
	unsigned long start_addr = drv->start_addr;
	int ret;

	/* Unhalt bus port. */
	ret = msm_bus_axi_portunhalt(MSM_BUS_MASTER_GSS_NAV);
	if (ret) {
		dev_err(pil->dev, "Failed to unhalt bus port\n");
		return ret;
	}

	/* Vote PLL on in GSS's voting register and wait for it to enable. */
	writel_relaxed(PLL5_VOTE, PLL_ENA_GSS);
	while ((readl_relaxed(PLL5_STATUS) & PLL_STATUS) == 0)
		cpu_relax();

	/* Perform GSS initialization. */
	gss_init(drv);

	/* Configure boot address and enable remap. */
	writel_relaxed(REMAP_ENABLE | (start_addr >> 16),
			base + GSS_CSR_BOOT_REMAP);

	/* Power up A5 core. */
	writel_relaxed(A5_POWER_ENA, base + GSS_CSR_POWER_UP_DOWN);
	while (!(readl_relaxed(base + GSS_CSR_POWER_UP_DOWN) & A5_POWER_STATUS))
		cpu_relax();

	if (cpu_is_apq8064() &&
	    ((SOCINFO_VERSION_MAJOR(socinfo_get_version()) == 1) &&
	     (SOCINFO_VERSION_MINOR(socinfo_get_version()) == 0))) {
		ret = smp_call_function_single(0, cfg_qgic2_bus_access, drv, 1);
		if (ret) {
			pr_err("Failed to configure QGIC2 bus access\n");
			pil_gss_shutdown(pil);
			return ret;
		}
	}

	/* Release A5 from reset. */
	writel_relaxed(0x0, base + GSS_CSR_RESET);

	return 0;
}

static struct pil_reset_ops pil_gss_ops = {
	.init_image = pil_gss_init_image,
	.auth_and_reset = pil_gss_reset,
	.shutdown = pil_gss_shutdown,
	.proxy_vote = make_gss_proxy_votes,
	.proxy_unvote = remove_gss_proxy_votes,
};

static int pil_gss_init_image_trusted(struct pil_desc *pil,
		const u8 *metadata, size_t size)
{
	return pas_init_image(PAS_GSS, metadata, size);
}

static int pil_gss_shutdown_trusted(struct pil_desc *pil)
{
	struct gss_data *drv = dev_get_drvdata(pil->dev);
	int ret;

	/*
	 * CXO is used in the secure shutdown code to configure the processor
	 * for low power mode.
	 */
	ret = clk_prepare_enable(drv->xo);
	if (ret) {
		dev_err(pil->dev, "Failed to enable XO\n");
		return ret;
	}

	msm_bus_axi_porthalt(MSM_BUS_MASTER_GSS_NAV);
	ret = pas_shutdown(PAS_GSS);
	clk_disable_unprepare(drv->xo);

	return ret;
}

static int pil_gss_reset_trusted(struct pil_desc *pil)
{
	int err;

	err = msm_bus_axi_portunhalt(MSM_BUS_MASTER_GSS_NAV);
	if (err) {
		dev_err(pil->dev, "Failed to unhalt bus port\n");
		goto out;
	}

	err =  pas_auth_and_reset(PAS_GSS);
	if (err)
		goto halt_port;

	return 0;

halt_port:
	msm_bus_axi_porthalt(MSM_BUS_MASTER_GSS_NAV);
out:
	return err;
}

static struct pil_reset_ops pil_gss_ops_trusted = {
	.init_image = pil_gss_init_image_trusted,
	.auth_and_reset = pil_gss_reset_trusted,
	.shutdown = pil_gss_shutdown_trusted,
	.proxy_vote = make_gss_proxy_votes,
	.proxy_unvote = remove_gss_proxy_votes,
};

static int __devinit pil_gss_probe(struct platform_device *pdev)
{
	struct gss_data *drv;
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

	desc = devm_kzalloc(&pdev->dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res)
		return -EINVAL;

	drv->qgic2_base = devm_ioremap(&pdev->dev, res->start,
					resource_size(res));
	if (!drv->qgic2_base)
		return -ENOMEM;

	drv->xo = devm_clk_get(&pdev->dev, "xo");
	if (IS_ERR(drv->xo))
		return PTR_ERR(drv->xo);

	desc->name = "gss";
	desc->dev = &pdev->dev;
	desc->owner = THIS_MODULE;
	desc->proxy_timeout = 10000;

	if (pas_supported(PAS_GSS) > 0) {
		desc->ops = &pil_gss_ops_trusted;
		dev_info(&pdev->dev, "using secure boot\n");
	} else {
		desc->ops = &pil_gss_ops;
		dev_info(&pdev->dev, "using non-secure boot\n");
	}
	/* Force into low power mode because hardware doesn't do this */
	desc->ops->shutdown(desc);

	drv->pil = msm_pil_register(desc);
	if (IS_ERR(drv->pil)) {
		return PTR_ERR(drv->pil);
	}
	return 0;
}

static int __devexit pil_gss_remove(struct platform_device *pdev)
{
	struct gss_data *drv = platform_get_drvdata(pdev);
	msm_pil_unregister(drv->pil);
	return 0;
}

static struct platform_driver pil_gss_driver = {
	.probe = pil_gss_probe,
	.remove = __devexit_p(pil_gss_remove),
	.driver = {
		.name = "pil_gss",
		.owner = THIS_MODULE,
	},
};

static int __init pil_gss_init(void)
{
	return platform_driver_register(&pil_gss_driver);
}
module_init(pil_gss_init);

static void __exit pil_gss_exit(void)
{
	platform_driver_unregister(&pil_gss_driver);
}
module_exit(pil_gss_exit);

MODULE_DESCRIPTION("Support for booting the GSS processor");
MODULE_LICENSE("GPL v2");
