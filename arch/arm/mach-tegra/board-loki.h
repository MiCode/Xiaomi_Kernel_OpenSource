/*
 * arch/arm/mach-tegra/board-loki.h
 *
 * Copyright (c) 2013, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _MACH_TEGRA_BOARD_LOKI_H
#define _MACH_TEGRA_BOARD_LOKI_H

#include <mach/gpio-tegra.h>
#include <mach/irqs.h>
#include "gpio-names.h"

int loki_pinmux_init(void);
int loki_panel_init(void);
int loki_kbc_init(void);
int loki_sdhci_init(void);
int loki_sensors_init(void);
int loki_regulator_init(void);
int loki_suspend_init(void);
int loki_pmon_init(void);
int loki_edp_init(void);
int loki_rail_alignment_init(void);
int loki_soctherm_init(void);
int loki_emc_init(void);

/* Invensense MPU Definitions */
#define MPU_GYRO_NAME			"mpu6050"
#define MPU_GYRO_IRQ_GPIO		TEGRA_GPIO_PR2
#define MPU_GYRO_ADDR			0x68
#define MPU_GYRO_BUS_NUM		0
#define MPU_GYRO_ORIENTATION		MTMAT_BOT_CCW_0
#define MPU_GYRO_ORIENTATION_FAB0	MTMAT_BOT_CCW_90
#define MPU_GYRO_ORIENTATION_T_1_95	{ 0, 1, 0, 0, 0, 1, 1, 0, 0 }
#define MPU_COMPASS_NAME		"ak8975"
#define MPU_COMPASS_ADDR		0x0C
#define MPU_COMPASS_ORIENTATION		MTMAT_BOT_CCW_180

/* PCA954x I2C bus expander bus addresses */
#define PCA954x_I2C_BUS_BASE    6
#define PCA954x_I2C_BUS0        (PCA954x_I2C_BUS_BASE + 0)
#define PCA954x_I2C_BUS1        (PCA954x_I2C_BUS_BASE + 1)
#define PCA954x_I2C_BUS2        (PCA954x_I2C_BUS_BASE + 2)
#define PCA954x_I2C_BUS3        (PCA954x_I2C_BUS_BASE + 3)


#define PALMAS_TEGRA_GPIO_BASE	TEGRA_NR_GPIOS
#define PALMAS_TEGRA_IRQ_BASE	TEGRA_NR_IRQS
#define AS3722_GPIO_BASE	TEGRA_NR_GPIOS
#define AS3722_GPIO_END	(AS3722_GPIO_BASE + AS3722_NUM_GPIO)

/* PMU_TCA6416 GPIOs */
#define PMU_TCA6416_GPIO_BASE   (AS3722_GPIO_END)
#define PMU_TCA6416_GPIO(x)     (PMU_TCA6416_GPIO_BASE + x)
#define PMU_TCA6416_NR_GPIOS    18
/* External peripheral act as interrupt controller */
/* AS3720 IRQs */
#define AS3722_IRQ_BASE         TEGRA_NR_IRQS

#define CAM_RSTN TEGRA_GPIO_PBB3
#define CAM_FLASH_STROBE TEGRA_GPIO_PBB4
#define CAM2_PWDN TEGRA_GPIO_PBB6
#define CAM1_PWDN TEGRA_GPIO_PBB5
#define CAM_AF_PWDN TEGRA_GPIO_PBB7

/* Modem related GPIOs */
#define MDM_RST			TEGRA_GPIO_PS3
#define MDM_COLDBOOT		TEGRA_GPIO_PO5

/* Hall Effect Sensor GPIO */
#define TEGRA_GPIO_HALL		TEGRA_GPIO_PS0

/* Baseband IDs */
enum tegra_bb_type {
	TEGRA_BB_BRUCE = 1,
	TEGRA_BB_HSIC_HUB = 6,
};

#define UTMI1_PORT_OWNER_XUSB   0x1
#define UTMI2_PORT_OWNER_XUSB   0x2
#define UTMI3_PORT_OWNER_XUSB   0x4
#define HSIC1_PORT_OWNER_XUSB   0x8

