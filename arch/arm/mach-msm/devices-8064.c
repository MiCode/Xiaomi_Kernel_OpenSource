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
 *
 */

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/msm_rotator.h>
#include <linux/gpio.h>
#include <linux/clkdev.h>
#include <linux/dma-mapping.h>
#include <linux/coresight.h>
#include <linux/avtimer.h>
#include <mach/irqs-8064.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <mach/usbdiag.h>
#include <mach/msm_serial_hs_lite.h>
#include <mach/msm_sps.h>
#include <mach/dma.h>
#include <mach/msm_dsps.h>
#include <mach/clk-provider.h>
#include <sound/msm-dai-q6.h>
#include <sound/apr_audio.h>
#include <mach/msm_tsif.h>
#include <mach/msm_tspp.h>
#include <mach/msm_bus_board.h>
#include <mach/rpm.h>
#include <mach/mdm2.h>
#include <mach/msm_smd.h>
#include <mach/msm_dcvs.h>
#include <mach/msm_rtb.h>
#include <linux/msm_ion.h>
#include "clock.h"
#include "pm.h"
#include "devices.h"
#include "footswitch.h"
#include "msm_watchdog.h"
#include "rpm_stats.h"
#include "rpm_log.h"
#include <mach/mpm.h>
#include <mach/iommu_domains.h>
#include <mach/msm_cache_dump.h>
#include "pm.h"

/* Address of GSBI blocks */
#define MSM_GSBI1_PHYS		0x12440000
#define MSM_GSBI2_PHYS		0x12480000
#define MSM_GSBI3_PHYS		0x16200000
#define MSM_GSBI4_PHYS		0x16300000
#define MSM_GSBI5_PHYS		0x1A200000
#define MSM_GSBI6_PHYS		0x16500000
#define MSM_GSBI7_PHYS		0x16600000

/* GSBI UART devices */
#define MSM_UART1DM_PHYS	(MSM_GSBI1_PHYS + 0x10000)
#define MSM_UART2DM_PHYS	(MSM_GSBI2_PHYS + 0x10000)
#define MSM_UART3DM_PHYS	(MSM_GSBI3_PHYS + 0x40000)
#define MSM_UART4DM_PHYS	(MSM_GSBI4_PHYS + 0x40000)
#define MSM_UART5DM_PHYS	(MSM_GSBI5_PHYS + 0x40000)
#define MSM_UART6DM_PHYS	(MSM_GSBI6_PHYS + 0x40000)
#define MSM_UART7DM_PHYS	(MSM_GSBI7_PHYS + 0x40000)

/* GSBI QUP devices */
#define MSM_GSBI1_QUP_PHYS	(MSM_GSBI1_PHYS + 0x20000)
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

/* Address of HS USBOTG1 */
#define MSM_HSUSB1_PHYS		0x12500000
#define MSM_HSUSB1_SIZE		SZ_4K

/* Address of HS USB3 */
#define MSM_HSUSB3_PHYS		0x12520000
#define MSM_HSUSB3_SIZE		SZ_4K

/* Address of HS USB4 */
#define MSM_HSUSB4_PHYS		0x12530000
#define MSM_HSUSB4_SIZE		SZ_4K

/* Address of PCIE20 PARF */
#define PCIE20_PARF_PHYS   0x1b600000
#define PCIE20_PARF_SIZE   SZ_128

/* Address of PCIE20 ELBI */
#define PCIE20_ELBI_PHYS   0x1b502000
#define PCIE20_ELBI_SIZE   SZ_256

/* Address of PCIE20 */
#define PCIE20_PHYS   0x1b500000
#define PCIE20_SIZE   SZ_4K
#define MSM8064_PC_CNTR_PHYS	(APQ8064_IMEM_PHYS + 0x664)
#define MSM8064_PC_CNTR_SIZE		0x40
#define MSM8064_RPM_MASTER_STATS_BASE	0x10BB00
/* avtimer */
#define AVTIMER_MSW_PHYSICAL_ADDRESS 0x2800900C
#define AVTIMER_LSW_PHYSICAL_ADDRESS 0x28009008

static struct resource msm8064_resources_pccntr[] = {
	{
		.start	= MSM8064_PC_CNTR_PHYS,
		.end	= MSM8064_PC_CNTR_PHYS + MSM8064_PC_CNTR_SIZE,
		.flags	= IORESOURCE_MEM,
	},
};

static uint32_t msm_pm_cp15_regs[] = {0x4501, 0x5501, 0x6501, 0x7501, 0x0500};

static struct msm_pm_init_data_type msm_pm_data = {
	.retention_calls_tz = true,
	.cp15_data.save_cp15 = true,
	.cp15_data.qsb_pc_vdd = 0x98,
	.cp15_data.reg_data = &msm_pm_cp15_regs[0],
	.cp15_data.reg_saved_state_size = ARRAY_SIZE(msm_pm_cp15_regs),
};

struct platform_device msm8064_pm_8x60 = {
	.name		= "pm-8x60",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(msm8064_resources_pccntr),
	.resource	= msm8064_resources_pccntr,
	.dev = {
		.platform_data = &msm_pm_data,
	},
};

static struct msm_pm_sleep_status_data msm_pm_slp_sts_data = {
	.base_addr = MSM_ACC0_BASE + 0x08,
	.cpu_offset = MSM_ACC1_BASE - MSM_ACC0_BASE,
	.mask = 1UL << 13,
};
struct platform_device msm8064_cpu_slp_status = {
	.name		= "cpu_slp_status",
	.id		= -1,
	.dev = {
		.platform_data = &msm_pm_slp_sts_data,
	},
};

static struct msm_watchdog_pdata msm_watchdog_pdata = {
	.pet_time = 10000,
	.bark_time = 11000,
	.has_secure = true,
	.needs_expired_enable = true,
	.base = MSM_TMR0_BASE + WDT0_OFFSET,
};

