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
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/msm_rotator.h>
#include <linux/ion.h>
#include <linux/gpio.h>
#include <linux/coresight.h>
#include <asm/clkdev.h>
#include <linux/msm_kgsl.h>
#include <linux/android_pmem.h>
#include <mach/irqs-8960.h>
#include <mach/dma.h>
#include <linux/dma-mapping.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <mach/msm_hsusb.h>
#include <mach/msm_sps.h>
#include <mach/rpm.h>
#include <mach/msm_bus_board.h>
#include <mach/msm_memtypes.h>
#include <mach/msm_smd.h>
#include <mach/msm_dcvs.h>
#include <mach/msm_rtb.h>
#include <mach/msm_cache_dump.h>
#include <sound/msm-dai-q6.h>
#include <sound/apr_audio.h>
#include <mach/msm_tsif.h>
#include <mach/msm_serial_hs_lite.h>
#include "clock.h"
#include "devices.h"
#include "devices-msm8x60.h"
#include "footswitch.h"
#include "msm_watchdog.h"
#include "rpm_log.h"
#include "rpm_stats.h"
#include "pil-q6v4.h"
#include "scm-pas.h"
#include <mach/msm_dcvs.h>
#include <mach/iommu_domains.h>

#ifdef CONFIG_MSM_MPM
#include <mach/mpm.h>
#endif
#ifdef CONFIG_MSM_DSPS
#include <mach/msm_dsps.h>
#endif


/* Address of GSBI blocks */
#define MSM_GSBI1_PHYS		0x16000000
#define MSM_GSBI2_PHYS		0x16100000
#define MSM_GSBI3_PHYS		0x16200000
#define MSM_GSBI4_PHYS		0x16300000
#define MSM_GSBI5_PHYS		0x16400000
#define MSM_GSBI6_PHYS		0x16500000
#define MSM_GSBI7_PHYS		0x16600000
#define MSM_GSBI8_PHYS		0x1A000000
#define MSM_GSBI9_PHYS		0x1A100000
#define MSM_GSBI10_PHYS		0x1A200000
#define MSM_GSBI11_PHYS		0x12440000
#define MSM_GSBI12_PHYS		0x12480000

#define MSM_UART2DM_PHYS	(MSM_GSBI2_PHYS + 0x40000)
#define MSM_UART5DM_PHYS	(MSM_GSBI5_PHYS + 0x40000)
#define MSM_UART6DM_PHYS	(MSM_GSBI6_PHYS + 0x40000)
#define MSM_UART8DM_PHYS	(MSM_GSBI8_PHYS + 0x40000)
#define MSM_UART9DM_PHYS	(MSM_GSBI9_PHYS + 0x40000)

/* GSBI QUP devices */
#define MSM_GSBI1_QUP_PHYS	(MSM_GSBI1_PHYS + 0x80000)
#define MSM_GSBI2_QUP_PHYS	(MSM_GSBI2_PHYS + 0x80000)
#define MSM_GSBI3_QUP_PHYS	(MSM_GSBI3_PHYS + 0x80000)
#define MSM_GSBI4_QUP_PHYS	(MSM_GSBI4_PHYS + 0x80000)
#define MSM_GSBI5_QUP_PHYS	(MSM_GSBI5_PHYS + 0x80000)
#define MSM_GSBI6_QUP_PHYS	(MSM_GSBI6_PHYS + 0x80000)
#define MSM_GSBI7_QUP_PHYS	(MSM_GSBI7_PHYS + 0x80000)
#define MSM_GSBI8_QUP_PHYS	(MSM_GSBI8_PHYS + 0x80000)
#define MSM_GSBI9_QUP_PHYS	(MSM_GSBI9_PHYS + 0x80000)
#define MSM_GSBI10_QUP_PHYS	(MSM_GSBI10_PHYS + 0x80000)
#define MSM_GSBI11_QUP_PHYS	(MSM_GSBI11_PHYS + 0x20000)
#define MSM_GSBI12_QUP_PHYS	(MSM_GSBI12_PHYS + 0x20000)
#define MSM_QUP_SIZE		SZ_4K

#define MSM_PMIC1_SSBI_CMD_PHYS	0x00500000
#define MSM_PMIC2_SSBI_CMD_PHYS	0x00C00000
#define MSM_PMIC_SSBI_SIZE	SZ_4K

#define MSM8960_HSUSB_PHYS		0x12500000
#define MSM8960_HSUSB_SIZE		SZ_4K

static struct resource resources_otg[] = {
	{
		.start	= MSM8960_HSUSB_PHYS,
		.end	= MSM8960_HSUSB_PHYS + MSM8960_HSUSB_SIZE,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= USB1_HS_IRQ,
		.end	= USB1_HS_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm8960_device_otg = {
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
		.start	= MSM8960_HSUSB_PHYS,
		.end	= MSM8960_HSUSB_PHYS + MSM8960_HSUSB_SIZE,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= USB1_HS_IRQ,
		.end	= USB1_HS_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm8960_device_gadget_peripheral = {
	.name		= "msm_hsusb",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_hsusb),
	.resource	= resources_hsusb,
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};

static struct resource resources_hsusb_host[] = {
	{
		.start  = MSM8960_HSUSB_PHYS,
		.end    = MSM8960_HSUSB_PHYS + MSM8960_HSUSB_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.start  = USB1_HS_IRQ,
		.end    = USB1_HS_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
};

static u64 dma_mask = DMA_BIT_MASK(32);
struct platform_device msm_device_hsusb_host = {
	.name           = "msm_hsusb_host",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(resources_hsusb_host),
	.resource       = resources_hsusb_host,
	.dev            = {
		.dma_mask               = &dma_mask,
		.coherent_dma_mask      = 0xffffffff,
	},
};

static struct resource resources_hsic_host[] = {
	{
		.start	= 0x12520000,
		.end	= 0x12520000 + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= USB_HSIC_IRQ,
		.end	= USB_HSIC_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= MSM_GPIO_TO_INT(69),
		.end	= MSM_GPIO_TO_INT(69),
		.name	= "peripheral_status_irq",
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm_device_hsic_host = {
	.name		= "msm_hsic_host",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_hsic_host),
	.resource	= resources_hsic_host,
	.dev		= {
		.dma_mask		= &dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};

struct platform_device msm8960_device_acpuclk = {
	.name		= "acpuclk-8960",
	.id		= -1,
};

#define SHARED_IMEM_TZ_BASE 0x2a03f720
static struct resource tzlog_resources[] = {
	{
		.start = SHARED_IMEM_TZ_BASE,
		.end = SHARED_IMEM_TZ_BASE + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
};

struct platform_device msm_device_tz_log = {
	.name		= "tz_log",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(tzlog_resources),
	.resource	= tzlog_resources,
};

static struct resource resources_uart_gsbi2[] = {
	{
		.start	= MSM8960_GSBI2_UARTDM_IRQ,
		.end	= MSM8960_GSBI2_UARTDM_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= MSM_UART2DM_PHYS,
		.end	= MSM_UART2DM_PHYS + PAGE_SIZE - 1,
		.name	= "uartdm_resource",
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= MSM_GSBI2_PHYS,
		.end	= MSM_GSBI2_PHYS + PAGE_SIZE - 1,
		.name	= "gsbi_resource",
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device msm8960_device_uart_gsbi2 = {
	.name	= "msm_serial_hsl",
	.id	= 0,
	.num_resources	= ARRAY_SIZE(resources_uart_gsbi2),
	.resource	= resources_uart_gsbi2,
};
/* GSBI 6 used into UARTDM Mode */
static struct resource msm_uart_dm6_resources[] = {
	{
		.start	= MSM_UART6DM_PHYS,
		.end	= MSM_UART6DM_PHYS + PAGE_SIZE - 1,
		.name	= "uartdm_resource",
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= GSBI6_UARTDM_IRQ,
		.end	= GSBI6_UARTDM_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= MSM_GSBI6_PHYS,
		.end	= MSM_GSBI6_PHYS + 4 - 1,
		.name	= "gsbi_resource",
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= DMOV_HSUART_GSBI6_TX_CHAN,
		.end	= DMOV_HSUART_GSBI6_RX_CHAN,
		.name	= "uartdm_channels",
		.flags	= IORESOURCE_DMA,
	},
	{
		.start	= DMOV_HSUART_GSBI6_TX_CRCI,
		.end	= DMOV_HSUART_GSBI6_RX_CRCI,
		.name	= "uartdm_crci",
		.flags	= IORESOURCE_DMA,
	},
};
static u64 msm_uart_dm6_dma_mask = DMA_BIT_MASK(32);
struct platform_device msm_device_uart_dm6 = {
	.name	= "msm_serial_hs",
	.id	= 0,
	.num_resources	= ARRAY_SIZE(msm_uart_dm6_resources),
	.resource	= msm_uart_dm6_resources,
	.dev	= {
		.dma_mask		= &msm_uart_dm6_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};
/*
 * GSBI 9 used into UARTDM Mode
 * For 8960 Fusion 2.2 Primary IPC
 */
static struct resource msm_uart_dm9_resources[] = {
	{
		.start	= MSM_UART9DM_PHYS,
		.end	= MSM_UART9DM_PHYS + PAGE_SIZE - 1,
		.name	= "uartdm_resource",
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= GSBI9_UARTDM_IRQ,
		.end	= GSBI9_UARTDM_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= MSM_GSBI9_PHYS,
		.end	= MSM_GSBI9_PHYS + 4 - 1,
		.name	= "gsbi_resource",
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= DMOV_HSUART_GSBI9_TX_CHAN,
		.end	= DMOV_HSUART_GSBI9_RX_CHAN,
		.name	= "uartdm_channels",
		.flags	= IORESOURCE_DMA,
	},
	{
		.start	= DMOV_HSUART_GSBI9_TX_CRCI,
		.end	= DMOV_HSUART_GSBI9_RX_CRCI,
		.name	= "uartdm_crci",
		.flags	= IORESOURCE_DMA,
	},
};
static u64 msm_uart_dm9_dma_mask = DMA_BIT_MASK(32);
struct platform_device msm_device_uart_dm9 = {
	.name	= "msm_serial_hs",
	.id	= 1,
	.num_resources	= ARRAY_SIZE(msm_uart_dm9_resources),
	.resource	= msm_uart_dm9_resources,
	.dev	= {
		.dma_mask		= &msm_uart_dm9_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};

static struct resource resources_uart_gsbi5[] = {
	{
		.start	= GSBI5_UARTDM_IRQ,
		.end	= GSBI5_UARTDM_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= MSM_UART5DM_PHYS,
		.end	= MSM_UART5DM_PHYS + PAGE_SIZE - 1,
		.name	= "uartdm_resource",
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= MSM_GSBI5_PHYS,
		.end	= MSM_GSBI5_PHYS + PAGE_SIZE - 1,
		.name	= "gsbi_resource",
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device msm8960_device_uart_gsbi5 = {
	.name	= "msm_serial_hsl",
	.id	= 0,
	.num_resources	= ARRAY_SIZE(resources_uart_gsbi5),
	.resource	= resources_uart_gsbi5,
};

static struct msm_serial_hslite_platform_data uart_gsbi8_pdata = {
	.line		= 0,
};

static struct resource resources_uart_gsbi8[] = {
	{
		.start	= GSBI8_UARTDM_IRQ,
		.end	= GSBI8_UARTDM_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= MSM_UART8DM_PHYS,
		.end	= MSM_UART8DM_PHYS + PAGE_SIZE - 1,
		.name	= "uartdm_resource",
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= MSM_GSBI8_PHYS,
		.end	= MSM_GSBI8_PHYS + PAGE_SIZE - 1,
		.name	= "gsbi_resource",
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device msm8960_device_uart_gsbi8 = {
	.name	= "msm_serial_hsl",
	.id	= 1,
	.num_resources	   = ARRAY_SIZE(resources_uart_gsbi8),
	.resource	   = resources_uart_gsbi8,
	.dev.platform_data = &uart_gsbi8_pdata,
};

/* MSM Video core device */
#ifdef CONFIG_MSM_BUS_SCALING
static struct msm_bus_vectors vidc_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT1,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};
static struct msm_bus_vectors vidc_venc_vga_vectors[] = {
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 54525952,
		.ib  = 436207616,
	},
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT1,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 72351744,
		.ib  = 289406976,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 500000,
		.ib  = 1000000,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 500000,
		.ib  = 1000000,
	},
};
static struct msm_bus_vectors vidc_vdec_vga_vectors[] = {
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 40894464,
		.ib  = 327155712,
	},
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT1,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 48234496,
		.ib  = 192937984,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 500000,
		.ib  = 2000000,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 500000,
		.ib  = 2000000,
	},
};
static struct msm_bus_vectors vidc_venc_720p_vectors[] = {
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 163577856,
		.ib  = 1308622848,
	},
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT1,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 219152384,
		.ib  = 876609536,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 1750000,
		.ib  = 3500000,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 1750000,
		.ib  = 3500000,
	},
};
static struct msm_bus_vectors vidc_vdec_720p_vectors[] = {
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 121634816,
		.ib  = 973078528,
	},
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT1,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 155189248,
		.ib  = 620756992,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 1750000,
		.ib  = 7000000,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 1750000,
		.ib  = 7000000,
	},
};
static struct msm_bus_vectors vidc_venc_1080p_vectors[] = {
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 372244480,
		.ib  = 2560000000U,
	},
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT1,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 501219328,
		.ib  = 2560000000U,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 2500000,
		.ib  = 5000000,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 2500000,
		.ib  = 5000000,
	},
};
static struct msm_bus_vectors vidc_vdec_1080p_vectors[] = {
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 222298112,
		.ib  = 2560000000U,
	},
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT1,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 330301440,
		.ib  = 2560000000U,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 2500000,
		.ib  = 700000000,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 2500000,
		.ib  = 10000000,
	},
};
static struct msm_bus_vectors vidc_venc_1080p_turbo_vectors[] = {
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 222298112,
		.ib  = 3522000000U,
	},
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT1,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 330301440,
		.ib  = 3522000000U,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 2500000,
		.ib  = 700000000,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 2500000,
		.ib  = 10000000,
	},
};
static struct msm_bus_vectors vidc_vdec_1080p_turbo_vectors[] = {
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 222298112,
		.ib  = 3522000000U,
	},
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT1,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 330301440,
		.ib  = 3522000000U,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 2500000,
		.ib  = 700000000,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 2500000,
		.ib  = 10000000,
	},
};

