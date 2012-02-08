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
#include <linux/workqueue.h>
#include <linux/clk.h>
#include <linux/smp.h>

#include <mach/msm_iomap.h>
#include <mach/msm_xo.h>

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

#define PLL5_MODE		(MSM_CLK_CTL_BASE + 0x30E0)
#define PLL5_L_VAL		(MSM_CLK_CTL_BASE + 0x30E4)
#define PLL5_M_VAL		(MSM_CLK_CTL_BASE + 0x30E8)
#define PLL5_N_VAL		(MSM_CLK_CTL_BASE + 0x30EC)
#define PLL5_CONFIG		(MSM_CLK_CTL_BASE + 0x30F4)
#define PLL5_STATUS		(MSM_CLK_CTL_BASE + 0x30F8)
#define PLL_ENA_GSS		(MSM_CLK_CTL_BASE + 0x3480)
#define PLL_ENA_RPM		(MSM_CLK_CTL_BASE + 0x34A0)

#define PLL5_VOTE		BIT(5)
#define PLL_STATUS		BIT(16)
#define REMAP_ENABLE		BIT(16)
#define A5_POWER_STATUS		BIT(4)
#define A5_POWER_ENA		BIT(0)
#define NAV_POWER_ENA		BIT(1)
#define XO_CLK_BRANCH_ENA	BIT(0)
#define SLP_CLK_BRANCH_ENA	BIT(4)
#define A5_RESET		BIT(0)

#define PROXY_VOTE_TIMEOUT	10000

struct gss_data {
	void __iomem *base;
	void __iomem *qgic2_base;
	unsigned long start_addr;
	struct delayed_work work;
	struct clk *xo;
};

static int nop_verify_blob(struct pil_desc *pil, u32 phy_addr, size_t size)
{
	return 0;
}

static int pil_gss_init_image(struct pil_desc *pil, const u8 *metadata,
		size_t size)
{
	const struct elf32_hdr *ehdr = (struct elf32_hdr *)metadata;
	struct gss_data *drv = dev_get_drvdata(pil->dev);
	drv->start_addr = ehdr->e_entry;
	return 0;
}

static int make_gss_proxy_votes(struct device *dev)
{
	int ret;
	struct gss_data *drv = dev_get_drvdata(dev);

	ret = clk_prepare_enable(drv->xo);
	if (ret) {
		dev_err(dev, "Failed to enable XO\n");
		return ret;
	}
	schedule_delayed_work(&drv->work, msecs_to_jiffies(PROXY_VOTE_TIMEOUT));
	return 0;
}

static void remove_gss_proxy_votes(struct work_struct *work)
{
	struct gss_data *drv = container_of(work, struct gss_data, work.work);
	clk_disable_unprepare(drv->xo);
}

