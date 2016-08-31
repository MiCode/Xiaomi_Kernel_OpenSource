/*
 * Copyright (C) 2010,2011 Google, Inc.
 *
 * Author:
 *	Colin Cross <ccross@android.com>
 *	Erik Gilling <ccross@android.com>
 *
 * Copyright (c) 2010-2014, NVIDIA CORPORATION.  All rights reserved.
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


#include <linux/resource.h>
#include <linux/dma-mapping.h>
#include <linux/fsl_devices.h>
#include <linux/serial_8250.h>
#include <linux/i2c-tegra.h>
#include <linux/mipi-bif-tegra.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/tegra_avp.h>
#include <linux/nvhost.h>
#include <linux/clk.h>
#include <linux/tegra-soc.h>
#include <mach/irqs.h>
#include <linux/usb/tegra_usb_phy.h>
#include <mach/tegra_smmu.h>
#include <mach/tegra-swgid.h>
#include <linux/dma-contiguous.h>

#ifdef CONFIG_TEGRA_WAKEUP_MONITOR
#include <mach/tegra_wakeup_monitor.h>
#endif

#ifdef CONFIG_PLATFORM_ENABLE_IOMMU
#include <asm/dma-iommu.h>
#endif

#include "gpio-names.h"
#include "iomap.h"
#include "devices.h"
#include "board.h"
#include "tegra_ptm.h"

#define TEGRA_DMA_REQ_SEL_I2S_1			2
#define TEGRA_DMA_REQ_SEL_SPD_I			3
#define TEGRA_DMA_REQ_SEL_I2S2_1		7
#define TEGRA_DMA_REQ_SEL_SPI			11
#define TEGRA_DMA_REQ_SEL_DTV			TEGRA_DMA_REQ_SEL_SPI

static struct resource emc_resource[] = {
	[0] = {
		.start	= TEGRA_EMC_BASE,
		.end	= TEGRA_EMC_BASE + TEGRA_EMC_SIZE-1,
		.flags	= IORESOURCE_MEM,
	}
};

struct platform_device tegra_emc_device = {
	.name		= "tegra-emc",
	.id		= -1,
	.resource	= emc_resource,
	.num_resources	= ARRAY_SIZE(emc_resource),
};

static struct resource gpio_resource[] = {
	[0] = {
		.start	= TEGRA_GPIO_BASE,
		.end	= TEGRA_GPIO_BASE + TEGRA_GPIO_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_GPIO1,
		.end	= INT_GPIO1,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= INT_GPIO2,
		.end	= INT_GPIO2,
		.flags	= IORESOURCE_IRQ,
	},
	[3] = {
		.start	= INT_GPIO3,
		.end	= INT_GPIO3,
		.flags	= IORESOURCE_IRQ,
	},
	[4] = {
		.start	= INT_GPIO4,
		.end	= INT_GPIO4,
		.flags	= IORESOURCE_IRQ,
	},
	[5] = {
		.start	= INT_GPIO5,
		.end	= INT_GPIO5,
		.flags	= IORESOURCE_IRQ,
	},
	[6] = {
		.start	= INT_GPIO6,
		.end	= INT_GPIO6,
		.flags	= IORESOURCE_IRQ,
	},
	[7] = {
		.start	= INT_GPIO7,
		.end	= INT_GPIO7,
		.flags	= IORESOURCE_IRQ,
	},
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	[8] = {
		.start	= INT_GPIO8,
		.end	= INT_GPIO8,
		.flags	= IORESOURCE_IRQ,
	},
#endif
};

struct platform_device tegra_gpio_device = {
	.name		= "tegra-gpio",
	.id		= -1,
	.resource	= gpio_resource,
	.num_resources	= ARRAY_SIZE(gpio_resource),
};

static struct resource tegra124_pinctrl_resource[] = {
	[0] = {
		/* Drive registers */
		.start	= TEGRA_APB_MISC_BASE + 0x868,
		.end	= TEGRA_APB_MISC_BASE + 0x9c8 + 3,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		/* Mux registers */
		.start	= TEGRA_APB_MISC_BASE + 0x3000,
		.end	= TEGRA_APB_MISC_BASE + 0x3430 + 3,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device tegra124_pinctrl_device = {
	.name		= "tegra124-pinctrl",
	.id		= -1,
	.resource	= tegra124_pinctrl_resource,
	.num_resources	= ARRAY_SIZE(tegra124_pinctrl_resource),
};

