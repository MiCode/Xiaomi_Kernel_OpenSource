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

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/elf.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <mach/msm_iomap.h>
#include <asm/mach-types.h>

#include "peripheral-loader.h"

#define MSM_FW_QDSP6SS_PHYS	0x08800000
#define MSM_SW_QDSP6SS_PHYS	0x08900000
#define MSM_LPASS_QDSP6SS_PHYS	0x28800000
#define MSM_MSS_ENABLE_PHYS	0x08B00000

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
#define SFAB_MSS_Q6_FW_ACLK_CTL (MSM_CLK_CTL_BASE + 0x2044)
#define SFAB_MSS_Q6_SW_ACLK_CTL	(MSM_CLK_CTL_BASE + 0x2040)
#define SFAB_LPASS_Q6_ACLK_CTL	(MSM_CLK_CTL_BASE + 0x23A0)
#define MSS_Q6FW_JTAG_CLK_CTL	(MSM_CLK_CTL_BASE + 0x2C6C)
#define MSS_Q6SW_JTAG_CLK_CTL	(MSM_CLK_CTL_BASE + 0x2C68)
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

#define MSM_RIVA_PHYS			0x03204000
#define RIVA_PMU_A2XB_CFG		(msm_riva_base + 0xB8)
#define RIVA_PMU_A2XB_CFG_EN		BIT(0)

#define RIVA_PMU_CFG			(msm_riva_base + 0x28)
#define RIVA_PMU_CFG_WARM_BOOT		BIT(0)
#define RIVA_PMU_CFG_IRIS_XO_MODE	0x6
#define RIVA_PMU_CFG_IRIS_XO_MODE_48	(3 << 1)

#define RIVA_PMU_OVRD_VAL		(msm_riva_base + 0x30)
#define RIVA_PMU_OVRD_VAL_CCPU_RESET	BIT(0)
#define RIVA_PMU_OVRD_VAL_CCPU_CLK	BIT(1)

#define RIVA_PMU_CCPU_CTL		(msm_riva_base + 0x9C)
#define RIVA_PMU_CCPU_CTL_HIGH_IVT	BIT(0)
#define RIVA_PMU_CCPU_CTL_REMAP_EN	BIT(2)

#define RIVA_PMU_CCPU_BOOT_REMAP_ADDR	(msm_riva_base + 0xA0)

#define RIVA_PLL_MODE			(MSM_CLK_CTL_BASE + 0x31A0)
#define PLL_MODE_OUTCTRL		BIT(0)
#define PLL_MODE_BYPASSNL		BIT(1)
#define PLL_MODE_RESET_N		BIT(2)
#define PLL_MODE_REF_XO_SEL		0x30
#define PLL_MODE_REF_XO_SEL_CXO		(2 << 4)
#define PLL_MODE_REF_XO_SEL_RF		(3 << 4)
#define RIVA_PLL_L_VAL			(MSM_CLK_CTL_BASE + 0x31A4)
#define RIVA_PLL_M_VAL			(MSM_CLK_CTL_BASE + 0x31A8)
#define RIVA_PLL_N_VAL			(MSM_CLK_CTL_BASE + 0x31Ac)
#define RIVA_PLL_CONFIG			(MSM_CLK_CTL_BASE + 0x31B4)
#define RIVA_PLL_STATUS			(MSM_CLK_CTL_BASE + 0x31B8)

#define RIVA_PMU_ROOT_CLK_SEL		(msm_riva_base + 0xC8)
#define RIVA_PMU_ROOT_CLK_SEL_3		BIT(2)

#define RIVA_PMU_CLK_ROOT3			(msm_riva_base + 0x78)
#define RIVA_PMU_CLK_ROOT3_ENA			BIT(0)
#define RIVA_PMU_CLK_ROOT3_SRC0_DIV		0x3C
#define RIVA_PMU_CLK_ROOT3_SRC0_DIV_2		(1 << 2)
#define RIVA_PMU_CLK_ROOT3_SRC0_SEL		0x1C0
#define RIVA_PMU_CLK_ROOT3_SRC0_SEL_RIVA	(1 << 6)
#define RIVA_PMU_CLK_ROOT3_SRC1_DIV		0x1E00
#define RIVA_PMU_CLK_ROOT3_SRC1_DIV_2		(1 << 9)
#define RIVA_PMU_CLK_ROOT3_SRC1_SEL		0xE000
#define RIVA_PMU_CLK_ROOT3_SRC1_SEL_RIVA	(1 << 13)

