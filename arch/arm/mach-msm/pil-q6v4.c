/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
#include <linux/regulator/consumer.h>
#include <linux/elf.h>
#include <linux/delay.h>
#include <linux/err.h>

#include <mach/msm_bus.h>
#include <mach/msm_iomap.h>
#include <mach/msm_xo.h>

#include "peripheral-loader.h"
#include "pil-q6v4.h"
#include "scm-pas.h"

#define PROXY_VOTE_TIMEOUT	10000

#define QDSP6SS_RST_EVB		0x0
#define QDSP6SS_RESET		0x04
#define QDSP6SS_CGC_OVERRIDE	0x18
#define QDSP6SS_STRAP_TCM	0x1C
#define QDSP6SS_STRAP_AHB	0x20
#define QDSP6SS_GFMUX_CTL	0x30
#define QDSP6SS_PWR_CTL		0x38

#define MSS_S_HCLK_CTL		(MSM_CLK_CTL_BASE + 0x2C70)
#define MSS_SLP_CLK_CTL		(MSM_CLK_CTL_BASE + 0x2C60)
#define SFAB_MSS_M_ACLK_CTL	(MSM_CLK_CTL_BASE + 0x2340)
#define SFAB_MSS_S_HCLK_CTL	(MSM_CLK_CTL_BASE + 0x2C00)
#define MSS_RESET		(MSM_CLK_CTL_BASE + 0x2C64)

#define Q6SS_SS_ARES		BIT(0)
#define Q6SS_CORE_ARES		BIT(1)
#define Q6SS_ISDB_ARES		BIT(2)
#define Q6SS_ETM_ARES		BIT(3)
#define Q6SS_STOP_CORE_ARES	BIT(4)
#define Q6SS_PRIV_ARES		BIT(5)

#define Q6SS_L2DATA_SLP_NRET_N	BIT(0)
#define Q6SS_SLP_RET_N		BIT(1)
#define Q6SS_L1TCM_SLP_NRET_N	BIT(2)
#define Q6SS_L2TAG_SLP_NRET_N	BIT(3)
#define Q6SS_ETB_SLEEP_NRET_N	BIT(4)
#define Q6SS_ARR_STBY_N		BIT(5)
#define Q6SS_CLAMP_IO		BIT(6)

#define Q6SS_CLK_ENA		BIT(1)
#define Q6SS_SRC_SWITCH_CLK_OVR	BIT(8)
#define Q6SS_AXIS_ACLK_EN	BIT(9)

struct q6v4_data {
	void __iomem *base;
	void __iomem *modem_base;
	unsigned long start_addr;
	struct regulator *vreg;
	bool vreg_enabled;
	struct msm_xo_voter *xo;
	struct timer_list xo_timer;
};

static int pil_q6v4_init_image(struct pil_desc *pil, const u8 *metadata,
		size_t size)
{
	const struct elf32_hdr *ehdr = (struct elf32_hdr *)metadata;
	struct q6v4_data *drv = dev_get_drvdata(pil->dev);
	drv->start_addr = ehdr->e_entry;
	return 0;
}

static int nop_verify_blob(struct pil_desc *pil, u32 phy_addr, size_t size)
{
	return 0;
}

static void pil_q6v4_make_xo_proxy_votes(struct device *dev)
{
	struct q6v4_data *drv = dev_get_drvdata(dev);

	msm_xo_mode_vote(drv->xo, MSM_XO_MODE_ON);
	mod_timer(&drv->xo_timer, jiffies+msecs_to_jiffies(PROXY_VOTE_TIMEOUT));
}

static void pil_q6v4_remove_xo_proxy_votes(unsigned long data)
{
	struct q6v4_data *drv = (struct q6v4_data *)data;

	msm_xo_mode_vote(drv->xo, MSM_XO_MODE_OFF);
}

static void pil_q6v4_remove_xo_proxy_votes_now(struct device *dev)
{
	struct q6v4_data *drv = dev_get_drvdata(dev);

	if (del_timer(&drv->xo_timer))
		pil_q6v4_remove_xo_proxy_votes((unsigned long)drv);
}

