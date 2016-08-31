/*
 * arch/arm/mach-tegra/board-macallan-powermon.c
 *
 * Copyright (c) 2013, NVIDIA Corporation. All Rights Reserved.
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
#include <linux/platform_data/ina230.h>

#include "board.h"
#include "board-macallan.h"

#define PRECISION_MULTIPLIER_MACALLAN 1000

#define AVG_SAMPLES (2 << 9) /* 16 samples */

/* AVG is specified from platform data */
#define INA230_CONT_CONFIG	(AVG_SAMPLES | INA230_VBUS_CT | \
				INA230_VSH_CT | INA230_CONT_MODE)
#define INA230_TRIG_CONFIG	(AVG_SAMPLES | INA230_VBUS_CT | \
				 INA230_VSH_CT | INA230_TRIG_MODE)

enum {
	VD_CPU,
	VD_SOC,
	VS_DDR0,
	VS_DDR1,
	VS_LCD_BL,
	VD_LCD_HV,
	VS_SYS_1V8,
	VD_AP_1V8,
	VD_AP_RTC,
	VS_AUD_SYS,
	VD_DDR0,
	VD_DDR1,
	VD_AP_VBUS,
	VS_SYS_2V9,
	VA_PLLX,
	VA_AP_1V2,
};

static struct ina219_platform_data power_mon_ina219_info[] = {
	[VD_CPU] = {
		.calibration_data  = 0x7CD2,
		.power_lsb = 2.563685298 * PRECISION_MULTIPLIER_MACALLAN,
		.rail_name = "VD_CPU",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_MACALLAN,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 1,
	},

	[VD_SOC] = {
		.calibration_data  = 0x7CD2,
		.power_lsb = 2.563685298 * PRECISION_MULTIPLIER_MACALLAN,
		.rail_name = "VD_SOC",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_MACALLAN,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 1,
	},

	[VS_DDR0] = {
		.calibration_data  = 0xfffe,
		.power_lsb = 1.2500381481 * PRECISION_MULTIPLIER_MACALLAN,
		.rail_name = "VS_DDR0",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_MACALLAN,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 10,
	},

	[VS_DDR1] = {
		.calibration_data  = 0xfffe,
		.power_lsb = 1.2500381481 * PRECISION_MULTIPLIER_MACALLAN,
		.rail_name = "VS_DDR1",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_MACALLAN,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 10,
	},

	[VS_LCD_BL] = {
		.calibration_data  = 0xfffe,
		.power_lsb = 1.2500381481 * PRECISION_MULTIPLIER_MACALLAN,
		.rail_name = "VS_LCD_BL",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_MACALLAN,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 10,
	},

	[VD_LCD_HV] = {
		.calibration_data  = 0xfffe,
		.power_lsb = 1.2500381481 * PRECISION_MULTIPLIER_MACALLAN,
		.rail_name = "VD_LCD_HV",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_MACALLAN,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 10,
	},

	[VS_SYS_1V8] = {
		.calibration_data  = 0x7CD2,
		.power_lsb = 2.563685298 * PRECISION_MULTIPLIER_MACALLAN,
		.rail_name = "VS_SYS_1V8",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_MACALLAN,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 10,
	},

	[VD_AP_1V8] = {
		.calibration_data  = 0xfffe,
		.power_lsb = 1.2500381481 * PRECISION_MULTIPLIER_MACALLAN,
		.rail_name = "VD_AP_1V8",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_MACALLAN,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 10,
	},

	[VD_AP_RTC] = {
		.calibration_data  = 0xfffe,
		.power_lsb = 1.2500381481 * PRECISION_MULTIPLIER_MACALLAN,
		.rail_name = "VD_AP_RTC",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_MACALLAN,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 10,
	},

	[VS_AUD_SYS] = {
		.calibration_data  = 0xfffe,
		.power_lsb = 1.2500381481 * PRECISION_MULTIPLIER_MACALLAN,
		.rail_name = "VS_AUD_SYS",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_MACALLAN,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 10,
	},

	[VD_DDR0] = {
		.calibration_data  = 0xaec0,
		.power_lsb = 1.8311874106 * PRECISION_MULTIPLIER_MACALLAN,
		.rail_name = "VD_DDR0",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_MACALLAN,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 10,
	},

	[VD_DDR1] = {
		.calibration_data  = 0xaec0,
		.power_lsb = 1.8311874106 * PRECISION_MULTIPLIER_MACALLAN,
		.rail_name = "VD_DDR1",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_MACALLAN,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 10,
	},

	[VD_AP_VBUS] = {
		.calibration_data  = 0xfffe,
		.power_lsb = 1.2500381481 * PRECISION_MULTIPLIER_MACALLAN,
		.rail_name = "VD_AP_VBUS",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_MACALLAN,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 200,
	},

	[VS_SYS_2V9] = {
		.calibration_data  = 0xfffe,
		.power_lsb = 1.2500381481 * PRECISION_MULTIPLIER_MACALLAN,
		.rail_name = "VS_SYS_2V9",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_MACALLAN,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 10,
	},

