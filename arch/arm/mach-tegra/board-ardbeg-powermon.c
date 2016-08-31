/*
 * arch/arm/mach-tegra/board-ardbeg-powermon.c
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
#include "tegra-board-id.h"

#define PRECISION_MULTIPLIER_ARDBEG	1000
#define ARDBEG_POWER_REWORKED_CONFIG	0x10
#define VDD_SOC_SD1_REWORKED		10
#define VDD_CPU_BUCKCPU_REWORKED	10
#define VDD_1V35_SD2_REWORKED		10

#define AVG_SAMPLES (2 << 9) /* 16 samples */

/* AVG is specified from platform data */
#define INA230_CONT_CONFIG	(AVG_SAMPLES | INA230_VBUS_CT | \
				INA230_VSH_CT | INA230_CONT_MODE)
#define INA230_TRIG_CONFIG	(AVG_SAMPLES | INA230_VBUS_CT | \
				INA230_VSH_CT | INA230_TRIG_MODE)

/* rails on i2c2_0 */
enum {
	VDD_BAT_0,
	VDD_SYS_BUCKCPU_0,
	VDD_SYS_BUCKSOC_0,
	VDD_SYS_BUCKGPU_0,
};

/* following rails are present on Ardbeg */
/* rails on i2c2_1 */
enum {
	VDD_SYS_BAT,
	VDD_RTC_LDO5,
	VDD_3V3A_SMPS1_2,
	VDD_SOC_SMPS1_2,
	VDD_SYS_BUCKCPU,
	VDD_CPU_BUCKCPU,
	VDD_1V8A_SMPS3,
	VDD_1V8B_SMPS9,
	VDD_GPU_BUCKGPU,
	VDD_1V35_SMPS6,
	VDD_3V3A_SMPS1_2_2,
	VDD_3V3B_SMPS9,
	VDD_LCD_1V8B_DIS,
	VDD_1V05_SMPS8,
};

/* rails on i2c2_2 */
enum {
	VDD_SYS_BL,
	AVDD_1V05_LDO2,
};

/* following rails are present on Ardbeg A01 and onward boards */
/* rails on i2c2_1 */
enum {
	ARDBEG_A01_VDD_SYS_BAT,
	ARDBEG_A01_VDD_RTC_LDO3,
	ARDBEG_A01_VDD_SYS_BUCKSOC,
	ARDBEG_A01_VDD_SOC_SD1,
	ARDBEG_A01_VDD_SYS_BUCKCPU,
	ARDBEG_A01_VDD_CPU_BUCKCPU,
	ARDBEG_A01_VDD_1V8_SD5,
	ARDBEG_A01_VDD_3V3A_LDO1_6,
	ARDBEG_A01_VDD_DIS_3V3_LCD,
	ARDBEG_A01_VDD_1V35_SD2,
	ARDBEG_A01_VDD_SYS_BUCKGPU,
	ARDBEG_A01_VDD_LCD_1V8B_DIS,
	ARDBEG_A01_VDD_1V05_LDO0,
};

/* rails on i2c2_2 */
enum {
	ARDBEG_A01_VDD_1V05_SD4,
	ARDBEG_A01_VDD_1V8A_LDO2_5_7,
	ARDBEG_A01_VDD_SYS_BL,
};

static struct ina230_platform_data power_mon_info_0[] = {
	/* E1780-A02 (Shield ERS) */
	[VDD_BAT_0] = {
		.calibration_data = 0x1366,
		.power_lsb = 2.577527185 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "__VDD_BAT",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
		.resistor = 10,
	},
	[VDD_SYS_BUCKCPU_0] = {
		.calibration_data = 0x1AC5,
		.power_lsb = 1.867795126 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "__VDD_SYS_BUCKCPU",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
		.resistor = 10,
	},
	[VDD_SYS_BUCKSOC_0] = {
		.calibration_data = 0x2802,
		.power_lsb = 0.624877954 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "__VDD_SYS_BUCKSOC",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
		.resistor = 20,
	},
	[VDD_SYS_BUCKGPU_0] = {
		.calibration_data = 0x1F38,
		.power_lsb = 1.601601602 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "__VDD_SYS_BUCKGPU",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
		.resistor = 10,
	},
};

