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
 *
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/msm_tsens.h>
#include <asm/hardware/gic.h>
#include <asm/mach/flash.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <mach/irqs.h>
#include <mach/socinfo.h>
#include <asm/hardware/cache-l2x0.h>
#include <mach/msm_sps.h>
#include <mach/dma.h>
#include "devices.h"
#include "acpuclock.h"

/* Address of GSBI blocks */
#define MSM_GSBI1_PHYS          0x16000000
#define MSM_GSBI2_PHYS          0x16100000
#define MSM_GSBI3_PHYS          0x16200000
#define MSM_GSBI4_PHYS		0x16300000
#define MSM_GSBI5_PHYS          0x16400000

#define MSM_UART4DM_PHYS	(MSM_GSBI4_PHYS + 0x40000)

/* GSBI QUP devices */
#define MSM_GSBI1_QUP_PHYS      (MSM_GSBI1_PHYS + 0x80000)
#define MSM_GSBI2_QUP_PHYS      (MSM_GSBI2_PHYS + 0x80000)
#define MSM_GSBI3_QUP_PHYS      (MSM_GSBI3_PHYS + 0x80000)
#define MSM_GSBI4_QUP_PHYS      (MSM_GSBI4_PHYS + 0x80000)
#define MSM_GSBI5_QUP_PHYS      (MSM_GSBI5_PHYS + 0x80000)
#define MSM_QUP_SIZE            SZ_4K

/* Address of SSBI CMD */
#define MSM_PMIC1_SSBI_CMD_PHYS	0x00500000
#define MSM_PMIC_SSBI_SIZE	SZ_4K

