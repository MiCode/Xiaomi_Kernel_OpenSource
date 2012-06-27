/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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
#include <linux/platform_data/qcom_crypto_device.h>
#include <linux/dma-mapping.h>
#include <sound/msm-dai-q6.h>
#include <sound/apr_audio.h>
#include <linux/usb/android.h>
#include <asm/hardware/gic.h>
#include <asm/mach/flash.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <mach/msm_hsusb.h>
#include <mach/irqs.h>
#include <mach/socinfo.h>
#include <mach/rpm.h>
#include <mach/msm_bus_board.h>
#include <asm/hardware/cache-l2x0.h>
#include <mach/msm_sps.h>
#include <mach/dma.h>
#include "pm.h"
#include "devices.h"
#include <mach/mpm.h>
#include "spm.h"
#include "rpm_resources.h"
#include "msm_watchdog.h"
#include "rpm_stats.h"
#include "rpm_log.h"

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

#define MSM_GPIO_I2C_CLK 16
#define MSM_GPIO_I2C_SDA 17

static struct msm_watchdog_pdata msm_watchdog_pdata = {
	.pet_time = 10000,
	.bark_time = 11000,
	.has_secure = false,
	.use_kernel_fiq = true,
};

struct platform_device msm9615_device_watchdog = {
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

struct platform_device msm9615_device_dmov = {
	.name	= "msm_dmov",
	.id	= -1,
	.resource = msm_dmov_resource,
	.num_resources = ARRAY_SIZE(msm_dmov_resource),
	.dev = {
		.platform_data = &msm_dmov_pdata,
	},
};

struct platform_device msm9615_device_acpuclk = {
	.name           = "acpuclk-9615",
	.id             = -1,
};

#define MSM_USB_BAM_BASE     0x12502000
#define MSM_USB_BAM_SIZE     SZ_16K
#define MSM_HSIC_BAM_BASE    0x12542000
#define MSM_HSIC_BAM_SIZE    SZ_16K

static struct resource resources_otg[] = {
	{
		.start	= MSM9615_HSUSB_PHYS,
		.end	= MSM9615_HSUSB_PHYS + MSM9615_HSUSB_SIZE - 1,
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
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};

#define MSM_HSUSB_RESUME_GPIO	79

static struct resource resources_hsusb[] = {
	{
		.start	= MSM9615_HSUSB_PHYS,
		.end	= MSM9615_HSUSB_PHYS + MSM9615_HSUSB_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= USB1_HS_IRQ,
		.end	= USB1_HS_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= MSM_HSUSB_RESUME_GPIO,
		.end	= MSM_HSUSB_RESUME_GPIO,
		.name	= "USB_RESUME",
		.flags	= IORESOURCE_IO,
	},
};

static struct resource resources_usb_bam[] = {
	{
		.name	= "usb_bam_addr",
		.start	= MSM_USB_BAM_BASE,
		.end	= MSM_USB_BAM_BASE + MSM_USB_BAM_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "usb_bam_irq",
		.start	= USB1_HS_BAM_IRQ,
		.end	= USB1_HS_BAM_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "hsic_bam_addr",
		.start	= MSM_HSIC_BAM_BASE,
		.end	= MSM_HSIC_BAM_BASE + MSM_HSIC_BAM_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "hsic_bam_irq",
		.start	= USB_HSIC_BAM_IRQ,
		.end	= USB_HSIC_BAM_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm_device_usb_bam = {
	.name		= "usb_bam",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_usb_bam),
	.resource	= resources_usb_bam,
};

struct platform_device msm_device_gadget_peripheral = {
	.name		= "msm_hsusb",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_hsusb),
	.resource	= resources_hsusb,
	.dev		= {
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};

static struct resource resources_hsic_peripheral[] = {
	{
		.start	= MSM9615_HSIC_PHYS,
		.end	= MSM9615_HSIC_PHYS + MSM9615_HSIC_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= USB_HSIC_IRQ,
		.end	= USB_HSIC_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm_device_hsic_peripheral = {
	.name		= "msm_hsic_peripheral",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_hsic_peripheral),
	.resource	= resources_hsic_peripheral,
	.dev		= {
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};

static struct resource resources_hsusb_host[] = {
	{
		.start  = MSM9615_HSUSB_PHYS,
		.end    = MSM9615_HSUSB_PHYS + MSM9615_HSUSB_PHYS - 1,
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
		.start	= MSM9615_HSIC_PHYS,
		.end	= MSM9615_HSIC_PHYS + MSM9615_HSIC_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= USB_HSIC_IRQ,
		.end	= USB_HSIC_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm_device_hsic_host = {
	.name		= "msm_hsic_host",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_hsic_host),
	.resource	= resources_hsic_host,
	.dev		= {
		.dma_mask               = &dma_mask,
		.coherent_dma_mask      = 0xffffffff,
	},
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
	{
		.name   = "i2c_clk",
		.start     = MSM_GPIO_I2C_CLK,
		.end       = MSM_GPIO_I2C_CLK,
		.flags     = IORESOURCE_IO,
	},
	{
		.name   = "i2c_sda",
		.start     = MSM_GPIO_I2C_SDA,
		.end       = MSM_GPIO_I2C_SDA,
		.flags     = IORESOURCE_IO,

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

#define LPASS_SLIMBUS_PHYS	0x28080000
#define LPASS_SLIMBUS_BAM_PHYS	0x28084000
#define LPASS_SLIMBUS_SLEW	(MSM9615_TLMM_PHYS + 0x207C)
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

struct platform_device msm9615_slim_ctrl = {
	.name	= "msm_slim_ctrl",
	.id	= 1,
	.num_resources	= ARRAY_SIZE(slimbus_res),
	.resource	= slimbus_res,
	.dev            = {
		.coherent_dma_mask      = 0xffffffffULL,
	},
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

struct platform_device msm_cpudai_bt_rx = {
	.name   = "msm-dai-q6",
	.id     = 0x3000,
};

struct platform_device msm_cpudai_bt_tx = {
	.name   = "msm-dai-q6",
	.id     = 0x3001,
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

struct msm_dai_auxpcm_pdata sec_auxpcm_pdata = {
	.clk = "sec_pcm_clk",
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

struct platform_device msm_cpudai_sec_auxpcm_rx = {
	.name = "msm-dai-q6",
	.id = 12,
	.dev = {
		.platform_data = &sec_auxpcm_pdata,
	},
};

struct platform_device msm_cpudai_sec_auxpcm_tx = {
	.name = "msm-dai-q6",
	.id = 13,
	.dev = {
		.platform_data = &sec_auxpcm_pdata,
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

struct platform_device msm_i2s_cpudai0 = {
	.name   = "msm-dai-q6",
	.id     = PRIMARY_I2S_RX,
};

struct platform_device msm_i2s_cpudai1 = {
	.name   = "msm-dai-q6",
	.id     = PRIMARY_I2S_TX,
};
struct platform_device msm_voip = {
	.name	= "msm-voip-dsp",
	.id	= -1,
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
	.version = VERSION_2,
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

struct platform_device msm_device_bam_dmux = {
	.name		= "BAM_RMNT",
	.id		= -1,
};

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

#if defined(CONFIG_CRYPTO_DEV_QCRYPTO) || \
		defined(CONFIG_CRYPTO_DEV_QCRYPTO_MODULE) || \
		defined(CONFIG_CRYPTO_DEV_QCEDEV) || \
		defined(CONFIG_CRYPTO_DEV_QCEDEV_MODULE)

#define QCE_SIZE		0x10000
#define QCE_0_BASE		0x18500000

#define QCE_HW_KEY_SUPPORT	0
#define QCE_SHA_HMAC_SUPPORT	1
#define QCE_SHARE_CE_RESOURCE	1
#define QCE_CE_SHARED		0

static struct resource qcrypto_resources[] = {
	[0] = {
		.start = QCE_0_BASE,
		.end = QCE_0_BASE + QCE_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.name = "crypto_channels",
		.start = DMOV_CE_IN_CHAN,
		.end = DMOV_CE_OUT_CHAN,
		.flags = IORESOURCE_DMA,
	},
	[2] = {
		.name = "crypto_crci_in",
		.start = DMOV_CE_IN_CRCI,
		.end = DMOV_CE_IN_CRCI,
		.flags = IORESOURCE_DMA,
	},
	[3] = {
		.name = "crypto_crci_out",
		.start = DMOV_CE_OUT_CRCI,
		.end = DMOV_CE_OUT_CRCI,
		.flags = IORESOURCE_DMA,
	},
};

static struct resource qcedev_resources[] = {
	[0] = {
		.start = QCE_0_BASE,
		.end = QCE_0_BASE + QCE_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.name = "crypto_channels",
		.start = DMOV_CE_IN_CHAN,
		.end = DMOV_CE_OUT_CHAN,
		.flags = IORESOURCE_DMA,
	},
	[2] = {
		.name = "crypto_crci_in",
		.start = DMOV_CE_IN_CRCI,
		.end = DMOV_CE_IN_CRCI,
		.flags = IORESOURCE_DMA,
	},
	[3] = {
		.name = "crypto_crci_out",
		.start = DMOV_CE_OUT_CRCI,
		.end = DMOV_CE_OUT_CRCI,
		.flags = IORESOURCE_DMA,
	},
};

#endif

#if defined(CONFIG_CRYPTO_DEV_QCRYPTO) || \
		defined(CONFIG_CRYPTO_DEV_QCRYPTO_MODULE)

static struct msm_ce_hw_support qcrypto_ce_hw_suppport = {
	.ce_shared = QCE_CE_SHARED,
	.shared_ce_resource = QCE_SHARE_CE_RESOURCE,
	.hw_key_support = QCE_HW_KEY_SUPPORT,
	.sha_hmac = QCE_SHA_HMAC_SUPPORT,
	.bus_scale_table = NULL,
};

struct platform_device msm9615_qcrypto_device = {
	.name		= "qcrypto",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(qcrypto_resources),
	.resource	= qcrypto_resources,
	.dev		= {
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.platform_data = &qcrypto_ce_hw_suppport,
	},
};
#endif

#if defined(CONFIG_CRYPTO_DEV_QCEDEV) || \
		defined(CONFIG_CRYPTO_DEV_QCEDEV_MODULE)

static struct msm_ce_hw_support qcedev_ce_hw_suppport = {
	.ce_shared = QCE_CE_SHARED,
	.shared_ce_resource = QCE_SHARE_CE_RESOURCE,
	.hw_key_support = QCE_HW_KEY_SUPPORT,
	.sha_hmac = QCE_SHA_HMAC_SUPPORT,
	.bus_scale_table = NULL,
};

struct platform_device msm9615_qcedev_device = {
	.name		= "qce",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(qcedev_resources),
	.resource	= qcedev_resources,
	.dev		= {
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.platform_data = &qcedev_ce_hw_suppport,
	},
};
#endif

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

#ifdef CONFIG_FB_MSM_EBI2
static struct resource msm_ebi2_lcdc_resources[] = {
	{
		.name   = "base",
		.start  = 0x1B300000,
		.end    = 0x1B300000 + PAGE_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "lcd01",
		.start  = 0x1FC00000,
		.end    = 0x1FC00000 + 0x80000 - 1,
		.flags  = IORESOURCE_MEM,
	},
};

struct platform_device msm_ebi2_lcdc_device = {
	.name   = "ebi2_lcd",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(msm_ebi2_lcdc_resources),
	.resource       = msm_ebi2_lcdc_resources,
};
#endif

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

struct msm_rpm_platform_data msm9615_rpm_data __initdata = {
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
		MSM_RPM_MAP(9615, NOTIFICATION_CONFIGURED_0, NOTIFICATION, 4),
		MSM_RPM_MAP(9615, NOTIFICATION_REGISTERED_0, NOTIFICATION, 4),
		MSM_RPM_MAP(9615, INVALIDATE_0, INVALIDATE, 8),
		MSM_RPM_MAP(9615, TRIGGER_TIMED_TO, TRIGGER_TIMED, 1),
		MSM_RPM_MAP(9615, TRIGGER_TIMED_SCLK_COUNT, TRIGGER_TIMED, 1),
		MSM_RPM_MAP(9615, RPM_CTL, RPM_CTL, 1),
		MSM_RPM_MAP(9615, CXO_CLK, CXO_CLK, 1),
		MSM_RPM_MAP(9615, SYSTEM_FABRIC_CLK, SYSTEM_FABRIC_CLK, 1),
		MSM_RPM_MAP(9615, DAYTONA_FABRIC_CLK, DAYTONA_FABRIC_CLK, 1),
		MSM_RPM_MAP(9615, SFPB_CLK, SFPB_CLK, 1),
		MSM_RPM_MAP(9615, CFPB_CLK, CFPB_CLK, 1),
		MSM_RPM_MAP(9615, EBI1_CLK, EBI1_CLK, 1),
		MSM_RPM_MAP(9615, SYS_FABRIC_CFG_HALT_0,
				SYS_FABRIC_CFG_HALT, 2),
		MSM_RPM_MAP(9615, SYS_FABRIC_CFG_CLKMOD_0,
				SYS_FABRIC_CFG_CLKMOD, 3),
		MSM_RPM_MAP(9615, SYS_FABRIC_CFG_IOCTL,
				SYS_FABRIC_CFG_IOCTL, 1),
		MSM_RPM_MAP(9615, SYSTEM_FABRIC_ARB_0,
				SYSTEM_FABRIC_ARB, 27),
		MSM_RPM_MAP(9615, PM8018_S1_0, PM8018_S1, 2),
		MSM_RPM_MAP(9615, PM8018_S2_0, PM8018_S2, 2),
		MSM_RPM_MAP(9615, PM8018_S3_0, PM8018_S3, 2),
		MSM_RPM_MAP(9615, PM8018_S4_0, PM8018_S4, 2),
		MSM_RPM_MAP(9615, PM8018_S5_0, PM8018_S5, 2),
		MSM_RPM_MAP(9615, PM8018_L1_0, PM8018_L1, 2),
		MSM_RPM_MAP(9615, PM8018_L2_0, PM8018_L2, 2),
		MSM_RPM_MAP(9615, PM8018_L3_0, PM8018_L3, 2),
		MSM_RPM_MAP(9615, PM8018_L4_0, PM8018_L4, 2),
		MSM_RPM_MAP(9615, PM8018_L5_0, PM8018_L5, 2),
		MSM_RPM_MAP(9615, PM8018_L6_0, PM8018_L6, 2),
		MSM_RPM_MAP(9615, PM8018_L7_0, PM8018_L7, 2),
		MSM_RPM_MAP(9615, PM8018_L8_0, PM8018_L8, 2),
		MSM_RPM_MAP(9615, PM8018_L9_0, PM8018_L9, 2),
		MSM_RPM_MAP(9615, PM8018_L10_0, PM8018_L10, 2),
		MSM_RPM_MAP(9615, PM8018_L11_0, PM8018_L11, 2),
		MSM_RPM_MAP(9615, PM8018_L12_0, PM8018_L12, 2),
		MSM_RPM_MAP(9615, PM8018_L13_0, PM8018_L13, 2),
		MSM_RPM_MAP(9615, PM8018_L14_0, PM8018_L14, 2),
		MSM_RPM_MAP(9615, PM8018_LVS1, PM8018_LVS1, 1),
		MSM_RPM_MAP(9615, NCP_0, NCP, 2),
		MSM_RPM_MAP(9615, CXO_BUFFERS, CXO_BUFFERS, 1),
		MSM_RPM_MAP(9615, USB_OTG_SWITCH, USB_OTG_SWITCH, 1),
		MSM_RPM_MAP(9615, HDMI_SWITCH, HDMI_SWITCH, 1),
		MSM_RPM_MAP(9615, VOLTAGE_CORNER, VOLTAGE_CORNER, 1),
	},
	.target_status = {
		MSM_RPM_STATUS_ID_MAP(9615, VERSION_MAJOR),
		MSM_RPM_STATUS_ID_MAP(9615, VERSION_MINOR),
		MSM_RPM_STATUS_ID_MAP(9615, VERSION_BUILD),
		MSM_RPM_STATUS_ID_MAP(9615, SUPPORTED_RESOURCES_0),
		MSM_RPM_STATUS_ID_MAP(9615, SUPPORTED_RESOURCES_1),
		MSM_RPM_STATUS_ID_MAP(9615, SUPPORTED_RESOURCES_2),
		MSM_RPM_STATUS_ID_MAP(9615, RESERVED_SUPPORTED_RESOURCES_0),
		MSM_RPM_STATUS_ID_MAP(9615, SEQUENCE),
		MSM_RPM_STATUS_ID_MAP(9615, RPM_CTL),
		MSM_RPM_STATUS_ID_MAP(9615, CXO_CLK),
		MSM_RPM_STATUS_ID_MAP(9615, SYSTEM_FABRIC_CLK),
		MSM_RPM_STATUS_ID_MAP(9615, DAYTONA_FABRIC_CLK),
		MSM_RPM_STATUS_ID_MAP(9615, SFPB_CLK),
		MSM_RPM_STATUS_ID_MAP(9615, CFPB_CLK),
		MSM_RPM_STATUS_ID_MAP(9615, EBI1_CLK),
		MSM_RPM_STATUS_ID_MAP(9615, SYS_FABRIC_CFG_HALT),
		MSM_RPM_STATUS_ID_MAP(9615, SYS_FABRIC_CFG_CLKMOD),
		MSM_RPM_STATUS_ID_MAP(9615, SYS_FABRIC_CFG_IOCTL),
		MSM_RPM_STATUS_ID_MAP(9615, SYSTEM_FABRIC_ARB),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_S1_0),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_S1_1),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_S2_0),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_S2_1),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_S3_0),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_S3_1),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_S4_0),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_S4_1),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_S5_0),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_S5_1),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_L1_0),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_L1_1),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_L2_0),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_L2_1),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_L3_0),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_L3_1),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_L4_0),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_L4_1),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_L5_0),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_L5_1),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_L6_0),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_L6_1),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_L7_0),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_L7_1),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_L8_0),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_L8_1),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_L9_0),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_L9_1),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_L10_0),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_L10_1),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_L11_0),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_L11_1),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_L12_0),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_L12_1),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_L13_0),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_L13_1),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_L14_0),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_L14_1),
		MSM_RPM_STATUS_ID_MAP(9615, PM8018_LVS1),
		MSM_RPM_STATUS_ID_MAP(9615, NCP_0),
		MSM_RPM_STATUS_ID_MAP(9615, NCP_1),
		MSM_RPM_STATUS_ID_MAP(9615, CXO_BUFFERS),
		MSM_RPM_STATUS_ID_MAP(9615, USB_OTG_SWITCH),
		MSM_RPM_STATUS_ID_MAP(9615, HDMI_SWITCH),
		MSM_RPM_STATUS_ID_MAP(9615, VOLTAGE_CORNER),
	},
	.target_ctrl_id = {
		MSM_RPM_CTRL_MAP(9615, VERSION_MAJOR),
		MSM_RPM_CTRL_MAP(9615, VERSION_MINOR),
		MSM_RPM_CTRL_MAP(9615, VERSION_BUILD),
		MSM_RPM_CTRL_MAP(9615, REQ_CTX_0),
		MSM_RPM_CTRL_MAP(9615, REQ_SEL_0),
		MSM_RPM_CTRL_MAP(9615, ACK_CTX_0),
		MSM_RPM_CTRL_MAP(9615, ACK_SEL_0),
	},
	.sel_invalidate = MSM_RPM_9615_SEL_INVALIDATE,
	.sel_notification = MSM_RPM_9615_SEL_NOTIFICATION,
	.sel_last = MSM_RPM_9615_SEL_LAST,
	.ver = {3, 0, 0},
};