/* following are power monitor parameters for Ardbeg */
static struct ina230_platform_data power_mon_info_1[] = {
	[VDD_SYS_BAT] = {
		.calibration_data  = 0x1366,
		.power_lsb = 2.577527185 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_SYS_BAT",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
	},

	[VDD_RTC_LDO5] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.078127384 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_RTC_LDO5",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
	},

	[VDD_3V3A_SMPS1_2] = {
		.calibration_data  = 0x4759,
		.power_lsb = 1.401587736 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_3V3A_SMPS1_2",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
	},

	[VDD_SOC_SMPS1_2] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 3.906369213 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_SOC_SMPS1_2",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
	},

	[VDD_SYS_BUCKCPU] = {
		.calibration_data  = 0x1AC5,
		.power_lsb = 1.867795126 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_SYS_BUCKCPU",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
	},

	[VDD_CPU_BUCKCPU] = {
		.calibration_data  = 0x2ECF,
		.power_lsb = 10.68179922 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_CPU_BUCKCPU",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
	},

	[VDD_1V8A_SMPS3] = {
		.calibration_data  = 0x5BA7,
		.power_lsb = 0.545539786 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_1V8A_SMPS3",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
	},

	[VDD_1V8B_SMPS9] = {
		.calibration_data  = 0x50B4,
		.power_lsb = 0.309777348 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_1V8B_SMPS9",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
	},

	[VDD_GPU_BUCKGPU] = {
		.calibration_data  = 0x369C,
		.power_lsb = 9.155937053 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_GPU_BUCKGPU",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
	},

	[VDD_1V35_SMPS6] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 3.906369213 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_1V35_SMPS6",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
	},

	/* following rail is duplicate of VDD_3V3A_SMPS1_2 hence mark unused */
	[VDD_3V3A_SMPS1_2_2] = {
		.calibration_data  = 0x4759,
		.power_lsb = 1.401587736 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "unused_rail",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
	},

	[VDD_3V3B_SMPS9] = {
		.calibration_data  = 0x3269,
		.power_lsb = 0.198372724 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_3V3B_SMPS9",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
	},

	[VDD_LCD_1V8B_DIS] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.039063692 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_LCD_1V8B_DIS",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
	},

	[VDD_1V05_SMPS8] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.130212307 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_1V05_SMPS8",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
	},
};

static struct ina230_platform_data power_mon_info_2[] = {
	[VDD_SYS_BL] = {
		.calibration_data  = 0x1A29,
		.power_lsb = 0.63710119 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_SYS_BL",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
	},

	[AVDD_1V05_LDO2] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.390636921 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "AVDD_1V05_LDO2",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
	},
};

/* following are power monitor parameters for Ardbeg A01*/
static struct ina230_platform_data ardbeg_A01_power_mon_info_1[] = {
	[ARDBEG_A01_VDD_SYS_BAT] = {
		.calibration_data  = 0x1366,
		.power_lsb = 2.577527185 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_SYS_BAT",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
		.resistor = 10,
	},