#define PPSS_RESET			(MSM_CLK_CTL_BASE + 0x2594)
#define PPSS_PROC_CLK_CTL		(MSM_CLK_CTL_BASE + 0x2588)
#define PPSS_HCLK_CTL			(MSM_CLK_CTL_BASE + 0x2580)

struct q6_data {
	const unsigned strap_tcm_base;
	const unsigned strap_ahb_upper;
	const unsigned strap_ahb_lower;
	const unsigned reg_base_phys;
	void __iomem *reg_base;
	void __iomem *aclk_reg;
	void __iomem *jtag_clk_reg;
	int start_addr;
	struct regulator *vreg;
	bool vreg_enabled;
	const char *name;
};

static struct q6_data q6_lpass = {
	.strap_tcm_base  = (0x146 << 16),
	.strap_ahb_upper = (0x029 << 16),
	.strap_ahb_lower = (0x028 << 4),
	.reg_base_phys = MSM_LPASS_QDSP6SS_PHYS,
	.aclk_reg = SFAB_LPASS_Q6_ACLK_CTL,
	.name = "q6_lpass",
};

static struct q6_data q6_modem_fw = {
	.strap_tcm_base  = (0x40 << 16),
	.strap_ahb_upper = (0x09 << 16),
	.strap_ahb_lower = (0x08 << 4),
	.reg_base_phys = MSM_FW_QDSP6SS_PHYS,
	.aclk_reg = SFAB_MSS_Q6_FW_ACLK_CTL,
	.jtag_clk_reg = MSS_Q6FW_JTAG_CLK_CTL,
	.name = "q6_modem_fw",
};

static struct q6_data q6_modem_sw = {
	.strap_tcm_base  = (0x42 << 16),
	.strap_ahb_upper = (0x09 << 16),
	.strap_ahb_lower = (0x08 << 4),
	.reg_base_phys = MSM_SW_QDSP6SS_PHYS,
	.aclk_reg = SFAB_MSS_Q6_SW_ACLK_CTL,
	.jtag_clk_reg = MSS_Q6SW_JTAG_CLK_CTL,
	.name = "q6_modem_sw",
};

static void __iomem *mss_enable_reg;
static void __iomem *msm_riva_base;
static unsigned long riva_start;

static int init_image_lpass_q6_untrusted(const u8 *metadata, size_t size)
{
	const struct elf32_hdr *ehdr = (struct elf32_hdr *)metadata;
	q6_lpass.start_addr = ehdr->e_entry;
	return 0;
}

static int init_image_modem_fw_q6_untrusted(const u8 *metadata, size_t size)
{
	const struct elf32_hdr *ehdr = (struct elf32_hdr *)metadata;
	q6_modem_fw.start_addr = ehdr->e_entry;
	return 0;
}

static int init_image_modem_sw_q6_untrusted(const u8 *metadata, size_t size)
{
	const struct elf32_hdr *ehdr = (struct elf32_hdr *)metadata;
	q6_modem_sw.start_addr = ehdr->e_entry;
	return 0;
}

static int verify_blob(u32 phy_addr, size_t size)
{
	return 0;
}

