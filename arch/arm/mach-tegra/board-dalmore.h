/*
 * arch/arm/mach-tegra/board-dalmore.h
 *
 * Copyright (c) 2012-2013, NVIDIA Corporation. All rights reserved.
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

#ifndef _MACH_TEGRA_BOARD_DALMORE_H
#define _MACH_TEGRA_BOARD_DALMORE_H

#include <mach/irqs.h>
#include <linux/mfd/max77663-core.h>
#include <linux/mfd/tps65090.h>
#include "gpio-names.h"

/* External peripheral act as gpio */
/* MAX77663 GPIO */
#define MAX77663_GPIO_BASE      TEGRA_NR_GPIOS
#define PALMAS_TEGRA_GPIO_BASE	TEGRA_NR_GPIOS
#define MAX77663_GPIO_END       (MAX77663_GPIO_BASE + MAX77663_GPIO_NR)

/* Hall Effect Sensor GPIO */
#define TEGRA_GPIO_HALL		TEGRA_GPIO_PS0

/* Audio-related GPIOs */
#define TEGRA_GPIO_CDC_IRQ		TEGRA_GPIO_PW3
#define TEGRA_GPIO_LDO1_EN		TEGRA_GPIO_PV3
#define TEGRA_GPIO_CODEC1_EN	TEGRA_GPIO_PP3
#define TEGRA_GPIO_CODEC2_EN	TEGRA_GPIO_PP1
#define TEGRA_GPIO_CODEC3_EN	TEGRA_GPIO_PV0

#define TEGRA_GPIO_SPKR_EN		-1
#define TEGRA_GPIO_HP_DET		TEGRA_GPIO_PR7
#define TEGRA_GPIO_INT_MIC_EN		TEGRA_GPIO_PK3
#define TEGRA_GPIO_EXT_MIC_EN		-1

#define TEGRA_GPIO_W_DISABLE		TEGRA_GPIO_PDD7
#define TEGRA_GPIO_MODEM_RSVD1		TEGRA_GPIO_PV0
#define TEGRA_GPIO_MODEM_RSVD2		TEGRA_GPIO_PH7

#define TPS65090_TEGRA_IRQ_BASE TEGRA_NR_IRQS
#define TPS65090_TEGRA_IRQ_END	(TPS65090_TEGRA_IRQ_BASE + 16)
/* External peripheral act as interrupt controller */
/* MAX77663 IRQs */
#define PALMAS_TEGRA_IRQ_BASE   TPS65090_TEGRA_IRQ_END
#define PALMAS_TEGRA_IRQ_END	(PALMAS_TEGRA_IRQ_BASE + PALMAS_NUM_IRQ)
#define MAX77663_IRQ_BASE	TPS65090_TEGRA_IRQ_END
#define MAX77663_IRQ_END	(MAX77663_IRQ_BASE + MAX77663_IRQ_NR)
#define MAX77663_IRQ_ACOK_RISING MAX77663_IRQ_ONOFF_ACOK_RISING

/* PMC Wake status registers */
#define PMC_WAKE_STATUS		0x14
#define PMC_WAKE2_STATUS	0x168

/* I2C related GPIOs */
#define TEGRA_GPIO_I2C1_SCL		TEGRA_GPIO_PC4
#define TEGRA_GPIO_I2C1_SDA             TEGRA_GPIO_PC5
#define TEGRA_GPIO_I2C2_SCL             TEGRA_GPIO_PT5
#define TEGRA_GPIO_I2C2_SDA             TEGRA_GPIO_PT6
#define TEGRA_GPIO_I2C3_SCL             TEGRA_GPIO_PBB1
#define TEGRA_GPIO_I2C3_SDA             TEGRA_GPIO_PBB2
#define TEGRA_GPIO_I2C4_SCL             TEGRA_GPIO_PV4
#define TEGRA_GPIO_I2C4_SDA             TEGRA_GPIO_PV5
#define TEGRA_GPIO_I2C5_SCL             TEGRA_GPIO_PZ6
#define TEGRA_GPIO_I2C5_SDA             TEGRA_GPIO_PZ7

/* Camera related GPIOs */
#define CAM_RSTN			TEGRA_GPIO_PBB3
#define CAM_FLASH_STROBE		TEGRA_GPIO_PBB4
#define CAM1_POWER_DWN_GPIO		TEGRA_GPIO_PBB5
#define CAM2_POWER_DWN_GPIO		TEGRA_GPIO_PBB6
#define CAM_AF_PWDN			TEGRA_GPIO_PBB7
#define CAM_GPIO1			TEGRA_GPIO_PCC1
#define CAM_GPIO2			TEGRA_GPIO_PCC2

/* Touchscreen definitions */
#define TOUCH_GPIO_IRQ_RAYDIUM_SPI      TEGRA_GPIO_PK2
#define TOUCH_GPIO_RST_RAYDIUM_SPI      TEGRA_GPIO_PK4

/* Invensense MPU Definitions */
#define MPU_GYRO_NAME           "mpu9150"
#define MPU_GYRO_IRQ_GPIO       TEGRA_GPIO_PR3
#define MPU_GYRO_ADDR           0x69
#define MPU_GYRO_BUS_NUM        0
#define MPU_GYRO_ORIENTATION	{ -1, 0, 0, 0, 1, 0, 0, 0, -1 }
#define MPU_COMPASS_NAME        "ak8975"
#define MPU_COMPASS_IRQ_GPIO    0
#define MPU_COMPASS_ADDR        0x0D
#define MPU_COMPASS_BUS_NUM     0
#define MPU_COMPASS_ORIENTATION { 0, -1, 0, 1, 0, 0, 0, 0, 1 }

/* Modem related GPIOs */
#define MODEM_EN		TEGRA_GPIO_PP2
#define MDM_RST			TEGRA_GPIO_PP0
#define MDM_COLDBOOT		TEGRA_GPIO_PQ5

int dalmore_regulator_init(void);
int dalmore_suspend_init(void);
int dalmore_sdhci_init(void);
int dalmore_sensors_init(void);
int dalmore_emc_init(void);
int dalmore_edp_init(void);
int dalmore_panel_init(void);
int roth_panel_init(int board_id);
int dalmore_kbc_init(void);
int dalmore_pmon_init(void);
int dalmore_soctherm_init(void);

extern struct tegra_sdhci_platform_data dalmore_tegra_sdhci_platform_data0;

/* Baseband IDs */
enum tegra_bb_type {
	TEGRA_BB_NEMO = 1,
};

#define UTMI1_PORT_OWNER_XUSB	0x1
#define UTMI2_PORT_OWNER_XUSB	0x2
#define HSIC1_PORT_OWNER_XUSB	0x4

#endif
