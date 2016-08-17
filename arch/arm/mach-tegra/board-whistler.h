/*
 * arch/arm/mach-tegra/board-whistler.h
 *
 * Copyright (C) 2010 Google, Inc.
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

#ifndef _MACH_TEGRA_BOARD_WHISTLER_H
#define _MACH_TEGRA_BOARD_WHISTLER_H

int whistler_regulator_init(void);
int whistler_sdhci_init(void);
int whistler_pinmux_init(void);
int whistler_panel_init(void);
int whistler_kbc_init(void);
int whistler_sensors_init(void);
int whistler_baseband_init(void);
int whistler_emc_init(void);

/* Interrupt numbers from external peripherals */
#define MAX8907C_INT_BASE       TEGRA_NR_IRQS
#define MAX8907C_INT_END        (MAX8907C_INT_BASE + 31)

/* Audio-related GPIOs */
#define TEGRA_GPIO_HP_DET		TEGRA_GPIO_PW3

/* TCA6416 GPIO expander */
#define TCA6416_GPIO_BASE		(TEGRA_NR_GPIOS)

#endif
