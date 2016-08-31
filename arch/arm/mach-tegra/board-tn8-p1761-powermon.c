/*
 * arch/arm/mach-tegra/board-tn8-p1761-powermon.c
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION. All rights reserved.
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
#include <linux/ina3221.h>
#include <linux/platform_data/ina230.h>

#include "board.h"
#include "board-ardbeg.h"
#include "tegra-board-id.h"

#define PRECISION_MULTIPLIER_TN8	1000
#define VDD_SOC_SD1_REWORKED		10
#define VDD_CPU_BUCKCPU_REWORKED	10
#define VDD_1V35_SD2_REWORKED		10

#define AVG_SAMPLES (2 << 9) /* 16 samples */

/* AVG is specified from platform data */
#define INA230_CONT_CONFIG	(AVG_SAMPLES | INA230_VBUS_CT | \
				INA230_VSH_CT | INA230_CONT_MODE)
#define INA230_TRIG_CONFIG	(AVG_SAMPLES | INA230_VBUS_CT | \
				INA230_VSH_CT | INA230_TRIG_MODE)

/* rails on TN8_P1761 i2c */
enum {
	VDD_BAT_CPU_GPU,
};
/* rails on TN8_P1761-A02 i2c */
enum {
	VDD_BAT_USB_MDM,
};
enum {
	INA_I2C_ADDR_40,
};

/* Configure 140us conversion time, 512 averages */
#define P1761_CONT_CONFIG_DATA 0x7c07

static struct ina3221_platform_data tn8_p1761_power_mon_info[] = {
	[VDD_BAT_CPU_GPU] = {
		.rail_name = {"VDD_BAT", "VDD_CPU", "VDD_GPU"},
		.shunt_resistor = {1, 1, 1},
		.cont_conf_data = P1761_CONT_CONFIG_DATA,
		.trig_conf_data = INA3221_TRIG_CONFIG_DATA,
		.warn_conf_limits = {8000, -1, -1},
		.crit_conf_limits = {9000, -1, -1},
	},
};

static struct i2c_board_info tn8_p1761_i2c_ina3221_info[] = {
	[INA_I2C_ADDR_40] = {
		I2C_BOARD_INFO("ina3221", 0x40),
		.platform_data = &tn8_p1761_power_mon_info[VDD_BAT_CPU_GPU],
		.irq = -1,
	},
};

static struct ina3221_platform_data tn8_p1761_a02_power_mon_info[] = {
	[VDD_BAT_USB_MDM] = {
		.rail_name = {"VDD_BAT", "VDD_USB_5V0", "VDD_SYS_MDM"},
		.shunt_resistor = {1, 10, 1},
		.cont_conf_data = P1761_CONT_CONFIG_DATA,
		.trig_conf_data = INA3221_TRIG_CONFIG_DATA,
		.warn_conf_limits = {8000, -1, -1},
		.crit_conf_limits = {9000, -1, -1},
	},
};

static struct i2c_board_info tn8_p1761_a02_i2c_ina3221_info[] = {
	[INA_I2C_ADDR_40] = {
		I2C_BOARD_INFO("ina3221", 0x40),
		.platform_data = &tn8_p1761_a02_power_mon_info[VDD_BAT_USB_MDM],
		.irq = -1,
	},
};

/* following rails are present on E1784 A00 boards */
/* rails on i2c2_1 */
enum {
	E1784_A00_VDD_BAT_CHG,
	E1784_A00_VDD_SYS_BUCKCPU,
	E1784_A00_VDD_SYS_BUCKGPU,
	E1784_A00_VDD_SYS_BUCKSOC,
	E1784_A00_VDD_5V0_SD2,
	E1784_A00_VDD_WWAN_MDM,
	E1784_A00_VDD_SYS_BL,
	E1784_A00_VDD_3V3A_COM,
};

/* rails on i2c2_2 */
enum {
	E1784_A00_VDD_RTC_LDO3,
	E1784_A00_VDD_3V3A_LDO1_6,
	E1784_A00_VDD_DIS_3V3_LCD,
	E1784_A00_VDD_LCD_1V8B_DIS,
};

/* rails on i2c2_3 */
enum {
	E1784_A00_VDD_GPU_BUCKGPU,
	E1784_A00_VDD_SOC_SD1,
	E1784_A00_VDD_CPU_BUCKCPU,
	E1784_A00_VDD_1V8_SD5,
	E1784_A00_VDD_1V05_LDO0,
};

