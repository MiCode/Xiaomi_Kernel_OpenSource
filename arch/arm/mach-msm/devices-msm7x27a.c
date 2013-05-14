/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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
#include <mach/kgsl.h>
#include <linux/regulator/machine.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/notifier.h>
#include <mach/irqs.h>
#include <mach/msm_iomap.h>
#include <mach/board.h>
#include <mach/dma.h>
#include <mach/dal_axi.h>
#include <asm/mach/flash.h>
#include <asm/hardware/gic.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/mach/mmc.h>
#include <asm/cacheflush.h>
#include <mach/rpc_hsusb.h>
#include <mach/socinfo.h>
#include <mach/clk-provider.h>

#include "devices.h"
#include "devices-msm7x2xa.h"
#include "footswitch.h"
#include "acpuclock.h"
#include "acpuclock-8625q.h"
#include "spm.h"
#include "mpm-8625.h"
#include "irq.h"
#include "pm.h"
#include "msm_cpr.h"
#include "msm_smem_iface.h"

/* Address of GSBI blocks */
#define MSM_GSBI0_PHYS		0xA1200000
#define MSM_GSBI1_PHYS		0xA1300000

/* GSBI QUPe devices */
#define MSM_GSBI0_QUP_PHYS	(MSM_GSBI0_PHYS + 0x80000)
#define MSM_GSBI1_QUP_PHYS	(MSM_GSBI1_PHYS + 0x80000)

#define A11S_TEST_BUS_SEL_ADDR (MSM_CSR_BASE + 0x518)
#define RBCPR_CLK_MUX_SEL (1 << 13)

/* Reset Address of RBCPR (Active Low)*/
#define RBCPR_SW_RESET_N       (MSM_CSR_BASE + 0x64)

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

static struct acpuclk_pdata msm7x27a_acpuclk_pdata = {
	.max_speed_delta_khz = 400000,
};

struct platform_device msm7x27a_device_acpuclk = {
	.name		= "acpuclk-7627",
	.id		= -1,
	.dev.platform_data = &msm7x27a_acpuclk_pdata,
};

static struct acpuclk_pdata msm7x27aa_acpuclk_pdata = {
	.max_speed_delta_khz = 504000,
};

struct platform_device msm7x27aa_device_acpuclk = {
	.name		= "acpuclk-7627",
	.id		= -1,
	.dev.platform_data = &msm7x27aa_acpuclk_pdata,
};

static struct acpuclk_pdata msm8625q_pdata = {
	.max_speed_delta_khz = 801600,
};

static struct acpuclk_pdata_8625q msm8625q_acpuclk_pdata = {
	.acpu_clk_data = &msm8625q_pdata,
	.pvs_voltage_uv = 1350000,
};

struct platform_device msm8625q_device_acpuclk = {
	.name		= "acpuclock-8625q",
	.id		= -1,
	.dev.platform_data = &msm8625q_acpuclk_pdata,
};

static struct acpuclk_pdata msm8625_acpuclk_pdata = {
	/* TODO: Need to update speed delta from H/w Team */
	.max_speed_delta_khz = 604800,
};

static struct acpuclk_pdata msm8625ab_acpuclk_pdata = {
	.max_speed_delta_khz = 801600,
};

struct platform_device msm8625_device_acpuclk = {
	.name		= "acpuclk-7627",
	.id		= -1,
	.dev.platform_data = &msm8625_acpuclk_pdata,
};

struct platform_device msm8625ab_device_acpuclk = {
	.name		= "acpuclk-7627",
	.id		= -1,
	.dev.platform_data = &msm8625ab_acpuclk_pdata,
};

struct platform_device msm_device_smd = {
	.name	= "msm_smd",
	.id	= -1,
};

