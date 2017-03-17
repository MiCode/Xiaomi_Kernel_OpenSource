/*
 * Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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
#include <linux/regulator/rpm-smd-regulator.h>
#include <linux/clk/msm-clk.h>

#include "peripheral-loader.h"
#include "pil-q6v5.h"

/* QDSP6SS Register Offsets */
#define QDSP6SS_RESET			0x014
#define QDSP6SS_GFMUX_CTL		0x020
#define QDSP6SS_PWR_CTL			0x030
#define QDSP6V6SS_MEM_PWR_CTL		0x034
#define QDSP6SS_BHS_STATUS		0x078
#define QDSP6SS_MEM_PWR_CTL		0x0B0
#define QDSP6SS_STRAP_ACC		0x110
#define QDSP6V62SS_BHS_STATUS		0x0C4

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
#define Q6SS_CLK_SRC_SEL_C		BIT(3)
#define Q6SS_CLK_SRC_SEL_FIELD		0xC
#define Q6SS_CLK_SRC_SWITCH_CLK_OVR	BIT(8)

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

/* QDSP6v55 parameters */
#define QDSP6v55_LDO_ON                 BIT(26)
#define QDSP6v55_LDO_BYP                BIT(25)
#define QDSP6v55_BHS_ON                 BIT(24)
#define QDSP6v55_CLAMP_WL               BIT(21)
#define QDSP6v55_CLAMP_QMC_MEM          BIT(22)
#define L1IU_SLP_NRET_N                 BIT(15)
#define L1DU_SLP_NRET_N                 BIT(14)
#define L2PLRU_SLP_NRET_N               BIT(13)
#define QDSP6v55_BHS_EN_REST_ACK        BIT(0)

#define HALT_CHECK_MAX_LOOPS            (200)
#define BHS_CHECK_MAX_LOOPS             (200)
#define QDSP6SS_XO_CBCR                 (0x0038)

#define QDSP6SS_ACC_OVERRIDE_VAL	0x20

int pil_q6v5_make_proxy_votes(struct pil_desc *pil)
{
	int ret;
	struct q6v5_data *drv = container_of(pil, struct q6v5_data, desc);
	int uv;

	ret = of_property_read_u32(pil->dev->of_node, "vdd_cx-voltage", &uv);
	if (ret) {
		dev_err(pil->dev, "missing vdd_cx-voltage property\n");
		return ret;
	}

	ret = clk_prepare_enable(drv->xo);
	if (ret) {
		dev_err(pil->dev, "Failed to vote for XO\n");
		goto out;
	}

	ret = clk_prepare_enable(drv->qpic_clk);
	if (ret) {
		dev_err(pil->dev, "Failed to vote for qpic clk\n");
		goto err_qpic_vote;
	}

	ret = clk_prepare_enable(drv->pnoc_clk);
	if (ret) {
		dev_err(pil->dev, "Failed to vote for pnoc\n");
		goto err_pnoc_vote;
	}

	ret = clk_prepare_enable(drv->qdss_clk);
	if (ret) {
		dev_err(pil->dev, "Failed to vote for qdss\n");
		goto err_qdss_vote;
	}

	ret = regulator_set_voltage(drv->vreg_cx, uv, INT_MAX);
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
	regulator_set_voltage(drv->vreg_cx, RPM_REGULATOR_CORNER_NONE, INT_MAX);
err_cx_voltage:
	clk_disable_unprepare(drv->qdss_clk);
err_qdss_vote:
	clk_disable_unprepare(drv->pnoc_clk);
err_pnoc_vote:
	clk_disable_unprepare(drv->qpic_clk);
err_qpic_vote:
	clk_disable_unprepare(drv->xo);
out:
	return ret;
}
EXPORT_SYMBOL(pil_q6v5_make_proxy_votes);