static struct msm_bus_paths vidc_bus_client_config[] = {
	{
		ARRAY_SIZE(vidc_init_vectors),
		vidc_init_vectors,
	},
	{
		ARRAY_SIZE(vidc_venc_vga_vectors),
		vidc_venc_vga_vectors,
	},
	{
		ARRAY_SIZE(vidc_vdec_vga_vectors),
		vidc_vdec_vga_vectors,
	},
	{
		ARRAY_SIZE(vidc_venc_720p_vectors),
		vidc_venc_720p_vectors,
	},
	{
		ARRAY_SIZE(vidc_vdec_720p_vectors),
		vidc_vdec_720p_vectors,
	},
	{
		ARRAY_SIZE(vidc_venc_1080p_vectors),
		vidc_venc_1080p_vectors,
	},
	{
		ARRAY_SIZE(vidc_vdec_1080p_vectors),
		vidc_vdec_1080p_vectors,
	},
	{
		ARRAY_SIZE(vidc_venc_1080p_turbo_vectors),
		vidc_vdec_1080p_turbo_vectors,
	},
	{
		ARRAY_SIZE(vidc_vdec_1080p_turbo_vectors),
		vidc_vdec_1080p_turbo_vectors,
	},
};

static struct msm_bus_scale_pdata vidc_bus_client_data = {
	vidc_bus_client_config,
	ARRAY_SIZE(vidc_bus_client_config),
	.name = "vidc",
};
#endif

#ifdef CONFIG_HW_RANDOM_MSM
/* PRNG device */
#define MSM_PRNG_PHYS		0x1A500000
static struct resource rng_resources = {
	.flags = IORESOURCE_MEM,
	.start = MSM_PRNG_PHYS,
	.end   = MSM_PRNG_PHYS + SZ_512 - 1,
};

struct platform_device msm_device_rng = {
	.name          = "msm_rng",
	.id            = 0,
	.num_resources = 1,
	.resource      = &rng_resources,
};
#endif

#define MSM_VIDC_BASE_PHYS 0x04400000
#define MSM_VIDC_BASE_SIZE 0x00100000