static struct resource tegra114_pinctrl_resource[] = {
	[0] = {
		/* Drive registers */
		.start	= TEGRA_APB_MISC_BASE + 0x868,
		.end	= TEGRA_APB_MISC_BASE + 0x938 + 3,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		/* Mux registers */
		.start	= TEGRA_APB_MISC_BASE + 0x3000,
		.end	= TEGRA_APB_MISC_BASE + 0x3408 + 3,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device tegra114_pinctrl_device = {
	.name		= "tegra114-pinctrl",
	.id		= -1,
	.resource	= tegra114_pinctrl_resource,
	.num_resources	= ARRAY_SIZE(tegra114_pinctrl_resource),
};

static struct resource apbdma_resource[] = {
	[0] = {
		.start	= TEGRA_APB_DMA_BASE,
		.end	= TEGRA_APB_DMA_BASE + TEGRA_APB_DMA_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_APB_DMA_CH0,
		.end	= INT_APB_DMA_CH0,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= INT_APB_DMA_CH1,
		.end	= INT_APB_DMA_CH1,
		.flags	= IORESOURCE_IRQ,
	},
	[3] = {
		.start	= INT_APB_DMA_CH2,
		.end	= INT_APB_DMA_CH2,
		.flags	= IORESOURCE_IRQ,
	},
	[4] = {
		.start	= INT_APB_DMA_CH3,
		.end	= INT_APB_DMA_CH3,
		.flags	= IORESOURCE_IRQ,
	},
	[5] = {
		.start	= INT_APB_DMA_CH4,
		.end	= INT_APB_DMA_CH4,
		.flags	= IORESOURCE_IRQ,
	},
	[6] = {
		.start	= INT_APB_DMA_CH5,
		.end	= INT_APB_DMA_CH5,
		.flags	= IORESOURCE_IRQ,
	},
	[7] = {
		.start	= INT_APB_DMA_CH6,
		.end	= INT_APB_DMA_CH6,
		.flags	= IORESOURCE_IRQ,
	},
	[8] = {
		.start	= INT_APB_DMA_CH7,
		.end	= INT_APB_DMA_CH7,
		.flags	= IORESOURCE_IRQ,
	},
	[9] = {
		.start	= INT_APB_DMA_CH8,
		.end	= INT_APB_DMA_CH8,
		.flags	= IORESOURCE_IRQ,
	},
	[10] = {
		.start	= INT_APB_DMA_CH9,
		.end	= INT_APB_DMA_CH9,
		.flags	= IORESOURCE_IRQ,
	},
	[11] = {
		.start	= INT_APB_DMA_CH10,
		.end	= INT_APB_DMA_CH10,
		.flags	= IORESOURCE_IRQ,
	},
	[12] = {
		.start	= INT_APB_DMA_CH11,
		.end	= INT_APB_DMA_CH11,
		.flags	= IORESOURCE_IRQ,
	},
	[13] = {
		.start	= INT_APB_DMA_CH12,
		.end	= INT_APB_DMA_CH12,
		.flags	= IORESOURCE_IRQ,
	},
	[14] = {
		.start	= INT_APB_DMA_CH13,
		.end	= INT_APB_DMA_CH13,
		.flags	= IORESOURCE_IRQ,
	},
	[15] = {
		.start	= INT_APB_DMA_CH14,
		.end	= INT_APB_DMA_CH14,
		.flags	= IORESOURCE_IRQ,
	},
	[16] = {
		.start	= INT_APB_DMA_CH15,
		.end	= INT_APB_DMA_CH15,
		.flags	= IORESOURCE_IRQ,
	},
	[17] = {
		.start	= INT_APB_DMA_CH16,
		.end	= INT_APB_DMA_CH16,
		.flags	= IORESOURCE_IRQ,
	},
	[18] = {
		.start	= INT_APB_DMA_CH17,
		.end	= INT_APB_DMA_CH17,
		.flags	= IORESOURCE_IRQ,
	},
	[19] = {
		.start	= INT_APB_DMA_CH18,
		.end	= INT_APB_DMA_CH18,
		.flags	= IORESOURCE_IRQ,
	},
	[20] = {
		.start	= INT_APB_DMA_CH19,
		.end	= INT_APB_DMA_CH19,
		.flags	= IORESOURCE_IRQ,
	},
	[21] = {
		.start	= INT_APB_DMA_CH20,
		.end	= INT_APB_DMA_CH20,
		.flags	= IORESOURCE_IRQ,
	},
	[22] = {
		.start	= INT_APB_DMA_CH21,
		.end	= INT_APB_DMA_CH21,
		.flags	= IORESOURCE_IRQ,
	},
	[23] = {
		.start	= INT_APB_DMA_CH22,
		.end	= INT_APB_DMA_CH22,
		.flags	= IORESOURCE_IRQ,
	},
	[24] = {
		.start	= INT_APB_DMA_CH23,
		.end	= INT_APB_DMA_CH23,
		.flags	= IORESOURCE_IRQ,
	},
	[25] = {
		.start	= INT_APB_DMA_CH24,
		.end	= INT_APB_DMA_CH24,
		.flags	= IORESOURCE_IRQ,
	},
	[26] = {
		.start	= INT_APB_DMA_CH25,
		.end	= INT_APB_DMA_CH25,
		.flags	= IORESOURCE_IRQ,
	},
	[27] = {
		.start	= INT_APB_DMA_CH26,
		.end	= INT_APB_DMA_CH26,
		.flags	= IORESOURCE_IRQ,
	},
	[28] = {
		.start	= INT_APB_DMA_CH27,
		.end	= INT_APB_DMA_CH27,
		.flags	= IORESOURCE_IRQ,
	},
	[29] = {
		.start	= INT_APB_DMA_CH28,
		.end	= INT_APB_DMA_CH28,
		.flags	= IORESOURCE_IRQ,
	},
	[30] = {
		.start	= INT_APB_DMA_CH29,
		.end	= INT_APB_DMA_CH29,
		.flags	= IORESOURCE_IRQ,
	},
	[31] = {
		.start	= INT_APB_DMA_CH30,
		.end	= INT_APB_DMA_CH30,
		.flags	= IORESOURCE_IRQ,
	},
	[32] = {
		.start	= INT_APB_DMA_CH31,
		.end	= INT_APB_DMA_CH31,
		.flags	= IORESOURCE_IRQ,
	},

};

struct platform_device tegra_apbdma = {
	.name		= "tegra124-apbdma",
	.id 		= -1,
	.resource	= apbdma_resource,
	.num_resources	= ARRAY_SIZE(apbdma_resource),
};

static struct resource i2c_resource1[] = {
	[0] = {
		.start	= INT_I2C,
		.end	= INT_I2C,
		.flags	= IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_I2C_BASE,
		.end	= TEGRA_I2C_BASE + TEGRA_I2C_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource i2c_resource2[] = {
	[0] = {
		.start	= INT_I2C2,
		.end	= INT_I2C2,
		.flags	= IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_I2C2_BASE,
		.end	= TEGRA_I2C2_BASE + TEGRA_I2C2_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource i2c_resource3[] = {
	[0] = {
		.start	= INT_I2C3,
		.end	= INT_I2C3,
		.flags	= IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_I2C3_BASE,
		.end	= TEGRA_I2C3_BASE + TEGRA_I2C3_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
static struct resource i2c_resource4[] = {
	[0] = {
		.start	= INT_DVC,
		.end	= INT_DVC,
		.flags	= IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_DVC_BASE,
		.end	= TEGRA_DVC_BASE + TEGRA_DVC_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

#else
static struct resource i2c_resource4[] = {
	[0] = {
		.start  = INT_I2C4,
		.end    = INT_I2C4,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_I2C4_BASE,
		.end	= TEGRA_I2C4_BASE + TEGRA_I2C4_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource i2c_resource5[] = {
	[0] = {
		.start  = INT_I2C5,
		.end    = INT_I2C5,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_I2C5_BASE,
		.end	= TEGRA_I2C5_BASE + TEGRA_I2C5_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};
#endif

#if defined(CONFIG_ARCH_TEGRA_14x_SOC) || defined(CONFIG_ARCH_TEGRA_12x_SOC)
static struct resource i2c_resource6[] = {
	[0] = {
		.start  = INT_I2C6,
		.end    = INT_I2C6,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_I2C6_BASE,
		.end	= TEGRA_I2C6_BASE + TEGRA_I2C6_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};
#endif

static struct tegra_i2c_platform_data tegra_i2c1_platform_data = {
	.bus_clk_rate   = 400000,
};

static struct tegra_i2c_platform_data tegra_i2c2_platform_data = {
	.bus_clk_rate   = 400000,
};

static struct tegra_i2c_platform_data tegra_i2c3_platform_data = {
	.bus_clk_rate   = 400000,
};

static struct tegra_i2c_platform_data tegra_dvc_platform_data = {
	.bus_clk_rate   = 400000,
};

struct platform_device tegra_i2c_device1 = {
	.name		= "tegra-i2c",
	.id		= 0,
	.resource	= i2c_resource1,
	.num_resources	= ARRAY_SIZE(i2c_resource1),
	.dev = {
		.platform_data = &tegra_i2c1_platform_data,
	},
};

struct platform_device tegra_i2c_device2 = {
	.name		= "tegra-i2c",
	.id		= 1,
	.resource	= i2c_resource2,
	.num_resources	= ARRAY_SIZE(i2c_resource2),
	.dev = {
		.platform_data = &tegra_i2c2_platform_data,
	},
};

struct platform_device tegra_i2c_device3 = {
	.name		= "tegra-i2c",
	.id		= 2,
	.resource	= i2c_resource3,
	.num_resources	= ARRAY_SIZE(i2c_resource3),
	.dev = {
		.platform_data = &tegra_i2c3_platform_data,
	},
};

struct platform_device tegra_i2c_device4 = {
	.name		= "tegra-i2c",
	.id		= 3,
	.resource	= i2c_resource4,
	.num_resources	= ARRAY_SIZE(i2c_resource4),
	.dev = {
		.platform_data = &tegra_dvc_platform_data,
	},
};

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
struct platform_device tegra_i2c_device5 = {
	.name		= "tegra-i2c",
	.id		= 4,
	.resource	= i2c_resource5,
	.num_resources	= ARRAY_SIZE(i2c_resource5),
	.dev = {
		.platform_data = 0,
	},
};
#endif

struct platform_device tegra11_i2c_device1 = {
	.name		= "tegra11-i2c",
	.id		= 0,
	.resource	= i2c_resource1,
	.num_resources	= ARRAY_SIZE(i2c_resource1),
	.dev = {
		.platform_data = 0,
	},
};

struct platform_device tegra11_i2c_device2 = {
	.name		= "tegra11-i2c",
	.id		= 1,
	.resource	= i2c_resource2,
	.num_resources	= ARRAY_SIZE(i2c_resource2),
	.dev = {
		.platform_data = 0,
	},
};

struct platform_device tegra11_i2c_device3 = {
	.name		= "tegra11-i2c",
	.id		= 2,
	.resource	= i2c_resource3,
	.num_resources	= ARRAY_SIZE(i2c_resource3),
	.dev = {
		.platform_data = 0,
	},
};

struct platform_device tegra11_i2c_device4 = {
	.name		= "tegra11-i2c",
	.id		= 3,
	.resource	= i2c_resource4,
	.num_resources	= ARRAY_SIZE(i2c_resource4),
	.dev = {
		.platform_data = 0,
	},
};

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
struct platform_device tegra11_i2c_device5 = {
	.name		= "tegra11-i2c",
	.id		= 4,
	.resource	= i2c_resource5,
	.num_resources	= ARRAY_SIZE(i2c_resource5),
	.dev = {
		.platform_data = 0,
	},
};
#endif

#if defined(CONFIG_ARCH_TEGRA_14x_SOC)

struct platform_device tegra14_i2c_device1 = {
	.name		= "tegra14-i2c",
	.id		= 0,
	.resource	= i2c_resource1,
	.num_resources	= ARRAY_SIZE(i2c_resource1),
	.dev = {
		.platform_data = 0,
	},
};

struct platform_device tegra14_i2c_device2 = {
	.name		= "tegra14-i2c",
	.id		= 1,
	.resource	= i2c_resource2,
	.num_resources	= ARRAY_SIZE(i2c_resource2),
	.dev = {
		.platform_data = 0,
	},
};

struct platform_device tegra14_i2c_device3 = {
	.name		= "tegra14-i2c",
	.id		= 2,
	.resource	= i2c_resource3,
	.num_resources	= ARRAY_SIZE(i2c_resource3),
	.dev = {
		.platform_data = 0,
	},
};

struct platform_device tegra14_i2c_device4 = {
	.name		= "tegra14-i2c",
	.id		= 3,
	.resource	= i2c_resource4,
	.num_resources	= ARRAY_SIZE(i2c_resource4),
	.dev = {
		.platform_data = 0,
	},
};

struct platform_device tegra14_i2c_device5 = {
	.name		= "tegra14-i2c",
	.id		= 4,
	.resource	= i2c_resource5,
	.num_resources	= ARRAY_SIZE(i2c_resource5),
	.dev = {
		.platform_data = 0,
	},
};

struct platform_device tegra14_i2c_device6 = {
	.name		= "tegra14-i2c",
	.id		= 5,
	.resource	= i2c_resource6,
	.num_resources	= ARRAY_SIZE(i2c_resource6),
	.dev = {
		.platform_data = 0,
	},
};

#endif

#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
struct platform_device tegra12_i2c_device1 = {
	.name		= "tegra12-i2c",
	.id		= 0,
	.resource	= i2c_resource1,
	.num_resources	= ARRAY_SIZE(i2c_resource1),
	.dev = {
		.platform_data = 0,
	},
};

struct platform_device tegra12_i2c_device2 = {
	.name		= "tegra12-i2c",
	.id		= 1,
	.resource	= i2c_resource2,
	.num_resources	= ARRAY_SIZE(i2c_resource2),
	.dev = {
		.platform_data = 0,
	},
};

struct platform_device tegra12_i2c_device3 = {
	.name		= "tegra12-i2c",
	.id		= 2,
	.resource	= i2c_resource3,
	.num_resources	= ARRAY_SIZE(i2c_resource3),
	.dev = {
		.platform_data = 0,
	},
};

struct platform_device tegra12_i2c_device4 = {
	.name		= "tegra12-i2c",
	.id		= 3,
	.resource	= i2c_resource4,
	.num_resources	= ARRAY_SIZE(i2c_resource4),
	.dev = {
		.platform_data = 0,
	},
};

struct platform_device tegra12_i2c_device5 = {
	.name		= "tegra12-i2c",
	.id		= 4,
	.resource	= i2c_resource5,
	.num_resources	= ARRAY_SIZE(i2c_resource5),
	.dev = {
		.platform_data = 0,
	},
};

struct platform_device tegra12_i2c_device6 = {
	.name		= "tegra12-i2c",
	.id		= 5,
	.resource	= i2c_resource6,
	.num_resources	= ARRAY_SIZE(i2c_resource6),
	.dev = {
		.platform_data = 0,
	},
};
#endif

#if defined(CONFIG_ARCH_TEGRA_14x_SOC)

static struct tegra_mipi_bif_platform_data mipi_bif_pdata = {
	.adapter_nr	= -1,
	.tauBIF		= 2,
	.bus_clk_rate	= 13,
};

static struct resource mipi_bif_resource[] = {
	[0] = {
		.start	= INT_MIPI_BIF,
		.end	= INT_MIPI_BIF,
		.flags	= IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_MIPI_BIF_BASE,
		.end	= TEGRA_MIPI_BIF_BASE + TEGRA_MIPI_BIF_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device tegra_mipi_bif_device = {
	.name		= "tegra-mipi-bif",
	.id		= -1,
	.resource	= mipi_bif_resource,
	.num_resources	= ARRAY_SIZE(mipi_bif_resource),
	.dev = {
		.platform_data = &mipi_bif_pdata,
	},
};

#endif

static struct resource spi_resource1[] = {
	[0] = {
		.start	= INT_SPI_1,
		.end	= INT_SPI_1,
		.flags	= IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SPI1_BASE,
		.end	= TEGRA_SPI1_BASE + TEGRA_SPI1_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource spi_resource2[] = {
	[0] = {
		.start	= INT_SPI_2,
		.end	= INT_SPI_2,
		.flags	= IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SPI2_BASE,
		.end	= TEGRA_SPI2_BASE + TEGRA_SPI2_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource spi_resource3[] = {
	[0] = {
		.start	= INT_SPI_3,
		.end	= INT_SPI_3,
		.flags	= IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SPI3_BASE,
		.end	= TEGRA_SPI3_BASE + TEGRA_SPI3_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource spi_resource4[] = {
	[0] = {
		.start	= INT_SPI_4,
		.end	= INT_SPI_4,
		.flags	= IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SPI4_BASE,
		.end	= TEGRA_SPI4_BASE + TEGRA_SPI4_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
static struct resource spi_resource5[] = {
	[0] = {
		.start  = INT_SPI_5,
		.end    = INT_SPI_5,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SPI5_BASE,
		.end	= TEGRA_SPI5_BASE + TEGRA_SPI5_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource spi_resource6[] = {
	[0] = {
		.start  = INT_SPI_6,
		.end    = INT_SPI_6,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SPI6_BASE,
		.end	= TEGRA_SPI6_BASE + TEGRA_SPI6_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};
#endif

struct platform_device tegra_spi_device1 = {
	.name		= "spi_tegra",
	.id		= 0,
	.resource	= spi_resource1,
	.num_resources	= ARRAY_SIZE(spi_resource1),
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};

struct platform_device tegra_spi_device2 = {
	.name		= "spi_tegra",
	.id		= 1,
	.resource	= spi_resource2,
	.num_resources	= ARRAY_SIZE(spi_resource2),
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};

struct platform_device tegra_spi_device3 = {
	.name		= "spi_tegra",
	.id		= 2,
	.resource	= spi_resource3,
	.num_resources	= ARRAY_SIZE(spi_resource3),
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};

struct platform_device tegra_spi_device4 = {
	.name		= "spi_tegra",
	.id		= 3,
	.resource	= spi_resource4,
	.num_resources	= ARRAY_SIZE(spi_resource4),
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
struct platform_device tegra_spi_device5 = {
	.name           = "spi_tegra",
	.id             = 4,
	.resource       = spi_resource5,
	.num_resources  = ARRAY_SIZE(spi_resource5),
	.dev  = {
		.coherent_dma_mask      = 0xffffffff,
	},
};

struct platform_device tegra_spi_device6 = {
	.name           = "spi_tegra",
	.id             = 5,
	.resource       = spi_resource6,
	.num_resources  = ARRAY_SIZE(spi_resource6),
	.dev  = {
		.coherent_dma_mask      = 0xffffffff,
	},
};
#endif

struct platform_device tegra11_spi_device1 = {
	.name		= "spi-tegra114",
	.id		= 0,
	.resource	= spi_resource1,
	.num_resources	= ARRAY_SIZE(spi_resource1),
	.dev		= {
		.coherent_dma_mask	= DMA_BIT_MASK(64),
	},
};

struct platform_device tegra11_spi_device2 = {
	.name		= "spi-tegra114",
	.id		= 1,
	.resource	= spi_resource2,
	.num_resources	= ARRAY_SIZE(spi_resource2),
	.dev		= {
		.coherent_dma_mask	= DMA_BIT_MASK(64),
	},
};

struct platform_device tegra11_spi_device3 = {
	.name		= "spi-tegra114",
	.id		= 2,
	.resource	= spi_resource3,
	.num_resources	= ARRAY_SIZE(spi_resource3),
	.dev		= {
		.coherent_dma_mask	= DMA_BIT_MASK(64),
	},
};

struct platform_device tegra11_spi_device4 = {
	.name		= "spi-tegra114",
	.id		= 3,
	.resource	= spi_resource4,
	.num_resources	= ARRAY_SIZE(spi_resource4),
	.dev		= {
		.coherent_dma_mask	= DMA_BIT_MASK(64),
	},
};

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
struct platform_device tegra11_spi_device5 = {
	.name           = "spi-tegra114",
	.id             = 4,
	.resource       = spi_resource5,
	.num_resources  = ARRAY_SIZE(spi_resource5),
	.dev  = {
		.coherent_dma_mask      = DMA_BIT_MASK(64),
	},
};

struct platform_device tegra11_spi_device6 = {
	.name           = "spi-tegra114",
	.id             = 5,
	.resource       = spi_resource6,
	.num_resources  = ARRAY_SIZE(spi_resource6),
	.dev  = {
		.coherent_dma_mask      = DMA_BIT_MASK(64),
	},
};
#endif

struct platform_device tegra_spi_slave_device1 = {
	.name           = "spi_slave_tegra",
	.id             = 0,
	.resource       = spi_resource1,
	.num_resources  = ARRAY_SIZE(spi_resource1),
	.dev  = {
		.coherent_dma_mask      = 0xffffffff,
	},
};

struct platform_device tegra_spi_slave_device2 = {
	.name           = "spi_slave_tegra",
	.id             = 1,
	.resource       = spi_resource2,
	.num_resources  = ARRAY_SIZE(spi_resource2),
	.dev  = {
		.coherent_dma_mask      = 0xffffffff,
	},
};

struct platform_device tegra_spi_slave_device3 = {
	.name           = "spi_slave_tegra",
	.id             = 2,
	.resource       = spi_resource3,
	.num_resources  = ARRAY_SIZE(spi_resource3),
	.dev  = {
		.coherent_dma_mask      = 0xffffffff,
	},
};

struct platform_device tegra_spi_slave_device4 = {
	.name           = "spi_slave_tegra",
	.id             = 3,
	.resource       = spi_resource4,
	.num_resources  = ARRAY_SIZE(spi_resource4),
	.dev  = {
		.coherent_dma_mask      = 0xffffffff,
	},
};
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
struct platform_device tegra_spi_slave_device5 = {
	.name           = "spi_slave_tegra",
	.id             = 4,
	.resource       = spi_resource5,
	.num_resources  = ARRAY_SIZE(spi_resource5),
	.dev  = {
		.coherent_dma_mask      = 0xffffffff,
	},
};

struct platform_device tegra_spi_slave_device6 = {
	.name           = "spi_slave_tegra",
	.id             = 5,
	.resource       = spi_resource6,
	.num_resources  = ARRAY_SIZE(spi_resource6),
	.dev  = {
		.coherent_dma_mask      = 0xffffffff,
	},
};
#endif

struct platform_device tegra11_spi_slave_device1 = {
	.name           = "tegra11-spi-slave",
	.id             = 0,
	.resource       = spi_resource1,
	.num_resources  = ARRAY_SIZE(spi_resource1),
	.dev  = {
		.coherent_dma_mask      = 0xffffffff,
	},
};

struct platform_device tegra11_spi_slave_device2 = {
	.name           = "tegra11-spi-slave",
	.id             = 1,
	.resource       = spi_resource2,
	.num_resources  = ARRAY_SIZE(spi_resource2),
	.dev  = {
		.coherent_dma_mask      = 0xffffffff,
	},
};

struct platform_device tegra11_spi_slave_device3 = {
	.name           = "tegra11-spi-slave",
	.id             = 2,
	.resource       = spi_resource3,
	.num_resources  = ARRAY_SIZE(spi_resource3),
	.dev  = {
		.coherent_dma_mask      = 0xffffffff,
	},
};

struct platform_device tegra11_spi_slave_device4 = {
	.name           = "tegra11-spi-slave",
	.id             = 3,
	.resource       = spi_resource4,
	.num_resources  = ARRAY_SIZE(spi_resource4),
	.dev  = {
		.coherent_dma_mask      = 0xffffffff,
	},
};
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
struct platform_device tegra11_spi_slave_device5 = {
	.name           = "tegra11-spi-slave",
	.id             = 4,
	.resource       = spi_resource5,
	.num_resources  = ARRAY_SIZE(spi_resource5),
	.dev  = {
		.coherent_dma_mask      = 0xffffffff,
	},
};

struct platform_device tegra11_spi_slave_device6 = {
	.name           = "tegra11-spi-slave",
	.id             = 5,
	.resource       = spi_resource6,
	.num_resources  = ARRAY_SIZE(spi_resource6),
	.dev  = {
		.coherent_dma_mask      = 0xffffffff,
	},
};
#endif

static struct resource resources_nor[] = {
	[0] = {
		.start = INT_SNOR,
		.end = INT_SNOR,
		.flags = IORESOURCE_IRQ,
	},
	[1] = {
		/* Map SNOR Controller */
		.start = TEGRA_SNOR_BASE,
		.end = TEGRA_SNOR_BASE + TEGRA_SNOR_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[2] = {
		/* Map the size of flash */
		.start = TEGRA_NOR_FLASH_BASE,
		.end = TEGRA_NOR_FLASH_BASE + TEGRA_NOR_FLASH_SIZE - 1,
		.flags = IORESOURCE_MEM,
	}
};

struct platform_device tegra_nor_device = {
	.name = "tegra-nor",
	.id = -1,
	.num_resources = ARRAY_SIZE(resources_nor),
	.resource = resources_nor,
	.dev = {
		.coherent_dma_mask = 0xffffffff,
	},
};

static struct resource dtv_resource[] = {
	[0] = {
		.start  = TEGRA_DTV_BASE,
		.end    = TEGRA_DTV_BASE + TEGRA_DTV_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start	= TEGRA_DMA_REQ_SEL_DTV,
		.end	= TEGRA_DMA_REQ_SEL_DTV,
		.flags	= IORESOURCE_DMA
	},
};

struct platform_device tegra_dtv_device = {
	.name           = "tegra_dtv",
	.id             = -1,
	.resource       = dtv_resource,
	.num_resources  = ARRAY_SIZE(dtv_resource),
	.dev = {
		.init_name = "dtv",
		.coherent_dma_mask = 0xffffffff,
	},
};

static struct resource sdhci_resource1[] = {
	[0] = {
		.start	= INT_SDMMC1,
		.end	= INT_SDMMC1,
		.flags	= IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SDMMC1_BASE,
		.end	= TEGRA_SDMMC1_BASE + TEGRA_SDMMC1_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource sdhci_resource2[] = {
	[0] = {
		.start	= INT_SDMMC2,
		.end	= INT_SDMMC2,
		.flags	= IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SDMMC2_BASE,
		.end	= TEGRA_SDMMC2_BASE + TEGRA_SDMMC2_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource sdhci_resource3[] = {
	[0] = {
		.start	= INT_SDMMC3,
		.end	= INT_SDMMC3,
		.flags	= IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SDMMC3_BASE,
		.end	= TEGRA_SDMMC3_BASE + TEGRA_SDMMC3_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource sdhci_resource4[] = {
	[0] = {
		.start	= INT_SDMMC4,
		.end	= INT_SDMMC4,
		.flags	= IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SDMMC4_BASE,
		.end	= TEGRA_SDMMC4_BASE + TEGRA_SDMMC4_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device tegra_pci_device = {
	.name		= "tegra-pcie",
	.id		= 0,
	.resource	= 0,
	.num_resources	= 0,
	.dev = {
		.platform_data = 0,
	},
};

/* board files should fill in platform_data register the devices themselvs.
 * See board-harmony.c for an example
 */
struct platform_device tegra_sdhci_device1 = {
	.name		= "sdhci-tegra",
	.id		= 0,
	.resource	= sdhci_resource1,
	.num_resources	= ARRAY_SIZE(sdhci_resource1),
};

struct platform_device tegra_sdhci_device2 = {
	.name		= "sdhci-tegra",
	.id		= 1,
	.resource	= sdhci_resource2,
	.num_resources	= ARRAY_SIZE(sdhci_resource2),
};

struct platform_device tegra_sdhci_device3 = {
	.name		= "sdhci-tegra",
	.id		= 2,
	.resource	= sdhci_resource3,
	.num_resources	= ARRAY_SIZE(sdhci_resource3),
};

struct platform_device tegra_sdhci_device4 = {
	.name		= "sdhci-tegra",
	.id		= 3,
	.resource	= sdhci_resource4,
	.num_resources	= ARRAY_SIZE(sdhci_resource4),
};

static struct resource tegra_usb1_resources[] = {
	[0] = {
		.start	= TEGRA_USB_BASE,
		.end	= TEGRA_USB_BASE + TEGRA_USB_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_USB,
		.end	= INT_USB,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource tegra_usb2_resources[] = {
	[0] = {
		.start	= TEGRA_USB2_BASE,
		.end	= TEGRA_USB2_BASE + TEGRA_USB2_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_USB2,
		.end	= INT_USB2,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource tegra_usb3_resources[] = {
	[0] = {
		.start	= TEGRA_USB3_BASE,
		.end	= TEGRA_USB3_BASE + TEGRA_USB3_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_USB3,
		.end	= INT_USB3,
		.flags	= IORESOURCE_IRQ,
	},
};

#if defined(CONFIG_ARCH_TEGRA_11x_SOC) || defined(CONFIG_ARCH_TEGRA_12x_SOC)
static struct resource tegra_xusb_resources[] = {
	[0] = DEFINE_RES_MEM_NAMED(TEGRA_XUSB_HOST_BASE, TEGRA_XUSB_HOST_SIZE,
			"host"),
	[1] = DEFINE_RES_MEM_NAMED(TEGRA_XUSB_FPCI_BASE, TEGRA_XUSB_FPCI_SIZE,
			"fpci"),
	[2] = DEFINE_RES_MEM_NAMED(TEGRA_XUSB_IPFS_BASE, TEGRA_XUSB_IPFS_SIZE,
			"ipfs"),
	[3] = DEFINE_RES_MEM_NAMED(TEGRA_XUSB_PADCTL_BASE,
			TEGRA_XUSB_PADCTL_SIZE, "padctl"),
	[4] = DEFINE_RES_IRQ_NAMED(INT_XUSB_HOST_INT, "host"),
	[5] = DEFINE_RES_IRQ_NAMED(INT_XUSB_HOST_SMI, "host-smi"),
	[6] = DEFINE_RES_IRQ_NAMED(INT_XUSB_PADCTL, "padctl"),
	[7] = DEFINE_RES_IRQ_NAMED(INT_USB3, "usb3"),
	[8] = DEFINE_RES_IRQ_NAMED(INT_USB2, "usb2"),
};

static u64 tegra_xusb_dmamask = DMA_BIT_MASK(64);

struct platform_device tegra_xhci_device = {
	.name = "tegra-xhci",
	.id = -1,
	.dev = {
		.dma_mask = &tegra_xusb_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(64),
	},
	.resource = tegra_xusb_resources,
	.num_resources = ARRAY_SIZE(tegra_xusb_resources),
};
#endif

static u64 tegra_ehci_dmamask = DMA_BIT_MASK(64);

struct platform_device tegra_ehci1_device = {
	.name	= "tegra-ehci",
	.id	= 0,
	.dev	= {
		.dma_mask	= &tegra_ehci_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(64),
	},
	.resource = tegra_usb1_resources,
	.num_resources = ARRAY_SIZE(tegra_usb1_resources),
};

struct platform_device tegra_ehci2_device = {
	.name	= "tegra-ehci",
	.id	= 1,
	.dev	= {
		.dma_mask	= &tegra_ehci_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(64),
	},
	.resource = tegra_usb2_resources,
	.num_resources = ARRAY_SIZE(tegra_usb2_resources),
};

struct platform_device tegra_ehci3_device = {
	.name	= "tegra-ehci",
	.id	= 2,
	.dev	= {
		.dma_mask	= &tegra_ehci_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(64),
	},
	.resource = tegra_usb3_resources,
	.num_resources = ARRAY_SIZE(tegra_usb3_resources),
};

static struct resource tegra_pmu_resources[] = {
	[0] = {
		.start	= INT_CPU0_PMU_INTR,
		.end	= INT_CPU0_PMU_INTR,
		.flags	= IORESOURCE_IRQ,
	},
	[1] = {
		.start	= INT_CPU1_PMU_INTR,
		.end	= INT_CPU1_PMU_INTR,
		.flags	= IORESOURCE_IRQ,
	},
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	[2] = {
		.start	= INT_CPU2_PMU_INTR,
		.end	= INT_CPU2_PMU_INTR,
		.flags	= IORESOURCE_IRQ,
	},
	[3] = {
		.start	= INT_CPU3_PMU_INTR,
		.end	= INT_CPU3_PMU_INTR,
		.flags	= IORESOURCE_IRQ,
	},
#endif
};

struct platform_device tegra_pmu_device = {
	.name		= "arm-pmu",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(tegra_pmu_resources),
	.resource	= tegra_pmu_resources,
};

static struct resource tegra_uarta_resources[] = {
	[0] = {
		.start	= TEGRA_UARTA_BASE,
		.end	= TEGRA_UARTA_BASE + TEGRA_UARTA_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_UARTA,
		.end	= INT_UARTA,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource tegra_uartb_resources[] = {
	[0] = {
		.start	= TEGRA_UARTB_BASE,
		.end	= TEGRA_UARTB_BASE + TEGRA_UARTB_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_UARTB,
		.end	= INT_UARTB,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource tegra_uartc_resources[] = {
	[0] = {
		.start	= TEGRA_UARTC_BASE,
		.end	= TEGRA_UARTC_BASE + TEGRA_UARTC_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_UARTC,
		.end	= INT_UARTC,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource tegra_uartd_resources[] = {
	[0] = {
		.start	= TEGRA_UARTD_BASE,
		.end	= TEGRA_UARTD_BASE + TEGRA_UARTD_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_UARTD,
		.end	= INT_UARTD,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource tegra_uarte_resources[] = {
	[0] = {
		.start	= TEGRA_UARTE_BASE,
		.end	= TEGRA_UARTE_BASE + TEGRA_UARTE_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_UARTE,
		.end	= INT_UARTE,
		.flags	= IORESOURCE_IRQ,
	},
};

#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
#define UART_DMA_BIT_MASK DMA_BIT_MASK(64)
#else
#define UART_DMA_BIT_MASK DMA_BIT_MASK(32)
#endif

struct platform_device tegra_uarta_device = {
	.name	= "serial-tegra",
	.id	= 0,
	.num_resources	= ARRAY_SIZE(tegra_uarta_resources),
	.resource	= tegra_uarta_resources,
	.dev	= {
		.coherent_dma_mask	= UART_DMA_BIT_MASK,
	},
};

struct platform_device tegra_uartb_device = {
	.name	= "serial-tegra",
	.id	= 1,
	.num_resources	= ARRAY_SIZE(tegra_uartb_resources),
	.resource	= tegra_uartb_resources,
	.dev	= {
		.coherent_dma_mask	= UART_DMA_BIT_MASK,
	},
};

struct platform_device tegra_uartc_device = {
	.name	= "serial-tegra",
	.id	= 2,
	.num_resources	= ARRAY_SIZE(tegra_uartc_resources),
	.resource	= tegra_uartc_resources,
	.dev	= {
		.coherent_dma_mask	= UART_DMA_BIT_MASK,
	},
};

struct platform_device tegra_uartd_device = {
	.name	= "serial-tegra",
	.id	= 3,
	.num_resources	= ARRAY_SIZE(tegra_uartd_resources),
	.resource	= tegra_uartd_resources,
	.dev	= {
		.coherent_dma_mask	= UART_DMA_BIT_MASK,
	},
};

struct platform_device tegra_uarte_device = {
	.name	= "serial-tegra",
	.id	= 4,
	.num_resources	= ARRAY_SIZE(tegra_uarte_resources),
	.resource	= tegra_uarte_resources,
	.dev	= {
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};

static struct plat_serial8250_port debug_uarta_platform_data[] = {
	{
		.membase        = IO_ADDRESS(TEGRA_UARTA_BASE),
		.mapbase        = TEGRA_UARTA_BASE,
		.irq            = INT_UARTA,
		.flags          = UPF_BOOT_AUTOCONF | UPF_FIXED_TYPE,
		.type           = PORT_TEGRA,
		.iotype         = UPIO_MEM,
		.regshift       = 2,
	},
	{
		.flags          = 0,
	},
};

static struct plat_serial8250_port debug_uartb_platform_data[] = {
	{
		.membase        = IO_ADDRESS(TEGRA_UARTB_BASE),
		.mapbase        = TEGRA_UARTB_BASE,
		.irq            = INT_UARTB,
		.flags          = UPF_BOOT_AUTOCONF | UPF_FIXED_TYPE,
		.type           = PORT_TEGRA,
		.iotype         = UPIO_MEM,
		.regshift       = 2,
	},
	{
		.flags          = 0,
	},
};

static struct plat_serial8250_port debug_uartc_platform_data[] = {
	{
		.membase        = IO_ADDRESS(TEGRA_UARTC_BASE),
		.mapbase        = TEGRA_UARTC_BASE,
		.irq            = INT_UARTC,
		.flags          = UPF_BOOT_AUTOCONF | UPF_FIXED_TYPE,
		.type           = PORT_TEGRA,
		.iotype         = UPIO_MEM,
		.regshift       = 2,
	},
	{
		.flags          = 0,
	},
};

static struct plat_serial8250_port debug_uartd_platform_data[] = {
	{
		.membase        = IO_ADDRESS(TEGRA_UARTD_BASE),
		.mapbase        = TEGRA_UARTD_BASE,
		.irq            = INT_UARTD,
		.flags          = UPF_BOOT_AUTOCONF | UPF_FIXED_TYPE,
		.type           = PORT_TEGRA,
		.iotype         = UPIO_MEM,
		.regshift       = 2,
	},
	{
		.flags          = 0,
	},
};

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
static struct plat_serial8250_port debug_uarte_platform_data[] = {
	{
		.membase        = IO_ADDRESS(TEGRA_UARTE_BASE),
		.mapbase        = TEGRA_UARTE_BASE,
		.irq            = INT_UARTE,
		.flags          = UPF_BOOT_AUTOCONF | UPF_FIXED_TYPE,
		.type           = PORT_TEGRA,
		.iotype         = UPIO_MEM,
		.regshift       = 2,
	},
	{
		.flags          = 0,
	},
};
#endif

struct platform_device debug_uarta_device = {
	.name = "serial8250",
	.id = PLAT8250_DEV_PLATFORM,
	.dev = {
		.platform_data = debug_uarta_platform_data,
	},
};

struct platform_device debug_uartb_device = {
	.name = "serial8250",
	.id = PLAT8250_DEV_PLATFORM,
	.dev = {
		.platform_data = debug_uartb_platform_data,
	},
};

struct platform_device debug_uartc_device = {
	.name = "serial8250",
	.id = PLAT8250_DEV_PLATFORM,
	.dev = {
		.platform_data = debug_uartc_platform_data,
	},
};

struct platform_device debug_uartd_device = {
	.name = "serial8250",
	.id = PLAT8250_DEV_PLATFORM,
	.dev = {
		.platform_data = debug_uartd_platform_data,
	},
};

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
struct platform_device debug_uarte_device = {
	.name = "serial8250",
	.id = PLAT8250_DEV_PLATFORM,
	.dev = {
		.platform_data = debug_uarte_platform_data,
	},
};
#endif

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
static struct resource i2s_resource1[] = {
	[0] = {
		.start	= INT_I2S1,
		.end	= INT_I2S1,
		.flags	= IORESOURCE_IRQ
	},
	[1] = {
		.start	= TEGRA_DMA_REQ_SEL_I2S_1,
		.end	= TEGRA_DMA_REQ_SEL_I2S_1,
		.flags	= IORESOURCE_DMA
	},
	[2] = {
		.start	= TEGRA_I2S1_BASE,
		.end	= TEGRA_I2S1_BASE + TEGRA_I2S1_SIZE - 1,
		.flags	= IORESOURCE_MEM
	}
};

struct platform_device tegra_i2s_device1 = {
	.name		= "tegra20-i2s",
	.id		= 0,
	.resource	= i2s_resource1,
	.num_resources	= ARRAY_SIZE(i2s_resource1),
};

static struct resource i2s_resource2[] = {
	[0] = {
		.start	= INT_I2S2,
		.end	= INT_I2S2,
		.flags	= IORESOURCE_IRQ
	},
	[1] = {
		.start	= TEGRA_DMA_REQ_SEL_I2S2_1,
		.end	= TEGRA_DMA_REQ_SEL_I2S2_1,
		.flags	= IORESOURCE_DMA
	},
	[2] = {
		.start	= TEGRA_I2S2_BASE,
		.end	= TEGRA_I2S2_BASE + TEGRA_I2S2_SIZE - 1,
		.flags	= IORESOURCE_MEM
	}
};

struct platform_device tegra_i2s_device2 = {
	.name		= "tegra20-i2s",
	.id		= 1,
	.resource	= i2s_resource2,
	.num_resources	= ARRAY_SIZE(i2s_resource2),
};
#else
static struct resource i2s_resource0[] = {
	[0] = {
		.start	= TEGRA_I2S0_BASE,
		.end	= TEGRA_I2S0_BASE + TEGRA_I2S0_SIZE - 1,
		.flags	= IORESOURCE_MEM
	}
};

struct platform_device tegra_i2s_device0 = {
	.name		= "tegra30-i2s",
	.id		= 0,
	.resource	= i2s_resource0,
	.num_resources	= ARRAY_SIZE(i2s_resource0),
};

static struct resource i2s_resource1[] = {
	[0] = {
		.start	= TEGRA_I2S1_BASE,
		.end	= TEGRA_I2S1_BASE + TEGRA_I2S1_SIZE - 1,
		.flags	= IORESOURCE_MEM
	}
};

struct platform_device tegra_i2s_device1 = {
	.name		= "tegra30-i2s",
	.id		= 1,
	.resource	= i2s_resource1,
	.num_resources	= ARRAY_SIZE(i2s_resource1),
};

static struct resource i2s_resource2[] = {
	[0] = {
		.start	= TEGRA_I2S2_BASE,
		.end	= TEGRA_I2S2_BASE + TEGRA_I2S2_SIZE - 1,
		.flags	= IORESOURCE_MEM
	}
};

struct platform_device tegra_i2s_device2 = {
	.name		= "tegra30-i2s",
	.id		= 2,
	.resource	= i2s_resource2,
	.num_resources	= ARRAY_SIZE(i2s_resource2),
};

static struct resource i2s_resource3[] = {
	[0] = {
		.start	= TEGRA_I2S3_BASE,
		.end	= TEGRA_I2S3_BASE + TEGRA_I2S3_SIZE - 1,
		.flags	= IORESOURCE_MEM
	}
};

struct platform_device tegra_i2s_device3 = {
	.name		= "tegra30-i2s",
	.id		= 3,
	.resource	= i2s_resource3,
	.num_resources	= ARRAY_SIZE(i2s_resource3),
};

static struct resource i2s_resource4[] = {
	[0] = {
		.start	= TEGRA_I2S4_BASE,
		.end	= TEGRA_I2S4_BASE + TEGRA_I2S4_SIZE - 1,
		.flags	= IORESOURCE_MEM
	}
};

struct platform_device tegra_i2s_device4 = {
	.name		= "tegra30-i2s",
	.id		= 4,
	.resource	= i2s_resource4,
	.num_resources	= ARRAY_SIZE(i2s_resource4),
};
#endif

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
static struct resource spdif_resource[] = {
	[0] = {
		.start	= INT_SPDIF,
		.end	= INT_SPDIF,
		.flags	= IORESOURCE_IRQ
	},
	[1] = {
		.start	= TEGRA_DMA_REQ_SEL_SPD_I,
		.end	= TEGRA_DMA_REQ_SEL_SPD_I,
		.flags	= IORESOURCE_DMA
	},
	[2] = {
		.start	= TEGRA_SPDIF_BASE,
		.end	= TEGRA_SPDIF_BASE + TEGRA_SPDIF_SIZE - 1,
		.flags	= IORESOURCE_MEM
	}
};

struct platform_device tegra_spdif_device = {
	.name		= "tegra20-spdif",
	.id		= -1,
	.resource	= spdif_resource,
	.num_resources	= ARRAY_SIZE(spdif_resource),
};
#else
static struct resource spdif_resource[] = {
	[0] = {
		.start	= TEGRA_SPDIF_BASE,
		.end	= TEGRA_SPDIF_BASE + TEGRA_SPDIF_SIZE - 1,
		.flags	= IORESOURCE_MEM
	}
};

struct platform_device tegra_spdif_device = {
	.name		= "tegra30-spdif",
	.id		= -1,
	.resource	= spdif_resource,
	.num_resources	= ARRAY_SIZE(spdif_resource),
};
#endif

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
static struct resource ahub_resource[] = {
	[0] = {
		.start	= TEGRA_APBIF0_BASE,
		.end	= TEGRA_APBIF3_BASE + TEGRA_APBIF3_SIZE - 1,
		.flags	= IORESOURCE_MEM
	},
	[1] = {
		.start	= TEGRA_AHUB_BASE,
		.end	= TEGRA_AHUB_BASE + TEGRA_AHUB_SIZE - 1,
		.flags	= IORESOURCE_MEM
	}
};

struct platform_device tegra_ahub_device = {
	.name	= "tegra30-ahub",
	.id	= -1,
	.resource	= ahub_resource,
	.num_resources	= ARRAY_SIZE(ahub_resource),
};

static struct resource dam_resource0[] = {
	[0] = {
		.start = TEGRA_DAM0_BASE,
		.end   = TEGRA_DAM0_BASE + TEGRA_DAM0_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_dam_device0 = {
	.name = "tegra30-dam",
	.id = 0,
	.resource      = dam_resource0,
	.num_resources = ARRAY_SIZE(dam_resource0),
};

static struct resource dam_resource1[] = {
	[0] = {
		.start = TEGRA_DAM1_BASE,
		.end   = TEGRA_DAM1_BASE + TEGRA_DAM1_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_dam_device1 = {
	.name = "tegra30-dam",
	.id = 1,
	.resource      = dam_resource1,
	.num_resources = ARRAY_SIZE(dam_resource1),
};

static struct resource dam_resource2[] = {
	[0] = {
		.start = TEGRA_DAM2_BASE,
		.end   = TEGRA_DAM2_BASE + TEGRA_DAM2_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_dam_device2 = {
	.name = "tegra30-dam",
	.id = 2,
	.resource      = dam_resource2,
	.num_resources = ARRAY_SIZE(dam_resource2),
};

static u64 tegra_hda_dma_mask = DMA_BIT_MASK(32);
static struct resource hda_platform_resources[] = {
	[0] = {
		.start	= TEGRA_HDA_BASE,
		.end	= TEGRA_HDA_BASE + TEGRA_HDA_SIZE - 1,
		.flags	= IORESOURCE_MEM
	},
	[1] = {
		.start	= INT_HDA,
		.end	= INT_HDA,
		.flags	= IORESOURCE_IRQ
	},
};

struct platform_device tegra_hda_device = {
	.name		= "tegra30-hda",
	.id		= -1,
	.dev = {
		.coherent_dma_mask	= DMA_BIT_MASK(32),
		.dma_mask		= &tegra_hda_dma_mask,
	},
	.resource	= hda_platform_resources,
	.num_resources	= ARRAY_SIZE(hda_platform_resources),
};
#endif

#if defined(CONFIG_ARCH_TEGRA_14x_SOC)
static struct resource dmic_resource0[] = {
	[0] = {
		.start = TEGRA_DMIC0_BASE,
		.end   = TEGRA_DMIC0_BASE + TEGRA_DMIC0_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_dmic_device0 = {
	.name = "tegra-dmic",
	.id = 0,
	.resource      = dmic_resource0,
	.num_resources = ARRAY_SIZE(dmic_resource0),
};

static struct resource dmic_resource1[] = {
	[0] = {
		.start = TEGRA_DMIC1_BASE,
		.end   = TEGRA_DMIC1_BASE + TEGRA_DMIC1_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_dmic_device1 = {
	.name = "tegra-dmic",
	.id = 1,
	.resource      = dmic_resource1,
	.num_resources = ARRAY_SIZE(dmic_resource1),
};
#endif

struct platform_device spdif_dit_device = {
	.name = "spdif-dit",
	.id = 0,
};

struct platform_device bluetooth_dit_device = {
	.name = "spdif-dit",
	.id = 1,
};

struct platform_device baseband_dit_device = {
	.name = "spdif-dit",
	.id = 2,
};

struct platform_device tegra_pcm_device = {
	.name = "tegra-pcm-audio",
	.id = -1,
};

struct platform_device tegra_tdm_pcm_device = {
	.name = "tegra-tdm-pcm-audio",
	.id = -1,
};

static struct resource w1_resources[] = {
	[0] = {
		.start = INT_OWR,
		.end   = INT_OWR,
		.flags = IORESOURCE_IRQ
	},
	[1] = {
		.start = TEGRA_OWR_BASE,
		.end = TEGRA_OWR_BASE + TEGRA_OWR_SIZE - 1,
		.flags = IORESOURCE_MEM
	}
};

struct platform_device tegra_w1_device = {
	.name          = "tegra_w1",
	.id            = -1,
	.resource      = w1_resources,
	.num_resources = ARRAY_SIZE(w1_resources),
};

static struct resource tegra_udc_resources[] = {
	[0] = {
		.start	= TEGRA_USB_BASE,
		.end	= TEGRA_USB_BASE + TEGRA_USB_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_USB,
		.end	= INT_USB,
		.flags	= IORESOURCE_IRQ,
	},
};

static u64 tegra_udc_dmamask = DMA_BIT_MASK(64);

struct platform_device tegra_udc_device = {
	.name	= "tegra-udc",
	.id	= 0,
	.dev	= {
		.dma_mask	= &tegra_udc_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(64),
	},
	.resource = tegra_udc_resources,
	.num_resources = ARRAY_SIZE(tegra_udc_resources),
};

static struct resource tegra_otg_resources[] = {
	[0] = {
		.start	= TEGRA_USB_BASE,
		.end	= TEGRA_USB_BASE + TEGRA_USB_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= INT_USB,
		.end	= INT_USB,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device tegra_otg_device = {
	.name		= "tegra-otg",
	.id		= -1,
	.resource	= tegra_otg_resources,
	.num_resources	= ARRAY_SIZE(tegra_otg_resources),
};

#ifdef CONFIG_SATA_AHCI_TEGRA
static u64 tegra_sata_dma_mask = DMA_BIT_MASK(64);

static struct resource tegra_sata_resources[] = {
	[0] = {
		.start = TEGRA_SATA_BAR5_BASE,
		.end = TEGRA_SATA_BAR5_BASE + TEGRA_SATA_BAR5_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = TEGRA_SATA_CONFIG_BASE,
		.end = TEGRA_SATA_CONFIG_BASE + TEGRA_SATA_CONFIG_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[2] = {
		.start = INT_SATA_CTL,
		.end = INT_SATA_CTL,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device tegra_sata_device = {
	.name 	= "tegra-sata",
	.id 	= 0,
	.dev 	= {
		.coherent_dma_mask = DMA_BIT_MASK(64),
		.dma_mask = &tegra_sata_dma_mask,
	},
	.resource = tegra_sata_resources,
	.num_resources = ARRAY_SIZE(tegra_sata_resources),
};
#endif

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
static struct resource das_resource[] = {
	[0] = {
		.start	= TEGRA_APB_MISC_DAS_BASE,
		.end	= TEGRA_APB_MISC_DAS_BASE + TEGRA_APB_MISC_DAS_SIZE - 1,
		.flags	= IORESOURCE_MEM
	}
};

struct platform_device tegra_das_device = {
	.name		= "tegra20-das",
	.id		= -1,
	.resource	= das_resource,
	.num_resources	= ARRAY_SIZE(das_resource),
};
#endif

#if defined(CONFIG_TEGRA_IOVMM_GART) || defined(CONFIG_TEGRA_IOMMU_GART)
static struct resource tegra_gart_resources[] = {
	[0] = {
		.name	= "mc",
		.flags	= IORESOURCE_MEM,
		.start	= TEGRA_MC_BASE,
		.end	= TEGRA_MC_BASE + TEGRA_MC_SIZE - 1,
	},
	[1] = {
		.name	= "gart",
		.flags	= IORESOURCE_MEM,
		.start	= TEGRA_GART_BASE,
		.end	= TEGRA_GART_BASE + TEGRA_GART_SIZE - 1,
	}
};

struct platform_device tegra_gart_device = {
	.name		= "tegra_gart",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(tegra_gart_resources),
	.resource	= tegra_gart_resources
};
#endif

#if defined(CONFIG_TEGRA_IOVMM_SMMU) || defined(CONFIG_TEGRA_IOMMU_SMMU)
static struct resource tegra_smmu_resources[] = {
	{
		.name	= "mc",
		.flags	= IORESOURCE_MEM,
		.start	= TEGRA_MC_BASE,
		.end	= TEGRA_MC_BASE + TEGRA_MC_SIZE - 1,
	},
	{
		.name   = "ahbarb",
		.flags  = IORESOURCE_MEM,
		.start  = TEGRA_AHB_ARB_BASE,
		.end    = TEGRA_AHB_ARB_BASE + TEGRA_AHB_ARB_SIZE - 1,
	},
};

struct platform_device tegra_smmu_device = {
	.name		= "tegra_smmu",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(tegra_smmu_resources),
	.resource	= tegra_smmu_resources
};

static struct resource tegra_smmu[] = {
	[0] = {
		.start	= TEGRA_SMMU_BASE,
		.end	= TEGRA_SMMU_BASE + TEGRA_SMMU_SIZE - 1,
	},
};

struct resource *tegra_smmu_window(int wnum)
{
	return &tegra_smmu[wnum];
}

int tegra_smmu_window_count(void)
{
	return ARRAY_SIZE(tegra_smmu);
}

static struct iommu_linear_map tegra_fb_linear_map[16]; /* Terminated with 0 */

#define LINEAR_MAP_ADD(n) \
do { \
	if (n##_start && n##_size) { \
		map[i].start = n##_start; \
		map[i++].size = n##_size; \
	} \
} while (0)

void tegra_fb_linear_set(struct iommu_linear_map *map)
{
	int i = 0;

	map = tegra_fb_linear_map;

	LINEAR_MAP_ADD(tegra_fb);
	LINEAR_MAP_ADD(tegra_fb2);
	LINEAR_MAP_ADD(tegra_bootloader_fb);
	LINEAR_MAP_ADD(tegra_bootloader_fb2);
#ifndef CONFIG_NVMAP_USE_CMA_FOR_CARVEOUT
	LINEAR_MAP_ADD(tegra_vpr);
	LINEAR_MAP_ADD(tegra_carveout);
#endif
}

#ifdef CONFIG_NVMAP_USE_CMA_FOR_CARVEOUT
void carveout_linear_set(struct device *cma_dev)
{
	struct dma_contiguous_stats stats;
	struct iommu_linear_map *map = &tegra_fb_linear_map[0];

	if (dma_get_contiguous_stats(cma_dev, &stats))
		return;

	/* get the free slot at end and add carveout entry */
	while (map && map->size)
		map++;
	map->start = stats.base;
	map->size = stats.size;
}
#endif

struct swgid_fixup {
	const char * const name;
	u64 swgids;
	struct iommu_linear_map *linear_map;
};

#ifdef CONFIG_PLATFORM_ENABLE_IOMMU
#define DUMMY_DEV_NAME "dummy_dev"
#define DUMMY_DEV_MAX_NAME_SIZE 100
static char dummy_name[DUMMY_DEV_MAX_NAME_SIZE] = DUMMY_DEV_NAME;
#endif

/*
 * FIXME: They should have a DT entry with swgroup IDs.
 */
struct swgid_fixup tegra_swgid_fixup[] = {
	{ .name = "540c0000.epp",	.swgids = SWGID(EPP), },
	{ .name = "epp",	.swgids = SWGID(EPP), },
	{ .name = "54200000.dc",	.swgids = SWGID(DC),
	  .linear_map = tegra_fb_linear_map, },
	{ .name = "54240000.dc",	.swgids = SWGID(DCB), },
	{ .name = "dc",	.swgids = SWGID(DC) | SWGID(DCB) },
	{ .name = "gr2d",	.swgids = SWGID(G2), },
	{ .name = "gr3d",	.swgids = SWGID(NV) | SWGID(NV2), },
	{ .name = "host1x",	.swgids = SWGID(HC) | SWGID(VDE) |	\
	  SWGID(EPP) | SWGID(HDA), },
	{ .name = "isp",	.swgids = SWGID(ISP), },
	{ .name = "max77660",	.swgids = SWGID(PPCS), },
	{ .name = "max8831",	.swgids = SWGID(PPCS), },
	{ .name = "msenc",	.swgids = SWGID(MSENC), },
	{ .name = "mpe",	.swgids = SWGID(MPE), },
	{ .name = "tegra-aes",	.swgids = SWGID(VDE), },
	{ .name = "nvavp",	.swgids = SWGID(AVPC), },
	{ .name = "sdhci-tegra",	.swgids = SWGID(PPCS), },
	{ .name = "serial8250",	.swgids = SWGID(PPCS), },
	{ .name = "serial-tegra",      .swgids = SWGID(PPCS), },
	{ .name = "snd-soc-dummy",	.swgids = SWGID(PPCS), },
	{ .name = "spdif-dit",	.swgids = SWGID(PPCS), },
	{ .name = "tegra11-se",	.swgids = SWGID(PPCS), },
	{ .name = "tegra11-spi",	.swgids = SWGID(PPCS), },
	{ .name = "tegra14-i2c",	.swgids = SWGID(PPCS), },
	{ .name = "tegra30-ahub",	.swgids = SWGID(PPCS), },
	{ .name = "tegra30-dam",	.swgids = SWGID(PPCS), },
	{ .name = "tegra30-hda",	.swgids = SWGID(HDA), },
	{ .name = "tegra30-i2s",	.swgids = SWGID(PPCS), },
	{ .name = "tegra30-spdif",	.swgids = SWGID(PPCS), },
	{ .name = "tegradc.0", .swgids = SWGID(DC),
	  .linear_map = tegra_fb_linear_map},
	{ .name = "tegradc.1", .swgids = SWGID(DCB), },
	{ .name = "tegra_bb",	.swgids = SWGID(PPCS), },
	{ .name = "tegra_dma",	.swgids = SWGID(PPCS), },
	{ .name = "tegra-ehci",	.swgids = SWGID(PPCS), },
	{ .name = "tegra-fuse",	.swgids = SWGID(PPCS), },
	{ .name = "tegra-i2c",	.swgids = SWGID(PPCS), },
	{ .name = "tegra-nvmap",	.swgids = SWGID(HC) | SWGID(AVPC), },
	{ .name = "tegra-otg",	.swgids = SWGID(PPCS), },
	{ .name = "tegra-pcm-audio",	.swgids = SWGID(PPCS), },
	{ .name = "tegra-rtc",	.swgids = SWGID(PPCS), },
	{ .name = "tegra-sata",	.swgids = SWGID(SATA), },
	{ .name = "tegra-se",	.swgids = SWGID(PPCS), },
	{ .name = "tegra-snd",	.swgids = SWGID(PPCS), },
	{ .name = "tegra-tzram",	.swgids = SWGID(VDE), },
	{ .name = "tegra_uart",	.swgids = SWGID(PPCS), },
	{ .name = "tegra-udc",	.swgids = SWGID(PPCS), },
	{ .name = "tegra_usb_modem_power",	.swgids = SWGID(PPCS), },
	{ .name = "tsec",	.swgids = SWGID(TSEC), },
	{ .name = "vi",	.swgids = SWGID(VI), },
	{ .name = "therm_est",	.swgids = SWGID(PPCS), },
	{ .name = "tegra-xhci",	.swgids = SWGID(XUSB_HOST), },
#ifdef CONFIG_PLATFORM_ENABLE_IOMMU
	{ .name = dummy_name,	.swgids = SWGID(PPCS) },
#endif
	{},
};

struct swgid_fixup tegra_swgid_fixup_t124[] = {
	{ .name = "54200000.dc",	.swgids = SWGID(DC), },
	{ .name = "54240000.dc",	.swgids = SWGID(DCB), },
	{ .name = "dc",	.swgids = SWGID(DC) | SWGID(DCB) },
	{ .name = "host1x",	.swgids = SWGID(HC) | SWGID(VDE) |
	  SWGID(EPP) | SWGID(HDA), },
	{ .name = "isp",	.swgids = SWGID(ISP2) | SWGID(ISP2B), },
	{ .name = "max77660",	.swgids = SWGID(PPCS), },
	{ .name = "max8831",	.swgids = SWGID(PPCS), },
	{ .name = "msenc",	.swgids = SWGID(MSENC), },
	{ .name = "mpe",	.swgids = SWGID(MPE), },
	{ .name = "tegra-aes",	.swgids = SWGID(VDE), },
	{ .name = "nvavp",	.swgids = SWGID(AVPC) | SWGID(A9AVP), },
	{ .name = "serial8250",	.swgids = SWGID(PPCS), },
	{ .name = "serial-tegra",	.swgids = SWGID(PPCS), },
	{ .name = "dtv",	.swgids = SWGID(PPCS), },
	{ .name = "snd-soc-dummy",	.swgids = SWGID(PPCS), },
	{ .name = "spdif-dit",	.swgids = SWGID(PPCS), },
	{ .name = "tegra12-se",	.swgids = SWGID(PPCS), },
	{ .name = "spi-tegra114",	.swgids = SWGID(PPCS), },
	{ .name = "tegra14-i2c",	.swgids = SWGID(PPCS), },
	{ .name = "tegra30-ahub",	.swgids = SWGID(PPCS), },
	{ .name = "tegra30-dam",	.swgids = SWGID(PPCS), },
	{ .name = "tegra30-hda",	.swgids = SWGID(HDA), },
	{ .name = "tegra30-i2s",	.swgids = SWGID(PPCS), },
	{ .name = "tegra30-spdif",	.swgids = SWGID(PPCS), },
	{ .name = "tegradc.0", .swgids = SWGID(DC) | SWGID(DC12),
	  .linear_map = tegra_fb_linear_map, },
	{ .name = "tegradc.1", .swgids = SWGID(DCB),
	  .linear_map = tegra_fb_linear_map, },
	{ .name = "tegra_bb",	.swgids = SWGID(PPCS), },
	{ .name = "tegra_dma",	.swgids = SWGID(PPCS), },
	{ .name = "tegra-ehci",	.swgids = SWGID(PPCS), },
	{ .name = "tegra-fuse",	.swgids = SWGID(PPCS), },
	{ .name = "tegra-i2c",	.swgids = SWGID(PPCS), },
	{ .name = "tegra-nvmap",	.swgids = SWGID(HC) | SWGID(AVPC), },
	/*
	 * PPCS1 selection for USB2 needs AHB_ARBC register program
	 * in warm boot and cold boot paths in BL as it needs
	 * secure write.
	 */
	{ .name = "tegra-otg",	.swgids = SWGID(PPCS1), },
	{ .name = "tegra-pcm-audio",	.swgids = SWGID(PPCS), },
	{ .name = "tegra-rtc",	.swgids = SWGID(PPCS), },
	{ .name = "tegra-sata",	.swgids = SWGID(SATA2), },
	{ .name = "tegra-se",	.swgids = SWGID(PPCS), },
	{ .name = "tegra-snd",	.swgids = SWGID(PPCS), },
	{ .name = "tegra-tzram",	.swgids = SWGID(PPCS), },
	{ .name = "tegra_uart",	.swgids = SWGID(PPCS), },
	{ .name = "tegra-udc",	.swgids = SWGID(PPCS), },
	{ .name = "tegra_usb_modem_power",	.swgids = SWGID(PPCS), },
	{ .name = "tsec",	.swgids = SWGID(TSEC), },
	/* vic must be before vi to prevent incorrect matching */
	{ .name = "vic",	.swgids = SWGID(VIC), },
	{ .name = "vi",	.swgids = SWGID(VI), },
	{ .name = "therm_est",	.swgids = SWGID(PPCS), },
	{ .name = "gk20a",	.swgids = SWGID(GPU) | SWGID(GPUB), },
	{ .name = "tegra124-apbdma",	.swgids = SWGID(PPCS), },
	{ .name = "tegra-nor",	.swgids = SWGID(PPCS), },
#ifdef CONFIG_PLATFORM_ENABLE_IOMMU
	{ .name = dummy_name,	.swgids = SWGID(PPCS) },
#endif
	{ .name = "tegra-xhci",	.swgids = SWGID(XUSB_HOST), },
	{},
};

u64 tegra_smmu_fixup_swgids(struct device *dev, struct iommu_linear_map **map)
{
	const char *s;
	struct swgid_fixup *table = tegra_swgid_fixup;

	if (!dev)
		return 0;

	if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA12)
		table = tegra_swgid_fixup_t124;

	while ((s = table->name) != NULL) {
		if (!strncmp(s, dev_name(dev), strlen(s))) {
			if (map)
				*map = table->linear_map;

			return table->swgids;
		}
		table++;
	}

	return 0;
}

#ifdef CONFIG_PLATFORM_ENABLE_IOMMU

struct tegra_iommu_mapping {
	dma_addr_t base;
	size_t size;
	struct dma_iommu_mapping *map;
};

static struct tegra_iommu_mapping smmu_default_map[] = {
	[SYSTEM_DEFAULT] = {TEGRA_IOMMU_BASE, TEGRA_IOMMU_SIZE},
	[SYSTEM_PROTECTED] = {TEGRA_IOMMU_BASE, TEGRA_IOMMU_SIZE},
	[PPCS1_ASID] = {TEGRA_IOMMU_BASE, TEGRA_IOMMU_SIZE},
	[SYSTEM_DC] = {0x10000, (u32)~0},
	[SYSTEM_DCB] = {0x10000, (u32)~0},
	/* Non-zero base to account for gk20a driver's assumptions */
	[SYSTEM_GK20A] = {0x100000, (u32)~0},
	[SDMMC1A_ASID] = {TEGRA_IOMMU_BASE, TEGRA_IOMMU_SIZE},
	[SDMMC2A_ASID] = {TEGRA_IOMMU_BASE, TEGRA_IOMMU_SIZE},
	[SDMMC3A_ASID] = {TEGRA_IOMMU_BASE, TEGRA_IOMMU_SIZE},
	[SDMMC4A_ASID] = {TEGRA_IOMMU_BASE, TEGRA_IOMMU_SIZE},
};

static void tegra_smmu_map_init(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(smmu_default_map); i++) {
		struct tegra_iommu_mapping *m = &smmu_default_map[i];
		struct dma_iommu_mapping *map;
		int order = 0;

		if (IS_ENABLED(CONFIG_TEGRA_ERRATA_1053704))
			order = get_order(SZ_16K);

		map = arm_iommu_create_mapping(&platform_bus_type,
					       m->base, m->size, 0);

		if (IS_ERR(map))
			dev_err(&pdev->dev,
				"Failed create IOVA map for ASID[%d]\n", i);

		m->map = map;
	}
}

void tegra_smmu_map_misc_device(struct device *dev)
{
	struct dma_iommu_mapping *map = smmu_default_map[SYSTEM_PROTECTED].map;
	if (!strncmp(dummy_name, DUMMY_DEV_NAME, strlen(dummy_name))) {
		strncpy(dummy_name, dev_name(dev),
			DUMMY_DEV_MAX_NAME_SIZE);
		arm_iommu_attach_device(dev, map);
		dev_info(dev, "Mapped the misc device\n");
		return;
	}
	dev_err(dev, "Can't Map device\n");
}
EXPORT_SYMBOL(tegra_smmu_map_misc_device);

void tegra_smmu_unmap_misc_device(struct device *dev)
{
	if (!strncmp(dummy_name, dev_name(dev), strlen(dummy_name))) {
		arm_iommu_detach_device(dev);
		strncpy(dummy_name, DUMMY_DEV_NAME,
			DUMMY_DEV_MAX_NAME_SIZE);
		dev_info(dev, "Un-mapped the misc device\n");
		return;
	}
	dev_err(dev, "Can't Unmap device\n");
}
EXPORT_SYMBOL(tegra_smmu_unmap_misc_device);

static int _tegra_smmu_get_asid(u64 swgids)
{
	if (swgids & SWGID(PPCS))
		return SYSTEM_PROTECTED;
#if defined(CONFIG_ARCH_TEGRA_12x_SOC) || \
	defined(CONFIG_ARCH_TEGRA_11x_SOC)
	if (swgids & SWGID(PPCS1))
		return PPCS1_ASID;
#else
	if (swgids & SWGID(PPCS1))
		return SYSTEM_PROTECTED;
#endif

	if (swgids & SWGID(GPUB))
		return SYSTEM_GK20A;

#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
	if (swgids & SWGID(DC) ||
	    swgids & SWGID(DCB))
		return SYSTEM_DC;
#elif defined(CONFIG_ARCH_TEGRA_12x_SOC)
	if (swgids & SWGID(DC) ||
	    swgids & SWGID(DC12))
		return SYSTEM_DC;
	if (swgids & SWGID(DCB))
		return SYSTEM_DCB;
	if (swgids & SWGID(SDMMC1A))
		return SDMMC1A_ASID;
	if (swgids & SWGID(SDMMC2A))
		return SDMMC2A_ASID;
	if (swgids & SWGID(SDMMC3A))
		return SDMMC3A_ASID;
	if (swgids & SWGID(SDMMC4A))
		return SDMMC4A_ASID;
#endif

	return SYSTEM_DEFAULT;
}

int tegra_smmu_get_asid(struct device *dev)
{
	u64 swgids = tegra_smmu_fixup_swgids(dev, NULL);
	return _tegra_smmu_get_asid(swgids);
}

struct dma_iommu_mapping *tegra_smmu_get_map(struct device *dev, u64 swgids)
{
	if (!swgids)
		swgids = tegra_smmu_fixup_swgids(dev, NULL);

	if (!swgids)
		return NULL;

	return smmu_default_map[_tegra_smmu_get_asid(swgids)].map;
}
#else
static inline void tegra_smmu_map_init(struct platform_device *pdev)
{
}
#endif

static int __init tegra_smmu_init(void)
{
	platform_device_register(&tegra_smmu_device);
	tegra_smmu_map_init(&tegra_smmu_device);
	return 0;
}
postcore_initcall(tegra_smmu_init);
#endif

#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
#define CLK_RESET_RST_SOURCE	0x0
static struct resource tegra_wdt_resources[] = {
	[0] = {
		.start	= TEGRA_CLK_RESET_BASE + CLK_RESET_RST_SOURCE,
		.end	= TEGRA_CLK_RESET_BASE + CLK_RESET_RST_SOURCE + 4 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= TEGRA_TMR1_BASE,
		.end	= TEGRA_TMR1_BASE + TEGRA_TMR1_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {
		.start	= TEGRA_PMC_BASE,
		.end	= TEGRA_PMC_BASE + TEGRA_PMC_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[3] = {
		.start	= INT_TMR1,
		.end	= INT_TMR1,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device tegra_wdt_device = {
	.name		= "tegra_wdt",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(tegra_wdt_resources),
	.resource	= tegra_wdt_resources,
};
#else
static struct resource tegra_wdt0_resources[] = {
	[0] = {
		.start	= TEGRA_WDT0_BASE,
		.end	= TEGRA_WDT0_BASE + TEGRA_WDT0_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= TEGRA_TMR7_BASE,
		.end	= TEGRA_TMR7_BASE + TEGRA_TMR7_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {
		.start	= TEGRA_PMC_BASE,
		.end	= TEGRA_PMC_BASE + TEGRA_PMC_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[3] = {
		.start	= INT_WDT_CPU,
		.end	= INT_WDT_CPU,
		.flags	= IORESOURCE_IRQ,
	},
#ifdef CONFIG_TEGRA_FIQ_DEBUGGER
	[4] = {
		.start	= TEGRA_QUATERNARY_ICTLR_BASE,
		.end	= TEGRA_QUATERNARY_ICTLR_BASE + \
				TEGRA_QUATERNARY_ICTLR_SIZE -1,
		.flags	= IORESOURCE_MEM,
	},
#endif
#if defined(CONFIG_TEGRA_USE_SECURE_KERNEL) && \
	defined(CONFIG_ARCH_TEGRA_12x_SOC) && defined(CONFIG_FIQ_DEBUGGER)
	[5] = {
		.start  = TEGRA_WDT4_BASE,
		.end    = TEGRA_WDT4_BASE + TEGRA_WDT4_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	[6] = {
		.start  = INT_WDT_AVP,
		.end    = INT_WDT_AVP,
		.flags  = IORESOURCE_IRQ,
	},
#endif
};

struct platform_device tegra_wdt0_device = {
	.name		= "tegra_wdt",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(tegra_wdt0_resources),
	.resource	= tegra_wdt0_resources,
};

#endif

static struct resource tegra_pwfm_resource = {
	.start	= TEGRA_PWFM_BASE,
	.end	= TEGRA_PWFM_BASE + TEGRA_PWFM_SIZE - 1,
	.flags	= IORESOURCE_MEM,
};

struct platform_device tegra_pwfm_device = {
	.name		= "tegra-pwm",
	.id		= -1,
	.num_resources	= 1,
	.resource	= &tegra_pwfm_resource,
};

static struct tegra_avp_platform_data tegra_avp_pdata = {
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	.emc_clk_rate = ULONG_MAX,
#else
	.emc_clk_rate = 200000000,
#endif
};

struct resource tegra_nvavp_resources[] = {
	[0] = {
		.start  = INT_SHR_SEM_INBOX_IBF,
		.end    = INT_SHR_SEM_INBOX_IBF,
		.flags  = IORESOURCE_IRQ,
		.name   = "mbox_from_nvavp_pending",
	},
};

struct platform_device nvavp_device = {
	.name           = "nvavp",
	.id             = -1,
	.resource       = tegra_nvavp_resources,
	.num_resources  = ARRAY_SIZE(tegra_nvavp_resources),
	.dev  = {
		.coherent_dma_mask	= 0xffffffffULL,
		.platform_data		= &tegra_avp_pdata,
	},
};

static struct resource tegra_avp_resources[] = {
	[0] = {
		.start	= INT_SHR_SEM_INBOX_IBF,
		.end	= INT_SHR_SEM_INBOX_IBF,
		.flags	= IORESOURCE_IRQ,
		.name	= "mbox_from_avp_pending",
	},
};

struct platform_device tegra_avp_device = {
	.name		= "tegra-avp",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(tegra_avp_resources),
	.resource	= tegra_avp_resources,
	.dev  = {
		.coherent_dma_mask	= 0xffffffffULL,
		.platform_data		= &tegra_avp_pdata,
	},
};

static struct resource tegra_aes_resources[] = {
	{
		.start	= TEGRA_VDE_BASE,
		.end	= TEGRA_VDE_BASE + TEGRA_VDE_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= TEGRA_BSEA_BASE,
		.end	= TEGRA_BSEA_BASE + TEGRA_BSEA_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_VDE_BSE_V,
		.end	= INT_VDE_BSE_V,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= INT_VDE_BSE_A,
		.end	= INT_VDE_BSE_A,
		.flags	= IORESOURCE_IRQ,
	},
};

static u64 tegra_aes_dma_mask = DMA_BIT_MASK(32);

struct platform_device tegra_aes_device = {
	.name		= "tegra-aes",
	.id		= -1,
	.resource	= tegra_aes_resources,
	.num_resources	= ARRAY_SIZE(tegra_aes_resources),
	.dev	= {
		.dma_mask = &tegra_aes_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
};

static struct resource tegra_kbc_resources[] = {
	[0] = {
		.start = TEGRA_KBC_BASE,
		.end   = TEGRA_KBC_BASE + TEGRA_KBC_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_KBC,
		.end   = INT_KBC,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device tegra_kbc_device = {
	.name = "tegra-kbc",
	.id = -1,
	.resource = tegra_kbc_resources,
	.num_resources = ARRAY_SIZE(tegra_kbc_resources),
	.dev = {
		.platform_data = 0,
	},
};

#if defined(CONFIG_TEGRA_SKIN_THROTTLE)
struct platform_device tegra_skin_therm_est_device = {
	.name	= "therm_est",
	.id	= -1,
	.num_resources	= 0,
	.dev = {
		.platform_data = 0,
	},
};
#endif

#if defined(CONFIG_ARCH_TEGRA_3x_SOC)
static struct resource tegra_tsensor_resources[]= {
	{
		.start	= TEGRA_TSENSOR_BASE,
		.end	= TEGRA_TSENSOR_BASE + TEGRA_TSENSOR_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_TSENSOR,
		.end	= INT_TSENSOR,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start 	= TEGRA_PMC_BASE + 0x1B0,
		/* 2 pmc registers mapped */
		.end	= TEGRA_PMC_BASE + 0x1B0 + (2 * 4),
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device tegra_tsensor_device = {
	.name	= "tegra-tsensor",
	.id	= -1,
	.num_resources	= ARRAY_SIZE(tegra_tsensor_resources),
	.resource	= tegra_tsensor_resources,
	.dev = {
		.platform_data = 0,
	},
};
#endif

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
static u64 tegra_se_dma_mask = DMA_BIT_MASK(32);
static u64 tegra12_se_dma_mask = DMA_BIT_MASK(64);

struct resource tegra_se_resources[] = {
	[0] = {
		.start = TEGRA_SE_BASE,
		.end = TEGRA_SE_BASE + TEGRA_SE_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start	= TEGRA_PMC_BASE,
		.end	= TEGRA_PMC_BASE + SZ_256 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {
		.start = INT_SE,
		.end = INT_SE,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device tegra_se_device = {
	.name = "tegra-se",
	.id = -1,
	.dev = {
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.dma_mask = &tegra_se_dma_mask,
	},
	.resource = tegra_se_resources,
	.num_resources = ARRAY_SIZE(tegra_se_resources),
};

struct platform_device tegra11_se_device = {
	.name = "tegra11-se",
	.id = -1,
	.dev = {
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.dma_mask = &tegra_se_dma_mask,
	},
	.resource = tegra_se_resources,
	.num_resources = ARRAY_SIZE(tegra_se_resources),
};

struct platform_device tegra12_se_device = {
	.name = "tegra12-se",
	.id = -1,
	.dev = {
		.coherent_dma_mask = DMA_BIT_MASK(64),
		.dma_mask = &tegra12_se_dma_mask,
	},
	.resource = tegra_se_resources,
	.num_resources = ARRAY_SIZE(tegra_se_resources),
};
#endif

static struct resource tegra_disp1_resources[] = {
	{
		.name	= "irq",
		.start	= INT_DISPLAY_GENERAL,
		.end	= INT_DISPLAY_GENERAL,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
		.start	= TEGRA_DISPLAY_BASE,
		.end	= TEGRA_DISPLAY_BASE + TEGRA_DISPLAY_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
		.start	= 0,
		.end	= 0,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "dsi_regs",
		.start	= TEGRA_DSI_BASE,
		.end	= TEGRA_DSI_BASE + TEGRA_DSI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device tegra_disp1_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= tegra_disp1_resources,
	.num_resources	= ARRAY_SIZE(tegra_disp1_resources),
};

static struct resource tegra_disp2_resources[] = {
	{
		.name	= "irq",
		.start	= INT_DISPLAY_B_GENERAL,
		.end	= INT_DISPLAY_B_GENERAL,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
		.start	= TEGRA_DISPLAY2_BASE,
		.end	= TEGRA_DISPLAY2_BASE + TEGRA_DISPLAY2_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
		.flags	= IORESOURCE_MEM,
		.start	= 0,
		.end	= 0,
	},
	{
		.name	= "hdmi_regs",
		.start	= TEGRA_HDMI_BASE,
		.end	= TEGRA_HDMI_BASE + TEGRA_HDMI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device tegra_disp2_device = {
	.name		= "tegradc",
	.id		= 1,
	.resource	= tegra_disp2_resources,
	.num_resources	= ARRAY_SIZE(tegra_disp2_resources),
	.dev = {
		.platform_data = 0,
	},
};

struct platform_device tegra_nvmap_device = {
	.name	= "tegra-nvmap",
	.id	= -1,
};

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
static struct resource tegra_cec_resources[] = {
	[0] = {
		.start = TEGRA_CEC_BASE,
		.end = TEGRA_CEC_BASE + TEGRA_CEC_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_CEC,
		.end = INT_CEC,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device tegra_cec_device = {
	.name = "tegra_cec",
	.id   = -1,
	.resource = tegra_cec_resources,
	.num_resources = ARRAY_SIZE(tegra_cec_resources),
};
#endif

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
static struct resource cl_dvfs_resource[] = {
	[0] = {
		.start	= TEGRA_CL_DVFS_BASE,
		.end	= TEGRA_CL_DVFS_BASE + TEGRA_CL_DVFS_SIZE-1,
		.flags	= IORESOURCE_MEM,
	}
};

struct platform_device tegra_cl_dvfs_device = {
	.name		= "tegra_cl_dvfs",
	.id		= -1,
	.resource	= cl_dvfs_resource,
	.num_resources	= ARRAY_SIZE(cl_dvfs_resource),
};
#endif

struct platform_device tegra_fuse_device = {
	.name	= "tegra-fuse",
	.id	= -1,
};

#if defined(CONFIG_TEGRA_WAKEUP_MONITOR)
static struct tegra_wakeup_monitor_platform_data
			tegratab_tegra_wakeup_monitor_pdata = {
	.wifi_wakeup_source     = 6,
	.rtc_wakeup_source      = 18,
};

struct platform_device tegratab_tegra_wakeup_monitor_device = {
	.name = "tegra_wakeup_monitor",
	.id   = -1,
	.dev  = {
		.platform_data = &tegratab_tegra_wakeup_monitor_pdata,
	},
};
#endif

static struct resource ptm_resources[] = {
	{
		.name  = "ptm",
		.start = PTM0_BASE,
		.end   = PTM0_BASE + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
	{
		.name  = "ptm",
		.start = PTM1_BASE,
		.end   = PTM1_BASE + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
	{
		.name  = "ptm",
		.start = PTM2_BASE,
		.end   = PTM2_BASE + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
	{
		.name  = "ptm",
		.start = PTM3_BASE,
		.end   = PTM3_BASE + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
	{
		.name  = "etb",
		.start = ETB_BASE,
		.end   = ETB_BASE + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
	{
		.name  = "funnel",
		.start = FUNNEL_BASE,
		.end   = FUNNEL_BASE + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
	{
		.name  = "tpiu",
		.start = TPIU_BASE,
		.end   = TPIU_BASE + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
};

struct platform_device tegra_ptm_device = {
	.name          = "ptm",
	.id            = -1,
	.num_resources = ARRAY_SIZE(ptm_resources),
	.resource      = ptm_resources,
};

void __init tegra_init_debug_uart_rate(void)
{
	unsigned int uartclk;
	struct clk *debug_uart_parent = clk_get_sys(NULL, "pll_p");

	BUG_ON(IS_ERR(debug_uart_parent));
	uartclk = clk_get_rate(debug_uart_parent);

	debug_uarta_platform_data[0].uartclk = uartclk;
	debug_uartb_platform_data[0].uartclk = uartclk;
	debug_uartc_platform_data[0].uartclk = uartclk;
	debug_uartd_platform_data[0].uartclk = uartclk;
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
	debug_uarte_platform_data[0].uartclk = uartclk;
#endif
}