static struct resource msm_dmov_resource[] = {
	{
		.start = ADM_0_SCSS_1_IRQ,
		.end = (resource_size_t)MSM_DMOV_BASE,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device msm9615_device_dmov = {
	.name	= "msm_dmov",
	.id	= -1,
	.resource = msm_dmov_resource,
	.num_resources = ARRAY_SIZE(msm_dmov_resource),
};

static struct resource resources_uart_gsbi4[] = {
	{
		.start	= GSBI4_UARTDM_IRQ,
		.end	= GSBI4_UARTDM_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= MSM_UART4DM_PHYS,
		.end	= MSM_UART4DM_PHYS + PAGE_SIZE - 1,
		.name	= "uartdm_resource",
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= MSM_GSBI4_PHYS,
		.end	= MSM_GSBI4_PHYS + PAGE_SIZE - 1,
		.name	= "gsbi_resource",
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device msm9615_device_uart_gsbi4 = {
	.name	= "msm_serial_hsl",
	.id	= 0,
	.num_resources	= ARRAY_SIZE(resources_uart_gsbi4),
	.resource	= resources_uart_gsbi4,
};

static struct resource resources_qup_i2c_gsbi5[] = {
	{
		.name	= "gsbi_qup_i2c_addr",
		.start	= MSM_GSBI5_PHYS,
		.end	= MSM_GSBI5_PHYS + 4 - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "qup_phys_addr",
		.start	= MSM_GSBI5_QUP_PHYS,
		.end	= MSM_GSBI5_QUP_PHYS + MSM_QUP_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "qup_err_intr",
		.start	= GSBI5_QUP_IRQ,
		.end	= GSBI5_QUP_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm9615_device_qup_i2c_gsbi5 = {
	.name		= "qup_i2c",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(resources_qup_i2c_gsbi5),
	.resource	= resources_qup_i2c_gsbi5,
};

static struct resource resources_qup_spi_gsbi3[] = {
	{
		.name   = "spi_base",
		.start  = MSM_GSBI3_QUP_PHYS,
		.end    = MSM_GSBI3_QUP_PHYS + SZ_4K - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "gsbi_base",
		.start  = MSM_GSBI3_PHYS,
		.end    = MSM_GSBI3_PHYS + 4 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "spi_irq_in",
		.start  = GSBI3_QUP_IRQ,
		.end    = GSBI3_QUP_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device msm9615_device_qup_spi_gsbi3 = {
	.name		= "spi_qsd",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(resources_qup_spi_gsbi3),
	.resource	= resources_qup_spi_gsbi3,
};

static struct resource resources_ssbi_pmic1[] = {
	{
		.start  = MSM_PMIC1_SSBI_CMD_PHYS,
		.end    = MSM_PMIC1_SSBI_CMD_PHYS + MSM_PMIC_SSBI_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
};

struct platform_device msm9615_device_ssbi_pmic1 = {
	.name           = "msm_ssbi",
	.id             = 0,
	.resource       = resources_ssbi_pmic1,
	.num_resources  = ARRAY_SIZE(resources_ssbi_pmic1),
};

static struct resource resources_sps[] = {
	{
		.name	= "pipe_mem",
		.start	= 0x12800000,
		.end	= 0x12800000 + 0x4000 - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "bamdma_dma",
		.start	= 0x12240000,
		.end	= 0x12240000 + 0x1000 - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "bamdma_bam",
		.start	= 0x12244000,
		.end	= 0x12244000 + 0x4000 - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "bamdma_irq",
		.start	= SPS_BAM_DMA_IRQ,
		.end	= SPS_BAM_DMA_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct msm_sps_platform_data msm_sps_pdata = {
	.bamdma_restricted_pipes = 0x06,
};

struct platform_device msm_device_sps = {
	.name		= "msm_sps",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_sps),
	.resource	= resources_sps,
	.dev.platform_data = &msm_sps_pdata,
};

static struct tsens_platform_data msm_tsens_pdata = {
	.slope			= 910,
	.tsens_factor		= 1000,
	.hw_type		= MSM_9615,
	.tsens_num_sensor	= 5,
};

struct platform_device msm9615_device_tsens = {
	.name		= "tsens8960-tm",
	.id		= -1,
	.dev 		= {
		.platform_data = &msm_tsens_pdata,
	},
};

#define MSM_NAND_PHYS		0x1B400000
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
		.end    = MSM_NAND_PHYS + 0x7FF,
		.flags  = IORESOURCE_MEM,
	},
};

struct flash_platform_data msm_nand_data = {
	.parts		= NULL,
	.nr_parts	= 0,
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

struct platform_device msm_device_smd = {
	.name		= "msm_smd",
	.id		= -1,
};


#define MSM_SDC1_BASE         0x12180000
#define MSM_SDC1_DML_BASE     (MSM_SDC1_BASE + 0x800)
#define MSM_SDC1_BAM_BASE     (MSM_SDC1_BASE + 0x2000)
#define MSM_SDC2_BASE         0x12140000
#define MSM_SDC2_DML_BASE     (MSM_SDC2_BASE + 0x800)
#define MSM_SDC2_BAM_BASE     (MSM_SDC2_BASE + 0x2000)

static struct resource resources_sdc1[] = {
	{
		.name   = "core_mem",
		.flags  = IORESOURCE_MEM,
		.start  = MSM_SDC1_BASE,
		.end    = MSM_SDC1_DML_BASE - 1,
	},
	{
		.name   = "core_irq",
		.flags  = IORESOURCE_IRQ,
		.start  = SDC1_IRQ_0,
		.end    = SDC1_IRQ_0
	},
#ifdef CONFIG_MMC_MSM_SPS_SUPPORT
	{
		.name   = "sdcc_dml_addr",
		.start  = MSM_SDC1_DML_BASE,
		.end    = MSM_SDC1_BAM_BASE - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "sdcc_bam_addr",
		.start  = MSM_SDC1_BAM_BASE,
		.end    = MSM_SDC1_BAM_BASE + (2 * SZ_4K) - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "sdcc_bam_irq",
		.start  = SDC1_BAM_IRQ,
		.end    = SDC1_BAM_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
#endif
};

static struct resource resources_sdc2[] = {
	{
		.name   = "core_mem",
		.flags  = IORESOURCE_MEM,
		.start  = MSM_SDC2_BASE,
		.end    = MSM_SDC2_DML_BASE - 1,
	},
	{
		.name   = "core_irq",
		.flags  = IORESOURCE_IRQ,
		.start  = SDC2_IRQ_0,
		.end    = SDC2_IRQ_0
	},
#ifdef CONFIG_MMC_MSM_SPS_SUPPORT
	{
		.name   = "sdcc_dml_addr",
		.start  = MSM_SDC2_DML_BASE,
		.end    = MSM_SDC2_BAM_BASE - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "sdcc_bam_addr",
		.start  = MSM_SDC2_BAM_BASE,
		.end    = MSM_SDC2_BAM_BASE + (2 * SZ_4K) - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "sdcc_bam_irq",
		.start  = SDC2_BAM_IRQ,
		.end    = SDC2_BAM_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
#endif
};

struct platform_device msm_device_sdc1 = {
	.name           = "msm_sdcc",
	.id             = 1,
	.num_resources  = ARRAY_SIZE(resources_sdc1),
	.resource       = resources_sdc1,
	.dev            = {
		.coherent_dma_mask      = 0xffffffff,
	},
};

struct platform_device msm_device_sdc2 = {
	.name           = "msm_sdcc",
	.id             = 2,
	.num_resources  = ARRAY_SIZE(resources_sdc2),
	.resource       = resources_sdc2,
	.dev            = {
		.coherent_dma_mask      = 0xffffffff,
	},
};

static struct platform_device *msm_sdcc_devices[] __initdata = {
	&msm_device_sdc1,
	&msm_device_sdc2,
};

int __init msm_add_sdcc(unsigned int controller, struct mmc_platform_data *plat)
{
	struct platform_device  *pdev;

	if (controller < 1 || controller > 2)
		return -EINVAL;

	pdev = msm_sdcc_devices[controller - 1];
	pdev->dev.platform_data = plat;
	return platform_device_register(pdev);
}

#ifdef CONFIG_CACHE_L2X0
static int __init l2x0_cache_init(void)
{
	int aux_ctrl = 0;

	/* Way Size 010(0x2) 32KB */
	aux_ctrl = (0x1 << L2X0_AUX_CTRL_SHARE_OVERRIDE_SHIFT) | \
		   (0x2 << L2X0_AUX_CTRL_WAY_SIZE_SHIFT) | \
		   (0x1 << L2X0_AUX_CTRL_EVNT_MON_BUS_EN_SHIFT);

	/* L2 Latency setting required by hardware. Default is 0x20
	   which is no good.
	 */
	writel_relaxed(0x220, MSM_L2CC_BASE + L2X0_DATA_LATENCY_CTRL);
	l2x0_init(MSM_L2CC_BASE, aux_ctrl, L2X0_AUX_CTRL_MASK);

	return 0;
}
#else
static int __init l2x0_cache_init(void){ return 0; }
#endif

void __init msm9615_device_init(void)
{
	if (socinfo_init() < 0)
		pr_err("socinfo_init() failed!\n");

	msm_clock_init(&msm9615_clock_init_data);
	acpuclk_init(&acpuclk_9615_soc_data);
}

#define MSM_SHARED_RAM_PHYS 0x40000000
void __init msm9615_map_io(void)
{
	msm_shared_ram_phys = MSM_SHARED_RAM_PHYS;
	msm_map_msm9615_io();
	l2x0_cache_init();
}

void __init msm9615_init_irq(void)
{
	unsigned int i;
	gic_init(0, GIC_PPI_START, MSM_QGIC_DIST_BASE,
						(void *)MSM_QGIC_CPU_BASE);

	/* Edge trigger PPIs except AVS_SVICINT and AVS_SVICINTSWDONE */
	writel_relaxed(0xFFFFD7FF, MSM_QGIC_DIST_BASE + GIC_DIST_CONFIG + 4);

	writel_relaxed(0x0000FFFF, MSM_QGIC_DIST_BASE + GIC_DIST_ENABLE_SET);
	mb();

	/*
	 * FIXME: Not installing AVS_SVICINT and AVS_SVICINTSWDONE yet
	 * as they are configured as level, which does not play nice with
	 * handle_percpu_irq.
	 */
	for (i = GIC_PPI_START; i < GIC_SPI_START; i++) {
		if (i != AVS_SVICINT && i != AVS_SVICINTSWDONE)
			irq_set_handler(i, handle_percpu_irq);
	}
}
