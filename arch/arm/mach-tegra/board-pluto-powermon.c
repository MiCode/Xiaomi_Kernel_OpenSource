/*
 * arch/arm/mach-tegra/board-pluto-powermon.c
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION. All rights reserved.
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
#include <linux/i2c/pca954x.h>

#include "board.h"
#include "board-pluto.h"

#define PRECISION_MULTIPLIER_PLUTO	1000

/*
 * following power_config will be passed from Bootloader
 * if board is reworked for power measurement
 */
#define PLUTO_POWER_REWORKED_CONFIG	0x10

enum {
	PM_VDD_CELL,
	UNUSED_RAIL,
};

enum {
	VDD_SYS_SUM,
	VDD_SYS_SMPS123,
	VDD_SYS_SMPS45,
	VDD_SYS_SMPS6,
	VDD_SYS_SMPS7,
	VDD_SYS_SMPS8,
	VDD_SYS_BL,
	VDD_SYS_LDO8,
	VDD_MMC_LDO9,
	VDD_5V0_LDOUSB,
	VDD_1V8_AP,
	VDD_MMC_LCD,
	VDDIO_HSIC_BB,
	AVDD_PLL_BB,
};

enum {
	AVDD_1V05_LDO1,
	VDDIO_1V8_BB,
};

static struct ina230_platform_data power_mon_info_0[] = {
	[PM_VDD_CELL] = {
		/*
		 * Calibration data is calculated by:
		 * Imax = 6A (max peak current from battery)
		 * Current_LSB = Imax / 2^15
		 *	       = 0.000183
		 *	      ~= 0.0002(A per bit)
		 *
		 * Rshunt (on Pluto) = 0.01(Ohm)
		 * cal = 0.00512 / (Current_LSB * Rshunt)
		 *     = 0.00512 / (0.0002 * 0.01)
		 *     = 2560
		 */
		.calibration_data = 2560,
		.rail_name = "PM_VDD_CELL",
		/*
		 * Current_LSB is 0.0002A per bit (0.2mA per bit), and we use mA
		 * as display as unit:
		 *	current_lsb = 0.2
		 *	power_lsb = current_lsb * POWER_LSB_TO_CURRENT_LSB_RATIO
		 *		  = 0.2 * 25 = 5
		 *
		 */
		.divisor = POWER_LSB_TO_CURRENT_LSB_RATIO,
		.power_lsb = 5,
		.precision_multiplier = 1,
		/*
		 * this value specify the battery in-serial resistor value in
		 * the unit of mOhm.
		 */
		.resistor = 10,
		.shunt_polarity_inverted = 1,
		/*
		 * INA230's alert pin can be used to generate the OC throttling
		 * signal to AP. To do so set the 'current_threshold' to a
		 * non-zero value in the unit of mA; set it to 0 will disable
		 * the alert pin generating OC
		 * signal.
		 */
		.current_threshold = 0,
	},
	/* All unused HPA01112 devices use below data*/
	[UNUSED_RAIL] = {
		.calibration_data = 0x369c,
		.power_lsb = 3.051979018 * PRECISION_MULTIPLIER_PLUTO,
		.rail_name = "unused_rail",
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_PLUTO,
	},
};

static struct ina230_platform_data power_mon_info_1[] = {
	[VDD_SYS_SUM] = {
		.calibration_data  = 0x369c,
		.power_lsb = 3.051979018 * PRECISION_MULTIPLIER_PLUTO,
		.rail_name = "VDD_SYS_SUM",
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_PLUTO,
	},

	[VDD_SYS_SMPS123] = {
		.calibration_data  = 0x2bb0,
		.power_lsb = 2.288984263 * PRECISION_MULTIPLIER_PLUTO,
		.rail_name = "VDD_SYS_SMPS123",
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_PLUTO,
	},

	[VDD_SYS_SMPS45] = {
		.calibration_data  = 0x4188,
		.power_lsb = 1.525989509 * PRECISION_MULTIPLIER_PLUTO,
		.rail_name = "VDD_SYS_SMPS45",
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_PLUTO,
	},