static void remove_gss_proxy_votes_now(struct gss_data *drv)
{
	flush_delayed_work(&drv->work);
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

static void setup_qgic2_bus_access(void *data)
{
	struct gss_data *drv = data;
	void __iomem *base = drv->base;
	int i;

	writel_relaxed(0x2, base + GSS_CSR_CFG_HID);
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
	remove_gss_proxy_votes_now(drv);

	return 0;
}

static int pil_gss_reset(struct pil_desc *pil)
{
	struct gss_data *drv = dev_get_drvdata(pil->dev);
	void __iomem *base = drv->base;
	unsigned long start_addr = drv->start_addr;
	int ret;

	ret = make_gss_proxy_votes(pil->dev);
	if (ret)
		return ret;

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

	/*
	 * Apply a 8064 v1.0 workaround to configure QGIC bus access. This must
	 * be done from Krait 0 to configure the Master ID correctly.
	 */
	ret = smp_call_function_single(0, setup_qgic2_bus_access, drv, 1);
	if (ret) {
		pr_err("Failed to configure QGIC2 bus access\n");
		pil_gss_shutdown(pil);
		return ret;
	}

	/* Release A5 from reset. */
	writel_relaxed(0x0, base + GSS_CSR_RESET);

	return 0;
}

static struct pil_reset_ops pil_gss_ops = {
	.init_image = pil_gss_init_image,
	.verify_blob = nop_verify_blob,
	.auth_and_reset = pil_gss_reset,
	.shutdown = pil_gss_shutdown,
};

static int pil_gss_init_image_trusted(struct pil_desc *pil,
		const u8 *metadata, size_t size)
{
	return pas_init_image(PAS_GSS, metadata, size);
}

static int pil_gss_reset_trusted(struct pil_desc *pil)
{
	struct gss_data *drv = dev_get_drvdata(pil->dev);
	int err;

	err = make_gss_proxy_votes(pil->dev);
	if (err)
		return err;

	err =  pas_auth_and_reset(PAS_GSS);
	if (err)
		remove_gss_proxy_votes_now(drv);

	return err;
}

static int pil_gss_shutdown_trusted(struct pil_desc *pil)
{
	struct gss_data *drv = dev_get_drvdata(pil->dev);
	int ret;

	ret = pas_shutdown(PAS_GSS);
	if (ret)
		return ret;

	remove_gss_proxy_votes_now(drv);

	return ret;
}

static struct pil_reset_ops pil_gss_ops_trusted = {
	.init_image = pil_gss_init_image_trusted,
	.verify_blob = nop_verify_blob,
	.auth_and_reset = pil_gss_reset_trusted,
	.shutdown = pil_gss_shutdown_trusted,
};

static void configure_gss_pll(struct gss_data *drv)
{
	u32 regval, is_pll_enabled;

	/* Check if PLL5 is enabled by FSM. */
	is_pll_enabled = readl_relaxed(PLL5_STATUS) & PLL_STATUS;
	if (!is_pll_enabled) {
		/* Enable XO reference for PLL5 */
		clk_prepare_enable(drv->xo);

		/*
		 * Assert a vote to hold PLL5 on in RPM register until other
		 * voters are in place.
		 */
		regval = readl_relaxed(PLL_ENA_RPM);
		regval |= PLL5_VOTE;
		writel_relaxed(regval, PLL_ENA_RPM);

		/* Ref clk = 27MHz and program pll5 to 288MHz */
		writel_relaxed(0xF, PLL5_L_VAL);
		writel_relaxed(0x0, PLL5_M_VAL);
		writel_relaxed(0x1, PLL5_N_VAL);

		regval = readl_relaxed(PLL5_CONFIG);
		/* Disable the MN accumulator and enable the main output. */
		regval &= ~BIT(22);
		regval |= BIT(23);

		/* Set pre-divider and post-divider values to 1 and 1 */
		regval &= ~BIT(19);
		regval &= ~(BIT(21)|BIT(20));

		/* Set VCO frequency */
		regval &= ~(BIT(17)|BIT(16));
		writel_relaxed(regval, PLL5_CONFIG);

		regval = readl_relaxed(PLL5_MODE);
		/* De-assert reset to FSM */
		regval &= ~BIT(21);
		writel_relaxed(regval, PLL5_MODE);

		/* Program bias count */
		regval &= ~(0x3F << 14);
		regval |= (0x1 << 14);
		writel_relaxed(regval, PLL5_MODE);

		/* Program lock count */
		regval &= ~(0x3F << 8);
		regval |= (0x8 << 8);
		writel_relaxed(regval, PLL5_MODE);

		/* Enable PLL FSM voting */
		regval |= BIT(20);
		writel_relaxed(regval, PLL5_MODE);
	}
}

static int __devinit pil_gss_probe(struct platform_device *pdev)
{
	struct gss_data *drv;
	struct resource *res;
	struct pil_desc *desc;
	int ret;

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

	drv->xo = clk_get(&pdev->dev, "xo");
	if (IS_ERR(drv->xo))
		return PTR_ERR(drv->xo);

	desc->name = "gss";
	desc->dev = &pdev->dev;

	if (pas_supported(PAS_GSS) > 0) {
		desc->ops = &pil_gss_ops_trusted;
		dev_info(&pdev->dev, "using secure boot\n");
	} else {
		desc->ops = &pil_gss_ops;
		dev_info(&pdev->dev, "using non-secure boot\n");
	}

	INIT_DELAYED_WORK(&drv->work, remove_gss_proxy_votes);

	/* FIXME: Remove when PLL is configured by bootloaders. */
	configure_gss_pll(drv);

	ret = msm_pil_register(desc);
	if (ret) {
		flush_delayed_work_sync(&drv->work);
		clk_put(drv->xo);
	}
	return ret;
}

static int __devexit pil_gss_remove(struct platform_device *pdev)
{
	struct gss_data *drv = platform_get_drvdata(pdev);
	flush_delayed_work_sync(&drv->work);
	clk_put(drv->xo);
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
