/*
 * arch/arm/mach-tegra/board-laguna-powermon.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
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
 *
 */

#include <linux/i2c.h>
#include <linux/platform_data/ina230.h>

#include "board.h"
#include "board-ardbeg.h"

#define PRECISION_MULTIPLIER_LAGUNA	1000

#define AVG_SAMPLES (2 << 9) /* 16 samples */

/* AVG is specified from platform data */
#define INA230_CONT_CONFIG	(AVG_SAMPLES | INA230_VBUS_CT | \
				INA230_VSH_CT | INA230_CONT_MODE)
#define INA230_TRIG_CONFIG	(AVG_SAMPLES | INA230_VBUS_CT | \
				 INA230_VSH_CT | INA230_TRIG_MODE)

enum {
	VDD_MUX,
	VDD_CPU,
	VDDIO_DDR_AP_1V35,
	VDD_CORE,
	COM_1V8,
	VDDIO_DDR_1V35,
	AVDDIO_PEX_AP_1V05,
	PEX_PLL_AP_3V3,
	VDD_USB_AP_3V3,
	VDD_GPU,
	HVDD_SATA_AP_3V3,
	VDDIO_SYS_AP_1V8,
	VDDIO_BB_AP,
	AVDD_LVDS_AP_1V05,
	AVDD_HDMI_AP_3V3,
};

/* power monitor parameters for Laguna PM358*/
static struct ina230_platform_data laguna_pm358_power_mon_info[] = {
	[VDD_MUX] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 3.63304501 * PRECISION_MULTIPLIER_LAGUNA,
		.rail_name = "VDD_MUX",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_LAGUNA,
	},
	[VDD_CPU] = {
		.calibration_data  = 0x3E6A,
		.power_lsb = 16.02172852 * PRECISION_MULTIPLIER_LAGUNA,
		.rail_name = "VDD_CPU",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_LAGUNA,
	},
	[VDDIO_DDR_AP_1V35] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.550446687 * PRECISION_MULTIPLIER_LAGUNA,
		.rail_name = "VDDIO_DDR_AP_1V35",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_LAGUNA,
	},
	[VDD_CORE] = {
		.calibration_data  = 0x369D,
		.power_lsb = 4.577636719 * PRECISION_MULTIPLIER_LAGUNA,
		.rail_name = "VDD_CORE",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_LAGUNA,
	},
	[COM_1V8] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.053405762 * PRECISION_MULTIPLIER_LAGUNA,
		.rail_name = "COM_1V8",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_LAGUNA,
	},
	[VDDIO_DDR_1V35] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.762939453 * PRECISION_MULTIPLIER_LAGUNA,
		.rail_name = "VDDIO_DDR_1V35",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_LAGUNA,
	},
	[AVDDIO_PEX_AP_1V05] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.356038411 * PRECISION_MULTIPLIER_LAGUNA,
		.rail_name = "AVDDIO_PEX_AP_1V05",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_LAGUNA,
	},
	[PEX_PLL_AP_3V3] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.027743253 * PRECISION_MULTIPLIER_LAGUNA,
		.rail_name = "PEX_PLL_AP_3V3",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_LAGUNA,
	},
	[VDD_USB_AP_3V3] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.023119377 * PRECISION_MULTIPLIER_LAGUNA,
		.rail_name = "VDD_USB_AP_3V3",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_LAGUNA,
	},
	[VDD_GPU] = {
		.calibration_data  = 0x4EF5,
		.power_lsb = 6.332397461 * PRECISION_MULTIPLIER_LAGUNA,
		.rail_name = "VDD_GPU",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_LAGUNA,
	},
	[HVDD_SATA_AP_3V3] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.003467907 * PRECISION_MULTIPLIER_LAGUNA,
		.rail_name = "HVDD_SATA_AP_3V3",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_LAGUNA,
	},
	[VDDIO_SYS_AP_1V8] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.023312039 * PRECISION_MULTIPLIER_LAGUNA,
		.rail_name = "VDDIO_SYS_AP_1V8",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_LAGUNA,
	},
	[VDDIO_BB_AP] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.002288818 * PRECISION_MULTIPLIER_LAGUNA,
		.rail_name = "VDDIO_BB_AP",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_LAGUNA,
	},
	[AVDD_LVDS_AP_1V05] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.007629395 * PRECISION_MULTIPLIER_LAGUNA,
		.rail_name = "AVDD_LVDS_AP_1V05",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_LAGUNA,
	},
	[AVDD_HDMI_AP_3V3] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.070514101 * PRECISION_MULTIPLIER_LAGUNA,
		.rail_name = "AVDD_HDMI_AP_3V3",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_LAGUNA,
	},

};

