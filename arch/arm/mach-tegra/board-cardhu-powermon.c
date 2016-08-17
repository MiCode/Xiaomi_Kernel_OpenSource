/*
 * arch/arm/mach-tegra/board-cardhu-powermon.c
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
#include "board-cardhu.h"

enum {
	VDD_AC_BAT,
	VDD_DRAM_IN,
	VDD_BACKLIGHT_IN,
	VDD_CPU_IN,
	VDD_CORE_IN,
	VDD_DISPLAY_IN,
	VDD_3V3_TEGRA,
	VDD_OTHER_PMU_IN,
	VDD_1V8_TEGRA,
	VDD_1V8_OTHER,
	UNUSED_RAIL,
};

static struct ina219_platform_data power_mon_info[] = {
	[VDD_AC_BAT] = {
		.calibration_data  = 0xa000,
		.power_lsb = 2,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DF,
		.rail_name = "VDD_AC_BAT",
		.divisor = 20,
		.shunt_resistor = 10, /*mOhms*/
	},

	[VDD_DRAM_IN] = {
		.calibration_data  = 0xa000,
		.power_lsb = 2,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DF,
		.rail_name = "VDD_DRAM_IN",
		.divisor = 20,
		.shunt_resistor = 10,
	},

	[VDD_BACKLIGHT_IN] = {
		.calibration_data  = 0x6aaa,
		.power_lsb = 1,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DF,
		.rail_name = "VDD_BACKLIGHT_IN",
		.divisor = 20,
		.shunt_resistor = 30,
	},

	[VDD_CPU_IN] = {
		.calibration_data  = 0xa000,
		.power_lsb = 1,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DF,
		.rail_name = "VDD_CPU_IN",
		.divisor = 20,
		.shunt_resistor = 20,
	},

	[VDD_CORE_IN] = {
		.calibration_data  = 0x6aaa,
		.power_lsb = 1,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DF,
		.rail_name = "VDD_CORE_IN",
		.divisor = 20,
		.shunt_resistor = 30,
	},

	[VDD_DISPLAY_IN] = {
		.calibration_data  = 0x4000,
		.power_lsb = 1,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DF,
		.rail_name = "VDD_DISPLAY_IN",
		.divisor = 20,
		.shunt_resistor = 50,
	},

	[VDD_3V3_TEGRA] = {
		.calibration_data  = 0x6aaa,
		.power_lsb = 1,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DF,
		.rail_name = "VDD_3V3_TEGRA",
		.divisor = 20,
		.shunt_resistor = 30,
	},

	[VDD_OTHER_PMU_IN] = {
		.calibration_data  = 0xa000,
		.power_lsb = 1,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DF,
		.rail_name = "VDD_OTHER_PMU_IN",
		.divisor = 20,
		.shunt_resistor = 20,
	},

	[VDD_1V8_TEGRA] = {
		.calibration_data  = 0x4000,
		.power_lsb = 1,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DF,
		.rail_name = "VDD_1V8_TEGRA",
		.divisor = 20,
		.shunt_resistor = 50,
	},

	[VDD_1V8_OTHER] = {
		.calibration_data  = 0xa000,
		.power_lsb = 1,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DF,
		.rail_name = "VDD_1V8_OTHER",
		.divisor = 20,
		.shunt_resistor = 10,
	},

	/* All unused INA219 devices use below data*/
	[UNUSED_RAIL] = {
		.calibration_data  = 0x4000,
		.power_lsb = 1,
		.cont_conf = 0x3FFF,
		.trig_conf = 0x1DF,
		.rail_name = "unused_rail",
		.divisor = 20,
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

static struct i2c_board_info cardhu_i2c0_ina219_board_info[] = {
	[INA_I2C_ADDR_40] = {
		I2C_BOARD_INFO("ina219", 0x40),
		.platform_data = &power_mon_info[VDD_AC_BAT],
		.irq = -1,
	},

	[INA_I2C_ADDR_41] = {
		I2C_BOARD_INFO("ina219", 0x41),
		.platform_data = &power_mon_info[VDD_DRAM_IN],
		.irq = -1,
	},

	[INA_I2C_ADDR_42] = {
		I2C_BOARD_INFO("ina219", 0x42),
		.platform_data = &power_mon_info[VDD_BACKLIGHT_IN],
		.irq = -1,
	},

	[INA_I2C_ADDR_43] = {
		I2C_BOARD_INFO("ina219", 0x43),
		.platform_data = &power_mon_info[VDD_CPU_IN],
		.irq = -1,
	},

	[INA_I2C_ADDR_44] = {
		I2C_BOARD_INFO("ina219", 0x44),
		.platform_data = &power_mon_info[VDD_CORE_IN],
		.irq = -1,
	},

	[INA_I2C_ADDR_45] = {
		I2C_BOARD_INFO("ina219", 0x45),
		.platform_data = &power_mon_info[VDD_DISPLAY_IN],
		.irq = -1,
	},

	[INA_I2C_ADDR_46] = {
		I2C_BOARD_INFO("ina219", 0x46),
		.platform_data = &power_mon_info[VDD_3V3_TEGRA],
		.irq = -1,
	},

	[INA_I2C_ADDR_47] = {
		I2C_BOARD_INFO("ina219", 0x47),
		.platform_data = &power_mon_info[VDD_OTHER_PMU_IN],
		.irq = -1,
	},

	[INA_I2C_ADDR_48] = {
		I2C_BOARD_INFO("ina219", 0x48),
		.platform_data = &power_mon_info[VDD_1V8_TEGRA],
		.irq = -1,
	},

	[INA_I2C_ADDR_49] = {
		I2C_BOARD_INFO("ina219", 0x49),
		.platform_data = &power_mon_info[VDD_1V8_OTHER],
		.irq = -1,
	},

	[INA_I2C_ADDR_4A] = {
		I2C_BOARD_INFO("ina219", 0x4A),
		.platform_data = &power_mon_info[UNUSED_RAIL],
		.irq = -1,
	},

	[INA_I2C_ADDR_4B] = {
		I2C_BOARD_INFO("ina219", 0x4B),
		.platform_data = &power_mon_info[UNUSED_RAIL],
		.irq = -1,
	},

	[INA_I2C_ADDR_4C] = {
		I2C_BOARD_INFO("ina219", 0x4C),
		.platform_data = &power_mon_info[UNUSED_RAIL],
		.irq = -1,
	},

	[INA_I2C_ADDR_4D] = {
		I2C_BOARD_INFO("ina219", 0x4D),
		.platform_data = &power_mon_info[UNUSED_RAIL],
		.irq = -1,
	},

	[INA_I2C_ADDR_4E] = {
		I2C_BOARD_INFO("ina219", 0x4E),
		.platform_data = &power_mon_info[UNUSED_RAIL],
		.irq = -1,
	},

	[INA_I2C_ADDR_4F] = {
		I2C_BOARD_INFO("ina219", 0x4F),
		.platform_data = &power_mon_info[UNUSED_RAIL],
		.irq = -1,
	},
};

int __init cardhu_pmon_init(void)
{
	struct board_info bi;

	tegra_get_board_info(&bi);

	/* for fab A04 VDD_CORE_IN changed from ina with addr 0x44 to 0x4A */
	if (bi.fab == BOARD_FAB_A04) {
		cardhu_i2c0_ina219_board_info[INA_I2C_ADDR_44].platform_data =
			&power_mon_info[UNUSED_RAIL];
		cardhu_i2c0_ina219_board_info[INA_I2C_ADDR_4A].platform_data =
			&power_mon_info[VDD_CORE_IN];
	}

	if (bi.board_id != BOARD_PM269 && bi.board_id != BOARD_PM315) {
		i2c_register_board_info(0, cardhu_i2c0_ina219_board_info,
			ARRAY_SIZE(cardhu_i2c0_ina219_board_info));
	}

	return 0;
}