static int reset_q6_untrusted(struct q6_data *q6)
{
	u32 reg, err = 0;

	err = regulator_set_voltage(q6->vreg, 1050000, 1050000);
	if (err) {
		pr_err("Failed to set %s regulator's voltage.\n", q6->name);
		goto out;
	}
	err = regulator_enable(q6->vreg);
	if (err) {
		pr_err("Failed to enable %s's regulator.\n", q6->name);
		goto out;
	}
	q6->vreg_enabled = true;

	/* Enable Q6 ACLK */
	writel_relaxed(0x10, q6->aclk_reg);

	if (q6 == &q6_modem_fw || q6 == &q6_modem_sw) {
		/* Enable MSS clocks */
		writel_relaxed(0x10, SFAB_MSS_M_ACLK_CTL);
		writel_relaxed(0x10, SFAB_MSS_S_HCLK_CTL);
		writel_relaxed(0x10, MSS_S_HCLK_CTL);
		writel_relaxed(0x10, MSS_SLP_CLK_CTL);
		/* Wait for clocks to enable */
		mb();
		udelay(10);

		/* Enable JTAG clocks */
		/* TODO: Remove if/when Q6 software enables them? */
		writel_relaxed(0x10, q6->jtag_clk_reg);

		/* De-assert MSS reset */
		writel_relaxed(0x0,  MSS_RESET);
		mb();
		udelay(10);

		/* Enable MSS */
		writel_relaxed(0x7,  mss_enable_reg);
	}

	/*
	 * Assert AXIS_ACLK_EN override to allow for correct updating of the
	 * QDSP6_CORE_STATE status bit. This is mandatory only for the SW Q6
	 * in 8960v1 and optional elsewhere.
	 */
	reg = readl_relaxed(q6->reg_base + QDSP6SS_CGC_OVERRIDE);
	reg |= Q6SS_AXIS_ACLK_EN;
	writel_relaxed(reg, q6->reg_base + QDSP6SS_CGC_OVERRIDE);

	/* Deassert Q6SS_SS_ARES */
	reg = readl_relaxed(q6->reg_base + QDSP6SS_RESET);
	reg &= ~(Q6SS_SS_ARES);
	writel_relaxed(reg, q6->reg_base + QDSP6SS_RESET);

	/* Program boot address */
	writel_relaxed((q6->start_addr >> 8) & 0xFFFFFF,
			q6->reg_base + QDSP6SS_RST_EVB);

	/* Program TCM and AHB address ranges */
	writel_relaxed(q6->strap_tcm_base, q6->reg_base + QDSP6SS_STRAP_TCM);
	writel_relaxed(q6->strap_ahb_upper | q6->strap_ahb_lower,
		       q6->reg_base + QDSP6SS_STRAP_AHB);

	/* Turn off Q6 core clock */
	writel_relaxed(Q6SS_SRC_SWITCH_CLK_OVR,
		       q6->reg_base + QDSP6SS_GFMUX_CTL);

	/* Put memories to sleep */
	writel_relaxed(Q6SS_CLAMP_IO, q6->reg_base + QDSP6SS_PWR_CTL);

	/* Assert resets */
	reg = readl_relaxed(q6->reg_base + QDSP6SS_RESET);
	reg |= (Q6SS_CORE_ARES | Q6SS_ISDB_ARES | Q6SS_ETM_ARES
	    | Q6SS_STOP_CORE_ARES);
	writel_relaxed(reg, q6->reg_base + QDSP6SS_RESET);

	/* Wait 8 AHB cycles for Q6 to be fully reset (AHB = 1.5Mhz) */
	mb();
	usleep_range(20, 30);

	/* Turn on Q6 memories */
	reg = Q6SS_L2DATA_SLP_NRET_N | Q6SS_SLP_RET_N | Q6SS_L1TCM_SLP_NRET_N
	    | Q6SS_L2TAG_SLP_NRET_N | Q6SS_ETB_SLEEP_NRET_N | Q6SS_ARR_STBY_N
	    | Q6SS_CLAMP_IO;
	writel_relaxed(reg, q6->reg_base + QDSP6SS_PWR_CTL);

	/* Turn on Q6 core clock */
	reg = Q6SS_CLK_ENA | Q6SS_SRC_SWITCH_CLK_OVR;
	writel_relaxed(reg, q6->reg_base + QDSP6SS_GFMUX_CTL);

	/* Remove Q6SS_CLAMP_IO */
	reg = readl_relaxed(q6->reg_base + QDSP6SS_PWR_CTL);
	reg &= ~Q6SS_CLAMP_IO;
	writel_relaxed(reg, q6->reg_base + QDSP6SS_PWR_CTL);

	/* Bring Q6 core out of reset and start execution. */
	writel_relaxed(0x0, q6->reg_base + QDSP6SS_RESET);

	/*
	 * Re-enable auto-gating of AXIS_ACLK at lease one AXI clock cycle
	 * after resets are de-asserted.
	 */
	mb();
	usleep_range(1, 10);
	reg = readl_relaxed(q6->reg_base + QDSP6SS_CGC_OVERRIDE);
	reg &= ~Q6SS_AXIS_ACLK_EN;
	writel_relaxed(reg, q6->reg_base + QDSP6SS_CGC_OVERRIDE);

out:
	return err;
}

