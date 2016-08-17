/*
 * arch/arm/mach-tegra/board-p852-sku1-b00.c
 *
 * Copyright (C) 2010-2011, NVIDIA Corporation.
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

#include "board-p852.h"

static inline void p852_sku1_b00_spi_init(void)
{
	p852_sku_peripherals |= P852_SKU_SPI_ENABLE;
	p852_spi_peripherals |=
	    ((P852_SPI_MASTER | P852_SPI_ENABLE) << P852_SPI1_SHIFT) |
	    ((P852_SPI_MASTER | P852_SPI_ENABLE) << P852_SPI4_SHIFT);
}

static inline void p852_sku1_b00_i2s_init(void)
{
	p852_sku_peripherals |= P852_SKU_I2S_ENABLE;
	p852_i2s_peripherals |= ((P852_I2S_ENABLE | P852_I2S_TDM)
		<< P852_I2S1_SHIFT) | ((P852_I2S_ENABLE | P852_I2S_TDM)
		<< P852_I2S2_SHIFT);
}

static inline void p852_sku1_b00_sdhci_init(void)
{
	p852_sku_peripherals |= P852_SKU_SDHCI_ENABLE;
	p852_sdhci_peripherals |=
	    ((P852_SDHCI_ENABLE)
	     << P852_SDHCI4_SHIFT) |
	    ((P852_SDHCI_ENABLE | P852_SDHCI_CD_EN | P852_SDHCI_WP_EN)
	     << P852_SDHCI1_SHIFT) |
	    ((P852_SDHCI_ENABLE | P852_SDHCI_CD_EN | P852_SDHCI_WP_EN)
	     << P852_SDHCI3_SHIFT);

	p852_sdhci_platform_data[0].cd_gpio = TEGRA_GPIO_PV0;
	p852_sdhci_platform_data[0].wp_gpio = TEGRA_GPIO_PV1;
	p852_sdhci_platform_data[2].cd_gpio = TEGRA_GPIO_PD7;
	p852_sdhci_platform_data[2].wp_gpio = TEGRA_GPIO_PT4;
}

static inline void p852_sku1_b00_uart_init(void)
{
	p852_sku_peripherals |= P852_SKU_UART_ENABLE;
	p852_uart_peripherals |=
	    ((P852_UART_ENABLE | P852_UART_DB) << P852_UARTD_SHIFT) |
	    ((P852_UART_ENABLE | P852_UART_HS) << P852_UARTB_SHIFT) |
	    ((P852_UART_ENABLE | P852_UART_HS | P852_UART_ALT_PIN_CFG)
	    << P852_UARTA_SHIFT);
}

static inline void p852_sku1_b00_display_init(void)
{
	p852_sku_peripherals |= P852_SKU_DISPLAY_ENABLE;
	p852_display_peripherals |=
		(P852_DISP_ENABLE << P852_DISPB_SHIFT);
}

static inline void p852_sku1_b00_ulpi_init(void)
{
	p852_sku_peripherals |= P852_SKU_ULPI_DISABLE;
}

static inline void p852_sku1_b00_i2c_init(void)
{
	p852_sku_peripherals |= P852_SKU_I2C_ENABLE;
	p852_i2c_peripherals |=
	    ((P852_I2C_ENABLE) << P852_I2C1_SHIFT) |
	    ((P852_I2C_ENABLE) << P852_I2C2_SHIFT) |
	    ((P852_I2C_ENABLE) << P852_I2C3_SHIFT) |
	    ((P852_I2C_ENABLE) << P852_I2C4_SHIFT);
}

void __init p852_sku1_b00_init(void)
{
	p852_sku_peripherals |= P852_SKU_NOR_ENABLE;

	p852_sku1_b00_spi_init();
	p852_sku1_b00_i2s_init();
	p852_sku1_b00_uart_init();
	p852_sku1_b00_sdhci_init();
	p852_sku1_b00_i2c_init();
	p852_sku1_b00_display_init();
	p852_sku1_b00_ulpi_init();

	p852_common_init();
}