	[VDD_SYS_SMPS6] = {
		.calibration_data  = 0x2bb0,
		.power_lsb = 0.381497377 * PRECISION_MULTIPLIER_PLUTO,
		.rail_name = "VDD_SYS_SMPS6",
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_PLUTO,
	},

	[VDD_SYS_SMPS7] = {
		.calibration_data  = 0x2bb0,
		.power_lsb = 0.228898426  * PRECISION_MULTIPLIER_PLUTO,
		.rail_name = "VDD_SYS_SMPS7",
		.resistor = 50,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_PLUTO,
	},

	[VDD_SYS_SMPS8] = {
		.calibration_data  = 0x2bb0,
		.power_lsb = 0.228898426 * PRECISION_MULTIPLIER_PLUTO,
		.rail_name = "VDD_SYS_SMPS8",
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_PLUTO,
	},

	[VDD_SYS_BL] = {
		.calibration_data  = 0x4188,
		.power_lsb = 0.152598951 * PRECISION_MULTIPLIER_PLUTO,
		.rail_name = "VDD_SYS_BL",
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_PLUTO,
	},

	[VDD_SYS_LDO8] = {
		.calibration_data  = 0x2d82,
		.power_lsb = 0.054935622 * PRECISION_MULTIPLIER_PLUTO,
		.rail_name = "VDD_SYS_LDO8",
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_PLUTO,
	},

	[VDD_MMC_LDO9] = {
		.calibration_data  = 0x51ea,
		.power_lsb = 0.03051979 * PRECISION_MULTIPLIER_PLUTO,
		.rail_name = "VDD_MMC_LDO9",
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_PLUTO,
	},

	[VDD_5V0_LDOUSB] = {
		.calibration_data  = 0x2f7d,
		.power_lsb = 0.052644567 * PRECISION_MULTIPLIER_PLUTO,
		.rail_name = "VDD_5V0_LDOUSB",
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_PLUTO,
	},

	[VDD_1V8_AP] = {
		.calibration_data  = 0x4feb,
		.power_lsb = 0.125128305 * PRECISION_MULTIPLIER_PLUTO,
		.rail_name = "VDD_1V8_AP",
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_PLUTO,
	},

	[VDD_MMC_LCD] = {
		.calibration_data  = 0x346d,
		.power_lsb = 0.047686462 * PRECISION_MULTIPLIER_PLUTO,
		.rail_name = "VDD_MMC_LCD",
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_PLUTO,
	},

	[VDDIO_HSIC_BB] = {
		.calibration_data  = 0x6d39,
		.power_lsb = 0.00915561 * PRECISION_MULTIPLIER_PLUTO,
		.rail_name = "VDDIO_HSIC_BB",
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_PLUTO,
	},

	[AVDD_PLL_BB] = {
		.calibration_data  = 0x7fff,
		.power_lsb = 0.007812738 * PRECISION_MULTIPLIER_PLUTO,
		.rail_name = "AVDD_PLL_BB",
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_PLUTO,
	},
};

static struct ina230_platform_data power_mon_info_2[] = {
	[AVDD_1V05_LDO1] = {
		.calibration_data  = 0x7fff,
		.power_lsb = 0.195318461 * PRECISION_MULTIPLIER_PLUTO,
		.rail_name = "AVDD_1V05_LDO1",
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_PLUTO,
	},

	[VDDIO_1V8_BB] = {
		.calibration_data  = 0x7fff,
		.power_lsb = 0.078127384 * PRECISION_MULTIPLIER_PLUTO,
		.rail_name = "VDDIO_1V8_BB",
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_PLUTO,
	},
};

enum {
	INA_I2C_2_0_ADDR_40,
	INA_I2C_2_0_ADDR_41,
	INA_I2C_2_0_ADDR_42,
	INA_I2C_2_0_ADDR_43,
};

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

enum {
	INA_I2C_2_2_ADDR_49,
	INA_I2C_2_2_ADDR_4C,
};