static int pil_q6v4_power_up(struct device *dev)
{
	int err;
	struct q6v4_data *drv = dev_get_drvdata(dev);

	err = regulator_set_voltage(drv->vreg, 1050000, 1050000);
	if (err) {
		dev_err(dev, "Failed to set regulator's voltage.\n");
		return err;
	}
	err = regulator_set_optimum_mode(drv->vreg, 100000);
	if (err < 0) {
		dev_err(dev, "Failed to set regulator's mode.\n");
		return err;
	}
	err = regulator_enable(drv->vreg);
	if (err) {
		dev_err(dev, "Failed to enable regulator.\n");
		return err;
	}
	drv->vreg_enabled = true;
	return 0;
}

static DEFINE_MUTEX(pil_q6v4_modem_lock);
static unsigned pil_q6v4_modem_count;

/* Bring modem subsystem out of reset */
static void pil_q6v4_init_modem(void __iomem *base, void __iomem *jtag_clk)
{
	mutex_lock(&pil_q6v4_modem_lock);
	if (!pil_q6v4_modem_count) {
		/* Enable MSS clocks */
		writel_relaxed(0x10, SFAB_MSS_M_ACLK_CTL);
		writel_relaxed(0x10, SFAB_MSS_S_HCLK_CTL);
		writel_relaxed(0x10, MSS_S_HCLK_CTL);
		writel_relaxed(0x10, MSS_SLP_CLK_CTL);
		/* Wait for clocks to enable */
		mb();
		udelay(10);

		/* De-assert MSS reset */
		writel_relaxed(0x0, MSS_RESET);
		mb();
		udelay(10);
		/* Enable MSS */
		writel_relaxed(0x7, base);
	}

	/* Enable JTAG clocks */
	/* TODO: Remove if/when Q6 software enables them? */
	writel_relaxed(0x10, jtag_clk);

	pil_q6v4_modem_count++;
	mutex_unlock(&pil_q6v4_modem_lock);
}

/* Put modem subsystem back into reset */
static void pil_q6v4_shutdown_modem(void)
{
	mutex_lock(&pil_q6v4_modem_lock);
	if (pil_q6v4_modem_count)
		pil_q6v4_modem_count--;
	if (pil_q6v4_modem_count == 0)
		writel_relaxed(0x1, MSS_RESET);
	mutex_unlock(&pil_q6v4_modem_lock);
}