static int reset_lpass_q6_untrusted(void)
{
	return reset_q6_untrusted(&q6_lpass);
}

static int reset_modem_fw_q6_untrusted(void)
{
	return reset_q6_untrusted(&q6_modem_fw);
}

static int reset_modem_sw_q6_untrusted(void)
{
	return reset_q6_untrusted(&q6_modem_sw);
}

static int shutdown_q6_untrusted(struct q6_data *q6)
{
	u32 reg;

	/* Turn off Q6 core clock */
	writel_relaxed(Q6SS_SRC_SWITCH_CLK_OVR,
		       q6->reg_base + QDSP6SS_GFMUX_CTL);

	/* Assert resets */
	reg = (Q6SS_SS_ARES | Q6SS_CORE_ARES | Q6SS_ISDB_ARES
	     | Q6SS_ETM_ARES | Q6SS_STOP_CORE_ARES | Q6SS_PRIV_ARES);
	writel_relaxed(reg, q6->reg_base + QDSP6SS_RESET);

	/* Turn off Q6 memories */
	writel_relaxed(Q6SS_CLAMP_IO, q6->reg_base + QDSP6SS_PWR_CTL);

	/* Put Modem Subsystem back into reset when shutting down FWQ6 */
	if (q6 == &q6_modem_fw)
		writel_relaxed(0x1, MSS_RESET);

	if (q6->vreg_enabled) {
		regulator_disable(q6->vreg);
		q6->vreg_enabled = false;
	}

	return 0;
}

static int shutdown_lpass_q6_untrusted(void)
{
	return shutdown_q6_untrusted(&q6_lpass);
}

static int shutdown_modem_fw_q6_untrusted(void)
{
	return shutdown_q6_untrusted(&q6_modem_fw);
}

static int shutdown_modem_sw_q6_untrusted(void)
{
	return shutdown_q6_untrusted(&q6_modem_sw);
}

static int init_image_riva_untrusted(const u8 *metadata, size_t size)
{
	const struct elf32_hdr *ehdr = (struct elf32_hdr *)metadata;
	riva_start = ehdr->e_entry;
	return 0;
}