	[VA_PLLX] = {
		.calibration_data  = 0xfffe,
		.power_lsb = 1.2500381481 * PRECISION_MULTIPLIER_MACALLAN,
		.rail_name = "VA_PLLX",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_MACALLAN,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 10,
	},

	[VA_AP_1V2] = {
		.calibration_data  = 0xfffe,
		.power_lsb = 1.2500381481 * PRECISION_MULTIPLIER_MACALLAN,
		.rail_name = "VA_AP_1V2",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_MACALLAN,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DB,
		.shunt_resistor = 10,
	},
};

enum {
	VDD_CELL
};

static struct ina230_platform_data power_mon_ina230_info[] = {
	[VDD_CELL] = {
		.calibration_data  = 0x20c4,
		.power_lsb = 3.051757813 * PRECISION_MULTIPLIER_MACALLAN,
		.rail_name = "VDD_CELL",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.resistor = 5,
		.min_cores_online = 2,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_MACALLAN,
		/* set to 5A, wait for syseng tuning */
		.current_threshold = 5000,
		.shunt_polarity_inverted = 1,
	}
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

static struct i2c_board_info macallan_i2c1_ina_board_info[] = {
	[INA_I2C_ADDR_40] = {
		I2C_BOARD_INFO("ina219", 0x40),
		.platform_data = &power_mon_ina219_info[VD_CPU],
		.irq = -1,
	},

	[INA_I2C_ADDR_41] = {
		I2C_BOARD_INFO("ina219", 0x41),
		.platform_data = &power_mon_ina219_info[VD_SOC],
		.irq = -1,
	},

	[INA_I2C_ADDR_42] = {
		I2C_BOARD_INFO("ina219", 0x42),
		.platform_data = &power_mon_ina219_info[VS_DDR0],
		.irq = -1,
	},

	[INA_I2C_ADDR_43] = {
		I2C_BOARD_INFO("ina219", 0x43),
		.platform_data = &power_mon_ina219_info[VS_DDR1],
		.irq = -1,
	},

	[INA_I2C_ADDR_44] = {
		I2C_BOARD_INFO("ina230", 0x44),
		.platform_data = &power_mon_ina230_info[VDD_CELL],
		.irq = -1,
	},

	[INA_I2C_ADDR_45] = {
		I2C_BOARD_INFO("ina219", 0x45),
		.platform_data = &power_mon_ina219_info[VD_LCD_HV],
		.irq = -1,
	},

	[INA_I2C_ADDR_46] = {
		I2C_BOARD_INFO("ina219", 0x46),
		.platform_data = &power_mon_ina219_info[VS_SYS_1V8],
		.irq = -1,
	},

	[INA_I2C_ADDR_47] = {
		I2C_BOARD_INFO("ina219", 0x47),
		.platform_data = &power_mon_ina219_info[VD_AP_1V8],
		.irq = -1,
	},

	[INA_I2C_ADDR_48] = {
		I2C_BOARD_INFO("ina219", 0x48),
		.platform_data = &power_mon_ina219_info[VD_AP_RTC],
		.irq = -1,
	},

	[INA_I2C_ADDR_49] = {
		I2C_BOARD_INFO("ina219", 0x49),
		.platform_data = &power_mon_ina219_info[VS_AUD_SYS],
		.irq = -1,
	},

	[INA_I2C_ADDR_4A] = {
		I2C_BOARD_INFO("ina219", 0x4A),
		.platform_data = &power_mon_ina219_info[VD_DDR0],
		.irq = -1,
	},

	[INA_I2C_ADDR_4B] = {
		I2C_BOARD_INFO("ina219", 0x4B),
		.platform_data = &power_mon_ina219_info[VD_DDR1],
		.irq = -1,
	},

	[INA_I2C_ADDR_4C] = {
		I2C_BOARD_INFO("ina219", 0x4C),
		.platform_data = &power_mon_ina219_info[VD_AP_VBUS],
		.irq = -1,
	},

	[INA_I2C_ADDR_4D] = {
		I2C_BOARD_INFO("ina219", 0x4D),
		.platform_data = &power_mon_ina219_info[VS_SYS_2V9],
		.irq = -1,
	},

	[INA_I2C_ADDR_4E] = {
		I2C_BOARD_INFO("ina219", 0x4E),
		.platform_data = &power_mon_ina219_info[VA_PLLX],
		.irq = -1,
	},

	[INA_I2C_ADDR_4F] = {
		I2C_BOARD_INFO("ina219", 0x4F),
		.platform_data = &power_mon_ina219_info[VA_AP_1V2],
		.irq = -1,
	},
};

int __init macallan_pmon_init(void)
{
	i2c_register_board_info(1, macallan_i2c1_ina_board_info,
		ARRAY_SIZE(macallan_i2c1_ina_board_info));

	return 0;
}


