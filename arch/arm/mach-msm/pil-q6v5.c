/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/err.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <mach/rpm-regulator-smd.h>
#include <mach/clk.h>
#include "peripheral-loader.h"
#include "pil-q6v5.h"

/* QDSP6SS Register Offsets */
#define QDSP6SS_RESET			0x014
#define QDSP6SS_GFMUX_CTL		0x020
#define QDSP6SS_PWR_CTL			0x030

/* AXI Halt Register Offsets */
#define AXI_HALTREQ			0x0
#define AXI_HALTACK			0x4
#define AXI_IDLE			0x8

#define HALT_ACK_TIMEOUT_US		100000

/* QDSP6SS_RESET */
#define Q6SS_STOP_CORE			BIT(0)
#define Q6SS_CORE_ARES			BIT(1)
#define Q6SS_BUS_ARES_ENA		BIT(2)

/* QDSP6SS_GFMUX_CTL */
#define Q6SS_CLK_ENA			BIT(1)

/* QDSP6SS_PWR_CTL */
#define Q6SS_L2DATA_SLP_NRET_N_0	BIT(0)
#define Q6SS_L2DATA_SLP_NRET_N_1	BIT(1)
#define Q6SS_L2DATA_SLP_NRET_N_2	BIT(2)
#define Q6SS_L2TAG_SLP_NRET_N		BIT(16)
#define Q6SS_ETB_SLP_NRET_N		BIT(17)
#define Q6SS_L2DATA_STBY_N		BIT(18)
#define Q6SS_SLP_RET_N			BIT(19)
#define Q6SS_CLAMP_IO			BIT(20)
#define QDSS_BHS_ON			BIT(21)
#define QDSS_LDO_BYP			BIT(22)

int pil_q6v5_make_proxy_votes(struct pil_desc *pil)
{
	int ret;
	struct q6v5_data *drv = container_of(pil, struct q6v5_data, desc);

	ret = clk_prepare_enable(drv->xo);
	if (ret) {
		dev_err(pil->dev, "Failed to vote for XO\n");
		goto out;
	}

	ret = regulator_set_voltage(drv->vreg_cx,
				    RPM_REGULATOR_CORNER_SUPER_TURBO,
				    RPM_REGULATOR_CORNER_SUPER_TURBO);
	if (ret) {
		dev_err(pil->dev, "Failed to request vdd_cx voltage.\n");
		goto err_cx_voltage;
	}

	ret = regulator_set_optimum_mode(drv->vreg_cx, 100000);
	if (ret < 0) {
		dev_err(pil->dev, "Failed to set vdd_cx mode.\n");
		goto err_cx_mode;
	}

	ret = regulator_enable(drv->vreg_cx);
	if (ret) {
		dev_err(pil->dev, "Failed to vote for vdd_cx\n");
		goto err_cx_enable;
	}

	if (drv->vreg_pll) {
		ret = regulator_enable(drv->vreg_pll);
		if (ret) {
			dev_err(pil->dev, "Failed to vote for vdd_pll\n");
			goto err_vreg_pll;
		}
	}

	return 0;

err_vreg_pll:
	regulator_disable(drv->vreg_cx);
err_cx_enable:
	regulator_set_optimum_mode(drv->vreg_cx, 0);
err_cx_mode:
	regulator_set_voltage(drv->vreg_cx, RPM_REGULATOR_CORNER_NONE,
			      RPM_REGULATOR_CORNER_SUPER_TURBO);
err_cx_voltage:
	clk_disable_unprepare(drv->xo);
out:
	return ret;
}
EXPORT_SYMBOL(pil_q6v5_make_proxy_votes);

void pil_q6v5_remove_proxy_votes(struct pil_desc *pil)
{
	struct q6v5_data *drv = container_of(pil, struct q6v5_data, desc);
	if (drv->vreg_pll)
		regulator_disable(drv->vreg_pll);
	regulator_disable(drv->vreg_cx);
	regulator_set_optimum_mode(drv->vreg_cx, 0);
	regulator_set_voltage(drv->vreg_cx, RPM_REGULATOR_CORNER_NONE,
			      RPM_REGULATOR_CORNER_SUPER_TURBO);
	clk_disable_unprepare(drv->xo);
}
EXPORT_SYMBOL(pil_q6v5_remove_proxy_votes);

void pil_q6v5_halt_axi_port(struct pil_desc *pil, void __iomem *halt_base)
{
	int ret;
	u32 status;

	/* Assert halt request */
	writel_relaxed(1, halt_base + AXI_HALTREQ);

	/* Wait for halt */
	ret = readl_poll_timeout(halt_base + AXI_HALTACK,
		status, status != 0, 50, HALT_ACK_TIMEOUT_US);
	if (ret)
		dev_warn(pil->dev, "Port %p halt timeout\n", halt_base);
	else if (!readl_relaxed(halt_base + AXI_IDLE))
		dev_warn(pil->dev, "Port %p halt failed\n", halt_base);

	/* Clear halt request (port will remain halted until reset) */
	writel_relaxed(0, halt_base + AXI_HALTREQ);
}
EXPORT_SYMBOL(pil_q6v5_halt_axi_port);

