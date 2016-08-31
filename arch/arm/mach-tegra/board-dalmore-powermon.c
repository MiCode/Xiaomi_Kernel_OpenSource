/*
 * arch/arm/mach-tegra/board-dalmore-powermon.c
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION. All Rights Reserved.
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

#include <linux/i2c.h>
#include <linux/ina219.h>

#include "board.h"
#include "board-dalmore.h"

#define PRECISION_MULTIPLIER_DALMORE 1000

enum {
	VDD_12V_DCIN_RS,
	VDD_AC_BAT_VIN1,
	VDD_5V0_SYS,
	VDD_3V3_SYS,
	VDD_3V3_SYS_VIN4_5_7,
	AVDD_USB_HDMI,
	VDD_AC_BAT_D1,
	VDD_AO_SMPS12_IN,
	VDD_3V3_SYS_SMPS45_IN,
	VDD_AO_SMPS2_IN,
	VDDIO_HV_AP,
	VDD_1V8_LDO3_IN,
	VDD_3V3_SYS_LDO4_IN,
	VDD_AO_LDO8_IN,
	VDD_1V8_AP,
	VDD_1V8_DSM,
};

static struct ina219_platform_data power_mon_info[] = {
	[VDD_12V_DCIN_RS] = {
		.calibration_data  = 0xaec0,
		.power_lsb = 1.8311874106 * PRECISION_MULTIPLIER_DALMORE,
		.rail_name = "VDD_12V_DCIN_RS",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_DALMORE,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 10,
	},

	[VDD_AC_BAT_VIN1] = {
		.calibration_data  = 0xfffe,
		.power_lsb = 1.2500381481 * PRECISION_MULTIPLIER_DALMORE,
		.rail_name = "VDD_AC_BAT_VIN1",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_DALMORE,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 10,
	},

	[VDD_5V0_SYS] = {
		.calibration_data  = 0xfffe,
		.power_lsb = 2.5000762963 * PRECISION_MULTIPLIER_DALMORE,
		.rail_name = "VDD_5V0_SYS",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_DALMORE,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 5,
	},

	[VDD_3V3_SYS] = {
		.calibration_data  = 0xfffe,
		.power_lsb = 2.5000762963 * PRECISION_MULTIPLIER_DALMORE,
		.rail_name = "VDD_3V3_SYS",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_DALMORE,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 5,
	},

	[VDD_3V3_SYS_VIN4_5_7] = {
		.calibration_data  = 0xfffe,
		.power_lsb = 1.2500381481 * PRECISION_MULTIPLIER_DALMORE,
		.rail_name = "VDD_3V3_SYS_VIN4_5_7",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_DALMORE,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 10,
	},

	[AVDD_USB_HDMI] = {
		.calibration_data  = 0xfffe,
		.power_lsb = 1.2500381481 * PRECISION_MULTIPLIER_DALMORE,
		.rail_name = "AVDD_USB_HDMI",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_DALMORE,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 10,
	},

	[VDD_AC_BAT_D1] = {
		.calibration_data  = 0x7CD2,
		.power_lsb = 2.563685298 * PRECISION_MULTIPLIER_DALMORE,
		.rail_name = "VDD_AC_BAT_D1",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_DALMORE,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 10,
	},

	[VDD_AO_SMPS12_IN] = {
		.calibration_data  = 0xaec0,
		.power_lsb = 1.8311874106 * PRECISION_MULTIPLIER_DALMORE,
		.rail_name = "VDD_AO_SMPS12_IN",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_DALMORE,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 10,
	},

	[VDD_3V3_SYS_SMPS45_IN] = {
		.calibration_data  = 0xfffe,
		.power_lsb = 1.2500381481 * PRECISION_MULTIPLIER_DALMORE,
		.rail_name = "VDD_3V3_SYS_SMPS45_IN",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_DALMORE,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 10,
	},

	[VDD_AO_SMPS2_IN] = {
		.calibration_data  = 0xfffe,
		.power_lsb = 1.2500381481 * PRECISION_MULTIPLIER_DALMORE,
		.rail_name = "VDD_AO_SMPS2_IN",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_DALMORE,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 10,
	},

	[VDDIO_HV_AP] = {
		.calibration_data  = 0xfffe,
		.power_lsb = 1.2500381481 * PRECISION_MULTIPLIER_DALMORE,
		.rail_name = "VDDIO_HV_AP",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_DALMORE,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 10,
	},

	[VDD_1V8_LDO3_IN] = {
		.calibration_data  = 0xfffe,
		.power_lsb = 1.2500381481 * PRECISION_MULTIPLIER_DALMORE,
		.rail_name = "VDD_1V8_LDO3_IN",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_DALMORE,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 10,
	},

	[VDD_3V3_SYS_LDO4_IN] = {
		.calibration_data  = 0xfffe,
		.power_lsb = 1.2500381481 * PRECISION_MULTIPLIER_DALMORE,
		.rail_name = "VDD_3V3_SYS_LDO4_IN",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_DALMORE,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 10,
	},

	[VDD_AO_LDO8_IN] = {
		.calibration_data  = 0xfffe,
		.power_lsb = 1.2500381481 * PRECISION_MULTIPLIER_DALMORE,
		.rail_name = "VDD_AO_LDO8_IN",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_DALMORE,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 10,
	},

	[VDD_1V8_AP] = {
		.calibration_data  = 0xfffe,
		.power_lsb = 1.2500381481 * PRECISION_MULTIPLIER_DALMORE,
		.rail_name = "VDD_1V8_AP",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_DALMORE,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 10,
	},

	[VDD_1V8_DSM] = {
		.calibration_data  = 0xfffe,
		.power_lsb = 1.2500381481 * PRECISION_MULTIPLIER_DALMORE,
		.rail_name = "VDD_1V8_DSM",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_DALMORE,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 10,
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
	INA_I2C_ADDR_4C,
	INA_I2C_ADDR_4D,
	INA_I2C_ADDR_4E,
	INA_I2C_ADDR_4F,
};

static struct i2c_board_info dalmore_i2c0_ina219_board_info[] = {
	[INA_I2C_ADDR_40] = {
		I2C_BOARD_INFO("ina219", 0x40),
		.platform_data = &power_mon_info[VDD_12V_DCIN_RS],
		.irq = -1,
	},

	[INA_I2C_ADDR_41] = {
		I2C_BOARD_INFO("ina219", 0x41),
		.platform_data = &power_mon_info[VDD_AC_BAT_VIN1],
		.irq = -1,
	},

	[INA_I2C_ADDR_42] = {
		I2C_BOARD_INFO("ina219", 0x42),
		.platform_data = &power_mon_info[VDD_5V0_SYS],
		.irq = -1,
	},

	[INA_I2C_ADDR_43] = {
		I2C_BOARD_INFO("ina219", 0x43),
		.platform_data = &power_mon_info[VDD_3V3_SYS],
		.irq = -1,
	},

	[INA_I2C_ADDR_44] = {
		I2C_BOARD_INFO("ina219", 0x44),
		.platform_data = &power_mon_info[VDD_3V3_SYS_VIN4_5_7],
		.irq = -1,
	},

	[INA_I2C_ADDR_45] = {
		I2C_BOARD_INFO("ina219", 0x45),
		.platform_data = &power_mon_info[AVDD_USB_HDMI],
		.irq = -1,
	},

	[INA_I2C_ADDR_46] = {
		I2C_BOARD_INFO("ina219", 0x46),
		.platform_data = &power_mon_info[VDD_AC_BAT_D1],
		.irq = -1,
	},

	[INA_I2C_ADDR_47] = {
		I2C_BOARD_INFO("ina219", 0x47),
		.platform_data = &power_mon_info[VDD_AO_SMPS12_IN],
		.irq = -1,
	},

	[INA_I2C_ADDR_48] = {
		I2C_BOARD_INFO("ina219", 0x48),
		.platform_data = &power_mon_info[VDD_3V3_SYS_SMPS45_IN],
		.irq = -1,
	},

	[INA_I2C_ADDR_49] = {
		I2C_BOARD_INFO("ina219", 0x49),
		.platform_data = &power_mon_info[VDD_AO_SMPS2_IN],
		.irq = -1,
	},

	[INA_I2C_ADDR_4A] = {
		I2C_BOARD_INFO("ina219", 0x4A),
		.platform_data = &power_mon_info[VDDIO_HV_AP],
		.irq = -1,
	},

	[INA_I2C_ADDR_4B] = {
		I2C_BOARD_INFO("ina219", 0x4B),
		.platform_data = &power_mon_info[VDD_1V8_LDO3_IN],
		.irq = -1,
	},

	[INA_I2C_ADDR_4C] = {
		I2C_BOARD_INFO("ina219", 0x4C),
		.platform_data = &power_mon_info[VDD_3V3_SYS_LDO4_IN],
		.irq = -1,
	},

	[INA_I2C_ADDR_4D] = {
		I2C_BOARD_INFO("ina219", 0x4D),
		.platform_data = &power_mon_info[VDD_AO_LDO8_IN],
		.irq = -1,
	},

	[INA_I2C_ADDR_4E] = {
		I2C_BOARD_INFO("ina219", 0x4E),
		.platform_data = &power_mon_info[VDD_1V8_AP],
		.irq = -1,
	},

	[INA_I2C_ADDR_4F] = {
		I2C_BOARD_INFO("ina219", 0x4F),
		.platform_data = &power_mon_info[VDD_1V8_DSM],
		.irq = -1,
	},
};

int __init dalmore_pmon_init(void)
{
	i2c_register_board_info(1, dalmore_i2c0_ina219_board_info,
		ARRAY_SIZE(dalmore_i2c0_ina219_board_info));

	return 0;
}