static int reset_riva_untrusted(void)
{
	u32 reg;
	bool xo;

	/* Enable A2XB bridge */
	reg = readl(RIVA_PMU_A2XB_CFG);
	reg |= RIVA_PMU_A2XB_CFG_EN;
	writel(reg, RIVA_PMU_A2XB_CFG);

	/* Determine which XO to use */
	reg = readl(RIVA_PMU_CFG);
	xo = (reg & RIVA_PMU_CFG_IRIS_XO_MODE) == RIVA_PMU_CFG_IRIS_XO_MODE_48;

	/* Program PLL 13 to 960 MHz */
	reg = readl(RIVA_PLL_MODE);
	reg &= ~(PLL_MODE_BYPASSNL | PLL_MODE_OUTCTRL | PLL_MODE_RESET_N);
	writel(reg, RIVA_PLL_MODE);

	if (xo)
		writel(0x40000C00 | 40, RIVA_PLL_L_VAL);
	else
		writel(0x40000C00 | 50, RIVA_PLL_L_VAL);
	writel(0, RIVA_PLL_M_VAL);
	writel(1, RIVA_PLL_N_VAL);
	writel_relaxed(0x01495227, RIVA_PLL_CONFIG);

	reg = readl(RIVA_PLL_MODE);
	reg &= ~(PLL_MODE_REF_XO_SEL);
	reg |= xo ? PLL_MODE_REF_XO_SEL_RF : PLL_MODE_REF_XO_SEL_CXO;
	writel(reg, RIVA_PLL_MODE);

	/* Enable PLL 13 */
	reg |= PLL_MODE_BYPASSNL;
	writel(reg, RIVA_PLL_MODE);

	usleep_range(10, 20);

	reg |= PLL_MODE_RESET_N;
	writel(reg, RIVA_PLL_MODE);
	reg |= PLL_MODE_OUTCTRL;
	writel(reg, RIVA_PLL_MODE);

	/* Wait for PLL to settle */
	usleep_range(50, 100);

	/* Configure cCPU for 240 MHz */
	reg = readl(RIVA_PMU_CLK_ROOT3);
	if (readl(RIVA_PMU_ROOT_CLK_SEL) & RIVA_PMU_ROOT_CLK_SEL_3) {
		reg &= ~(RIVA_PMU_CLK_ROOT3_SRC0_SEL |
			 RIVA_PMU_CLK_ROOT3_SRC0_DIV);
		reg |= RIVA_PMU_CLK_ROOT3_SRC0_SEL_RIVA |
		       RIVA_PMU_CLK_ROOT3_SRC0_DIV_2;
	} else {
		reg &= ~(RIVA_PMU_CLK_ROOT3_SRC1_SEL |
			 RIVA_PMU_CLK_ROOT3_SRC1_DIV);
		reg |= RIVA_PMU_CLK_ROOT3_SRC1_SEL_RIVA |
		       RIVA_PMU_CLK_ROOT3_SRC1_DIV_2;
	}
	writel(reg, RIVA_PMU_CLK_ROOT3);
	reg |= RIVA_PMU_CLK_ROOT3_ENA;
	writel(reg, RIVA_PMU_CLK_ROOT3);
	reg = readl(RIVA_PMU_ROOT_CLK_SEL);
	reg ^= RIVA_PMU_ROOT_CLK_SEL_3;
	writel(reg, RIVA_PMU_ROOT_CLK_SEL);

	/* Use the high vector table */
	reg = readl(RIVA_PMU_CCPU_CTL);
	reg |= RIVA_PMU_CCPU_CTL_HIGH_IVT | RIVA_PMU_CCPU_CTL_REMAP_EN;
	writel(reg, RIVA_PMU_CCPU_CTL);

	/* Set base memory address */
	writel_relaxed(riva_start >> 16, RIVA_PMU_CCPU_BOOT_REMAP_ADDR);

	/* Clear warmboot bit indicating this is a cold boot */
	reg = readl(RIVA_PMU_CFG);
	reg &= ~(RIVA_PMU_CFG_WARM_BOOT);
	writel(reg, RIVA_PMU_CFG);

	/* Enable the cCPU clock */
	reg = readl(RIVA_PMU_OVRD_VAL);
	reg |= RIVA_PMU_OVRD_VAL_CCPU_CLK;
	writel(reg, RIVA_PMU_OVRD_VAL);

	/* Take cCPU out of reset */
	reg |= RIVA_PMU_OVRD_VAL_CCPU_RESET;
	writel(reg, RIVA_PMU_OVRD_VAL);

	return 0;
}

static int shutdown_riva_untrusted(void)
{
	u32 reg;
	/* Put riva into reset */
	reg = readl(RIVA_PMU_OVRD_VAL);
	reg &= ~(RIVA_PMU_OVRD_VAL_CCPU_RESET | RIVA_PMU_OVRD_VAL_CCPU_CLK);
	writel(reg, RIVA_PMU_OVRD_VAL);
	return 0;
}

static int init_image_dsps_untrusted(const u8 *metadata, size_t size)
{
	/* Bring memory and bus interface out of reset */
	writel_relaxed(0x2, PPSS_RESET);
	writel_relaxed(0x10, PPSS_HCLK_CTL);
	return 0;
}

static int reset_dsps_untrusted(void)
{
	writel_relaxed(0x10, PPSS_PROC_CLK_CTL);
	/* Bring DSPS out of reset */
	writel_relaxed(0x0, PPSS_RESET);
	return 0;
}

static int shutdown_dsps_untrusted(void)
{
	writel_relaxed(0x2, PPSS_RESET);
	writel_relaxed(0x0, PPSS_PROC_CLK_CTL);
	return 0;
}

static struct pil_reset_ops pil_modem_fw_q6_ops = {
	.init_image = init_image_modem_fw_q6_untrusted,
	.verify_blob = verify_blob,
	.auth_and_reset = reset_modem_fw_q6_untrusted,
	.shutdown = shutdown_modem_fw_q6_untrusted,
};