enum {
	INA_I2C_ADDR_40,
	INA_I2C_ADDR_41,
	INA_I2C_ADDR_42,
	INA_I2C_ADDR_43,
	INA_I2C_ADDR_44,
	INA_I2C_ADDR_45,
	INA_I2C_ADDR_46,
	INA_I2C_ADDR_47,
	INA_I2C_ADDR_48,
	INA_I2C_ADDR_49,
	INA_I2C_ADDR_4A,
	INA_I2C_ADDR_4B,
	INA_I2C_ADDR_4D,
	INA_I2C_ADDR_4E,
	INA_I2C_ADDR_4F,
};

static struct i2c_board_info laguna_pm358_i2c_ina230_board_info[] = {
	[INA_I2C_ADDR_40] = {
		I2C_BOARD_INFO("ina230", 0x40),
		.platform_data = &laguna_pm358_power_mon_info[VDD_MUX],
		.irq = -1,
	},
	[INA_I2C_ADDR_41] = {
		I2C_BOARD_INFO("ina230", 0x41),
		.platform_data = &laguna_pm358_power_mon_info[VDD_CPU],
		.irq = -1,
	},
	[INA_I2C_ADDR_42] = {
		I2C_BOARD_INFO("ina230", 0x42),
		.platform_data =
		&laguna_pm358_power_mon_info[VDDIO_DDR_AP_1V35],
		.irq = -1,
	},
	[INA_I2C_ADDR_43] = {
		I2C_BOARD_INFO("ina230", 0x43),
		.platform_data = &laguna_pm358_power_mon_info[VDD_CORE],
		.irq = -1,
	},
	[INA_I2C_ADDR_44] = {
		I2C_BOARD_INFO("ina230", 0x44),
		.platform_data = &laguna_pm358_power_mon_info[COM_1V8],
		.irq = -1,
	},
	[INA_I2C_ADDR_45] = {
		I2C_BOARD_INFO("ina230", 0x45),
		.platform_data = &laguna_pm358_power_mon_info[VDDIO_DDR_1V35],
		.irq = -1,
	},
	[INA_I2C_ADDR_46] = {
		I2C_BOARD_INFO("ina230", 0x46),
		.platform_data =
		&laguna_pm358_power_mon_info[AVDDIO_PEX_AP_1V05],
		.irq = -1,
	},
	[INA_I2C_ADDR_47] = {
		I2C_BOARD_INFO("ina230", 0x47),
		.platform_data = &laguna_pm358_power_mon_info[PEX_PLL_AP_3V3],
		.irq = -1,
	},
	[INA_I2C_ADDR_48] = {
		I2C_BOARD_INFO("ina230", 0x48),
		.platform_data = &laguna_pm358_power_mon_info[VDD_USB_AP_3V3],
		.irq = -1,
	},
	[INA_I2C_ADDR_49] = {
		I2C_BOARD_INFO("ina230", 0x49),
		.platform_data = &laguna_pm358_power_mon_info[VDD_GPU],
		.irq = -1,
	},
	[INA_I2C_ADDR_4A] = {
		I2C_BOARD_INFO("ina230", 0x4A),
		.platform_data =
		&laguna_pm358_power_mon_info[HVDD_SATA_AP_3V3],
		.irq = -1,
	},
	[INA_I2C_ADDR_4B] = {
		I2C_BOARD_INFO("ina230", 0x4B),
		.platform_data =
		&laguna_pm358_power_mon_info[VDDIO_SYS_AP_1V8],
		.irq = -1,
	},
	[INA_I2C_ADDR_4D] = {
		I2C_BOARD_INFO("ina230", 0x4D),
		.platform_data = &laguna_pm358_power_mon_info[VDDIO_BB_AP],
		.irq = -1,
	},
	[INA_I2C_ADDR_4E] = {
		I2C_BOARD_INFO("ina230", 0x4E),
		.platform_data =
		&laguna_pm358_power_mon_info[AVDD_LVDS_AP_1V05],
		.irq = -1,
	},
	[INA_I2C_ADDR_4F] = {
		I2C_BOARD_INFO("ina230", 0x4F),
		.platform_data =
		&laguna_pm358_power_mon_info[AVDD_HDMI_AP_3V3],
		.irq = -1,
	},

};

int __init laguna_pm358_pmon_init(void)
{
	i2c_register_board_info(1, laguna_pm358_i2c_ina230_board_info,
		ARRAY_SIZE(laguna_pm358_i2c_ina230_board_info));
	return 0;
}

