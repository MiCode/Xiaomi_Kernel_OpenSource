/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/clk.h>

#include <mach/msm_bus.h>

#include "peripheral-loader.h"
#include "pil-q6v4.h"
#include "scm-pas.h"

#define QDSP6SS_RST_EVB		0x0
#define QDSP6SS_RESET		0x04
#define QDSP6SS_STRAP_TCM	0x1C
#define QDSP6SS_STRAP_AHB	0x20
#define QDSP6SS_GFMUX_CTL	0x30
#define QDSP6SS_PWR_CTL		0x38

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

int pil_q6v4_make_proxy_votes(struct pil_desc *pil)
{
	const struct q6v4_data *drv = pil_to_q6v4_data(pil);
	int ret;

	ret = clk_prepare_enable(drv->xo);
	if (ret) {
		dev_err(pil->dev, "Failed to enable XO\n");
		goto err;
	}
	if (drv->pll_supply) {
		ret = regulator_enable(drv->pll_supply);
		if (ret) {
			dev_err(pil->dev, "Failed to enable pll supply\n");
			goto err_regulator;
		}
	}
	return 0;
err_regulator:
	clk_disable_unprepare(drv->xo);
err:
	return ret;
}
EXPORT_SYMBOL(pil_q6v4_make_proxy_votes);

void pil_q6v4_remove_proxy_votes(struct pil_desc *pil)
{
	const struct q6v4_data *drv = pil_to_q6v4_data(pil);
	if (drv->pll_supply)
		regulator_disable(drv->pll_supply);
	clk_disable_unprepare(drv->xo);
}
EXPORT_SYMBOL(pil_q6v4_remove_proxy_votes);

int pil_q6v4_power_up(struct q6v4_data *drv)
{
	int err;
	struct device *dev = drv->desc.dev;

	err = regulator_set_voltage(drv->vreg, 743750, 743750);
	if (err) {
		dev_err(dev, "Failed to set regulator's voltage step.\n");
		return err;
	}
	err = regulator_enable(drv->vreg);
	if (err) {
		dev_err(dev, "Failed to enable regulator.\n");
		return err;
	}

	/*
	 * Q6 hardware requires a two step voltage ramp-up.
	 * Delay between the steps.
	 */
	udelay(100);

	err = regulator_set_voltage(drv->vreg, 1050000, 1050000);
	if (err) {
		dev_err(dev, "Failed to set regulator's voltage.\n");
		return err;
	}
	drv->vreg_enabled = true;
	return 0;
}
EXPORT_SYMBOL(pil_q6v4_power_up);

void pil_q6v4_power_down(struct q6v4_data *drv)
{
	if (drv->vreg_enabled) {
		regulator_disable(drv->vreg);
		drv->vreg_enabled = false;
	}
}
EXPORT_SYMBOL(pil_q6v4_power_down);

int pil_q6v4_boot(struct pil_desc *pil)
{
	u32 reg, err;
	const struct q6v4_data *drv = pil_to_q6v4_data(pil);
	phys_addr_t start_addr = pil_get_entry_addr(pil);

	/* Enable Q6 ACLK */
	writel_relaxed(0x10, drv->aclk_reg);

	/* Unhalt bus port */
	err = msm_bus_axi_portunhalt(drv->bus_port);
	if (err)
		dev_err(pil->dev, "Failed to unhalt bus port\n");

	/* Deassert Q6SS_SS_ARES */
	reg = readl_relaxed(drv->base + QDSP6SS_RESET);
	reg &= ~(Q6SS_SS_ARES);
	writel_relaxed(reg, drv->base + QDSP6SS_RESET);

	/* Program boot address */
	writel_relaxed((start_addr >> 8) & 0xFFFFFF,
			drv->base + QDSP6SS_RST_EVB);

	/* Program TCM and AHB address ranges */
	writel_relaxed(drv->strap_tcm_base, drv->base + QDSP6SS_STRAP_TCM);
	writel_relaxed(drv->strap_ahb_upper | drv->strap_ahb_lower,
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

	return 0;
}
EXPORT_SYMBOL(pil_q6v4_boot);

int pil_q6v4_shutdown(struct pil_desc *pil)
{
	u32 reg;
	struct q6v4_data *drv = pil_to_q6v4_data(pil);

	/* Make sure bus port is halted */
	msm_bus_axi_porthalt(drv->bus_port);

	/* Turn off Q6 core clock */
	writel_relaxed(Q6SS_SRC_SWITCH_CLK_OVR,
		       drv->base + QDSP6SS_GFMUX_CTL);

	/* Assert resets */
	reg = (Q6SS_SS_ARES | Q6SS_CORE_ARES | Q6SS_ISDB_ARES
	     | Q6SS_ETM_ARES | Q6SS_STOP_CORE_ARES | Q6SS_PRIV_ARES);
	writel_relaxed(reg, drv->base + QDSP6SS_RESET);

	/* Turn off Q6 memories */
	writel_relaxed(Q6SS_CLAMP_IO, drv->base + QDSP6SS_PWR_CTL);

	return 0;
}
EXPORT_SYMBOL(pil_q6v4_shutdown);

int pil_q6v4_init_image_trusted(struct pil_desc *pil,
		const u8 *metadata, size_t size)
{
	struct q6v4_data *drv = pil_to_q6v4_data(pil);
	return pas_init_image(drv->pas_id, metadata, size);
}
EXPORT_SYMBOL(pil_q6v4_init_image_trusted);

int pil_q6v4_boot_trusted(struct pil_desc *pil)
{
	struct q6v4_data *drv = pil_to_q6v4_data(pil);
	int err;

	err = pil_q6v4_power_up(drv);
	if (err)
		return err;

	/* Unhalt bus port */
	err = msm_bus_axi_portunhalt(drv->bus_port);
	if (err)
		dev_err(pil->dev, "Failed to unhalt bus port\n");
	return pas_auth_and_reset(drv->pas_id);
}
EXPORT_SYMBOL(pil_q6v4_boot_trusted);

int pil_q6v4_shutdown_trusted(struct pil_desc *pil)
{
	int ret;
	struct q6v4_data *drv = pil_to_q6v4_data(pil);

	/* Make sure bus port is halted */
	msm_bus_axi_porthalt(drv->bus_port);

	ret = pas_shutdown(drv->pas_id);
	if (ret)
		return ret;

	pil_q6v4_power_down(drv);

	return ret;
}
EXPORT_SYMBOL(pil_q6v4_shutdown_trusted);

void __devinit
pil_q6v4_init(struct q6v4_data *drv, const struct pil_q6v4_pdata *pdata)
{
	drv->strap_tcm_base	= pdata->strap_tcm_base;
	drv->strap_ahb_upper	= pdata->strap_ahb_upper;
	drv->strap_ahb_lower	= pdata->strap_ahb_lower;
	drv->aclk_reg		= pdata->aclk_reg;
	drv->jtag_clk_reg	= pdata->jtag_clk_reg;
	drv->pas_id		= pdata->pas_id;
	drv->bus_port		= pdata->bus_port;

	regulator_set_optimum_mode(drv->vreg, 100000);
}
EXPORT_SYMBOL(pil_q6v4_init);

MODULE_DESCRIPTION("Support for booting QDSP6v4 (Hexagon) processors");
MODULE_LICENSE("GPL v2");
