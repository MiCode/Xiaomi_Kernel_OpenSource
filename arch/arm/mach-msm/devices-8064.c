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
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/msm_rotator.h>
#include <linux/clkdev.h>
#include <mach/irqs-8064.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>
#include "clock.h"
#include "devices.h"

/* Address of GSBI blocks */
#define MSM_GSBI3_PHYS		0x16200000
#define MSM_GSBI4_PHYS		0x16300000
#define MSM_GSBI5_PHYS		0x1A200000
#define MSM_GSBI6_PHYS		0x16500000
#define MSM_GSBI7_PHYS		0x16600000

#define MSM_UART3DM_PHYS	(MSM_GSBI3_PHYS + 0x40000)

/* GSBI QUP devices */
#define MSM_GSBI3_QUP_PHYS	(MSM_GSBI3_PHYS + 0x80000)
#define MSM_GSBI4_QUP_PHYS	(MSM_GSBI4_PHYS + 0x80000)
#define MSM_GSBI5_QUP_PHYS	(MSM_GSBI5_PHYS + 0x80000)
#define MSM_GSBI6_QUP_PHYS	(MSM_GSBI6_PHYS + 0x80000)
#define MSM_GSBI7_QUP_PHYS	(MSM_GSBI7_PHYS + 0x80000)
#define MSM_QUP_SIZE		SZ_4K

/* Address of SSBI CMD */
#define MSM_PMIC1_SSBI_CMD_PHYS	0x00500000
#define MSM_PMIC2_SSBI_CMD_PHYS	0x00C00000
#define MSM_PMIC_SSBI_SIZE	SZ_4K