struct platform_device msm9615_rpm_device = {
	.name   = "msm_rpm",
	.id     = -1,
};

static uint16_t msm_mpm_irqs_m2a[MSM_MPM_NR_MPM_IRQS] __initdata = {
	[4] = MSM_GPIO_TO_INT(30),
	[5] = MSM_GPIO_TO_INT(59),
	[6] = MSM_GPIO_TO_INT(81),
	[7] = MSM_GPIO_TO_INT(87),
	[8] = MSM_GPIO_TO_INT(86),
	[9] = MSM_GPIO_TO_INT(2),
	[10] = MSM_GPIO_TO_INT(6),
	[11] = MSM_GPIO_TO_INT(10),
	[12] = MSM_GPIO_TO_INT(14),
	[13] = MSM_GPIO_TO_INT(18),
	[14] = MSM_GPIO_TO_INT(7),
	[15] = MSM_GPIO_TO_INT(11),
	[16] = MSM_GPIO_TO_INT(15),
	[19] = MSM_GPIO_TO_INT(26),
	[20] = MSM_GPIO_TO_INT(28),
	[22] = USB_HSIC_IRQ,
	[23] = MSM_GPIO_TO_INT(19),
	[24] = MSM_GPIO_TO_INT(23),
	[26] = MSM_GPIO_TO_INT(3),
	[27] = MSM_GPIO_TO_INT(68),
	[29] = MSM_GPIO_TO_INT(78),
	[31] = MSM_GPIO_TO_INT(0),
	[32] = MSM_GPIO_TO_INT(4),
	[33] = MSM_GPIO_TO_INT(22),
	[34] = MSM_GPIO_TO_INT(17),
	[37] = MSM_GPIO_TO_INT(20),
	[39] = MSM_GPIO_TO_INT(84),
	[40] = USB1_HS_IRQ,
	[42] = MSM_GPIO_TO_INT(24),
	[43] = MSM_GPIO_TO_INT(79),
	[44] = MSM_GPIO_TO_INT(80),
	[45] = MSM_GPIO_TO_INT(82),
	[46] = MSM_GPIO_TO_INT(85),
	[47] = MSM_GPIO_TO_INT(45),
	[48] = MSM_GPIO_TO_INT(50),
	[49] = MSM_GPIO_TO_INT(51),
	[50] = MSM_GPIO_TO_INT(69),
	[51] = MSM_GPIO_TO_INT(77),
	[52] = MSM_GPIO_TO_INT(1),
	[53] = MSM_GPIO_TO_INT(5),
	[54] = MSM_GPIO_TO_INT(40),
	[55] = MSM_GPIO_TO_INT(27),
};

