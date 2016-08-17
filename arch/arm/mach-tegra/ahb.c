/*
 * arch/arm/mach-tegra/ahb.c
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (C) 2011-2013 NVIDIA CORPORATION. All rights reserved.
 *
 * Author:
 *	Jay Cheng <jacheng@nvidia.com>
 *	James Wylder <james.wylder@motorola.com>
 *	Benoit Goby <benoit@android.com>
 *	Colin Cross <ccross@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/syscore_ops.h>

#include <mach/iomap.h>
#include <mach/hardware.h>

#define AHB_ARBITRATION_DISABLE		0x00
#define AHB_ARBITRATION_PRIORITY_CTRL	0x04
#define   AHB_PRIORITY_WEIGHT(x)	(((x) & 0x7) << 29)
#define   PRIORITY_SELECT_USB BIT(6)
#define   PRIORITY_SELECT_USB2 BIT(18)
#define   PRIORITY_SELECT_USB3 BIT(17)

#define AHB_GIZMO_AHB_MEM		0x0c
#define   ENB_FAST_REARBITRATE BIT(2)
#define   DONT_SPLIT_AHB_WR     BIT(7)
#define   WR_WAIT_COMMIT_ON_1K	BIT(8)

#define AHB_GIZMO_APB_DMA		0x10
#define AHB_GIZMO_IDE			0x18
#define AHB_GIZMO_USB			0x1c
#define AHB_GIZMO_AHB_XBAR_BRIDGE	0x20
#define AHB_GIZMO_CPU_AHB_BRIDGE	0x24
#define AHB_GIZMO_COP_AHB_BRIDGE	0x28
#define AHB_GIZMO_XBAR_APB_CTLR		0x2c
#define AHB_GIZMO_VCP_AHB_BRIDGE	0x30
#define AHB_GIZMO_NAND			0x3c
#define AHB_GIZMO_SDMMC4		0x44
#define AHB_GIZMO_XIO			0x48
#if !defined(CONFIG_ARCH_TEGRA_3x_SOC)
#define AHB_GIZMO_SE			0x4c
#define AHB_GIZMO_TZRAM			0x50
#endif
#define AHB_GIZMO_BSEV			0x60
#define AHB_GIZMO_BSEA			0x70
#define AHB_GIZMO_NOR			0x74
#define AHB_GIZMO_USB2			0x78
#define AHB_GIZMO_USB3			0x7c
#define   IMMEDIATE	BIT(18)

#define AHB_GIZMO_SDMMC1		0x80
#define AHB_GIZMO_SDMMC2		0x84
#define AHB_GIZMO_SDMMC3		0x88
#define AHB_MEM_PREFETCH_CFG_X		0xd8
#define AHB_ARBITRATION_XBAR_CTRL	0xdc
#ifdef CONFIG_ARCH_TEGRA_11x_SOC
#define AHB_MEM_PREFETCH_CFG5		0xc8
#define AHB_MEM_PREFETCH_CFG6		0xcc
#endif
#define AHB_MEM_PREFETCH_CFG3		0xe0
#define AHB_MEM_PREFETCH_CFG4		0xe4
#define AHB_MEM_PREFETCH_CFG1		0xec
#define AHB_MEM_PREFETCH_CFG2		0xf0
#define   PREFETCH_ENB	BIT(31)
#define   MST_ID(x)	(((x) & 0x1f) << 26)
#define   AHBDMA_MST_ID	MST_ID(5)
#define   USB_MST_ID	MST_ID(6)
#define   USB2_MST_ID	MST_ID(18)
#define   USB3_MST_ID	MST_ID(17)
#define   ADDR_BNDRY(x)	(((x) & 0xf) << 21)
#define   INACTIVITY_TIMEOUT(x)	(((x) & 0xffff) << 0)

#define AHB_ARBITRATION_AHB_MEM_WRQUE_MST_ID	0xf8


static inline unsigned long gizmo_readl(unsigned long offset)
{
	return readl(IO_TO_VIRT(TEGRA_AHB_GIZMO_BASE + offset));
}

static inline void gizmo_writel(unsigned long value, unsigned long offset)
{
	writel(value, IO_TO_VIRT(TEGRA_AHB_GIZMO_BASE + offset));
}

#ifdef CONFIG_PM

#if !defined(CONFIG_ARCH_TEGRA_3x_SOC)
static u32 ahb_gizmo[33];
#else
static u32 ahb_gizmo[29];
#endif

int tegra_ahbgizmo_suspend(void)
{
	ahb_gizmo[0] = gizmo_readl(AHB_ARBITRATION_DISABLE);
	ahb_gizmo[1] = gizmo_readl(AHB_ARBITRATION_PRIORITY_CTRL);
	ahb_gizmo[2] = gizmo_readl(AHB_GIZMO_AHB_MEM);
	ahb_gizmo[3] = gizmo_readl(AHB_GIZMO_APB_DMA);
	ahb_gizmo[4] = gizmo_readl(AHB_GIZMO_IDE);
	ahb_gizmo[5] = gizmo_readl(AHB_GIZMO_USB);
	ahb_gizmo[6] = gizmo_readl(AHB_GIZMO_AHB_XBAR_BRIDGE);
	ahb_gizmo[7] = gizmo_readl(AHB_GIZMO_CPU_AHB_BRIDGE);
	ahb_gizmo[8] = gizmo_readl(AHB_GIZMO_COP_AHB_BRIDGE);
	ahb_gizmo[9] = gizmo_readl(AHB_GIZMO_XBAR_APB_CTLR);
	ahb_gizmo[10] = gizmo_readl(AHB_GIZMO_VCP_AHB_BRIDGE);
	ahb_gizmo[11] = gizmo_readl(AHB_GIZMO_NAND);
	ahb_gizmo[12] = gizmo_readl(AHB_GIZMO_SDMMC4);
	ahb_gizmo[13] = gizmo_readl(AHB_GIZMO_XIO);
	ahb_gizmo[14] = gizmo_readl(AHB_GIZMO_BSEV);
	ahb_gizmo[15] = gizmo_readl(AHB_GIZMO_BSEA);
	ahb_gizmo[16] = gizmo_readl(AHB_GIZMO_NOR);
	ahb_gizmo[17] = gizmo_readl(AHB_GIZMO_USB2);
	ahb_gizmo[18] = gizmo_readl(AHB_GIZMO_USB3);
	ahb_gizmo[19] = gizmo_readl(AHB_GIZMO_SDMMC1);
	ahb_gizmo[20] = gizmo_readl(AHB_GIZMO_SDMMC2);
	ahb_gizmo[21] = gizmo_readl(AHB_GIZMO_SDMMC3);
	ahb_gizmo[22] = gizmo_readl(AHB_MEM_PREFETCH_CFG_X);
	ahb_gizmo[23] = gizmo_readl(AHB_ARBITRATION_XBAR_CTRL);
	ahb_gizmo[24] = gizmo_readl(AHB_MEM_PREFETCH_CFG3);
	ahb_gizmo[25] = gizmo_readl(AHB_MEM_PREFETCH_CFG4);
	ahb_gizmo[26] = gizmo_readl(AHB_MEM_PREFETCH_CFG1);
	ahb_gizmo[27] = gizmo_readl(AHB_MEM_PREFETCH_CFG2);
	ahb_gizmo[28] = gizmo_readl(AHB_ARBITRATION_AHB_MEM_WRQUE_MST_ID);
#if !defined(CONFIG_ARCH_TEGRA_3x_SOC)
	ahb_gizmo[29] = gizmo_readl(AHB_MEM_PREFETCH_CFG5);
	ahb_gizmo[30] = gizmo_readl(AHB_MEM_PREFETCH_CFG6);
	ahb_gizmo[31] = gizmo_readl(AHB_GIZMO_SE);
	ahb_gizmo[32] = gizmo_readl(AHB_GIZMO_TZRAM);
#endif
	return 0;
}

void tegra_ahbgizmo_resume(void)
{
	gizmo_writel(ahb_gizmo[0],  AHB_ARBITRATION_DISABLE);
	gizmo_writel(ahb_gizmo[1],  AHB_ARBITRATION_PRIORITY_CTRL);
	gizmo_writel(ahb_gizmo[2],  AHB_GIZMO_AHB_MEM);
	gizmo_writel(ahb_gizmo[3],  AHB_GIZMO_APB_DMA);
	gizmo_writel(ahb_gizmo[4],  AHB_GIZMO_IDE);
	gizmo_writel(ahb_gizmo[5],  AHB_GIZMO_USB);
	gizmo_writel(ahb_gizmo[6],  AHB_GIZMO_AHB_XBAR_BRIDGE);
	gizmo_writel(ahb_gizmo[7],  AHB_GIZMO_CPU_AHB_BRIDGE);
	gizmo_writel(ahb_gizmo[8],  AHB_GIZMO_COP_AHB_BRIDGE);
	gizmo_writel(ahb_gizmo[9],  AHB_GIZMO_XBAR_APB_CTLR);
	gizmo_writel(ahb_gizmo[10], AHB_GIZMO_VCP_AHB_BRIDGE);
	gizmo_writel(ahb_gizmo[11], AHB_GIZMO_NAND);
	gizmo_writel(ahb_gizmo[12], AHB_GIZMO_SDMMC4);
	gizmo_writel(ahb_gizmo[13], AHB_GIZMO_XIO);
	gizmo_writel(ahb_gizmo[14], AHB_GIZMO_BSEV);
	gizmo_writel(ahb_gizmo[15], AHB_GIZMO_BSEA);
	gizmo_writel(ahb_gizmo[16], AHB_GIZMO_NOR);
	gizmo_writel(ahb_gizmo[17], AHB_GIZMO_USB2);
	gizmo_writel(ahb_gizmo[18], AHB_GIZMO_USB3);
	gizmo_writel(ahb_gizmo[19], AHB_GIZMO_SDMMC1);
	gizmo_writel(ahb_gizmo[20], AHB_GIZMO_SDMMC2);
	gizmo_writel(ahb_gizmo[21], AHB_GIZMO_SDMMC3);
	gizmo_writel(ahb_gizmo[22], AHB_MEM_PREFETCH_CFG_X);
	gizmo_writel(ahb_gizmo[23], AHB_ARBITRATION_XBAR_CTRL);
	gizmo_writel(ahb_gizmo[24], AHB_MEM_PREFETCH_CFG3);
	gizmo_writel(ahb_gizmo[25], AHB_MEM_PREFETCH_CFG4);
	gizmo_writel(ahb_gizmo[26], AHB_MEM_PREFETCH_CFG1);
	gizmo_writel(ahb_gizmo[27], AHB_MEM_PREFETCH_CFG2);
	gizmo_writel(ahb_gizmo[28], AHB_ARBITRATION_AHB_MEM_WRQUE_MST_ID);
#if !defined(CONFIG_ARCH_TEGRA_3x_SOC)
	gizmo_writel(ahb_gizmo[29], AHB_MEM_PREFETCH_CFG5);
	gizmo_writel(ahb_gizmo[30], AHB_MEM_PREFETCH_CFG6);
	gizmo_writel(ahb_gizmo[31], AHB_GIZMO_SE);
	gizmo_writel(ahb_gizmo[32], AHB_GIZMO_TZRAM);
#endif
}
#else
#define tegra_ahbgizmo_suspend NULL
#define tegra_ahbgizmo_resume NULL
#endif

static struct syscore_ops tegra_ahbgizmo_syscore_ops = {
	.suspend = tegra_ahbgizmo_suspend,
	.resume = tegra_ahbgizmo_resume,
};

static int __init tegra_init_ahb_gizmo(void)
{
	register_syscore_ops(&tegra_ahbgizmo_syscore_ops);

	return 0;
}
postcore_initcall(tegra_init_ahb_gizmo);
