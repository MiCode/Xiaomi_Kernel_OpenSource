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
#include <linux/elf.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/clk.h>

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
#define Q6SS_CORE_ARES			BIT(1)
#define Q6SS_ETM_ISDB_ARES		BIT(3)
#define Q6SS_STOP_CORE			BIT(4)

/* QDSP6SS_GFMUX_CTL */
#define Q6SS_CLK_ENA			BIT(1)

/* QDSP6SS_PWR_CTL */
#define Q6SS_L2DATA_SLP_NRET_N		BIT(0)
#define Q6SS_L2TAG_SLP_NRET_N		BIT(16)
#define Q6SS_ETB_SLP_NRET_N		BIT(17)
#define Q6SS_L2DATA_STBY_N		BIT(18)
#define Q6SS_SLP_RET_N			BIT(19)
#define Q6SS_CLAMP_IO			BIT(20)
#define QDSS_BHS_ON			BIT(21)

int pil_q6v5_make_proxy_votes(struct pil_desc *pil)
{
	int ret;
	struct q6v5_data *drv = dev_get_drvdata(pil->dev);

	ret = clk_prepare_enable(drv->xo);
	if (ret) {
		dev_err(pil->dev, "Failed to enable XO\n");
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL(pil_q6v5_make_proxy_votes);

void pil_q6v5_remove_proxy_votes(struct pil_desc *pil)
{
	struct q6v5_data *drv = dev_get_drvdata(pil->dev);
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

int pil_q6v5_init_image(struct pil_desc *pil, const u8 *metadata,
			       size_t size)
{
	const struct elf32_hdr *ehdr = (struct elf32_hdr *)metadata;
	struct q6v5_data *drv = dev_get_drvdata(pil->dev);
	drv->start_addr = ehdr->e_entry;
	return 0;
}
EXPORT_SYMBOL(pil_q6v5_init_image);

int pil_q6v5_enable_clks(struct pil_desc *pil)
{
	struct q6v5_data *drv = dev_get_drvdata(pil->dev);
	int ret;

	ret = clk_reset(drv->core_clk, CLK_RESET_DEASSERT);
	if (ret)
		goto err_reset;
	ret = clk_prepare_enable(drv->core_clk);
	if (ret)
		goto err_core_clk;
	ret = clk_prepare_enable(drv->bus_clk);
	if (ret)
		goto err_bus_clk;

	return 0;

err_bus_clk:
	clk_disable_unprepare(drv->core_clk);
err_core_clk:
	clk_reset(drv->core_clk, CLK_RESET_ASSERT);
err_reset:
	return ret;
}
EXPORT_SYMBOL(pil_q6v5_enable_clks);

void pil_q6v5_disable_clks(struct pil_desc *pil)
{
	struct q6v5_data *drv = dev_get_drvdata(pil->dev);

	clk_disable_unprepare(drv->bus_clk);
	clk_disable_unprepare(drv->core_clk);
	clk_reset(drv->core_clk, CLK_RESET_ASSERT);
}
EXPORT_SYMBOL(pil_q6v5_disable_clks);

void pil_q6v5_shutdown(struct pil_desc *pil)
{
	u32 val;
	struct q6v5_data *drv = dev_get_drvdata(pil->dev);

	/* Turn off core clock */
	val = readl_relaxed(drv->reg_base + QDSP6SS_GFMUX_CTL);
	val &= ~Q6SS_CLK_ENA;
	writel_relaxed(val, drv->reg_base + QDSP6SS_GFMUX_CTL);

	/* Clamp IO */
	val = readl_relaxed(drv->reg_base + QDSP6SS_PWR_CTL);
	val |= Q6SS_CLAMP_IO;
	writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);

	/* Turn off Q6 memories */
	val &= ~(Q6SS_L2DATA_SLP_NRET_N | Q6SS_SLP_RET_N |
		 Q6SS_L2TAG_SLP_NRET_N | Q6SS_ETB_SLP_NRET_N |
		 Q6SS_L2DATA_STBY_N);
	writel_relaxed(Q6SS_CLAMP_IO, drv->reg_base + QDSP6SS_PWR_CTL);

	/* Assert Q6 resets */
	val = readl_relaxed(drv->reg_base + QDSP6SS_RESET);
	val = (Q6SS_CORE_ARES | Q6SS_ETM_ISDB_ARES);
	writel_relaxed(val, drv->reg_base + QDSP6SS_RESET);

	/* Kill power at block headswitch (affects LPASS only) */
	val = readl_relaxed(drv->reg_base + QDSP6SS_PWR_CTL);
	val &= ~QDSS_BHS_ON;
	writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);
}
EXPORT_SYMBOL(pil_q6v5_shutdown);

int pil_q6v5_reset(struct pil_desc *pil)
{
	struct q6v5_data *drv = dev_get_drvdata(pil->dev);
	u32 val;

	/* Assert resets, stop core */
	val = readl_relaxed(drv->reg_base + QDSP6SS_RESET);
	val |= (Q6SS_CORE_ARES | Q6SS_ETM_ISDB_ARES | Q6SS_STOP_CORE);
	writel_relaxed(val, drv->reg_base + QDSP6SS_RESET);

	/* Enable power block headswitch (only affects LPASS) */
	val = readl_relaxed(drv->reg_base + QDSP6SS_PWR_CTL);
	val |= QDSS_BHS_ON;
	writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);

	/* Turn on memories */
	val = readl_relaxed(drv->reg_base + QDSP6SS_PWR_CTL);
	val |= Q6SS_L2DATA_SLP_NRET_N | Q6SS_SLP_RET_N |
	       Q6SS_L2TAG_SLP_NRET_N | Q6SS_ETB_SLP_NRET_N |
	       Q6SS_L2DATA_STBY_N;
	writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);

	/* Remove IO clamp */
	val &= ~Q6SS_CLAMP_IO;
	writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);

	/* Bring core out of reset */
	val = Q6SS_STOP_CORE;
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

struct pil_desc __devinit *pil_q6v5_init(struct platform_device *pdev)
{
	struct q6v5_data *drv;
	struct resource *res;
	struct pil_desc *desc;
	int ret;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return ERR_PTR(-ENOMEM);
	platform_set_drvdata(pdev, drv);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return ERR_PTR(-EINVAL);
	drv->reg_base = devm_ioremap(&pdev->dev, res->start,
				     resource_size(res));
	if (!drv->reg_base)
		return ERR_PTR(-ENOMEM);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	drv->axi_halt_base = devm_ioremap(&pdev->dev, res->start,
					  resource_size(res));
	if (!drv->axi_halt_base)
		return ERR_PTR(-ENOMEM);

	desc = devm_kzalloc(&pdev->dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return ERR_PTR(-ENOMEM);

	ret = of_property_read_string(pdev->dev.of_node, "qcom,firmware-name",
				      &desc->name);
	if (ret)
		return ERR_PTR(ret);

	drv->xo = devm_clk_get(&pdev->dev, "xo");
	if (IS_ERR(drv->xo))
		return ERR_CAST(drv->xo);

	drv->bus_clk = devm_clk_get(&pdev->dev, "bus_clk");
	if (IS_ERR(drv->bus_clk))
		return ERR_CAST(drv->bus_clk);

	drv->core_clk = devm_clk_get(&pdev->dev, "core_clk");
	if (IS_ERR(drv->core_clk))
		return ERR_CAST(drv->core_clk);

	desc->dev = &pdev->dev;

	return desc;
}
EXPORT_SYMBOL(pil_q6v5_init);
