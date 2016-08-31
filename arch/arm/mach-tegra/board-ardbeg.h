/*
 * arch/arm/mach-tegra/board-ardbeg.h
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

#ifndef _MACH_TEGRA_BOARD_ARDBEG_H
#define _MACH_TEGRA_BOARD_ARDBEG_H

#include <linux/mfd/as3722-plat.h>
#include <mach/gpio-tegra.h>
#include <mach/irqs.h>
#include "gpio-names.h"

int ardbeg_emc_init(void);
int ardbeg_display_init(void);
int ardbeg_panel_init(void);
int ardbeg_sdhci_init(void);
int ardbeg_sata_init(void);
void arbdeg_sata_clk_gate(void);
int ardbeg_sensors_init(void);
int ardbeg_regulator_init(void);
int ardbeg_suspend_init(void);
int ardbeg_pmon_init(void);
int ardbeg_rail_alignment_init(void);
int ardbeg_soctherm_init(void);
int ardbeg_edp_init(void);
void shield_new_sysedp_init(void);
void shield_sysedp_dynamic_capping_init(void);
void shield_sysedp_batmon_init(void);


/* Invensense MPU Definitions */
#define MPU_GYRO_NAME			"mpu9250"
#define MPU_GYRO_IRQ_GPIO		TEGRA_GPIO_PS0
#define MPU_GYRO_ADDR			0x69
#define MPU_GYRO_BUS_NUM		0
#define MPU_GYRO_ORIENTATION		MTMAT_TOP_CCW_0
#define MPU_GYRO_ORIENTATION_E1762	MTMAT_TOP_CCW_270
#define MPU_COMPASS_NAME		"ak8975"
#define MPU_COMPASS_ADDR		0x0C
#define MPU_COMPASS_ADDR_TN8		0x0D
#define MPU_COMPASS_ORIENTATION		MTMAT_BOT_CCW_270
#define MPU_BMP_NAME			"bmp280"
#define MPU_BMP_ADDR			0x77

/* generated soc_therm OC interrupts */
#define TEGRA_SOC_OC_IRQ_BASE	TEGRA_NR_IRQS
#define TEGRA_SOC_OC_NUM_IRQ	TEGRA_SOC_OC_IRQ_MAX

/* PCA954x I2C bus expander bus addresses */
#define PCA954x_I2C_BUS_BASE    6
#define PCA954x_I2C_BUS0        (PCA954x_I2C_BUS_BASE + 0)
#define PCA954x_I2C_BUS1        (PCA954x_I2C_BUS_BASE + 1)
#define PCA954x_I2C_BUS2        (PCA954x_I2C_BUS_BASE + 2)
#define PCA954x_I2C_BUS3        (PCA954x_I2C_BUS_BASE + 3)


#define PALMAS_TEGRA_GPIO_BASE	TEGRA_NR_GPIOS
#define PALMAS_TEGRA_IRQ_BASE	(TEGRA_SOC_OC_IRQ_BASE + TEGRA_SOC_OC_NUM_IRQ)
#define AS3722_GPIO_BASE	TEGRA_NR_GPIOS
#define AS3722_GPIO_END	(AS3722_GPIO_BASE + AS3722_NUM_GPIO)

/* PMU_TCA6416 GPIOs */
#define PMU_TCA6416_GPIO_BASE   (AS3722_GPIO_END)
#define PMU_TCA6416_GPIO(x)     (PMU_TCA6416_GPIO_BASE + x)
#define PMU_TCA6416_NR_GPIOS    18
/* External peripheral act as interrupt controller */
/* AS3720 IRQs */
#define AS3722_IRQ_BASE         (TEGRA_SOC_OC_IRQ_BASE + TEGRA_SOC_OC_NUM_IRQ)

#define CAM_RSTN TEGRA_GPIO_PBB3
#define CAM_FLASH_STROBE TEGRA_GPIO_PBB4
#define CAM2_PWDN TEGRA_GPIO_PBB6
#define CAM1_PWDN TEGRA_GPIO_PBB5
#define CAM_AF_PWDN TEGRA_GPIO_PBB7
#define CAM_BOARD_E1806

/* Modem related GPIOs */
#define MODEM_EN		TEGRA_GPIO_PS4
#define MDM_RST			TEGRA_GPIO_PS3
#define MDM_COLDBOOT		TEGRA_GPIO_PO5
#define MDM_SAR0		TEGRA_GPIO_PG2

/* Baseband IDs */
enum tegra_bb_type {
	TEGRA_BB_BRUCE = 1,
	TEGRA_BB_HSIC_HUB = 6,
};

#define UTMI1_PORT_OWNER_XUSB   0x1
#define UTMI2_PORT_OWNER_XUSB   0x2
#define HSIC1_PORT_OWNER_XUSB   0x4
#define HSIC2_PORT_OWNER_XUSB   0x8

/* Touchscreen definitions */
#define TOUCH_GPIO_IRQ_RAYDIUM_SPI	TEGRA_GPIO_PK2
#define TOUCH_GPIO_RST_RAYDIUM_SPI	TEGRA_GPIO_PK4
#define TOUCH_SPI_ID			0	/*SPI 1 on ardbeg_interposer*/
#define TOUCH_SPI_CS			0	/*CS  0 on ardbeg_interposer*/
#define NORRIN_TOUCH_SPI_ID			2	/*SPI 2 on Norrin*/
#define NORRIN_TOUCH_SPI_CS			1	/*CS  1 on Norrin*/

#define TOUCH_GPIO_IRQ_MAXIM_STI_SPI	TEGRA_GPIO_PK2
#define TOUCH_GPIO_RST_MAXIM_STI_SPI	TEGRA_GPIO_PK4

/* Audio-related GPIOs */
/*Same GPIO's used for T114(Interposer) and T124*/
/*Below GPIO's are same for Laguna and Ardbeg*/
#define TEGRA_GPIO_CDC_IRQ	TEGRA_GPIO_PH4
#define TEGRA_GPIO_HP_DET		TEGRA_GPIO_PR7
/*LDO_EN signal is required only for RT5639 and not for RT5645,
on Laguna the LDO_EN signal comes from a GPIO expander and
this is exposed as a fixed regulator directly handeled from
machine driver of rt5639 and for ardebeg we use the below tegra
GPIO, also the GPIO is same for T114 interposer and T124*/
#define TEGRA_GPIO_LDO_EN	TEGRA_GPIO_PR2

/*GPIOs used by board panel file */
#define DSI_PANEL_RST_GPIO      TEGRA_GPIO_PH3
#define DSI_PANEL_BL_PWM_GPIO   TEGRA_GPIO_PH1

/* HDMI Hotplug detection pin */
#define ardbeg_hdmi_hpd	TEGRA_GPIO_PN7

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

/* Laguna specific */

int laguna_pinmux_init(void);
int laguna_regulator_init(void);
int laguna_pm358_pmon_init(void);
int laguna_edp_init(void);

/* Norrin specific */
int norrin_regulator_init(void);

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
int tn8_fixed_regulator_init(void);
int tn8_edp_init(void);
void tn8_new_sysedp_init(void);
void tn8_sysedp_dynamic_capping_init(void);

int tn8_p1761_pmon_init(void);

/* SATA Specific */

#define CLK_RST_CNTRL_RST_DEV_W_SET 0x7000E438
#define CLK_RST_CNTRL_RST_DEV_V_SET 0x7000E430
#define SET_CEC_RST 0x100

#endif
