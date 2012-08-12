/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include <linux/platform_device.h>

#include <linux/dma-mapping.h>
#include <mach/irqs.h>
#include <mach/msm_iomap.h>
#include <mach/dma.h>
#include <mach/board.h>

#include "devices.h"
#include "smd_private.h"
#include "clock-local.h"
#include "msm_watchdog.h"

#include <asm/mach/flash.h>
#include <asm/mach/mmc.h>

/*
 * UARTs
 */

static struct resource resources_uart1[] = {
	{
		.start	= INT_UART1,
		.end	= INT_UART1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= MSM_UART1_PHYS,
		.end	= MSM_UART1_PHYS + MSM_UART1_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device msm_device_uart1 = {
	.name	= "msm_serial",
	.id	= 0,
	.num_resources	= ARRAY_SIZE(resources_uart1),
	.resource	= resources_uart1,
};

static struct resource resources_uart2[] = {
	{
		.start	= INT_UART2,
		.end	= INT_UART2,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= MSM_UART2_PHYS,
		.end	= MSM_UART2_PHYS + MSM_UART2_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device msm_device_uart2 = {
	.name	= "msm_serial",
	.id	= 1,
	.num_resources	= ARRAY_SIZE(resources_uart2),
	.resource	= resources_uart2,
};

static struct resource resources_uart3[] = {
	{
		.start	= INT_UART3,
		.end	= INT_UART3,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= MSM_UART3_PHYS,
		.end	= MSM_UART3_PHYS + MSM_UART3_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device msm_device_uart3 = {
	.name	= "msm_uim",
	.id	= 2,
	.num_resources	= ARRAY_SIZE(resources_uart3),
	.resource	= resources_uart3,
};

/*
 * SSBIs
 */

#ifdef CONFIG_MSM_SSBI
#define MSM_SSBI1_PHYS          0x94080000
#define MSM_SSBI_PMIC1_PHYS     MSM_SSBI1_PHYS
static struct resource msm_ssbi_pmic1_resources[] = {
	{
		.start  = MSM_SSBI_PMIC1_PHYS,
		.end    = MSM_SSBI_PMIC1_PHYS + SZ_4K - 1,
		.flags  = IORESOURCE_MEM,
	},
};

struct platform_device msm_device_ssbi_pmic1 = {
	.name           = "msm_ssbi",
	.id             = 0,
	.resource       = msm_ssbi_pmic1_resources,
	.num_resources  = ARRAY_SIZE(msm_ssbi_pmic1_resources),
};
#endif

#ifdef CONFIG_I2C_SSBI
#define MSM_SSBI2_PHYS		0x94090000
#define MSM_SSBI2_SIZE		SZ_4K

static struct resource msm_ssbi2_resources[] = {
	{
		.name   = "ssbi_base",
		.start  = MSM_SSBI2_PHYS,
		.end    = MSM_SSBI2_PHYS + MSM_SSBI2_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
};

struct platform_device msm_device_ssbi2 = {
	.name		= "i2c_ssbi",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(msm_ssbi2_resources),
	.resource	= msm_ssbi2_resources,
};

#define MSM_SSBI3_PHYS		0x940c0000
#define MSM_SSBI3_SIZE		SZ_4K

static struct resource msm_ssbi3_resources[] = {
	{
		.name   = "ssbi_base",
		.start	= MSM_SSBI3_PHYS,
		.end	= MSM_SSBI3_PHYS + MSM_SSBI3_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device msm_device_ssbi3 = {
	.name		= "i2c_ssbi",
	.id		= 2,
	.num_resources	= ARRAY_SIZE(msm_ssbi3_resources),
	.resource	= msm_ssbi3_resources,
};

#endif /* CONFIG_I2C_SSBI */

/*
 * GSBI
 */

#ifdef CONFIG_I2C_QUP

#define MSM_GSBI1_PHYS		0x81200000
#define MSM_GSBI1_QUP_PHYS	0x81a00000

static struct resource gsbi1_qup_i2c_resources[] = {
	{
		.name	= "qup_phys_addr",
		.start	= MSM_GSBI1_QUP_PHYS,
		.end	= MSM_GSBI1_QUP_PHYS + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "gsbi_qup_i2c_addr",
		.start	= MSM_GSBI1_PHYS,
		.end	= MSM_GSBI1_PHYS + 4 - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "qup_err_intr",
		.start	= INT_GSBI_QUP_ERROR,
		.end	= INT_GSBI_QUP_ERROR,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm_gsbi1_qup_i2c_device = {
	.name		= "qup_i2c",
	.id		= 3,
	.num_resources	= ARRAY_SIZE(gsbi1_qup_i2c_resources),
	.resource	= gsbi1_qup_i2c_resources,
};

#endif /* CONFIG_I2C_QUP */

/*
 * NAND
 */

#define MSM_NAND_PHYS		0x81600000
#define MSM_NAND_SIZE		SZ_4K
#define MSM_EBI2_CTRL_PHYS      0x81400000
#define MSM_EBI2_CTRL_SIZE	SZ_4K

static struct resource resources_nand[] = {
	[0] = {
		.name   = "msm_nand_dmac",
		.start	= DMOV_NAND_CHAN,
		.end	= DMOV_NAND_CHAN,
		.flags	= IORESOURCE_DMA,
	},
	[1] = {
		.name   = "msm_nand_phys",
		.start  = MSM_NAND_PHYS,
		.end    = MSM_NAND_PHYS + MSM_NAND_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	[3] = {
		.name   = "ebi2_reg_base",
		.start  = MSM_EBI2_CTRL_PHYS,
		.end    = MSM_EBI2_CTRL_PHYS + MSM_EBI2_CTRL_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
};

struct flash_platform_data msm_nand_data = {
	.parts		= NULL,
	.nr_parts	= 0,
	.interleave     = 0,
};

struct platform_device msm_device_nand = {
	.name		= "msm_nand",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_nand),
	.resource	= resources_nand,
	.dev		= {
		.platform_data	= &msm_nand_data,
	},
};

/*
 * SMD
 */

struct platform_device msm_device_smd = {
	.name	= "msm_smd",
	.id	= -1,
};

/*
 * ADM
 */

static struct resource msm_dmov_resource[] = {
	{
		.start = INT_ADM_AARM,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = 0x94610000,
		.end = 0x94610000 + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
};

static struct msm_dmov_pdata msm_dmov_pdata = {
	.sd = 3,
	.sd_size = 0x400,
};

struct platform_device msm_device_dmov = {
	.name	= "msm_dmov",
	.id	= -1,
	.resource = msm_dmov_resource,
	.num_resources = ARRAY_SIZE(msm_dmov_resource),
	.dev = {
		.platform_data = &msm_dmov_pdata,
	},
};

/*
 * SDC
 */

#define MSM_SDC1_PHYS		0x80A00000
#define MSM_SDC1_SIZE		SZ_4K

static struct resource resources_sdc1[] = {
	{
		.start	= MSM_SDC1_PHYS,
		.end	= MSM_SDC1_PHYS + MSM_SDC1_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_SDC1_0,
		.end	= INT_SDC1_1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= DMOV_SDC1_CHAN,
		.end	= DMOV_SDC1_CHAN,
		.flags	= IORESOURCE_DMA,
	},
};

struct platform_device msm_device_sdc1 = {
	.name		= "msm_sdcc",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(resources_sdc1),
	.resource	= resources_sdc1,
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};

static struct platform_device *msm_sdcc_devices[] __initdata = {
	&msm_device_sdc1,
};

int __init msm_add_sdcc(unsigned int controller, struct mmc_platform_data *plat)
{
	struct platform_device	*pdev;

	if (controller != 1)
		return -EINVAL;

	pdev = msm_sdcc_devices[controller-1];
	pdev->dev.platform_data = plat;
	return platform_device_register(pdev);
}

/*
 * QFEC
 */

# define QFEC_MAC_IRQ           INT_SBD_IRQ
# define QFEC_MAC_BASE          0x40000000
# define QFEC_CLK_BASE          0x94020000

# define QFEC_MAC_SIZE          0x2000
# define QFEC_CLK_SIZE          0x18100

# define QFEC_MAC_FUSE_BASE     0x80004210
# define QFEC_MAC_FUSE_SIZE     16

static struct resource qfec_resources[] = {
	[0] = {
		.start = QFEC_MAC_BASE,
		.end   = QFEC_MAC_BASE + QFEC_MAC_SIZE,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = QFEC_MAC_IRQ,
		.end   = QFEC_MAC_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	[2] = {
		.start = QFEC_CLK_BASE,
		.end   = QFEC_CLK_BASE + QFEC_CLK_SIZE,
		.flags = IORESOURCE_IO,
	},
	[3] = {
		.start = QFEC_MAC_FUSE_BASE,
		.end   = QFEC_MAC_FUSE_BASE + QFEC_MAC_FUSE_SIZE,
		.flags = IORESOURCE_DMA,
	},
};

struct platform_device qfec_device = {
	.name           = "qfec",
	.id             = 0,
	.num_resources  = ARRAY_SIZE(qfec_resources),
	.resource       = qfec_resources,
};

/*
 * FUSE
 */

#if defined(CONFIG_QFP_FUSE)

char fuse_regulator_name[] = "8058_lvs0";

struct resource qfp_fuse_resources[] = {
	{
		.start = (uint32_t) MSM_QFP_FUSE_BASE,
		.end   = (uint32_t) MSM_QFP_FUSE_BASE + MSM_QFP_FUSE_SIZE,
		.flags = IORESOURCE_MEM,
	},
};

struct platform_device fsm_qfp_fuse_device = {
	.name           = "qfp_fuse_driver",
	.id             = 0,
	.dev = {.platform_data = fuse_regulator_name},
	.num_resources  = ARRAY_SIZE(qfp_fuse_resources),
	.resource       = qfp_fuse_resources,
};

#endif

/*
 * XO
 */

struct platform_device fsm_xo_device = {
	.name   = "fsm_xo_driver",
	.id     = -1,
};

/*
 * Watchdog
 */

static struct msm_watchdog_pdata fsm_watchdog_pdata = {
	.pet_time = 10000,
	.bark_time = 11000,
	.has_secure = false,
	.has_vic = true,
	.base = MSM_TMR_BASE + WDT1_OFFSET,
};

static struct resource msm_watchdog_resources[] = {
	{
		.start	= INT_WDT1_ACCSCSSBARK,
		.end	= INT_WDT1_ACCSCSSBARK,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device fsm9xxx_device_watchdog = {
	.name = "msm_watchdog",
	.id = -1,
	.dev = {
		.platform_data = &fsm_watchdog_pdata,
	},
	.num_resources	= ARRAY_SIZE(msm_watchdog_resources),
	.resource	= msm_watchdog_resources,
};