static struct resource msm_watchdog_resources[] = {
	{
		.start	= WDT0_ACCSCSSNBARK_INT,
		.end	= WDT0_ACCSCSSNBARK_INT,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm8064_device_watchdog = {
	.name = "msm_watchdog",
	.id = -1,
	.dev = {
		.platform_data = &msm_watchdog_pdata,
	},
	.num_resources	= ARRAY_SIZE(msm_watchdog_resources),
	.resource	= msm_watchdog_resources,
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

struct platform_device apq8064_device_dmov = {
	.name	= "msm_dmov",
	.id	= -1,
	.resource = msm_dmov_resource,
	.num_resources = ARRAY_SIZE(msm_dmov_resource),
	.dev = {
		.platform_data = &msm_dmov_pdata,
	},
};

static struct resource resources_uart_gsbi1[] = {
	{
		.start	= APQ8064_GSBI1_UARTDM_IRQ,
		.end	= APQ8064_GSBI1_UARTDM_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= MSM_UART1DM_PHYS,
		.end	= MSM_UART1DM_PHYS + PAGE_SIZE - 1,
		.name	= "uartdm_resource",
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= MSM_GSBI1_PHYS,
		.end	= MSM_GSBI1_PHYS + PAGE_SIZE - 1,
		.name	= "gsbi_resource",
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device apq8064_device_uart_gsbi1 = {
	.name	= "msm_serial_hsl",
	.id	= 1,
	.num_resources	= ARRAY_SIZE(resources_uart_gsbi1),
	.resource	= resources_uart_gsbi1,
};

static struct resource resources_uart_gsbi2[] = {
	{
		.start	= APQ8064_GSBI2_UARTDM_IRQ,
		.end	= APQ8064_GSBI2_UARTDM_IRQ,
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

static struct msm_serial_hslite_platform_data uart_gsbi2_pdata = {
	.line		= 0,
};

struct platform_device apq8064_device_uart_gsbi2 = {
	.name	= "msm_serial_hsl",
	.id	= 3,
	.num_resources	= ARRAY_SIZE(resources_uart_gsbi2),
	.resource	= resources_uart_gsbi2,
	.dev.platform_data = &uart_gsbi2_pdata,
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
	{
		.name	= "i2c_clk",
		.start	= 9,
		.end	= 9,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "i2c_sda",
		.start	= 8,
		.end	= 8,
		.flags	= IORESOURCE_IO,
	},
};

static struct resource resources_qup_i2c_gsbi1[] = {
	{
		.name	= "gsbi_qup_i2c_addr",
		.start	= MSM_GSBI1_PHYS,
		.end	= MSM_GSBI1_PHYS + 4 - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "qup_phys_addr",
		.start	= MSM_GSBI1_QUP_PHYS,
		.end	= MSM_GSBI1_QUP_PHYS + MSM_QUP_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "qup_err_intr",
		.start	= APQ8064_GSBI1_QUP_IRQ,
		.end	= APQ8064_GSBI1_QUP_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "i2c_clk",
		.start	= 21,
		.end	= 21,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "i2c_sda",
		.start	= 20,
		.end	= 20,
		.flags	= IORESOURCE_IO,
	},
};

struct platform_device apq8064_device_qup_i2c_gsbi1 = {
	.name		= "qup_i2c",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(resources_qup_i2c_gsbi1),
	.resource	= resources_qup_i2c_gsbi1,
};

struct platform_device apq8064_device_qup_i2c_gsbi3 = {
	.name		= "qup_i2c",
	.id		= 3,
	.num_resources	= ARRAY_SIZE(resources_qup_i2c_gsbi3),
	.resource	= resources_qup_i2c_gsbi3,
};

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
	{
		.name	= "i2c_clk",
		.start	= 11,
		.end	= 11,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "i2c_sda",
		.start	= 10,
		.end	= 10,
		.flags	= IORESOURCE_IO,
	},
};

struct platform_device apq8064_device_qup_i2c_gsbi4 = {
	.name		= "qup_i2c",
	.id		= 4,
	.num_resources	= ARRAY_SIZE(resources_qup_i2c_gsbi4),
	.resource	= resources_qup_i2c_gsbi4,
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

static struct msm_serial_hslite_platform_data uart_gsbi4_pdata = {
	.line		= 2,
};

struct platform_device apq8064_device_uart_gsbi4 = {
	.name	= "msm_serial_hsl",
	.id	= 4,
	.num_resources	= ARRAY_SIZE(resources_uart_gsbi4),
	.resource	= resources_uart_gsbi4,
	.dev.platform_data = &uart_gsbi4_pdata,
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

static struct resource resources_qup_spi_gsbi6[] = {
	{
		.name   = "spi_base",
		.start  = MSM_GSBI6_QUP_PHYS,
		.end    = MSM_GSBI6_QUP_PHYS + SZ_4K - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "gsbi_base",
		.start  = MSM_GSBI6_PHYS,
		.end    = MSM_GSBI6_PHYS + 4 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "spi_irq_in",
		.start  = GSBI6_QUP_IRQ,
		.end    = GSBI6_QUP_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
	{
		.name   = "spi_clk",
		.start  = 17,
		.end    = 17,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "spi_miso",
		.start  = 15,
		.end    = 15,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "spi_mosi",
		.start  = 14,
		.end    = 14,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "spi_cs",
		.start  = 16,
		.end    = 16,
		.flags  = IORESOURCE_IO,
	}
};

struct platform_device mpq8064_device_qup_spi_gsbi6 = {
	.name		= "spi_qsd",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(resources_qup_spi_gsbi6),
	.resource	= resources_qup_spi_gsbi6,
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
		.name	= "i2c_clk",
		.start	= 54,
		.end	= 54,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "i2c_sda",
		.start	= 53,
		.end	= 53,
		.flags	= IORESOURCE_IO,
	},
};

struct platform_device mpq8064_device_qup_i2c_gsbi5 = {
	.name		= "qup_i2c",
	.id		= 5,
	.num_resources	= ARRAY_SIZE(resources_qup_i2c_gsbi5),
	.resource	= resources_qup_i2c_gsbi5,
};

static struct resource resources_uart_gsbi5[] = {
	{
		.start  = GSBI5_UARTDM_IRQ,
		.end    = GSBI5_UARTDM_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
	{
		.start  = MSM_UART5DM_PHYS,
		.end    = MSM_UART5DM_PHYS + PAGE_SIZE - 1,
		.name   = "uartdm_resource",
		.flags  = IORESOURCE_MEM,
	},
	{
		.start  = MSM_GSBI5_PHYS,
		.end    = MSM_GSBI5_PHYS + PAGE_SIZE - 1,
		.name   = "gsbi_resource",
		.flags  = IORESOURCE_MEM,
	},
};

struct platform_device mpq8064_device_uart_gsbi5 = {
	.name	= "msm_serial_hsl",
	.id	= 2,
	.num_resources	= ARRAY_SIZE(resources_uart_gsbi5),
	.resource	= resources_uart_gsbi5,
};

/* GSBI 6 used into UARTDM Mode */
static struct resource msm_uart_dm6_resources[] = {
	{
		.start  = MSM_UART6DM_PHYS,
		.end    = MSM_UART6DM_PHYS + PAGE_SIZE - 1,
		.name   = "uartdm_resource",
		.flags  = IORESOURCE_MEM,
	},
	{
		.start  = GSBI6_UARTDM_IRQ,
		.end    = GSBI6_UARTDM_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
	{
		.start  = MSM_GSBI6_PHYS,
		.end    = MSM_GSBI6_PHYS + 4 - 1,
		.name   = "gsbi_resource",
		.flags  = IORESOURCE_MEM,
	},
	{
		.start  = DMOV_MPQ8064_HSUART_GSBI6_TX_CHAN,
		.end    = DMOV_MPQ8064_HSUART_GSBI6_RX_CHAN,
		.name   = "uartdm_channels",
		.flags  = IORESOURCE_DMA,
	},
	{
		.start  = DMOV_MPQ8064_HSUART_GSBI6_TX_CRCI,
		.end    = DMOV_MPQ8064_HSUART_GSBI6_RX_CRCI,
		.name   = "uartdm_crci",
		.flags  = IORESOURCE_DMA,
	},
};
static u64 msm_uart_dm6_dma_mask = DMA_BIT_MASK(32);
struct platform_device mpq8064_device_uartdm_gsbi6 = {
	.name   = "msm_serial_hs",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(msm_uart_dm6_resources),
	.resource       = msm_uart_dm6_resources,
	.dev    = {
		.dma_mask		= &msm_uart_dm6_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};

static struct resource resources_uart_gsbi7[] = {
	{
		.start	= GSBI7_UARTDM_IRQ,
		.end	= GSBI7_UARTDM_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= MSM_UART7DM_PHYS,
		.end	= MSM_UART7DM_PHYS + PAGE_SIZE - 1,
		.name	= "uartdm_resource",
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= MSM_GSBI7_PHYS,
		.end	= MSM_GSBI7_PHYS + PAGE_SIZE - 1,
		.name	= "gsbi_resource",
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device apq8064_device_uart_gsbi7 = {
	.name	= "msm_serial_hsl",
	.id	= 0,
	.num_resources	= ARRAY_SIZE(resources_uart_gsbi7),
	.resource	= resources_uart_gsbi7,
};

struct platform_device apq_pcm = {
	.name	= "msm-pcm-dsp",
	.id	= -1,
};

struct platform_device apq_pcm_routing = {
	.name	= "msm-pcm-routing",
	.id	= -1,
};

struct platform_device apq_cpudai0 = {
	.name	= "msm-dai-q6",
	.id	= 0x4000,
};

struct platform_device apq_cpudai1 = {
	.name	= "msm-dai-q6",
	.id	= 0x4001,
};
struct platform_device mpq_cpudai_sec_i2s_rx = {
	.name = "msm-dai-q6",
	.id = 4,
};
struct platform_device apq_cpudai_hdmi_rx = {
	.name	= "msm-dai-q6-hdmi",
	.id	= 8,
};

struct platform_device apq_cpudai_bt_rx = {
	.name   = "msm-dai-q6",
	.id     = 0x3000,
};

struct platform_device apq_cpudai_bt_tx = {
	.name   = "msm-dai-q6",
	.id     = 0x3001,
};

struct platform_device apq_cpudai_fm_rx = {
	.name   = "msm-dai-q6",
	.id     = 0x3004,
};

struct platform_device apq_cpudai_fm_tx = {
	.name   = "msm-dai-q6",
	.id     = 0x3005,
};

struct platform_device apq_cpudai_slim_4_rx = {
	.name   = "msm-dai-q6",
	.id     = 0x4008,
};

struct platform_device apq_cpudai_slim_4_tx = {
	.name   = "msm-dai-q6",
	.id     = 0x4009,
};

struct platform_device mpq_cpudai_pseudo = {
	.name   = "msm-dai-q6",
	.id     = 0x8001,
};
#define MSM_TSIF0_PHYS       (0x18200000)
#define MSM_TSIF1_PHYS       (0x18201000)
#define MSM_TSIF_SIZE        (0x200)

#define TSIF_0_CLK       GPIO_CFG(55, 1, GPIO_CFG_INPUT, \
	GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA)
#define TSIF_0_EN        GPIO_CFG(56, 1, GPIO_CFG_INPUT, \
	GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA)
#define TSIF_0_DATA      GPIO_CFG(57, 1, GPIO_CFG_INPUT, \
	GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA)
#define TSIF_0_SYNC      GPIO_CFG(62, 1, GPIO_CFG_INPUT, \
	GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA)
#define TSIF_1_CLK       GPIO_CFG(59, 1, GPIO_CFG_INPUT, \
	GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA)
#define TSIF_1_EN        GPIO_CFG(60, 1, GPIO_CFG_INPUT, \
	GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA)
#define TSIF_1_DATA      GPIO_CFG(61, 1, GPIO_CFG_INPUT, \
	GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA)
#define TSIF_1_SYNC      GPIO_CFG(58, 1, GPIO_CFG_INPUT, \
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

struct msm_tsif_platform_data tsif1_8064_platform_data = {
	.num_gpios = ARRAY_SIZE(tsif1_gpios),
	.gpios = tsif1_gpios,
	.tsif_pclk = "iface_clk",
	.tsif_ref_clk = "ref_clk",
};

struct resource tsif1_8064_resources[] = {
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
		.start = DMOV8064_TSIF_CHAN,
		.end   = DMOV8064_TSIF_CRCI,
	},
};

struct msm_tsif_platform_data tsif0_8064_platform_data = {
	.num_gpios = ARRAY_SIZE(tsif0_gpios),
	.gpios = tsif0_gpios,
	.tsif_pclk = "iface_clk",
	.tsif_ref_clk = "ref_clk",
};

struct resource tsif0_8064_resources[] = {
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

struct platform_device msm_8064_device_tsif[2] = {
	{
		.name          = "msm_tsif",
		.id            = 0,
		.num_resources = ARRAY_SIZE(tsif0_8064_resources),
		.resource      = tsif0_8064_resources,
		.dev = {
			.platform_data = &tsif0_8064_platform_data
		},
	},
	{
		.name          = "msm_tsif",
		.id            = 1,
		.num_resources = ARRAY_SIZE(tsif1_8064_resources),
		.resource      = tsif1_8064_resources,
		.dev = {
			.platform_data = &tsif1_8064_platform_data
		},
	}
};

#define MSM_TSPP_PHYS			(0x18202000)
#define MSM_TSPP_SIZE			(0x1000)
#define MSM_TSPP_BAM_PHYS		(0x18204000)
#define MSM_TSPP_BAM_SIZE		(0x2000)

static const struct msm_gpio tspp_gpios[] = {
	{ .gpio_cfg = TSIF_0_CLK,  .label =  "tsif_clk", },
	{ .gpio_cfg = TSIF_0_EN,   .label =  "tsif_en", },
	{ .gpio_cfg = TSIF_0_DATA, .label =  "tsif_data", },
	{ .gpio_cfg = TSIF_0_SYNC, .label =  "tsif_sync", },
	{ .gpio_cfg = TSIF_1_CLK,  .label =  "tsif_clk", },
	{ .gpio_cfg = TSIF_1_EN,   .label =  "tsif_en", },
	{ .gpio_cfg = TSIF_1_DATA, .label =  "tsif_data", },
	{ .gpio_cfg = TSIF_1_SYNC, .label =  "tsif_sync", },
};

static struct resource tspp_resources[] = {
	[0] = {
		.name = "TSIF_TSPP_IRQ",
		.flags = IORESOURCE_IRQ,
		.start = TSIF_TSPP_IRQ,
		.end   = TSIF_TSPP_IRQ,
	},
	[1] = {
		.name = "TSIF0_IRQ",
		.flags = IORESOURCE_IRQ,
		.start = TSIF1_IRQ,
		.end   = TSIF1_IRQ,
	},
	[2] = {
		.name = "TSIF1_IRQ",
		.flags = IORESOURCE_IRQ,
		.start = TSIF2_IRQ,
		.end   = TSIF2_IRQ,
	},
	[3] = {
		.name = "TSIF_BAM_IRQ",
		.flags = IORESOURCE_IRQ,
		.start = TSIF_BAM_IRQ,
		.end   = TSIF_BAM_IRQ,
	},
	[4] = {
		.name = "MSM_TSIF0_PHYS",
		.flags = IORESOURCE_MEM,
		.start = MSM_TSIF0_PHYS,
		.end   = MSM_TSIF0_PHYS + MSM_TSIF_SIZE - 1,
	},
	[5] = {
		.name = "MSM_TSIF1_PHYS",
		.flags = IORESOURCE_MEM,
		.start = MSM_TSIF1_PHYS,
		.end   = MSM_TSIF1_PHYS + MSM_TSIF_SIZE - 1,
	},
	[6] = {
		.name = "MSM_TSPP_PHYS",
		.flags = IORESOURCE_MEM,
		.start = MSM_TSPP_PHYS,
		.end   = MSM_TSPP_PHYS + MSM_TSPP_SIZE - 1,
	},
	[7] = {
		.name = "MSM_TSPP_BAM_PHYS",
		.flags = IORESOURCE_MEM,
		.start = MSM_TSPP_BAM_PHYS,
		.end   = MSM_TSPP_BAM_PHYS + MSM_TSPP_BAM_SIZE - 1,
	},
};

static struct msm_tspp_platform_data tspp_platform_data = {
	.num_gpios = ARRAY_SIZE(tspp_gpios),
	.gpios = tspp_gpios,
	.tsif_pclk = "iface_clk",
	.tsif_ref_clk = "ref_clk",
	.tsif_vreg_present = 0,
};

struct platform_device msm_8064_device_tspp = {
	.name          = "msm_tspp",
	.id            = 0,
	.num_resources = ARRAY_SIZE(tspp_resources),
	.resource      = tspp_resources,
	.dev = {
		.platform_data = &tspp_platform_data
	},
};

/*
 * Machine specific data for AUX PCM Interface
 * which the driver will  be unware of.
 */
struct msm_dai_auxpcm_pdata apq_auxpcm_pdata = {
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

struct platform_device apq_cpudai_auxpcm_rx = {
	.name = "msm-dai-q6",
	.id = 2,
	.dev = {
		.platform_data = &apq_auxpcm_pdata,
	},
};

struct platform_device apq_cpudai_auxpcm_tx = {
	.name = "msm-dai-q6",
	.id = 3,
	.dev = {
		.platform_data = &apq_auxpcm_pdata,
	},
};

struct msm_mi2s_pdata mpq_mi2s_tx_data = {
	.rx_sd_lines = 0,
	.tx_sd_lines = MSM_MI2S_SD0 | MSM_MI2S_SD1 | MSM_MI2S_SD2 |
		       MSM_MI2S_SD3,
};

struct platform_device mpq_cpudai_mi2s_tx = {
	.name	= "msm-dai-q6-mi2s",
	.id	= -1, /*MI2S_TX */
	.dev = {
		.platform_data = &mpq_mi2s_tx_data,
	},
};

struct msm_mi2s_pdata apq_mi2s_data = {
	.rx_sd_lines = MSM_MI2S_SD0,
	.tx_sd_lines = MSM_MI2S_SD3,
};

struct platform_device apq_cpudai_mi2s = {
	.name	= "msm-dai-q6-mi2s",
	.id	= -1,
	.dev = {
		.platform_data = &apq_mi2s_data,
	},
};

struct platform_device apq_cpudai_i2s_rx = {
	.name	= "msm-dai-q6",
	.id	= PRIMARY_I2S_RX,
};

struct platform_device apq_cpudai_i2s_tx = {
	.name	= "msm-dai-q6",
	.id	= PRIMARY_I2S_TX,
};

struct platform_device apq_cpu_fe = {
	.name	= "msm-dai-fe",
	.id	= -1,
};

struct platform_device apq_stub_codec = {
	.name	= "msm-stub-codec",
	.id	= 1,
};

struct platform_device apq_voice = {
	.name	= "msm-pcm-voice",
	.id	= -1,
};

struct platform_device apq_voip = {
	.name	= "msm-voip-dsp",
	.id	= -1,
};

struct platform_device apq_lpa_pcm = {
	.name   = "msm-pcm-lpa",
	.id     = -1,
};

struct platform_device apq_compr_dsp = {
	.name   = "msm-compr-dsp",
	.id     = -1,
};

struct platform_device apq_multi_ch_pcm = {
	.name   = "msm-multi-ch-pcm-dsp",
	.id     = -1,
};

struct platform_device apq_lowlatency_pcm = {
	.name   = "msm-lowlatency-pcm-dsp",
	.id     = -1,
};

struct platform_device apq_pcm_hostless = {
	.name	= "msm-pcm-hostless",
	.id	= -1,
};

struct platform_device apq_cpudai_afe_01_rx = {
	.name = "msm-dai-q6",
	.id = 0xE0,
};

struct platform_device apq_cpudai_afe_01_tx = {
	.name = "msm-dai-q6",
	.id = 0xF0,
};

struct platform_device apq_cpudai_afe_02_rx = {
	.name = "msm-dai-q6",
	.id = 0xF1,
};

struct platform_device apq_cpudai_afe_02_tx = {
	.name = "msm-dai-q6",
	.id = 0xE1,
};

struct platform_device apq_pcm_afe = {
	.name	= "msm-pcm-afe",
	.id	= -1,
};

struct platform_device apq_cpudai_stub = {
	.name = "msm-dai-stub",
	.id = -1,
};

struct platform_device apq_cpudai_slimbus_1_rx = {
	.name = "msm-dai-q6",
	.id = 0x4002,
};

struct platform_device apq_cpudai_slimbus_1_tx = {
	.name = "msm-dai-q6",
	.id = 0x4003,
};

struct platform_device apq_cpudai_slimbus_2_rx = {
	.name = "msm-dai-q6",
	.id = 0x4004,
};

struct platform_device apq_cpudai_slimbus_2_tx = {
	.name = "msm-dai-q6",
	.id = 0x4005,
};

struct platform_device apq_cpudai_slimbus_3_rx = {
	.name = "msm-dai-q6",
	.id = 0x4006,
};

struct platform_device apq_cpudai_slimbus_3_tx = {
	.name = "msm-dai-q6",
	.id = 0x4007,
};

static struct resource resources_ssbi_pmic1[] = {
	{
		.start  = MSM_PMIC1_SSBI_CMD_PHYS,
		.end    = MSM_PMIC1_SSBI_CMD_PHYS + MSM_PMIC_SSBI_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
};

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

struct platform_device apq8064_slim_ctrl = {
	.name	= "msm_slim_ctrl",
	.id	= 1,
	.num_resources	= ARRAY_SIZE(slimbus_res),
	.resource	= slimbus_res,
	.dev            = {
		.coherent_dma_mask      = 0xffffffffULL,
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
		.start	= MSM_HSUSB1_PHYS,
		.end	= MSM_HSUSB1_PHYS + MSM_HSUSB1_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= USB1_HS_IRQ,
		.end	= USB1_HS_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device apq8064_device_otg = {
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
		.start	= MSM_HSUSB1_PHYS,
		.end	= MSM_HSUSB1_PHYS + MSM_HSUSB1_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= USB1_HS_IRQ,
		.end	= USB1_HS_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device apq8064_device_gadget_peripheral = {
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
		.start  = MSM_HSUSB1_PHYS,
		.end    = MSM_HSUSB1_PHYS + MSM_HSUSB1_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.start  = USB1_HS_IRQ,
		.end    = USB1_HS_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct resource resources_hsic_host[] = {
	{
		.start	= 0x12510000,
		.end	= 0x12510000 + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= USB2_HSIC_IRQ,
		.end	= USB2_HSIC_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= MSM_GPIO_TO_INT(49),
		.end	= MSM_GPIO_TO_INT(49),
		.name	= "peripheral_status_irq",
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= MSM_GPIO_TO_INT(47),
		.end	= MSM_GPIO_TO_INT(47),
		.name	= "wakeup",
		.flags	= IORESOURCE_IRQ,
	},
};

static u64 dma_mask = DMA_BIT_MASK(32);
struct platform_device apq8064_device_hsusb_host = {
	.name           = "msm_hsusb_host",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(resources_hsusb_host),
	.resource       = resources_hsusb_host,
	.dev            = {
		.dma_mask               = &dma_mask,
		.coherent_dma_mask      = 0xffffffff,
	},
};

struct platform_device apq8064_device_hsic_host = {
	.name		= "msm_hsic_host",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_hsic_host),
	.resource	= resources_hsic_host,
	.dev		= {
		.dma_mask		= &dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};

static struct resource resources_ehci_host3[] = {
{
		.start  = MSM_HSUSB3_PHYS,
		.end    = MSM_HSUSB3_PHYS + MSM_HSUSB3_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.start  = USB3_HS_IRQ,
		.end    = USB3_HS_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device apq8064_device_ehci_host3 = {
	.name           = "msm_ehci_host",
	.id             = 0,
	.num_resources  = ARRAY_SIZE(resources_ehci_host3),
	.resource       = resources_ehci_host3,
	.dev            = {
		.dma_mask               = &dma_mask,
		.coherent_dma_mask      = 0xffffffff,
	},
};

static struct resource resources_ehci_host4[] = {
{
		.start  = MSM_HSUSB4_PHYS,
		.end    = MSM_HSUSB4_PHYS + MSM_HSUSB4_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.start  = USB4_HS_IRQ,
		.end    = USB4_HS_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device apq8064_device_ehci_host4 = {
	.name           = "msm_ehci_host",
	.id             = 1,
	.num_resources  = ARRAY_SIZE(resources_ehci_host4),
	.resource       = resources_ehci_host4,
	.dev            = {
		.dma_mask               = &dma_mask,
		.coherent_dma_mask      = 0xffffffff,
	},
};

struct platform_device apq8064_device_acpuclk = {
	.name		= "acpuclk-8064",
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

struct platform_device apq_device_tz_log = {
	.name		= "tz_log",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(tzlog_resources),
	.resource	= tzlog_resources,
};

/* MSM Video core device */
#ifdef CONFIG_MSM_BUS_SCALING
static struct msm_bus_vectors vidc_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VIDEO_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_VIDEO_DEC,
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
		.src = MSM_BUS_MASTER_VIDEO_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 54525952,
		.ib  = 436207616,
	},
	{
		.src = MSM_BUS_MASTER_VIDEO_DEC,
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
		.src = MSM_BUS_MASTER_VIDEO_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 40894464,
		.ib  = 327155712,
	},
	{
		.src = MSM_BUS_MASTER_VIDEO_DEC,
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
		.src = MSM_BUS_MASTER_VIDEO_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 163577856,
		.ib  = 1308622848,
	},
	{
		.src = MSM_BUS_MASTER_VIDEO_DEC,
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
		.src = MSM_BUS_MASTER_VIDEO_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 121634816,
		.ib  = 973078528,
	},
	{
		.src = MSM_BUS_MASTER_VIDEO_DEC,
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
		.src = MSM_BUS_MASTER_VIDEO_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 372244480,
		.ib  = 2560000000U,
	},
	{
		.src = MSM_BUS_MASTER_VIDEO_DEC,
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
		.src = MSM_BUS_MASTER_VIDEO_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 222298112,
		.ib  = 2560000000U,
	},
	{
		.src = MSM_BUS_MASTER_VIDEO_DEC,
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
		.src = MSM_BUS_MASTER_VIDEO_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 222298112,
		.ib  = 3522000000U,
	},
	{
		.src = MSM_BUS_MASTER_VIDEO_DEC,
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
		.src = MSM_BUS_MASTER_VIDEO_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 222298112,
		.ib  = 3522000000U,
	},
	{
		.src = MSM_BUS_MASTER_VIDEO_DEC,
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
		vidc_venc_1080p_turbo_vectors,
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


#define APQ8064_VIDC_BASE_PHYS 0x04400000
#define APQ8064_VIDC_BASE_SIZE 0x00100000

static struct resource apq8064_device_vidc_resources[] = {
	{
		.start	= APQ8064_VIDC_BASE_PHYS,
		.end	= APQ8064_VIDC_BASE_PHYS + APQ8064_VIDC_BASE_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= VCODEC_IRQ,
		.end	= VCODEC_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct msm_vidc_platform_data apq8064_vidc_platform_data = {
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

struct platform_device apq8064_msm_device_vidc = {
	.name = "msm_vidc",
	.id = 0,
	.num_resources = ARRAY_SIZE(apq8064_device_vidc_resources),
	.resource = apq8064_device_vidc_resources,
	.dev = {
		.platform_data	= &apq8064_vidc_platform_data,
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
		.name   = "dml_mem",
		.start	= MSM_SDC1_DML_BASE,
		.end	= MSM_SDC1_BAM_BASE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "bam_mem",
		.start	= MSM_SDC1_BAM_BASE,
		.end	= MSM_SDC1_BAM_BASE + (2 * SZ_4K) - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "bam_irq",
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
		.name   = "dml_mem",
		.start	= MSM_SDC2_DML_BASE,
		.end	= MSM_SDC2_BAM_BASE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "bam_mem",
		.start	= MSM_SDC2_BAM_BASE,
		.end	= MSM_SDC2_BAM_BASE + (2 * SZ_4K) - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "bam_irq",
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
		.name   = "dml_mem",
		.start	= MSM_SDC3_DML_BASE,
		.end	= MSM_SDC3_BAM_BASE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "bam_mem",
		.start	= MSM_SDC3_BAM_BASE,
		.end	= MSM_SDC3_BAM_BASE + (2 * SZ_4K) - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "bam_irq",
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
		.name   = "dml_mem",
		.start	= MSM_SDC4_DML_BASE,
		.end	= MSM_SDC4_BAM_BASE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "bam_mem",
		.start	= MSM_SDC4_BAM_BASE,
		.end	= MSM_SDC4_BAM_BASE + (2 * SZ_4K) - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "bam_irq",
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

#define MSM_SATA_AHCI_BASE	0x29000000
#define MSM_SATA_AHCI_REGS_SZ	0x180
#define MSM_SATA_PHY_BASE	0x1B400000
#define MSM_SATA_PHY_REGS_SZ	0x200

static struct resource resources_sata[] = {
	{
		.name	= "ahci_mem",
		.flags	= IORESOURCE_MEM,
		.start	= MSM_SATA_AHCI_BASE,
		.end	= MSM_SATA_AHCI_BASE + MSM_SATA_AHCI_REGS_SZ - 1,
	},
	{
		.name	= "ahci_irq",
		.flags	= IORESOURCE_IRQ,
		.start	= SATA_CONTROLLER_IRQ,
		.end	= SATA_CONTROLLER_IRQ,
	},
	{
		.name	= "phy_mem",
		.flags	= IORESOURCE_MEM,
		.start	= MSM_SATA_PHY_BASE,
		.end	= MSM_SATA_PHY_BASE + MSM_SATA_PHY_REGS_SZ - 1,
	},
};

static u64 sata_dma_mask = DMA_BIT_MASK(32);
struct platform_device apq8064_device_sata = {
	.name		= "msm_sata",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(resources_sata),
	.resource	= resources_sata,
	.dev		= {
		.dma_mask		= &sata_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
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

struct platform_device msm_bus_8064_sys_fabric = {
	.name  = "msm_bus_fabric",
	.id    =  MSM_BUS_FAB_SYSTEM,
};
struct platform_device msm_bus_8064_apps_fabric = {
	.name  = "msm_bus_fabric",
	.id    = MSM_BUS_FAB_APPSS,
};
struct platform_device msm_bus_8064_mm_fabric = {
	.name  = "msm_bus_fabric",
	.id    = MSM_BUS_FAB_MMSS,
};
struct platform_device msm_bus_8064_sys_fpb = {
	.name  = "msm_bus_fabric",
	.id    = MSM_BUS_FAB_SYSTEM_FPB,
};
struct platform_device msm_bus_8064_cpss_fpb = {
	.name  = "msm_bus_fabric",
	.id    = MSM_BUS_FAB_CPSS_FPB,
};

static struct msm_sps_platform_data msm_sps_pdata = {
	.bamdma_restricted_pipes = 0x06,
};

struct platform_device msm_device_sps_apq8064 = {
	.name		= "msm_sps",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_sps),
	.resource	= resources_sps,
	.dev.platform_data = &msm_sps_pdata,
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
		.subsys_name = "gss",
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
		.subsys_name = "adsp",
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

struct platform_device msm_device_smd_apq8064 = {
	.name		= "msm_smd",
	.id		= -1,
	.resource = smd_resource,
	.num_resources = ARRAY_SIZE(smd_resource),
	.dev = {
		.platform_data = &smd_platform_data,
	},
};

static struct resource resources_msm_pcie[] = {
	{
		.name   = "pcie_parf",
		.start  = PCIE20_PARF_PHYS,
		.end    = PCIE20_PARF_PHYS + PCIE20_PARF_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "pcie_elbi",
		.start  = PCIE20_ELBI_PHYS,
		.end    = PCIE20_ELBI_PHYS + PCIE20_ELBI_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "pcie20",
		.start  = PCIE20_PHYS,
		.end    = PCIE20_PHYS + PCIE20_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
};

struct platform_device msm_device_pcie = {
	.name           = "msm_pcie",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(resources_msm_pcie),
	.resource       = resources_msm_pcie,
};

#ifdef CONFIG_HW_RANDOM_MSM
/* PRNG device */
#define MSM_PRNG_PHYS           0x1A500000
static struct resource rng_resources = {
	.flags = IORESOURCE_MEM,
	.start = MSM_PRNG_PHYS,
	.end   = MSM_PRNG_PHYS + SZ_512 - 1,
};

struct platform_device apq8064_device_rng = {
	.name          = "msm_rng",
	.id            = 0,
	.num_resources = 1,
	.resource      = &rng_resources,
};
#endif

static struct resource msm_gss_resources[] = {
	{
		.start  = 0x10000000,
		.end    = 0x10000000 + SZ_256 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.start  = 0x10008000,
		.end    = 0x10008000 + SZ_256 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.start  = 0x00900000,
		.end    = 0x00900000 + SZ_16K - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.start	= GSS_A5_WDOG_EXPIRED,
		.end	= GSS_A5_WDOG_EXPIRED,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm_gss = {
	.name = "pil_gss",
	.id = -1,
	.num_resources  = ARRAY_SIZE(msm_gss_resources),
	.resource       = msm_gss_resources,
};

static struct fs_driver_data gfx3d_fs_data = {
	.clks = (struct fs_clk_data[]){
		{ .name = "core_clk", .reset_rate = 1800000 },
		{ .name = "iface_clk" },
		{ .name = "bus_clk" },
		{ 0 }
	},
	.reset_delay_us = 10,
	.bus_port0 = MSM_BUS_MASTER_GRAPHICS_3D,
	.bus_port1 = MSM_BUS_MASTER_GRAPHICS_3D_PORT1,
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
		{ .name = "reset1_clk" },
		{ .name = "reset2_clk" },
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
	.bus_port0 = MSM_BUS_MASTER_VIDEO_ENC,
	.bus_port1 = MSM_BUS_MASTER_VIDEO_DEC,
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

static struct fs_driver_data vcap_fs_data = {
	.clks = (struct fs_clk_data[]){
		{ .name = "core_clk" },
		{ .name = "iface_clk" },
		{ .name = "bus_clk" },
		{ 0 },
	},
	.bus_port0 = MSM_BUS_MASTER_VIDEO_CAP,
};

struct platform_device *apq8064_footswitch[] __initdata = {
	FS_8X60(FS_MDP,    "vdd",       "mdp.0",        &mdp_fs_data),
	FS_8X60(FS_ROT,    "vdd",	"msm_rotator.0", &rot_fs_data),
	FS_8X60(FS_IJPEG,  "vdd",	"msm_gemini.0",	&ijpeg_fs_data),
	FS_8X60(FS_VFE,    "vdd",	"msm_vfe.0",	&vfe_fs_data),
	FS_8X60(FS_VPE,    "vdd",	"msm_vpe.0",	&vpe_fs_data),
	FS_8X60(FS_GFX3D_8064, "vdd",	"kgsl-3d0.0",	&gfx3d_fs_data),
	FS_8X60(FS_VED,    "vdd",	"msm_vidc.0",	&ved_fs_data),
	FS_8X60(FS_VCAP,   "vdd",	"msm_vcap.0",	&vcap_fs_data),
};
unsigned apq8064_num_footswitch __initdata = ARRAY_SIZE(apq8064_footswitch);

struct msm_rpm_platform_data apq8064_rpm_data __initdata = {
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
	.target_id =  {
		MSM_RPM_MAP(8064, NOTIFICATION_CONFIGURED_0, NOTIFICATION, 4),
		MSM_RPM_MAP(8064, NOTIFICATION_REGISTERED_0, NOTIFICATION, 4),
		MSM_RPM_MAP(8064, INVALIDATE_0, INVALIDATE, 8),
		MSM_RPM_MAP(8064, TRIGGER_TIMED_TO, TRIGGER_TIMED, 1),
		MSM_RPM_MAP(8064, TRIGGER_TIMED_SCLK_COUNT, TRIGGER_TIMED, 1),
		MSM_RPM_MAP(8064, RPM_CTL, RPM_CTL, 1),
		MSM_RPM_MAP(8064, CXO_CLK, CXO_CLK, 1),
		MSM_RPM_MAP(8064, PXO_CLK, PXO_CLK, 1),
		MSM_RPM_MAP(8064, APPS_FABRIC_CLK, APPS_FABRIC_CLK, 1),
		MSM_RPM_MAP(8064, SYSTEM_FABRIC_CLK, SYSTEM_FABRIC_CLK, 1),
		MSM_RPM_MAP(8064, MM_FABRIC_CLK, MM_FABRIC_CLK, 1),
		MSM_RPM_MAP(8064, DAYTONA_FABRIC_CLK, DAYTONA_FABRIC_CLK, 1),
		MSM_RPM_MAP(8064, SFPB_CLK, SFPB_CLK, 1),
		MSM_RPM_MAP(8064, CFPB_CLK, CFPB_CLK, 1),
		MSM_RPM_MAP(8064, MMFPB_CLK, MMFPB_CLK, 1),
		MSM_RPM_MAP(8064, EBI1_CLK, EBI1_CLK, 1),
		MSM_RPM_MAP(8064, APPS_FABRIC_CFG_HALT_0,
				APPS_FABRIC_CFG_HALT, 2),
		MSM_RPM_MAP(8064, APPS_FABRIC_CFG_CLKMOD_0,
				APPS_FABRIC_CFG_CLKMOD, 3),
		MSM_RPM_MAP(8064, APPS_FABRIC_CFG_IOCTL,
				APPS_FABRIC_CFG_IOCTL, 1),
		MSM_RPM_MAP(8064, APPS_FABRIC_ARB_0, APPS_FABRIC_ARB, 12),
		MSM_RPM_MAP(8064, SYS_FABRIC_CFG_HALT_0,
				SYS_FABRIC_CFG_HALT, 2),
		MSM_RPM_MAP(8064, SYS_FABRIC_CFG_CLKMOD_0,
				SYS_FABRIC_CFG_CLKMOD, 3),
		MSM_RPM_MAP(8064, SYS_FABRIC_CFG_IOCTL,
				SYS_FABRIC_CFG_IOCTL, 1),
		MSM_RPM_MAP(8064, SYSTEM_FABRIC_ARB_0, SYSTEM_FABRIC_ARB, 30),
		MSM_RPM_MAP(8064, MMSS_FABRIC_CFG_HALT_0,
				MMSS_FABRIC_CFG_HALT, 2),
		MSM_RPM_MAP(8064, MMSS_FABRIC_CFG_CLKMOD_0,
				MMSS_FABRIC_CFG_CLKMOD, 3),
		MSM_RPM_MAP(8064, MMSS_FABRIC_CFG_IOCTL,
				MMSS_FABRIC_CFG_IOCTL, 1),
		MSM_RPM_MAP(8064, MM_FABRIC_ARB_0, MM_FABRIC_ARB, 21),
		MSM_RPM_MAP(8064, PM8921_S1_0, PM8921_S1, 2),
		MSM_RPM_MAP(8064, PM8921_S2_0, PM8921_S2, 2),
		MSM_RPM_MAP(8064, PM8921_S3_0, PM8921_S3, 2),
		MSM_RPM_MAP(8064, PM8921_S4_0, PM8921_S4, 2),
		MSM_RPM_MAP(8064, PM8921_S5_0, PM8921_S5, 2),
		MSM_RPM_MAP(8064, PM8921_S6_0, PM8921_S6, 2),
		MSM_RPM_MAP(8064, PM8921_S7_0, PM8921_S7, 2),
		MSM_RPM_MAP(8064, PM8921_S8_0, PM8921_S8, 2),
		MSM_RPM_MAP(8064, PM8921_L1_0, PM8921_L1, 2),
		MSM_RPM_MAP(8064, PM8921_L2_0, PM8921_L2, 2),
		MSM_RPM_MAP(8064, PM8921_L3_0, PM8921_L3, 2),
		MSM_RPM_MAP(8064, PM8921_L4_0, PM8921_L4, 2),
		MSM_RPM_MAP(8064, PM8921_L5_0, PM8921_L5, 2),
		MSM_RPM_MAP(8064, PM8921_L6_0, PM8921_L6, 2),
		MSM_RPM_MAP(8064, PM8921_L7_0, PM8921_L7, 2),
		MSM_RPM_MAP(8064, PM8921_L8_0, PM8921_L8, 2),
		MSM_RPM_MAP(8064, PM8921_L9_0, PM8921_L9, 2),
		MSM_RPM_MAP(8064, PM8921_L10_0, PM8921_L10, 2),
		MSM_RPM_MAP(8064, PM8921_L11_0, PM8921_L11, 2),
		MSM_RPM_MAP(8064, PM8921_L12_0, PM8921_L12, 2),
		MSM_RPM_MAP(8064, PM8921_L13_0, PM8921_L13, 2),
		MSM_RPM_MAP(8064, PM8921_L14_0, PM8921_L14, 2),
		MSM_RPM_MAP(8064, PM8921_L15_0, PM8921_L15, 2),
		MSM_RPM_MAP(8064, PM8921_L16_0, PM8921_L16, 2),
		MSM_RPM_MAP(8064, PM8921_L17_0, PM8921_L17, 2),
		MSM_RPM_MAP(8064, PM8921_L18_0, PM8921_L18, 2),
		MSM_RPM_MAP(8064, PM8921_L19_0, PM8921_L19, 2),
		MSM_RPM_MAP(8064, PM8921_L20_0, PM8921_L20, 2),
		MSM_RPM_MAP(8064, PM8921_L21_0, PM8921_L21, 2),
		MSM_RPM_MAP(8064, PM8921_L22_0, PM8921_L22, 2),
		MSM_RPM_MAP(8064, PM8921_L23_0, PM8921_L23, 2),
		MSM_RPM_MAP(8064, PM8921_L24_0, PM8921_L24, 2),
		MSM_RPM_MAP(8064, PM8921_L25_0, PM8921_L25, 2),
		MSM_RPM_MAP(8064, PM8921_L26_0, PM8921_L26, 2),
		MSM_RPM_MAP(8064, PM8921_L27_0, PM8921_L27, 2),
		MSM_RPM_MAP(8064, PM8921_L28_0, PM8921_L28, 2),
		MSM_RPM_MAP(8064, PM8921_L29_0, PM8921_L29, 2),
		MSM_RPM_MAP(8064, PM8921_CLK1_0, PM8921_CLK1, 2),
		MSM_RPM_MAP(8064, PM8921_CLK2_0, PM8921_CLK2, 2),
		MSM_RPM_MAP(8064, PM8921_LVS1, PM8921_LVS1, 1),
		MSM_RPM_MAP(8064, PM8921_LVS2, PM8921_LVS2, 1),
		MSM_RPM_MAP(8064, PM8921_LVS3, PM8921_LVS3, 1),
		MSM_RPM_MAP(8064, PM8921_LVS4, PM8921_LVS4, 1),
		MSM_RPM_MAP(8064, PM8921_LVS5, PM8921_LVS5, 1),
		MSM_RPM_MAP(8064, PM8921_LVS6, PM8921_LVS6, 1),
		MSM_RPM_MAP(8064, PM8921_LVS7, PM8921_LVS7, 1),
		MSM_RPM_MAP(8064, PM8821_S1_0, PM8821_S1, 2),
		MSM_RPM_MAP(8064, PM8821_S2_0, PM8821_S2, 2),
		MSM_RPM_MAP(8064, PM8821_L1_0, PM8821_L1, 2),
		MSM_RPM_MAP(8064, NCP_0, NCP, 2),
		MSM_RPM_MAP(8064, CXO_BUFFERS, CXO_BUFFERS, 1),
		MSM_RPM_MAP(8064, USB_OTG_SWITCH, USB_OTG_SWITCH, 1),
		MSM_RPM_MAP(8064, HDMI_SWITCH, HDMI_SWITCH, 1),
		MSM_RPM_MAP(8064, DDR_DMM_0, DDR_DMM, 2),
		MSM_RPM_MAP(8064, QDSS_CLK, QDSS_CLK, 1),
		MSM_RPM_MAP(8064, VDDMIN_GPIO, VDDMIN_GPIO, 1),
	},
	.target_status = {
		MSM_RPM_STATUS_ID_MAP(8064, VERSION_MAJOR),
		MSM_RPM_STATUS_ID_MAP(8064, VERSION_MINOR),
		MSM_RPM_STATUS_ID_MAP(8064, VERSION_BUILD),
		MSM_RPM_STATUS_ID_MAP(8064, SUPPORTED_RESOURCES_0),
		MSM_RPM_STATUS_ID_MAP(8064, SUPPORTED_RESOURCES_1),
		MSM_RPM_STATUS_ID_MAP(8064, SUPPORTED_RESOURCES_2),
		MSM_RPM_STATUS_ID_MAP(8064, RESERVED_SUPPORTED_RESOURCES_0),
		MSM_RPM_STATUS_ID_MAP(8064, SEQUENCE),
		MSM_RPM_STATUS_ID_MAP(8064, RPM_CTL),
		MSM_RPM_STATUS_ID_MAP(8064, CXO_CLK),
		MSM_RPM_STATUS_ID_MAP(8064, PXO_CLK),
		MSM_RPM_STATUS_ID_MAP(8064, APPS_FABRIC_CLK),
		MSM_RPM_STATUS_ID_MAP(8064, SYSTEM_FABRIC_CLK),
		MSM_RPM_STATUS_ID_MAP(8064, MM_FABRIC_CLK),
		MSM_RPM_STATUS_ID_MAP(8064, DAYTONA_FABRIC_CLK),
		MSM_RPM_STATUS_ID_MAP(8064, SFPB_CLK),
		MSM_RPM_STATUS_ID_MAP(8064, CFPB_CLK),
		MSM_RPM_STATUS_ID_MAP(8064, MMFPB_CLK),
		MSM_RPM_STATUS_ID_MAP(8064, EBI1_CLK),
		MSM_RPM_STATUS_ID_MAP(8064, APPS_FABRIC_CFG_HALT),
		MSM_RPM_STATUS_ID_MAP(8064, APPS_FABRIC_CFG_CLKMOD),
		MSM_RPM_STATUS_ID_MAP(8064, APPS_FABRIC_CFG_IOCTL),
		MSM_RPM_STATUS_ID_MAP(8064, APPS_FABRIC_ARB),
		MSM_RPM_STATUS_ID_MAP(8064, SYS_FABRIC_CFG_HALT),
		MSM_RPM_STATUS_ID_MAP(8064, SYS_FABRIC_CFG_CLKMOD),
		MSM_RPM_STATUS_ID_MAP(8064, SYS_FABRIC_CFG_IOCTL),
		MSM_RPM_STATUS_ID_MAP(8064, SYSTEM_FABRIC_ARB),
		MSM_RPM_STATUS_ID_MAP(8064, MMSS_FABRIC_CFG_HALT),
		MSM_RPM_STATUS_ID_MAP(8064, MMSS_FABRIC_CFG_CLKMOD),
		MSM_RPM_STATUS_ID_MAP(8064, MMSS_FABRIC_CFG_IOCTL),
		MSM_RPM_STATUS_ID_MAP(8064, MM_FABRIC_ARB),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_S1_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_S1_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_S2_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_S2_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_S3_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_S3_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_S4_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_S4_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_S5_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_S5_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_S6_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_S6_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_S7_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_S7_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_S8_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_S8_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L1_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L1_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L2_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L2_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L3_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L3_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L4_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L4_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L5_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L5_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L6_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L6_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L7_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L7_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L8_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L8_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L9_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L9_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L10_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L10_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L11_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L11_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L12_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L12_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L13_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L13_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L14_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L14_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L15_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L15_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L16_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L16_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L17_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L17_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L18_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L18_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L19_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L19_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L20_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L20_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L21_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L21_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L22_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L22_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L23_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L23_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L24_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L24_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L25_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L25_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L26_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L26_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L27_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L27_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L28_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L28_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L29_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_L29_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_CLK1_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_CLK1_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_CLK2_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_CLK2_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_LVS1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_LVS2),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_LVS3),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_LVS4),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_LVS5),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_LVS6),
		MSM_RPM_STATUS_ID_MAP(8064, PM8921_LVS7),
		MSM_RPM_STATUS_ID_MAP(8064, NCP_0),
		MSM_RPM_STATUS_ID_MAP(8064, NCP_1),
		MSM_RPM_STATUS_ID_MAP(8064, CXO_BUFFERS),
		MSM_RPM_STATUS_ID_MAP(8064, USB_OTG_SWITCH),
		MSM_RPM_STATUS_ID_MAP(8064, HDMI_SWITCH),
		MSM_RPM_STATUS_ID_MAP(8064, DDR_DMM_0),
		MSM_RPM_STATUS_ID_MAP(8064, DDR_DMM_1),
		MSM_RPM_STATUS_ID_MAP(8064, EBI1_CH0_RANGE),
		MSM_RPM_STATUS_ID_MAP(8064, EBI1_CH1_RANGE),
		MSM_RPM_STATUS_ID_MAP(8064, PM8821_S1_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8821_S1_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8821_S2_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8821_S2_1),
		MSM_RPM_STATUS_ID_MAP(8064, PM8821_L1_0),
		MSM_RPM_STATUS_ID_MAP(8064, PM8821_L1_1),
		MSM_RPM_STATUS_ID_MAP(8064, VDDMIN_GPIO),
	},
	.target_ctrl_id = {
		MSM_RPM_CTRL_MAP(8064, VERSION_MAJOR),
		MSM_RPM_CTRL_MAP(8064, VERSION_MINOR),
		MSM_RPM_CTRL_MAP(8064, VERSION_BUILD),
		MSM_RPM_CTRL_MAP(8064, REQ_CTX_0),
		MSM_RPM_CTRL_MAP(8064, REQ_SEL_0),
		MSM_RPM_CTRL_MAP(8064, ACK_CTX_0),
		MSM_RPM_CTRL_MAP(8064, ACK_SEL_0),
	},
	.sel_invalidate = MSM_RPM_8064_SEL_INVALIDATE,
	.sel_notification = MSM_RPM_8064_SEL_NOTIFICATION,
	.sel_last = MSM_RPM_8064_SEL_LAST,
	.ver = {3, 0, 0},
};

struct platform_device apq8064_rpm_device = {
	.name   = "msm_rpm",
	.id     = -1,
};

static struct msm_rpmstats_platform_data msm_rpm_stat_pdata = {
	.version = 1,
};


static struct resource msm_rpm_stat_resource[] = {
	{
		.start	= 0x0010D204,
		.end	= 0x0010D204 + SZ_8K,
		.flags	= IORESOURCE_MEM,
		.name	= "phys_addr_base"
	},
};


struct platform_device apq8064_rpm_stat_device = {
	.name = "msm_rpm_stat",
	.id = -1,
	.resource = msm_rpm_stat_resource,
	.num_resources	= ARRAY_SIZE(msm_rpm_stat_resource),
	.dev	= {
		.platform_data = &msm_rpm_stat_pdata,
	}
};

static struct resource resources_rpm_master_stats[] = {
	{
		.start	= MSM8064_RPM_MASTER_STATS_BASE,
		.end	= MSM8064_RPM_MASTER_STATS_BASE + SZ_256,
		.flags	= IORESOURCE_MEM,
	},
};

static char *master_names[] = {
	"KPSS",
	"MPSS",
	"LPASS",
	"RIVA",
	"DSPS",
};

static struct msm_rpm_master_stats_platform_data msm_rpm_master_stat_pdata = {
	.masters = master_names,
	.nomasters = ARRAY_SIZE(master_names),
};

struct platform_device apq8064_rpm_master_stat_device = {
	.name = "msm_rpm_master_stat",
	.id = -1,
	.num_resources	= ARRAY_SIZE(resources_rpm_master_stats),
	.resource	= resources_rpm_master_stats,
	.dev = {
		.platform_data = &msm_rpm_master_stat_pdata,
	},
};

static struct msm_rpm_log_platform_data msm_rpm_log_pdata = {
	.phys_addr_base = 0x0010C000,
	.reg_offsets = {
		[MSM_RPM_LOG_PAGE_INDICES] = 0x00000080,
		[MSM_RPM_LOG_PAGE_BUFFER]  = 0x000000A0,
	},
	.phys_size = SZ_8K,
	.log_len = 6144,		  /* log's buffer length in bytes */
	.log_len_mask = (6144 >> 2) - 1,  /* length mask in units of u32 */
};

struct platform_device apq8064_rpm_log_device = {
	.name	= "msm_rpm_log",
	.id	= -1,
	.dev	= {
		.platform_data = &msm_rpm_log_pdata,
	},
};

/* Sensors DSPS platform data */

static struct dsps_clk_info dsps_clks[] = {};
static struct dsps_regulator_info dsps_regs[] = {};

/*
 * Note: GPIOs field is	intialized in run-time at the function
 * apq8064_init_dsps().
 */

#define PPSS_REG_PHYS_BASE	0x12080000

struct msm_dsps_platform_data msm_dsps_pdata_8064 = {
	.clks = dsps_clks,
	.clks_num = ARRAY_SIZE(dsps_clks),
	.gpios = NULL,
	.gpios_num = 0,
	.regs = dsps_regs,
	.regs_num = ARRAY_SIZE(dsps_regs),
	.dsps_pwr_ctl_en = 1,
	.signature = DSPS_SIGNATURE,
};

static struct resource msm_dsps_resources[] = {
	{
		.start = PPSS_REG_PHYS_BASE,
		.end   = PPSS_REG_PHYS_BASE + SZ_8K - 1,
		.name  = "ppss_reg",
		.flags = IORESOURCE_MEM,
	},
};

struct platform_device msm_dsps_device_8064 = {
	.name          = "msm_dsps",
	.id            = 0,
	.num_resources = ARRAY_SIZE(msm_dsps_resources),
	.resource      = msm_dsps_resources,
	.dev.platform_data = &msm_dsps_pdata_8064,
};

#ifdef CONFIG_MSM_MPM
static uint16_t msm_mpm_irqs_m2a[MSM_MPM_NR_MPM_IRQS] __initdata = {
	[1] = MSM_GPIO_TO_INT(26),
	[2] = MSM_GPIO_TO_INT(88),
	[4] = MSM_GPIO_TO_INT(73),
	[5] = MSM_GPIO_TO_INT(74),
	[6] = MSM_GPIO_TO_INT(75),
	[7] = MSM_GPIO_TO_INT(76),
	[8] = MSM_GPIO_TO_INT(77),
	[9] = MSM_GPIO_TO_INT(36),
	[10] = MSM_GPIO_TO_INT(84),
	[11] = MSM_GPIO_TO_INT(7),
	[12] = MSM_GPIO_TO_INT(11),
	[13] = MSM_GPIO_TO_INT(52),
	[14] = MSM_GPIO_TO_INT(15),
	[15] = MSM_GPIO_TO_INT(83),
	[16] = USB3_HS_IRQ,
	[19] = MSM_GPIO_TO_INT(61),
	[20] = MSM_GPIO_TO_INT(58),
	[23] = MSM_GPIO_TO_INT(65),
	[24] = MSM_GPIO_TO_INT(63),
	[25] = USB1_HS_IRQ,
	[27] = HDMI_IRQ,
	[29] = MSM_GPIO_TO_INT(22),
	[30] = MSM_GPIO_TO_INT(72),
	[31] = USB4_HS_IRQ,
	[33] = MSM_GPIO_TO_INT(44),
	[34] = MSM_GPIO_TO_INT(39),
	[35] = MSM_GPIO_TO_INT(19),
	[36] = MSM_GPIO_TO_INT(23),
	[37] = MSM_GPIO_TO_INT(41),
	[38] = MSM_GPIO_TO_INT(30),
	[41] = MSM_GPIO_TO_INT(42),
	[42] = MSM_GPIO_TO_INT(56),
	[43] = MSM_GPIO_TO_INT(55),
	[44] = MSM_GPIO_TO_INT(50),
	[45] = MSM_GPIO_TO_INT(49),
	[46] = MSM_GPIO_TO_INT(47),
	[47] = MSM_GPIO_TO_INT(45),
	[48] = MSM_GPIO_TO_INT(38),
	[49] = MSM_GPIO_TO_INT(34),
	[50] = MSM_GPIO_TO_INT(32),
	[51] = MSM_GPIO_TO_INT(29),
	[52] = MSM_GPIO_TO_INT(18),
	[53] = MSM_GPIO_TO_INT(10),
	[54] = MSM_GPIO_TO_INT(81),
	[55] = MSM_GPIO_TO_INT(6),
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
	PM8821_SEC_IRQ_N,
};

struct msm_mpm_device_data apq8064_mpm_dev_data __initdata = {
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

/* AP2MDM_SOFT_RESET is implemented by the PON_RESET_N gpio */
#define MDM2AP_ERRFATAL			19
#define AP2MDM_ERRFATAL			18
#define MDM2AP_STATUS			49
#define AP2MDM_STATUS			48
#define AP2MDM_SOFT_RESET		27
#define I2S_AP2MDM_SOFT_RESET		0
#define AP2MDM_WAKEUP			35
#define I2S_AP2MDM_WAKEUP		44
#define MDM2AP_PBLRDY			46
#define I2S_MDM2AP_PBLRDY		81

static struct resource mdm_resources[] = {
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
		.start	= AP2MDM_SOFT_RESET,
		.end	= AP2MDM_SOFT_RESET,
		.name	= "AP2MDM_SOFT_RESET",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= AP2MDM_WAKEUP,
		.end	= AP2MDM_WAKEUP,
		.name	= "AP2MDM_WAKEUP",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= MDM2AP_PBLRDY,
		.end	= MDM2AP_PBLRDY,
		.name	= "MDM2AP_PBLRDY",
		.flags	= IORESOURCE_IO,
	},
};

static struct resource i2s_mdm_resources[] = {
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
		.start	= I2S_AP2MDM_SOFT_RESET,
		.end	= I2S_AP2MDM_SOFT_RESET,
		.name	= "AP2MDM_SOFT_RESET",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= I2S_AP2MDM_WAKEUP,
		.end	= I2S_AP2MDM_WAKEUP,
		.name	= "AP2MDM_WAKEUP",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= I2S_MDM2AP_PBLRDY,
		.end	= I2S_MDM2AP_PBLRDY,
		.name	= "MDM2AP_PBLRDY",
		.flags	= IORESOURCE_IO,
	},
};

struct platform_device mdm_8064_device = {
	.name		= "mdm2_modem",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(mdm_resources),
	.resource	= mdm_resources,
};

struct platform_device i2s_mdm_8064_device = {
	.name		= "mdm2_modem",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(i2s_mdm_resources),
	.resource	= i2s_mdm_resources,
};

static struct msm_dcvs_sync_rule apq8064_dcvs_sync_rules[] = {
	{1026000,	400000},
	{384000,	200000},
	{0,		128000},
};

static struct msm_dcvs_platform_data apq8064_dcvs_data = {
	.sync_rules	= apq8064_dcvs_sync_rules,
	.num_sync_rules = ARRAY_SIZE(apq8064_dcvs_sync_rules),
	.gpu_max_nom_khz = 320000,
};

struct platform_device apq8064_dcvs_device = {
	.name		= "dcvs",
	.id		= -1,
	.dev		= {
		.platform_data = &apq8064_dcvs_data,
	},
};

static struct msm_dcvs_core_info apq8064_core_info = {
	.num_cores		= 4,
	.sensors		= (int[]){7, 8, 9, 10},
	.thermal_poll_ms	= 60000,
	.core_param		= {
		.core_type	= MSM_DCVS_CORE_TYPE_CPU,
	},
	.algo_param		= {
		.disable_pc_threshold		= 1458000,
		.em_win_size_min_us		= 100000,
		.em_win_size_max_us		= 300000,
		.em_max_util_pct		= 97,
		.group_id			= 1,
		.max_freq_chg_time_us		= 100000,
		.slack_mode_dynamic		= 0,
		.slack_weight_thresh_pct	= 3,
		.slack_time_min_us		= 45000,
		.slack_time_max_us		= 45000,
		.ss_no_corr_below_freq		= 0,
		.ss_win_size_min_us		= 1000000,
		.ss_win_size_max_us		= 1000000,
		.ss_util_pct			= 95,
	},
	.energy_coeffs		= {
		.active_coeff_a		= 336,
		.active_coeff_b		= 0,
		.active_coeff_c		= 0,

		.leakage_coeff_a	= -17720,
		.leakage_coeff_b	= 37,
		.leakage_coeff_c	= 3329,
		.leakage_coeff_d	= -277,
	},
	.power_param		= {
		.current_temp	= 25,
		.num_freq	= 0, /* set at runtime */
	}
};

#define APQ8064_LPM_LATENCY  1000 /* >100 usec for WFI */

static struct msm_gov_platform_data gov_platform_data = {
	.info = &apq8064_core_info,
	.latency = APQ8064_LPM_LATENCY,
};

struct platform_device apq8064_msm_gov_device = {
	.name = "msm_dcvs_gov",
	.id = -1,
	.dev = {
		.platform_data = &gov_platform_data,
	},
};

static struct msm_mpd_algo_param apq8064_mpd_algo_param = {
	.em_win_size_min_us	= 10000,
	.em_win_size_max_us	= 100000,
	.em_max_util_pct	= 90,
	.online_util_pct_min	= 60,
	.slack_time_min_us	= 50000,
	.slack_time_max_us	= 100000,
};

struct platform_device apq8064_msm_mpd_device = {
	.name	= "msm_mpdecision",
	.id	= -1,
	.dev	= {
		.platform_data = &apq8064_mpd_algo_param,
	},
};

#ifdef CONFIG_MSM_VCAP
#define VCAP_HW_BASE         0x05900000

static struct msm_bus_vectors vcap_init_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_CAP,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors vcap_480_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_CAP,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 480 * 720 * 3 * 60,
		.ib = 480 * 720 * 3 * 60 * 1.5,
	},
};

static struct msm_bus_vectors vcap_576_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_CAP,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 576 * 720 * 3 * 60,
		.ib = 576 * 720 * 3 * 60 * 1.5,
	},
};

static struct msm_bus_vectors vcap_720_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_CAP,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 1280 * 720 * 3 * 60,
		.ib = 1280 * 720 * 3 * 60 * 1.5,
	},
};

static struct msm_bus_vectors vcap_1080_vectors[]  = {
	{
		.src = MSM_BUS_MASTER_VIDEO_CAP,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 1920 * 1080 * 10 * 60,
		.ib = 1920 * 1080 * 10 * 60 * 1.5,
	},
};

static struct msm_bus_paths vcap_bus_usecases[]  = {
	{
		ARRAY_SIZE(vcap_init_vectors),
		vcap_init_vectors,
	},
	{
		ARRAY_SIZE(vcap_480_vectors),
		vcap_480_vectors,
	},
	{
		ARRAY_SIZE(vcap_576_vectors),
		vcap_576_vectors,
	},
	{
		ARRAY_SIZE(vcap_720_vectors),
		vcap_720_vectors,
	},
	{
		ARRAY_SIZE(vcap_1080_vectors),
		vcap_1080_vectors,
	},
};

static struct msm_bus_scale_pdata vcap_axi_client_pdata = {
	vcap_bus_usecases,
	ARRAY_SIZE(vcap_bus_usecases),
};

static struct resource msm_vcap_resources[] = {
	{
		.name	= "vcap",
		.start	= VCAP_HW_BASE,
		.end	= VCAP_HW_BASE + SZ_1M - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "vc_irq",
		.start	= VCAP_VC,
		.end	= VCAP_VC,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "vp_irq",
		.start	= VCAP_VP,
		.end	= VCAP_VP,
		.flags	= IORESOURCE_IRQ,
	},
};

static unsigned vcap_gpios[] = {
	2, 3, 4, 5, 6, 7, 8, 9, 10,
	11, 12, 13, 18, 19, 20, 21,
	22, 23, 24, 25, 26, 80, 82,
	83, 84, 85, 86, 87,
};

static struct vcap_platform_data vcap_pdata = {
	.gpios = vcap_gpios,
	.num_gpios = ARRAY_SIZE(vcap_gpios),
	.bus_client_pdata = &vcap_axi_client_pdata
};

struct platform_device msm8064_device_vcap = {
	.name           = "msm_vcap",
	.id             = 0,
	.resource       = msm_vcap_resources,
	.num_resources  = ARRAY_SIZE(msm_vcap_resources),
	.dev = {
		.platform_data = &vcap_pdata,
	},
};
#endif

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

struct platform_device apq8064_device_cache_erp = {
	.name		= "msm_cache_erp",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(msm_cache_erp_resources),
	.resource	= msm_cache_erp_resources,
};

#define CORESIGHT_PHYS_BASE		0x01A00000
#define CORESIGHT_FUNNEL_PHYS_BASE	(CORESIGHT_PHYS_BASE + 0x4000)
#define CORESIGHT_ETM2_PHYS_BASE	(CORESIGHT_PHYS_BASE + 0x1E000)
#define CORESIGHT_ETM3_PHYS_BASE	(CORESIGHT_PHYS_BASE + 0x1F000)

static struct resource coresight_funnel_resources[] = {
	{
		.name  = "funnel-base",
		.start = CORESIGHT_FUNNEL_PHYS_BASE,
		.end   = CORESIGHT_FUNNEL_PHYS_BASE + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
};

static const int coresight_funnel_outports[] = { 0, 1 };
static const int coresight_funnel_child_ids[] = { 0, 1 };
static const int coresight_funnel_child_ports[] = { 0, 0 };

static struct coresight_platform_data coresight_funnel_pdata = {
	.id		= 2,
	.name		= "coresight-funnel",
	.nr_inports	= 8,
	.outports	= coresight_funnel_outports,
	.child_ids	= coresight_funnel_child_ids,
	.child_ports	= coresight_funnel_child_ports,
	.nr_outports	= ARRAY_SIZE(coresight_funnel_outports),
};

struct platform_device apq8064_coresight_funnel_device = {
	.name          = "coresight-funnel",
	.id            = 0,
	.num_resources = ARRAY_SIZE(coresight_funnel_resources),
	.resource      = coresight_funnel_resources,
	.dev = {
		.platform_data = &coresight_funnel_pdata,
	},
};

static struct resource coresight_etm2_resources[] = {
	{
		.name  = "etm-base",
		.start = CORESIGHT_ETM2_PHYS_BASE,
		.end   = CORESIGHT_ETM2_PHYS_BASE + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
};

static const int coresight_etm2_outports[] = { 0 };
static const int coresight_etm2_child_ids[] = { 2 };
static const int coresight_etm2_child_ports[] = { 4 };

static struct coresight_platform_data coresight_etm2_pdata = {
	.id		= 6,
	.name		= "coresight-etm2",
	.nr_inports	= 0,
	.outports	= coresight_etm2_outports,
	.child_ids	= coresight_etm2_child_ids,
	.child_ports	= coresight_etm2_child_ports,
	.nr_outports	= ARRAY_SIZE(coresight_etm2_outports),
};

struct platform_device coresight_etm2_device = {
	.name          = "coresight-etm",
	.id            = 2,
	.num_resources = ARRAY_SIZE(coresight_etm2_resources),
	.resource      = coresight_etm2_resources,
	.dev = {
		.platform_data = &coresight_etm2_pdata,
	},
};

static struct resource coresight_etm3_resources[] = {
	{
		.name  = "etm-base",
		.start = CORESIGHT_ETM3_PHYS_BASE,
		.end   = CORESIGHT_ETM3_PHYS_BASE + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
};

static const int coresight_etm3_outports[] = { 0 };
static const int coresight_etm3_child_ids[] = { 2 };
static const int coresight_etm3_child_ports[] = { 5 };

static struct coresight_platform_data coresight_etm3_pdata = {
	.id		= 7,
	.name		= "coresight-etm3",
	.nr_inports	= 0,
	.outports	= coresight_etm3_outports,
	.child_ids	= coresight_etm3_child_ids,
	.child_ports	= coresight_etm3_child_ports,
	.nr_outports	= ARRAY_SIZE(coresight_etm3_outports),
};

struct platform_device coresight_etm3_device = {
	.name          = "coresight-etm",
	.id            = 3,
	.num_resources = ARRAY_SIZE(coresight_etm3_resources),
	.resource      = coresight_etm3_resources,
	.dev = {
		.platform_data = &coresight_etm3_pdata,
	},
};

struct msm_iommu_domain_name apq8064_iommu_ctx_names[] = {
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
	/* Rotator src*/
	{
		.name = "rot_src",
		.domain = ROTATOR_SRC_DOMAIN,
	},
	/* Rotator dst */
	{
		.name = "rot_dst",
		.domain = ROTATOR_DST_DOMAIN,
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

static struct mem_pool apq8064_video_pools[] =  {
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

static struct mem_pool apq8064_camera_pools[] =  {
	[GEN_POOL] =
	/* One address space for camera */
		{
			.paddr	= SZ_128K,
			.size	= SZ_2G - SZ_128K,
		},
};

static struct mem_pool apq8064_display_read_pools[] =  {
	[GEN_POOL] =
	/* One address space for display reads */
		{
			.paddr	= SZ_128K,
			.size	= SZ_2G - SZ_128K,
		},
};

static struct mem_pool apq8064_display_write_pools[] =  {
	[GEN_POOL] =
	/* One address space for display writes */
		{
			.paddr	= SZ_128K,
			.size	= SZ_2G - SZ_128K,
		},
};

static struct mem_pool apq8064_rotator_src_pools[] =  {
	[GEN_POOL] =
	/* One address space for rotator src */
		{
			.paddr	= SZ_128K,
			.size	= SZ_2G - SZ_128K,
		},
};

static struct mem_pool apq8064_rotator_dst_pools[] =  {
	[GEN_POOL] =
	/* One address space for rotator dst */
		{
			.paddr	= SZ_128K,
			.size	= SZ_2G - SZ_128K,
		},
};

static struct msm_iommu_domain apq8064_iommu_domains[] = {
		[VIDEO_DOMAIN] = {
			.iova_pools = apq8064_video_pools,
			.npools = ARRAY_SIZE(apq8064_video_pools),
		},
		[CAMERA_DOMAIN] = {
			.iova_pools = apq8064_camera_pools,
			.npools = ARRAY_SIZE(apq8064_camera_pools),
		},
		[DISPLAY_READ_DOMAIN] = {
			.iova_pools = apq8064_display_read_pools,
			.npools = ARRAY_SIZE(apq8064_display_read_pools),
		},
		[DISPLAY_WRITE_DOMAIN] = {
			.iova_pools = apq8064_display_write_pools,
			.npools = ARRAY_SIZE(apq8064_display_write_pools),
		},
		[ROTATOR_SRC_DOMAIN] = {
			.iova_pools = apq8064_rotator_src_pools,
			.npools = ARRAY_SIZE(apq8064_rotator_src_pools),
		},
		[ROTATOR_DST_DOMAIN] = {
			.iova_pools = apq8064_rotator_dst_pools,
			.npools = ARRAY_SIZE(apq8064_rotator_dst_pools),
		},
};

struct iommu_domains_pdata apq8064_iommu_domain_pdata = {
	.domains = apq8064_iommu_domains,
	.ndomains = ARRAY_SIZE(apq8064_iommu_domains),
	.domain_names = apq8064_iommu_ctx_names,
	.nnames = ARRAY_SIZE(apq8064_iommu_ctx_names),
	.domain_alloc_flags = 0,
};

struct platform_device apq8064_iommu_domain_device = {
	.name = "iommu_domains",
	.id = -1,
	.dev = {
		.platform_data = &apq8064_iommu_domain_pdata,
	}
};

struct msm_rtb_platform_data apq8064_rtb_pdata = {
	.size = SZ_1M,
};

static int __init msm_rtb_set_buffer_size(char *p)
{
	int s;

	s = memparse(p, NULL);
	apq8064_rtb_pdata.size = ALIGN(s, SZ_4K);
	return 0;
}
early_param("msm_rtb_size", msm_rtb_set_buffer_size);

struct platform_device apq8064_rtb_device = {
	.name           = "msm_rtb",
	.id             = -1,
	.dev            = {
		.platform_data = &apq8064_rtb_pdata,
	},
};

#define APQ8064_L1_SIZE  SZ_1M
/*
 * The actual L2 size is smaller but we need a larger buffer
 * size to store other dump information
 */
#define APQ8064_L2_SIZE  SZ_8M

struct msm_cache_dump_platform_data apq8064_cache_dump_pdata = {
	.l2_size = APQ8064_L2_SIZE,
	.l1_size = APQ8064_L1_SIZE,
};

struct platform_device apq8064_cache_dump_device = {
	.name           = "msm_cache_dump",
	.id             = -1,
	.dev            = {
		.platform_data = &apq8064_cache_dump_pdata,
	},
};

struct dev_avtimer_data dev_avtimer_pdata = {
	.avtimer_msw_phy_addr = AVTIMER_MSW_PHYSICAL_ADDRESS,
	.avtimer_lsw_phy_addr = AVTIMER_LSW_PHYSICAL_ADDRESS,
};