void pil_q6v5_shutdown(struct pil_desc *pil)
{
	u32 val;
	struct q6v5_data *drv = container_of(pil, struct q6v5_data, desc);

	/* Turn off core clock */
	val = readl_relaxed(drv->reg_base + QDSP6SS_GFMUX_CTL);
	val &= ~Q6SS_CLK_ENA;
	writel_relaxed(val, drv->reg_base + QDSP6SS_GFMUX_CTL);

	/* Clamp IO */
	val = readl_relaxed(drv->reg_base + QDSP6SS_PWR_CTL);
	val |= Q6SS_CLAMP_IO;
	writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);

	/* Turn off Q6 memories */
	val &= ~(Q6SS_L2DATA_SLP_NRET_N_0 | Q6SS_L2DATA_SLP_NRET_N_1 |
		 Q6SS_L2DATA_SLP_NRET_N_2 | Q6SS_SLP_RET_N |
		 Q6SS_L2TAG_SLP_NRET_N | Q6SS_ETB_SLP_NRET_N |
		 Q6SS_L2DATA_STBY_N);
	writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);

	/* Assert Q6 resets */
	val = readl_relaxed(drv->reg_base + QDSP6SS_RESET);
	val |= (Q6SS_CORE_ARES | Q6SS_BUS_ARES_ENA);
	writel_relaxed(val, drv->reg_base + QDSP6SS_RESET);

	/* Kill power at block headswitch */
	val = readl_relaxed(drv->reg_base + QDSP6SS_PWR_CTL);
	val &= ~QDSS_BHS_ON;
	writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);
}
EXPORT_SYMBOL(pil_q6v5_shutdown);

int pil_q6v5_reset(struct pil_desc *pil)
{
	struct q6v5_data *drv = container_of(pil, struct q6v5_data, desc);
	u32 val;

	/* Assert resets, stop core */
	val = readl_relaxed(drv->reg_base + QDSP6SS_RESET);
	val |= (Q6SS_CORE_ARES | Q6SS_BUS_ARES_ENA | Q6SS_STOP_CORE);
	writel_relaxed(val, drv->reg_base + QDSP6SS_RESET);

	/* Enable power block headswitch, and wait for it to stabilize */
	val = readl_relaxed(drv->reg_base + QDSP6SS_PWR_CTL);
	val |= QDSS_BHS_ON | QDSS_LDO_BYP;
	writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);
	mb();
	udelay(1);

	/*
	 * Turn on memories. L2 banks should be done individually
	 * to minimize inrush current.
	 */
	val = readl_relaxed(drv->reg_base + QDSP6SS_PWR_CTL);
	val |= Q6SS_SLP_RET_N | Q6SS_L2TAG_SLP_NRET_N |
	       Q6SS_ETB_SLP_NRET_N | Q6SS_L2DATA_STBY_N;
	writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);
	val |= Q6SS_L2DATA_SLP_NRET_N_2;
	writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);
	val |= Q6SS_L2DATA_SLP_NRET_N_1;
	writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);
	val |= Q6SS_L2DATA_SLP_NRET_N_0;
	writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);

	/* Remove IO clamp */
	val &= ~Q6SS_CLAMP_IO;
	writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);

	/* Bring core out of reset */
	val = readl_relaxed(drv->reg_base + QDSP6SS_RESET);
	val &= ~Q6SS_CORE_ARES;
	writel_relaxed(val, drv->reg_base + QDSP6SS_RESET);

	/* Turn on core clock */
	val = readl_relaxed(drv->reg_base + QDSP6SS_GFMUX_CTL);
	val |= Q6SS_CLK_ENA;
	writel_relaxed(val, drv->reg_base + QDSP6SS_GFMUX_CTL);

	/* Start core execution */
	val = readl_relaxed(drv->reg_base + QDSP6SS_RESET);
	val &= ~Q6SS_STOP_CORE;
	writel_relaxed(val, drv->reg_base + QDSP6SS_RESET);

	return 0;
}
EXPORT_SYMBOL(pil_q6v5_reset);

struct q6v5_data __devinit *pil_q6v5_init(struct platform_device *pdev)
{
	struct q6v5_data *drv;
	struct resource *res;
	struct pil_desc *desc;
	int ret;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return ERR_PTR(-ENOMEM);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "qdsp6_base");
	drv->reg_base = devm_request_and_ioremap(&pdev->dev, res);
	if (!drv->reg_base)
		return ERR_PTR(-ENOMEM);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "halt_base");
	drv->axi_halt_base = devm_ioremap(&pdev->dev, res->start,
					  resource_size(res));
	if (!drv->axi_halt_base)
		return ERR_PTR(-ENOMEM);

	desc = &drv->desc;
	ret = of_property_read_string(pdev->dev.of_node, "qcom,firmware-name",
				      &desc->name);
	if (ret)
		return ERR_PTR(ret);

	drv->xo = devm_clk_get(&pdev->dev, "xo");
	if (IS_ERR(drv->xo))
		return ERR_CAST(drv->xo);

	drv->vreg_cx = devm_regulator_get(&pdev->dev, "vdd_cx");
	if (IS_ERR(drv->vreg_cx))
		return ERR_CAST(drv->vreg_cx);

	drv->vreg_pll = devm_regulator_get(&pdev->dev, "vdd_pll");
	if (!IS_ERR(drv->vreg_pll)) {
		int voltage;
		ret = of_property_read_u32(pdev->dev.of_node, "qcom,vdd_pll",
					   &voltage);
		if (ret) {
			dev_err(&pdev->dev, "Failed to find vdd_pll voltage.\n");
			return ERR_PTR(ret);
		}

		ret = regulator_set_voltage(drv->vreg_pll, voltage, voltage);
		if (ret) {
			dev_err(&pdev->dev, "Failed to request vdd_pll voltage.\n");
			return ERR_PTR(ret);
		}

		ret = regulator_set_optimum_mode(drv->vreg_pll, 10000);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to set vdd_pll mode.\n");
			return ERR_PTR(ret);
		}
	} else {
		 drv->vreg_pll = NULL;
	}

	desc->dev = &pdev->dev;

	return drv;
}
EXPORT_SYMBOL(pil_q6v5_init);