static int pil_q6v4_reset(struct pil_desc *pil)
{
	u32 reg, err = 0;
	const struct q6v4_data *drv = dev_get_drvdata(pil->dev);
	const struct pil_q6v4_pdata *pdata = pil->dev->platform_data;

	pil_q6v4_make_xo_proxy_votes(pil->dev);

	err = pil_q6v4_power_up(pil->dev);
	if (err)
		return err;
	/* Enable Q6 ACLK */
	writel_relaxed(0x10, pdata->aclk_reg);

	if (drv->modem_base)
		pil_q6v4_init_modem(drv->modem_base, pdata->jtag_clk_reg);

	/* Unhalt bus port */
	err = msm_bus_axi_portunhalt(pdata->bus_port);
	if (err)
		dev_err(pil->dev, "Failed to unhalt bus port\n");

	/*
	 * Assert AXIS_ACLK_EN override to allow for correct updating of the
	 * QDSP6_CORE_STATE status bit. This is mandatory only for the SW Q6
	 * in 8960v1 and optional elsewhere.
	 */
	reg = readl_relaxed(drv->base + QDSP6SS_CGC_OVERRIDE);
	reg |= Q6SS_AXIS_ACLK_EN;
	writel_relaxed(reg, drv->base + QDSP6SS_CGC_OVERRIDE);

	/* Deassert Q6SS_SS_ARES */
	reg = readl_relaxed(drv->base + QDSP6SS_RESET);
	reg &= ~(Q6SS_SS_ARES);
	writel_relaxed(reg, drv->base + QDSP6SS_RESET);

	/* Program boot address */
	writel_relaxed((drv->start_addr >> 8) & 0xFFFFFF,
			drv->base + QDSP6SS_RST_EVB);

	/* Program TCM and AHB address ranges */
	writel_relaxed(pdata->strap_tcm_base, drv->base + QDSP6SS_STRAP_TCM);
	writel_relaxed(pdata->strap_ahb_upper | pdata->strap_ahb_lower,
		       drv->base + QDSP6SS_STRAP_AHB);

	/* Turn off Q6 core clock */
	writel_relaxed(Q6SS_SRC_SWITCH_CLK_OVR,
		       drv->base + QDSP6SS_GFMUX_CTL);

	/* Put memories to sleep */
	writel_relaxed(Q6SS_CLAMP_IO, drv->base + QDSP6SS_PWR_CTL);

	/* Assert resets */
	reg = readl_relaxed(drv->base + QDSP6SS_RESET);
	reg |= (Q6SS_CORE_ARES | Q6SS_ISDB_ARES | Q6SS_ETM_ARES
	    | Q6SS_STOP_CORE_ARES);
	writel_relaxed(reg, drv->base + QDSP6SS_RESET);

	/* Wait 8 AHB cycles for Q6 to be fully reset (AHB = 1.5Mhz) */
	mb();
	usleep_range(20, 30);

	/* Turn on Q6 memories */
	reg = Q6SS_L2DATA_SLP_NRET_N | Q6SS_SLP_RET_N | Q6SS_L1TCM_SLP_NRET_N
	    | Q6SS_L2TAG_SLP_NRET_N | Q6SS_ETB_SLEEP_NRET_N | Q6SS_ARR_STBY_N
	    | Q6SS_CLAMP_IO;
	writel_relaxed(reg, drv->base + QDSP6SS_PWR_CTL);

	/* Turn on Q6 core clock */
	reg = Q6SS_CLK_ENA | Q6SS_SRC_SWITCH_CLK_OVR;
	writel_relaxed(reg, drv->base + QDSP6SS_GFMUX_CTL);

	/* Remove Q6SS_CLAMP_IO */
	reg = readl_relaxed(drv->base + QDSP6SS_PWR_CTL);
	reg &= ~Q6SS_CLAMP_IO;
	writel_relaxed(reg, drv->base + QDSP6SS_PWR_CTL);

	/* Bring Q6 core out of reset and start execution. */
	writel_relaxed(0x0, drv->base + QDSP6SS_RESET);

	/*
	 * Re-enable auto-gating of AXIS_ACLK at lease one AXI clock cycle
	 * after resets are de-asserted.
	 */
	mb();
	usleep_range(1, 10);
	reg = readl_relaxed(drv->base + QDSP6SS_CGC_OVERRIDE);
	reg &= ~Q6SS_AXIS_ACLK_EN;
	writel_relaxed(reg, drv->base + QDSP6SS_CGC_OVERRIDE);

	return 0;
}

static int pil_q6v4_shutdown(struct pil_desc *pil)
{
	u32 reg;
	struct q6v4_data *drv = dev_get_drvdata(pil->dev);
	const struct pil_q6v4_pdata *pdata = pil->dev->platform_data;

	/* Make sure bus port is halted */
	msm_bus_axi_porthalt(pdata->bus_port);

	/* Turn off Q6 core clock */
	writel_relaxed(Q6SS_SRC_SWITCH_CLK_OVR,
		       drv->base + QDSP6SS_GFMUX_CTL);

	/* Assert resets */
	reg = (Q6SS_SS_ARES | Q6SS_CORE_ARES | Q6SS_ISDB_ARES
	     | Q6SS_ETM_ARES | Q6SS_STOP_CORE_ARES | Q6SS_PRIV_ARES);
	writel_relaxed(reg, drv->base + QDSP6SS_RESET);

	/* Turn off Q6 memories */
	writel_relaxed(Q6SS_CLAMP_IO, drv->base + QDSP6SS_PWR_CTL);

	if (drv->modem_base)
		pil_q6v4_shutdown_modem();

	if (drv->vreg_enabled) {
		regulator_disable(drv->vreg);
		drv->vreg_enabled = false;
	}

	pil_q6v4_remove_xo_proxy_votes_now(pil->dev);

	return 0;
}

static struct pil_reset_ops pil_q6v4_ops = {
	.init_image = pil_q6v4_init_image,
	.verify_blob = nop_verify_blob,
	.auth_and_reset = pil_q6v4_reset,
	.shutdown = pil_q6v4_shutdown,
};

