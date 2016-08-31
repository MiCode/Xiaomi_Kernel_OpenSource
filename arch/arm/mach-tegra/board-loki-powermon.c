/*
 * arch/arm/mach-tegra/board-loki-powermon.c
 *
 * Copyright (c) 2013, NVIDIA Corporation. All Rights Reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/i2c.h>
#include <linux/ina3221.h>

#include "board.h"
#include "board-loki.h"
#include "tegra-board-id.h"

enum {
	VDD_CPU_BAT_VBUS,
	VDD_DDR_SOC_GPU,
	VDD_3V3_EMMC_MDM,
	VDD_BAT_VBUS_MODEM,
};

static struct ina3221_platform_data power_mon_info[] = {
	[VDD_CPU_BAT_VBUS] = {
		.rail_name = {"VDD_CPU", "VDD_BAT",
							"VDD_VBUS"},
		.shunt_resistor = {5, 5, 20},
		.cont_conf_data = INA3221_CONT_CONFIG_DATA,
		.trig_conf_data = INA3221_TRIG_CONFIG_DATA,
		.warn_conf_limits = {-1, -1, -1},
		.crit_conf_limits = {-1, 6800, -1},
	},
	[VDD_DDR_SOC_GPU] = {
		.rail_name = {"VDD_DDR", "VDD_SOC",
							"VDD_GPU"},
		.shunt_resistor = {30, 30, 10},
		.cont_conf_data = INA3221_CONT_CONFIG_DATA,
		.trig_conf_data = INA3221_TRIG_CONFIG_DATA,
		.warn_conf_limits = {-1, -1, -1},
		.crit_conf_limits = {-1, -1, -1},
	},
	[VDD_3V3_EMMC_MDM] = {
		.rail_name = {"VDD_3V3_SYS", "VDD_EMMC",
							"VDD_MDM"},
		.shunt_resistor = {30, 30, 20},
		.cont_conf_data = INA3221_CONT_CONFIG_DATA,
		.trig_conf_data = INA3221_TRIG_CONFIG_DATA,
		.warn_conf_limits = {-1, -1, -1},
		.crit_conf_limits = {-1, -1, -1},
	},
	[VDD_BAT_VBUS_MODEM] = {
		.rail_name = {"VDD_BATT", "VDD_VBUS",
							"VDD_MODEM"},
		.shunt_resistor = {10, 10, 10},
		.cont_conf_data = INA3221_CONT_CONFIG_DATA,
		.trig_conf_data = INA3221_TRIG_CONFIG_DATA,
		.warn_conf_limits = {-1, -1, -1},
		.crit_conf_limits = {6800, -1, -1},
	},
};

enum {
	INA_I2C_ADDR_40,
	INA_I2C_ADDR_41,
	INA_I2C_ADDR_42,
};

static struct i2c_board_info loki_i2c1_ina3221_board_e2548_info[] = {
	[INA_I2C_ADDR_40] = {
		I2C_BOARD_INFO("ina3221", 0x40),
		.platform_data = &power_mon_info[VDD_CPU_BAT_VBUS],
		.irq = -1,
	},
	[INA_I2C_ADDR_41] = {
		I2C_BOARD_INFO("ina3221", 0x41),
		.platform_data = &power_mon_info[VDD_DDR_SOC_GPU],
		.irq = -1,
	},
	[INA_I2C_ADDR_42] = {
		I2C_BOARD_INFO("ina3221", 0x42),
		.platform_data = &power_mon_info[VDD_3V3_EMMC_MDM],
		.irq = -1,
	},
};

static struct i2c_board_info loki_i2c1_ina3221_board_e2549_info[] = {
	[INA_I2C_ADDR_40] = {
		I2C_BOARD_INFO("ina3221", 0x40),
		.platform_data = &power_mon_info[VDD_BAT_VBUS_MODEM],
		.irq = -1,
	},
};

int __init loki_pmon_init(void)
{
	struct board_info info;
	tegra_get_board_info(&info);
	if (info.board_id == BOARD_E2548) {
		pr_info("INA3221: registering for E2548\n");
		i2c_register_board_info(1, loki_i2c1_ina3221_board_e2548_info,
			ARRAY_SIZE(loki_i2c1_ina3221_board_e2548_info));
	} else {
		pr_info("INA3221: registering for E2549/P2530\n");
		i2c_register_board_info(1, loki_i2c1_ina3221_board_e2549_info,
			ARRAY_SIZE(loki_i2c1_ina3221_board_e2549_info));
	}
	return 0;
}
