/*
 * arch/arm/mach-tegra/board-pluto.h
 *
 * Copyright (c) 2012, NVIDIA Corporation.
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

#ifndef _MACH_TEGRA_BOARD_PLUTO_H
#define _MACH_TEGRA_BOARD_PLUTO_H

#include <mach/gpio-tegra.h>
#include <mach/irqs.h>
#include <linux/mfd/palmas.h>
#include <linux/mfd/max77665.h>
#include "gpio-names.h"
#include <linux/thermal.h>
#include <linux/platform_data/thermal_sensors.h>
#include "tegra11_soctherm.h"

/* External peripheral act as gpio */
/* PALMAS GPIO */
#define PALMAS_TEGRA_GPIO_BASE	TEGRA_NR_GPIOS

/* Audio-related GPIOs */
#define TEGRA_GPIO_CDC_IRQ		TEGRA_GPIO_PW3
#define TEGRA_GPIO_LDO1_EN		TEGRA_GPIO_PV3
#define TEGRA_GPIO_SPKR_EN		TEGRA_GPIO_PN4
#define TEGRA_GPIO_HP_DET		TEGRA_GPIO_PR7
#define TEGRA_GPIO_INT_MIC_EN		-1
#define TEGRA_GPIO_EXT_MIC_EN		-1

#define TEGRA_SOC_OC_IRQ_BASE		TEGRA_NR_IRQS
#define TEGRA_SOC_OC_NUM_IRQ		TEGRA_SOC_OC_IRQ_MAX

/* External peripheral act as interrupt controller */
/* PLUTO IRQs */
#define PALMAS_TEGRA_IRQ_BASE	(TEGRA_SOC_OC_IRQ_BASE + TEGRA_SOC_OC_NUM_IRQ)
#define MAX77665_TEGRA_IRQ_BASE	(PALMAS_TEGRA_IRQ_BASE + PALMAS_NUM_IRQ)

/* AIC326X IRQs */
/* Assuming TPS is the PMIC on Ent */
#define AIC3262_CODEC_IRQ_BASE (MAX77665_TEGRA_IRQ_BASE + MAX77665_NUM_IRQ)
#define AIC3262_CODEC_IRQ_END  (AIC3262_CODEC_IRQ_BASE + 6)

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
#define SYNAPTICS_ATTN_GPIO		TEGRA_GPIO_PK2
#define SYNAPTICS_RESET_GPIO		TEGRA_GPIO_PK4

#define TOUCH_GPIO_IRQ_RAYDIUM_SPI      TEGRA_GPIO_PK2
#define TOUCH_GPIO_RST_RAYDIUM_SPI      TEGRA_GPIO_PK4

/* Invensense MPU Definitions */
#define MPU_GYRO_NAME		"mpu6050"
#define MPU_GYRO_IRQ_GPIO	TEGRA_GPIO_PO7
#define MPU_GYRO_ADDR		0x69
#define MPU_GYRO_BUS_NUM	0
#define MPU_GYRO_ORIENTATION	{ -1, 0, 0, 0, -1, 0, 0, 0, 1 }
#define MPU_COMPASS_NAME	"ak8975"
#define MPU_COMPASS_IRQ_GPIO	0
#define MPU_COMPASS_ADDR	0x0D
#define MPU_COMPASS_BUS_NUM	0
#define MPU_COMPASS_ORIENTATION	{ -1, 0, 0, 0, -1, 0, 0, 0, 1 }

/* Modem1 related GPIOs */
#define MDM_RST				TEGRA_GPIO_PR3
#define MDM_COLDBOOT			TEGRA_GPIO_PO5

/* Modem2 related GPIOs */
#define MDM2_PWR_ON			TEGRA_GPIO_PX1
#define MDM2_RST			TEGRA_GPIO_PR5
#define MDM2_COLDBOOT			TEGRA_GPIO_PR4
#define MDM2_REQ1			TEGRA_GPIO_PV0
#define MDM2_ACK1			TEGRA_GPIO_PO2
#define MDM2_REQ2			TEGRA_GPIO_PV1
#define MDM2_ACK2			TEGRA_GPIO_PO3

/* Modem2 related GPIOs (for Pluto Rev A02 only) */
#define MDM2_PWR_ON_FOR_PLUTO_A02	TEGRA_GPIO_PR6
					/* If Pluto A03 or later, use PX1 */

/* OEM1 Modem related GPIOs */

#define BB_OEM1_GPIO_RST		TEGRA_GPIO_PR5
#define BB_OEM1_GPIO_ON			TEGRA_GPIO_PR6
#define BB_OEM1_GPIO_ON_V		TEGRA_GPIO_PX1
#define BB_OEM1_GPIO_AWR		TEGRA_GPIO_PG2
#define BB_OEM1_GPIO_CWR		TEGRA_GPIO_PV1
#define BB_OEM1_GPIO_SPARE		TEGRA_GPIO_PO2
#define BB_OEM1_GPIO_WDI		TEGRA_GPIO_PV0

/* OEM2 Modem related GPIOs */

#define XMM_GPIO_BB_ON			MDM2_PWR_ON /* AP -> BB */
				/* E1193 Rev B7: pin 55 = MDM_PWRON_AP2BB */
#define XMM_GPIO_BB_RST			MDM2_RST /* AP -> BB */
				/* E1193 Rev B7: pin 53 = RESET_AP2BB* */
#define XMM_GPIO_IPC_HSIC_ACTIVE	MDM2_ACK2 /* AP -> BB */
				/* E1193 Rev B7: pin 46 = HS2_AP2BB */
#define XMM_GPIO_IPC_HSIC_SUS_REQ	MDM2_REQ2 /* BB -> AP */
				/* E1193 Rev B7: pin 41 = HS2_BB2AP */
#define XMM_GPIO_IPC_BB_WAKE		MDM2_ACK1 /* AP -> BB */
				/* E1193 Rev B7: pin 45 = HS1_AP2BB */
#define XMM_GPIO_IPC_AP_WAKE		MDM2_REQ1 /* BB -> AP */
				/* E1193 Rev B7: pin 43 = HS1_BB2AP */

int pluto_regulator_init(void);
int pluto_suspend_init(void);
int pluto_sdhci_init(void);
int pluto_sensors_init(void);
int pluto_emc_init(void);
int pluto_edp_init(void);
int pluto_panel_init(void);
int pluto_kbc_init(void);
int pluto_baseband_init(void);
int pluto_pmon_init(void);
int pluto_soctherm_init(void);

extern struct tegra_sdhci_platform_data pluto_tegra_sdhci_platform_data0;

/* PCA954x I2C bus expander bus addresses */
#define PCA954x_I2C_BUS_BASE	5
#define PCA954x_I2C_BUS0	(PCA954x_I2C_BUS_BASE + 0)
#define PCA954x_I2C_BUS1	(PCA954x_I2C_BUS_BASE + 1)
#define PCA954x_I2C_BUS2	(PCA954x_I2C_BUS_BASE + 2)
#define PCA954x_I2C_BUS3	(PCA954x_I2C_BUS_BASE + 3)

/* Baseband IDs */
enum tegra_bb_type {
	TEGRA_BB_I500 = 1,
	TEGRA_BB_I500SWD,
	TEGRA_BB_OEM1,
	TEGRA_BB_OEM2,
	TEGRA_BB_OEM3,
	TEGRA_BB_HSIC_HUB,
};

#define UTMI1_PORT_OWNER_XUSB	0x1
#define HSIC1_PORT_OWNER_XUSB	0x4
#define HSIC2_PORT_OWNER_XUSB	0x8

#endif
