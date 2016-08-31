/*
 * arch/arm/mach-tegra/board-vcm30_t124.h
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MACH_TEGRA_BOARD_VCM30_T124_H
#define _MACH_TEGRA_BOARD_VCM30_T124_H

#include <linux/mfd/as3722-plat.h>
#include <mach/gpio-tegra.h>
#include <mach/irqs.h>
#include <linux/mfd/max77663-core.h>
#include "gpio-names.h"

int vcm30_t124_pinmux_init(void);
int vcm30_t124_panel_init(void);
int vcm30_t124_sdhci_init(void);
int vcm30_t124_sensors_init(void);
int vcm30_t124_regulator_init(void);
int vcm30_t124_suspend_init(void);
int vcm30_t124_pmon_init(void);
int vcm30_t124_panel_init(void);
int vcm30_t124_pca953x_init(void);

/* FIXME: Needed? */
#define AS3722_GPIO_BASE	TEGRA_NR_GPIOS
#define AS3722_GPIO_END	(AS3722_GPIO_BASE + AS3722_NUM_GPIO)

/* PMU_TCA6416 GPIOs */
#define PMU_TCA6416_GPIO_BASE   (AS3722_GPIO_END)
#define PMU_TCA6416_GPIO(x)     (PMU_TCA6416_GPIO_BASE + x)
#define PMU_TCA6416_NR_GPIOS    18

#define UTMI1_PORT_OWNER_XUSB   0x1
#define UTMI2_PORT_OWNER_XUSB   0x2
#define UTMI3_PORT_OWNER_XUSB   0x4
#define HSIC1_PORT_OWNER_XUSB   0x8

/* FIXME: Confirm these GPIOs */
/* Audio-related GPIOs */
#define TEGRA_GPIO_CDC_IRQ	TEGRA_GPIO_PH4
#define TEGRA_GPIO_HP_DET		TEGRA_GPIO_PR7
/*LDO_EN signal is required only for RT5639 and not for RT5645,
on Laguna the LDO_EN signal comes from a GPIO expander and
this is exposed as a fixed regulator directly handeled from
machine driver of rt5639 and for ardebeg we use the below tegra
GPIO, also the GPIO is same for T114 interposer and T124*/
#define TEGRA_GPIO_LDO_EN	TEGRA_GPIO_PR2

/* I2C related GPIOs */
#define TEGRA_GPIO_I2C1_SCL	TEGRA_GPIO_PC4
#define TEGRA_GPIO_I2C1_SDA	TEGRA_GPIO_PC5
#define TEGRA_GPIO_I2C2_SCL	TEGRA_GPIO_PT5
#define TEGRA_GPIO_I2C2_SDA	TEGRA_GPIO_PT6
#define TEGRA_GPIO_I2C3_SCL	TEGRA_GPIO_PK5
#define TEGRA_GPIO_I2C3_SDA	TEGRA_GPIO_PK6
#define TEGRA_GPIO_I2C4_SCL	TEGRA_GPIO_PV4
#define TEGRA_GPIO_I2C4_SDA	TEGRA_GPIO_PV5
#define TEGRA_GPIO_I2C5_SCL	TEGRA_GPIO_PZ6
#define TEGRA_GPIO_I2C5_SDA	TEGRA_GPIO_PZ7

/* External peripheral act as gpio */
#define MAX77663_IRQ_BASE	TEGRA_NR_IRQS
#define MAX77663_IRQ_END	(MAX77663_IRQ_BASE + MAX77663_IRQ_NR)
#define MAX77663_GPIO_BASE	TEGRA_NR_GPIOS
#define MAX77663_GPIO_END       (MAX77663_GPIO_BASE + MAX77663_GPIO_NR)

/* PCA953X - MISC SYSTEM IO */
#define PCA953X_MISCIO_GPIO_BASE        (MAX77663_GPIO_END + 1)
#define MISCIO_BT_RST_GPIO              (PCA953X_MISCIO_GPIO_BASE + 0)
#define MISCIO_GPS_RST_GPIO             (PCA953X_MISCIO_GPIO_BASE + 1)
#define MISCIO_GPS_EN_GPIO              (PCA953X_MISCIO_GPIO_BASE + 2)
#define MISCIO_WF_EN_GPIO               (PCA953X_MISCIO_GPIO_BASE + 3)
#define MISCIO_WF_RST_GPIO              (PCA953X_MISCIO_GPIO_BASE + 4)
#define MISCIO_BT_EN_GPIO               (PCA953X_MISCIO_GPIO_BASE + 5)
/* GPIO6 is not used */
#define MISCIO_NOT_USED0                (PCA953X_MISCIO_GPIO_BASE + 6)
#define MISCIO_BT_WAKEUP_GPIO           (PCA953X_MISCIO_GPIO_BASE + 7)
#define MISCIO_FAN_SEL_GPIO             (PCA953X_MISCIO_GPIO_BASE + 8)
#define MISCIO_EN_MISC_BUF_GPIO         (PCA953X_MISCIO_GPIO_BASE + 9)
#define MISCIO_EN_MSATA_GPIO            (PCA953X_MISCIO_GPIO_BASE + 10)
#define MISCIO_EN_SDCARD_GPIO           (PCA953X_MISCIO_GPIO_BASE + 11)
/* GPIO12 is not used */
#define MISCIO_NOT_USED1                (PCA953X_MISCIO_GPIO_BASE + 12)
#define MISCIO_ABB_RST_GPIO             (PCA953X_MISCIO_GPIO_BASE + 13)
#define MISCIO_USER_LED2_GPIO           (PCA953X_MISCIO_GPIO_BASE + 14)
#define MISCIO_USER_LED1_GPIO           (PCA953X_MISCIO_GPIO_BASE + 15)
#define PCA953X_MISCIO_GPIO_END         (PCA953X_MISCIO_GPIO_BASE + 16)

/* PCA953X I2C IO expander bus addresses */
#define PCA953X_MISCIO_ADDR             0x75

#endif