	[ARDBEG_A01_VDD_RTC_LDO3] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.078127384 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_RTC_LDO3",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
		.resistor = 50,
	},

	[ARDBEG_A01_VDD_SYS_BUCKSOC] = {
		.calibration_data  = 0x1AAC,
		.power_lsb = 0.624877954 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_SYS_BUCKSOC",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
		.resistor = 30,
	},

	[ARDBEG_A01_VDD_SOC_SD1] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 3.906369213 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_SOC_SD1",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
		.resistor = 1,
	},

	[ARDBEG_A01_VDD_SYS_BUCKCPU] = {
		.calibration_data  = 0x1AC5,
		.power_lsb = 1.867795126 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_SYS_BUCKCPU",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
		.resistor = 10,
	},

	[ARDBEG_A01_VDD_CPU_BUCKCPU] = {
		.calibration_data  = 0x2ECF,
		.power_lsb = 10.68179922 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_CPU_BUCKCPU",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
		.resistor = 1,
	},

	[ARDBEG_A01_VDD_1V8_SD5] = {
		.calibration_data  = 0x45F0,
		.power_lsb = 0.714924039 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_1V8_SD5",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
		.resistor = 10,
	},

	[ARDBEG_A01_VDD_3V3A_LDO1_6] = {
		.calibration_data  = 0x3A83,
		.power_lsb = 0.042726484 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_3V3A_LDO1_6",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
		.resistor = 200,
	},

	[ARDBEG_A01_VDD_DIS_3V3_LCD] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.390636921 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_DIS_3V3_LCD",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
		.resistor = 10,
	},

	[ARDBEG_A01_VDD_1V35_SD2] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 3.906369213 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_1V35_SD2",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
		.resistor = 1,
	},

	[ARDBEG_A01_VDD_SYS_BUCKGPU] = {
		.calibration_data  = 0x1F38,
		.power_lsb = 1.601601602 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_SYS_BUCKGPU",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
		.resistor = 10,
	},

	[ARDBEG_A01_VDD_LCD_1V8B_DIS] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.039063692 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_LCD_1V8B_DIS",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
		.resistor = 100,
	},

	[ARDBEG_A01_VDD_1V05_LDO0] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.130212307 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_1V05_LDO0",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
		.resistor = 30,
	},
};

static struct ina230_platform_data ardbeg_A01_power_mon_info_2[] = {
	[ARDBEG_A01_VDD_1V05_SD4] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.390636921 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_1V05_SD4",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
		.resistor = 10,
	},

	[ARDBEG_A01_VDD_1V8A_LDO2_5_7] = {
		.calibration_data  = 0x5A04,
		.power_lsb = 0.277729561 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_1V8A_LDO2_5_7",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
		.resistor = 20,
	},

	[ARDBEG_A01_VDD_SYS_BL] = {
		.calibration_data  = 0x2468,
		.power_lsb = 0.274678112 * PRECISION_MULTIPLIER_ARDBEG,
		.rail_name = "VDD_SYS_BL",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_ARDBEG,
		.resistor = 50,
	},
};

/* i2c addresses of rails present on Ardbeg */
/* addresses on i2c2_0 */
enum {
	INA_I2C_2_0_ADDR_40,
	INA_I2C_2_0_ADDR_41,
	INA_I2C_2_0_ADDR_42,
	INA_I2C_2_0_ADDR_43,
};

/* addresses on i2c2_1 */
enum {
	INA_I2C_2_1_ADDR_40,
	INA_I2C_2_1_ADDR_41,
	INA_I2C_2_1_ADDR_42,
	INA_I2C_2_1_ADDR_43,
	INA_I2C_2_1_ADDR_44,
	INA_I2C_2_1_ADDR_45,
	INA_I2C_2_1_ADDR_46,
	INA_I2C_2_1_ADDR_47,
	INA_I2C_2_1_ADDR_48,
	INA_I2C_2_1_ADDR_49,
	INA_I2C_2_1_ADDR_4B,
	INA_I2C_2_1_ADDR_4C,
	INA_I2C_2_1_ADDR_4E,
	INA_I2C_2_1_ADDR_4F,
};

/* addresses on i2c2_2 */
enum {
	INA_I2C_2_2_ADDR_49,
	INA_I2C_2_2_ADDR_4C,
};

