/*
 * Copyright (C) 2008 Google, Inc.
 * Copyright (c) 2008-2011, Code Aurora Forum. All rights reserved.
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
#include <linux/platform_device.h>

#include <linux/dma-mapping.h>
#include <asm/clkdev.h>
#include <mach/irqs.h>
#include <mach/msm_iomap.h>
#include <mach/dma.h>
#include <mach/board.h>

#include "devices.h"
#include "smd_private.h"

#include <asm/mach/flash.h>

#include <asm/mach/mmc.h>
#include <mach/msm_hsusb.h>
#include <mach/usbdiag.h>
#include <mach/usb_gadget_fserial.h>
#include <mach/rpc_hsusb.h>

#include "clock-pcom.h"

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

struct platform_device msm_device_uart1 = {
	.name	= "msm_serial",
	.id	= 0,
	.num_resources	= ARRAY_SIZE(resources_uart1),
	.resource	= resources_uart1,
};

struct platform_device msm_device_uart2 = {
	.name	= "msm_serial",
	.id	= 1,
	.num_resources	= ARRAY_SIZE(resources_uart2),
	.resource	= resources_uart2,
};

struct platform_device msm_device_uart3 = {
	.name	= "msm_serial",
	.id	= 2,
	.num_resources	= ARRAY_SIZE(resources_uart3),
	.resource	= resources_uart3,
};

#define MSM_UART1DM_PHYS      0xA0200000
#define MSM_UART2DM_PHYS      0xA0300000
static struct resource msm_uart1_dm_resources[] = {
	{
		.start = MSM_UART1DM_PHYS,
		.end   = MSM_UART1DM_PHYS + PAGE_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = INT_UART1DM_IRQ,
		.end   = INT_UART1DM_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = INT_UART1DM_RX,
		.end   = INT_UART1DM_RX,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = DMOV_HSUART1_TX_CHAN,
		.end   = DMOV_HSUART1_RX_CHAN,
		.name  = "uartdm_channels",
		.flags = IORESOURCE_DMA,
	},
	{
		.start = DMOV_HSUART1_TX_CRCI,
		.end   = DMOV_HSUART1_RX_CRCI,
		.name  = "uartdm_crci",
		.flags = IORESOURCE_DMA,
	},
};

static u64 msm_uart_dm1_dma_mask = DMA_BIT_MASK(32);

struct platform_device msm_device_uart_dm1 = {
	.name = "msm_serial_hs",
	.id = 0,
	.num_resources = ARRAY_SIZE(msm_uart1_dm_resources),
	.resource = msm_uart1_dm_resources,
	.dev		= {
		.dma_mask = &msm_uart_dm1_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
};

static struct resource msm_uart2_dm_resources[] = {
	{
		.start = MSM_UART2DM_PHYS,
		.end   = MSM_UART2DM_PHYS + PAGE_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = INT_UART2DM_IRQ,
		.end   = INT_UART2DM_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = INT_UART2DM_RX,
		.end   = INT_UART2DM_RX,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = DMOV_HSUART2_TX_CHAN,
		.end   = DMOV_HSUART2_RX_CHAN,
		.name  = "uartdm_channels",
		.flags = IORESOURCE_DMA,
	},
	{
		.start = DMOV_HSUART2_TX_CRCI,
		.end   = DMOV_HSUART2_RX_CRCI,
		.name  = "uartdm_crci",
		.flags = IORESOURCE_DMA,
	},
};

static u64 msm_uart_dm2_dma_mask = DMA_BIT_MASK(32);

struct platform_device msm_device_uart_dm2 = {
	.name = "msm_serial_hs",
	.id = 1,
	.num_resources = ARRAY_SIZE(msm_uart2_dm_resources),
	.resource = msm_uart2_dm_resources,
	.dev		= {
		.dma_mask = &msm_uart_dm2_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
};

#define MSM_I2C_SIZE          SZ_4K
#define MSM_I2C_PHYS          0xA9900000
static struct resource resources_i2c[] = {
	{
		.start	= MSM_I2C_PHYS,
		.end	= MSM_I2C_PHYS + MSM_I2C_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_PWB_I2C,
		.end	= INT_PWB_I2C,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm_device_i2c = {
	.name		= "msm_i2c",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(resources_i2c),
	.resource	= resources_i2c,
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
struct platform_device msm_device_hsusb_otg = {
	.name		= "msm_hsusb_otg",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_hsusb_otg),
	.resource	= resources_hsusb_otg,
	.dev		= {
		.dma_mask 		= &dma_mask,
		.coherent_dma_mask	= 0xffffffffULL,
	},
};

static struct resource resources_hsusb_peripheral[] = {
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

struct platform_device msm_device_hsusb_peripheral = {
	.name		= "msm_hsusb_peripheral",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_hsusb_peripheral),
	.resource	= resources_hsusb_peripheral,
	.dev		= {
		.dma_mask 		= &dma_mask,
		.coherent_dma_mask	= 0xffffffffULL,
	},
};

struct platform_device msm_device_gadget_peripheral = {
	.name		= "msm_hsusb",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_gadget_peripheral),
	.resource	= resources_gadget_peripheral,
	.dev		= {
		.dma_mask 		= &dma_mask,
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
		.dma_mask 		= &dma_mask,
		.coherent_dma_mask	= 0xffffffffULL,
	},
};

static struct platform_device *msm_host_devices[] = {
	&msm_device_hsusb_host,
};

int msm_add_host(unsigned int host, struct msm_usb_host_platform_data *plat)
{
	struct platform_device	*pdev;

	pdev = msm_host_devices[host];
	if (!pdev)
		return -ENODEV;
	pdev->dev.platform_data = plat;
	return platform_device_register(pdev);
}

#ifdef CONFIG_USB_ANDROID_DIAG
struct usb_diag_platform_data usb_diag_pdata = {
	.ch_name = DIAG_LEGACY,
	.update_pid_and_serial_num = usb_diag_update_pid_and_serial_num,
};

struct platform_device usb_diag_device = {
	.name	= "usb_diag",
	.id	= -1,
	.dev	= {
		.platform_data = &usb_diag_pdata,
	},
};
#endif

#ifdef CONFIG_USB_F_SERIAL
static struct usb_gadget_fserial_platform_data fserial_pdata = {
	.no_ports	= 2,
};

struct platform_device usb_gadget_fserial_device = {
	.name	= "usb_fserial",
	.id	= -1,
	.dev	= {
		.platform_data = &fserial_pdata,
	},
};
#endif

#define MSM_NAND_PHYS		0xA0A00000
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

static struct resource resources_otg[] = {
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

struct platform_device msm_device_otg = {
	.name		= "msm_otg",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_otg),
	.resource	= resources_otg,
	.dev		= {
		.coherent_dma_mask	= 0xffffffffULL,
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
	.name	= "msm_smd",
	.id	= -1,
};

struct resource msm_dmov_resource[] = {
	{
		.start = INT_ADM_AARM,
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

int __init msm_add_sdcc(unsigned int controller, struct mmc_platform_data *plat)
{
	struct platform_device	*pdev;

	if (controller < 1 || controller > 4)
		return -EINVAL;

	pdev = msm_sdcc_devices[controller-1];
	pdev->dev.platform_data = plat;
	return platform_device_register(pdev);
}

#define RAMFS_INFO_MAGICNUMBER		0x654D4D43
#define RAMFS_INFO_VERSION		0x00000001
#define RAMFS_MODEMSTORAGE_ID		0x4D454653

static struct resource rmt_storage_resources[] = {
       {
		.flags  = IORESOURCE_MEM,
       },
};

static struct platform_device rmt_storage_device = {
       .name           = "rmt_storage",
       .id             = -1,
       .num_resources  = ARRAY_SIZE(rmt_storage_resources),
       .resource       = rmt_storage_resources,
};

struct shared_ramfs_entry {
	uint32_t client_id;   	/* Client id to uniquely identify a client */
	uint32_t base_addr;	/* Base address of shared RAMFS memory */
	uint32_t size;		/* Size of the shared RAMFS memory */
	uint32_t reserved;	/* Reserved attribute for future use */
};
struct shared_ramfs_table {
	uint32_t magic_id;  	/* Identify RAMFS details in SMEM */
	uint32_t version;	/* Version of shared_ramfs_table */
	uint32_t entries;	/* Total number of valid entries   */
	struct shared_ramfs_entry ramfs_entry[3];	/* List all entries */
};