void pil_q6v5_remove_proxy_votes(struct pil_desc *pil)
{
	struct q6v5_data *drv = container_of(pil, struct q6v5_data, desc);
	int uv, ret = 0;

	ret = of_property_read_u32(pil->dev->of_node, "vdd_cx-voltage", &uv);
	if (ret) {
		dev_err(pil->dev, "missing vdd_cx-voltage property\n");
		return;
	}

	if (drv->vreg_pll) {
		regulator_disable(drv->vreg_pll);
		regulator_set_optimum_mode(drv->vreg_pll, 0);
	}
	regulator_disable(drv->vreg_cx);
	regulator_set_optimum_mode(drv->vreg_cx, 0);
	regulator_set_voltage(drv->vreg_cx, RPM_REGULATOR_CORNER_NONE, INT_MAX);
	clk_disable_unprepare(drv->xo);
	clk_disable_unprepare(drv->qpic_clk);
	clk_disable_unprepare(drv->pnoc_clk);
	clk_disable_unprepare(drv->qdss_clk);
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
		dev_warn(pil->dev, "Port %pK halt timeout\n", halt_base);
	else if (!readl_relaxed(halt_base + AXI_IDLE))
		dev_warn(pil->dev, "Port %pK halt failed\n", halt_base);

	/* Clear halt request (port will remain halted until reset) */
	writel_relaxed(0, halt_base + AXI_HALTREQ);
}
EXPORT_SYMBOL(pil_q6v5_halt_axi_port);

void assert_clamps(struct pil_desc *pil)
{
	u32 val;
	struct q6v5_data *drv = container_of(pil, struct q6v5_data, desc);

	/*
	 * Assert QDSP6 I/O clamp, memory wordline clamp, and compiler memory
	 * clamp as a software workaround to avoid high MX current during
	 * LPASS/MSS restart.
	 */
	val = readl_relaxed(drv->reg_base + QDSP6SS_PWR_CTL);
	val |= (Q6SS_CLAMP_IO | QDSP6v55_CLAMP_WL |
			QDSP6v55_CLAMP_QMC_MEM);
	writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);
	/* To make sure asserting clamps is done before MSS restart*/
	mb();
}

static void __pil_q6v5_shutdown(struct pil_desc *pil)
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

void pil_q6v5_shutdown(struct pil_desc *pil)
{
	struct q6v5_data *drv = container_of(pil, struct q6v5_data, desc);
	if (drv->qdsp6v55)
		/* Subsystem driver expected to halt bus and assert reset */
		return;
	else
		__pil_q6v5_shutdown(pil);
}
EXPORT_SYMBOL(pil_q6v5_shutdown);

static int __pil_q6v5_reset(struct pil_desc *pil)
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

	/* Need a different clock source for v5.2.0 */
	if (drv->qdsp6v5_2_0) {
		val &= ~Q6SS_CLK_SRC_SEL_FIELD;
		val |= Q6SS_CLK_SRC_SEL_C;
	}

	/* force clock on during source switch */
	if (drv->qdsp6v56)
		val |= Q6SS_CLK_SRC_SWITCH_CLK_OVR;

	writel_relaxed(val, drv->reg_base + QDSP6SS_GFMUX_CTL);

	/* Start core execution */
	val = readl_relaxed(drv->reg_base + QDSP6SS_RESET);
	val &= ~Q6SS_STOP_CORE;
	writel_relaxed(val, drv->reg_base + QDSP6SS_RESET);

	return 0;
}

static int q6v55_branch_clk_enable(struct q6v5_data *drv)
{
	u32 val, count;
	void __iomem *cbcr_reg = drv->reg_base + QDSP6SS_XO_CBCR;

	val = readl_relaxed(cbcr_reg);
	val |= 0x1;
	writel_relaxed(val, cbcr_reg);

	for (count = HALT_CHECK_MAX_LOOPS; count > 0; count--) {
		val = readl_relaxed(cbcr_reg);
		if (!(val & BIT(31)))
			return 0;
		udelay(1);
	}

	dev_err(drv->desc.dev, "Failed to enable xo branch clock.\n");
	return -EINVAL;
}