/* i2c addresses of rails present on Ardbeg A01*/
/* addresses on i2c2_1 */
enum {
	ARDBEG_A01_INA_I2C_2_1_ADDR_40,
	ARDBEG_A01_INA_I2C_2_1_ADDR_41,
	ARDBEG_A01_INA_I2C_2_1_ADDR_42,
	ARDBEG_A01_INA_I2C_2_1_ADDR_43,
	ARDBEG_A01_INA_I2C_2_1_ADDR_44,
	ARDBEG_A01_INA_I2C_2_1_ADDR_45,
	ARDBEG_A01_INA_I2C_2_1_ADDR_46,
	ARDBEG_A01_INA_I2C_2_1_ADDR_47,
	ARDBEG_A01_INA_I2C_2_1_ADDR_48,
	ARDBEG_A01_INA_I2C_2_1_ADDR_49,
	ARDBEG_A01_INA_I2C_2_1_ADDR_4B,
	ARDBEG_A01_INA_I2C_2_1_ADDR_4E,
	ARDBEG_A01_INA_I2C_2_1_ADDR_4F,
};

/* addresses on i2c2_2 */
enum {
	ARDBEG_A01_INA_I2C_2_2_ADDR_40,
	ARDBEG_A01_INA_I2C_2_2_ADDR_41,
	ARDBEG_A01_INA_I2C_2_2_ADDR_49,
};

/* following is the i2c board info for Ardbeg */
static struct i2c_board_info ardbeg_i2c2_0_ina230_board_info[] = {
	[INA_I2C_2_0_ADDR_40] = {
		I2C_BOARD_INFO("ina230", 0x40),
		.platform_data = &power_mon_info_0[VDD_BAT_0],
		.irq = -1,
	},

	[INA_I2C_2_0_ADDR_41] = {
		I2C_BOARD_INFO("ina230", 0x41),
		.platform_data = &power_mon_info_0[VDD_SYS_BUCKCPU_0],
		.irq = -1,
	},

	[INA_I2C_2_0_ADDR_42] = {
		I2C_BOARD_INFO("ina230", 0x42),
		.platform_data = &power_mon_info_0[VDD_SYS_BUCKSOC_0],
		.irq = -1,
	},

	[INA_I2C_2_0_ADDR_43] = {
		I2C_BOARD_INFO("ina230", 0x43),
		.platform_data = &power_mon_info_0[VDD_SYS_BUCKGPU_0],
		.irq = -1,
	},
};

static struct i2c_board_info ardbeg_i2c2_1_ina230_board_info[] = {
	[INA_I2C_2_1_ADDR_40] = {
		I2C_BOARD_INFO("ina230", 0x40),
		.platform_data = &power_mon_info_1[VDD_SYS_BAT],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_41] = {
		I2C_BOARD_INFO("ina230", 0x41),
		.platform_data = &power_mon_info_1[VDD_RTC_LDO5],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_42] = {
		I2C_BOARD_INFO("ina230", 0x42),
		.platform_data = &power_mon_info_1[VDD_3V3A_SMPS1_2],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_43] = {
		I2C_BOARD_INFO("ina230", 0x43),
		.platform_data = &power_mon_info_1[VDD_SOC_SMPS1_2],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_44] = {
		I2C_BOARD_INFO("ina230", 0x44),
		.platform_data = &power_mon_info_1[VDD_SYS_BUCKCPU],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_45] = {
		I2C_BOARD_INFO("ina230", 0x45),
		.platform_data = &power_mon_info_1[VDD_CPU_BUCKCPU],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_46] = {
		I2C_BOARD_INFO("ina230", 0x46),
		.platform_data = &power_mon_info_1[VDD_1V8A_SMPS3],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_47] = {
		I2C_BOARD_INFO("ina230", 0x47),
		.platform_data = &power_mon_info_1[VDD_1V8B_SMPS9],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_48] = {
		I2C_BOARD_INFO("ina230", 0x48),
		.platform_data = &power_mon_info_1[VDD_GPU_BUCKGPU],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_49] = {
		I2C_BOARD_INFO("ina230", 0x49),
		.platform_data = &power_mon_info_1[VDD_1V35_SMPS6],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_4B] = {
		I2C_BOARD_INFO("ina230", 0x4B),
		.platform_data = &power_mon_info_1[VDD_3V3A_SMPS1_2_2],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_4C] = {
		I2C_BOARD_INFO("ina230", 0x4C),
		.platform_data = &power_mon_info_1[VDD_3V3B_SMPS9],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_4E] = {
		I2C_BOARD_INFO("ina230", 0x4E),
		.platform_data = &power_mon_info_1[VDD_LCD_1V8B_DIS],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_4F] = {
		I2C_BOARD_INFO("ina230", 0x4F),
		.platform_data = &power_mon_info_1[VDD_1V05_SMPS8],
		.irq = -1,
	},
};

