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
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/msm_kgsl.h>
#include <linux/regulator/machine.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <mach/irqs.h>
#include <mach/msm_iomap.h>
#include <mach/board.h>
#include <mach/dma.h>
#include <mach/dal_axi.h>
#include <asm/mach/flash.h>
#include <asm/hardware/gic.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/mach/mmc.h>
#include <mach/rpc_hsusb.h>
#include <mach/socinfo.h>

#include "devices.h"
#include "devices-msm7x2xa.h"
#include "footswitch.h"
#include "acpuclock.h"

/* Address of GSBI blocks */
#define MSM_GSBI0_PHYS		0xA1200000
#define MSM_GSBI1_PHYS		0xA1300000

/* GSBI QUPe devices */
#define MSM_GSBI0_QUP_PHYS	(MSM_GSBI0_PHYS + 0x80000)
#define MSM_GSBI1_QUP_PHYS	(MSM_GSBI1_PHYS + 0x80000)

static struct resource gsbi0_qup_i2c_resources[] = {
	{
		.name	= "qup_phys_addr",
		.start	= MSM_GSBI0_QUP_PHYS,
		.end	= MSM_GSBI0_QUP_PHYS + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "gsbi_qup_i2c_addr",
		.start	= MSM_GSBI0_PHYS,
		.end	= MSM_GSBI0_PHYS + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "qup_err_intr",
		.start	= INT_PWB_I2C,
		.end	= INT_PWB_I2C,
		.flags	= IORESOURCE_IRQ,
	},
};

/* Use GSBI0 QUP for /dev/i2c-0 */
struct platform_device msm_gsbi0_qup_i2c_device = {
	.name		= "qup_i2c",
	.id		= MSM_GSBI0_QUP_I2C_BUS_ID,
	.num_resources	= ARRAY_SIZE(gsbi0_qup_i2c_resources),
	.resource	= gsbi0_qup_i2c_resources,
};

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
		.end	= MSM_GSBI1_PHYS + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "qup_err_intr",
		.start	= INT_ARM11_DMA,
		.end	= INT_ARM11_DMA,
		.flags	= IORESOURCE_IRQ,
	},
};

/* Use GSBI1 QUP for /dev/i2c-1 */
struct platform_device msm_gsbi1_qup_i2c_device = {
	.name		= "qup_i2c",
	.id		= MSM_GSBI1_QUP_I2C_BUS_ID,
	.num_resources	= ARRAY_SIZE(gsbi1_qup_i2c_resources),
	.resource	= gsbi1_qup_i2c_resources,
};

#define MSM_HSUSB_PHYS        0xA0800000
static struct resource resources_hsusb_otg[] = {
	{
		.start	= MSM_HSUSB_PHYS,
		.end	= MSM_HSUSB_PHYS + SZ_1K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_USB_HS,
		.end	= INT_USB_HS,
		.flags	= IORESOURCE_IRQ,
	},
};

static u64 dma_mask = 0xffffffffULL;
struct platform_device msm_device_otg = {
	.name		= "msm_otg",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_hsusb_otg),
	.resource	= resources_hsusb_otg,
	.dev		= {
		.dma_mask		= &dma_mask,
		.coherent_dma_mask	= 0xffffffffULL,
	},
};