static struct ina230_platform_data e1784_A00_power_mon_info_1[] = {
	[E1784_A00_VDD_BAT_CHG] = {
		.calibration_data  = 0x0E90,
		.power_lsb = 6.86695279 * PRECISION_MULTIPLIER_TN8 ,
		.rail_name = "VDD_BAT_CHG",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_TN8 ,
		.resistor = 5,
	},

	[E1784_A00_VDD_SYS_BUCKCPU] = {
		.calibration_data  = 0x0F9D,
		.power_lsb = 3.202401801 * PRECISION_MULTIPLIER_TN8 ,
		.rail_name = "VDD_SYS_BUCKCPU",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_TN8 ,
		.resistor = 10,
	},

	[E1784_A00_VDD_SYS_BUCKGPU] = {
		.calibration_data  = 0x176B,
		.power_lsb = 2.135112594 * PRECISION_MULTIPLIER_TN8 ,
		.rail_name = "VDD_SYS_BUCKGPU",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_TN8 ,
		.resistor = 10,
	},

	[E1784_A00_VDD_SYS_BUCKSOC] = {
		.calibration_data  = 0x2ED7,
		.power_lsb = 1.067467267 * PRECISION_MULTIPLIER_TN8 ,
		.rail_name = "VDD_SYS_BUCKSOC",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_TN8 ,
		.resistor = 10,
	},

	[E1784_A00_VDD_5V0_SD2] = {
		.calibration_data  = 0x4BAC,
		.power_lsb = 0.660747471 * PRECISION_MULTIPLIER_TN8 ,
		.rail_name = "VDD_5V0_SD2",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_TN8 ,
		.resistor = 10,
	},

	[E1784_A00_VDD_WWAN_MDM] = {
		.calibration_data  = 0x3F39,
		.power_lsb = 1.581711461 * PRECISION_MULTIPLIER_TN8 ,
		.rail_name = "VDD_WWAN_MDM",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_TN8 ,
		.resistor = 5,
	},

	[E1784_A00_VDD_SYS_BL] = {
		.calibration_data  = 0x2D6A,
		.power_lsb = 0.36699352 * PRECISION_MULTIPLIER_TN8 ,
		.rail_name = "VDD_SYS_BL",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_TN8 ,
		.resistor = 30,
	},

	[E1784_A00_VDD_3V3A_COM] = {
		.calibration_data  = 0x0C17,
		.power_lsb = 0.413570275 * PRECISION_MULTIPLIER_TN8 ,
		.rail_name = "VDD_3V3A_COM",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_TN8 ,
		.resistor = 100,
	},
};

static struct ina230_platform_data e1784_A00_power_mon_info_2[] = {
	[E1784_A00_VDD_RTC_LDO3] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.078127384 * PRECISION_MULTIPLIER_TN8 ,
		.rail_name = "VDD_RTC_LDO3",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_TN8 ,
		.resistor = 50,
	},

	[E1784_A00_VDD_3V3A_LDO1_6] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.003906369 * PRECISION_MULTIPLIER_TN8 ,
		.rail_name = "VDD_3V3A_LDO1_6",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_TN8 ,
		.resistor = 1000,
	},

	[E1784_A00_VDD_DIS_3V3_LCD] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.390636921 * PRECISION_MULTIPLIER_TN8 ,
		.rail_name = "VDD_DIS_3V3_LCD",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_TN8 ,
		.resistor = 10,
	},

	[E1784_A00_VDD_LCD_1V8B_DIS] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.039063692 * PRECISION_MULTIPLIER_TN8 ,
		.rail_name = "VDD_LCD_1V8B_DIS",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_TN8 ,
		.resistor = 100,
	},
};

static struct ina230_platform_data e1784_A00_power_mon_info_3[] = {
	[E1784_A00_VDD_GPU_BUCKGPU] = {
		.calibration_data  = 0x51EA,
		.power_lsb = 6.103958035 * PRECISION_MULTIPLIER_TN8 ,
		.rail_name = "VDD_GPU_BUCKGPU",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_TN8 ,
		.resistor = 1,
	},