int __init rmt_storage_add_ramfs(void)
{
	struct shared_ramfs_table *ramfs_table;
	struct shared_ramfs_entry *ramfs_entry;
	int index;

	ramfs_table = smem_alloc(SMEM_SEFS_INFO,
			sizeof(struct shared_ramfs_table));

	if (!ramfs_table) {
		printk(KERN_WARNING "%s: No RAMFS table in SMEM\n", __func__);
		return -ENOENT;
	}

	if ((ramfs_table->magic_id != (u32) RAMFS_INFO_MAGICNUMBER) ||
		(ramfs_table->version != (u32) RAMFS_INFO_VERSION)) {
		printk(KERN_WARNING "%s: Magic / Version mismatch:, "
		       "magic_id=%#x, format_version=%#x\n", __func__,
		       ramfs_table->magic_id, ramfs_table->version);
		return -ENOENT;
	}

	for (index = 0; index < ramfs_table->entries; index++) {
		ramfs_entry = &ramfs_table->ramfs_entry[index];

		/* Find a match for the Modem Storage RAMFS area */
		if (ramfs_entry->client_id == (u32) RAMFS_MODEMSTORAGE_ID) {
			printk(KERN_INFO "%s: RAMFS Info (from SMEM): "
				"Baseaddr = 0x%08x, Size = 0x%08x\n", __func__,
				ramfs_entry->base_addr, ramfs_entry->size);

			rmt_storage_resources[0].start = ramfs_entry->base_addr;
			rmt_storage_resources[0].end = ramfs_entry->base_addr +
							ramfs_entry->size - 1;
			platform_device_register(&rmt_storage_device);
			return 0;
		}
	}
	return -ENOENT;
}