/* Touchscreen definitions */
enum touch_panel_id {
	TOUCH_PANEL_RESERVED = 0,
	TOUCH_PANEL_WINTEK,
	TOUCH_PANEL_TPK,
	TOUCH_PANEL_TOUCHTURNS,
	TOUCH_PANEL_THOR_WINTEK,
	TOUCH_PANEL_LOKI_WINTEK_5_66_UNLAMIN,
};

#define TOUCH_GPIO_IRQ_RAYDIUM_SPI	TEGRA_GPIO_PK2
#define TOUCH_GPIO_RST_RAYDIUM_SPI	TEGRA_GPIO_PK4
#define TOUCH_SPI_ID			0	/*SPI 1 on ardbeg_interposer*/
#define TOUCH_SPI_CS			0	/*CS  0 on ardbeg_interposer*/

/* Audio-related GPIOs */
/*Same GPIO's used for T114(Interposer) and T124*/
/*Below GPIO's are same for Laguna and Loki*/
#define TEGRA_GPIO_CDC_IRQ	TEGRA_GPIO_PH4
#define TEGRA_GPIO_HP_DET		TEGRA_GPIO_PR7
/*LDO_EN signal is required only for RT5639 and not for RT5645,
on Laguna the LDO_EN signal comes from a GPIO expander and
this is exposed as a fixed regulator directly handeled from
machine driver of rt5639 and for ardebeg we use the below tegra
GPIO, also the GPIO is same for T114 interposer and T124*/
#define TEGRA_GPIO_LDO_EN	TEGRA_GPIO_PV3

/*GPIOs used by board panel file */
#define DSI_PANEL_RST_GPIO      TEGRA_GPIO_PH3
#define DSI_PANEL_BL_PWM_GPIO   TEGRA_GPIO_PH1

/* HDMI Hotplug detection pin */
#define loki_hdmi_hpd	TEGRA_GPIO_PN7

/* I2C related GPIOs */
/* Same for interposer and t124 */
#define TEGRA_GPIO_I2C1_SCL	TEGRA_GPIO_PC4
#define TEGRA_GPIO_I2C1_SDA	TEGRA_GPIO_PC5
#define TEGRA_GPIO_I2C2_SCL	TEGRA_GPIO_PT5
#define TEGRA_GPIO_I2C2_SDA	TEGRA_GPIO_PT6
#define TEGRA_GPIO_I2C3_SCL	TEGRA_GPIO_PBB1
#define TEGRA_GPIO_I2C3_SDA	TEGRA_GPIO_PBB2
#define TEGRA_GPIO_I2C4_SCL	TEGRA_GPIO_PV4
#define TEGRA_GPIO_I2C4_SDA	TEGRA_GPIO_PV5
#define TEGRA_GPIO_I2C5_SCL	TEGRA_GPIO_PZ6
#define TEGRA_GPIO_I2C5_SDA	TEGRA_GPIO_PZ7

/* AUO Display related GPIO */
#define DSI_PANEL_RST_GPIO      TEGRA_GPIO_PH3 /* GMI_AD11 */
#define LCD_RST_L               TEGRA_GPIO_PH5 /* GMI_AD13 */
#define LCD_LR                  TEGRA_GPIO_PH6 /* GMI_AD14 */
#define LCD_TE                  TEGRA_GPIO_PI4 /* GMI_RST_N */
#define DSI_PANEL_BL_PWM        TEGRA_GPIO_PH1 /*GMI_AD9 */
#define en_vdd_bl       TEGRA_GPIO_PP2 /* DAP3_DOUT */
#define lvds_en         TEGRA_GPIO_PI0 /* GMI_WR_N */
#define refclk_en       TEGRA_GPIO_PG4 /* GMI_AD4 */

/* HID keyboard and trackpad irq same for interposer and t124 */
#define I2C_KB_IRQ	TEGRA_GPIO_PC7
#define I2C_TP_IRQ	TEGRA_GPIO_PW3

/* TN8 specific */

int tn8_regulator_init(void);
int loki_fan_init(void);

enum {
	P2530 = 0,
	E2549 = 1,
	E2548 = 2,
};

#endif