static struct resource msm_dmov_resource[] = {
	{
		.start = ADM_0_SCSS_0_IRQ,
		.end = (resource_size_t)MSM_DMOV_BASE,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device msm_device_dmov = {
	.name	= "msm_dmov",
	.id	= -1,
	.resource = msm_dmov_resource,
	.num_resources = ARRAY_SIZE(msm_dmov_resource),
};

static struct resource resources_uart_gsbi3[] = {
	{
		.start	= GSBI3_UARTDM_IRQ,
		.end	= GSBI3_UARTDM_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= MSM_UART3DM_PHYS,
		.end	= MSM_UART3DM_PHYS + PAGE_SIZE - 1,
		.name	= "uartdm_resource",
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= MSM_GSBI3_PHYS,
		.end	= MSM_GSBI3_PHYS + PAGE_SIZE - 1,
		.name	= "gsbi_resource",
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device apq8064_device_uart_gsbi3 = {
	.name	= "msm_serial_hsl",
	.id	= 0,
	.num_resources	= ARRAY_SIZE(resources_uart_gsbi3),
	.resource	= resources_uart_gsbi3,
};

static struct resource resources_qup_spi_gsbi5[] = {
	{
		.name   = "spi_base",
		.start  = MSM_GSBI5_QUP_PHYS,
		.end    = MSM_GSBI5_QUP_PHYS + SZ_4K - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "gsbi_base",
		.start  = MSM_GSBI5_PHYS,
		.end    = MSM_GSBI5_PHYS + 4 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "spi_irq_in",
		.start  = GSBI5_QUP_IRQ,
		.end    = GSBI5_QUP_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device apq8064_device_qup_spi_gsbi5 = {
	.name		= "spi_qsd",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(resources_qup_spi_gsbi5),
	.resource	= resources_qup_spi_gsbi5,
};

static struct resource resources_ssbi_pmic1[] = {
	{
		.start  = MSM_PMIC1_SSBI_CMD_PHYS,
		.end    = MSM_PMIC1_SSBI_CMD_PHYS + MSM_PMIC_SSBI_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
};

struct platform_device apq8064_device_ssbi_pmic1 = {
	.name           = "msm_ssbi",
	.id             = 0,
	.resource       = resources_ssbi_pmic1,
	.num_resources  = ARRAY_SIZE(resources_ssbi_pmic1),
};

static struct resource resources_ssbi_pmic2[] = {
	{
		.start  = MSM_PMIC2_SSBI_CMD_PHYS,
		.end    = MSM_PMIC2_SSBI_CMD_PHYS + MSM_PMIC_SSBI_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
};

struct platform_device apq8064_device_ssbi_pmic2 = {
	.name           = "msm_ssbi",
	.id             = 1,
	.resource       = resources_ssbi_pmic2,
	.num_resources  = ARRAY_SIZE(resources_ssbi_pmic2),
};

static struct resource resources_otg[] = {
	{
		.start	= MSM_HSUSB_PHYS,
		.end	= MSM_HSUSB_PHYS + MSM_HSUSB_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= USB1_HS_IRQ,
		.end	= USB1_HS_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm_device_otg = {
	.name		= "msm_otg",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_otg),
	.resource	= resources_otg,
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};

static struct resource resources_hsusb[] = {
	{
		.start	= MSM_HSUSB_PHYS,
		.end	= MSM_HSUSB_PHYS + MSM_HSUSB_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= USB1_HS_IRQ,
		.end	= USB1_HS_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm_device_gadget_peripheral = {
	.name		= "msm_hsusb",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_hsusb),
	.resource	= resources_hsusb,
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};

#define MSM_SDC1_BASE         0x12400000
#define MSM_SDC1_DML_BASE     (MSM_SDC1_BASE + 0x800)
#define MSM_SDC1_BAM_BASE     (MSM_SDC1_BASE + 0x2000)
#define MSM_SDC2_BASE         0x12140000
#define MSM_SDC2_DML_BASE     (MSM_SDC2_BASE + 0x800)
#define MSM_SDC2_BAM_BASE     (MSM_SDC2_BASE + 0x2000)
#define MSM_SDC3_BASE         0x12180000
#define MSM_SDC3_DML_BASE     (MSM_SDC3_BASE + 0x800)
#define MSM_SDC3_BAM_BASE     (MSM_SDC3_BASE + 0x2000)
#define MSM_SDC4_BASE         0x121C0000
#define MSM_SDC4_DML_BASE     (MSM_SDC4_BASE + 0x800)
#define MSM_SDC4_BAM_BASE     (MSM_SDC4_BASE + 0x2000)

static struct resource resources_sdc1[] = {
	{
		.name	= "core_mem",
		.flags	= IORESOURCE_MEM,
		.start	= MSM_SDC1_BASE,
		.end	= MSM_SDC1_DML_BASE - 1,
	},
	{
		.name	= "core_irq",
		.flags	= IORESOURCE_IRQ,
		.start	= SDC1_IRQ_0,
		.end	= SDC1_IRQ_0
	},
#ifdef CONFIG_MMC_MSM_SPS_SUPPORT
	{
		.name   = "sdcc_dml_addr",
		.start	= MSM_SDC1_DML_BASE,
		.end	= MSM_SDC1_BAM_BASE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "sdcc_bam_addr",
		.start	= MSM_SDC1_BAM_BASE,
		.end	= MSM_SDC1_BAM_BASE + (2 * SZ_4K) - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "sdcc_bam_irq",
		.start	= SDC1_BAM_IRQ,
		.end	= SDC1_BAM_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
#endif
};

static struct resource resources_sdc2[] = {
	{
		.name	= "core_mem",
		.flags	= IORESOURCE_MEM,
		.start	= MSM_SDC2_BASE,
		.end	= MSM_SDC2_DML_BASE - 1,
	},
	{
		.name	= "core_irq",
		.flags	= IORESOURCE_IRQ,
		.start	= SDC2_IRQ_0,
		.end	= SDC2_IRQ_0
	},
#ifdef CONFIG_MMC_MSM_SPS_SUPPORT
	{
		.name   = "sdcc_dml_addr",
		.start	= MSM_SDC2_DML_BASE,
		.end	= MSM_SDC2_BAM_BASE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "sdcc_bam_addr",
		.start	= MSM_SDC2_BAM_BASE,
		.end	= MSM_SDC2_BAM_BASE + (2 * SZ_4K) - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "sdcc_bam_irq",
		.start	= SDC2_BAM_IRQ,
		.end	= SDC2_BAM_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
#endif
};

static struct resource resources_sdc3[] = {
	{
		.name	= "core_mem",
		.flags	= IORESOURCE_MEM,
		.start	= MSM_SDC3_BASE,
		.end	= MSM_SDC3_DML_BASE - 1,
	},
	{
		.name	= "core_irq",
		.flags	= IORESOURCE_IRQ,
		.start	= SDC3_IRQ_0,
		.end	= SDC3_IRQ_0
	},
#ifdef CONFIG_MMC_MSM_SPS_SUPPORT
	{
		.name   = "sdcc_dml_addr",
		.start	= MSM_SDC3_DML_BASE,
		.end	= MSM_SDC3_BAM_BASE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "sdcc_bam_addr",
		.start	= MSM_SDC3_BAM_BASE,
		.end	= MSM_SDC3_BAM_BASE + (2 * SZ_4K) - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "sdcc_bam_irq",
		.start	= SDC3_BAM_IRQ,
		.end	= SDC3_BAM_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
#endif
};

static struct resource resources_sdc4[] = {
	{
		.name	= "core_mem",
		.flags	= IORESOURCE_MEM,
		.start	= MSM_SDC4_BASE,
		.end	= MSM_SDC4_DML_BASE - 1,
	},
	{
		.name	= "core_irq",
		.flags	= IORESOURCE_IRQ,
		.start	= SDC4_IRQ_0,
		.end	= SDC4_IRQ_0
	},
#ifdef CONFIG_MMC_MSM_SPS_SUPPORT
	{
		.name   = "sdcc_dml_addr",
		.start	= MSM_SDC4_DML_BASE,
		.end	= MSM_SDC4_BAM_BASE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "sdcc_bam_addr",
		.start	= MSM_SDC4_BAM_BASE,
		.end	= MSM_SDC4_BAM_BASE + (2 * SZ_4K) - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "sdcc_bam_irq",
		.start	= SDC4_BAM_IRQ,
		.end	= SDC4_BAM_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
#endif
};

struct platform_device apq8064_device_sdc1 = {
	.name		= "msm_sdcc",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(resources_sdc1),
	.resource	= resources_sdc1,
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};

struct platform_device apq8064_device_sdc2 = {
	.name		= "msm_sdcc",
	.id		= 2,
	.num_resources	= ARRAY_SIZE(resources_sdc2),
	.resource	= resources_sdc2,
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};

struct platform_device apq8064_device_sdc3 = {
	.name		= "msm_sdcc",
	.id		= 3,
	.num_resources	= ARRAY_SIZE(resources_sdc3),
	.resource	= resources_sdc3,
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};

struct platform_device apq8064_device_sdc4 = {
	.name		= "msm_sdcc",
	.id		= 4,
	.num_resources	= ARRAY_SIZE(resources_sdc4),
	.resource	= resources_sdc4,
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};

static struct platform_device *apq8064_sdcc_devices[] __initdata = {
	&apq8064_device_sdc1,
	&apq8064_device_sdc2,
	&apq8064_device_sdc3,
	&apq8064_device_sdc4,
};

int __init apq8064_add_sdcc(unsigned int controller,
				struct mmc_platform_data *plat)
{
	struct platform_device	*pdev;