	[E1784_A00_VDD_SOC_SD1] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 3.906369213 * PRECISION_MULTIPLIER_TN8 ,
		.rail_name = "VDD_SOC_SD1",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_TN8 ,
		.resistor = 1,
	},

	[E1784_A00_VDD_CPU_BUCKCPU] = {
		.calibration_data  =  0x369C,
		.power_lsb = 9.155937053 * PRECISION_MULTIPLIER_TN8 ,
		.rail_name = "VDD_CPU_BUCKCPU",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_TN8 ,
		.resistor = 1,
	},

	[E1784_A00_VDD_1V8_SD5] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 1.302123071 * PRECISION_MULTIPLIER_TN8 ,
		.rail_name = "VDD_1V8_SD5",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_TN8 ,
		.resistor = 3,
	},

	[E1784_A00_VDD_1V05_LDO0] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.130212307 * PRECISION_MULTIPLIER_TN8 ,
		.rail_name = "VDD_1V05_LDO0",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_TN8 ,
		.resistor = 30,
	},
};

/* i2c addresses of rails present on E1784 A00*/
/* addresses on i2c2_1 */
enum {
	E1784_A00_INA_I2C_2_1_ADDR_41,
	E1784_A00_INA_I2C_2_1_ADDR_42,
	E1784_A00_INA_I2C_2_1_ADDR_43,
	E1784_A00_INA_I2C_2_1_ADDR_48,
	E1784_A00_INA_I2C_2_1_ADDR_49,
	E1784_A00_INA_I2C_2_1_ADDR_4B,
	E1784_A00_INA_I2C_2_1_ADDR_4C,
	E1784_A00_INA_I2C_2_1_ADDR_4E,
};

/* addresses on i2c2_2 */
enum {
	E1784_A00_INA_I2C_2_2_ADDR_41,
	E1784_A00_INA_I2C_2_2_ADDR_42,
	E1784_A00_INA_I2C_2_2_ADDR_43,
	E1784_A00_INA_I2C_2_2_ADDR_48,
};

/* addresses on i2c2_3 */
enum {
	E1784_A00_INA_I2C_2_3_ADDR_48,
	E1784_A00_INA_I2C_2_3_ADDR_49,
	E1784_A00_INA_I2C_2_3_ADDR_4B,
	E1784_A00_INA_I2C_2_3_ADDR_4C,
	E1784_A00_INA_I2C_2_3_ADDR_4E,
};

/* following is the i2c board info for E1784 A00 */
static struct i2c_board_info e1784_A00_i2c2_1_ina230_board_info[] = {
	[E1784_A00_INA_I2C_2_1_ADDR_41] = {
		I2C_BOARD_INFO("ina230", 0x41),
		.platform_data =
			&e1784_A00_power_mon_info_1[E1784_A00_VDD_BAT_CHG],
		.irq = -1,
	},

	[E1784_A00_INA_I2C_2_1_ADDR_42] = {
		I2C_BOARD_INFO("ina230", 0x42),
		.platform_data =
			&e1784_A00_power_mon_info_1[E1784_A00_VDD_SYS_BUCKCPU],
		.irq = -1,
	},

	[E1784_A00_INA_I2C_2_1_ADDR_43] = {
		I2C_BOARD_INFO("ina230", 0x43),
		.platform_data =
			&e1784_A00_power_mon_info_1[E1784_A00_VDD_SYS_BUCKGPU],
		.irq = -1,
	},

	[E1784_A00_INA_I2C_2_1_ADDR_48] = {
		I2C_BOARD_INFO("ina230", 0x48),
		.platform_data =
			&e1784_A00_power_mon_info_1[E1784_A00_VDD_SYS_BUCKSOC],
		.irq = -1,
	},

	[E1784_A00_INA_I2C_2_1_ADDR_49] = {
		I2C_BOARD_INFO("ina230", 0x49),
		.platform_data =
			&e1784_A00_power_mon_info_1[E1784_A00_VDD_5V0_SD2],
		.irq = -1,
	},

	[E1784_A00_INA_I2C_2_1_ADDR_4B] = {
		I2C_BOARD_INFO("ina230", 0x4B),
		.platform_data =
			&e1784_A00_power_mon_info_1[E1784_A00_VDD_WWAN_MDM],
		.irq = -1,
	},

	[E1784_A00_INA_I2C_2_1_ADDR_4C] = {
		I2C_BOARD_INFO("ina230", 0x4C),
		.platform_data =
			&e1784_A00_power_mon_info_1[E1784_A00_VDD_SYS_BL],
		.irq = -1,
	},

	[E1784_A00_INA_I2C_2_1_ADDR_4E] = {
		I2C_BOARD_INFO("ina230", 0x4E),
		.platform_data =
			&e1784_A00_power_mon_info_1[E1784_A00_VDD_3V3A_COM],
		.irq = -1,
	},
};

