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

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/elf.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>

#include <asm/mach-types.h>

#include <mach/scm.h>
#include <mach/msm_iomap.h>
#include <mach/msm_xo.h>

#include "peripheral-loader.h"
#include "scm-pas.h"

#define PROXY_VOTE_TIMEOUT		10000

#define MSM_MMS_REGS_BASE		0x10200000

#define MARM_RESET			(MSM_CLK_CTL_BASE + 0x2BD4)
#define MARM_BOOT_CONTROL		(msm_mms_regs_base + 0x0010)
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

#define PPSS_RESET			(MSM_CLK_CTL_BASE + 0x2594)
#define PPSS_PROC_CLK_CTL		(MSM_CLK_CTL_BASE + 0x2588)
#define CLK_HALT_DFAB_STATE		(MSM_CLK_CTL_BASE + 0x2FC8)

static int modem_start, dsps_start;
static void __iomem *msm_mms_regs_base;

static int init_image_modem_trusted(struct pil_desc *pil, const u8 *metadata,
				    size_t size)
{
	return pas_init_image(PAS_MODEM, metadata, size);
}

static int init_image_modem_untrusted(struct pil_desc *pil,
				      const u8 *metadata, size_t size)
{
	struct elf32_hdr *ehdr = (struct elf32_hdr *)metadata;
	modem_start = ehdr->e_entry;
	return 0;
}

static int init_image_dsps_trusted(struct pil_desc *pil, const u8 *metadata,
				   size_t size)
{
	return pas_init_image(PAS_DSPS, metadata, size);
}

static int init_image_dsps_untrusted(struct pil_desc *pil, const u8 *metadata,
				     size_t size)
{
	struct elf32_hdr *ehdr = (struct elf32_hdr *)metadata;
	dsps_start = ehdr->e_entry;
	/* Bring memory and bus interface out of reset */
	__raw_writel(0x2, PPSS_RESET);
	mb();
	return 0;
}

static int verify_blob(struct pil_desc *pil, u32 phy_addr, size_t size)
{
	return 0;
}

static struct msm_xo_voter *pxo;
static void remove_modem_proxy_votes(unsigned long data)
{
	msm_xo_mode_vote(pxo, MSM_XO_MODE_OFF);
}
static DEFINE_TIMER(modem_timer, remove_modem_proxy_votes, 0, 0);

static void make_modem_proxy_votes(void)
{
	/* Make proxy votes for modem and set up timer to disable it. */
	msm_xo_mode_vote(pxo, MSM_XO_MODE_ON);
	mod_timer(&modem_timer, jiffies + msecs_to_jiffies(PROXY_VOTE_TIMEOUT));
}

static void remove_modem_proxy_votes_now(void)
{
	/*
	 * If the modem proxy vote hasn't been removed yet, them remove the
	 * votes immediately.
	 */
	if (del_timer(&modem_timer))
		remove_modem_proxy_votes(0);
}