	if (!plat)
		return 0;
	if (controller < 1 || controller > 4)
		return -EINVAL;

	pdev = apq8064_sdcc_devices[controller-1];
	pdev->dev.platform_data = plat;
	return platform_device_register(pdev);
}

static struct clk_lookup msm_clocks_8064_dummy[] = {
	CLK_DUMMY("pll2",		PLL2,		NULL, 0),
	CLK_DUMMY("pll8",		PLL8,		NULL, 0),
	CLK_DUMMY("pll4",		PLL4,		NULL, 0),

	CLK_DUMMY("afab_clk",		AFAB_CLK,	NULL, 0),
	CLK_DUMMY("afab_a_clk",		AFAB_A_CLK,	NULL, 0),
	CLK_DUMMY("cfpb_clk",		CFPB_CLK,	NULL, 0),
	CLK_DUMMY("cfpb_a_clk",		CFPB_A_CLK,	NULL, 0),
	CLK_DUMMY("dfab_clk",		DFAB_CLK,	NULL, 0),
	CLK_DUMMY("dfab_a_clk",		DFAB_A_CLK,	NULL, 0),
	CLK_DUMMY("ebi1_clk",		EBI1_CLK,	NULL, 0),
	CLK_DUMMY("ebi1_a_clk",		EBI1_A_CLK,	NULL, 0),
	CLK_DUMMY("mmfab_clk",		MMFAB_CLK,	NULL, 0),
	CLK_DUMMY("mmfab_a_clk",	MMFAB_A_CLK,	NULL, 0),
	CLK_DUMMY("mmfpb_clk",		MMFPB_CLK,	NULL, 0),
	CLK_DUMMY("mmfpb_a_clk",	MMFPB_A_CLK,	NULL, 0),
	CLK_DUMMY("sfab_clk",		SFAB_CLK,	NULL, 0),
	CLK_DUMMY("sfab_a_clk",		SFAB_A_CLK,	NULL, 0),
	CLK_DUMMY("sfpb_clk",		SFPB_CLK,	NULL, 0),
	CLK_DUMMY("sfpb_a_clk",		SFPB_A_CLK,	NULL, 0),