static struct resource msm_device_vidc_resources[] = {
	{
		.start	= MSM_VIDC_BASE_PHYS,
		.end	= MSM_VIDC_BASE_PHYS + MSM_VIDC_BASE_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= VCODEC_IRQ,
		.end	= VCODEC_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct msm_vidc_platform_data vidc_platform_data = {
#ifdef CONFIG_MSM_BUS_SCALING
	.vidc_bus_client_pdata = &vidc_bus_client_data,
#endif
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	.memtype = ION_CP_MM_HEAP_ID,
	.enable_ion = 1,
	.cp_enabled = 1,
#else
	.memtype = MEMTYPE_EBI1,
	.enable_ion = 0,
#endif
	.disable_dmx = 0,
	.disable_fullhd = 0,
	.cont_mode_dpb_count = 18,
	.fw_addr = 0x9fe00000,
};

struct platform_device msm_device_vidc = {
	.name = "msm_vidc",
	.id = 0,
	.num_resources = ARRAY_SIZE(msm_device_vidc_resources),
	.resource = msm_device_vidc_resources,
	.dev = {
		.platform_data	= &vidc_platform_data,
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
#define MSM_SDC5_BASE         0x12200000
#define MSM_SDC5_DML_BASE     (MSM_SDC5_BASE + 0x800)
#define MSM_SDC5_BAM_BASE     (MSM_SDC5_BASE + 0x2000)

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

static struct resource resources_sdc5[] = {
	{
		.name	= "core_mem",
		.flags	= IORESOURCE_MEM,
		.start	= MSM_SDC5_BASE,
		.end	= MSM_SDC5_DML_BASE - 1,
	},
	{
		.name	= "core_irq",
		.flags	= IORESOURCE_IRQ,
		.start	= SDC5_IRQ_0,
		.end	= SDC5_IRQ_0
	},
#ifdef CONFIG_MMC_MSM_SPS_SUPPORT
	{
		.name   = "sdcc_dml_addr",
		.start	= MSM_SDC5_DML_BASE,
		.end	= MSM_SDC5_BAM_BASE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "sdcc_bam_addr",
		.start	= MSM_SDC5_BAM_BASE,
		.end	= MSM_SDC5_BAM_BASE + (2 * SZ_4K) - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "sdcc_bam_irq",
		.start	= SDC5_BAM_IRQ,
		.end	= SDC5_BAM_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
#endif
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

struct platform_device msm_device_sdc2 = {
	.name		= "msm_sdcc",
	.id		= 2,
	.num_resources	= ARRAY_SIZE(resources_sdc2),
	.resource	= resources_sdc2,
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};

struct platform_device msm_device_sdc3 = {
	.name		= "msm_sdcc",
	.id		= 3,
	.num_resources	= ARRAY_SIZE(resources_sdc3),
	.resource	= resources_sdc3,
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};

struct platform_device msm_device_sdc4 = {
	.name		= "msm_sdcc",
	.id		= 4,
	.num_resources	= ARRAY_SIZE(resources_sdc4),
	.resource	= resources_sdc4,
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};

struct platform_device msm_device_sdc5 = {
	.name		= "msm_sdcc",
	.id		= 5,
	.num_resources	= ARRAY_SIZE(resources_sdc5),
	.resource	= resources_sdc5,
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};

#define MSM_LPASS_QDSP6SS_PHYS	0x28800000
#define SFAB_LPASS_Q6_ACLK_CTL	(MSM_CLK_CTL_BASE + 0x23A0)

static struct resource msm_8960_q6_lpass_resources[] = {
	{
		.start  = MSM_LPASS_QDSP6SS_PHYS,
		.end    = MSM_LPASS_QDSP6SS_PHYS + SZ_256 - 1,
		.flags  = IORESOURCE_MEM,
	},
};

static struct pil_q6v4_pdata msm_8960_q6_lpass_data = {
	.strap_tcm_base  = 0x01460000,
	.strap_ahb_upper = 0x00290000,
	.strap_ahb_lower = 0x00000280,
	.aclk_reg = SFAB_LPASS_Q6_ACLK_CTL,
	.name = "q6",
	.pas_id = PAS_Q6,
	.bus_port = MSM_BUS_MASTER_LPASS_PROC,
};

struct platform_device msm_8960_q6_lpass = {
	.name = "pil_qdsp6v4",
	.id = 0,
	.num_resources  = ARRAY_SIZE(msm_8960_q6_lpass_resources),
	.resource       = msm_8960_q6_lpass_resources,
	.dev.platform_data = &msm_8960_q6_lpass_data,
};

#define MSM_MSS_ENABLE_PHYS	0x08B00000
#define MSM_FW_QDSP6SS_PHYS	0x08800000
#define MSS_Q6FW_JTAG_CLK_CTL	(MSM_CLK_CTL_BASE + 0x2C6C)
#define SFAB_MSS_Q6_FW_ACLK_CTL (MSM_CLK_CTL_BASE + 0x2044)

static struct resource msm_8960_q6_mss_fw_resources[] = {
	{
		.start  = MSM_FW_QDSP6SS_PHYS,
		.end    = MSM_FW_QDSP6SS_PHYS + SZ_256 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.start  = MSM_MSS_ENABLE_PHYS,
		.end    = MSM_MSS_ENABLE_PHYS + 4 - 1,
		.flags  = IORESOURCE_MEM,
	},
};

static struct pil_q6v4_pdata msm_8960_q6_mss_fw_data = {
	.strap_tcm_base  = 0x00400000,
	.strap_ahb_upper = 0x00090000,
	.strap_ahb_lower = 0x00000080,
	.aclk_reg = SFAB_MSS_Q6_FW_ACLK_CTL,
	.jtag_clk_reg = MSS_Q6FW_JTAG_CLK_CTL,
	.name = "modem_fw",
	.depends = "q6",
	.pas_id = PAS_MODEM_FW,
	.bus_port = MSM_BUS_MASTER_MSS_FW_PROC,
};

struct platform_device msm_8960_q6_mss_fw = {
	.name = "pil_qdsp6v4",
	.id = 1,
	.num_resources  = ARRAY_SIZE(msm_8960_q6_mss_fw_resources),
	.resource       = msm_8960_q6_mss_fw_resources,
	.dev.platform_data = &msm_8960_q6_mss_fw_data,
};

#define MSM_SW_QDSP6SS_PHYS	0x08900000
#define SFAB_MSS_Q6_SW_ACLK_CTL	(MSM_CLK_CTL_BASE + 0x2040)
#define MSS_Q6SW_JTAG_CLK_CTL	(MSM_CLK_CTL_BASE + 0x2C68)

static struct resource msm_8960_q6_mss_sw_resources[] = {
	{
		.start  = MSM_SW_QDSP6SS_PHYS,
		.end    = MSM_SW_QDSP6SS_PHYS + SZ_256 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.start  = MSM_MSS_ENABLE_PHYS,
		.end    = MSM_MSS_ENABLE_PHYS + 4 - 1,
		.flags  = IORESOURCE_MEM,
	},
};

static struct pil_q6v4_pdata msm_8960_q6_mss_sw_data = {
	.strap_tcm_base  = 0x00420000,
	.strap_ahb_upper = 0x00090000,
	.strap_ahb_lower = 0x00000080,
	.aclk_reg = SFAB_MSS_Q6_SW_ACLK_CTL,
	.jtag_clk_reg = MSS_Q6SW_JTAG_CLK_CTL,
	.name = "modem",
	.depends = "modem_fw",
	.pas_id = PAS_MODEM_SW,
	.bus_port = MSM_BUS_MASTER_MSS_SW_PROC,
};

struct platform_device msm_8960_q6_mss_sw = {
	.name = "pil_qdsp6v4",
	.id = 2,
	.num_resources  = ARRAY_SIZE(msm_8960_q6_mss_sw_resources),
	.resource       = msm_8960_q6_mss_sw_resources,
	.dev.platform_data = &msm_8960_q6_mss_sw_data,
};

static struct resource msm_8960_riva_resources[] = {
	{
		.start  = 0x03204000,
		.end    = 0x03204000 + SZ_256 - 1,
		.flags  = IORESOURCE_MEM,
	},
};

struct platform_device msm_8960_riva = {
	.name = "pil_riva",
	.id = -1,
	.num_resources  = ARRAY_SIZE(msm_8960_riva_resources),
	.resource       = msm_8960_riva_resources,
};

struct platform_device msm_pil_tzapps = {
	.name = "pil_tzapps",
	.id = -1,
};

struct platform_device msm_pil_dsps = {
	.name          = "pil_dsps",
	.id            = -1,
	.dev.platform_data = "dsps",
};

struct platform_device msm_pil_vidc = {
	.name = "pil_vidc",
	.id = -1,
};

static struct resource smd_resource[] = {
	{
		.name   = "a9_m2a_0",
		.start  = INT_A9_M2A_0,
		.flags  = IORESOURCE_IRQ,
	},
	{
		.name   = "a9_m2a_5",
		.start  = INT_A9_M2A_5,
		.flags  = IORESOURCE_IRQ,
	},
	{
		.name   = "adsp_a11",
		.start  = INT_ADSP_A11,
		.flags  = IORESOURCE_IRQ,
	},
	{
		.name   = "adsp_a11_smsm",
		.start  = INT_ADSP_A11_SMSM,
		.flags  = IORESOURCE_IRQ,
	},
	{
		.name   = "dsps_a11",
		.start  = INT_DSPS_A11,
		.flags  = IORESOURCE_IRQ,
	},
	{
		.name   = "dsps_a11_smsm",
		.start  = INT_DSPS_A11_SMSM,
		.flags  = IORESOURCE_IRQ,
	},
	{
		.name   = "wcnss_a11",
		.start  = INT_WCNSS_A11,
		.flags  = IORESOURCE_IRQ,
	},
	{
		.name   = "wcnss_a11_smsm",
		.start  = INT_WCNSS_A11_SMSM,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct smd_subsystem_config smd_config_list[] = {
	{
		.irq_config_id = SMD_MODEM,
		.subsys_name = "modem",
		.edge = SMD_APPS_MODEM,

		.smd_int.irq_name = "a9_m2a_0",
		.smd_int.flags = IRQF_TRIGGER_RISING,
		.smd_int.irq_id = -1,
		.smd_int.device_name = "smd_dev",
		.smd_int.dev_id = 0,
		.smd_int.out_bit_pos =  1 << 3,
		.smd_int.out_base = (void __iomem *)MSM_APCS_GCC_BASE,
		.smd_int.out_offset = 0x8,

		.smsm_int.irq_name = "a9_m2a_5",
		.smsm_int.flags = IRQF_TRIGGER_RISING,
		.smsm_int.irq_id = -1,
		.smsm_int.device_name = "smd_smsm",
		.smsm_int.dev_id = 0,
		.smsm_int.out_bit_pos =  1 << 4,
		.smsm_int.out_base = (void __iomem *)MSM_APCS_GCC_BASE,
		.smsm_int.out_offset = 0x8,
	},
	{
		.irq_config_id = SMD_Q6,
		.subsys_name = "q6",
		.edge = SMD_APPS_QDSP,

		.smd_int.irq_name = "adsp_a11",
		.smd_int.flags = IRQF_TRIGGER_RISING,
		.smd_int.irq_id = -1,
		.smd_int.device_name = "smd_dev",
		.smd_int.dev_id = 0,
		.smd_int.out_bit_pos =  1 << 15,
		.smd_int.out_base = (void __iomem *)MSM_APCS_GCC_BASE,
		.smd_int.out_offset = 0x8,

		.smsm_int.irq_name = "adsp_a11_smsm",
		.smsm_int.flags = IRQF_TRIGGER_RISING,
		.smsm_int.irq_id = -1,
		.smsm_int.device_name = "smd_smsm",
		.smsm_int.dev_id = 0,
		.smsm_int.out_bit_pos =  1 << 14,
		.smsm_int.out_base = (void __iomem *)MSM_APCS_GCC_BASE,
		.smsm_int.out_offset = 0x8,
	},
	{
		.irq_config_id = SMD_DSPS,
		.subsys_name = "dsps",
		.edge = SMD_APPS_DSPS,

		.smd_int.irq_name = "dsps_a11",
		.smd_int.flags = IRQF_TRIGGER_RISING,
		.smd_int.irq_id = -1,
		.smd_int.device_name = "smd_dev",
		.smd_int.dev_id = 0,
		.smd_int.out_bit_pos =  1,
		.smd_int.out_base = (void __iomem *)MSM_SIC_NON_SECURE_BASE,
		.smd_int.out_offset = 0x4080,

		.smsm_int.irq_name = "dsps_a11_smsm",
		.smsm_int.flags = IRQF_TRIGGER_RISING,
		.smsm_int.irq_id = -1,
		.smsm_int.device_name = "smd_smsm",
		.smsm_int.dev_id = 0,
		.smsm_int.out_bit_pos =  1,
		.smsm_int.out_base = (void __iomem *)MSM_SIC_NON_SECURE_BASE,
		.smsm_int.out_offset = 0x4094,
	},
	{
		.irq_config_id = SMD_WCNSS,
		.subsys_name = "wcnss",
		.edge = SMD_APPS_WCNSS,

		.smd_int.irq_name = "wcnss_a11",
		.smd_int.flags = IRQF_TRIGGER_RISING,
		.smd_int.irq_id = -1,
		.smd_int.device_name = "smd_dev",
		.smd_int.dev_id = 0,
		.smd_int.out_bit_pos =  1 << 25,
		.smd_int.out_base = (void __iomem *)MSM_APCS_GCC_BASE,
		.smd_int.out_offset = 0x8,

		.smsm_int.irq_name = "wcnss_a11_smsm",
		.smsm_int.flags = IRQF_TRIGGER_RISING,
		.smsm_int.irq_id = -1,
		.smsm_int.device_name = "smd_smsm",
		.smsm_int.dev_id = 0,
		.smsm_int.out_bit_pos =  1 << 23,
		.smsm_int.out_base = (void __iomem *)MSM_APCS_GCC_BASE,
		.smsm_int.out_offset = 0x8,
	},
};

static struct smd_subsystem_restart_config smd_ssr_config = {
	.disable_smsm_reset_handshake = 1,
};

static struct smd_platform smd_platform_data = {
	.num_ss_configs = ARRAY_SIZE(smd_config_list),
	.smd_ss_configs = smd_config_list,
	.smd_ssr_config = &smd_ssr_config,
};

struct platform_device msm_device_smd = {
	.name		= "msm_smd",
	.id		= -1,
	.resource = smd_resource,
	.num_resources = ARRAY_SIZE(smd_resource),
	.dev = {
		.platform_data = &smd_platform_data,
	},
};

struct platform_device msm_device_bam_dmux = {
	.name		= "BAM_RMNT",
	.id		= -1,
};

static struct msm_watchdog_pdata msm_watchdog_pdata = {
	.pet_time = 10000,
	.bark_time = 11000,
	.has_secure = true,
};

struct platform_device msm8960_device_watchdog = {
	.name = "msm_watchdog",
	.id = -1,
	.dev = {
		.platform_data = &msm_watchdog_pdata,
	},
};

static struct resource msm_dmov_resource[] = {
	{
		.start = ADM_0_SCSS_1_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = 0x18320000,
		.end = 0x18320000 + SZ_1M - 1,
		.flags = IORESOURCE_MEM,
	},
};

static struct msm_dmov_pdata msm_dmov_pdata = {
	.sd = 1,
	.sd_size = 0x800,
};

struct platform_device msm8960_device_dmov = {
	.name	= "msm_dmov",
	.id	= -1,
	.resource = msm_dmov_resource,
	.num_resources = ARRAY_SIZE(msm_dmov_resource),
	.dev = {
		.platform_data = &msm_dmov_pdata,
	},
};

static struct platform_device *msm_sdcc_devices[] __initdata = {
	&msm_device_sdc1,
	&msm_device_sdc2,
	&msm_device_sdc3,
	&msm_device_sdc4,
	&msm_device_sdc5,
};

int __init msm_add_sdcc(unsigned int controller, struct mmc_platform_data *plat)
{
	struct platform_device	*pdev;

	if (controller < 1 || controller > 5)
		return -EINVAL;

	pdev = msm_sdcc_devices[controller-1];
	pdev->dev.platform_data = plat;
	return platform_device_register(pdev);
}

static struct resource resources_qup_i2c_gsbi4[] = {
	{
		.name	= "gsbi_qup_i2c_addr",
		.start	= MSM_GSBI4_PHYS,
		.end	= MSM_GSBI4_PHYS + 4 - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "qup_phys_addr",
		.start	= MSM_GSBI4_QUP_PHYS,
		.end	= MSM_GSBI4_QUP_PHYS + MSM_QUP_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "qup_err_intr",
		.start	= GSBI4_QUP_IRQ,
		.end	= GSBI4_QUP_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm8960_device_qup_i2c_gsbi4 = {
	.name		= "qup_i2c",
	.id		= 4,
	.num_resources	= ARRAY_SIZE(resources_qup_i2c_gsbi4),
	.resource	= resources_qup_i2c_gsbi4,
};

static struct resource resources_qup_i2c_gsbi3[] = {
	{
		.name	= "gsbi_qup_i2c_addr",
		.start	= MSM_GSBI3_PHYS,
		.end	= MSM_GSBI3_PHYS + 4 - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "qup_phys_addr",
		.start	= MSM_GSBI3_QUP_PHYS,
		.end	= MSM_GSBI3_QUP_PHYS + MSM_QUP_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "qup_err_intr",
		.start	= GSBI3_QUP_IRQ,
		.end	= GSBI3_QUP_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm8960_device_qup_i2c_gsbi3 = {
	.name		= "qup_i2c",
	.id		= 3,
	.num_resources	= ARRAY_SIZE(resources_qup_i2c_gsbi3),
	.resource	= resources_qup_i2c_gsbi3,
};

static struct resource resources_qup_i2c_gsbi9[] = {
	{
		.name	= "gsbi_qup_i2c_addr",
		.start	= MSM_GSBI9_PHYS,
		.end	= MSM_GSBI9_PHYS + 4 - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "qup_phys_addr",
		.start	= MSM_GSBI9_QUP_PHYS,
		.end	= MSM_GSBI9_QUP_PHYS + MSM_QUP_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "qup_err_intr",
		.start	= GSBI9_QUP_IRQ,
		.end	= GSBI9_QUP_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm8960_device_qup_i2c_gsbi9 = {
	.name		= "qup_i2c",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(resources_qup_i2c_gsbi9),
	.resource	= resources_qup_i2c_gsbi9,
};

static struct resource resources_qup_i2c_gsbi10[] = {
	{
		.name	= "gsbi_qup_i2c_addr",
		.start	= MSM_GSBI10_PHYS,
		.end	= MSM_GSBI10_PHYS + 4 - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "qup_phys_addr",
		.start	= MSM_GSBI10_QUP_PHYS,
		.end	= MSM_GSBI10_QUP_PHYS + MSM_QUP_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "qup_err_intr",
		.start	= GSBI10_QUP_IRQ,
		.end	= GSBI10_QUP_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm8960_device_qup_i2c_gsbi10 = {
	.name		= "qup_i2c",
	.id		= 10,
	.num_resources	= ARRAY_SIZE(resources_qup_i2c_gsbi10),
	.resource	= resources_qup_i2c_gsbi10,
};

static struct resource resources_qup_i2c_gsbi12[] = {
	{
		.name	= "gsbi_qup_i2c_addr",
		.start	= MSM_GSBI12_PHYS,
		.end	= MSM_GSBI12_PHYS + 4 - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "qup_phys_addr",
		.start	= MSM_GSBI12_QUP_PHYS,
		.end	= MSM_GSBI12_QUP_PHYS + MSM_QUP_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "qup_err_intr",
		.start	= GSBI12_QUP_IRQ,
		.end	= GSBI12_QUP_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm8960_device_qup_i2c_gsbi12 = {
	.name		= "qup_i2c",
	.id		= 12,
	.num_resources	= ARRAY_SIZE(resources_qup_i2c_gsbi12),
	.resource	= resources_qup_i2c_gsbi12,
};

#ifdef CONFIG_MSM_CAMERA
static struct resource msm_cam_gsbi4_i2c_mux_resources[] = {
	{
		.name   = "i2c_mux_rw",
		.start  = 0x008003E0,
		.end    = 0x008003E0 + SZ_8 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "i2c_mux_ctl",
		.start  = 0x008020B8,
		.end    = 0x008020B8 + SZ_4 - 1,
		.flags  = IORESOURCE_MEM,
	},
};

struct platform_device msm8960_device_i2c_mux_gsbi4 = {
	.name           = "msm_cam_i2c_mux",
	.id             = 0,
	.resource       = msm_cam_gsbi4_i2c_mux_resources,
	.num_resources  = ARRAY_SIZE(msm_cam_gsbi4_i2c_mux_resources),
};

static struct resource msm_csiphy0_resources[] = {
	{
		.name	= "csiphy",
		.start	= 0x04800C00,
		.end	= 0x04800C00 + SZ_1K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "csiphy",
		.start	= CSIPHY_4LN_IRQ,
		.end	= CSIPHY_4LN_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource msm_csiphy1_resources[] = {
	{
		.name	= "csiphy",
		.start	= 0x04801000,
		.end	= 0x04801000 + SZ_1K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "csiphy",
		.start	= MSM8960_CSIPHY_2LN_IRQ,
		.end	= MSM8960_CSIPHY_2LN_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource msm_csiphy2_resources[] = {
	{
		.name	= "csiphy",
		.start	= 0x04801400,
		.end	= 0x04801400 + SZ_1K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "csiphy",
		.start	= MSM8960_CSIPHY_2_2LN_IRQ,
		.end	= MSM8960_CSIPHY_2_2LN_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm8960_device_csiphy0 = {
	.name           = "msm_csiphy",
	.id             = 0,
	.resource       = msm_csiphy0_resources,
	.num_resources  = ARRAY_SIZE(msm_csiphy0_resources),
};

struct platform_device msm8960_device_csiphy1 = {
	.name           = "msm_csiphy",
	.id             = 1,
	.resource       = msm_csiphy1_resources,
	.num_resources  = ARRAY_SIZE(msm_csiphy1_resources),
};

struct platform_device msm8960_device_csiphy2 = {
	.name           = "msm_csiphy",
	.id             = 2,
	.resource       = msm_csiphy2_resources,
	.num_resources  = ARRAY_SIZE(msm_csiphy2_resources),
};

static struct resource msm_csid0_resources[] = {
	{
		.name	= "csid",
		.start	= 0x04800000,
		.end	= 0x04800000 + SZ_1K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "csid",
		.start	= CSI_0_IRQ,
		.end	= CSI_0_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource msm_csid1_resources[] = {
	{
		.name	= "csid",
		.start	= 0x04800400,
		.end	= 0x04800400 + SZ_1K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "csid",
		.start	= CSI_1_IRQ,
		.end	= CSI_1_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource msm_csid2_resources[] = {
	{
		.name	= "csid",
		.start	= 0x04801800,
		.end	= 0x04801800 + SZ_1K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "csid",
		.start	= CSI_2_IRQ,
		.end	= CSI_2_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm8960_device_csid0 = {
	.name           = "msm_csid",
	.id             = 0,
	.resource       = msm_csid0_resources,
	.num_resources  = ARRAY_SIZE(msm_csid0_resources),
};

struct platform_device msm8960_device_csid1 = {
	.name           = "msm_csid",
	.id             = 1,
	.resource       = msm_csid1_resources,
	.num_resources  = ARRAY_SIZE(msm_csid1_resources),
};

struct platform_device msm8960_device_csid2 = {
	.name           = "msm_csid",
	.id             = 2,
	.resource       = msm_csid2_resources,
	.num_resources  = ARRAY_SIZE(msm_csid2_resources),
};

struct resource msm_ispif_resources[] = {
	{
		.name	= "ispif",
		.start	= 0x04800800,
		.end	= 0x04800800 + SZ_1K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "ispif",
		.start	= ISPIF_IRQ,
		.end	= ISPIF_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm8960_device_ispif = {
	.name           = "msm_ispif",
	.id             = 0,
	.resource       = msm_ispif_resources,
	.num_resources  = ARRAY_SIZE(msm_ispif_resources),
};

static struct resource msm_vfe_resources[] = {
	{
		.name	= "vfe32",
		.start	= 0x04500000,
		.end	= 0x04500000 + SZ_1M - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "vfe32",
		.start	= VFE_IRQ,
		.end	= VFE_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm8960_device_vfe = {
	.name           = "msm_vfe",
	.id             = 0,
	.resource       = msm_vfe_resources,
	.num_resources  = ARRAY_SIZE(msm_vfe_resources),
};

static struct resource msm_vpe_resources[] = {
	{
		.name	= "vpe",
		.start	= 0x05300000,
		.end	= 0x05300000 + SZ_1M - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "vpe",
		.start	= VPE_IRQ,
		.end	= VPE_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm8960_device_vpe = {
	.name           = "msm_vpe",
	.id             = 0,
	.resource       = msm_vpe_resources,
	.num_resources  = ARRAY_SIZE(msm_vpe_resources),
};
#endif

#define MSM_TSIF0_PHYS       (0x18200000)
#define MSM_TSIF1_PHYS       (0x18201000)
#define MSM_TSIF_SIZE        (0x200)

#define TSIF_0_CLK       GPIO_CFG(75, 1, GPIO_CFG_INPUT, \
	GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA)
#define TSIF_0_EN        GPIO_CFG(76, 1, GPIO_CFG_INPUT, \
	GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA)
#define TSIF_0_DATA      GPIO_CFG(77, 1, GPIO_CFG_INPUT, \
	GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA)
#define TSIF_0_SYNC      GPIO_CFG(82, 1, GPIO_CFG_INPUT, \
	GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA)
#define TSIF_1_CLK       GPIO_CFG(79, 1, GPIO_CFG_INPUT, \
	GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA)
#define TSIF_1_EN        GPIO_CFG(80, 1, GPIO_CFG_INPUT, \
	GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA)
#define TSIF_1_DATA      GPIO_CFG(81, 1, GPIO_CFG_INPUT, \
	GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA)
#define TSIF_1_SYNC      GPIO_CFG(78, 1, GPIO_CFG_INPUT, \
	GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA)

static const struct msm_gpio tsif0_gpios[] = {
	{ .gpio_cfg = TSIF_0_CLK,  .label =  "tsif_clk", },
	{ .gpio_cfg = TSIF_0_EN,   .label =  "tsif_en", },
	{ .gpio_cfg = TSIF_0_DATA, .label =  "tsif_data", },
	{ .gpio_cfg = TSIF_0_SYNC, .label =  "tsif_sync", },
};

static const struct msm_gpio tsif1_gpios[] = {
	{ .gpio_cfg = TSIF_1_CLK,  .label =  "tsif_clk", },
	{ .gpio_cfg = TSIF_1_EN,   .label =  "tsif_en", },
	{ .gpio_cfg = TSIF_1_DATA, .label =  "tsif_data", },
	{ .gpio_cfg = TSIF_1_SYNC, .label =  "tsif_sync", },
};

struct msm_tsif_platform_data tsif1_platform_data = {
	.num_gpios = ARRAY_SIZE(tsif1_gpios),
	.gpios = tsif1_gpios,
	.tsif_pclk = "tsif_pclk",
	.tsif_ref_clk = "tsif_ref_clk",
};

struct resource tsif1_resources[] = {
	[0] = {
		.flags = IORESOURCE_IRQ,
		.start = TSIF2_IRQ,
		.end   = TSIF2_IRQ,
	},
	[1] = {
		.flags = IORESOURCE_MEM,
		.start = MSM_TSIF1_PHYS,
		.end   = MSM_TSIF1_PHYS + MSM_TSIF_SIZE - 1,
	},
	[2] = {
		.flags = IORESOURCE_DMA,
		.start = DMOV_TSIF_CHAN,
		.end   = DMOV_TSIF_CRCI,
	},
};

struct msm_tsif_platform_data tsif0_platform_data = {
	.num_gpios = ARRAY_SIZE(tsif0_gpios),
	.gpios = tsif0_gpios,
	.tsif_pclk = "tsif_pclk",
	.tsif_ref_clk = "tsif_ref_clk",
};
struct resource tsif0_resources[] = {
	[0] = {
		.flags = IORESOURCE_IRQ,
		.start = TSIF1_IRQ,
		.end   = TSIF1_IRQ,
	},
	[1] = {
		.flags = IORESOURCE_MEM,
		.start = MSM_TSIF0_PHYS,
		.end   = MSM_TSIF0_PHYS + MSM_TSIF_SIZE - 1,
	},
	[2] = {
		.flags = IORESOURCE_DMA,
		.start = DMOV_TSIF_CHAN,
		.end   = DMOV_TSIF_CRCI,
	},
};

struct platform_device msm_device_tsif[2] = {
	{
		.name          = "msm_tsif",
		.id            = 0,
		.num_resources = ARRAY_SIZE(tsif0_resources),
		.resource      = tsif0_resources,
		.dev = {
			.platform_data = &tsif0_platform_data
		},
	},
	{
		.name          = "msm_tsif",
		.id            = 1,
		.num_resources = ARRAY_SIZE(tsif1_resources),
		.resource      = tsif1_resources,
		.dev = {
			.platform_data = &tsif1_platform_data
		},
	}
};

static struct resource resources_ssbi_pmic[] = {
	{
		.start  = MSM_PMIC1_SSBI_CMD_PHYS,
		.end    = MSM_PMIC1_SSBI_CMD_PHYS + MSM_PMIC_SSBI_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
};

struct platform_device msm8960_device_ssbi_pmic = {
	.name           = "msm_ssbi",
	.id             = 0,
	.resource       = resources_ssbi_pmic,
	.num_resources  = ARRAY_SIZE(resources_ssbi_pmic),
};

static struct resource resources_qup_spi_gsbi1[] = {
	{
		.name   = "spi_base",
		.start  = MSM_GSBI1_QUP_PHYS,
		.end    = MSM_GSBI1_QUP_PHYS + SZ_4K - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "gsbi_base",
		.start  = MSM_GSBI1_PHYS,
		.end    = MSM_GSBI1_PHYS + 4 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "spi_irq_in",
		.start  = MSM8960_GSBI1_QUP_IRQ,
		.end    = MSM8960_GSBI1_QUP_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
	{
		.name   = "spi_clk",
		.start  = 9,
		.end    = 9,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "spi_miso",
		.start  = 7,
		.end    = 7,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "spi_mosi",
		.start  = 6,
		.end    = 6,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "spi_cs",
		.start  = 8,
		.end    = 8,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "spi_cs1",
		.start  = 14,
		.end    = 14,
		.flags  = IORESOURCE_IO,
	},
};

struct platform_device msm8960_device_qup_spi_gsbi1 = {
	.name	= "spi_qsd",
	.id	= 0,
	.num_resources	= ARRAY_SIZE(resources_qup_spi_gsbi1),
	.resource	= resources_qup_spi_gsbi1,
};

struct platform_device msm_pcm = {
	.name	= "msm-pcm-dsp",
	.id	= -1,
};

struct platform_device msm_multi_ch_pcm = {
	.name	= "msm-multi-ch-pcm-dsp",
	.id	= -1,
};

struct platform_device msm_pcm_routing = {
	.name	= "msm-pcm-routing",
	.id	= -1,
};

struct platform_device msm_cpudai0 = {
	.name	= "msm-dai-q6",
	.id	= 0x4000,
};

struct platform_device msm_cpudai1 = {
	.name	= "msm-dai-q6",
	.id	= 0x4001,
};

struct platform_device msm8960_cpudai_slimbus_2_rx = {
	.name = "msm-dai-q6",
	.id = 0x4004,
};

struct platform_device msm8960_cpudai_slimbus_2_tx = {
	.name = "msm-dai-q6",
	.id = 0x4005,
};

struct platform_device msm_cpudai_hdmi_rx = {
	.name	= "msm-dai-q6-hdmi",
	.id	= 8,
};

struct platform_device msm_cpudai_bt_rx = {
	.name   = "msm-dai-q6",
	.id     = 0x3000,
};

struct platform_device msm_cpudai_bt_tx = {
	.name   = "msm-dai-q6",
	.id     = 0x3001,
};

struct platform_device msm_cpudai_fm_rx = {
	.name   = "msm-dai-q6",
	.id     = 0x3004,
};

struct platform_device msm_cpudai_fm_tx = {
	.name   = "msm-dai-q6",
	.id     = 0x3005,
};

struct platform_device msm_cpudai_incall_music_rx = {
	.name   = "msm-dai-q6",
	.id     = 0x8005,
};

struct platform_device msm_cpudai_incall_record_rx = {
	.name   = "msm-dai-q6",
	.id     = 0x8004,
};

struct platform_device msm_cpudai_incall_record_tx = {
	.name   = "msm-dai-q6",
	.id     = 0x8003,
};

/*
 * Machine specific data for AUX PCM Interface
 * which the driver will  be unware of.
 */
struct msm_dai_auxpcm_pdata auxpcm_pdata = {
	.clk = "pcm_clk",
	.mode_8k = {
		.mode = AFE_PCM_CFG_MODE_PCM,
		.sync = AFE_PCM_CFG_SYNC_INT,
		.frame = AFE_PCM_CFG_FRM_256BPF,
		.quant = AFE_PCM_CFG_QUANT_LINEAR_NOPAD,
		.slot = 0,
		.data = AFE_PCM_CFG_CDATAOE_MASTER,
		.pcm_clk_rate = 2048000,
	},
	.mode_16k = {
		.mode = AFE_PCM_CFG_MODE_PCM,
		.sync = AFE_PCM_CFG_SYNC_INT,
		.frame = AFE_PCM_CFG_FRM_256BPF,
		.quant = AFE_PCM_CFG_QUANT_LINEAR_NOPAD,
		.slot = 0,
		.data = AFE_PCM_CFG_CDATAOE_MASTER,
		.pcm_clk_rate = 4096000,
	}
};

struct platform_device msm_cpudai_auxpcm_rx = {
	.name = "msm-dai-q6",
	.id = 2,
	.dev = {
		.platform_data = &auxpcm_pdata,
	},
};

struct platform_device msm_cpudai_auxpcm_tx = {
	.name = "msm-dai-q6",
	.id = 3,
	.dev = {
		.platform_data = &auxpcm_pdata,
	},
};

struct platform_device msm_cpu_fe = {
	.name	= "msm-dai-fe",
	.id	= -1,
};

struct platform_device msm_stub_codec = {
	.name	= "msm-stub-codec",
	.id	= 1,
};

struct platform_device msm_voice = {
	.name	= "msm-pcm-voice",
	.id	= -1,
};

struct platform_device msm_voip = {
	.name	= "msm-voip-dsp",
	.id	= -1,
};

struct platform_device msm_lpa_pcm = {
	.name   = "msm-pcm-lpa",
	.id     = -1,
};

struct platform_device msm_compr_dsp = {
	.name	= "msm-compr-dsp",
	.id	= -1,
};

struct platform_device msm_pcm_hostless = {
	.name	= "msm-pcm-hostless",
	.id	= -1,
};

struct platform_device msm_cpudai_afe_01_rx = {
	.name = "msm-dai-q6",
	.id = 0xE0,
};

struct platform_device msm_cpudai_afe_01_tx = {
	.name = "msm-dai-q6",
	.id = 0xF0,
};

struct platform_device msm_cpudai_afe_02_rx = {
	.name = "msm-dai-q6",
	.id = 0xF1,
};

struct platform_device msm_cpudai_afe_02_tx = {
	.name = "msm-dai-q6",
	.id = 0xE1,
};

struct platform_device msm_pcm_afe = {
	.name	= "msm-pcm-afe",
	.id	= -1,
};

static struct fs_driver_data gfx2d0_fs_data = {
	.clks = (struct fs_clk_data[]){
		{ .name = "core_clk" },
		{ .name = "iface_clk" },
		{ 0 }
	},
	.bus_port0 = MSM_BUS_MASTER_GRAPHICS_2D_CORE0,
};

static struct fs_driver_data gfx2d1_fs_data = {
	.clks = (struct fs_clk_data[]){
		{ .name = "core_clk" },
		{ .name = "iface_clk" },
		{ 0 }
	},
	.bus_port0 = MSM_BUS_MASTER_GRAPHICS_2D_CORE1,
};

static struct fs_driver_data gfx3d_fs_data = {
	.clks = (struct fs_clk_data[]){
		{ .name = "core_clk", .reset_rate = 27000000 },
		{ .name = "iface_clk" },
		{ 0 }
	},
	.bus_port0 = MSM_BUS_MASTER_GRAPHICS_3D,
};

static struct fs_driver_data ijpeg_fs_data = {
	.clks = (struct fs_clk_data[]){
		{ .name = "core_clk" },
		{ .name = "iface_clk" },
		{ .name = "bus_clk" },
		{ 0 }
	},
	.bus_port0 = MSM_BUS_MASTER_JPEG_ENC,
};

static struct fs_driver_data mdp_fs_data = {
	.clks = (struct fs_clk_data[]){
		{ .name = "core_clk" },
		{ .name = "iface_clk" },
		{ .name = "bus_clk" },
		{ .name = "vsync_clk" },
		{ .name = "lut_clk" },
		{ .name = "tv_src_clk" },
		{ .name = "tv_clk" },
		{ 0 }
	},
	.bus_port0 = MSM_BUS_MASTER_MDP_PORT0,
	.bus_port1 = MSM_BUS_MASTER_MDP_PORT1,
};

static struct fs_driver_data rot_fs_data = {
	.clks = (struct fs_clk_data[]){
		{ .name = "core_clk" },
		{ .name = "iface_clk" },
		{ .name = "bus_clk" },
		{ 0 }
	},
	.bus_port0 = MSM_BUS_MASTER_ROTATOR,
};

static struct fs_driver_data ved_fs_data = {
	.clks = (struct fs_clk_data[]){
		{ .name = "core_clk" },
		{ .name = "iface_clk" },
		{ .name = "bus_clk" },
		{ 0 }
	},
	.bus_port0 = MSM_BUS_MASTER_HD_CODEC_PORT0,
	.bus_port1 = MSM_BUS_MASTER_HD_CODEC_PORT1,
};

static struct fs_driver_data vfe_fs_data = {
	.clks = (struct fs_clk_data[]){
		{ .name = "core_clk" },
		{ .name = "iface_clk" },
		{ .name = "bus_clk" },
		{ 0 }
	},
	.bus_port0 = MSM_BUS_MASTER_VFE,
};

static struct fs_driver_data vpe_fs_data = {
	.clks = (struct fs_clk_data[]){
		{ .name = "core_clk" },
		{ .name = "iface_clk" },
		{ .name = "bus_clk" },
		{ 0 }
	},
	.bus_port0 = MSM_BUS_MASTER_VPE,
};

struct platform_device *msm8960_footswitch[] __initdata = {
	FS_8X60(FS_MDP,    "vdd",	"mdp.0",	&mdp_fs_data),
	FS_8X60(FS_ROT,    "vdd",	"msm_rotator.0", &rot_fs_data),
	FS_8X60(FS_IJPEG,  "vdd",	"msm_gemini.0",	&ijpeg_fs_data),
	FS_8X60(FS_VFE,    "vdd",	"msm_vfe.0",	&vfe_fs_data),
	FS_8X60(FS_VPE,    "vdd",	"msm_vpe.0",	&vpe_fs_data),
	FS_8X60(FS_GFX3D,  "vdd",	"kgsl-3d0.0",	&gfx3d_fs_data),
	FS_8X60(FS_GFX2D0, "vdd",	"kgsl-2d0.0",	&gfx2d0_fs_data),
	FS_8X60(FS_GFX2D1, "vdd",	"kgsl-2d1.1",	&gfx2d1_fs_data),
	FS_8X60(FS_VED,    "vdd",	"msm_vidc.0",	&ved_fs_data),
};
unsigned msm8960_num_footswitch __initdata = ARRAY_SIZE(msm8960_footswitch);

#ifdef CONFIG_MSM_ROTATOR
static struct msm_bus_vectors rotator_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_ROTATOR,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors rotator_ui_vectors[] = {
	{
		.src = MSM_BUS_MASTER_ROTATOR,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = (1024 * 600 * 4 * 2 * 60),
		.ib  = (1024 * 600 * 4 * 2 * 60 * 1.5),
	},
};

static struct msm_bus_vectors rotator_vga_vectors[] = {
	{
		.src = MSM_BUS_MASTER_ROTATOR,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = (640 * 480 * 2 * 2 * 30),
		.ib  = (640 * 480 * 2 * 2 * 30 * 1.5),
	},
};
static struct msm_bus_vectors rotator_720p_vectors[] = {
	{
		.src = MSM_BUS_MASTER_ROTATOR,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = (1280 * 736 * 2 * 2 * 30),
		.ib  = (1280 * 736 * 2 * 2 * 30 * 1.5),
	},
};

static struct msm_bus_vectors rotator_1080p_vectors[] = {
	{
		.src = MSM_BUS_MASTER_ROTATOR,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = (1920 * 1088 * 2 * 2 * 30),
		.ib  = (1920 * 1088 * 2 * 2 * 30 * 1.5),
	},
};

static struct msm_bus_paths rotator_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(rotator_init_vectors),
		rotator_init_vectors,
	},
	{
		ARRAY_SIZE(rotator_ui_vectors),
		rotator_ui_vectors,
	},
	{
		ARRAY_SIZE(rotator_vga_vectors),
		rotator_vga_vectors,
	},
	{
		ARRAY_SIZE(rotator_720p_vectors),
		rotator_720p_vectors,
	},
	{
		ARRAY_SIZE(rotator_1080p_vectors),
		rotator_1080p_vectors,
	},
};

struct msm_bus_scale_pdata rotator_bus_scale_pdata = {
	rotator_bus_scale_usecases,
	ARRAY_SIZE(rotator_bus_scale_usecases),
	.name = "rotator",
};

void __init msm_rotator_update_bus_vectors(unsigned int xres,
	unsigned int yres)
{
	rotator_ui_vectors[0].ab = xres * yres * 4 * 2 * 60;
	rotator_ui_vectors[0].ib = xres * yres * 4 * 2 * 60 * 3 / 2;
}

#define ROTATOR_HW_BASE         0x04E00000
static struct resource resources_msm_rotator[] = {
	{
		.start  = ROTATOR_HW_BASE,
		.end    = ROTATOR_HW_BASE + 0x100000 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.start  = ROT_IRQ,
		.end    = ROT_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct msm_rot_clocks rotator_clocks[] = {
	{
		.clk_name = "core_clk",
		.clk_type = ROTATOR_CORE_CLK,
		.clk_rate = 200 * 1000 * 1000,
	},
	{
		.clk_name = "iface_clk",
		.clk_type = ROTATOR_PCLK,
		.clk_rate = 0,
	},
};

static struct msm_rotator_platform_data rotator_pdata = {
	.number_of_clocks = ARRAY_SIZE(rotator_clocks),
	.hardware_version_number = 0x01020309,
	.rotator_clks = rotator_clocks,
#ifdef CONFIG_MSM_BUS_SCALING
	.bus_scale_table = &rotator_bus_scale_pdata,
#endif
};

struct platform_device msm_rotator_device = {
	.name           = "msm_rotator",
	.id             = 0,
	.num_resources  = ARRAY_SIZE(resources_msm_rotator),
	.resource       = resources_msm_rotator,
	.dev            = {
		.platform_data = &rotator_pdata,
	},
};

void __init msm_rotator_set_split_iommu_domain(void)
{
	rotator_pdata.rot_iommu_split_domain = 1;
}
#endif

#define MIPI_DSI_HW_BASE        0x04700000
#define MDP_HW_BASE             0x05100000

static struct resource msm_mipi_dsi1_resources[] = {
	{
		.name   = "mipi_dsi",
		.start  = MIPI_DSI_HW_BASE,
		.end    = MIPI_DSI_HW_BASE + 0x000F0000 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.start  = DSI1_IRQ,
		.end    = DSI1_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device msm_mipi_dsi1_device = {
	.name   = "mipi_dsi",
	.id     = 1,
	.num_resources  = ARRAY_SIZE(msm_mipi_dsi1_resources),
	.resource       = msm_mipi_dsi1_resources,
};

static struct resource msm_mdp_resources[] = {
	{
		.name   = "mdp",
		.start  = MDP_HW_BASE,
		.end    = MDP_HW_BASE + 0x000F0000 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.start  = MDP_IRQ,
		.end    = MDP_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device msm_mdp_device = {
	.name   = "mdp",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(msm_mdp_resources),
	.resource       = msm_mdp_resources,
};

static void __init msm_register_device(struct platform_device *pdev, void *data)
{
	int ret;

	pdev->dev.platform_data = data;
	ret = platform_device_register(pdev);
	if (ret)
		dev_err(&pdev->dev,
			"%s: platform_device_register() failed = %d\n",
			__func__, ret);
}

#ifdef CONFIG_MSM_BUS_SCALING
static struct platform_device msm_dtv_device = {
	.name   = "dtv",
	.id     = 0,
};
#endif

struct platform_device msm_lvds_device = {
	.name   = "lvds",
	.id     = 0,
};

void __init msm_fb_register_device(char *name, void *data)
{
	if (!strncmp(name, "mdp", 3))
		msm_register_device(&msm_mdp_device, data);
	else if (!strncmp(name, "mipi_dsi", 8))
		msm_register_device(&msm_mipi_dsi1_device, data);
	else if (!strncmp(name, "lvds", 4))
		msm_register_device(&msm_lvds_device, data);
#ifdef CONFIG_MSM_BUS_SCALING
	else if (!strncmp(name, "dtv", 3))
		msm_register_device(&msm_dtv_device, data);
#endif
	else
		printk(KERN_ERR "%s: unknown device! %s\n", __func__, name);
}

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

#ifdef CONFIG_MSM_MPM
static uint16_t msm_mpm_irqs_m2a[MSM_MPM_NR_MPM_IRQS] __initdata = {
	[1] = MSM_GPIO_TO_INT(46),
	[2] = MSM_GPIO_TO_INT(150),
	[4] = MSM_GPIO_TO_INT(103),
	[5] = MSM_GPIO_TO_INT(104),
	[6] = MSM_GPIO_TO_INT(105),
	[7] = MSM_GPIO_TO_INT(106),
	[8] = MSM_GPIO_TO_INT(107),
	[9] = MSM_GPIO_TO_INT(7),
	[10] = MSM_GPIO_TO_INT(11),
	[11] = MSM_GPIO_TO_INT(15),
	[12] = MSM_GPIO_TO_INT(19),
	[13] = MSM_GPIO_TO_INT(23),
	[14] = MSM_GPIO_TO_INT(27),
	[15] = MSM_GPIO_TO_INT(31),
	[16] = MSM_GPIO_TO_INT(35),
	[19] = MSM_GPIO_TO_INT(90),
	[20] = MSM_GPIO_TO_INT(92),
	[23] = MSM_GPIO_TO_INT(85),
	[24] = MSM_GPIO_TO_INT(83),
	[25] = USB1_HS_IRQ,
	[27] = HDMI_IRQ,
	[29] = MSM_GPIO_TO_INT(10),
	[30] = MSM_GPIO_TO_INT(102),
	[31] = MSM_GPIO_TO_INT(81),
	[32] = MSM_GPIO_TO_INT(78),
	[33] = MSM_GPIO_TO_INT(94),
	[34] = MSM_GPIO_TO_INT(72),
	[35] = MSM_GPIO_TO_INT(39),
	[36] = MSM_GPIO_TO_INT(43),
	[37] = MSM_GPIO_TO_INT(61),
	[38] = MSM_GPIO_TO_INT(50),
	[39] = MSM_GPIO_TO_INT(42),
	[41] = MSM_GPIO_TO_INT(62),
	[42] = MSM_GPIO_TO_INT(76),
	[43] = MSM_GPIO_TO_INT(75),
	[44] = MSM_GPIO_TO_INT(70),
	[45] = MSM_GPIO_TO_INT(69),
	[46] = MSM_GPIO_TO_INT(67),
	[47] = MSM_GPIO_TO_INT(65),
	[48] = MSM_GPIO_TO_INT(58),
	[49] = MSM_GPIO_TO_INT(54),
	[50] = MSM_GPIO_TO_INT(52),
	[51] = MSM_GPIO_TO_INT(49),
	[52] = MSM_GPIO_TO_INT(40),
	[53] = MSM_GPIO_TO_INT(37),
	[54] = MSM_GPIO_TO_INT(24),
	[55] = MSM_GPIO_TO_INT(14),
};

static uint16_t msm_mpm_bypassed_apps_irqs[] __initdata = {
	TLMM_MSM_SUMMARY_IRQ,
	RPM_APCC_CPU0_GP_HIGH_IRQ,
	RPM_APCC_CPU0_GP_MEDIUM_IRQ,
	RPM_APCC_CPU0_GP_LOW_IRQ,
	RPM_APCC_CPU0_WAKE_UP_IRQ,
	RPM_APCC_CPU1_GP_HIGH_IRQ,
	RPM_APCC_CPU1_GP_MEDIUM_IRQ,
	RPM_APCC_CPU1_GP_LOW_IRQ,
	RPM_APCC_CPU1_WAKE_UP_IRQ,
	MSS_TO_APPS_IRQ_0,
	MSS_TO_APPS_IRQ_1,
	MSS_TO_APPS_IRQ_2,
	MSS_TO_APPS_IRQ_3,
	MSS_TO_APPS_IRQ_4,
	MSS_TO_APPS_IRQ_5,
	MSS_TO_APPS_IRQ_6,
	MSS_TO_APPS_IRQ_7,
	MSS_TO_APPS_IRQ_8,
	MSS_TO_APPS_IRQ_9,
	LPASS_SCSS_GP_LOW_IRQ,
	LPASS_SCSS_GP_MEDIUM_IRQ,
	LPASS_SCSS_GP_HIGH_IRQ,
	SPS_MTI_30,
	SPS_MTI_31,
	RIVA_APSS_SPARE_IRQ,
	RIVA_APPS_WLAN_SMSM_IRQ,
	RIVA_APPS_WLAN_RX_DATA_AVAIL_IRQ,
	RIVA_APPS_WLAN_DATA_XFER_DONE_IRQ,
};

struct msm_mpm_device_data msm8960_mpm_dev_data __initdata = {
	.irqs_m2a = msm_mpm_irqs_m2a,
	.irqs_m2a_size = ARRAY_SIZE(msm_mpm_irqs_m2a),
	.bypassed_apps_irqs = msm_mpm_bypassed_apps_irqs,
	.bypassed_apps_irqs_size = ARRAY_SIZE(msm_mpm_bypassed_apps_irqs),
	.mpm_request_reg_base = MSM_RPM_BASE + 0x9d8,
	.mpm_status_reg_base = MSM_RPM_BASE + 0xdf8,
	.mpm_apps_ipc_reg = MSM_APCS_GCC_BASE + 0x008,
	.mpm_apps_ipc_val =  BIT(1),
	.mpm_ipc_irq = RPM_APCC_CPU0_GP_MEDIUM_IRQ,

};
#endif

#define LPASS_SLIMBUS_PHYS	0x28080000
#define LPASS_SLIMBUS_BAM_PHYS	0x28084000
#define LPASS_SLIMBUS_SLEW	(MSM8960_TLMM_PHYS + 0x207C)
/* Board info for the slimbus slave device */
static struct resource slimbus_res[] = {
	{
		.start	= LPASS_SLIMBUS_PHYS,
		.end	= LPASS_SLIMBUS_PHYS + 8191,
		.flags	= IORESOURCE_MEM,
		.name	= "slimbus_physical",
	},
	{
		.start	= LPASS_SLIMBUS_BAM_PHYS,
		.end	= LPASS_SLIMBUS_BAM_PHYS + 8191,
		.flags	= IORESOURCE_MEM,
		.name	= "slimbus_bam_physical",
	},
	{
		.start	= LPASS_SLIMBUS_SLEW,
		.end	= LPASS_SLIMBUS_SLEW + 4 - 1,
		.flags	= IORESOURCE_MEM,
		.name	= "slimbus_slew_reg",
	},
	{
		.start	= SLIMBUS0_CORE_EE1_IRQ,
		.end	= SLIMBUS0_CORE_EE1_IRQ,
		.flags	= IORESOURCE_IRQ,
		.name	= "slimbus_irq",
	},
	{
		.start	= SLIMBUS0_BAM_EE1_IRQ,
		.end	= SLIMBUS0_BAM_EE1_IRQ,
		.flags	= IORESOURCE_IRQ,
		.name	= "slimbus_bam_irq",
	},
};

struct platform_device msm_slim_ctrl = {
	.name	= "msm_slim_ctrl",
	.id	= 1,
	.num_resources	= ARRAY_SIZE(slimbus_res),
	.resource	= slimbus_res,
	.dev            = {
		.coherent_dma_mask      = 0xffffffffULL,
	},
};

static struct msm_dcvs_freq_entry grp3d_freq[] = {
	{0, 0, 333932},
	{0, 0, 497532},
	{0, 0, 707610},
	{0, 0, 844545},
};

static struct msm_dcvs_freq_entry grp2d_freq[] = {
	{0, 0, 86000},
	{0, 0, 200000},
};

static struct msm_dcvs_core_info grp3d_core_info = {
	.freq_tbl = &grp3d_freq[0],
	.core_param = {
		.max_time_us = 100000,
		.num_freq = ARRAY_SIZE(grp3d_freq),
	},
	.algo_param = {
		.slack_time_us = 39000,
		.disable_pc_threshold = 86000,
		.ss_window_size = 1000000,
		.ss_util_pct = 95,
		.em_max_util_pct = 97,
		.ss_iobusy_conv = 100,
	},
};

static struct msm_dcvs_core_info grp2d_core_info = {
	.freq_tbl = &grp2d_freq[0],
	.core_param = {
		.max_time_us = 100000,
		.num_freq = ARRAY_SIZE(grp2d_freq),
	},
	.algo_param = {
		.slack_time_us = 39000,
		.disable_pc_threshold = 90000,
		.ss_window_size = 1000000,
		.ss_util_pct = 90,
		.em_max_util_pct = 95,
	},
};

#ifdef CONFIG_MSM_BUS_SCALING
static struct msm_bus_vectors grp3d_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_GRAPHICS_3D,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors grp3d_low_vectors[] = {
	{
		.src = MSM_BUS_MASTER_GRAPHICS_3D,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = KGSL_CONVERT_TO_MBPS(1000),
	},
};

static struct msm_bus_vectors grp3d_nominal_low_vectors[] = {
	{
		.src = MSM_BUS_MASTER_GRAPHICS_3D,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = KGSL_CONVERT_TO_MBPS(2048),
	},
};

static struct msm_bus_vectors grp3d_nominal_high_vectors[] = {
	{
		.src = MSM_BUS_MASTER_GRAPHICS_3D,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = KGSL_CONVERT_TO_MBPS(2656),
	},
};

static struct msm_bus_vectors grp3d_max_vectors[] = {
	{
		.src = MSM_BUS_MASTER_GRAPHICS_3D,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = KGSL_CONVERT_TO_MBPS(3968),
	},
};

static struct msm_bus_paths grp3d_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(grp3d_init_vectors),
		grp3d_init_vectors,
	},
	{
		ARRAY_SIZE(grp3d_low_vectors),
		grp3d_low_vectors,
	},
	{
		ARRAY_SIZE(grp3d_nominal_low_vectors),
		grp3d_nominal_low_vectors,
	},
	{
		ARRAY_SIZE(grp3d_nominal_high_vectors),
		grp3d_nominal_high_vectors,
	},
	{
		ARRAY_SIZE(grp3d_max_vectors),
		grp3d_max_vectors,
	},
};

static struct msm_bus_scale_pdata grp3d_bus_scale_pdata = {
	grp3d_bus_scale_usecases,
	ARRAY_SIZE(grp3d_bus_scale_usecases),
	.name = "grp3d",
};

static struct msm_bus_vectors grp2d0_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_GRAPHICS_2D_CORE0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors grp2d0_nominal_vectors[] = {
	{
		.src = MSM_BUS_MASTER_GRAPHICS_2D_CORE0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = KGSL_CONVERT_TO_MBPS(1000),
	},
};

static struct msm_bus_vectors grp2d0_max_vectors[] = {
	{
		.src = MSM_BUS_MASTER_GRAPHICS_2D_CORE0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = KGSL_CONVERT_TO_MBPS(2048),
	},
};

static struct msm_bus_paths grp2d0_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(grp2d0_init_vectors),
		grp2d0_init_vectors,
	},
	{
		ARRAY_SIZE(grp2d0_nominal_vectors),
		grp2d0_nominal_vectors,
	},
	{
		ARRAY_SIZE(grp2d0_max_vectors),
		grp2d0_max_vectors,
	},
};

struct msm_bus_scale_pdata grp2d0_bus_scale_pdata = {
	grp2d0_bus_scale_usecases,
	ARRAY_SIZE(grp2d0_bus_scale_usecases),
	.name = "grp2d0",
};

static struct msm_bus_vectors grp2d1_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_GRAPHICS_2D_CORE1,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors grp2d1_nominal_vectors[] = {
	{
		.src = MSM_BUS_MASTER_GRAPHICS_2D_CORE1,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = KGSL_CONVERT_TO_MBPS(1000),
	},
};

static struct msm_bus_vectors grp2d1_max_vectors[] = {
	{
		.src = MSM_BUS_MASTER_GRAPHICS_2D_CORE1,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = KGSL_CONVERT_TO_MBPS(2048),
	},
};

static struct msm_bus_paths grp2d1_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(grp2d1_init_vectors),
		grp2d1_init_vectors,
	},
	{
		ARRAY_SIZE(grp2d1_nominal_vectors),
		grp2d1_nominal_vectors,
	},
	{
		ARRAY_SIZE(grp2d1_max_vectors),
		grp2d1_max_vectors,
	},
};

struct msm_bus_scale_pdata grp2d1_bus_scale_pdata = {
	grp2d1_bus_scale_usecases,
	ARRAY_SIZE(grp2d1_bus_scale_usecases),
	.name = "grp2d1",
};
#endif

static struct resource kgsl_3d0_resources[] = {
	{
		.name = KGSL_3D0_REG_MEMORY,
		.start = 0x04300000, /* GFX3D address */
		.end = 0x0431ffff,
		.flags = IORESOURCE_MEM,
	},
	{
		.name = KGSL_3D0_IRQ,
		.start = GFX3D_IRQ,
		.end = GFX3D_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static const struct kgsl_iommu_ctx kgsl_3d0_iommu_ctxs[] = {
	{ "gfx3d_user", 0 },
	{ "gfx3d_priv", 1 },
};

static struct kgsl_device_iommu_data kgsl_3d0_iommu_data[] = {
	{
		.iommu_ctxs = kgsl_3d0_iommu_ctxs,
		.iommu_ctx_count = ARRAY_SIZE(kgsl_3d0_iommu_ctxs),
		.physstart = 0x07C00000,
		.physend = 0x07C00000 + SZ_1M - 1,
	},
};

static struct kgsl_device_platform_data kgsl_3d0_pdata = {
	.pwrlevel = {
		{
			.gpu_freq = 400000000,
			.bus_freq = 4,
			.io_fraction = 0,
		},
		{
			.gpu_freq = 300000000,
			.bus_freq = 3,
			.io_fraction = 33,
		},
		{
			.gpu_freq = 200000000,
			.bus_freq = 2,
			.io_fraction = 100,
		},
		{
			.gpu_freq = 128000000,
			.bus_freq = 1,
			.io_fraction = 100,
		},
		{
			.gpu_freq = 27000000,
			.bus_freq = 0,
		},
	},
	.init_level = 1,
	.num_levels = ARRAY_SIZE(grp3d_freq) + 1,
	.set_grp_async = NULL,
	.idle_timeout = HZ/12,
	.nap_allowed = true,
	.clk_map = KGSL_CLK_CORE | KGSL_CLK_IFACE | KGSL_CLK_MEM_IFACE,
#ifdef CONFIG_MSM_BUS_SCALING
	.bus_scale_table = &grp3d_bus_scale_pdata,
#endif
	.iommu_data = kgsl_3d0_iommu_data,
	.iommu_count = ARRAY_SIZE(kgsl_3d0_iommu_data),
	.core_info = &grp3d_core_info,
};

struct platform_device msm_kgsl_3d0 = {
	.name = "kgsl-3d0",
	.id = 0,
	.num_resources = ARRAY_SIZE(kgsl_3d0_resources),
	.resource = kgsl_3d0_resources,
	.dev = {
		.platform_data = &kgsl_3d0_pdata,
	},
};

static struct resource kgsl_2d0_resources[] = {
	{
		.name = KGSL_2D0_REG_MEMORY,
		.start = 0x04100000, /* Z180 base address */
		.end = 0x04100FFF,
		.flags = IORESOURCE_MEM,
	},
	{
		.name  = KGSL_2D0_IRQ,
		.start = GFX2D0_IRQ,
		.end = GFX2D0_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static const struct kgsl_iommu_ctx kgsl_2d0_iommu_ctxs[] = {
	{ "gfx2d0_2d0", 0 },
};

static struct kgsl_device_iommu_data kgsl_2d0_iommu_data[] = {
	{
		.iommu_ctxs = kgsl_2d0_iommu_ctxs,
		.iommu_ctx_count = ARRAY_SIZE(kgsl_2d0_iommu_ctxs),
		.physstart = 0x07D00000,
		.physend = 0x07D00000 + SZ_1M - 1,
	},
};

static struct kgsl_device_platform_data kgsl_2d0_pdata = {
	.pwrlevel = {
		{
			.gpu_freq = 200000000,
			.bus_freq = 2,
		},
		{
			.gpu_freq = 96000000,
			.bus_freq = 1,
		},
		{
			.gpu_freq = 27000000,
			.bus_freq = 0,
		},
	},
	.init_level = 0,
	.num_levels = ARRAY_SIZE(grp2d_freq) + 1,
	.set_grp_async = NULL,
	.idle_timeout = HZ/5,
	.nap_allowed = true,
	.clk_map = KGSL_CLK_CORE | KGSL_CLK_IFACE,
#ifdef CONFIG_MSM_BUS_SCALING
	.bus_scale_table = &grp2d0_bus_scale_pdata,
#endif
	.iommu_data = kgsl_2d0_iommu_data,
	.iommu_count = ARRAY_SIZE(kgsl_2d0_iommu_data),
	.core_info = &grp2d_core_info,
};

struct platform_device msm_kgsl_2d0 = {
	.name = "kgsl-2d0",
	.id = 0,
	.num_resources = ARRAY_SIZE(kgsl_2d0_resources),
	.resource = kgsl_2d0_resources,
	.dev = {
		.platform_data = &kgsl_2d0_pdata,
	},
};

static const struct kgsl_iommu_ctx kgsl_2d1_iommu_ctxs[] = {
	{ "gfx2d1_2d1", 0 },
};

static struct kgsl_device_iommu_data kgsl_2d1_iommu_data[] = {
	{
		.iommu_ctxs = kgsl_2d1_iommu_ctxs,
		.iommu_ctx_count = ARRAY_SIZE(kgsl_2d1_iommu_ctxs),
		.physstart = 0x07E00000,
		.physend = 0x07E00000 + SZ_1M - 1,
	},
};

static struct resource kgsl_2d1_resources[] = {
	{
		.name = KGSL_2D1_REG_MEMORY,
		.start = 0x04200000, /* Z180 device 1 base address */
		.end =   0x04200FFF,
		.flags = IORESOURCE_MEM,
	},
	{
		.name  = KGSL_2D1_IRQ,
		.start = GFX2D1_IRQ,
		.end = GFX2D1_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static struct kgsl_device_platform_data kgsl_2d1_pdata = {
	.pwrlevel = {
		{
			.gpu_freq = 200000000,
			.bus_freq = 2,
		},
		{
			.gpu_freq = 96000000,
			.bus_freq = 1,
		},
		{
			.gpu_freq = 27000000,
			.bus_freq = 0,
		},
	},
	.init_level = 0,
	.num_levels = ARRAY_SIZE(grp2d_freq) + 1,
	.set_grp_async = NULL,
	.idle_timeout = HZ/5,
	.nap_allowed = true,
	.clk_map = KGSL_CLK_CORE | KGSL_CLK_IFACE,
#ifdef CONFIG_MSM_BUS_SCALING
	.bus_scale_table = &grp2d1_bus_scale_pdata,
#endif
	.iommu_data = kgsl_2d1_iommu_data,
	.iommu_count = ARRAY_SIZE(kgsl_2d1_iommu_data),
	.core_info = &grp2d_core_info,
};

struct platform_device msm_kgsl_2d1 = {
	.name = "kgsl-2d1",
	.id = 1,
	.num_resources = ARRAY_SIZE(kgsl_2d1_resources),
	.resource = kgsl_2d1_resources,
	.dev = {
		.platform_data = &kgsl_2d1_pdata,
	},
};

#ifdef CONFIG_MSM_GEMINI
static struct resource msm_gemini_resources[] = {
	{
		.start  = 0x04600000,
		.end    = 0x04600000 + SZ_1M - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.start  = JPEG_IRQ,
		.end    = JPEG_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device msm8960_gemini_device = {
	.name           = "msm_gemini",
	.resource       = msm_gemini_resources,
	.num_resources  = ARRAY_SIZE(msm_gemini_resources),
};
#endif

#ifdef CONFIG_MSM_MERCURY
static struct resource msm_mercury_resources[] = {
	{
		.start  = 0x05000000,
		.end  = 0x05000000 + SZ_1M - 1,
		.name   = "mercury_resource_base",
		.flags  = IORESOURCE_MEM,
	},
	{
		.start  = JPEGD_IRQ,
		.end  = JPEGD_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
};
struct platform_device msm8960_mercury_device = {
	.name       = "msm_mercury",
	.resource     = msm_mercury_resources,
	.num_resources  = ARRAY_SIZE(msm_mercury_resources),
};
#endif

struct msm_rpm_platform_data msm8960_rpm_data __initdata = {
	.reg_base_addrs = {
		[MSM_RPM_PAGE_STATUS] = MSM_RPM_BASE,
		[MSM_RPM_PAGE_CTRL] = MSM_RPM_BASE + 0x400,
		[MSM_RPM_PAGE_REQ] = MSM_RPM_BASE + 0x600,
		[MSM_RPM_PAGE_ACK] = MSM_RPM_BASE + 0xa00,
	},
	.irq_ack = RPM_APCC_CPU0_GP_HIGH_IRQ,
	.irq_err = RPM_APCC_CPU0_GP_LOW_IRQ,
	.irq_wakeup = RPM_APCC_CPU0_WAKE_UP_IRQ,
	.ipc_rpm_reg = MSM_APCS_GCC_BASE + 0x008,
	.ipc_rpm_val = 4,
	.target_id = {
		MSM_RPM_MAP(8960, NOTIFICATION_CONFIGURED_0, NOTIFICATION, 4),
		MSM_RPM_MAP(8960, NOTIFICATION_REGISTERED_0, NOTIFICATION, 4),
		MSM_RPM_MAP(8960, INVALIDATE_0, INVALIDATE, 8),
		MSM_RPM_MAP(8960, TRIGGER_TIMED_TO, TRIGGER_TIMED, 1),
		MSM_RPM_MAP(8960, TRIGGER_TIMED_SCLK_COUNT, TRIGGER_TIMED, 1),
		MSM_RPM_MAP(8960, RPM_CTL, RPM_CTL, 1),
		MSM_RPM_MAP(8960, CXO_CLK, CXO_CLK, 1),
		MSM_RPM_MAP(8960, PXO_CLK, PXO_CLK, 1),
		MSM_RPM_MAP(8960, APPS_FABRIC_CLK, APPS_FABRIC_CLK, 1),
		MSM_RPM_MAP(8960, SYSTEM_FABRIC_CLK, SYSTEM_FABRIC_CLK, 1),
		MSM_RPM_MAP(8960, MM_FABRIC_CLK, MM_FABRIC_CLK, 1),
		MSM_RPM_MAP(8960, DAYTONA_FABRIC_CLK, DAYTONA_FABRIC_CLK, 1),
		MSM_RPM_MAP(8960, SFPB_CLK, SFPB_CLK, 1),
		MSM_RPM_MAP(8960, CFPB_CLK, CFPB_CLK, 1),
		MSM_RPM_MAP(8960, MMFPB_CLK, MMFPB_CLK, 1),
		MSM_RPM_MAP(8960, EBI1_CLK, EBI1_CLK, 1),
		MSM_RPM_MAP(8960, APPS_FABRIC_CFG_HALT_0,
				APPS_FABRIC_CFG_HALT, 2),
		MSM_RPM_MAP(8960, APPS_FABRIC_CFG_CLKMOD_0,
				APPS_FABRIC_CFG_CLKMOD, 3),
		MSM_RPM_MAP(8960, APPS_FABRIC_CFG_IOCTL,
				APPS_FABRIC_CFG_IOCTL, 1),
		MSM_RPM_MAP(8960, APPS_FABRIC_ARB_0, APPS_FABRIC_ARB, 12),
		MSM_RPM_MAP(8960, SYS_FABRIC_CFG_HALT_0,
				SYS_FABRIC_CFG_HALT, 2),
		MSM_RPM_MAP(8960, SYS_FABRIC_CFG_CLKMOD_0,
				SYS_FABRIC_CFG_CLKMOD, 3),
		MSM_RPM_MAP(8960, SYS_FABRIC_CFG_IOCTL,
				SYS_FABRIC_CFG_IOCTL, 1),
		MSM_RPM_MAP(8960, SYSTEM_FABRIC_ARB_0,
				SYSTEM_FABRIC_ARB, 29),
		MSM_RPM_MAP(8960, MMSS_FABRIC_CFG_HALT_0,
				MMSS_FABRIC_CFG_HALT, 2),
		MSM_RPM_MAP(8960, MMSS_FABRIC_CFG_CLKMOD_0,
				MMSS_FABRIC_CFG_CLKMOD, 3),
		MSM_RPM_MAP(8960, MMSS_FABRIC_CFG_IOCTL,
				MMSS_FABRIC_CFG_IOCTL, 1),
		MSM_RPM_MAP(8960, MM_FABRIC_ARB_0, MM_FABRIC_ARB, 23),
		MSM_RPM_MAP(8960, PM8921_S1_0, PM8921_S1, 2),
		MSM_RPM_MAP(8960, PM8921_S2_0, PM8921_S2, 2),
		MSM_RPM_MAP(8960, PM8921_S3_0, PM8921_S3, 2),
		MSM_RPM_MAP(8960, PM8921_S4_0, PM8921_S4, 2),
		MSM_RPM_MAP(8960, PM8921_S5_0, PM8921_S5, 2),
		MSM_RPM_MAP(8960, PM8921_S6_0, PM8921_S6, 2),
		MSM_RPM_MAP(8960, PM8921_S7_0, PM8921_S7, 2),
		MSM_RPM_MAP(8960, PM8921_S8_0, PM8921_S8, 2),
		MSM_RPM_MAP(8960, PM8921_L1_0, PM8921_L1, 2),
		MSM_RPM_MAP(8960, PM8921_L2_0, PM8921_L2, 2),
		MSM_RPM_MAP(8960, PM8921_L3_0, PM8921_L3, 2),
		MSM_RPM_MAP(8960, PM8921_L4_0, PM8921_L4, 2),
		MSM_RPM_MAP(8960, PM8921_L5_0, PM8921_L5, 2),
		MSM_RPM_MAP(8960, PM8921_L6_0, PM8921_L6, 2),
		MSM_RPM_MAP(8960, PM8921_L7_0, PM8921_L7, 2),
		MSM_RPM_MAP(8960, PM8921_L8_0, PM8921_L8, 2),
		MSM_RPM_MAP(8960, PM8921_L9_0, PM8921_L9, 2),
		MSM_RPM_MAP(8960, PM8921_L10_0, PM8921_L10, 2),
		MSM_RPM_MAP(8960, PM8921_L11_0, PM8921_L11, 2),
		MSM_RPM_MAP(8960, PM8921_L12_0, PM8921_L12, 2),
		MSM_RPM_MAP(8960, PM8921_L13_0, PM8921_L13, 2),
		MSM_RPM_MAP(8960, PM8921_L14_0, PM8921_L14, 2),
		MSM_RPM_MAP(8960, PM8921_L15_0, PM8921_L15, 2),
		MSM_RPM_MAP(8960, PM8921_L16_0, PM8921_L16, 2),
		MSM_RPM_MAP(8960, PM8921_L17_0, PM8921_L17, 2),
		MSM_RPM_MAP(8960, PM8921_L18_0, PM8921_L18, 2),
		MSM_RPM_MAP(8960, PM8921_L19_0, PM8921_L19, 2),
		MSM_RPM_MAP(8960, PM8921_L20_0, PM8921_L20, 2),
		MSM_RPM_MAP(8960, PM8921_L21_0, PM8921_L21, 2),
		MSM_RPM_MAP(8960, PM8921_L22_0, PM8921_L22, 2),
		MSM_RPM_MAP(8960, PM8921_L23_0, PM8921_L23, 2),
		MSM_RPM_MAP(8960, PM8921_L24_0, PM8921_L24, 2),
		MSM_RPM_MAP(8960, PM8921_L25_0, PM8921_L25, 2),
		MSM_RPM_MAP(8960, PM8921_L26_0, PM8921_L26, 2),
		MSM_RPM_MAP(8960, PM8921_L27_0, PM8921_L27, 2),
		MSM_RPM_MAP(8960, PM8921_L28_0, PM8921_L28, 2),
		MSM_RPM_MAP(8960, PM8921_L29_0, PM8921_L29, 2),
		MSM_RPM_MAP(8960, PM8921_CLK1_0, PM8921_CLK1, 2),
		MSM_RPM_MAP(8960, PM8921_CLK2_0, PM8921_CLK2, 2),
		MSM_RPM_MAP(8960, PM8921_LVS1, PM8921_LVS1, 1),
		MSM_RPM_MAP(8960, PM8921_LVS2, PM8921_LVS2, 1),
		MSM_RPM_MAP(8960, PM8921_LVS3, PM8921_LVS3, 1),
		MSM_RPM_MAP(8960, PM8921_LVS4, PM8921_LVS4, 1),
		MSM_RPM_MAP(8960, PM8921_LVS5, PM8921_LVS5, 1),
		MSM_RPM_MAP(8960, PM8921_LVS6, PM8921_LVS6, 1),
		MSM_RPM_MAP(8960, PM8921_LVS7, PM8921_LVS7, 1),
		MSM_RPM_MAP(8960, NCP_0, NCP, 2),
		MSM_RPM_MAP(8960, CXO_BUFFERS, CXO_BUFFERS, 1),
		MSM_RPM_MAP(8960, USB_OTG_SWITCH, USB_OTG_SWITCH, 1),
		MSM_RPM_MAP(8960, HDMI_SWITCH, HDMI_SWITCH, 1),
		MSM_RPM_MAP(8960, DDR_DMM_0, DDR_DMM, 2),
		MSM_RPM_MAP(8960, QDSS_CLK, QDSS_CLK, 1),
	},
	.target_status = {
		MSM_RPM_STATUS_ID_MAP(8960, VERSION_MAJOR),
		MSM_RPM_STATUS_ID_MAP(8960, VERSION_MINOR),
		MSM_RPM_STATUS_ID_MAP(8960, VERSION_BUILD),
		MSM_RPM_STATUS_ID_MAP(8960, SUPPORTED_RESOURCES_0),
		MSM_RPM_STATUS_ID_MAP(8960, SUPPORTED_RESOURCES_1),
		MSM_RPM_STATUS_ID_MAP(8960, SUPPORTED_RESOURCES_2),
		MSM_RPM_STATUS_ID_MAP(8960, RESERVED_SUPPORTED_RESOURCES_0),
		MSM_RPM_STATUS_ID_MAP(8960, SEQUENCE),
		MSM_RPM_STATUS_ID_MAP(8960, RPM_CTL),
		MSM_RPM_STATUS_ID_MAP(8960, CXO_CLK),
		MSM_RPM_STATUS_ID_MAP(8960, PXO_CLK),
		MSM_RPM_STATUS_ID_MAP(8960, APPS_FABRIC_CLK),
		MSM_RPM_STATUS_ID_MAP(8960, SYSTEM_FABRIC_CLK),
		MSM_RPM_STATUS_ID_MAP(8960, MM_FABRIC_CLK),
		MSM_RPM_STATUS_ID_MAP(8960, DAYTONA_FABRIC_CLK),
		MSM_RPM_STATUS_ID_MAP(8960, SFPB_CLK),
		MSM_RPM_STATUS_ID_MAP(8960, CFPB_CLK),
		MSM_RPM_STATUS_ID_MAP(8960, MMFPB_CLK),
		MSM_RPM_STATUS_ID_MAP(8960, EBI1_CLK),
		MSM_RPM_STATUS_ID_MAP(8960, APPS_FABRIC_CFG_HALT),
		MSM_RPM_STATUS_ID_MAP(8960, APPS_FABRIC_CFG_CLKMOD),
		MSM_RPM_STATUS_ID_MAP(8960, APPS_FABRIC_CFG_IOCTL),
		MSM_RPM_STATUS_ID_MAP(8960, APPS_FABRIC_ARB),
		MSM_RPM_STATUS_ID_MAP(8960, SYS_FABRIC_CFG_HALT),
		MSM_RPM_STATUS_ID_MAP(8960, SYS_FABRIC_CFG_CLKMOD),
		MSM_RPM_STATUS_ID_MAP(8960, SYS_FABRIC_CFG_IOCTL),
		MSM_RPM_STATUS_ID_MAP(8960, SYSTEM_FABRIC_ARB),
		MSM_RPM_STATUS_ID_MAP(8960, MMSS_FABRIC_CFG_HALT),
		MSM_RPM_STATUS_ID_MAP(8960, MMSS_FABRIC_CFG_CLKMOD),
		MSM_RPM_STATUS_ID_MAP(8960, MMSS_FABRIC_CFG_IOCTL),
		MSM_RPM_STATUS_ID_MAP(8960, MM_FABRIC_ARB),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_S1_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_S1_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_S2_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_S2_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_S3_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_S3_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_S4_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_S4_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_S5_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_S5_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_S6_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_S6_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_S7_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_S7_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_S8_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_S8_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L1_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L1_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L2_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L2_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L3_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L3_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L4_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L4_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L5_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L5_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L6_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L6_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L7_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L7_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L8_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L8_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L9_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L9_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L10_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L10_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L11_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L11_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L12_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L12_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L13_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L13_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L14_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L14_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L15_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L15_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L16_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L16_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L17_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L17_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L18_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L18_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L19_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L19_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L20_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L20_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L21_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L21_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L22_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L22_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L23_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L23_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L24_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L24_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L25_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L25_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L26_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L26_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L27_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L27_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L28_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L28_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L29_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_L29_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_CLK1_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_CLK1_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_CLK2_0),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_CLK2_1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_LVS1),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_LVS2),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_LVS3),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_LVS4),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_LVS5),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_LVS6),
		MSM_RPM_STATUS_ID_MAP(8960, PM8921_LVS7),
		MSM_RPM_STATUS_ID_MAP(8960, NCP_0),
		MSM_RPM_STATUS_ID_MAP(8960, NCP_1),
		MSM_RPM_STATUS_ID_MAP(8960, CXO_BUFFERS),
		MSM_RPM_STATUS_ID_MAP(8960, USB_OTG_SWITCH),
		MSM_RPM_STATUS_ID_MAP(8960, HDMI_SWITCH),
		MSM_RPM_STATUS_ID_MAP(8960, DDR_DMM_0),
		MSM_RPM_STATUS_ID_MAP(8960, DDR_DMM_1),
		MSM_RPM_STATUS_ID_MAP(8960, EBI1_CH0_RANGE),
		MSM_RPM_STATUS_ID_MAP(8960, EBI1_CH1_RANGE),
	},
	.target_ctrl_id = {
		MSM_RPM_CTRL_MAP(8960, VERSION_MAJOR),
		MSM_RPM_CTRL_MAP(8960, VERSION_MINOR),
		MSM_RPM_CTRL_MAP(8960, VERSION_BUILD),
		MSM_RPM_CTRL_MAP(8960, REQ_CTX_0),
		MSM_RPM_CTRL_MAP(8960, REQ_SEL_0),
		MSM_RPM_CTRL_MAP(8960, ACK_CTX_0),
		MSM_RPM_CTRL_MAP(8960, ACK_SEL_0),
	},
	.sel_invalidate = MSM_RPM_8960_SEL_INVALIDATE,
	.sel_notification = MSM_RPM_8960_SEL_NOTIFICATION,
	.sel_last = MSM_RPM_8960_SEL_LAST,
	.ver = {3, 0, 0},
};

struct platform_device msm8960_rpm_device = {
	.name   = "msm_rpm",
	.id     = -1,
};

static struct msm_rpm_log_platform_data msm_rpm_log_pdata = {
	.phys_addr_base = 0x0010C000,
	.reg_offsets = {
		[MSM_RPM_LOG_PAGE_INDICES] = 0x00000080,
		[MSM_RPM_LOG_PAGE_BUFFER]  = 0x000000A0,
	},
	.phys_size = SZ_8K,
	.log_len = 4096,		  /* log's buffer length in bytes */
	.log_len_mask = (4096 >> 2) - 1,  /* length mask in units of u32 */
};

struct platform_device msm8960_rpm_log_device = {
	.name	= "msm_rpm_log",
	.id	= -1,
	.dev	= {
		.platform_data = &msm_rpm_log_pdata,
	},
};

static struct msm_rpmstats_platform_data msm_rpm_stat_pdata = {
	.phys_addr_base = 0x0010D204,
	.phys_size = SZ_8K,
};

struct platform_device msm8960_rpm_stat_device = {
	.name = "msm_rpm_stat",
	.id = -1,
	.dev = {
		.platform_data = &msm_rpm_stat_pdata,
	},
};

struct platform_device msm_bus_sys_fabric = {
	.name  = "msm_bus_fabric",
	.id    =  MSM_BUS_FAB_SYSTEM,
};
struct platform_device msm_bus_apps_fabric = {
	.name  = "msm_bus_fabric",
	.id    = MSM_BUS_FAB_APPSS,
};
struct platform_device msm_bus_mm_fabric = {
	.name  = "msm_bus_fabric",
	.id    = MSM_BUS_FAB_MMSS,
};
struct platform_device msm_bus_sys_fpb = {
	.name  = "msm_bus_fabric",
	.id    = MSM_BUS_FAB_SYSTEM_FPB,
};
struct platform_device msm_bus_cpss_fpb = {
	.name  = "msm_bus_fabric",
	.id    = MSM_BUS_FAB_CPSS_FPB,
};

/* Sensors DSPS platform data */
#ifdef CONFIG_MSM_DSPS

#define PPSS_DSPS_TCM_CODE_BASE 0x12000000
#define PPSS_DSPS_TCM_CODE_SIZE 0x28000
#define PPSS_DSPS_TCM_BUF_BASE  0x12040000
#define PPSS_DSPS_TCM_BUF_SIZE  0x4000
#define PPSS_DSPS_PIPE_BASE     0x12800000
#define PPSS_DSPS_PIPE_SIZE     0x4000
#define PPSS_DSPS_DDR_BASE      0x8fe00000
#define PPSS_DSPS_DDR_SIZE      0x100000
#define PPSS_SMEM_BASE          0x80000000
#define PPSS_SMEM_SIZE          0x200000
#define PPSS_REG_PHYS_BASE	0x12080000

static struct dsps_clk_info dsps_clks[] = {};
static struct dsps_regulator_info dsps_regs[] = {};

/*
 * Note: GPIOs field is	intialized in run-time at the function
 * msm8960_init_dsps().
 */

struct msm_dsps_platform_data msm_dsps_pdata = {
	.clks = dsps_clks,
	.clks_num = ARRAY_SIZE(dsps_clks),
	.gpios = NULL,
	.gpios_num = 0,
	.regs = dsps_regs,
	.regs_num = ARRAY_SIZE(dsps_regs),
	.dsps_pwr_ctl_en = 1,
	.tcm_code_start = PPSS_DSPS_TCM_CODE_BASE,
	.tcm_code_size = PPSS_DSPS_TCM_CODE_SIZE,
	.tcm_buf_start = PPSS_DSPS_TCM_BUF_BASE,
	.tcm_buf_size = PPSS_DSPS_TCM_BUF_SIZE,
	.pipe_start = PPSS_DSPS_PIPE_BASE,
	.pipe_size = PPSS_DSPS_PIPE_SIZE,
	.ddr_start = PPSS_DSPS_DDR_BASE,
	.ddr_size = PPSS_DSPS_DDR_SIZE,
	.smem_start = PPSS_SMEM_BASE,
	.smem_size  = PPSS_SMEM_SIZE,
	.signature = DSPS_SIGNATURE,
};

static struct resource msm_dsps_resources[] = {
	{
		.start = PPSS_REG_PHYS_BASE,
		.end   = PPSS_REG_PHYS_BASE + SZ_8K - 1,
		.name  = "ppss_reg",
		.flags = IORESOURCE_MEM,
	},
	{
		.start = PPSS_WDOG_TIMER_IRQ,
		.end   = PPSS_WDOG_TIMER_IRQ,
		.name  = "ppss_wdog",
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device msm_dsps_device = {
	.name          = "msm_dsps",
	.id            = 0,
	.num_resources = ARRAY_SIZE(msm_dsps_resources),
	.resource      = msm_dsps_resources,
	.dev.platform_data = &msm_dsps_pdata,
};

#endif /* CONFIG_MSM_DSPS */

#ifdef CONFIG_MSM_QDSS

#define MSM_QDSS_PHYS_BASE		0x01A00000
#define MSM_ETB_PHYS_BASE		(MSM_QDSS_PHYS_BASE + 0x1000)
#define MSM_TPIU_PHYS_BASE		(MSM_QDSS_PHYS_BASE + 0x3000)
#define MSM_FUNNEL_PHYS_BASE		(MSM_QDSS_PHYS_BASE + 0x4000)
#define MSM_ETM_PHYS_BASE		(MSM_QDSS_PHYS_BASE + 0x1C000)

#define QDSS_SOURCE(src_name, fpm) { .name = src_name, .fport_mask = fpm, }

static struct qdss_source msm_qdss_sources[] = {
	QDSS_SOURCE("msm_etm", 0x3),
};

static struct msm_qdss_platform_data qdss_pdata = {
	.src_table = msm_qdss_sources,
	.size = ARRAY_SIZE(msm_qdss_sources),
	.afamily = 1,
};

struct platform_device msm_qdss_device = {
	.name          = "msm_qdss",
	.id            = -1,
	.dev           = {
		.platform_data = &qdss_pdata,
	},
};

static struct resource msm_etb_resources[] = {
	{
		.start = MSM_ETB_PHYS_BASE,
		.end   = MSM_ETB_PHYS_BASE + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
};

struct platform_device msm_etb_device = {
	.name          = "msm_etb",
	.id            = 0,
	.num_resources = ARRAY_SIZE(msm_etb_resources),
	.resource      = msm_etb_resources,
};

static struct resource msm_tpiu_resources[] = {
	{
		.start = MSM_TPIU_PHYS_BASE,
		.end   = MSM_TPIU_PHYS_BASE + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
};

struct platform_device msm_tpiu_device = {
	.name          = "msm_tpiu",
	.id            = 0,
	.num_resources = ARRAY_SIZE(msm_tpiu_resources),
	.resource      = msm_tpiu_resources,
};

static struct resource msm_funnel_resources[] = {
	{
		.start = MSM_FUNNEL_PHYS_BASE,
		.end   = MSM_FUNNEL_PHYS_BASE + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
};

struct platform_device msm_funnel_device = {
	.name          = "msm_funnel",
	.id            = 0,
	.num_resources = ARRAY_SIZE(msm_funnel_resources),
	.resource      = msm_funnel_resources,
};

static struct resource msm_etm_resources[] = {
	{
		.start = MSM_ETM_PHYS_BASE,
		.end   = MSM_ETM_PHYS_BASE + (SZ_4K * 2) - 1,
		.flags = IORESOURCE_MEM,
	},
};

struct platform_device msm_etm_device = {
	.name          = "msm_etm",
	.id            = 0,
	.num_resources = ARRAY_SIZE(msm_etm_resources),
	.resource      = msm_etm_resources,
};

#endif

static struct resource msm_ebi1_ch0_erp_resources[] = {
	{
		.start = HSDDRX_EBI1CH0_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = 0x00A40000,
		.end   = 0x00A40000 + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
};

struct platform_device msm8960_device_ebi1_ch0_erp = {
	.name		= "msm_ebi_erp",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(msm_ebi1_ch0_erp_resources),
	.resource	= msm_ebi1_ch0_erp_resources,
};

static struct resource msm_ebi1_ch1_erp_resources[] = {
	{
		.start = HSDDRX_EBI1CH1_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = 0x00D40000,
		.end   = 0x00D40000 + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
};

struct platform_device msm8960_device_ebi1_ch1_erp = {
	.name		= "msm_ebi_erp",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(msm_ebi1_ch1_erp_resources),
	.resource	= msm_ebi1_ch1_erp_resources,
};

static int msm8960_LPM_latency = 1000; /* >100 usec for WFI */

struct platform_device msm8960_cpu_idle_device = {
	.name   = "msm_cpu_idle",
	.id     = -1,
	.dev = {
		.platform_data = &msm8960_LPM_latency,
	},
};

static struct msm_dcvs_freq_entry msm8960_freq[] = {
	{ 384000, 166981,  345600},
	{ 702000, 213049,  632502},
	{1026000, 285712,  925613},
	{1242000, 383945, 1176550},
	{1458000, 419729, 1465478},
	{1512000, 434116, 1546674},

};

static struct msm_dcvs_core_info msm8960_core_info = {
	.freq_tbl = &msm8960_freq[0],
	.core_param = {
		.max_time_us = 100000,
		.num_freq = ARRAY_SIZE(msm8960_freq),
	},
	.algo_param = {
		.slack_time_us = 58000,
		.scale_slack_time = 0,
		.scale_slack_time_pct = 0,
		.disable_pc_threshold = 1458000,
		.em_window_size = 100000,
		.em_max_util_pct = 97,
		.ss_window_size = 1000000,
		.ss_util_pct = 95,
		.ss_iobusy_conv = 100,
	},
};

struct platform_device msm8960_msm_gov_device = {
	.name = "msm_dcvs_gov",
	.id = -1,
	.dev = {
		.platform_data = &msm8960_core_info,
	},
};

static struct resource msm_cache_erp_resources[] = {
	{
		.name = "l1_irq",
		.start = SC_SICCPUXEXTFAULTIRPTREQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.name = "l2_irq",
		.start = APCC_QGICL2IRPTREQ,
		.flags = IORESOURCE_IRQ,
	}
};

struct platform_device msm8960_device_cache_erp = {
	.name		= "msm_cache_erp",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(msm_cache_erp_resources),
	.resource	= msm_cache_erp_resources,
};

struct msm_iommu_domain_name msm8960_iommu_ctx_names[] = {
	/* Camera */
	{
		.name = "vpe_src",
		.domain = CAMERA_DOMAIN,
	},
	/* Camera */
	{
		.name = "vpe_dst",
		.domain = CAMERA_DOMAIN,
	},
	/* Camera */
	{
		.name = "vfe_imgwr",
		.domain = CAMERA_DOMAIN,
	},
	/* Camera */
	{
		.name = "vfe_misc",
		.domain = CAMERA_DOMAIN,
	},
	/* Camera */
	{
		.name = "ijpeg_src",
		.domain = CAMERA_DOMAIN,
	},
	/* Camera */
	{
		.name = "ijpeg_dst",
		.domain = CAMERA_DOMAIN,
	},
	/* Camera */
	{
		.name = "jpegd_src",
		.domain = CAMERA_DOMAIN,
	},
	/* Camera */
	{
		.name = "jpegd_dst",
		.domain = CAMERA_DOMAIN,
	},
	/* Rotator */
	{
		.name = "rot_src",
		.domain = ROTATOR_SRC_DOMAIN,
	},
	/* Rotator */
	{
		.name = "rot_dst",
		.domain = ROTATOR_SRC_DOMAIN,
	},
	/* Video */
	{
		.name = "vcodec_a_mm1",
		.domain = VIDEO_DOMAIN,
	},
	/* Video */
	{
		.name = "vcodec_b_mm2",
		.domain = VIDEO_DOMAIN,
	},
	/* Video */
	{
		.name = "vcodec_a_stream",
		.domain = VIDEO_DOMAIN,
	},
};

static struct mem_pool msm8960_video_pools[] =  {
	/*
	 * Video hardware has the following requirements:
	 * 1. All video addresses used by the video hardware must be at a higher
	 *    address than video firmware address.
	 * 2. Video hardware can only access a range of 256MB from the base of
	 *    the video firmware.
	*/
	[VIDEO_FIRMWARE_POOL] =
	/* Low addresses, intended for video firmware */
		{
			.paddr	= SZ_128K,
			.size	= SZ_16M - SZ_128K,
		},
	[VIDEO_MAIN_POOL] =
	/* Main video pool */
		{
			.paddr	= SZ_16M,
			.size	= SZ_256M - SZ_16M,
		},
	[GEN_POOL] =
	/* Remaining address space up to 2G */
		{
			.paddr	= SZ_256M,
			.size	= SZ_2G - SZ_256M,
		},
};

static struct mem_pool msm8960_camera_pools[] =  {
	[GEN_POOL] =
	/* One address space for camera */
		{
			.paddr	= SZ_128K,
			.size	= SZ_2G - SZ_128K,
		},
};

static struct mem_pool msm8960_display_read_pools[] =  {
	[GEN_POOL] =
	/* One address space for display reads */
		{
			.paddr	= SZ_128K,
			.size	= SZ_2G - SZ_128K,
		},
};

static struct mem_pool msm8960_rotator_src_pools[] =  {
	[GEN_POOL] =
	/* One address space for rotator src */
		{
			.paddr	= SZ_128K,
			.size	= SZ_2G - SZ_128K,
		},
};

static struct msm_iommu_domain msm8960_iommu_domains[] = {
		[VIDEO_DOMAIN] = {
			.iova_pools = msm8960_video_pools,
			.npools = ARRAY_SIZE(msm8960_video_pools),
		},
		[CAMERA_DOMAIN] = {
			.iova_pools = msm8960_camera_pools,
			.npools = ARRAY_SIZE(msm8960_camera_pools),
		},
		[DISPLAY_READ_DOMAIN] = {
			.iova_pools = msm8960_display_read_pools,
			.npools = ARRAY_SIZE(msm8960_display_read_pools),
		},
		[ROTATOR_SRC_DOMAIN] = {
			.iova_pools = msm8960_rotator_src_pools,
			.npools = ARRAY_SIZE(msm8960_rotator_src_pools),
		},
};

struct iommu_domains_pdata msm8960_iommu_domain_pdata = {
	.domains = msm8960_iommu_domains,
	.ndomains = ARRAY_SIZE(msm8960_iommu_domains),
	.domain_names = msm8960_iommu_ctx_names,
	.nnames = ARRAY_SIZE(msm8960_iommu_ctx_names),
	.domain_alloc_flags = 0,
};

struct platform_device msm8960_iommu_domain_device = {
	.name = "iommu_domains",
	.id = -1,
	.dev = {
		.platform_data = &msm8960_iommu_domain_pdata,
	}
};

struct msm_rtb_platform_data msm8960_rtb_pdata = {
	.size = SZ_1M,
};

static int __init msm_rtb_set_buffer_size(char *p)
{
	int s;

	s = memparse(p, NULL);
	msm8960_rtb_pdata.size = ALIGN(s, SZ_4K);
	return 0;
}
early_param("msm_rtb_size", msm_rtb_set_buffer_size);


struct platform_device msm8960_rtb_device = {
	.name           = "msm_rtb",
	.id             = -1,
	.dev            = {
		.platform_data = &msm8960_rtb_pdata,
	},
};

#define MSM_8960_L1_SIZE  SZ_1M
/*
 * The actual L2 size is smaller but we need a larger buffer
 * size to store other dump information
 */
#define MSM_8960_L2_SIZE  SZ_4M

struct msm_cache_dump_platform_data msm8960_cache_dump_pdata = {
	.l2_size = MSM_8960_L2_SIZE,
	.l1_size = MSM_8960_L1_SIZE,
};

struct platform_device msm8960_cache_dump_device = {
	.name           = "msm_cache_dump",
	.id             = -1,
	.dev            = {
		.platform_data = &msm8960_cache_dump_pdata,
	},
};

#define MDM2AP_ERRFATAL			40
#define AP2MDM_ERRFATAL			80
#define MDM2AP_STATUS			24
#define AP2MDM_STATUS			77
#define AP2MDM_PMIC_PWR_EN		22
#define AP2MDM_KPDPWR_N			79
#define AP2MDM_SOFT_RESET		78

static struct resource sglte_resources[] = {
	{
		.start	= MDM2AP_ERRFATAL,
		.end	= MDM2AP_ERRFATAL,
		.name	= "MDM2AP_ERRFATAL",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= AP2MDM_ERRFATAL,
		.end	= AP2MDM_ERRFATAL,
		.name	= "AP2MDM_ERRFATAL",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= MDM2AP_STATUS,
		.end	= MDM2AP_STATUS,
		.name	= "MDM2AP_STATUS",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= AP2MDM_STATUS,
		.end	= AP2MDM_STATUS,
		.name	= "AP2MDM_STATUS",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= AP2MDM_PMIC_PWR_EN,
		.end	= AP2MDM_PMIC_PWR_EN,
		.name	= "AP2MDM_PMIC_PWR_EN",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= AP2MDM_KPDPWR_N,
		.end	= AP2MDM_KPDPWR_N,
		.name	= "AP2MDM_KPDPWR_N",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= AP2MDM_SOFT_RESET,
		.end	= AP2MDM_SOFT_RESET,
		.name	= "AP2MDM_SOFT_RESET",
		.flags	= IORESOURCE_IO,
	},
};

struct platform_device mdm_sglte_device = {
	.name		= "mdm2_modem",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(sglte_resources),
	.resource	= sglte_resources,
};
