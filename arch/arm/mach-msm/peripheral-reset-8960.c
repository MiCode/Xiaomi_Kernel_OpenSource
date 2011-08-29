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
#include <linux/platform_device.h>

#include <asm/mach-types.h>

#include <mach/msm_iomap.h>
#include <mach/scm.h>

#include "peripheral-loader.h"
#include "scm-pas.h"

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

static void __iomem *msm_riva_base;
static unsigned long riva_start;

static int verify_blob(struct pil_desc *pil, u32 phy_addr, size_t size)
{
	return 0;
}

static int init_image_riva_untrusted(struct pil_desc *pil, const u8 *metadata,
				     size_t size)
{
	const struct elf32_hdr *ehdr = (struct elf32_hdr *)metadata;
	riva_start = ehdr->e_entry;
	return 0;
}

static int reset_riva_untrusted(struct pil_desc *pil)
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

static int shutdown_riva_untrusted(struct pil_desc *pil)
{
	u32 reg;
	/* Put riva into reset */
	reg = readl(RIVA_PMU_OVRD_VAL);
	reg &= ~(RIVA_PMU_OVRD_VAL_CCPU_RESET | RIVA_PMU_OVRD_VAL_CCPU_CLK);
	writel(reg, RIVA_PMU_OVRD_VAL);
	return 0;
}

static int init_image_riva_trusted(struct pil_desc *pil, const u8 *metadata,
				   size_t size)
{
	return pas_init_image(PAS_RIVA, metadata, size);
}

static int reset_riva_trusted(struct pil_desc *pil)
{
	return pas_auth_and_reset(PAS_RIVA);
}

static int shutdown_riva_trusted(struct pil_desc *pil)
{
	return pas_shutdown(PAS_RIVA);
}

static int init_image_dsps_untrusted(struct pil_desc *pil, const u8 *metadata,
				     size_t size)
{
	/* Bring memory and bus interface out of reset */
	writel_relaxed(0x2, PPSS_RESET);
	writel_relaxed(0x10, PPSS_HCLK_CTL);
	return 0;
}

static int reset_dsps_untrusted(struct pil_desc *pil)
{
	writel_relaxed(0x10, PPSS_PROC_CLK_CTL);
	/* Bring DSPS out of reset */
	writel_relaxed(0x0, PPSS_RESET);
	return 0;
}

static int shutdown_dsps_untrusted(struct pil_desc *pil)
{
	writel_relaxed(0x2, PPSS_RESET);
	writel_relaxed(0x0, PPSS_PROC_CLK_CTL);
	return 0;
}

static int init_image_dsps_trusted(struct pil_desc *pil, const u8 *metadata,
				   size_t size)
{
	return pas_init_image(PAS_DSPS, metadata, size);
}

static int reset_dsps_trusted(struct pil_desc *pil)
{
	return pas_auth_and_reset(PAS_DSPS);
}

static int shutdown_dsps_trusted(struct pil_desc *pil)
{
	return pas_shutdown(PAS_DSPS);
}

static int init_image_tzapps(struct pil_desc *pil, const u8 *metadata,
			     size_t size)
{
	return pas_init_image(PAS_TZAPPS, metadata, size);
}

static int reset_tzapps(struct pil_desc *pil)
{
	return pas_auth_and_reset(PAS_TZAPPS);
}

static int shutdown_tzapps(struct pil_desc *pil)
{
	return pas_shutdown(PAS_TZAPPS);
}

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

struct pil_reset_ops pil_tzapps_ops = {
	.init_image = init_image_tzapps,
	.verify_blob = verify_blob,
	.auth_and_reset = reset_tzapps,
	.shutdown = shutdown_tzapps,
};

static struct platform_device pil_riva = {
	.name = "pil_riva",
};

static struct pil_desc pil_riva_desc = {
	.name = "wcnss",
	.dev = &pil_riva.dev,
	.ops = &pil_riva_ops,
};

static struct platform_device pil_dsps = {
	.name = "pil_dsps",
};

static struct pil_desc pil_dsps_desc = {
	.name = "dsps",
	.dev = &pil_dsps.dev,
	.ops = &pil_dsps_ops,
};

static struct platform_device pil_tzapps = {
	.name = "pil_tzapps",
};

static struct pil_desc pil_tzapps_desc = {
	.name = "tzapps",
	.dev = &pil_tzapps.dev,
	.ops = &pil_tzapps_ops,
};

static void __init use_secure_pil(void)
{
	if (pas_supported(PAS_DSPS) > 0) {
		pil_dsps_ops.init_image = init_image_dsps_trusted;
		pil_dsps_ops.auth_and_reset = reset_dsps_trusted;
		pil_dsps_ops.shutdown = shutdown_dsps_trusted;
	}

	if (pas_supported(PAS_RIVA) > 0) {
		pil_riva_ops.init_image = init_image_riva_trusted;
		pil_riva_ops.auth_and_reset = reset_riva_trusted;
		pil_riva_ops.shutdown = shutdown_riva_trusted;
	}
}

static int __init msm_peripheral_reset_init(void)
{
	/*
	 * Don't initialize PIL on simulated targets, as some
	 * subsystems may not be emulated on them.
	 */
	if (machine_is_msm8960_sim() || machine_is_msm8960_rumi3())
		return 0;

	use_secure_pil();

	BUG_ON(platform_device_register(&pil_dsps));
	BUG_ON(msm_pil_register(&pil_dsps_desc));
	BUG_ON(platform_device_register(&pil_tzapps));
	BUG_ON(msm_pil_register(&pil_tzapps_desc));

	msm_riva_base = ioremap(MSM_RIVA_PHYS, SZ_256);
	if (!msm_riva_base)
		return -ENOMEM;
	BUG_ON(platform_device_register(&pil_riva));
	BUG_ON(msm_pil_register(&pil_riva_desc));

	return 0;
}
arch_initcall(msm_peripheral_reset_init);