static int reset_modem_untrusted(struct pil_desc *pil)
{
	u32 reg;

	make_modem_proxy_votes();

	/* Put modem AHB0,1,2 clocks into reset */
	__raw_writel(BIT(0) | BIT(1), MAHB0_SFAB_PORT_RESET);
	__raw_writel(BIT(7), MAHB1_CLK_CTL);
	__raw_writel(BIT(7), MAHB2_CLK_CTL);

	/* Vote for pll8 on behalf of the modem */
	reg = __raw_readl(PLL_ENA_MARM);
	reg |= BIT(8);
	__raw_writel(reg, PLL_ENA_MARM);

	/* Wait for PLL8 to enable */
	while (!(__raw_readl(PLL8_STATUS) & BIT(16)))
		cpu_relax();

	/* Set MAHB1 divider to Div-5 to run MAHB1,2 and sfab at 79.8 Mhz*/
	__raw_writel(0x4, MAHB1_NS);

	/* Vote for modem AHB1 and 2 clocks to be on on behalf of the modem */
	reg = __raw_readl(MARM_CLK_BRANCH_ENA_VOTE);
	reg |= BIT(0) | BIT(1);
	__raw_writel(reg, MARM_CLK_BRANCH_ENA_VOTE);

	/* Source marm_clk off of PLL8 */
	reg = __raw_readl(MARM_CLK_SRC_CTL);
	if ((reg & 0x1) == 0) {
		__raw_writel(0x3, MARM_CLK_SRC1_NS);
		reg |= 0x1;
	} else {
		__raw_writel(0x3, MARM_CLK_SRC0_NS);
		reg &= ~0x1;
	}
	__raw_writel(reg | 0x2, MARM_CLK_SRC_CTL);

	/*
	 * Force core on and periph on signals to remain active during halt
	 * for marm_clk and mahb2_clk
	 */
	__raw_writel(0x6F, MARM_CLK_FS);
	__raw_writel(0x6F, MAHB2_CLK_FS);

	/*
	 * Enable all of the marm_clk branches, cxo sourced marm branches,
	 * and sleep clock branches
	 */
	__raw_writel(0x10, MARM_CLK_CTL);
	__raw_writel(0x10, MAHB0_CLK_CTL);
	__raw_writel(0x10, SFAB_MSS_S_HCLK_CTL);
	__raw_writel(0x10, MSS_MODEM_CXO_CLK_CTL);
	__raw_writel(0x10, MSS_SLP_CLK_CTL);
	__raw_writel(0x10, MSS_MARM_SYS_REF_CLK_CTL);

	/* Wait for above clocks to be turned on */
	while (__raw_readl(CLK_HALT_MSS_SMPSS_MISC_STATE) & (BIT(7) | BIT(8) |
				BIT(9) | BIT(10) | BIT(4) | BIT(6)))
		cpu_relax();

	/* Take MAHB0,1,2 clocks out of reset */
	__raw_writel(0x0, MAHB2_CLK_CTL);
	__raw_writel(0x0, MAHB1_CLK_CTL);
	__raw_writel(0x0, MAHB0_SFAB_PORT_RESET);

	/* Setup exception vector table base address */
	__raw_writel(modem_start | 0x1, MARM_BOOT_CONTROL);

	/* Wait for vector table to be setup */
	mb();

	/* Bring modem out of reset */
	__raw_writel(0x0, MARM_RESET);

	return 0;
}

static int reset_modem_trusted(struct pil_desc *pil)
{
	int ret;

	make_modem_proxy_votes();

	ret = pas_auth_and_reset(PAS_MODEM);
	if (ret)
		remove_modem_proxy_votes_now();

	return ret;
}

static int shutdown_modem_untrusted(struct pil_desc *pil)
{
	u32 reg;

	/* Put modem into reset */
	__raw_writel(0x1, MARM_RESET);
	mb();

	/* Put modem AHB0,1,2 clocks into reset */
	__raw_writel(BIT(0) | BIT(1), MAHB0_SFAB_PORT_RESET);
	__raw_writel(BIT(7), MAHB1_CLK_CTL);
	__raw_writel(BIT(7), MAHB2_CLK_CTL);
	mb();

	/*
	 * Disable all of the marm_clk branches, cxo sourced marm branches,
	 * and sleep clock branches
	 */
	__raw_writel(0x0, MARM_CLK_CTL);
	__raw_writel(0x0, MAHB0_CLK_CTL);
	__raw_writel(0x0, SFAB_MSS_S_HCLK_CTL);
	__raw_writel(0x0, MSS_MODEM_CXO_CLK_CTL);
	__raw_writel(0x0, MSS_SLP_CLK_CTL);
	__raw_writel(0x0, MSS_MARM_SYS_REF_CLK_CTL);

	/* Disable marm_clk */
	reg = __raw_readl(MARM_CLK_SRC_CTL);
	reg &= ~0x2;
	__raw_writel(reg, MARM_CLK_SRC_CTL);

	/* Clear modem's votes for ahb clocks */
	__raw_writel(0x0, MARM_CLK_BRANCH_ENA_VOTE);

	/* Clear modem's votes for PLLs */
	__raw_writel(0x0, PLL_ENA_MARM);

	remove_modem_proxy_votes_now();

	return 0;
}

static int shutdown_modem_trusted(struct pil_desc *pil)
{
	int ret;

	ret = pas_shutdown(PAS_MODEM);
	if (ret)
		return ret;

	remove_modem_proxy_votes_now();

	return 0;
}

static int reset_dsps_untrusted(struct pil_desc *pil)
{
	__raw_writel(0x10, PPSS_PROC_CLK_CTL);
	while (__raw_readl(CLK_HALT_DFAB_STATE) & BIT(18))
		cpu_relax();

	/* Bring DSPS out of reset */
	__raw_writel(0x0, PPSS_RESET);
	return 0;
}