#if defined(CONFIG_FB_MSM_MDP40)
#define MDP_BASE          0xA3F00000
#define PMDH_BASE         0xAD600000
#define EMDH_BASE         0xAD700000
#define TVENC_BASE        0xAD400000
#else
#define MDP_BASE          0xAA200000
#define PMDH_BASE         0xAA600000
#define EMDH_BASE         0xAA700000
#define TVENC_BASE        0xAA400000
#endif

static struct resource msm_mdp_resources[] = {
	{
		.name   = "mdp",
		.start  = MDP_BASE,
		.end    = MDP_BASE + 0x000F0000 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.start  = INT_MDP,
		.end    = INT_MDP,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct resource msm_mddi_resources[] = {
	{
		.name   = "pmdh",
		.start  = PMDH_BASE,
		.end    = PMDH_BASE + PAGE_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	}
};

static struct resource msm_mddi_ext_resources[] = {
	{
		.name   = "emdh",
		.start  = EMDH_BASE,
		.end    = EMDH_BASE + PAGE_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	}
};

static struct resource msm_ebi2_lcd_resources[] = {
	{
		.name   = "base",
		.start  = 0xa0d00000,
		.end    = 0xa0d00000 + PAGE_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "lcd01",
		.start  = 0x98000000,
		.end    = 0x98000000 + 0x80000 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "lcd02",
		.start  = 0x9c000000,
		.end    = 0x9c000000 + 0x80000 - 1,
		.flags  = IORESOURCE_MEM,
	},
};

static struct resource msm_tvenc_resources[] = {
	{
		.name   = "tvenc",
		.start  = TVENC_BASE,
		.end    = TVENC_BASE + PAGE_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	}
};

static struct platform_device msm_mdp_device = {
	.name   = "mdp",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(msm_mdp_resources),
	.resource       = msm_mdp_resources,
};

static struct platform_device msm_mddi_device = {
	.name   = "mddi",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(msm_mddi_resources),
	.resource       = msm_mddi_resources,
};

static struct platform_device msm_mddi_ext_device = {
	.name   = "mddi_ext",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(msm_mddi_ext_resources),
	.resource       = msm_mddi_ext_resources,
};

static struct platform_device msm_ebi2_lcd_device = {
	.name   = "ebi2_lcd",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(msm_ebi2_lcd_resources),
	.resource       = msm_ebi2_lcd_resources,
};

static struct platform_device msm_lcdc_device = {
	.name   = "lcdc",
	.id     = 0,
};

static struct platform_device msm_tvenc_device = {
	.name   = "tvenc",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(msm_tvenc_resources),
	.resource       = msm_tvenc_resources,
};

/* TSIF begin */
#if defined(CONFIG_TSIF) || defined(CONFIG_TSIF_MODULE)

#define MSM_TSIF_PHYS        (0xa0100000)
#define MSM_TSIF_SIZE        (0x200)

static struct resource tsif_resources[] = {
	[0] = {
		.flags = IORESOURCE_IRQ,
		.start = INT_TSIF_IRQ,
		.end   = INT_TSIF_IRQ,
	},
	[1] = {
		.flags = IORESOURCE_MEM,
		.start = MSM_TSIF_PHYS,
		.end   = MSM_TSIF_PHYS + MSM_TSIF_SIZE - 1,
	},
	[2] = {
		.flags = IORESOURCE_DMA,
		.start = DMOV_TSIF_CHAN,
		.end   = DMOV_TSIF_CRCI,
	},
};

static void tsif_release(struct device *dev)
{
	dev_info(dev, "release\n");
}

struct platform_device msm_device_tsif = {
	.name          = "msm_tsif",
	.id            = 0,
	.num_resources = ARRAY_SIZE(tsif_resources),
	.resource      = tsif_resources,
	.dev = {
		.release       = tsif_release,
	},
};
#endif /* defined(CONFIG_TSIF) || defined(CONFIG_TSIF_MODULE) */
/* TSIF end   */

#define MSM_TSSC_PHYS         0xAA300000
static struct resource resources_tssc[] = {
	{
		.start	= MSM_TSSC_PHYS,
		.end	= MSM_TSSC_PHYS + SZ_4K - 1,
		.name	= "tssc",
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_TCHSCRN1,
		.end	= INT_TCHSCRN1,
		.name	= "tssc1",
		.flags	= IORESOURCE_IRQ | IRQF_TRIGGER_RISING,
	},
	{
		.start	= INT_TCHSCRN2,
		.end	= INT_TCHSCRN2,
		.name	= "tssc2",
		.flags	= IORESOURCE_IRQ | IRQF_TRIGGER_RISING,
	},
};

struct platform_device msm_device_tssc = {
	.name = "msm_touchscreen",
	.id = 0,
	.num_resources = ARRAY_SIZE(resources_tssc),
	.resource = resources_tssc,
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

void __init msm_fb_register_device(char *name, void *data)
{
	if (!strncmp(name, "mdp", 3))
		msm_register_device(&msm_mdp_device, data);
	else if (!strncmp(name, "pmdh", 4))
		msm_register_device(&msm_mddi_device, data);
	else if (!strncmp(name, "emdh", 4))
		msm_register_device(&msm_mddi_ext_device, data);
	else if (!strncmp(name, "ebi2", 4))
		msm_register_device(&msm_ebi2_lcd_device, data);
	else if (!strncmp(name, "tvenc", 5))
		msm_register_device(&msm_tvenc_device, data);
	else if (!strncmp(name, "lcdc", 4))
		msm_register_device(&msm_lcdc_device, data);
	else
		printk(KERN_ERR "%s: unknown device! %s\n", __func__, name);
}

static struct platform_device msm_camera_device = {
	.name	= "msm_camera",
	.id	= 0,
};

void __init msm_camera_register_device(void *res, uint32_t num,
	void *data)
{
	msm_camera_device.num_resources = num;
	msm_camera_device.resource = res;

	msm_register_device(&msm_camera_device, data);
}

static DEFINE_CLK_PCOM(adm_clk,		ADM_CLK, 0);
static DEFINE_CLK_PCOM(adsp_clk,	ADSP_CLK, 0);
static DEFINE_CLK_PCOM(ebi1_clk,	EBI1_CLK, CLK_MIN);
static DEFINE_CLK_PCOM(ebi2_clk,	EBI2_CLK, 0);
static DEFINE_CLK_PCOM(ecodec_clk,	ECODEC_CLK, 0);
static DEFINE_CLK_PCOM(gp_clk,		GP_CLK,	 0);
static DEFINE_CLK_PCOM(i2c_clk,		I2C_CLK, 0);
static DEFINE_CLK_PCOM(icodec_rx_clk,	ICODEC_RX_CLK, 0);
static DEFINE_CLK_PCOM(icodec_tx_clk,	ICODEC_TX_CLK, 0);
static DEFINE_CLK_PCOM(imem_clk,	IMEM_CLK, OFF);
static DEFINE_CLK_PCOM(mdc_clk,		MDC_CLK, 0);
static DEFINE_CLK_PCOM(pmdh_clk,	PMDH_CLK, OFF | CLK_MINMAX);
static DEFINE_CLK_PCOM(mdp_clk,		MDP_CLK, OFF);
static DEFINE_CLK_PCOM(mdp_lcdc_pclk_clk, MDP_LCDC_PCLK_CLK, 0);
static DEFINE_CLK_PCOM(mdp_lcdc_pad_pclk_clk, MDP_LCDC_PAD_PCLK_CLK, 0);
static DEFINE_CLK_PCOM(mdp_vsync_clk,	MDP_VSYNC_CLK,  OFF);
static DEFINE_CLK_PCOM(pbus_clk,	PBUS_CLK, CLK_MIN);
static DEFINE_CLK_PCOM(pcm_clk,		PCM_CLK, 0);
static DEFINE_CLK_PCOM(sdac_clk,	SDAC_CLK, OFF);
static DEFINE_CLK_PCOM(sdc1_clk,	SDC1_CLK, OFF);
static DEFINE_CLK_PCOM(sdc1_p_clk,	SDC1_P_CLK, OFF);
static DEFINE_CLK_PCOM(sdc2_clk,	SDC2_CLK, OFF);
static DEFINE_CLK_PCOM(sdc2_p_clk,	SDC2_P_CLK, OFF);
static DEFINE_CLK_PCOM(sdc3_clk,	SDC3_CLK, OFF);
static DEFINE_CLK_PCOM(sdc3_p_clk,	SDC3_P_CLK, OFF);
static DEFINE_CLK_PCOM(sdc4_clk,	SDC4_CLK, OFF);
static DEFINE_CLK_PCOM(sdc4_p_clk,	SDC4_P_CLK, OFF);
static DEFINE_CLK_PCOM(uart1_clk,	UART1_CLK, OFF);
static DEFINE_CLK_PCOM(uart2_clk,	UART2_CLK, 0);
static DEFINE_CLK_PCOM(uart3_clk,	UART3_CLK, OFF);
static DEFINE_CLK_PCOM(uart1dm_clk,	UART1DM_CLK, OFF);
static DEFINE_CLK_PCOM(uart2dm_clk,	UART2DM_CLK, 0);
static DEFINE_CLK_PCOM(usb_hs_clk,	USB_HS_CLK, OFF);
static DEFINE_CLK_PCOM(usb_hs_p_clk,	USB_HS_P_CLK, OFF);
static DEFINE_CLK_PCOM(usb_otg_clk,	USB_OTG_CLK, 0);
static DEFINE_CLK_PCOM(vdc_clk,		VDC_CLK, OFF | CLK_MIN);
static DEFINE_CLK_PCOM(vfe_clk,		VFE_CLK, OFF);
static DEFINE_CLK_PCOM(vfe_mdc_clk,	VFE_MDC_CLK, OFF);

struct clk_lookup msm_clocks_7x25[] = {
	CLK_LOOKUP("adm_clk",		adm_clk.c,	NULL),
	CLK_LOOKUP("adsp_clk",		adsp_clk.c,	NULL),
	CLK_LOOKUP("ebi1_clk",		ebi1_clk.c,	NULL),
	CLK_LOOKUP("ebi2_clk",		ebi2_clk.c,	NULL),
	CLK_LOOKUP("ecodec_clk",	ecodec_clk.c,	NULL),
	CLK_LOOKUP("gp_clk",		gp_clk.c,		NULL),
	CLK_LOOKUP("i2c_clk",		i2c_clk.c,	"msm_i2c.0"),
	CLK_LOOKUP("icodec_rx_clk",	icodec_rx_clk.c,	NULL),
	CLK_LOOKUP("icodec_tx_clk",	icodec_tx_clk.c,	NULL),
	CLK_LOOKUP("imem_clk",		imem_clk.c,	NULL),
	CLK_LOOKUP("mdc_clk",		mdc_clk.c,	NULL),
	CLK_LOOKUP("mddi_clk",		pmdh_clk.c,	NULL),
	CLK_LOOKUP("mdp_clk",		mdp_clk.c,	NULL),
	CLK_LOOKUP("mdp_lcdc_pclk_clk", mdp_lcdc_pclk_clk.c, NULL),
	CLK_LOOKUP("mdp_lcdc_pad_pclk_clk", mdp_lcdc_pad_pclk_clk.c, NULL),
	CLK_LOOKUP("mdp_vsync_clk",	mdp_vsync_clk.c,  NULL),
	CLK_LOOKUP("pbus_clk",		pbus_clk.c,	NULL),
	CLK_LOOKUP("pcm_clk",		pcm_clk.c,	NULL),
	CLK_LOOKUP("sdac_clk",		sdac_clk.c,	NULL),
	CLK_LOOKUP("sdc_clk",		sdc1_clk.c,	"msm_sdcc.1"),
	CLK_LOOKUP("sdc_pclk",		sdc1_p_clk.c,	"msm_sdcc.1"),
	CLK_LOOKUP("sdc_clk",		sdc2_clk.c,	"msm_sdcc.2"),
	CLK_LOOKUP("sdc_pclk",		sdc2_p_clk.c,	"msm_sdcc.2"),
	CLK_LOOKUP("sdc_clk",		sdc3_clk.c,	"msm_sdcc.3"),
	CLK_LOOKUP("sdc_pclk",		sdc3_p_clk.c,	"msm_sdcc.3"),
	CLK_LOOKUP("sdc_clk",		sdc4_clk.c,	"msm_sdcc.4"),
	CLK_LOOKUP("sdc_pclk",		sdc4_p_clk.c,	"msm_sdcc.4"),
	CLK_LOOKUP("uart_clk",		uart1_clk.c,	"msm_serial.0"),
	CLK_LOOKUP("uart_clk",		uart2_clk.c,	"msm_serial.1"),
	CLK_LOOKUP("uart_clk",		uart3_clk.c,	"msm_serial.2"),
	CLK_LOOKUP("uartdm_clk",	uart1dm_clk.c,	"msm_serial_hs.0"),
	CLK_LOOKUP("uartdm_clk",	uart2dm_clk.c,	"msm_serial_hs.1"),
	CLK_LOOKUP("usb_hs_clk",	usb_hs_clk.c,	NULL),
	CLK_LOOKUP("usb_hs_pclk",	usb_hs_p_clk.c,	NULL),
	CLK_LOOKUP("usb_otg_clk",	usb_otg_clk.c,	NULL),
	CLK_LOOKUP("vdc_clk",		vdc_clk.c,	NULL),
	CLK_LOOKUP("vfe_clk",		vfe_clk.c,	NULL),
	CLK_LOOKUP("vfe_mdc_clk",	vfe_mdc_clk.c,	NULL),
};

unsigned msm_num_clocks_7x25 = ARRAY_SIZE(msm_clocks_7x25);