static struct i2c_board_info e1784_A00_i2c2_2_ina230_board_info[] = {
	[E1784_A00_INA_I2C_2_2_ADDR_41] = {
		I2C_BOARD_INFO("ina230", 0x41),
		.platform_data =
			&e1784_A00_power_mon_info_2[E1784_A00_VDD_RTC_LDO3],
		.irq = -1,
	},

	[E1784_A00_INA_I2C_2_2_ADDR_42] = {
		I2C_BOARD_INFO("ina230", 0x42),
		.platform_data =
			&e1784_A00_power_mon_info_2[E1784_A00_VDD_3V3A_LDO1_6],
		.irq = -1,
	},

	[E1784_A00_INA_I2C_2_2_ADDR_43] = {
		I2C_BOARD_INFO("ina230", 0x43),
		.platform_data =
			&e1784_A00_power_mon_info_2[E1784_A00_VDD_DIS_3V3_LCD],
		.irq = -1,
	},

	[E1784_A00_INA_I2C_2_2_ADDR_48] = {
		I2C_BOARD_INFO("ina230", 0x48),
		.platform_data =
			&e1784_A00_power_mon_info_2[E1784_A00_VDD_LCD_1V8B_DIS],
		.irq = -1,
	},
};

static struct i2c_board_info e1784_A00_i2c2_3_ina230_board_info[] = {
	[E1784_A00_INA_I2C_2_3_ADDR_48] = {
		I2C_BOARD_INFO("ina230", 0x48),
		.platform_data =
			&e1784_A00_power_mon_info_3[E1784_A00_VDD_GPU_BUCKGPU],
		.irq = -1,
	},

	[E1784_A00_INA_I2C_2_3_ADDR_49] = {
		I2C_BOARD_INFO("ina230", 0x49),
		.platform_data =
			&e1784_A00_power_mon_info_3[E1784_A00_VDD_SOC_SD1],
		.irq = -1,
	},

	[E1784_A00_INA_I2C_2_3_ADDR_4B] = {
		I2C_BOARD_INFO("ina230", 0x4B),
		.platform_data =
			&e1784_A00_power_mon_info_3[E1784_A00_VDD_CPU_BUCKCPU],
		.irq = -1,
	},

	[E1784_A00_INA_I2C_2_3_ADDR_4C] = {
		I2C_BOARD_INFO("ina230", 0x4C),
		.platform_data =
			&e1784_A00_power_mon_info_3[E1784_A00_VDD_1V8_SD5],
		.irq = -1,
	},

	[E1784_A00_INA_I2C_2_3_ADDR_4E] = {
		I2C_BOARD_INFO("ina230", 0x4E),
		.platform_data =
			&e1784_A00_power_mon_info_3[E1784_A00_VDD_1V05_LDO0],
		.irq = -1,
	},
};

static void __init register_devices_e1784_a00(void)
{
	i2c_register_board_info(PCA954x_I2C_BUS0,
			tn8_p1761_i2c_ina3221_info,
			ARRAY_SIZE(tn8_p1761_i2c_ina3221_info));

	i2c_register_board_info(PCA954x_I2C_BUS1,
			e1784_A00_i2c2_1_ina230_board_info,
			ARRAY_SIZE(e1784_A00_i2c2_1_ina230_board_info));

	i2c_register_board_info(PCA954x_I2C_BUS2,
			e1784_A00_i2c2_2_ina230_board_info,
			ARRAY_SIZE(e1784_A00_i2c2_2_ina230_board_info));

	i2c_register_board_info(PCA954x_I2C_BUS3,
			e1784_A00_i2c2_3_ina230_board_info,
			ARRAY_SIZE(e1784_A00_i2c2_3_ina230_board_info));
}

int __init tn8_p1761_pmon_init(void)
{
	struct board_info bi;
	int ret;
	tegra_get_board_info(&bi);

	if (bi.board_id == BOARD_E1784)
		register_devices_e1784_a00();

	else if (bi.fab >= BOARD_FAB_A02)
		ret = i2c_register_board_info(1, tn8_p1761_a02_i2c_ina3221_info,
					      ARRAY_SIZE(tn8_p1761_a02_i2c_ina3221_info));
	else
		ret = i2c_register_board_info(1, tn8_p1761_i2c_ina3221_info,
					      ARRAY_SIZE(tn8_p1761_i2c_ina3221_info));

	return ret;
}