static struct i2c_board_info ardbeg_i2c2_2_ina230_board_info[] = {
	[INA_I2C_2_2_ADDR_49] = {
		I2C_BOARD_INFO("ina230", 0x49),
		.platform_data = &power_mon_info_2[VDD_SYS_BL],
		.irq = -1,
	},

	[INA_I2C_2_2_ADDR_4C] = {
		I2C_BOARD_INFO("ina230", 0x4C),
		.platform_data = &power_mon_info_2[AVDD_1V05_LDO2],
		.irq = -1,
	},

};

/* following is the i2c board info for Ardbeg A01 */
static struct i2c_board_info ardbeg_A01_i2c2_1_ina230_board_info[] = {
	[ARDBEG_A01_INA_I2C_2_1_ADDR_40] = {
		I2C_BOARD_INFO("ina230", 0x40),
		.platform_data =
			&ardbeg_A01_power_mon_info_1[ARDBEG_A01_VDD_SYS_BAT],
		.irq = -1,
	},

	[ARDBEG_A01_INA_I2C_2_1_ADDR_41] = {
		I2C_BOARD_INFO("ina230", 0x41),
		.platform_data =
			&ardbeg_A01_power_mon_info_1[ARDBEG_A01_VDD_RTC_LDO3],
		.irq = -1,
	},

	[ARDBEG_A01_INA_I2C_2_1_ADDR_42] = {
		I2C_BOARD_INFO("ina230", 0x42),
		.platform_data =
		&ardbeg_A01_power_mon_info_1[ARDBEG_A01_VDD_SYS_BUCKSOC],
		.irq = -1,
	},

	[ARDBEG_A01_INA_I2C_2_1_ADDR_43] = {
		I2C_BOARD_INFO("ina230", 0x43),
		.platform_data =
			&ardbeg_A01_power_mon_info_1[ARDBEG_A01_VDD_SOC_SD1],
		.irq = -1,
	},

	[ARDBEG_A01_INA_I2C_2_1_ADDR_44] = {
		I2C_BOARD_INFO("ina230", 0x44),
		.platform_data =
		&ardbeg_A01_power_mon_info_1[ARDBEG_A01_VDD_SYS_BUCKCPU],
		.irq = -1,
	},

	[ARDBEG_A01_INA_I2C_2_1_ADDR_45] = {
		I2C_BOARD_INFO("ina230", 0x45),
		.platform_data =
		&ardbeg_A01_power_mon_info_1[ARDBEG_A01_VDD_CPU_BUCKCPU],
		.irq = -1,
	},

	[ARDBEG_A01_INA_I2C_2_1_ADDR_46] = {
		I2C_BOARD_INFO("ina230", 0x46),
		.platform_data =
			&ardbeg_A01_power_mon_info_1[ARDBEG_A01_VDD_1V8_SD5],
		.irq = -1,
	},

	[ARDBEG_A01_INA_I2C_2_1_ADDR_47] = {
		I2C_BOARD_INFO("ina230", 0x47),
		.platform_data =
		&ardbeg_A01_power_mon_info_1[ARDBEG_A01_VDD_3V3A_LDO1_6],
		.irq = -1,
	},

	[ARDBEG_A01_INA_I2C_2_1_ADDR_48] = {
		I2C_BOARD_INFO("ina230", 0x48),
		.platform_data =
		&ardbeg_A01_power_mon_info_1[ARDBEG_A01_VDD_DIS_3V3_LCD],
		.irq = -1,
	},

