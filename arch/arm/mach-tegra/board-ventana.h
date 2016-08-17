/*
 * arch/arm/mach-tegra/board-ventana.h
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (C) 2012 NVIDIA Corporation.
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

#ifndef _MACH_TEGRA_BOARD_VENTANA_H
#define _MACH_TEGRA_BOARD_VENTANA_H

int ventana_charge_init(void);
int ventana_regulator_init(void);
int ventana_sdhci_init(void);
int ventana_pinmux_init(void);
int ventana_panel_init(void);
int ventana_sensors_init(void);
int ventana_kbc_init(void);
int ventana_emc_init(void);
int ventana_charger_init(void);
int ventana_cam_fixed_voltage_regulator_init(void);

/* PCA954x I2C bus expander bus addresses */
#define PCA954x_I2C_BUS_BASE	6
#define PCA954x_I2C_BUS0	(PCA954x_I2C_BUS_BASE + 0)
#define PCA954x_I2C_BUS1	(PCA954x_I2C_BUS_BASE + 1)
#define PCA954x_I2C_BUS2	(PCA954x_I2C_BUS_BASE + 2)

/* Sensor gpios */
#define ISL29018_IRQ_GPIO	TEGRA_GPIO_PZ2
#define AKM8975_IRQ_GPIO	TEGRA_GPIO_PN5
#define NCT1008_THERM2_GPIO	TEGRA_GPIO_PN6

#define CAMERA_POWER_GPIO	TEGRA_GPIO_PV4
#define CAMERA_CSI_MUX_SEL_GPIO	TEGRA_GPIO_PBB4
#define CAMERA_FLASH_ACT_GPIO	TEGRA_GPIO_PD2

#define PANEL_POWER_EN_GPIO	TEGRA_GPIO_PC6

/* TPS6586X gpios */
#define TPS6586X_GPIO_BASE	TEGRA_NR_GPIOS
#define TPS6586X_GPIO(_x_)	(TPS6586X_GPIO_BASE + (_x_))
#define TPS6586X_NR_GPIOS	4
#define AVDD_DSI_CSI_ENB_GPIO	TPS6586X_GPIO(1) /* gpio2 */
#define TPS6586X_GPIO_END	TPS6586X_GPIO(TPS6586X_NR_GPIOS - 1)

/* TCA6416 gpios */
#define TCA6416_GPIO_BASE	(TPS6586X_GPIO_END + 1)
#define TCA6416_GPIO(_x_)	(TCA6416_GPIO_BASE + (_x_))
#define TCA6416_NR_GPIOS	16
#define CAM1_PWR_DN_GPIO	TCA6416_GPIO(0) /* gpio0 */
#define CAM1_RST_L_GPIO		TCA6416_GPIO(1) /* gpio1 */
#define CAM1_AF_PWR_DN_L_GPIO	TCA6416_GPIO(2) /* gpio2 */
#define CAM1_LDO_SHUTDN_L_GPIO	TCA6416_GPIO(3) /* gpio3 */
#define CAM2_PWR_DN_GPIO	TCA6416_GPIO(4) /* gpio4 */
#define CAM2_RST_L_GPIO		TCA6416_GPIO(5) /* gpio5 */
#define CAM2_AF_PWR_DN_L_GPIO	TCA6416_GPIO(6) /* gpio6 */
#define CAM2_LDO_SHUTDN_L_GPIO	TCA6416_GPIO(7) /* gpio7 */
#define CAM3_PWR_DN_GPIO	TCA6416_GPIO(8) /* gpio8 */
#define CAM3_RST_L_GPIO		TCA6416_GPIO(9) /* gpio9 */
#define CAM3_AF_PWR_DN_L_GPIO	TCA6416_GPIO(10) /* gpio10 */
#define CAM3_LDO_SHUTDN_L_GPIO	TCA6416_GPIO(11) /* gpio11 */
#define CAM_LED_GPIO		TCA6416_GPIO(12) /* gpio12 */
#define CAM_I2C_MUX_RST_GPIO	TCA6416_GPIO(15) /* gpio15 */
#define TCA6416_GPIO_END	TCA6416_GPIO(TCA6416_NR_GPIOS - 1)

/* WM8903 GPIOs */
#define WM8903_GPIO_BASE	(TCA6416_GPIO_END + 1)
#define WM8903_GPIO(_x_)	(WM8903_GPIO_BASE + (_x_))
#define WM8903_NR_GPIOS		4
#define WM8903_GPIO_END		WM8903_GPIO(WM8903_NR_GPIOS - 1)

/* Audio-related GPIOs */
#define TEGRA_GPIO_CDC_IRQ	TEGRA_GPIO_PX3
#define TEGRA_GPIO_SPKR_EN	WM8903_GPIO(2)
#define TEGRA_GPIO_HP_DET	TEGRA_GPIO_PW2
#define TEGRA_GPIO_HP_DET	TEGRA_GPIO_PW2
#define TEGRA_GPIO_INT_MIC_EN	TEGRA_GPIO_PX0
#define TEGRA_GPIO_EXT_MIC_EN	TEGRA_GPIO_PX1

/* Usb1 vbus GPIO */
#define TEGRA_GPIO_USB1_VBUS	TEGRA_GPIO_PD0

/* AC detect GPIO */
#define AC_PRESENT_GPIO		TEGRA_GPIO_PV3

/* Interrupt numbers from external peripherals */
#define TPS6586X_INT_BASE	TEGRA_NR_IRQS
#define TPS6586X_INT_END	(TPS6586X_INT_BASE + 32)

/* Invensense MPU Definitions */
#define MPU_TYPE_MPU3050	1
#define MPU_TYPE_MPU6050	2
#define MPU_GYRO_TYPE		MPU_TYPE_MPU3050
#define MPU_GYRO_IRQ_GPIO	TEGRA_GPIO_PZ4
#define MPU_GYRO_ADDR		0x68
#define MPU_GYRO_BUS_NUM	0
#define MPU_GYRO_ORIENTATION	{ 0, -1, 0, -1, 0, 0, 0, 0, -1 }
#define MPU_ACCEL_NAME		"kxtf9"
#define MPU_ACCEL_IRQ_GPIO	0 /* Disable ACCELIRQ: TEGRA_GPIO_PN4 */
#define MPU_ACCEL_ADDR		0x0F
#define MPU_ACCEL_BUS_NUM	0
#define MPU_ACCEL_ORIENTATION	{ 0, -1, 0, -1, 0, 0, 0, 0, -1 }
#define MPU_COMPASS_NAME	"ak8975"
#define MPU_COMPASS_IRQ_GPIO	TEGRA_GPIO_PN5
#define MPU_COMPASS_ADDR	0x0C
#define MPU_COMPASS_BUS_NUM	4
#define MPU_COMPASS_ORIENTATION	{ 1, 0, 0, 0, 1, 0, 0, 0, 1 }

#endif
