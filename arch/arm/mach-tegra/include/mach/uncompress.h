/*
* arch/arm/mach-tegra/include/mach/uncompress.h
 *
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2011 Google, Inc.
 *
 * Author:
 *	Colin Cross <ccross@google.com>
 *	Erik Gilling <konkers@google.com>
 *	Doug Anderson <dianders@chromium.org>
 *	Stephen Warren <swarren@nvidia.com>
 *
 * Copyright (C) 2011-2013 NVIDIA CORPORATION. All Rights Reserved.
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

#ifndef __MACH_TEGRA_UNCOMPRESS_H
#define __MACH_TEGRA_UNCOMPRESS_H

#include <linux/types.h>
#include <linux/serial_reg.h>

#include "../../iomap.h"

#define BIT(x) (1 << (x))
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define DEBUG_UART_SHIFT 2

volatile u8 *uart;

#if defined(CONFIG_TEGRA_DEBUG_UARTA)
#define DEBUG_UART_CLK_SRC		(TEGRA_CLK_RESET_BASE + 0x178)
#define DEBUG_UART_CLK_ENB_SET_REG	(TEGRA_CLK_RESET_BASE + 0x320)
#define DEBUG_UART_CLK_ENB_SET_BIT	(1 << 6)
#define DEBUG_UART_RST_CLR_REG		(TEGRA_CLK_RESET_BASE + 0x304)
#define DEBUG_UART_RST_CLR_BIT		(1 << 6)
#elif defined(CONFIG_TEGRA_DEBUG_UARTB)
#define DEBUG_UART_CLK_SRC		(TEGRA_CLK_RESET_BASE + 0x17c)
#define DEBUG_UART_CLK_ENB_SET_REG	(TEGRA_CLK_RESET_BASE + 0x320)
#define DEBUG_UART_CLK_ENB_SET_BIT	(1 << 7)
#define DEBUG_UART_RST_CLR_REG		(TEGRA_CLK_RESET_BASE + 0x304)
#define DEBUG_UART_RST_CLR_BIT		(1 << 7)
#elif defined(CONFIG_TEGRA_DEBUG_UARTC)
#define DEBUG_UART_CLK_SRC		(TEGRA_CLK_RESET_BASE + 0x1a0)
#define DEBUG_UART_CLK_ENB_SET_REG	(TEGRA_CLK_RESET_BASE + 0x328)
#define DEBUG_UART_CLK_ENB_SET_BIT	(1 << 23)
#define DEBUG_UART_RST_CLR_REG		(TEGRA_CLK_RESET_BASE + 0x30C)
#define DEBUG_UART_RST_CLR_BIT		(1 << 23)
#elif defined(CONFIG_TEGRA_DEBUG_UARTD)
#define DEBUG_UART_CLK_SRC		(TEGRA_CLK_RESET_BASE + 0x1c0)
#define DEBUG_UART_CLK_ENB_SET_REG	(TEGRA_CLK_RESET_BASE + 0x330)
#define DEBUG_UART_CLK_ENB_SET_BIT	(1 << 1)
#define DEBUG_UART_RST_CLR_REG		(TEGRA_CLK_RESET_BASE + 0x314)
#define DEBUG_UART_RST_CLR_BIT		(1 << 1)
#elif defined(CONFIG_TEGRA_DEBUG_UARTE)
#define DEBUG_UART_CLK_SRC		(TEGRA_CLK_RESET_BASE + 0x1c4)
#define DEBUG_UART_CLK_ENB_SET_REG	(TEGRA_CLK_RESET_BASE + 0x330)
#define DEBUG_UART_CLK_ENB_SET_BIT	(1 << 2)
#define DEBUG_UART_RST_CLR_REG		(TEGRA_CLK_RESET_BASE + 0x314)
#define DEBUG_UART_RST_CLR_BIT		(1 << 2)
#else
#define DEBUG_UART_CLK_SRC		0
#define DEBUG_UART_CLK_ENB_SET_REG	0
#define DEBUG_UART_CLK_ENB_SET_BIT	0
#define DEBUG_UART_RST_CLR_REG		0
#define DEBUG_UART_RST_CLR_BIT		0
#endif
#define PLLP_BASE			(TEGRA_CLK_RESET_BASE + 0x0a0)
#define PLLP_BASE_OVERRIDE		(1 << 28)
#define PLLP_BASE_DIVP_SHIFT		20
#define PLLP_BASE_DIVP_MASK		(0x7 << 20)
#define PLLP_BASE_DIVN_SHIFT		8
#define PLLP_BASE_DIVN_MASK		(0x3FF << 8)

#define DEBUG_UART_CLK_CLKM		0x6
#define DEBUG_UART_CLK_SHIFT		29

#define DEBUG_UART_DLL_216		0x75
#define DEBUG_UART_DLL_408		0xdd
#define DEBUG_UART_DLL_204		0x6f
#define DEBUG_UART_DLL_13		0x7
static void putc(int c)
{
	if (uart == NULL)
		return;

	while (!(uart[UART_LSR << DEBUG_UART_SHIFT] & UART_LSR_THRE))
		barrier();
	uart[UART_TX << DEBUG_UART_SHIFT] = c;
}

static inline void flush(void)
{
}

static inline void konk_delay(int delay)
{
	int i;

	for (i = 0; i < (1000 * delay); i++) {
		barrier();
	}
}

static const struct {
	u32 base;
	u32 reset_reg;
	u32 clock_reg;
	u32 bit;
} uarts[] = {
	{
		TEGRA_UARTA_BASE,
		TEGRA_CLK_RESET_BASE + 0x04,
		TEGRA_CLK_RESET_BASE + 0x10,
		6,
	},
	{
		TEGRA_UARTB_BASE,
		TEGRA_CLK_RESET_BASE + 0x04,
		TEGRA_CLK_RESET_BASE + 0x10,
		7,
	},
	{
		TEGRA_UARTC_BASE,
		TEGRA_CLK_RESET_BASE + 0x08,
		TEGRA_CLK_RESET_BASE + 0x14,
		23,
	},
	{
		TEGRA_UARTD_BASE,
		TEGRA_CLK_RESET_BASE + 0x0c,
		TEGRA_CLK_RESET_BASE + 0x18,
		1,
	},
	{
		TEGRA_UARTE_BASE,
		TEGRA_CLK_RESET_BASE + 0x0c,
		TEGRA_CLK_RESET_BASE + 0x18,
		2,
	},
};

static inline bool uart_clocked(int i)
{
	if (*(u8 *)uarts[i].reset_reg & BIT(uarts[i].bit))
		return false;

	if (!(*(u8 *)uarts[i].clock_reg & BIT(uarts[i].bit)))
		return false;

	return true;
}

#ifdef CONFIG_TEGRA_DEBUG_UART_AUTO_ODMDATA
int auto_odmdata(void)
{
	volatile u32 *pmc = (volatile u32 *)TEGRA_PMC_BASE;
	u32 odmdata = pmc[0xa0 / 4];
	u32 uart_port;

	/*
	 * Bits 19:18 are the console type: 0=default, 1=none, 2==DCC, 3==UART
	 * Some boards apparently swap the last two values, but we don't have
	 * any way of catering for that here, so we just accept either. If this
	 * doesn't make sense for your board, just don't enable this feature.
	 *
	 * Bits 17:15 indicate the UART to use, 0/1/2/3/4 are UART A/B/C/D/E.
	 * 5 indicates that UART over uSD is enabled over UARTA
	 */

	switch  ((odmdata >> 18) & 3) {
	case 2:
	case 3:
		break;
	default:
		return -1;
	}

	uart_port = (odmdata >> 15) & 7;
	if (uart_port == 5)
		uart_port = 0;
	return uart_port;
}
#endif