static struct resource smd_8625_resource[] = {
	{
		.name   = "a9_m2a_0",
		.start  = MSM8625_INT_A9_M2A_0,
		.flags  = IORESOURCE_IRQ,
	},
	{
		.name   = "a9_m2a_5",
		.start  = MSM8625_INT_A9_M2A_5,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct smd_subsystem_config smd_8625_config_list[] = {
	{
		.irq_config_id = SMD_MODEM,
		.subsys_name = "modem",
		.edge = SMD_APPS_MODEM,

		.smd_int.irq_name = "a9_m2a_0",
		.smd_int.flags = IRQF_TRIGGER_RISING,
		.smd_int.irq_id = -1,
		.smd_int.device_name = "smd_dev",
		.smd_int.dev_id = 0,

		.smd_int.out_bit_pos =  1,
		.smd_int.out_base = (void __iomem *)MSM_CSR_BASE,
		.smd_int.out_offset = 0x400 + (0) * 4,

		.smsm_int.irq_name = "a9_m2a_5",
		.smsm_int.flags = IRQF_TRIGGER_RISING,
		.smsm_int.irq_id = -1,
		.smsm_int.device_name = "smsm_dev",
		.smsm_int.dev_id = 0,

		.smsm_int.out_bit_pos =  1,
		.smsm_int.out_base = (void __iomem *)MSM_CSR_BASE,
		.smsm_int.out_offset = 0x400 + (5) * 4,

	}
};

static struct smd_platform smd_8625_platform_data = {
	.num_ss_configs = ARRAY_SIZE(smd_8625_config_list),
	.smd_ss_configs = smd_8625_config_list,
};

struct platform_device msm8625_device_smd = {
	.name	= "msm_smd",
	.id	= -1,
	.resource = smd_8625_resource,
	.num_resources = ARRAY_SIZE(smd_8625_resource),
	.dev = {
		.platform_data = &smd_8625_platform_data,
	}
};

static struct resource resources_adsp[] = {
	{
		.start  = INT_ADSP_A9_A11,
		.end    = INT_ADSP_A9_A11,
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device msm_adsp_device = {
	.name           = "msm_adsp",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(resources_adsp),
	.resource       = resources_adsp,
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

static struct msm_pm_irq_calls msm7x27a_pm_irq_calls = {
	.irq_pending = msm_irq_pending,
	.idle_sleep_allowed = msm_irq_idle_sleep_allowed,
	.enter_sleep1 = msm_irq_enter_sleep1,
	.enter_sleep2 = msm_irq_enter_sleep2,
	.exit_sleep1 = msm_irq_exit_sleep1,
	.exit_sleep2 = msm_irq_exit_sleep2,
	.exit_sleep3 = msm_irq_exit_sleep3,
};

static struct msm_pm_irq_calls msm8625_pm_irq_calls = {
	.irq_pending = msm_gic_spi_ppi_pending,
	.idle_sleep_allowed = msm_gic_irq_idle_sleep_allowed,
	.enter_sleep1 = msm_gic_irq_enter_sleep1,
	.enter_sleep2 = msm_gic_irq_enter_sleep2,
	.exit_sleep1 = msm_gic_irq_exit_sleep1,
	.exit_sleep2 = msm_gic_irq_exit_sleep2,
	.exit_sleep3 = msm_gic_irq_exit_sleep3,
};

void msm_clk_dump_debug_info(void)
{
	pr_info("%s: GLBL_CLK_ENA: 0x%08X\n", __func__,
		readl_relaxed(MSM_CLK_CTL_BASE + 0x0));
	pr_info("%s: GLBL_CLK_STATE: 0x%08X\n", __func__,
		readl_relaxed(MSM_CLK_CTL_BASE + 0x4));
	pr_info("%s: GRP_NS_REG: 0x%08X\n", __func__,
		readl_relaxed(MSM_CLK_CTL_BASE + 0x84));
	pr_info("%s: CLK_HALT_STATEB: 0x%08X\n", __func__,
		readl_relaxed(MSM_CLK_CTL_BASE + 0x10C));
}

void __init msm_pm_register_irqs(void)
{
	if (cpu_is_msm8625() || cpu_is_msm8625q())
		msm_pm_set_irq_extns(&msm8625_pm_irq_calls);
	else
		msm_pm_set_irq_extns(&msm7x27a_pm_irq_calls);

}

static struct msm_pm_cpr_ops msm8625_pm_cpr_ops = {
	.cpr_suspend = msm_cpr_pm_suspend,
	.cpr_resume = msm_cpr_pm_resume,
};

void __init msm_pm_register_cpr_ops(void)
{
	/* CPR presents on revision >= v2.0 chipsets */
	if ((cpu_is_msm8625() &&
			SOCINFO_VERSION_MAJOR(socinfo_get_version()) >= 2)
			|| cpu_is_msm8625q())
		msm_pm_set_cpr_ops(&msm8625_pm_cpr_ops);
}

#define MSM_SDC1_BASE         0xA0400000
#define MSM_SDC2_BASE         0xA0500000
#define MSM_SDC3_BASE         0xA0600000
#define MSM_SDC4_BASE         0xA0700000
static struct resource resources_sdc1[] = {
	{
		.name	= "core_mem",
		.start	= MSM_SDC1_BASE,
		.end	= MSM_SDC1_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "core_irq",
		.start	= INT_SDC1_0,
		.end	= INT_SDC1_1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "dma_chnl",
		.start	= DMOV_SDC1_CHAN,
		.end	= DMOV_SDC1_CHAN,
		.flags	= IORESOURCE_DMA,
	},
	{
		.name	= "dma_crci",
		.start	= DMOV_SDC1_CRCI,
		.end	= DMOV_SDC1_CRCI,
		.flags	= IORESOURCE_DMA,
	}
};

static struct resource resources_sdc2[] = {
	{
		.name	= "core_mem",
		.start	= MSM_SDC2_BASE,
		.end	= MSM_SDC2_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "core_irq",
		.start	= INT_SDC2_0,
		.end	= INT_SDC2_1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "dma_chnl",
		.start	= DMOV_SDC2_CHAN,
		.end	= DMOV_SDC2_CHAN,
		.flags	= IORESOURCE_DMA,
	},
	{
		.name	= "dma_crci",
		.start	= DMOV_SDC2_CRCI,
		.end	= DMOV_SDC2_CRCI,
		.flags	= IORESOURCE_DMA,
	}
};

static struct resource resources_sdc3[] = {
	{
		.name   = "core_mem",
		.start	= MSM_SDC3_BASE,
		.end	= MSM_SDC3_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "core_irq",
		.start	= INT_SDC3_0,
		.end	= INT_SDC3_1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "dma_chnl",
		.start	= DMOV_NAND_CHAN,
		.end	= DMOV_NAND_CHAN,
		.flags	= IORESOURCE_DMA,
	},
	{
		.name	= "dma_crci",
		.start	= DMOV_SDC3_CRCI,
		.end	= DMOV_SDC3_CRCI,
		.flags	= IORESOURCE_DMA,
	},
};

static struct resource resources_sdc4[] = {
	{
		.name	= "core_mem",
		.start	= MSM_SDC4_BASE,
		.end	= MSM_SDC4_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "core_irq",
		.start	= INT_SDC4_0,
		.end	= INT_SDC4_1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "dma_chnl",
		.start	= DMOV_SDC4_CHAN,
		.end	= DMOV_SDC4_CHAN,
		.flags	= IORESOURCE_DMA,
	},
	{
		.name	= "dma_crci",
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
static int apps_reset;
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
	.dev = {
		.platform_data = &apps_reset,
	},
};

struct platform_device msm7x27a_device_vfe = {
	.name           = "msm_vfe",
	.id             = 0,
};

#endif

/* Command sequence for simple WFI */
static uint8_t spm_wfi_cmd_sequence[] __initdata = {
	0x04, 0x03, 0x04, 0x0f,
};

/* Command sequence for GDFS, this won't send any interrupt to the modem */
static uint8_t spm_pc_without_modem[] __initdata = {
	0x20, 0x00, 0x30, 0x10,
	0x03, 0x1e, 0x0e, 0x3e,
	0x4e, 0x4e, 0x4e, 0x4e,
	0x4e, 0x4e, 0x4e, 0x4e,
	0x4e, 0x4e, 0x4e, 0x4e,
	0x4e, 0x4e, 0x4e, 0x4e,
	0x2E, 0x0f,
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
		.cmd = spm_pc_without_modem,
	},
	[2] = {
		.mode = MSM_SPM_MODE_POWER_COLLAPSE,
		.notify_rpm = false,
		.cmd = spm_pc_without_modem,
	},
	[3] = {
		.mode = MSM_SPM_MODE_POWER_COLLAPSE,
		.notify_rpm = false,
		.cmd = spm_pc_without_modem,
	},
};

static struct msm_spm_platform_data msm_spm_data[] __initdata = {
	[0] = {
		.reg_base_addr = MSM_SAW0_BASE,
		.reg_init_values[MSM_SPM_REG_SAW2_CFG] = 0x0,
		.reg_init_values[MSM_SPM_REG_SAW2_SPM_CTL] = 0x01,
		.num_modes = ARRAY_SIZE(msm_spm_seq_list),
		.modes = msm_spm_seq_list,
	},
	[1] = {
		.reg_base_addr = MSM_SAW1_BASE,
		.reg_init_values[MSM_SPM_REG_SAW2_CFG] = 0x0,
		.reg_init_values[MSM_SPM_REG_SAW2_SPM_CTL] = 0x01,
		.num_modes = ARRAY_SIZE(msm_spm_seq_list),
		.modes = msm_spm_seq_list,
	},
	[2] = {
		.reg_base_addr = MSM_SAW2_BASE,
		.reg_init_values[MSM_SPM_REG_SAW2_CFG] = 0x0,
		.reg_init_values[MSM_SPM_REG_SAW2_SPM_CTL] = 0x01,
		.num_modes = ARRAY_SIZE(msm_spm_seq_list),
		.modes = msm_spm_seq_list,
	},
	[3] = {
		.reg_base_addr = MSM_SAW3_BASE,
		.reg_init_values[MSM_SPM_REG_SAW2_CFG] = 0x0,
		.reg_init_values[MSM_SPM_REG_SAW2_SPM_CTL] = 0x01,
		.num_modes = ARRAY_SIZE(msm_spm_seq_list),
		.modes = msm_spm_seq_list,
	},
};

void __init msm8x25_spm_device_init(void)
{
	msm_spm_init(msm_spm_data, ARRAY_SIZE(msm_spm_data));
}

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

struct platform_device msm_lcdc_device = {
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
	.idle_timeout = HZ,
	.strtstp_sleepwake = true,
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
	if (cpu_is_msm7x25a() || cpu_is_msm7x25aa() || cpu_is_msm7x25ab()) {
		kgsl_3d0_pdata.num_levels = 2;
		kgsl_3d0_pdata.pwrlevel[0].gpu_freq = 133330000;
		kgsl_3d0_pdata.pwrlevel[0].bus_freq = 160000000;
		kgsl_3d0_pdata.pwrlevel[1].gpu_freq = 96000000;
		kgsl_3d0_pdata.pwrlevel[1].bus_freq = 0;
	}
}

void __init msm8x25_kgsl_3d0_init(void)
{
	if (cpu_is_msm8625() || cpu_is_msm8625q()) {
		kgsl_3d0_pdata.idle_timeout = HZ/5;
		kgsl_3d0_pdata.strtstp_sleepwake = false;

		if (SOCINFO_VERSION_MAJOR(socinfo_get_version()) >= 2)
			/* 8x25 v2.0 & above supports a higher GPU frequency */
			kgsl_3d0_pdata.pwrlevel[0].gpu_freq = 320000000;
		else
			kgsl_3d0_pdata.pwrlevel[0].gpu_freq = 300000000;

		kgsl_3d0_pdata.pwrlevel[0].bus_freq = 200000000;
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
	FS_PCOM(FS_GFX3D,  "vdd", "kgsl-3d0.0"),
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

static struct resource msm8625_resources_adsp[] = {
	{
		.start  = MSM8625_INT_ADSP_A9_A11,
		.end    = MSM8625_INT_ADSP_A9_A11,
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device msm8625_device_adsp = {
	.name           = "msm_adsp",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(msm8625_resources_adsp),
	.resource       = msm8625_resources_adsp,
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
struct platform_device msm8625_gsbi0_qup_i2c_device = {
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
struct platform_device msm8625_gsbi1_qup_i2c_device = {
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
		.name	= "core_mem",
		.start	= MSM_SDC1_BASE,
		.end	= MSM_SDC1_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "core_irq",
		.start	= MSM8625_INT_SDC1_0,
		.end	= MSM8625_INT_SDC1_1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "dma_chnl",
		.start	= DMOV_SDC1_CHAN,
		.end	= DMOV_SDC1_CHAN,
		.flags	= IORESOURCE_DMA,
	},
	{
		.name	= "dma_crci",
		.start	= DMOV_SDC1_CRCI,
		.end	= DMOV_SDC1_CRCI,
		.flags	= IORESOURCE_DMA,
	}
};

static struct resource msm8625_resources_sdc2[] = {
	{
		.name   = "core_mem",
		.start	= MSM_SDC2_BASE,
		.end	= MSM_SDC2_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "core_irq",
		.start	= MSM8625_INT_SDC2_0,
		.end	= MSM8625_INT_SDC2_1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "dma_chnl",
		.start	= DMOV_SDC2_CHAN,
		.end	= DMOV_SDC2_CHAN,
		.flags	= IORESOURCE_DMA,
	},
	{
		.name	= "dma_crci",
		.start	= DMOV_SDC2_CRCI,
		.end	= DMOV_SDC2_CRCI,
		.flags	= IORESOURCE_DMA,
	}
};

static struct resource msm8625_resources_sdc3[] = {
	{
		.name   = "core_mem",
		.start	= MSM_SDC3_BASE,
		.end	= MSM_SDC3_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "core_irq",
		.start	= MSM8625_INT_SDC3_0,
		.end	= MSM8625_INT_SDC3_1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "dma_chnl",
		.start	= DMOV_NAND_CHAN,
		.end	= DMOV_NAND_CHAN,
		.flags	= IORESOURCE_DMA,
	},
	{
		.name	= "dma_crci",
		.start	= DMOV_SDC3_CRCI,
		.end	= DMOV_SDC3_CRCI,
		.flags	= IORESOURCE_DMA,
	},
};

static struct resource msm8625_resources_sdc4[] = {
	{
		.name   = "core_mem",
		.start	= MSM_SDC4_BASE,
		.end	= MSM_SDC4_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "core_irq",
		.start	= MSM8625_INT_SDC4_0,
		.end	= MSM8625_INT_SDC4_1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "dma_chnl",
		.start	= DMOV_SDC4_CHAN,
		.end	= DMOV_SDC4_CHAN,
		.flags	= IORESOURCE_DMA,
	},
	{
		.name	= "dma_crci",
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

	if (cpu_is_msm8625() || cpu_is_msm8625q())
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

	if (cpu_is_msm8625() || cpu_is_msm8625q())
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

static struct resource msm8625_mipi_dsi_resources[] = {
	{
		.name   = "mipi_dsi",
		.start  = MIPI_DSI_HW_BASE,
		.end    = MIPI_DSI_HW_BASE + 0x000F0000 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.start  = MSM8625_INT_DSI_IRQ,
		.end    = MSM8625_INT_DSI_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device msm8625_mipi_dsi_device = {
	.name   = "mipi_dsi",
	.id     = 1,
	.num_resources  = ARRAY_SIZE(msm8625_mipi_dsi_resources),
	.resource       = msm8625_mipi_dsi_resources,
};

static struct resource msm8625_mdp_resources[] = {
	{
		.name   = "mdp",
		.start  = MDP_BASE,
		.end    = MDP_BASE + 0x000F1008 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.start  = MSM8625_INT_MDP,
		.end    = MSM8625_INT_MDP,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device msm8625_mdp_device = {
	.name   = "mdp",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(msm8625_mdp_resources),
	.resource       = msm8625_mdp_resources,
};

struct platform_device mipi_dsi_device;

void __init msm_fb_register_device(char *name, void *data)
{
	if (!strncmp(name, "mdp", 3)) {
		if (cpu_is_msm8625() || cpu_is_msm8625q())
			msm_register_device(&msm8625_mdp_device, data);
		else
			msm_register_device(&msm_mdp_device, data);
	} else if (!strncmp(name, "mipi_dsi", 8)) {
		if (cpu_is_msm8625() || cpu_is_msm8625q()) {
			msm_register_device(&msm8625_mipi_dsi_device, data);
			mipi_dsi_device = msm8625_mipi_dsi_device;
		} else {
			msm_register_device(&msm_mipi_dsi_device, data);
			mipi_dsi_device = msm_mipi_dsi_device;
		}
	} else if (!strncmp(name, "lcdc", 4)) {
			msm_register_device(&msm_lcdc_device, data);
	} else {
		printk(KERN_ERR "%s: unknown device! %s\n", __func__, name);
	}
}

static struct resource msm8625_kgsl_3d0_resources[] = {
	{
		.name  = KGSL_3D0_REG_MEMORY,
		.start = 0xA0000000,
		.end = 0xA001ffff,
		.flags = IORESOURCE_MEM,
	},
	{
		.name = KGSL_3D0_IRQ,
		.start = MSM8625_INT_GRAPHICS,
		.end = MSM8625_INT_GRAPHICS,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device msm8625_kgsl_3d0 = {
	.name = "kgsl-3d0",
	.id = 0,
	.num_resources = ARRAY_SIZE(msm8625_kgsl_3d0_resources),
	.resource = msm8625_kgsl_3d0_resources,
	.dev = {
		.platform_data = &kgsl_3d0_pdata,
	},
};

static struct resource pl310_resources[] = {
	{
		.start = 0xC0400000,
		.end   = 0xC0400000 + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
	{
		.name   = "l2_irq",
		.start  = MSM8625_INT_SC_SICL2PERFMONIRPTREQ,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device pl310_erp_device = {
	.name           = "pl310_erp",
	.id             = -1,
	.resource       = pl310_resources,
	.num_resources  = ARRAY_SIZE(pl310_resources),
};

enum {
	MSM8625,
	MSM8625A,
	MSM8625AB,
};

static int __init msm8625_cpu_id(void)
{
	int raw_id, cpu;

	raw_id = socinfo_get_raw_id();
	switch (raw_id) {
	/* Part number for 1GHz part */
	case 0x770:
	case 0x771:
	case 0x77C:
	case 0x780:
	case 0x785: /* Edge-only MSM8125-0 */
	case 0x8D0:
		cpu = MSM8625;
		break;
	/* Part number for 1.2GHz part */
	case 0x773:
	case 0x774:
	case 0x781:
	case 0x8D1:
		cpu = MSM8625A;
		break;
	case 0x775:
	case 0x776:
	case 0x779:
	case 0x77D:
	case 0x782:
	case 0x8D2:
		cpu = MSM8625AB;
		break;
	default:
		pr_err("Invalid Raw ID\n");
		return -ENODEV;
	}
	return cpu;
}

static struct resource cpr_resources[] = {
	{
		.start = MSM8625_INT_CPR_IRQ0,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = MSM8625_CPR_PHYS,
		.end = MSM8625_CPR_PHYS + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
};

/**
 * These are various Vdd levels supported by PMIC
 */
static uint32_t msm_c2_pmic_mv[] __initdata = {
	1350000, 1337500, 1325000, 1312500, 1300000,
	1287500, 1275000, 1262500, 1250000, 1237500,
	1225000, 1212500, 1200000, 1187500, 1175000,
	1162500, 1150000, 1137500, 1125000, 1112500,
	1100000, 1087500, 1075000, 1062500, 0,
	0,	 0,	  0,	   0,	    0,
	0, 1050000,
};

/**
 * This data will be based on CPR mode of operation
 */
static struct msm_cpr_mode msm_cpr_mode_data[] = {
	[NORMAL_MODE] = {
			.ring_osc_data = {
				{0, },
				{0, },
				{0, },
				{0, },
				{0, },
				{0, },
				{0, },
				{0, },
			},
			.ring_osc = 0,
			.step_quot = ~0,
			.tgt_volt_offset = 0,
			.nom_Vmax = 1350000,
			.nom_Vmin = 1250000,
			.calibrated_uV = 1100000,
	},
	[TURBO_MODE] = {
			.ring_osc_data = {
				{0, },
				{0, },
				{0, },
				{0, },
				{0, },
				{0, },
				{0, },
				{0, },
			},
			.ring_osc = 0,
			.step_quot = ~0,
			.tgt_volt_offset = 0,
			.turbo_Vmax = 1350000,
			.turbo_Vmin = 1150000,
			.nom_Vmax = 1350000,
			.nom_Vmin = 1150000,
			.calibrated_uV = 1300000,
	},
};

static uint32_t
msm_cpr_get_quot(uint32_t max_quot, uint32_t max_freq, uint32_t new_freq)
{
	uint32_t quot;

	/* This formula is as per chip characterization data */
	quot = max_quot - (((max_freq - new_freq) * 7) / 10);

	return quot;
}

static void msm_cpr_clk_enable(void)
{
	uint32_t reg_val;

	/* Select TCXO (19.2MHz) as clock source */
	reg_val = readl_relaxed(A11S_TEST_BUS_SEL_ADDR);
	reg_val |= RBCPR_CLK_MUX_SEL;
	writel_relaxed(reg_val, A11S_TEST_BUS_SEL_ADDR);

	/* Get CPR out of reset */
	writel_relaxed(0x1, RBCPR_SW_RESET_N);
}

static struct msm_cpr_config msm_cpr_pdata = {
	.ref_clk_khz = 19200,
	.delay_us = 25000,
	.irq_line = 0,
	.cpr_mode_data = msm_cpr_mode_data,
	.tgt_count_div_N = 1,
	.floor = 0,
	.ceiling = 40,
	.sw_vlevel = 20,
	.up_threshold = 1,
	.dn_threshold = 3,
	.up_margin = 0,
	.dn_margin = 0,
	.max_nom_freq = 700800,
	.max_freq = 1401600,
	.max_quot = 0,
	.disable_cpr = false,
	.step_size = 12500,
	.get_quot = msm_cpr_get_quot,
	.clk_enable = msm_cpr_clk_enable,
};

static struct platform_device msm8625_device_cpr = {
	.name           = "msm-cpr",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(cpr_resources),
	.resource       = cpr_resources,
	.dev = {
		.platform_data = &msm_cpr_pdata,
	},
};

static struct platform_device msm8625_vp_device = {
	.name           = "vp-regulator",
	.id             = -1,
};

static void __init msm_cpr_init(void)
{
	struct cpr_info_type *cpr_info = NULL;
	uint8_t ring_osc = 0;

	cpr_info = kzalloc(sizeof(struct cpr_info_type), GFP_KERNEL);
	if (!cpr_info) {
		pr_err("%s: Out of memory %d\n", __func__, -ENOMEM);
		return;
	}

	msm_smem_get_cpr_info(cpr_info);
	msm_cpr_pdata.disable_cpr = cpr_info->disable_cpr;

	/**
	 * Set the ring_osc based on efuse BIT(0)
	 * CPR_fuse[0] = 0 selects 2nd RO (010)
	 * CPR_fuse[0] = 1 select  3rd RO (011)
	 */
	if (cpr_info->ring_osc == 0x0)
		ring_osc = 0x2;
	else if (cpr_info->ring_osc == 0x1)
		ring_osc = 0x3;

	msm_cpr_mode_data[TURBO_MODE].ring_osc = ring_osc;
	msm_cpr_mode_data[NORMAL_MODE].ring_osc = ring_osc;

	/* GCNT = 1000 nsec/52nsec (@TCX0=19.2Mhz) = 19.2 */
	msm_cpr_mode_data[TURBO_MODE].ring_osc_data[ring_osc].gcnt = 19;
	msm_cpr_mode_data[NORMAL_MODE].ring_osc_data[ring_osc].gcnt = 19;

	/**
	 * The scaling factor and offset are as per chip characterization data
	 * This formula is used since available fuse bits in the chip are not
	 * enough to represent the value of maximum quot
	 */
	msm_cpr_pdata.max_quot = cpr_info->turbo_quot * 10 + 600;
	/**
	 * Fused Quot value for 1.2GHz on a 1.2GHz part is lower than
	 * the quot value calculated using the scaling factor formula for
	 * 1.2GHz when running on a 1.4GHz part. So, prop up the Quot for
	 * a 1.2GHz part by a chip characterization recommended value.
	 * Ditto for a 1.0GHz part.
	 */
	if (msm8625_cpu_id() == MSM8625A) {
		msm_cpr_pdata.max_quot += 30;
		if (msm_cpr_pdata.max_quot > 1400)
			msm_cpr_pdata.max_quot = 1400;
	} else if (msm8625_cpu_id() == MSM8625) {
		msm_cpr_pdata.max_quot += 50;
		if (msm_cpr_pdata.max_quot > 1350)
			msm_cpr_pdata.max_quot = 1350;
	}

	/**
	 * Bits 4:0 of pvs_fuse provide mapping to the safe boot up voltage.
	 * Boot up mode is by default Turbo.
	 */
	msm_cpr_mode_data[TURBO_MODE].calibrated_uV =
				msm_c2_pmic_mv[cpr_info->pvs_fuse & 0x1F];

	if ((cpr_info->floor_fuse & 0x3) == 0x0) {
		msm_cpr_mode_data[TURBO_MODE].nom_Vmin = 1000000;
		msm_cpr_mode_data[TURBO_MODE].turbo_Vmin = 1100000;
	} else if ((cpr_info->floor_fuse & 0x3) == 0x1) {
		msm_cpr_mode_data[TURBO_MODE].nom_Vmin = 1050000;
		msm_cpr_mode_data[TURBO_MODE].turbo_Vmin = 1100000;
	} else if ((cpr_info->floor_fuse & 0x3) == 0x2) {
		msm_cpr_mode_data[TURBO_MODE].nom_Vmin = 1100000;
		msm_cpr_mode_data[TURBO_MODE].turbo_Vmin = 1100000;
	}

	pr_debug("%s: cpr: ring_osc: 0x%x\n", __func__,
		msm_cpr_mode_data[TURBO_MODE].ring_osc);
	pr_debug("%s: cpr: turbo_quot: 0x%x\n", __func__, cpr_info->turbo_quot);
	pr_debug("%s: cpr: pvs_fuse: 0x%x\n", __func__, cpr_info->pvs_fuse);
	pr_debug("%s: cpr: floor_fuse: 0x%x\n", __func__, cpr_info->floor_fuse);
	pr_debug("%s: cpr: nom_Vmin: %d, turbo_Vmin: %d\n", __func__,
		msm_cpr_mode_data[TURBO_MODE].nom_Vmin,
		msm_cpr_mode_data[TURBO_MODE].turbo_Vmin);
	kfree(cpr_info);

	if (msm8625_cpu_id() == MSM8625A)
		msm_cpr_pdata.max_freq = 1209600;
	else if (msm8625_cpu_id() == MSM8625)
		msm_cpr_pdata.max_freq = 1008000;

	if (machine_is_qrd_skud_prime() || cpu_is_msm8625q())
		msm_cpr_pdata.step_size = 6250;
	platform_device_register(&msm8625_vp_device);
	platform_device_register(&msm8625_device_cpr);
}

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


static int __init msm_gpio_config_gps(void)
{
	unsigned int gps_gpio = 7;
	int ret = 0;

	if (!machine_is_msm8625_evb())
		return ret;

	ret = gpio_tlmm_config(GPIO_CFG(gps_gpio, 0, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	if (ret < 0) {
		pr_err("gpio tlmm failed for gpio-%d\n", gps_gpio);
		return ret;
	}

	ret = gpio_request(gps_gpio, "gnss-gpio");
	if (ret < 0) {
		pr_err("failed to request gpio-%d\n", gps_gpio);
		return ret;
	}

	ret = gpio_direction_input(gps_gpio);
	if (ret < 0) {
		pr_err("failed to change direction for gpio-%d\n", gps_gpio);
		return ret;
	}

	ret = gpio_export(gps_gpio, true);
	if (ret < 0)
		pr_err("failed to export gpio for user\n");

	return ret;
}

static int __init msm_acpuclock_init(bool flag)
{
	struct cpr_info_type *acpu_info = NULL;
	acpu_info = kzalloc(sizeof(struct cpr_info_type), GFP_KERNEL);
	if (!acpu_info) {
		pr_err("%s: Out of memory %d\n", __func__, -ENOMEM);
		return -ENOMEM;
	}
	msm_smem_get_cpr_info(acpu_info);
	msm8625q_acpuclk_pdata.pvs_voltage_uv =
			msm_c2_pmic_mv[acpu_info->pvs_fuse & 0x1F];
	kfree(acpu_info);
	msm8625q_acpuclk_pdata.flag = flag;
	return 0;
}

int __init msm7x2x_misc_init(void)
{
	if (machine_is_msm8625_rumi3()) {
		msm_clock_init(&msm8625_dummy_clock_init_data);
		msm_cpr_init();
		return 0;
	}

	msm_clock_init(&msm7x27a_clock_init_data);
	if (cpu_is_msm7x27aa() || cpu_is_msm7x25ab())
		platform_device_register(&msm7x27aa_device_acpuclk);
	else if (cpu_is_msm8625q()) {
		msm_acpuclock_init(1);
		platform_device_register(&msm8625q_device_acpuclk);
	} else if (cpu_is_msm8625()) {
		if (machine_is_qrd_skud_prime()) {
			msm_acpuclock_init(0);
			platform_device_register(&msm8625q_device_acpuclk);
		} else if (msm8625_cpu_id() == MSM8625)
			platform_device_register(&msm7x27aa_device_acpuclk);
		else if (msm8625_cpu_id() == MSM8625A)
			platform_device_register(&msm8625_device_acpuclk);
		else if (msm8625_cpu_id() == MSM8625AB)
			platform_device_register(&msm8625ab_device_acpuclk);
	} else {
		platform_device_register(&msm7x27a_device_acpuclk);
	}

	if (cpu_is_msm8625() || (cpu_is_msm8625q() &&
			SOCINFO_VERSION_MAJOR(socinfo_get_version()) >= 2))
		msm_cpr_init();

	if (!cpu_is_msm8625() && !cpu_is_msm8625q())
		pl310_resources[1].start = SC_SICL2PERFMONIRPTREQ;

	platform_device_register(&pl310_erp_device);

	if (msm_gpio_config_gps() < 0)
		pr_err("Error for gpio config for GPS gpio\n");

	return 0;
}

#ifdef CONFIG_CACHE_L2X0
static int __init msm7x27x_cache_init(void)
{
	int aux_ctrl = 0;
	int pctrl = 0;

	/* Way Size 010(0x2) 32KB */
	aux_ctrl = (0x1 << L2X0_AUX_CTRL_SHARE_OVERRIDE_SHIFT) | \
		   (0x2 << L2X0_AUX_CTRL_WAY_SIZE_SHIFT) | \
		   (0x1 << L2X0_AUX_CTRL_EVNT_MON_BUS_EN_SHIFT);

	if (cpu_is_msm8625() || cpu_is_msm8625q()) {
		/* Way Size 011(0x3) 64KB */
		aux_ctrl |= (0x3 << L2X0_AUX_CTRL_WAY_SIZE_SHIFT) | \
			    (0x1 << L2X0_AUX_CTRL_DATA_PREFETCH_SHIFT) | \
			    (0X1 << L2X0_AUX_CTRL_INSTR_PREFETCH_SHIFT) | \
			    (0x1 << L2X0_AUX_CTRL_L2_FORCE_NWA_SHIFT);

		/* Write Prefetch Control settings */
		pctrl = readl_relaxed(MSM_L2CC_BASE + L2X0_PREFETCH_CTRL);
		pctrl |= (0x3 << L2X0_PREFETCH_CTRL_OFFSET_SHIFT) | \
			 (0x1 << L2X0_PREFETCH_CTRL_WRAP8_INC_SHIFT) | \
			 (0x1 << L2X0_PREFETCH_CTRL_WRAP8_SHIFT);
		writel_relaxed(pctrl , MSM_L2CC_BASE + L2X0_PREFETCH_CTRL);
	}

	l2x0_init(MSM_L2CC_BASE, aux_ctrl, L2X0_AUX_CTRL_MASK);
	if (cpu_is_msm8625() || cpu_is_msm8625q()) {
		pctrl = readl_relaxed(MSM_L2CC_BASE + L2X0_PREFETCH_CTRL);
		pr_info("Prfetch Ctrl: 0x%08x\n", pctrl);
	}

	return 0;
}
#else
static int __init msm7x27x_cache_init(void){ return 0; }
#endif

void __init msm_common_io_init(void)
{
	msm_map_common_io();
	if (socinfo_init() < 0)
		pr_err("socinfo_init() failed!\n");
	msm7x27x_cache_init();
}

void __init msm8625_init_irq(void)
{
	msm_gic_irq_extn_init();
	gic_init(0, GIC_PPI_START, MSM_QGIC_DIST_BASE,
			(void *)MSM_QGIC_CPU_BASE);
}

void __init msm8625_map_io(void)
{
	msm_map_msm8625_io();

	if (socinfo_init() < 0)
		pr_err("socinfo_init() failed!\n");
	msm7x27x_cache_init();
}

static int msm7627a_init_gpio(void)
{
	if (cpu_is_msm8625() || cpu_is_msm8625q())
		platform_device_register(&msm8625_device_gpio);
	else
		platform_device_register(&msm_device_gpio);
	return 0;
}
postcore_initcall(msm7627a_init_gpio);

static int msm7627a_panic_handler(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	msm_clk_dump_debug_info();
	flush_cache_all();
	outer_flush_all();
	return NOTIFY_DONE;
}

static struct notifier_block panic_handler = {
	.notifier_call = msm7627a_panic_handler,
	.priority = INT_MAX,
};

static int __init panic_register(void)
{
	atomic_notifier_chain_register(&panic_notifier_list,
			&panic_handler);
	return 0;
}
module_init(panic_register);