static uint16_t msm_mpm_bypassed_apps_irqs[] __initdata = {
	TLMM_MSM_SUMMARY_IRQ,
	RPM_APCC_CPU0_GP_HIGH_IRQ,
	RPM_APCC_CPU0_GP_MEDIUM_IRQ,
	RPM_APCC_CPU0_GP_LOW_IRQ,
	RPM_APCC_CPU0_WAKE_UP_IRQ,
	MSS_TO_APPS_IRQ_0,
	MSS_TO_APPS_IRQ_1,
	LPASS_SCSS_GP_LOW_IRQ,
	LPASS_SCSS_GP_MEDIUM_IRQ,
	LPASS_SCSS_GP_HIGH_IRQ,
	SPS_MTI_31,
	A2_BAM_IRQ,
};

struct msm_mpm_device_data msm9615_mpm_dev_data __initdata = {
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

static uint8_t spm_wfi_cmd_sequence[] __initdata = {
	0x00, 0x03, 0x00, 0x0f,
};

static uint8_t spm_power_collapse_without_rpm[] __initdata = {
	0x34, 0x24, 0x14, 0x04,
	0x54, 0x03, 0x54, 0x04,
	0x14, 0x24, 0x3e, 0x0f,
};

static uint8_t spm_power_collapse_with_rpm[] __initdata = {
	0x34, 0x24, 0x14, 0x04,
	0x54, 0x07, 0x54, 0x04,
	0x14, 0x24, 0x3e, 0x0f,
};

static struct msm_spm_seq_entry msm_spm_seq_list[] __initdata = {
	[0] = {
		.mode = MSM_SPM_MODE_CLOCK_GATING,
		.notify_rpm = false,
		.cmd = spm_wfi_cmd_sequence,
	},
	[1] = {
		.mode = MSM_SPM_MODE_POWER_COLLAPSE,
		.notify_rpm = false,
		.cmd = spm_power_collapse_without_rpm,
	},
	[2] = {
		.mode = MSM_SPM_MODE_POWER_COLLAPSE,
		.notify_rpm = true,
		.cmd = spm_power_collapse_with_rpm,
	},
};

static struct msm_spm_platform_data msm_spm_data[] __initdata = {
	[0] = {
		.reg_base_addr = MSM_SAW0_BASE,
		.reg_init_values[MSM_SPM_REG_SAW2_SPM_CTL] = 0x01,
		.reg_init_values[MSM_SPM_REG_SAW2_CFG] = 0x1001,
		.num_modes = ARRAY_SIZE(msm_spm_seq_list),
		.modes = msm_spm_seq_list,
	},
};

static struct msm_rpmrs_level msm_rpmrs_levels[] __initdata = {
	{
		MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT,
		MSM_RPMRS_LIMITS(ON, ACTIVE, MAX, ACTIVE),
		true,
		100, 8000, 100000, 1,
	},
	{
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE,
		MSM_RPMRS_LIMITS(ON, ACTIVE, MAX, ACTIVE),
		true,
		2000, 5000, 60100000, 3000,
	},
	{
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE,
		MSM_RPMRS_LIMITS(ON, ACTIVE, MAX, ACTIVE),
		false,
		6300, 5000, 60350000, 3500,
	},
	{
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE,
		MSM_RPMRS_LIMITS(OFF, HSFS_OPEN, MAX, ACTIVE),
		false,
		13300, 2000, 71850000, 6800,
	},
	{
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE,
		MSM_RPMRS_LIMITS(OFF, HSFS_OPEN, RET_HIGH, RET_LOW),
		false,
		28300, 0, 76350000, 9800,
	},
};

static struct msm_rpmrs_platform_data msm_rpmrs_data __initdata = {
	.levels = &msm_rpmrs_levels[0],
	.num_levels = ARRAY_SIZE(msm_rpmrs_levels),
	.vdd_mem_levels  = {
		[MSM_RPMRS_VDD_MEM_RET_LOW]     = 750000,
		[MSM_RPMRS_VDD_MEM_RET_HIGH]    = 750000,
		[MSM_RPMRS_VDD_MEM_ACTIVE]      = 1050000,
		[MSM_RPMRS_VDD_MEM_MAX]         = 1150000,
	},
	.vdd_dig_levels = {
		[MSM_RPMRS_VDD_DIG_RET_LOW]     = 0,
		[MSM_RPMRS_VDD_DIG_RET_HIGH]    = 0,
		[MSM_RPMRS_VDD_DIG_ACTIVE]      = 1,
		[MSM_RPMRS_VDD_DIG_MAX]         = 3,
	},
	.vdd_mask = 0x7FFFFF,
	.rpmrs_target_id = {
		[MSM_RPMRS_ID_PXO_CLK]          = MSM_RPM_ID_CXO_CLK,
		[MSM_RPMRS_ID_L2_CACHE_CTL]     = MSM_RPM_ID_LAST,
		[MSM_RPMRS_ID_VDD_DIG_0]        = MSM_RPM_ID_VOLTAGE_CORNER,
		[MSM_RPMRS_ID_VDD_DIG_1]        = MSM_RPM_ID_LAST,
		[MSM_RPMRS_ID_VDD_MEM_0]        = MSM_RPM_ID_PM8018_L9_0,
		[MSM_RPMRS_ID_VDD_MEM_1]        = MSM_RPM_ID_PM8018_L9_1,
		[MSM_RPMRS_ID_RPM_CTL]          = MSM_RPM_ID_RPM_CTL,
	},
};

static struct msm_rpmstats_platform_data msm_rpm_stat_pdata = {
	.phys_addr_base = 0x0010D204,
	.phys_size = SZ_8K,
};

struct platform_device msm9615_rpm_stat_device = {
	.name = "msm_rpm_stat",
	.id = -1,
	.dev = {
		.platform_data = &msm_rpm_stat_pdata,
	},
};

static struct msm_rpm_log_platform_data msm_rpm_log_pdata = {
	.phys_addr_base = 0x0010AC00,
	.reg_offsets = {
		[MSM_RPM_LOG_PAGE_INDICES] = 0x00000080,
		[MSM_RPM_LOG_PAGE_BUFFER]  = 0x000000A0,
	},
	.phys_size = SZ_8K,
	.log_len = 4096,		  /* log's buffer length in bytes */
	.log_len_mask = (4096 >> 2) - 1,  /* length mask in units of u32 */
};

struct platform_device msm9615_rpm_log_device = {
	.name	= "msm_rpm_log",
	.id	= -1,
	.dev	= {
		.platform_data = &msm_rpm_log_pdata,
	},
};

uint32_t __init msm9615_rpm_get_swfi_latency(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(msm_rpmrs_levels); i++) {
		if (msm_rpmrs_levels[i].sleep_mode ==
			MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT)
				return msm_rpmrs_levels[i].latency_us;
	}
	return 0;
}