static int reset_dsps_trusted(struct pil_desc *pil)
{
	return pas_auth_and_reset(PAS_DSPS);
}

static int shutdown_dsps_trusted(struct pil_desc *pil)
{
	return pas_shutdown(PAS_DSPS);
}

static int shutdown_dsps_untrusted(struct pil_desc *pil)
{
	__raw_writel(0x2, PPSS_RESET);
	__raw_writel(0x0, PPSS_PROC_CLK_CTL);
	return 0;
}

static int init_image_playready(struct pil_desc *pil, const u8 *metadata,
		size_t size)
{
	return pas_init_image(PAS_PLAYREADY, metadata, size);
}

static int reset_playready(struct pil_desc *pil)
{
	return pas_auth_and_reset(PAS_PLAYREADY);
}

static int shutdown_playready(struct pil_desc *pil)
{
	return pas_shutdown(PAS_PLAYREADY);
}

struct pil_reset_ops pil_modem_ops = {
	.init_image = init_image_modem_untrusted,
	.verify_blob = verify_blob,
	.auth_and_reset = reset_modem_untrusted,
	.shutdown = shutdown_modem_untrusted,
};

struct pil_reset_ops pil_dsps_ops = {
	.init_image = init_image_dsps_untrusted,
	.verify_blob = verify_blob,
	.auth_and_reset = reset_dsps_untrusted,
	.shutdown = shutdown_dsps_untrusted,
};

struct pil_reset_ops pil_playready_ops = {
	.init_image = init_image_playready,
	.verify_blob = verify_blob,
	.auth_and_reset = reset_playready,
	.shutdown = shutdown_playready,
};

static struct platform_device pil_modem = {
	.name = "pil_modem",
};

static struct pil_desc pil_modem_desc = {
	.name = "modem",
	.depends_on = "q6",
	.dev = &pil_modem.dev,
	.ops = &pil_modem_ops,
};

static struct platform_device pil_playready = {
	.name = "pil_playready",
};

static struct pil_desc pil_playready_desc = {
	.name = "tzapps",
	.dev = &pil_playready.dev,
	.ops = &pil_playready_ops,
};

static struct platform_device pil_dsps = {
	.name = "pil_dsps",
};

static struct pil_desc pil_dsps_desc = {
	.name = "dsps",
	.dev = &pil_dsps.dev,
	.ops = &pil_dsps_ops,
};

static int __init msm_peripheral_reset_init(void)
{
	msm_mms_regs_base = ioremap(MSM_MMS_REGS_BASE, SZ_256);
	if (!msm_mms_regs_base)
		goto err;

	pxo = msm_xo_get(MSM_XO_PXO, "pil");
	if (IS_ERR(pxo))
		goto err_pxo;

	if (pas_supported(PAS_MODEM) > 0) {
		pil_modem_ops.init_image = init_image_modem_trusted;
		pil_modem_ops.auth_and_reset = reset_modem_trusted;
		pil_modem_ops.shutdown = shutdown_modem_trusted;
	}

	if (pas_supported(PAS_DSPS) > 0) {
		pil_dsps_ops.init_image = init_image_dsps_trusted;
		pil_dsps_ops.auth_and_reset = reset_dsps_trusted;
		pil_dsps_ops.shutdown = shutdown_dsps_trusted;
	}

	BUG_ON(platform_device_register(&pil_modem));
	BUG_ON(msm_pil_register(&pil_modem_desc));
	BUG_ON(platform_device_register(&pil_playready));
	BUG_ON(msm_pil_register(&pil_playready_desc));

	if (machine_is_msm8x60_fluid())
		pil_dsps_desc.name = "dsps_fluid";
	BUG_ON(platform_device_register(&pil_dsps));
	BUG_ON(msm_pil_register(&pil_dsps_desc));

	return 0;

err_pxo:
	iounmap(msm_mms_regs_base);
err:
	return -ENOMEM;
}

static void __exit msm_peripheral_reset_exit(void)
{
	iounmap(msm_mms_regs_base);
}

arch_initcall(msm_peripheral_reset_init);
module_exit(msm_peripheral_reset_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Validate and bring peripherals out of reset");
