/*
 * arch/arm/mach-tegra/board-p1852.h
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _MACH_TEGRA_BOARD_P1852_H
#define _MACH_TEGRA_BOARD_P1852_H

#include <mach/gpio.h>
#include <mach/irqs.h>
#include <linux/gpio.h>
#include <linux/mfd/tps6591x.h>


/* External peripheral act as gpio */
/* TPS6591x GPIOs */
#define TPS6591X_GPIO_BASE	TEGRA_NR_GPIOS
#define TPS6591X_GPIO_GP0	(TPS6591X_GPIO_BASE + 0)
#define TPS6591X_GPIO_GP1	(TPS6591X_GPIO_BASE + 1)
#define TPS6591X_GPIO_GP2	(TPS6591X_GPIO_BASE + 2)
#define TPS6591X_GPIO_GP3	(TPS6591X_GPIO_BASE + 3)
#define TPS6591X_GPIO_GP4	(TPS6591X_GPIO_BASE + 4)
#define TPS6591X_GPIO_GP5	(TPS6591X_GPIO_BASE + 5)
#define TPS6591X_GPIO_GP6	(TPS6591X_GPIO_BASE + 6)
#define TPS6591X_GPIO_GP7	(TPS6591X_GPIO_BASE + 7)
#define TPS6591X_GPIO_GP8	(TPS6591X_GPIO_BASE + 8)
#define TPS6591X_GPIO_END	TPS6591X_GPIO_GP8

/* CAM_TCA6416 GPIOs */
#define CAM_TCA6416_GPIO_BASE		(TPS6591X_GPIO_END + 1)
#define CAM1_PWR_DN_GPIO		(CAM_TCA6416_GPIO_BASE + 0)
#define CAM1_RST_L_GPIO			(CAM_TCA6416_GPIO_BASE + 1)
#define CAM1_AF_PWR_DN_L_GPIO		(CAM_TCA6416_GPIO_BASE + 2)
#define CAM1_LDO_SHUTDN_L_GPIO		(CAM_TCA6416_GPIO_BASE + 3)
#define CAM2_PWR_DN_GPIO		(CAM_TCA6416_GPIO_BASE + 4)
#define CAM2_RST_L_GPIO			(CAM_TCA6416_GPIO_BASE + 5)
#define CAM2_AF_PWR_DN_L_GPIO		(CAM_TCA6416_GPIO_BASE + 6)
#define CAM2_LDO_SHUTDN_L_GPIO		(CAM_TCA6416_GPIO_BASE + 7)
#define CAM_FRONT_PWR_DN_GPIO		(CAM_TCA6416_GPIO_BASE + 8)
#define CAM_FRONT_RST_L_GPIO		(CAM_TCA6416_GPIO_BASE + 9)
#define CAM_FRONT_AF_PWR_DN_L_GPIO	(CAM_TCA6416_GPIO_BASE + 10)
#define CAM_FRONT_LDO_SHUTDN_L_GPIO	(CAM_TCA6416_GPIO_BASE + 11)
#define CAM_FRONT_LED_EXP		(CAM_TCA6416_GPIO_BASE + 12)
#define CAM_SNN_LED_REAR_EXP		(CAM_TCA6416_GPIO_BASE + 13)
/* PIN 19 NOT USED and is reserved */
#define CAM_NOT_USED			(CAM_TCA6416_GPIO_BASE + 14)
#define CAM_I2C_MUX_RST_EXP		(CAM_TCA6416_GPIO_BASE + 15)
#define CAM_TCA6416_GPIO_END		CAM_I2C_MUX_RST_EXP

/* WM8903 gpios */
#define WM8903_GPIO_BASE	(CAM_TCA6416_GPIO_END + 1)
#define WM8903_GP1		(WM8903_GPIO_BASE + 0)
#define WM8903_GP2		(WM8903_GPIO_BASE + 1)
#define WM8903_GP3		(WM8903_GPIO_BASE + 2)
#define WM8903_GP4		(WM8903_GPIO_BASE + 3)
#define WM8903_GP5		(WM8903_GPIO_BASE + 4)
#define WM8903_GPIO_END		WM8903_GP5

/* CAMERA RELATED GPIOs on p1852 */
#define OV5650_RESETN_GPIO		TEGRA_GPIO_PBB0
#define CAM1_POWER_DWN_GPIO		TEGRA_GPIO_PBB5
#define CAM2_POWER_DWN_GPIO		TEGRA_GPIO_PBB6
#define CAM3_POWER_DWN_GPIO		TEGRA_GPIO_PBB7
#define CAMERA_CSI_CAM_SEL_GPIO		TEGRA_GPIO_PBB4
#define CAMERA_CSI_MUX_SEL_GPIO		TEGRA_GPIO_PCC1
#define CAM1_LDO_EN_GPIO		TEGRA_GPIO_PR6
#define CAM2_LDO_EN_GPIO		TEGRA_GPIO_PR7
#define CAM3_LDO_EN_GPIO		TEGRA_GPIO_PS0


#define AC_PRESENT_GPIO		TPS6591X_GPIO_GP4
/*****************Interrupt tables ******************/
/* External peripheral act as interrupt controller */
/* TPS6591x IRQs */
#define TPS6591X_IRQ_BASE	TEGRA_NR_IRQS
#define TPS6591X_IRQ_END	(TPS6591X_IRQ_BASE + 18)

#define AC_PRESENT_INT		(TPS6591X_INT_GPIO4 + TPS6591X_IRQ_BASE)

/* List of P1852 skus - replicated from core/include/nvmachtypes.h */
#define TEGRA_P1852_SKU2_A00  0x020000UL
#define TEGRA_P1852_SKU2_B00  0x020200UL
#define TEGRA_P1852_SKU5_A00  0x050000UL
#define TEGRA_P1852_SKU5_B00  0x050200UL
#define TEGRA_P1852_SKU8_A00  0x080000UL
#define TEGRA_P1852_SKU8_B00  0x080200UL

int p1852_sdhci_init(void);
int p1852_pinmux_init(void);
int p1852_pinmux_set_i2s4_master(void);
int p1852_panel_init(void);
int p1852_gpio_init(void);
int p1852_pins_state_init(void);
int p1852_suspend_init(void);

int p1852_get_skuid(void);

#ifdef CONFIG_TOUCHSCREEN_ATMEL_MXT
#define TOUCH_GPIO_IRQ_ATMEL_T9 TEGRA_GPIO_PEE1
#define TOUCH_GPIO_RST_ATMEL_T9 TEGRA_GPIO_PW2
#define TOUCH_BUS_ATMEL_T9  0
#endif


#endif