struct android_usb_platform_data msm_android_usb_pdata = {
	.usb_core_id = 0,
};

struct platform_device msm_android_usb_device = {
	.name	= "android_usb",
	.id	= -1,
	.dev	= {
		.platform_data = &msm_android_usb_pdata,
	},
};

struct android_usb_platform_data msm_android_usb_hsic_pdata  = {
	.usb_core_id = 1,
};

struct platform_device msm_android_usb_hsic_device = {
	.name	= "android_usb_hsic",
	.id	= -1,
	.dev	= {
		.platform_data = &msm_android_usb_hsic_pdata,
	},
};


void __init msm9615_device_init(void)
{
	msm_spm_init(msm_spm_data, ARRAY_SIZE(msm_spm_data));
	BUG_ON(msm_rpm_init(&msm9615_rpm_data));
	BUG_ON(msm_rpmrs_levels_init(&msm_rpmrs_data));
	msm_android_usb_pdata.swfi_latency =
		msm_rpmrs_levels[0].latency_us;
	msm_android_usb_hsic_pdata.swfi_latency =
		msm_rpmrs_levels[0].latency_us;

}

#define MSM_SHARED_RAM_PHYS 0x40000000
void __init msm9615_map_io(void)
{
	msm_shared_ram_phys = MSM_SHARED_RAM_PHYS;
	msm_map_msm9615_io();
	l2x0_cache_init();
	if (socinfo_init() < 0)
		pr_err("socinfo_init() failed!\n");
}

void __init msm9615_init_irq(void)
{
	struct msm_mpm_device_data *data = NULL;

#ifdef CONFIG_MSM_MPM
	data = &msm9615_mpm_dev_data;
#endif

	msm_mpm_irq_extn_init(data);
	gic_init(0, GIC_PPI_START, MSM_QGIC_DIST_BASE,
						(void *)MSM_QGIC_CPU_BASE);
}

struct platform_device msm_bus_9615_sys_fabric = {
	.name  = "msm_bus_fabric",
	.id    =  MSM_BUS_FAB_SYSTEM,
};

struct platform_device msm_bus_def_fab = {
	.name  = "msm_bus_fabric",
	.id    =  MSM_BUS_FAB_DEFAULT,
};

#ifdef CONFIG_FB_MSM_EBI2
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
	if (!strncmp(name, "ebi2", 4))
		msm_register_device(&msm_ebi2_lcdc_device, data);
	else
		pr_err("%s: unknown device! %s\n", __func__, name);
}
#endif