static int pil_q6v4_init_image_trusted(struct pil_desc *pil,
		const u8 *metadata, size_t size)
{
	const struct pil_q6v4_pdata *pdata = pil->dev->platform_data;
	return pas_init_image(pdata->pas_id, metadata, size);
}

static int pil_q6v4_reset_trusted(struct pil_desc *pil)
{
	const struct pil_q6v4_pdata *pdata = pil->dev->platform_data;
	int err;

	pil_q6v4_make_xo_proxy_votes(pil->dev);

	err = pil_q6v4_power_up(pil->dev);
	if (err)
		return err;

	/* Unhalt bus port */
	err = msm_bus_axi_portunhalt(pdata->bus_port);
	if (err)
		dev_err(pil->dev, "Failed to unhalt bus port\n");
	return pas_auth_and_reset(pdata->pas_id);
}

static int pil_q6v4_shutdown_trusted(struct pil_desc *pil)
{
	int ret;
	struct q6v4_data *drv = dev_get_drvdata(pil->dev);
	struct pil_q6v4_pdata *pdata = pil->dev->platform_data;

	/* Make sure bus port is halted */
	msm_bus_axi_porthalt(pdata->bus_port);

	ret = pas_shutdown(pdata->pas_id);
	if (ret)
		return ret;

	if (drv->vreg_enabled) {
		regulator_disable(drv->vreg);
		drv->vreg_enabled = false;
	}

	pil_q6v4_remove_xo_proxy_votes_now(pil->dev);

	return ret;
}

static struct pil_reset_ops pil_q6v4_ops_trusted = {
	.init_image = pil_q6v4_init_image_trusted,
	.verify_blob = nop_verify_blob,
	.auth_and_reset = pil_q6v4_reset_trusted,
	.shutdown = pil_q6v4_shutdown_trusted,
};

static int __devinit pil_q6v4_driver_probe(struct platform_device *pdev)
{
	const struct pil_q6v4_pdata *pdata = pdev->dev.platform_data;
	struct q6v4_data *drv;
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

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res) {
		drv->modem_base = devm_ioremap(&pdev->dev, res->start,
				resource_size(res));
		if (!drv->modem_base)
			return -ENOMEM;
	}

	desc = devm_kzalloc(&pdev->dev, sizeof(*desc), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	desc->name = pdata->name;
	desc->depends_on = pdata->depends;
	desc->dev = &pdev->dev;

	if (pas_supported(pdata->pas_id) > 0) {
		desc->ops = &pil_q6v4_ops_trusted;
		dev_info(&pdev->dev, "using secure boot\n");
	} else {
		desc->ops = &pil_q6v4_ops;
		dev_info(&pdev->dev, "using non-secure boot\n");
	}

	drv->vreg = regulator_get(&pdev->dev, "core_vdd");
	if (IS_ERR(drv->vreg))
		return PTR_ERR(drv->vreg);

	setup_timer(&drv->xo_timer, pil_q6v4_remove_xo_proxy_votes,
		    (unsigned long)drv);
	drv->xo = msm_xo_get(pdata->xo_id, pdata->name);
	if (IS_ERR(drv->xo))
		return PTR_ERR(drv->xo);

	if (msm_pil_register(desc)) {
		regulator_put(drv->vreg);
		return -EINVAL;
	}
	return 0;
}

static int __devexit pil_q6v4_driver_exit(struct platform_device *pdev)
{
	struct q6v4_data *drv = platform_get_drvdata(pdev);
	regulator_put(drv->vreg);
	return 0;
}

static struct platform_driver pil_q6v4_driver = {
	.probe = pil_q6v4_driver_probe,
	.remove = __devexit_p(pil_q6v4_driver_exit),
	.driver = {
		.name = "pil_qdsp6v4",
		.owner = THIS_MODULE,
	},
};

static int __init pil_q6v4_init(void)
{
	return platform_driver_register(&pil_q6v4_driver);
}
module_init(pil_q6v4_init);

static void __exit pil_q6v4_exit(void)
{
	platform_driver_unregister(&pil_q6v4_driver);
}
module_exit(pil_q6v4_exit);

MODULE_DESCRIPTION("Support for booting QDSP6v4 (Hexagon) processors");
MODULE_LICENSE("GPL v2");