	[ARDBEG_A01_INA_I2C_2_1_ADDR_49] = {
		I2C_BOARD_INFO("ina230", 0x49),
		.platform_data =
			&ardbeg_A01_power_mon_info_1[ARDBEG_A01_VDD_1V35_SD2],
		.irq = -1,
	},

	[ARDBEG_A01_INA_I2C_2_1_ADDR_4B] = {
		I2C_BOARD_INFO("ina230", 0x4B),
		.platform_data =
		&ardbeg_A01_power_mon_info_1[ARDBEG_A01_VDD_SYS_BUCKGPU],
		.irq = -1,
	},

	[ARDBEG_A01_INA_I2C_2_1_ADDR_4E] = {
		I2C_BOARD_INFO("ina230", 0x4E),
		.platform_data =
		&ardbeg_A01_power_mon_info_1[ARDBEG_A01_VDD_LCD_1V8B_DIS],
		.irq = -1,
	},

	[ARDBEG_A01_INA_I2C_2_1_ADDR_4F] = {
		I2C_BOARD_INFO("ina230", 0x4F),
		.platform_data =
			&ardbeg_A01_power_mon_info_1[ARDBEG_A01_VDD_1V05_LDO0],
		.irq = -1,
	},
};

static struct i2c_board_info ardbeg_A01_i2c2_2_ina230_board_info[] = {
	[ARDBEG_A01_INA_I2C_2_2_ADDR_40] = {
		I2C_BOARD_INFO("ina230", 0x40),
		.platform_data =
			&ardbeg_A01_power_mon_info_2[ARDBEG_A01_VDD_1V05_SD4],
		.irq = -1,
	},

	[ARDBEG_A01_INA_I2C_2_2_ADDR_41] = {
		I2C_BOARD_INFO("ina230", 0x41),
		.platform_data =
		&ardbeg_A01_power_mon_info_2[ARDBEG_A01_VDD_1V8A_LDO2_5_7],
		.irq = -1,
	},

	[ARDBEG_A01_INA_I2C_2_2_ADDR_49] = {
		I2C_BOARD_INFO("ina230", 0x49),
		.platform_data =
			&ardbeg_A01_power_mon_info_2[ARDBEG_A01_VDD_SYS_BL],
		.irq = -1,
	},
};

static void __init register_devices_ardbeg_A01(void)
{
	i2c_register_board_info(PCA954x_I2C_BUS1,
			ardbeg_A01_i2c2_1_ina230_board_info,
			ARRAY_SIZE(ardbeg_A01_i2c2_1_ina230_board_info));

	i2c_register_board_info(PCA954x_I2C_BUS2,
			ardbeg_A01_i2c2_2_ina230_board_info,
			ARRAY_SIZE(ardbeg_A01_i2c2_2_ina230_board_info));
}

static void __init register_devices_ardbeg(void)
{
	i2c_register_board_info(PCA954x_I2C_BUS1,
			ardbeg_i2c2_1_ina230_board_info,
			ARRAY_SIZE(ardbeg_i2c2_1_ina230_board_info));

	i2c_register_board_info(PCA954x_I2C_BUS2,
			ardbeg_i2c2_2_ina230_board_info,
			ARRAY_SIZE(ardbeg_i2c2_2_ina230_board_info));
}

static void modify_reworked_rail_data(void)
{
	ardbeg_A01_power_mon_info_1[ARDBEG_A01_VDD_1V35_SD2].resistor
					= VDD_1V35_SD2_REWORKED;
	ardbeg_A01_power_mon_info_1[ARDBEG_A01_VDD_CPU_BUCKCPU].resistor
					= VDD_CPU_BUCKCPU_REWORKED;
	ardbeg_A01_power_mon_info_1[ARDBEG_A01_VDD_SOC_SD1].resistor
					= VDD_SOC_SD1_REWORKED;
}