static struct resource resources_gadget_peripheral[] = {
	{
		.start	= MSM_HSUSB_PHYS,
		.end	= MSM_HSUSB_PHYS + SZ_1K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_USB_HS,
		.end	= INT_USB_HS,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm_device_gadget_peripheral = {
	.name		= "msm_hsusb",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_gadget_peripheral),
	.resource	= resources_gadget_peripheral,
	.dev		= {
		.dma_mask		= &dma_mask,
		.coherent_dma_mask	= 0xffffffffULL,
	},
};

static struct resource resources_hsusb_host[] = {
	{
		.start	= MSM_HSUSB_PHYS,
		.end	= MSM_HSUSB_PHYS + SZ_1K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_USB_HS,
		.end	= INT_USB_HS,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm_device_hsusb_host = {
	.name		= "msm_hsusb_host",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(resources_hsusb_host),
	.resource	= resources_hsusb_host,
	.dev		= {
		.dma_mask		= &dma_mask,
		.coherent_dma_mask	= 0xffffffffULL,
	},
};

static struct platform_device *msm_host_devices[] = {
	&msm_device_hsusb_host,
};

static struct resource msm_dmov_resource[] = {
	{
		.start = INT_ADM_AARM,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = 0xA9700000,
		.end = 0xA9700000 + SZ_4K - 1,
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

struct platform_device msm_device_smd = {
	.name	= "msm_smd",
	.id	= -1,
};

static struct resource resources_uart1[] = {
	{
		.start	= INT_UART1,
		.end	= INT_UART1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= MSM7XXX_UART1_PHYS,
		.end	= MSM7XXX_UART1_PHYS + MSM7XXX_UART1_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device msm_device_uart1 = {
	.name	= "msm_serial",
	.id	= 0,
	.num_resources	= ARRAY_SIZE(resources_uart1),
	.resource	= resources_uart1,
};

#define MSM_UART1DM_PHYS      0xA0200000
static struct resource msm_uart1_dm_resources[] = {
	{
		.start	= MSM_UART1DM_PHYS,
		.end	= MSM_UART1DM_PHYS + PAGE_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_UART1DM_IRQ,
		.end	= INT_UART1DM_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= INT_UART1DM_RX,
		.end	= INT_UART1DM_RX,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= DMOV_HSUART1_TX_CHAN,
		.end	= DMOV_HSUART1_RX_CHAN,
		.name	= "uartdm_channels",
		.flags	= IORESOURCE_DMA,
	},
	{
		.start	= DMOV_HSUART1_TX_CRCI,
		.end	= DMOV_HSUART1_RX_CRCI,
		.name	= "uartdm_crci",
		.flags	= IORESOURCE_DMA,
	},
};

static u64 msm_uart_dm1_dma_mask = DMA_BIT_MASK(32);
struct platform_device msm_device_uart_dm1 = {
	.name	= "msm_serial_hs",
	.id	= 0,
	.num_resources	= ARRAY_SIZE(msm_uart1_dm_resources),
	.resource	= msm_uart1_dm_resources,
	.dev	= {
		.dma_mask		= &msm_uart_dm1_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};

#define MSM_UART2DM_PHYS	0xA0300000
static struct resource msm_uart2dm_resources[] = {
	{
		.start	= MSM_UART2DM_PHYS,
		.end	= MSM_UART2DM_PHYS + PAGE_SIZE - 1,
		.name	= "uartdm_resource",
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_UART2DM_IRQ,
		.end	= INT_UART2DM_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm_device_uart_dm2 = {
	.name	= "msm_serial_hsl",
	.id	= 0,
	.num_resources	= ARRAY_SIZE(msm_uart2dm_resources),
	.resource	= msm_uart2dm_resources,
};

#define MSM_NAND_PHYS		0xA0A00000
#define MSM_NANDC01_PHYS	0xA0A40000
#define MSM_NANDC10_PHYS	0xA0A80000
#define MSM_NANDC11_PHYS	0xA0AC0000
#define EBI2_REG_BASE		0xA0D00000
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
	[2] = {
		.name   = "msm_nandc01_phys",
		.start  = MSM_NANDC01_PHYS,
		.end    = MSM_NANDC01_PHYS + 0x7FF,
		.flags  = IORESOURCE_MEM,
	},
	[3] = {
		.name   = "msm_nandc10_phys",
		.start  = MSM_NANDC10_PHYS,
		.end    = MSM_NANDC10_PHYS + 0x7FF,
		.flags  = IORESOURCE_MEM,
	},
	[4] = {
		.name   = "msm_nandc11_phys",
		.start  = MSM_NANDC11_PHYS,
		.end    = MSM_NANDC11_PHYS + 0x7FF,
		.flags  = IORESOURCE_MEM,
	},
	[5] = {
		.name   = "ebi2_reg_base",
		.start  = EBI2_REG_BASE,
		.end    = EBI2_REG_BASE + 0x60,
		.flags  = IORESOURCE_MEM,
	},
};

struct flash_platform_data msm_nand_data;

struct platform_device msm_device_nand = {
	.name		= "msm_nand",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_nand),
	.resource	= resources_nand,
	.dev		= {
		.platform_data	= &msm_nand_data,
	},
};

#define MSM_SDC1_BASE         0xA0400000
#define MSM_SDC2_BASE         0xA0500000
#define MSM_SDC3_BASE         0xA0600000
#define MSM_SDC4_BASE         0xA0700000
static struct resource resources_sdc1[] = {
	{
		.start	= MSM_SDC1_BASE,
		.end	= MSM_SDC1_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_SDC1_0,
		.end	= INT_SDC1_1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "sdcc_dma_chnl",
		.start	= DMOV_SDC1_CHAN,
		.end	= DMOV_SDC1_CHAN,
		.flags	= IORESOURCE_DMA,
	},
	{
		.name	= "sdcc_dma_crci",
		.start	= DMOV_SDC1_CRCI,
		.end	= DMOV_SDC1_CRCI,
		.flags	= IORESOURCE_DMA,
	}
};

static struct resource resources_sdc2[] = {
	{
		.start	= MSM_SDC2_BASE,
		.end	= MSM_SDC2_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_SDC2_0,
		.end	= INT_SDC2_1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "sdcc_dma_chnl",
		.start	= DMOV_SDC2_CHAN,
		.end	= DMOV_SDC2_CHAN,
		.flags	= IORESOURCE_DMA,
	},
	{
		.name	= "sdcc_dma_crci",
		.start	= DMOV_SDC2_CRCI,
		.end	= DMOV_SDC2_CRCI,
		.flags	= IORESOURCE_DMA,
	}
};

static struct resource resources_sdc3[] = {
	{
		.start	= MSM_SDC3_BASE,
		.end	= MSM_SDC3_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_SDC3_0,
		.end	= INT_SDC3_1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "sdcc_dma_chnl",
		.start	= DMOV_SDC3_CHAN,
		.end	= DMOV_SDC3_CHAN,
		.flags	= IORESOURCE_DMA,
	},
	{
		.name	= "sdcc_dma_crci",
		.start	= DMOV_SDC3_CRCI,
		.end	= DMOV_SDC3_CRCI,
		.flags	= IORESOURCE_DMA,
	},
};

static struct resource resources_sdc4[] = {
	{
		.start	= MSM_SDC4_BASE,
		.end	= MSM_SDC4_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_SDC4_0,
		.end	= INT_SDC4_1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "sdcc_dma_chnl",
		.start	= DMOV_SDC4_CHAN,
		.end	= DMOV_SDC4_CHAN,
		.flags	= IORESOURCE_DMA,
	},
	{
		.name	= "sdcc_dma_crci",
		.start	= DMOV_SDC4_CRCI,
		.end	= DMOV_SDC4_CRCI,
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

static struct platform_device *msm_sdcc_devices[] __initdata = {
	&msm_device_sdc1,
	&msm_device_sdc2,
	&msm_device_sdc3,
	&msm_device_sdc4,
};

#ifdef CONFIG_MSM_CAMERA_V4L2
static struct resource msm_csic0_resources[] = {
	{
		.name   = "csic",
		.start  = 0xA0F00000,
		.end    = 0xA0F00000 + 0x00100000 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "csic",
		.start  = INT_CSI_IRQ_0,
		.end    = INT_CSI_IRQ_0,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct resource msm_csic1_resources[] = {
	{
		.name   = "csic",
		.start  = 0xA1000000,
		.end    = 0xA1000000 + 0x00100000 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "csic",
		.start  = INT_CSI_IRQ_1,
		.end    = INT_CSI_IRQ_1,
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device msm7x27a_device_csic0 = {
	.name           = "msm_csic",
	.id             = 0,
	.resource       = msm_csic0_resources,
	.num_resources  = ARRAY_SIZE(msm_csic0_resources),
};

struct platform_device msm7x27a_device_csic1 = {
	.name           = "msm_csic",
	.id             = 1,
	.resource       = msm_csic1_resources,
	.num_resources  = ARRAY_SIZE(msm_csic1_resources),
};

static struct resource msm_clkctl_resources[] = {
	{
		.name   = "clk_ctl",
		.start  = MSM7XXX_CLK_CTL_PHYS,
		.end    = MSM7XXX_CLK_CTL_PHYS + MSM7XXX_CLK_CTL_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
};
struct platform_device msm7x27a_device_clkctl = {
	.name           = "msm_clk_ctl",
	.id             = 0,
	.resource       = msm_clkctl_resources,
	.num_resources  = ARRAY_SIZE(msm_clkctl_resources),
};

struct platform_device msm7x27a_device_vfe = {
	.name           = "msm_vfe",
	.id             = 0,
};

#endif

#define MDP_BASE		0xAA200000
#define MIPI_DSI_HW_BASE	0xA1100000

static struct resource msm_mipi_dsi_resources[] = {
	{
		.name   = "mipi_dsi",
		.start  = MIPI_DSI_HW_BASE,
		.end    = MIPI_DSI_HW_BASE + 0x000F0000 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.start  = INT_DSI_IRQ,
		.end    = INT_DSI_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device msm_mipi_dsi_device = {
	.name   = "mipi_dsi",
	.id     = 1,
	.num_resources  = ARRAY_SIZE(msm_mipi_dsi_resources),
	.resource       = msm_mipi_dsi_resources,
};

static struct resource msm_mdp_resources[] = {
	{
		.name   = "mdp",
		.start  = MDP_BASE,
		.end    = MDP_BASE + 0x000F1008 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.start  = INT_MDP,
		.end    = INT_MDP,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device msm_mdp_device = {
	.name   = "mdp",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(msm_mdp_resources),
	.resource       = msm_mdp_resources,
};

static struct platform_device msm_lcdc_device = {
	.name   = "lcdc",
	.id     = 0,
};

static struct resource kgsl_3d0_resources[] = {
	{
		.name  = KGSL_3D0_REG_MEMORY,
		.start = 0xA0000000,
		.end = 0xA001ffff,
		.flags = IORESOURCE_MEM,
	},
	{
		.name = KGSL_3D0_IRQ,
		.start = INT_GRAPHICS,
		.end = INT_GRAPHICS,
		.flags = IORESOURCE_IRQ,
	},
};

static struct kgsl_device_platform_data kgsl_3d0_pdata = {
	.pwrlevel = {
		{
			.gpu_freq = 245760000,
			.bus_freq = 200000000,
		},
		{
			.gpu_freq = 192000000,
			.bus_freq = 160000000,
		},
		{
			.gpu_freq = 133330000,
			.bus_freq = 0,
		},
	},
	.init_level = 0,
	.num_levels = 3,
	.set_grp_async = set_grp_xbar_async,
	.idle_timeout = HZ/5,
	.nap_allowed = false,
	.clk_map = KGSL_CLK_CORE | KGSL_CLK_IFACE | KGSL_CLK_MEM,
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

void __init msm7x25a_kgsl_3d0_init(void)
{
	if (cpu_is_msm7x25a() || cpu_is_msm7x25aa()) {
		kgsl_3d0_pdata.num_levels = 2;
		kgsl_3d0_pdata.pwrlevel[0].gpu_freq = 133330000;
		kgsl_3d0_pdata.pwrlevel[0].bus_freq = 160000000;
		kgsl_3d0_pdata.pwrlevel[1].gpu_freq = 96000000;
		kgsl_3d0_pdata.pwrlevel[1].bus_freq = 0;
	}
}

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

void __init msm_fb_register_device(char *name, void *data)
{
	if (!strncmp(name, "mdp", 3))
		msm_register_device(&msm_mdp_device, data);
	else if (!strncmp(name, "mipi_dsi", 8))
		msm_register_device(&msm_mipi_dsi_device, data);
	else if (!strncmp(name, "lcdc", 4))
		msm_register_device(&msm_lcdc_device, data);
	else
		printk(KERN_ERR "%s: unknown device! %s\n", __func__, name);
}

#define PERPH_WEB_BLOCK_ADDR (0xA9D00040)
#define PDM0_CTL_OFFSET (0x04)
#define SIZE_8B (0x08)

static struct resource resources_led[] = {
	{
		.start	= PERPH_WEB_BLOCK_ADDR,
		.end	= PERPH_WEB_BLOCK_ADDR + (SIZE_8B) - 1,
		.name	= "led-gpio-pdm",
		.flags	= IORESOURCE_MEM,
	},
};

static struct led_info msm_kpbl_pdm_led_pdata = {
	.name = "keyboard-backlight",
};

struct platform_device led_pdev = {
	.name	= "leds-msm-pdm",
	/* use pdev id to represent pdm id */
	.id	= 0,
	.num_resources	= ARRAY_SIZE(resources_led),
	.resource	= resources_led,
	.dev	= {
		.platform_data	= &msm_kpbl_pdm_led_pdata,
	},
};

struct platform_device asoc_msm_pcm = {
	.name   = "msm-dsp-audio",
	.id     = 0,
};

struct platform_device asoc_msm_dai0 = {
	.name   = "msm-codec-dai",
	.id     = 0,
};

struct platform_device asoc_msm_dai1 = {
	.name   = "msm-cpu-dai",
	.id     = 0,
};

static struct resource gpio_resources[] = {
	{
		.start	= INT_GPIO_GROUP1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= INT_GPIO_GROUP2,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device msm_device_gpio = {
	.name		= "msmgpio",
	.id		= -1,
	.resource	= gpio_resources,
	.num_resources	= ARRAY_SIZE(gpio_resources),
};

struct platform_device *msm_footswitch_devices[] = {
	FS_PCOM(FS_GFX3D,  "fs_gfx3d"),
};
unsigned msm_num_footswitch_devices = ARRAY_SIZE(msm_footswitch_devices);

/* MSM8625 Devices */

static struct resource msm8625_resources_uart1[] = {
	{
		.start  = MSM8625_INT_UART1,
		.end    = MSM8625_INT_UART1,
		.flags  = IORESOURCE_IRQ,
	},
	{
		.start	= MSM7XXX_UART1_PHYS,
		.end    = MSM7XXX_UART1_PHYS + MSM7XXX_UART1_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
};

struct platform_device msm8625_device_uart1 = {
	.name		= "msm_serial",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(msm8625_resources_uart1),
	.resource	= msm8625_resources_uart1,
};

static struct resource msm8625_uart1_dm_resources[] = {
	{
		.start	= MSM_UART1DM_PHYS,
		.end	= MSM_UART1DM_PHYS + PAGE_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= MSM8625_INT_UART1DM_IRQ,
		.end	= MSM8625_INT_UART1DM_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= MSM8625_INT_UART1DM_RX,
		.end	= MSM8625_INT_UART1DM_RX,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= DMOV_HSUART1_TX_CHAN,
		.end	= DMOV_HSUART1_RX_CHAN,
		.name	= "uartdm_channels",
		.flags	= IORESOURCE_DMA,
	},
	{
		.start	= DMOV_HSUART1_TX_CRCI,
		.end	= DMOV_HSUART1_RX_CRCI,
		.name	= "uartdm_crci",
		.flags	= IORESOURCE_DMA,
	},
};

struct platform_device msm8625_device_uart_dm1 = {
	.name	= "msm_serial_hs",
	.id	= 0,
	.num_resources	= ARRAY_SIZE(msm8625_uart1_dm_resources),
	.resource	= msm8625_uart1_dm_resources,
	.dev	= {
		.dma_mask		= &msm_uart_dm1_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};

static struct resource msm8625_uart2dm_resources[] = {
	{
		.start	= MSM_UART2DM_PHYS,
		.end	= MSM_UART2DM_PHYS + PAGE_SIZE - 1,
		.name	= "uartdm_resource",
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= MSM8625_INT_UART2DM_IRQ,
		.end	= MSM8625_INT_UART2DM_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm8625_device_uart_dm2 = {
	.name	= "msm_serial_hsl",
	.id	= 0,
	.num_resources	= ARRAY_SIZE(msm8625_uart2dm_resources),
	.resource	= msm8625_uart2dm_resources,
};

static struct resource msm8625_dmov_resource[] = {
	{
		.start	= MSM8625_INT_ADM_AARM,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= 0xA9700000,
		.end	= 0xA9700000 + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device msm8625_device_dmov = {
	.name		= "msm_dmov",
	.id		= -1,
	.resource	= msm8625_dmov_resource,
	.num_resources	= ARRAY_SIZE(msm8625_dmov_resource),
	.dev		= {
		.platform_data = &msm_dmov_pdata,
	},
};

static struct resource gsbi0_msm8625_qup_resources[] = {
	{
		.name	= "qup_phys_addr",
		.start	= MSM_GSBI0_QUP_PHYS,
		.end	= MSM_GSBI0_QUP_PHYS + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "gsbi_qup_i2c_addr",
		.start	= MSM_GSBI0_PHYS,
		.end	= MSM_GSBI0_PHYS + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "qup_err_intr",
		.start	= MSM8625_INT_PWB_I2C,
		.end	= MSM8625_INT_PWB_I2C,
		.flags	= IORESOURCE_IRQ,
	},
};

/* Use GSBI0 QUP for /dev/i2c-0 */
struct platform_device msm8625_device_qup_i2c_gsbi0 = {
	.name		= "qup_i2c",
	.id		= MSM_GSBI0_QUP_I2C_BUS_ID,
	.num_resources	= ARRAY_SIZE(gsbi0_msm8625_qup_resources),
	.resource	= gsbi0_msm8625_qup_resources,
};

static struct resource gsbi1_msm8625_qup_i2c_resources[] = {
	{
		.name	= "qup_phys_addr",
		.start	= MSM_GSBI1_QUP_PHYS,
		.end	= MSM_GSBI1_QUP_PHYS + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "gsbi_qup_i2c_addr",
		.start	= MSM_GSBI1_PHYS,
		.end	= MSM_GSBI1_PHYS + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "qup_err_intr",
		.start	= MSM8625_INT_ARM11_DMA,
		.end	= MSM8625_INT_ARM11_DMA,
		.flags	= IORESOURCE_IRQ,
	},
};

/* Use GSBI1 QUP for /dev/i2c-1 */
struct platform_device msm8625_device_qup_i2c_gsbi1 = {
	.name		= "qup_i2c",
	.id		= MSM_GSBI1_QUP_I2C_BUS_ID,
	.num_resources	= ARRAY_SIZE(gsbi1_qup_i2c_resources),
	.resource	= gsbi1_msm8625_qup_i2c_resources,
};

static struct resource msm8625_gpio_resources[] = {
	{
		.start	= MSM8625_INT_GPIO_GROUP1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= MSM8625_INT_GPIO_GROUP2,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device msm8625_device_gpio = {
	.name		= "msmgpio",
	.id		= -1,
	.resource	= msm8625_gpio_resources,
	.num_resources	= ARRAY_SIZE(msm8625_gpio_resources),
};

static struct resource msm8625_resources_sdc1[] = {
	{
		.start	= MSM_SDC1_BASE,
		.end	= MSM_SDC1_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= MSM8625_INT_SDC1_0,
		.end	= MSM8625_INT_SDC1_1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "sdcc_dma_chnl",
		.start	= DMOV_SDC1_CHAN,
		.end	= DMOV_SDC1_CHAN,
		.flags	= IORESOURCE_DMA,
	},
	{
		.name	= "sdcc_dma_crci",
		.start	= DMOV_SDC1_CRCI,
		.end	= DMOV_SDC1_CRCI,
		.flags	= IORESOURCE_DMA,
	}
};

static struct resource msm8625_resources_sdc2[] = {
	{
		.start	= MSM_SDC2_BASE,
		.end	= MSM_SDC2_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= MSM8625_INT_SDC2_0,
		.end	= MSM8625_INT_SDC2_1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "sdcc_dma_chnl",
		.start	= DMOV_SDC2_CHAN,
		.end	= DMOV_SDC2_CHAN,
		.flags	= IORESOURCE_DMA,
	},
	{
		.name	= "sdcc_dma_crci",
		.start	= DMOV_SDC2_CRCI,
		.end	= DMOV_SDC2_CRCI,
		.flags	= IORESOURCE_DMA,
	}
};

static struct resource msm8625_resources_sdc3[] = {
	{
		.start	= MSM_SDC3_BASE,
		.end	= MSM_SDC3_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= MSM8625_INT_SDC3_0,
		.end	= MSM8625_INT_SDC3_1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "sdcc_dma_chnl",
		.start	= DMOV_SDC3_CHAN,
		.end	= DMOV_SDC3_CHAN,
		.flags	= IORESOURCE_DMA,
	},
	{
		.name	= "sdcc_dma_crci",
		.start	= DMOV_SDC3_CRCI,
		.end	= DMOV_SDC3_CRCI,
		.flags	= IORESOURCE_DMA,
	},
};

static struct resource msm8625_resources_sdc4[] = {
	{
		.start	= MSM_SDC4_BASE,
		.end	= MSM_SDC4_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= MSM8625_INT_SDC4_0,
		.end	= MSM8625_INT_SDC4_1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "sdcc_dma_chnl",
		.start	= DMOV_SDC4_CHAN,
		.end	= DMOV_SDC4_CHAN,
		.flags	= IORESOURCE_DMA,
	},
	{
		.name	= "sdcc_dma_crci",
		.start	= DMOV_SDC4_CRCI,
		.end	= DMOV_SDC4_CRCI,
		.flags	= IORESOURCE_DMA,
	},
};

struct platform_device msm8625_device_sdc1 = {
	.name		= "msm_sdcc",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(msm8625_resources_sdc1),
	.resource	= msm8625_resources_sdc1,
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};

struct platform_device msm8625_device_sdc2 = {
	.name		= "msm_sdcc",
	.id		= 2,
	.num_resources	= ARRAY_SIZE(msm8625_resources_sdc2),
	.resource	= msm8625_resources_sdc2,
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};

struct platform_device msm8625_device_sdc3 = {
	.name		= "msm_sdcc",
	.id		= 3,
	.num_resources	= ARRAY_SIZE(msm8625_resources_sdc3),
	.resource	= msm8625_resources_sdc3,
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};

struct platform_device msm8625_device_sdc4 = {
	.name		= "msm_sdcc",
	.id		= 4,
	.num_resources	= ARRAY_SIZE(msm8625_resources_sdc4),
	.resource	= msm8625_resources_sdc4,
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};

static struct platform_device *msm8625_sdcc_devices[] __initdata = {
	&msm8625_device_sdc1,
	&msm8625_device_sdc2,
	&msm8625_device_sdc3,
	&msm8625_device_sdc4,
};

int __init msm_add_sdcc(unsigned int controller, struct mmc_platform_data *plat)
{
	struct platform_device	*pdev;

	if (controller < 1 || controller > 4)
		return -EINVAL;

	if (cpu_is_msm8625())
		pdev = msm8625_sdcc_devices[controller-1];
	else
		pdev = msm_sdcc_devices[controller-1];

	pdev->dev.platform_data = plat;
	return platform_device_register(pdev);
}

static struct resource msm8625_resources_hsusb_otg[] = {
	{
		.start	= MSM_HSUSB_PHYS,
		.end	= MSM_HSUSB_PHYS + SZ_1K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= MSM8625_INT_USB_HS,
		.end	= MSM8625_INT_USB_HS,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm8625_device_otg = {
	.name		= "msm_otg",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(msm8625_resources_hsusb_otg),
	.resource	= msm8625_resources_hsusb_otg,
	.dev		= {
		.dma_mask		= &dma_mask,
		.coherent_dma_mask	= 0xffffffffULL,
	},
};

static struct resource msm8625_resources_gadget_peripheral[] = {
	{
		.start	= MSM_HSUSB_PHYS,
		.end	= MSM_HSUSB_PHYS + SZ_1K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= MSM8625_INT_USB_HS,
		.end	= MSM8625_INT_USB_HS,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm8625_device_gadget_peripheral = {
	.name		= "msm_hsusb",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(msm8625_resources_gadget_peripheral),
	.resource	= msm8625_resources_gadget_peripheral,
	.dev		= {
		.dma_mask		= &dma_mask,
		.coherent_dma_mask	= 0xffffffffULL,
	},
};

static struct resource msm8625_resources_hsusb_host[] = {
	{
		.start	= MSM_HSUSB_PHYS,
		.end	= MSM_HSUSB_PHYS + SZ_1K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= MSM8625_INT_USB_HS,
		.end	= MSM8625_INT_USB_HS,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm8625_device_hsusb_host = {
	.name		= "msm_hsusb_host",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(msm8625_resources_hsusb_host),
	.resource	= msm8625_resources_hsusb_host,
	.dev		= {
		.dma_mask		= &dma_mask,
		.coherent_dma_mask	= 0xffffffffULL,
	},
};

static struct platform_device *msm8625_host_devices[] = {
	&msm8625_device_hsusb_host,
};

int msm_add_host(unsigned int host, struct msm_usb_host_platform_data *plat)
{
	struct platform_device	*pdev;

	if (cpu_is_msm8625())
		pdev = msm8625_host_devices[host];
	else
		pdev = msm_host_devices[host];
	if (!pdev)
		return -ENODEV;
	pdev->dev.platform_data = plat;
	return platform_device_register(pdev);
}

#ifdef CONFIG_MSM_CAMERA_V4L2
static struct resource msm8625_csic0_resources[] = {
	{
		.name   = "csic",
		.start  = 0xA0F00000,
		.end    = 0xA0F00000 + 0x00100000 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "csic",
		.start  = MSM8625_INT_CSI_IRQ_0,
		.end    = MSM8625_INT_CSI_IRQ_0,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct resource msm8625_csic1_resources[] = {
	{
		.name   = "csic",
		.start  = 0xA1000000,
		.end    = 0xA1000000 + 0x00100000 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "csic",
		.start  = MSM8625_INT_CSI_IRQ_1,
		.end    = MSM8625_INT_CSI_IRQ_1,
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device msm8625_device_csic0 = {
	.name           = "msm_csic",
	.id             = 0,
	.resource       = msm8625_csic0_resources,
	.num_resources  = ARRAY_SIZE(msm8625_csic0_resources),
};

struct platform_device msm8625_device_csic1 = {
	.name           = "msm_csic",
	.id             = 1,
	.resource       = msm8625_csic1_resources,
	.num_resources  = ARRAY_SIZE(msm8625_csic1_resources),
};
#endif

static struct clk_lookup msm_clock_8625_dummy[] = {
	CLK_DUMMY("core_clk",		adm_clk.c,	"msm_dmov", 0),
	CLK_DUMMY("adsp_clk",		adsp_clk.c,	NULL, 0),
	CLK_DUMMY("ahb_m_clk",		ahb_m_clk.c,	NULL, 0),
	CLK_DUMMY("ahb_s_clk",		ahb_s_clk.c,	NULL, 0),
	CLK_DUMMY("cam_m_clk",		cam_m_clk.c,	NULL, 0),
	CLK_DUMMY("csi_clk",		csi1_clk.c,	NULL, 0),
	CLK_DUMMY("csi_pclk",		csi1_p_clk.c,	NULL, 0),
	CLK_DUMMY("csi_vfe_clk",	csi1_vfe_clk.c,	NULL, 0),
	CLK_DUMMY("dsi_byte_clk",	dsi_byte_clk.c,	NULL, 0),
	CLK_DUMMY("dsi_clk",		dsi_clk.c,	NULL, 0),
	CLK_DUMMY("dsi_esc_clk",	dsi_esc_clk.c,	NULL, 0),
	CLK_DUMMY("dsi_pixel_clk",	dsi_pixel_clk.c, NULL, 0),
	CLK_DUMMY("dsi_ref_clk",	dsi_ref_clk.c,	NULL, 0),
	CLK_DUMMY("ebi1_clk",		ebi1_clk.c,	NULL, 0),
	CLK_DUMMY("ebi2_clk",		ebi2_clk.c,	NULL, 0),
	CLK_DUMMY("ecodec_clk",		ecodec_clk.c,	NULL, 0),
	CLK_DUMMY("gp_clk",		gp_clk.c,	NULL, 0),
	CLK_DUMMY("core_clk",		gsbi1_qup_clk.c, "qup_i2c.0", 0),
	CLK_DUMMY("core_clk",		gsbi2_qup_clk.c, "qup_i2c.1", 0),
	CLK_DUMMY("iface_clk",		gsbi1_qup_p_clk.c, "qup_i2c.0", 0),
	CLK_DUMMY("iface_clk",		gsbi2_qup_p_clk.c, "qup_i2c.1", 0),
	CLK_DUMMY("icodec_rx_clk",	icodec_rx_clk.c, NULL, 0),
	CLK_DUMMY("icodec_tx_clk",	icodec_tx_clk.c, NULL, 0),
	CLK_DUMMY("mem_clk",		imem_clk.c,	NULL, 0),
	CLK_DUMMY("mddi_clk",		pmdh_clk.c,	NULL, 0),
	CLK_DUMMY("mdp_clk",		mdp_clk.c,	NULL, 0),
	CLK_DUMMY("mdp_lcdc_pclk_clk",	mdp_lcdc_pclk_clk.c, NULL, 0),
	CLK_DUMMY("mdp_lcdc_pad_pclk_clk", mdp_lcdc_pad_pclk_clk.c, NULL, 0),
	CLK_DUMMY("mdp_vsync_clk",	mdp_vsync_clk.c,	NULL, 0),
	CLK_DUMMY("mdp_dsi_pclk",	mdp_dsi_p_clk.c,	NULL, 0),
	CLK_DUMMY("pbus_clk",		pbus_clk.c,	NULL, 0),
	CLK_DUMMY("pcm_clk",		pcm_clk.c,	NULL, 0),
	CLK_DUMMY("sdac_clk",		sdac_clk.c,	NULL, 0),
	CLK_DUMMY("core_clk",		sdc1_clk.c,	"msm_sdcc.1", 0),
	CLK_DUMMY("iface_clk",		sdc1_p_clk.c,	"msm_sdcc.1", 0),
	CLK_DUMMY("core_clk",		sdc2_clk.c,	"msm_sdcc.2", 0),
	CLK_DUMMY("iface_clk",		sdc2_p_clk.c,	"msm_sdcc.2", 0),
	CLK_DUMMY("core_clk",		sdc3_clk.c,	"msm_sdcc.3", 0),
	CLK_DUMMY("iface_clk",		sdc3_p_clk.c,	"msm_sdcc.3", 0),
	CLK_DUMMY("core_clk",		sdc4_clk.c,	"msm_sdcc.4", 0),
	CLK_DUMMY("iface_clk",		sdc4_p_clk.c,	"msm_sdcc.4", 0),
	CLK_DUMMY("ref_clk",		tsif_ref_clk.c,	"msm_tsif.0", 0),
	CLK_DUMMY("iface_clk",		tsif_p_clk.c,	"msm_tsif.0", 0),
	CLK_DUMMY("core_clk",		uart1_clk.c,	"msm_serial.0", 0),
	CLK_DUMMY("core_clk",		uart2_clk.c,	"msm_serial.1", 0),
	CLK_DUMMY("core_clk",		uart1dm_clk.c,	"msm_serial_hs.0", 0),
	CLK_DUMMY("core_clk",		uart2dm_clk.c,	"msm_serial_hsl.0", 0),
	CLK_DUMMY("usb_hs_core_clk",	usb_hs_core_clk.c, NULL, 0),
	CLK_DUMMY("usb_hs2_clk",	usb_hs2_clk.c,	NULL, 0),
	CLK_DUMMY("usb_hs_clk",		usb_hs_clk.c,	NULL, 0),
	CLK_DUMMY("usb_hs_pclk",	usb_hs_p_clk.c,	NULL, 0),
	CLK_DUMMY("usb_phy_clk",	usb_phy_clk.c,	NULL, 0),
	CLK_DUMMY("vdc_clk",		vdc_clk.c,	NULL, 0),
	CLK_DUMMY("ebi1_acpu_clk",	ebi_acpu_clk.c,	NULL, 0),
	CLK_DUMMY("ebi1_lcdc_clk",	ebi_lcdc_clk.c,	NULL, 0),
	CLK_DUMMY("ebi1_mddi_clk",	ebi_mddi_clk.c,	NULL, 0),
	CLK_DUMMY("ebi1_usb_clk",	ebi_usb_clk.c,	NULL, 0),
	CLK_DUMMY("ebi1_vfe_clk",	ebi_vfe_clk.c,	NULL, 0),
	CLK_DUMMY("mem_clk",		ebi_adm_clk.c,	"msm_dmov", 0),
};

struct clock_init_data msm8625_dummy_clock_init_data __initdata = {
	.table = msm_clock_8625_dummy,
	.size = ARRAY_SIZE(msm_clock_8625_dummy),
};

int __init msm7x2x_misc_init(void)
{
	if (machine_is_msm8625_rumi3()) {
		msm_clock_init(&msm8625_dummy_clock_init_data);
		return 0;
	}

	msm_clock_init(&msm7x27a_clock_init_data);
	if (cpu_is_msm7x27aa())
		acpuclk_init(&acpuclk_7x27aa_soc_data);
	else
		acpuclk_init(&acpuclk_7x27a_soc_data);


	return 0;
}

#ifdef CONFIG_CACHE_L2X0
static int __init msm7x27x_cache_init(void)
{
	int aux_ctrl = 0;

	/* Way Size 010(0x2) 32KB */
	aux_ctrl = (0x1 << L2X0_AUX_CTRL_SHARE_OVERRIDE_SHIFT) | \
		   (0x2 << L2X0_AUX_CTRL_WAY_SIZE_SHIFT) | \
		   (0x1 << L2X0_AUX_CTRL_EVNT_MON_BUS_EN_SHIFT);

	if (cpu_is_msm8625()) {
		/* Way Size 011(0x3) 64KB */
		aux_ctrl |= (0x3 << L2X0_AUX_CTRL_WAY_SIZE_SHIFT) | \
			    (0x1 << L2X0_AUX_CTRL_DATA_PREFETCH_SHIFT) | \
			    (0x1 << L2X0_AUX_CTRL_INSTR_PREFETCH_SHIFT);
	}

	l2x0_init(MSM_L2CC_BASE, aux_ctrl, L2X0_AUX_CTRL_MASK);

	return 0;
}
#else
static int __init msm7x27x_cache_init(void){ return 0; }
#endif

void __init msm_common_io_init(void)
{
	msm_map_common_io();
	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed!\n", __func__);
	msm7x27x_cache_init();
}

void __init msm8625_init_irq(void)
{
	gic_init(0, GIC_PPI_START, MSM_QGIC_DIST_BASE,
			(void *)MSM_QGIC_CPU_BASE);
}

void __init msm8625_map_io(void)
{
	msm_map_msm8625_io();

	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed!\n", __func__);
	msm7x27x_cache_init();
}

static int msm7627a_init_gpio(void)
{
	if (cpu_is_msm8625())
		platform_device_register(&msm8625_device_gpio);
	else
		platform_device_register(&msm_device_gpio);
	return 0;
}
postcore_initcall(msm7627a_init_gpio);