static struct i2c_board_info pluto_i2c2_0_ina230_board_info[] = {
	[INA_I2C_2_0_ADDR_40] = {
		I2C_BOARD_INFO("ina230", 0x40),
		.platform_data = &power_mon_info_0[PM_VDD_CELL],
		.irq = -1,
	},

	[INA_I2C_2_0_ADDR_41] = {
		I2C_BOARD_INFO("ina230", 0x41),
		.platform_data = &power_mon_info_0[UNUSED_RAIL],
		.irq = -1,
	},

	[INA_I2C_2_0_ADDR_42] = {
		I2C_BOARD_INFO("ina230", 0x42),
		.platform_data = &power_mon_info_0[UNUSED_RAIL],
		.irq = -1,
	},

	[INA_I2C_2_0_ADDR_43] = {
		I2C_BOARD_INFO("ina230", 0x43),
		.platform_data = &power_mon_info_0[UNUSED_RAIL],
		.irq = -1,
	},
};

static struct i2c_board_info pluto_i2c2_1_ina230_board_info[] = {
	[INA_I2C_2_1_ADDR_40] = {
		I2C_BOARD_INFO("ina230", 0x40),
		.platform_data = &power_mon_info_1[VDD_SYS_SUM],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_41] = {
		I2C_BOARD_INFO("ina230", 0x41),
		.platform_data = &power_mon_info_1[VDD_SYS_SMPS123],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_42] = {
		I2C_BOARD_INFO("ina230", 0x42),
		.platform_data = &power_mon_info_1[VDD_SYS_SMPS45],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_43] = {
		I2C_BOARD_INFO("ina230", 0x43),
		.platform_data = &power_mon_info_1[VDD_SYS_SMPS6],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_44] = {
		I2C_BOARD_INFO("ina230", 0x44),
		.platform_data = &power_mon_info_1[VDD_SYS_SMPS7],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_45] = {
		I2C_BOARD_INFO("ina230", 0x45),
		.platform_data = &power_mon_info_1[VDD_SYS_SMPS8],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_46] = {
		I2C_BOARD_INFO("ina230", 0x46),
		.platform_data = &power_mon_info_1[VDD_SYS_BL],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_47] = {
		I2C_BOARD_INFO("ina230", 0x47),
		.platform_data = &power_mon_info_1[VDD_SYS_LDO8],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_48] = {
		I2C_BOARD_INFO("ina230", 0x48),
		.platform_data = &power_mon_info_1[VDD_MMC_LDO9],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_49] = {
		I2C_BOARD_INFO("ina230", 0x49),
		.platform_data = &power_mon_info_1[VDD_5V0_LDOUSB],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_4B] = {
		I2C_BOARD_INFO("ina230", 0x4B),
		.platform_data = &power_mon_info_1[VDD_1V8_AP],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_4C] = {
		I2C_BOARD_INFO("ina230", 0x4C),
		.platform_data = &power_mon_info_1[VDD_MMC_LCD],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_4E] = {
		I2C_BOARD_INFO("ina230", 0x4E),
		.platform_data = &power_mon_info_1[VDDIO_HSIC_BB],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_4F] = {
		I2C_BOARD_INFO("ina230", 0x4F),
		.platform_data = &power_mon_info_1[AVDD_PLL_BB],
		.irq = -1,
	},
};

static struct i2c_board_info pluto_i2c2_2_ina230_board_info[] = {
	[INA_I2C_2_2_ADDR_49] = {
		I2C_BOARD_INFO("ina230", 0x49),
		.platform_data = &power_mon_info_2[AVDD_1V05_LDO1],
		.irq = -1,
	},

	[INA_I2C_2_2_ADDR_4C] = {
		I2C_BOARD_INFO("ina230", 0x4C),
		.platform_data = &power_mon_info_2[VDDIO_1V8_BB],
		.irq = -1,
	},
};