static int __pil_q6v55_reset(struct pil_desc *pil)
{
	struct q6v5_data *drv = container_of(pil, struct q6v5_data, desc);
	u32 val;
	int i;

	/* Override the ACC value if required */
	if (drv->override_acc)
		writel_relaxed(QDSP6SS_ACC_OVERRIDE_VAL,
				drv->reg_base + QDSP6SS_STRAP_ACC);

	/* Override the ACC value with input value */
	if (!of_property_read_u32(pil->dev->of_node, "qcom,override-acc-1",
				&drv->override_acc_1))
		writel_relaxed(drv->override_acc_1,
				drv->reg_base + QDSP6SS_STRAP_ACC);

	/* Assert resets, stop core */
	val = readl_relaxed(drv->reg_base + QDSP6SS_RESET);
	val |= (Q6SS_CORE_ARES | Q6SS_BUS_ARES_ENA | Q6SS_STOP_CORE);
	writel_relaxed(val, drv->reg_base + QDSP6SS_RESET);

	/* BHS require xo cbcr to be enabled */
	i = q6v55_branch_clk_enable(drv);
	if (i)
		return i;

	/* Enable power block headswitch, and wait for it to stabilize */
	val = readl_relaxed(drv->reg_base + QDSP6SS_PWR_CTL);
	val |= QDSP6v55_BHS_ON;
	writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);
	mb();
	udelay(1);

	if (drv->qdsp6v62_1_2) {
		for (i = BHS_CHECK_MAX_LOOPS; i > 0; i--) {
			if (readl_relaxed(drv->reg_base + QDSP6V62SS_BHS_STATUS)
			    & QDSP6v55_BHS_EN_REST_ACK)
				break;
			udelay(1);
		}
		if (!i) {
			pr_err("%s: BHS_EN_REST_ACK not set!\n", __func__);
			return -ETIMEDOUT;
		}
	}

	if (drv->qdsp6v61_1_1) {
		for (i = BHS_CHECK_MAX_LOOPS; i > 0; i--) {
			if (readl_relaxed(drv->reg_base + QDSP6SS_BHS_STATUS)
			    & QDSP6v55_BHS_EN_REST_ACK)
				break;
			udelay(1);
		}
		if (!i) {
			pr_err("%s: BHS_EN_REST_ACK not set!\n", __func__);
			return -ETIMEDOUT;
		}
	}

	/* Put LDO in bypass mode */
	val |= QDSP6v55_LDO_BYP;
	writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);

	if (drv->qdsp6v56_1_3) {
		/* Deassert memory peripheral sleep and L2 memory standby */
		val = readl_relaxed(drv->reg_base + QDSP6SS_PWR_CTL);
		val |= (Q6SS_L2DATA_STBY_N | Q6SS_SLP_RET_N);
		writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);

		/* Turn on L1, L2 and ETB memories 1 at a time */
		for (i = 17; i >= 0; i--) {
			val |= BIT(i);
			writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);
			udelay(1);
		}
	} else if (drv->qdsp6v56_1_5 || drv->qdsp6v56_1_8
					|| drv->qdsp6v56_1_10) {
		/* Deassert QDSP6 compiler memory clamp */
		val = readl_relaxed(drv->reg_base + QDSP6SS_PWR_CTL);
		val &= ~QDSP6v55_CLAMP_QMC_MEM;
		writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);

		/* Deassert memory peripheral sleep and L2 memory standby */
		val |= (Q6SS_L2DATA_STBY_N | Q6SS_SLP_RET_N);
		writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);

		/* Turn on L1, L2, ETB and JU memories 1 at a time */
		val = readl_relaxed(drv->reg_base + QDSP6SS_MEM_PWR_CTL);
		for (i = 19; i >= 0; i--) {
			val |= BIT(i);
			writel_relaxed(val, drv->reg_base +
						QDSP6SS_MEM_PWR_CTL);
			val |= readl_relaxed(drv->reg_base +
						QDSP6SS_MEM_PWR_CTL);
			/*
			 * Wait for 1us for both memory peripheral and
			 * data array to turn on.
			 */
			udelay(1);
		}
	} else if (drv->qdsp6v56_1_8_inrush_current) {
		/* Deassert QDSP6 compiler memory clamp */
		val = readl_relaxed(drv->reg_base + QDSP6SS_PWR_CTL);
		val &= ~QDSP6v55_CLAMP_QMC_MEM;
		writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);

		/* Deassert memory peripheral sleep and L2 memory standby */
		val |= (Q6SS_L2DATA_STBY_N | Q6SS_SLP_RET_N);
		writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);

		/* Turn on L1, L2, ETB and JU memories 1 at a time */
		val = readl_relaxed(drv->reg_base + QDSP6SS_MEM_PWR_CTL);
		for (i = 19; i >= 6; i--) {
			val |= BIT(i);
			writel_relaxed(val, drv->reg_base +
						QDSP6SS_MEM_PWR_CTL);
			/*
			 * Wait for 1us for both memory peripheral and
			 * data array to turn on.
			 */
			udelay(1);
		}

		for (i = 0 ; i <= 5 ; i++) {
			val |= BIT(i);
			writel_relaxed(val, drv->reg_base +
						QDSP6SS_MEM_PWR_CTL);
			/*
			 * Wait for 1us for both memory peripheral and
			 * data array to turn on.
			 */
			udelay(1);
		}
	} else if (drv->qdsp6v61_1_1 || drv->qdsp6v62_1_2) {
		/* Deassert QDSP6 compiler memory clamp */
		val = readl_relaxed(drv->reg_base + QDSP6SS_PWR_CTL);
		val &= ~QDSP6v55_CLAMP_QMC_MEM;
		writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);

		/* Deassert memory peripheral sleep and L2 memory standby */
		val |= (Q6SS_L2DATA_STBY_N | Q6SS_SLP_RET_N);
		writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);

		/* Turn on L1, L2, ETB and JU memories 1 at a time */
		val = readl_relaxed(drv->reg_base +
				QDSP6V6SS_MEM_PWR_CTL);
		for (i = 28; i >= 0; i--) {
			val |= BIT(i);
			writel_relaxed(val, drv->reg_base +
					QDSP6V6SS_MEM_PWR_CTL);
			/*
			 * Wait for 1us for both memory peripheral and
			 * data array to turn on.
			 */
			udelay(1);
		}
	} else {
		/* Turn on memories. */
		val = readl_relaxed(drv->reg_base + QDSP6SS_PWR_CTL);
		val |= 0xFFF00;
		writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);

		/* Turn on L2 banks 1 at a time */
		for (i = 0; i <= 7; i++) {
			val |= BIT(i);
			writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);
		}
	}

	/* Remove word line clamp */
	val = readl_relaxed(drv->reg_base + QDSP6SS_PWR_CTL);
	val &= ~QDSP6v55_CLAMP_WL;
	writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);

	/* Remove IO clamp */
	val &= ~Q6SS_CLAMP_IO;
	writel_relaxed(val, drv->reg_base + QDSP6SS_PWR_CTL);

	/* Bring core out of reset */
	val = readl_relaxed(drv->reg_base + QDSP6SS_RESET);
	val &= ~(Q6SS_CORE_ARES | Q6SS_STOP_CORE);
	writel_relaxed(val, drv->reg_base + QDSP6SS_RESET);

	/* Turn on core clock */
	val = readl_relaxed(drv->reg_base + QDSP6SS_GFMUX_CTL);
	val |= Q6SS_CLK_ENA;
	writel_relaxed(val, drv->reg_base + QDSP6SS_GFMUX_CTL);

	return 0;
}