static inline int tegra_platform_is_fpga(void)
{
#ifdef CONFIG_TEGRA_PRE_SILICON_SUPPORT
	u32 chip_id = *(volatile u32 *)(TEGRA_APB_MISC_BASE + 0x804);
	u32 minor = chip_id >> 16 & 0xf;

	return (minor == 1);
#else
	return 0;
#endif
}

/*
 * Setup before decompression.  This is where we do UART selection for
 * earlyprintk and init the uart_base register.
 */
static inline void arch_decomp_setup(void)
{
	int uart_id = -1;
	volatile u32 *addr;
	u32 uart_dll = DEBUG_UART_DLL_216;
	u32 val;

#if defined(CONFIG_TEGRA_DEBUG_UART_AUTO_ODMDATA)
	uart_id = auto_odmdata();
#elif defined(CONFIG_TEGRA_DEBUG_UARTA)
	uart_id = 0;
#elif defined(CONFIG_TEGRA_DEBUG_UARTB)
	uart_id = 1;
#elif defined(CONFIG_TEGRA_DEBUG_UARTC)
	uart_id = 2;
#elif defined(CONFIG_TEGRA_DEBUG_UARTD)
	uart_id = 3;
#elif defined(CONFIG_TEGRA_DEBUG_UARTE)
	uart_id = 4;
#endif

	if (uart_id < 0 || uart_id >= ARRAY_SIZE(uarts) ||
	    !uart_clocked(uart_id))
		uart = NULL;
	else
		uart = (volatile u8 *)uarts[uart_id].base;

	if (uart == NULL)
		return;


	addr = (volatile u32 *)DEBUG_UART_CLK_SRC;
	if (tegra_platform_is_fpga())
		/* Debug UART clock source is clk_m on FGPA Platforms. */
		*addr = DEBUG_UART_CLK_CLKM << DEBUG_UART_CLK_SHIFT;
	else
		/* Debug UART clock source is PLLP_OUT0. */
		*addr = 0;

	/* Enable clock to debug UART. */
	addr = (volatile u32 *)DEBUG_UART_CLK_ENB_SET_REG;
	*addr = DEBUG_UART_CLK_ENB_SET_BIT;

	konk_delay(5);

	/* Deassert reset to debug UART. */
	addr = (volatile u32 *)DEBUG_UART_RST_CLR_REG;
	*addr = DEBUG_UART_RST_CLR_BIT;

	konk_delay(5);

	/*
	 * On Tegra2 platforms PLLP always run at 216MHz
	 * On Tegra3 platforms PLLP can run at 216MHz, 204MHz, or 408MHz
	 * Discrimantion algorithm below assumes that PLLP is configured
	 * according to h/w recomendations with update rate 1MHz or 1.2MHz
	 * depending on oscillator frequency
	 * clk_m runs at 13 Mhz
	 */
	if (tegra_platform_is_fpga()) {
		(void) val;
		uart_dll = DEBUG_UART_DLL_13;
	} else {
		#ifdef CONFIG_ARCH_TEGRA_14x_SOC
			uart_dll = DEBUG_UART_DLL_408;
		#else
		addr = (volatile u32 *)PLLP_BASE;
		val = *addr;
		if (val & PLLP_BASE_OVERRIDE) {
			u32 p = (val & PLLP_BASE_DIVP_MASK)
					 >> PLLP_BASE_DIVP_SHIFT;
			val = (val & PLLP_BASE_DIVN_MASK)
					 >> (PLLP_BASE_DIVN_SHIFT + p);
			switch (val) {
			case 170:
			case 204:
				uart_dll = DEBUG_UART_DLL_204;
				break;
			case 340:
			case 408:
				uart_dll = DEBUG_UART_DLL_408;
				break;
			case 180:
			case 216:
			default:
				break;
			}
		}
		#endif
	}
	/* Set up debug UART. */
	uart[UART_LCR << DEBUG_UART_SHIFT] |= UART_LCR_DLAB;
	uart[UART_DLL << DEBUG_UART_SHIFT] = uart_dll;
	uart[UART_DLM << DEBUG_UART_SHIFT] = 0x0;
	uart[UART_LCR << DEBUG_UART_SHIFT] = 3;
}

#endif