	CLK_DUMMY("gsbi_uart_clk",	GSBI1_UART_CLK,		NULL, OFF),
	CLK_DUMMY("gsbi_uart_clk",	GSBI2_UART_CLK,		NULL, OFF),
	CLK_DUMMY("gsbi_uart_clk",	GSBI3_UART_CLK,
						  "msm_serial_hsl.0", OFF),
	CLK_DUMMY("gsbi_uart_clk",	GSBI4_UART_CLK,		NULL, OFF),
	CLK_DUMMY("gsbi_uart_clk",	GSBI5_UART_CLK,		NULL, OFF),
	CLK_DUMMY("uartdm_clk",		GSBI6_UART_CLK,		NULL, OFF),
	CLK_DUMMY("gsbi_uart_clk",	GSBI7_UART_CLK,		NULL, OFF),
	CLK_DUMMY("gsbi_uart_clk",	GSBI8_UART_CLK,		NULL, OFF),
	CLK_DUMMY("gsbi_uart_clk",	GSBI9_UART_CLK,		NULL, OFF),
	CLK_DUMMY("gsbi_uart_clk",	GSBI10_UART_CLK,	NULL, OFF),
	CLK_DUMMY("gsbi_uart_clk",	GSBI11_UART_CLK,	NULL, OFF),
	CLK_DUMMY("gsbi_uart_clk",	GSBI12_UART_CLK,	NULL, OFF),
	CLK_DUMMY("spi_clk",		GSBI1_QUP_CLK,		NULL, OFF),
	CLK_DUMMY("gsbi_qup_clk",	GSBI2_QUP_CLK,		NULL, OFF),
	CLK_DUMMY("gsbi_qup_clk",	GSBI3_QUP_CLK,		NULL, OFF),
	CLK_DUMMY("gsbi_qup_clk",	GSBI4_QUP_CLK,		NULL, OFF),
	CLK_DUMMY("spi_clk",	GSBI5_QUP_CLK,		 "spi_qsd.0", OFF),
	CLK_DUMMY("gsbi_qup_clk",	GSBI6_QUP_CLK,		NULL, OFF),
	CLK_DUMMY("gsbi_qup_clk",	GSBI7_QUP_CLK,		NULL, OFF),
	CLK_DUMMY("gsbi_qup_clk",	GSBI8_QUP_CLK,		NULL, OFF),
	CLK_DUMMY("gsbi_qup_clk",	GSBI9_QUP_CLK,		NULL, OFF),
	CLK_DUMMY("gsbi_qup_clk",	GSBI10_QUP_CLK,		NULL, OFF),
	CLK_DUMMY("gsbi_qup_clk",	GSBI11_QUP_CLK,		NULL, OFF),
	CLK_DUMMY("gsbi_qup_clk",	GSBI12_QUP_CLK,		NULL, OFF),
	CLK_DUMMY("pdm_clk",		PDM_CLK,		NULL, OFF),
	CLK_DUMMY("pmem_clk",		PMEM_CLK,		NULL, OFF),
	CLK_DUMMY("prng_clk",		PRNG_CLK,		NULL, OFF),
	CLK_DUMMY("sdc_clk",		SDC1_CLK,		NULL, OFF),
	CLK_DUMMY("sdc_clk",		SDC2_CLK,		NULL, OFF),
	CLK_DUMMY("sdc_clk",		SDC3_CLK,		NULL, OFF),
	CLK_DUMMY("sdc_clk",		SDC4_CLK,		NULL, OFF),
	CLK_DUMMY("sdc_clk",		SDC5_CLK,		NULL, OFF),
	CLK_DUMMY("tsif_ref_clk",	TSIF_REF_CLK,		NULL, OFF),
	CLK_DUMMY("tssc_clk",		TSSC_CLK,		NULL, OFF),
	CLK_DUMMY("usb_hs_clk",		USB_HS1_XCVR_CLK,	NULL, OFF),
	CLK_DUMMY("usb_phy_clk",	USB_PHY0_CLK,		NULL, OFF),
	CLK_DUMMY("usb_fs_src_clk",	USB_FS1_SRC_CLK,	NULL, OFF),
	CLK_DUMMY("usb_fs_clk",		USB_FS1_XCVR_CLK,	NULL, OFF),
	CLK_DUMMY("usb_fs_sys_clk",	USB_FS1_SYS_CLK,	NULL, OFF),
	CLK_DUMMY("usb_fs_src_clk",	USB_FS2_SRC_CLK,	NULL, OFF),
	CLK_DUMMY("usb_fs_clk",		USB_FS2_XCVR_CLK,	NULL, OFF),
	CLK_DUMMY("usb_fs_sys_clk",	USB_FS2_SYS_CLK,	NULL, OFF),
	CLK_DUMMY("ce_pclk",		CE2_CLK,		NULL, OFF),
	CLK_DUMMY("ce_clk",		CE1_CORE_CLK,		NULL, OFF),
	CLK_DUMMY("spi_pclk",		GSBI1_P_CLK,		NULL, OFF),
	CLK_DUMMY("gsbi_pclk",		GSBI2_P_CLK,		NULL, OFF),
	CLK_DUMMY("gsbi_pclk",		GSBI3_P_CLK,
						  "msm_serial_hsl.0", OFF),
	CLK_DUMMY("gsbi_pclk",		GSBI4_P_CLK,		NULL, OFF),
	CLK_DUMMY("spi_pclk",		GSBI5_P_CLK,	 "spi_qsd.0", OFF),
	CLK_DUMMY("uartdm_pclk",	GSBI6_P_CLK,		NULL, OFF),
	CLK_DUMMY("gsbi_pclk",		GSBI7_P_CLK,		NULL, OFF),
	CLK_DUMMY("gsbi_pclk",		GSBI8_P_CLK,		NULL, OFF),
	CLK_DUMMY("gsbi_pclk",		GSBI9_P_CLK,		NULL, OFF),
	CLK_DUMMY("gsbi_pclk",		GSBI10_P_CLK,		NULL, OFF),
	CLK_DUMMY("gsbi_pclk",		GSBI11_P_CLK,		NULL, OFF),
	CLK_DUMMY("gsbi_pclk",		GSBI12_P_CLK,		NULL, OFF),
	CLK_DUMMY("gsbi_pclk",		GSBI12_P_CLK,		NULL, OFF),
	CLK_DUMMY("tsif_pclk",		TSIF_P_CLK,		NULL, OFF),
	CLK_DUMMY("usb_fs_pclk",	USB_FS1_P_CLK,		NULL, OFF),
	CLK_DUMMY("usb_fs_pclk",	USB_FS2_P_CLK,		NULL, OFF),
	CLK_DUMMY("usb_hs_pclk",	USB_HS1_P_CLK,		NULL, OFF),
	CLK_DUMMY("sdc_pclk",		SDC1_P_CLK,		NULL, OFF),
	CLK_DUMMY("sdc_pclk",		SDC2_P_CLK,		NULL, OFF),
	CLK_DUMMY("sdc_pclk",		SDC3_P_CLK,		NULL, OFF),
	CLK_DUMMY("sdc_pclk",		SDC4_P_CLK,		NULL, OFF),
	CLK_DUMMY("sdc_pclk",		SDC5_P_CLK,		NULL, OFF),
	CLK_DUMMY("adm_clk",		ADM0_CLK,		NULL, OFF),
	CLK_DUMMY("adm_pclk",		ADM0_P_CLK,		NULL, OFF),
	CLK_DUMMY("pmic_arb_pclk",	PMIC_ARB0_P_CLK,	NULL, OFF),
	CLK_DUMMY("pmic_arb_pclk",	PMIC_ARB1_P_CLK,	NULL, OFF),
	CLK_DUMMY("pmic_ssbi2",		PMIC_SSBI2_CLK,		NULL, OFF),
	CLK_DUMMY("rpm_msg_ram_pclk",	RPM_MSG_RAM_P_CLK,	NULL, OFF),
	CLK_DUMMY("amp_clk",		AMP_CLK,		NULL, OFF),
	CLK_DUMMY("cam_clk",		CAM0_CLK,		NULL, OFF),
	CLK_DUMMY("cam_clk",		CAM1_CLK,		NULL, OFF),
	CLK_DUMMY("csi_src_clk",	CSI0_SRC_CLK,		NULL, OFF),
	CLK_DUMMY("csi_src_clk",	CSI1_SRC_CLK,		NULL, OFF),
	CLK_DUMMY("csi_clk",		CSI0_CLK,		NULL, OFF),
	CLK_DUMMY("csi_clk",		CSI1_CLK,		NULL, OFF),
	CLK_DUMMY("csi_pix_clk",	CSI_PIX_CLK,		NULL, OFF),
	CLK_DUMMY("csi_rdi_clk",	CSI_RDI_CLK,		NULL, OFF),
	CLK_DUMMY("csiphy_timer_src_clk", CSIPHY_TIMER_SRC_CLK,	NULL, OFF),
	CLK_DUMMY("csi0phy_timer_clk",	CSIPHY0_TIMER_CLK,	NULL, OFF),
	CLK_DUMMY("csi1phy_timer_clk",	CSIPHY1_TIMER_CLK,	NULL, OFF),
	CLK_DUMMY("dsi_byte_div_clk",	DSI1_BYTE_CLK,		NULL, OFF),
	CLK_DUMMY("dsi_byte_div_clk",	DSI2_BYTE_CLK,		NULL, OFF),
	CLK_DUMMY("dsi_esc_clk",	DSI1_ESC_CLK,		NULL, OFF),
	CLK_DUMMY("dsi_esc_clk",	DSI2_ESC_CLK,		NULL, OFF),
	CLK_DUMMY("gfx2d0_clk",		GFX2D0_CLK,		NULL, OFF),
	CLK_DUMMY("gfx2d1_clk",		GFX2D1_CLK,		NULL, OFF),
	CLK_DUMMY("gfx3d_clk",		GFX3D_CLK,		NULL, OFF),
	CLK_DUMMY("ijpeg_clk",		IJPEG_CLK,		NULL, OFF),
	CLK_DUMMY("imem_clk",		IMEM_CLK,		NULL, OFF),
	CLK_DUMMY("jpegd_clk",		JPEGD_CLK,		NULL, OFF),
	CLK_DUMMY("mdp_clk",		MDP_CLK,		NULL, OFF),
	CLK_DUMMY("mdp_vsync_clk",	MDP_VSYNC_CLK,		NULL, OFF),
	CLK_DUMMY("lut_mdp",		LUT_MDP_CLK,		NULL, OFF),
	CLK_DUMMY("rot_clk",		ROT_CLK,		NULL, OFF),
	CLK_DUMMY("tv_src_clk",		TV_SRC_CLK,		NULL, OFF),
	CLK_DUMMY("tv_enc_clk",		TV_ENC_CLK,		NULL, OFF),
	CLK_DUMMY("tv_dac_clk",		TV_DAC_CLK,		NULL, OFF),
	CLK_DUMMY("vcodec_clk",		VCODEC_CLK,		NULL, OFF),
	CLK_DUMMY("mdp_tv_clk",		MDP_TV_CLK,		NULL, OFF),
	CLK_DUMMY("hdmi_clk",		HDMI_TV_CLK,		NULL, OFF),
	CLK_DUMMY("hdmi_app_clk",	HDMI_APP_CLK,		NULL, OFF),
	CLK_DUMMY("vpe_clk",		VPE_CLK,		NULL, OFF),
	CLK_DUMMY("vfe_clk",		VFE_CLK,		NULL, OFF),
	CLK_DUMMY("csi_vfe_clk",	CSI0_VFE_CLK,		NULL, OFF),
	CLK_DUMMY("vfe_axi_clk",	VFE_AXI_CLK,		NULL, OFF),
	CLK_DUMMY("ijpeg_axi_clk",	IJPEG_AXI_CLK,		NULL, OFF),
	CLK_DUMMY("mdp_axi_clk",	MDP_AXI_CLK,		NULL, OFF),
	CLK_DUMMY("rot_axi_clk",	ROT_AXI_CLK,		NULL, OFF),
	CLK_DUMMY("vcodec_axi_clk",	VCODEC_AXI_CLK,		NULL, OFF),
	CLK_DUMMY("vcodec_axi_a_clk",	VCODEC_AXI_A_CLK,	NULL, OFF),
	CLK_DUMMY("vcodec_axi_b_clk",	VCODEC_AXI_B_CLK,	NULL, OFF),
	CLK_DUMMY("vpe_axi_clk",	VPE_AXI_CLK,		NULL, OFF),
	CLK_DUMMY("amp_pclk",		AMP_P_CLK,		NULL, OFF),
	CLK_DUMMY("csi_pclk",		CSI0_P_CLK,		NULL, OFF),
	CLK_DUMMY("dsi_m_pclk",		DSI1_M_P_CLK,		NULL, OFF),
	CLK_DUMMY("dsi_s_pclk",		DSI1_S_P_CLK,		NULL, OFF),
	CLK_DUMMY("dsi_m_pclk",		DSI2_M_P_CLK,		NULL, OFF),
	CLK_DUMMY("dsi_s_pclk",		DSI2_S_P_CLK,		NULL, OFF),
	CLK_DUMMY("gfx2d0_pclk",	GFX2D0_P_CLK,		NULL, OFF),
	CLK_DUMMY("gfx2d1_pclk",	GFX2D1_P_CLK,		NULL, OFF),
	CLK_DUMMY("gfx3d_pclk",		GFX3D_P_CLK,		NULL, OFF),
	CLK_DUMMY("hdmi_m_pclk",	HDMI_M_P_CLK,		NULL, OFF),
	CLK_DUMMY("hdmi_s_pclk",	HDMI_S_P_CLK,		NULL, OFF),
	CLK_DUMMY("ijpeg_pclk",		IJPEG_P_CLK,		NULL, OFF),
	CLK_DUMMY("jpegd_pclk",		JPEGD_P_CLK,		NULL, OFF),
	CLK_DUMMY("imem_pclk",		IMEM_P_CLK,		NULL, OFF),
	CLK_DUMMY("mdp_pclk",		MDP_P_CLK,		NULL, OFF),
	CLK_DUMMY("smmu_pclk",		SMMU_P_CLK,		NULL, OFF),
	CLK_DUMMY("rotator_pclk",	ROT_P_CLK,		NULL, OFF),
	CLK_DUMMY("tv_enc_pclk",	TV_ENC_P_CLK,		NULL, OFF),
	CLK_DUMMY("vcodec_pclk",	VCODEC_P_CLK,		NULL, OFF),
	CLK_DUMMY("vfe_pclk",		VFE_P_CLK,		NULL, OFF),
	CLK_DUMMY("vpe_pclk",		VPE_P_CLK,		NULL, OFF),
	CLK_DUMMY("mi2s_osr_clk",	MI2S_OSR_CLK,		NULL, OFF),
	CLK_DUMMY("mi2s_bit_clk",	MI2S_BIT_CLK,		NULL, OFF),
	CLK_DUMMY("i2s_mic_osr_clk",	CODEC_I2S_MIC_OSR_CLK,	NULL, OFF),
	CLK_DUMMY("i2s_mic_bit_clk",	CODEC_I2S_MIC_BIT_CLK,	NULL, OFF),
	CLK_DUMMY("i2s_mic_osr_clk",	SPARE_I2S_MIC_OSR_CLK,	NULL, OFF),
	CLK_DUMMY("i2s_mic_bit_clk",	SPARE_I2S_MIC_BIT_CLK,	NULL, OFF),
	CLK_DUMMY("i2s_spkr_osr_clk",	CODEC_I2S_SPKR_OSR_CLK,	NULL, OFF),
	CLK_DUMMY("i2s_spkr_bit_clk",	CODEC_I2S_SPKR_BIT_CLK,	NULL, OFF),
	CLK_DUMMY("i2s_spkr_osr_clk",	SPARE_I2S_SPKR_OSR_CLK,	NULL, OFF),
	CLK_DUMMY("i2s_spkr_bit_clk",	SPARE_I2S_SPKR_BIT_CLK,	NULL, OFF),
	CLK_DUMMY("pcm_clk",		PCM_CLK,		NULL, OFF),
	CLK_DUMMY("iommu_clk",		JPEGD_AXI_CLK,		NULL, 0),
	CLK_DUMMY("iommu_clk",		VFE_AXI_CLK,		NULL, 0),
	CLK_DUMMY("iommu_clk",		VCODEC_AXI_CLK,		NULL, 0),
	CLK_DUMMY("iommu_clk",		GFX3D_CLK,		NULL, 0),
	CLK_DUMMY("iommu_clk",		GFX2D0_CLK,		NULL, 0),
	CLK_DUMMY("iommu_clk",		GFX2D1_CLK,		NULL, 0),

	CLK_DUMMY("dfab_dsps_clk",	DFAB_DSPS_CLK,		NULL, 0),
	CLK_DUMMY("dfab_usb_hs_clk",	DFAB_USB_HS_CLK,	NULL, 0),
	CLK_DUMMY("dfab_sdc_clk",	DFAB_SDC1_CLK,		NULL, 0),
	CLK_DUMMY("dfab_sdc_clk",	DFAB_SDC2_CLK,		NULL, 0),
	CLK_DUMMY("dfab_sdc_clk",	DFAB_SDC3_CLK,		NULL, 0),
	CLK_DUMMY("dfab_sdc_clk",	DFAB_SDC4_CLK,		NULL, 0),
	CLK_DUMMY("dfab_sdc_clk",	DFAB_SDC5_CLK,		NULL, 0),
	CLK_DUMMY("dfab_clk",		DFAB_CLK,		NULL, 0),
	CLK_DUMMY("dma_bam_pclk",	DMA_BAM_P_CLK,		NULL, 0),
};

unsigned msm_num_clocks_8064_dummy = ARRAY_SIZE(msm_clocks_8064_dummy);