int pil_q6v5_reset(struct pil_desc *pil)
{
	struct q6v5_data *drv = container_of(pil, struct q6v5_data, desc);
	if (drv->qdsp6v55)
		return __pil_q6v55_reset(pil);
	else
		return __pil_q6v5_reset(pil);
}
EXPORT_SYMBOL(pil_q6v5_reset);

struct q6v5_data *pil_q6v5_init(struct platform_device *pdev)
{
	struct q6v5_data *drv;
	struct resource *res;
	struct pil_desc *desc;
	struct property *prop;
	int ret;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return ERR_PTR(-ENOMEM);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "qdsp6_base");
	drv->reg_base = devm_ioremap_resource(&pdev->dev, res);
	if (!drv->reg_base)
		return ERR_PTR(-ENOMEM);

	desc = &drv->desc;
	ret = of_property_read_string(pdev->dev.of_node, "qcom,firmware-name",
				      &desc->name);
	if (ret)
		return ERR_PTR(ret);

	desc->clear_fw_region = false;
	desc->dev = &pdev->dev;

	drv->qdsp6v5_2_0 = of_device_is_compatible(pdev->dev.of_node,
						   "qcom,pil-femto-modem");

	if (drv->qdsp6v5_2_0)
		return drv;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "halt_base");
	if (res) {
		drv->axi_halt_base = devm_ioremap(&pdev->dev, res->start,
							resource_size(res));
		if (!drv->axi_halt_base) {
			dev_err(&pdev->dev, "Failed to map axi_halt_base.\n");
			return ERR_PTR(-ENOMEM);
		}
	}

	if (!drv->axi_halt_base) {
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
								"halt_q6");
		if (res) {
			drv->axi_halt_q6 = devm_ioremap(&pdev->dev,
					res->start, resource_size(res));
			if (!drv->axi_halt_q6) {
				dev_err(&pdev->dev, "Failed to map axi_halt_q6.\n");
				return ERR_PTR(-ENOMEM);
			}
		}

		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
								"halt_modem");
		if (res) {
			drv->axi_halt_mss = devm_ioremap(&pdev->dev,
					res->start, resource_size(res));
			if (!drv->axi_halt_mss) {
				dev_err(&pdev->dev, "Failed to map axi_halt_mss.\n");
				return ERR_PTR(-ENOMEM);
			}
		}

		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
								"halt_nc");
		if (res) {
			drv->axi_halt_nc = devm_ioremap(&pdev->dev,
					res->start, resource_size(res));
			if (!drv->axi_halt_nc) {
				dev_err(&pdev->dev, "Failed to map axi_halt_nc.\n");
				return ERR_PTR(-ENOMEM);
			}
		}
	}

	if (!(drv->axi_halt_base || (drv->axi_halt_q6 && drv->axi_halt_mss
					&& drv->axi_halt_nc))) {
		dev_err(&pdev->dev, "halt bases for Q6 are not defined.\n");
		return ERR_PTR(-EINVAL);
	}

	drv->qdsp6v55 = of_device_is_compatible(pdev->dev.of_node,
						"qcom,pil-q6v55-mss");
	drv->qdsp6v56 = of_device_is_compatible(pdev->dev.of_node,
						"qcom,pil-q6v56-mss");

	drv->qdsp6v56_1_3 = of_property_read_bool(pdev->dev.of_node,
						"qcom,qdsp6v56-1-3");
	drv->qdsp6v56_1_5 = of_property_read_bool(pdev->dev.of_node,
						"qcom,qdsp6v56-1-5");

	drv->qdsp6v56_1_8 = of_property_read_bool(pdev->dev.of_node,
						"qcom,qdsp6v56-1-8");
	drv->qdsp6v56_1_10 = of_property_read_bool(pdev->dev.of_node,
						"qcom,qdsp6v56-1-10");

	drv->qdsp6v56_1_8_inrush_current = of_property_read_bool(
						pdev->dev.of_node,
						"qcom,qdsp6v56-1-8-inrush-current");

	drv->qdsp6v61_1_1 = of_property_read_bool(pdev->dev.of_node,
						"qcom,qdsp6v61-1-1");

	drv->qdsp6v62_1_2 = of_property_read_bool(pdev->dev.of_node,
						"qcom,qdsp6v62-1-2");

	drv->non_elf_image = of_property_read_bool(pdev->dev.of_node,
						"qcom,mba-image-is-not-elf");

	drv->override_acc = of_property_read_bool(pdev->dev.of_node,
						"qcom,override-acc");

	drv->ahb_clk_vote = of_property_read_bool(pdev->dev.of_node,
						"qcom,ahb-clk-vote");
	drv->mx_spike_wa = of_property_read_bool(pdev->dev.of_node,
						"qcom,mx-spike-wa");

	drv->xo = devm_clk_get(&pdev->dev, "xo");
	if (IS_ERR(drv->xo))
		return ERR_CAST(drv->xo);

	drv->qpic_clk = devm_clk_get(&pdev->dev, "qpic");
	if (IS_ERR(drv->qpic_clk))
		drv->qpic_clk = NULL;

	if (of_property_read_bool(pdev->dev.of_node, "qcom,pnoc-clk-vote")) {
		drv->pnoc_clk = devm_clk_get(&pdev->dev, "pnoc_clk");
		if (IS_ERR(drv->pnoc_clk))
			return ERR_CAST(drv->pnoc_clk);
	} else {
		drv->pnoc_clk = NULL;
	}

	if (of_property_match_string(pdev->dev.of_node,
			"qcom,proxy-clock-names", "qdss_clk") >= 0) {
		drv->qdss_clk = devm_clk_get(&pdev->dev, "qdss_clk");
		if (IS_ERR(drv->qdss_clk))
			return ERR_CAST(drv->qdss_clk);
	} else {
		drv->qdss_clk = NULL;
	}

	drv->vreg_cx = devm_regulator_get(&pdev->dev, "vdd_cx");
	if (IS_ERR(drv->vreg_cx))
		return ERR_CAST(drv->vreg_cx);
	prop = of_find_property(pdev->dev.of_node, "vdd_cx-voltage", NULL);
	if (!prop) {
		dev_err(&pdev->dev, "Missing vdd_cx-voltage property\n");
		return ERR_CAST(prop);
	}

	drv->vreg_pll = devm_regulator_get(&pdev->dev, "vdd_pll");
	if (!IS_ERR_OR_NULL(drv->vreg_pll)) {
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

	return drv;
}
EXPORT_SYMBOL(pil_q6v5_init);