static struct pca954x_platform_mode pluto_pca954x_modes[] = {
	{ .adap_id = PCA954x_I2C_BUS0, .deselect_on_exit = true, },
	{ .adap_id = PCA954x_I2C_BUS1, .deselect_on_exit = true, },
	{ .adap_id = PCA954x_I2C_BUS2, .deselect_on_exit = true, },
	{ .adap_id = PCA954x_I2C_BUS3, .deselect_on_exit = true, },
};

static struct pca954x_platform_data pluto_pca954x_data = {
	.modes    = pluto_pca954x_modes,
	.num_modes      = ARRAY_SIZE(pluto_pca954x_modes),
};

static const struct i2c_board_info pluto_i2c2_board_info[] = {
	{
		I2C_BOARD_INFO("pca9546", 0x71),
		.platform_data = &pluto_pca954x_data,
	},
};

static void modify_reworked_rail_data(void)
{
	power_mon_info_1[VDD_SYS_SUM].calibration_data = 0x426;
	power_mon_info_1[VDD_SYS_SUM].power_lsb =
				1.20527307 * PRECISION_MULTIPLIER_PLUTO;

	power_mon_info_1[VDD_SYS_SMPS123].calibration_data = 0x2134;
	power_mon_info_1[VDD_SYS_SMPS123].power_lsb =
				0.301176471 * PRECISION_MULTIPLIER_PLUTO;

	power_mon_info_1[VDD_SYS_SMPS45].calibration_data = 0x2134;
	power_mon_info_1[VDD_SYS_SMPS45].power_lsb =
				0.301176471 * PRECISION_MULTIPLIER_PLUTO;

	power_mon_info_1[VDD_SYS_SMPS6].calibration_data = 0x3756;
	power_mon_info_1[VDD_SYS_SMPS6].power_lsb =
				0.180714387 * PRECISION_MULTIPLIER_PLUTO;

	power_mon_info_1[VDD_SYS_SMPS7].calibration_data = 0x84D;
	power_mon_info_1[VDD_SYS_SMPS7].power_lsb =
				0.120470588 * PRECISION_MULTIPLIER_PLUTO;

	power_mon_info_1[VDD_SYS_SMPS8].calibration_data = 0x14C0;
	power_mon_info_1[VDD_SYS_SMPS8].power_lsb =
				0.240963855 * PRECISION_MULTIPLIER_PLUTO;

	power_mon_info_1[VDD_SYS_LDO8].calibration_data = 0x18E7;
	power_mon_info_1[VDD_SYS_LDO8].power_lsb =
				0.040156863 * PRECISION_MULTIPLIER_PLUTO;

	power_mon_info_1[VDD_MMC_LDO9].calibration_data = 0xEAD;
	power_mon_info_1[VDD_MMC_LDO9].power_lsb =
				0.068139473 * PRECISION_MULTIPLIER_PLUTO;

	power_mon_info_1[VDD_MMC_LCD].calibration_data = 0x1E95;
	power_mon_info_1[VDD_MMC_LCD].power_lsb =
				0.08174735 * PRECISION_MULTIPLIER_PLUTO;
}

int __init pluto_pmon_init(void)
{
	u8 power_config;

	/*
	 * Get power_config of board and check whether
	 * board is power reworked or not.
	 * In case board is reworked, modify rail data
	 * for which rework was done.
	 */
	power_config = get_power_config();
	if (power_config & PLUTO_POWER_REWORKED_CONFIG)
		modify_reworked_rail_data();

	i2c_register_board_info(1, pluto_i2c2_board_info,
		ARRAY_SIZE(pluto_i2c2_board_info));

	i2c_register_board_info(PCA954x_I2C_BUS0,
			pluto_i2c2_0_ina230_board_info,
			ARRAY_SIZE(pluto_i2c2_0_ina230_board_info));

	i2c_register_board_info(PCA954x_I2C_BUS1,
			pluto_i2c2_1_ina230_board_info,
			ARRAY_SIZE(pluto_i2c2_1_ina230_board_info));

	i2c_register_board_info(PCA954x_I2C_BUS2,
			pluto_i2c2_2_ina230_board_info,
			ARRAY_SIZE(pluto_i2c2_2_ina230_board_info));
	return 0;
}