static void modify_tn8_rail_data(void)
{
	/* E1780-A02 TN8 w/ E1736-A00 PMU*/
	ardbeg_A01_power_mon_info_1[ARDBEG_A01_VDD_SYS_BAT]
		.calibration_data  = 0x3547;
	ardbeg_A01_power_mon_info_1[ARDBEG_A01_VDD_SYS_BAT]
		.power_lsb = 3.128284087 * PRECISION_MULTIPLIER_ARDBEG;
	ardbeg_A01_power_mon_info_1[ARDBEG_A01_VDD_SYS_BAT]
		.resistor = 3;

	ardbeg_A01_power_mon_info_1[ARDBEG_A01_VDD_SYS_BUCKSOC]
		.calibration_data  = 0x2ED7;
	ardbeg_A01_power_mon_info_1[ARDBEG_A01_VDD_SYS_BUCKSOC]
		.power_lsb = 1.067467267 * PRECISION_MULTIPLIER_ARDBEG;
	ardbeg_A01_power_mon_info_1[ARDBEG_A01_VDD_SYS_BUCKSOC]
		.resistor = 10;

	ardbeg_A01_power_mon_info_1[ARDBEG_A01_VDD_3V3A_LDO1_6]
		.calibration_data  = 0x7FFF;
	ardbeg_A01_power_mon_info_1[ARDBEG_A01_VDD_3V3A_LDO1_6]
		.power_lsb = 0.390636921 * PRECISION_MULTIPLIER_ARDBEG;
	ardbeg_A01_power_mon_info_1[ARDBEG_A01_VDD_3V3A_LDO1_6]
		.resistor = 10;

	ardbeg_A01_power_mon_info_2[ARDBEG_A01_VDD_1V8A_LDO2_5_7]
		.calibration_data  = 0x7FFF;
	ardbeg_A01_power_mon_info_2[ARDBEG_A01_VDD_1V8A_LDO2_5_7]
		.power_lsb = 0.390636921 * PRECISION_MULTIPLIER_ARDBEG;
	ardbeg_A01_power_mon_info_2[ARDBEG_A01_VDD_1V8A_LDO2_5_7]
		.resistor = 10;

	power_mon_info_0[VDD_BAT_0]
		.calibration_data = 0x1FF7;
	power_mon_info_0[VDD_BAT_0]
		.power_lsb = 3.128437004 * PRECISION_MULTIPLIER_ARDBEG;
	power_mon_info_0[VDD_BAT_0]
		.resistor = 5;

	power_mon_info_0[VDD_SYS_BUCKSOC_0]
		.calibration_data = 0x2ED7;
	power_mon_info_0[VDD_SYS_BUCKSOC_0]
		.power_lsb = 1.067467267 * PRECISION_MULTIPLIER_ARDBEG;
	power_mon_info_0[VDD_SYS_BUCKSOC_0]
		.resistor = 10;
}

int __init ardbeg_pmon_init(void)
{
	/*
	* Get power_config of board and check whether
	* board is power reworked or not.
	* In case board is reworked, modify rail data
	* for which rework was done.
	*/
	u8 power_config;
	struct board_info bi;
	power_config = get_power_config();
	if (power_config & ARDBEG_POWER_REWORKED_CONFIG)
		modify_reworked_rail_data();

	tegra_get_board_info(&bi);

	if (bi.sku == 1100)
		modify_tn8_rail_data();

	i2c_register_board_info(PCA954x_I2C_BUS0,
			ardbeg_i2c2_0_ina230_board_info,
			ARRAY_SIZE(ardbeg_i2c2_0_ina230_board_info));

	if (bi.fab >= BOARD_FAB_A01)
		register_devices_ardbeg_A01();
	else if ((bi.board_id != BOARD_E1784) &&
		(bi.board_id != BOARD_E1922) &&
		(bi.board_id != BOARD_E1923))
		register_devices_ardbeg();

	return 0;
}