static struct pil_reset_ops pil_modem_sw_q6_ops = {
	.init_image = init_image_modem_sw_q6_untrusted,
	.verify_blob = verify_blob,
	.auth_and_reset = reset_modem_sw_q6_untrusted,
	.shutdown = shutdown_modem_sw_q6_untrusted,
};

static struct pil_reset_ops pil_lpass_q6_ops = {
	.init_image = init_image_lpass_q6_untrusted,
	.verify_blob = verify_blob,
	.auth_and_reset = reset_lpass_q6_untrusted,
	.shutdown = shutdown_lpass_q6_untrusted,
};

static struct pil_reset_ops pil_riva_ops = {
	.init_image = init_image_riva_untrusted,
	.verify_blob = verify_blob,
	.auth_and_reset = reset_riva_untrusted,
	.shutdown = shutdown_riva_untrusted,
};

struct pil_reset_ops pil_dsps_ops = {
	.init_image = init_image_dsps_untrusted,
	.verify_blob = verify_blob,
	.auth_and_reset = reset_dsps_untrusted,
	.shutdown = shutdown_dsps_untrusted,
};

static struct pil_device pil_lpass_q6 = {
	.name = "q6",
	.pdev = {
		.name = "pil_lpass_q6",
		.id = -1,
	},
	.ops = &pil_lpass_q6_ops,
};

static struct pil_device pil_modem_fw_q6 = {
	.name = "modem_fw",
	.depends_on = "q6",
	.pdev = {
		.name = "pil_modem_fw_q6",
		.id = -1,
	},
	.ops = &pil_modem_fw_q6_ops,
};

static struct pil_device pil_modem_sw_q6 = {
	.name = "modem",
	.depends_on = "modem_fw",
	.pdev = {
		.name = "pil_modem_sw_q6",
		.id = -1,
	},
	.ops = &pil_modem_sw_q6_ops,
};

static struct pil_device pil_riva = {
	.name = "wcnss",
	.pdev = {
		.name = "pil_riva",
		.id = -1,
	},
	.ops = &pil_riva_ops,
};

static struct pil_device pil_dsps = {
	.name = "dsps",
	.pdev = {
		.name = "pil_dsps",
		.id = -1,
	},
	.ops = &pil_dsps_ops,
};

static int __init q6_reset_init(struct q6_data *q6)
{
	int err;

	q6->reg_base = ioremap(q6->reg_base_phys, SZ_256);
	if (!q6->reg_base) {
		err = -ENOMEM;
		goto err_map;
	}

	q6->vreg = regulator_get(NULL, q6->name);
	if (IS_ERR(q6->vreg)) {
		err = PTR_ERR(q6->vreg);
		goto err_vreg;
	}

	return 0;

err_vreg:
	iounmap(q6->reg_base);
err_map:
	return err;
}

static int __init msm_peripheral_reset_init(void)
{
	int err;

	/*
	 * Don't initialize PIL on simulated targets, as some
	 * subsystems may not be emulated on them.
	 */
	if (machine_is_msm8960_sim() || machine_is_msm8960_rumi3())
		return 0;

	err = q6_reset_init(&q6_lpass);
	if (err)
		return err;
	msm_pil_add_device(&pil_lpass_q6);

	mss_enable_reg = ioremap(MSM_MSS_ENABLE_PHYS, 4);
	if (!mss_enable_reg)
		return -ENOMEM;

	err = q6_reset_init(&q6_modem_fw);
	if (err) {
		iounmap(mss_enable_reg);
		return err;
	}
	msm_pil_add_device(&pil_modem_fw_q6);

	err = q6_reset_init(&q6_modem_sw);
	if (err)
		return err;
	msm_pil_add_device(&pil_modem_sw_q6);

	msm_pil_add_device(&pil_dsps);

	msm_riva_base = ioremap(MSM_RIVA_PHYS, SZ_256);
	if (!msm_riva_base)
		return -ENOMEM;
	msm_pil_add_device(&pil_riva);

	return 0;
}
arch_initcall(msm_peripheral_reset_init);
