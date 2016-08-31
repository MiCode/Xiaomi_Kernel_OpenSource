/*
 * arch/arm/mach-tegra/board-bonaire.h
 *
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2011 Nvidia Corporation.
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

#ifndef _MACH_TEGRA_BOARD_BONAIRE_H
#define _MACH_TEGRA_BOARD_BONAIRE_H

/* External peripheral act as gpio */
/* TPS6591x GPIOs */
#define TPS6591X_GPIO_BASE      TEGRA_NR_GPIOS
#define TPS6591X_GPIO_GP0       (TPS6591X_GPIO_BASE + 0)
#define TPS6591X_GPIO_GP1       (TPS6591X_GPIO_BASE + 1)
#define TPS6591X_GPIO_GP2       (TPS6591X_GPIO_BASE + 2)
#define TPS6591X_GPIO_GP3       (TPS6591X_GPIO_BASE + 3)
#define TPS6591X_GPIO_GP4       (TPS6591X_GPIO_BASE + 4)
#define TPS6591X_GPIO_GP5       (TPS6591X_GPIO_BASE + 5)
#define TPS6591X_GPIO_GP6       (TPS6591X_GPIO_BASE + 6)
#define TPS6591X_GPIO_GP7       (TPS6591X_GPIO_BASE + 7)
#define TPS6591X_GPIO_GP8       (TPS6591X_GPIO_BASE + 8)
#define TPS6591X_GPIO_END       (TPS6591X_GPIO_GP8 + 1)

/* PMU_TCA6416 GPIOs */
#define PMU_TCA6416_GPIO_BASE   (TPS6591X_GPIO_END)
#define PMU_TCA6416_GPIO_PORT00 (PMU_TCA6416_GPIO_BASE + 0)
#define PMU_TCA6416_GPIO_PORT01 (PMU_TCA6416_GPIO_BASE + 1)
#define PMU_TCA6416_GPIO_PORT02 (PMU_TCA6416_GPIO_BASE + 2)
#define PMU_TCA6416_GPIO_PORT03 (PMU_TCA6416_GPIO_BASE + 3)
#define PMU_TCA6416_GPIO_PORT04 (PMU_TCA6416_GPIO_BASE + 4)
#define PMU_TCA6416_GPIO_PORT05 (PMU_TCA6416_GPIO_BASE + 5)
#define PMU_TCA6416_GPIO_PORT06 (PMU_TCA6416_GPIO_BASE + 6)
#define PMU_TCA6416_GPIO_PORT07 (PMU_TCA6416_GPIO_BASE + 7)
#define PMU_TCA6416_GPIO_PORT10 (PMU_TCA6416_GPIO_BASE + 8)
#define PMU_TCA6416_GPIO_PORT11 (PMU_TCA6416_GPIO_BASE + 9)
#define PMU_TCA6416_GPIO_PORT12 (PMU_TCA6416_GPIO_BASE + 10)
#define PMU_TCA6416_GPIO_PORT13 (PMU_TCA6416_GPIO_BASE + 11)
#define PMU_TCA6416_GPIO_PORT14 (PMU_TCA6416_GPIO_BASE + 12)
#define PMU_TCA6416_GPIO_PORT15 (PMU_TCA6416_GPIO_BASE + 13)
#define PMU_TCA6416_GPIO_PORT16 (PMU_TCA6416_GPIO_BASE + 14)
#define PMU_TCA6416_GPIO_PORT17 (PMU_TCA6416_GPIO_BASE + 15)
#define PMU_TCA6416_GPIO_END    (PMU_TCA6416_GPIO_BASE + 16)

int bonaire_regulator_init(void);
int bonaire_sdhci_init(void);
int bonaire_pinmux_init(void);
int bonaire_panel_init(void);
int bonaire_sensors_init(void);
int bonaire_suspend_init(void);

int bonaire_power_off_init(void);
#endif
