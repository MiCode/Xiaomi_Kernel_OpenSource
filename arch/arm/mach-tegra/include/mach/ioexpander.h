/*
 * arch/arm/mach-tegra/include/mach/ioexpander.h
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _MACH_TEGRA_IO_EXPANDER_H_
#define _MACH_TEGRA_IO_EXPANDER_H_

/*
 * Define I/O expander register offsets for I2C command
 */
#define IO_EXP_INPUT_PORT_REG_0		0x0
#define IO_EXP_INPUT_PORT_REG_1		0x1

#define IO_EXP_OUTPUT_PORT_REG_0	0x2
#define IO_EXP_OUTPUT_PORT_REG_1	0x3

#define IO_EXP_POLARITY_INV_REG_0	0x4
#define IO_EXP_POLARITY_INV_REG_1	0x5

#define IO_EXP_CONFIG_REG_0		0x6
#define IO_EXP_CONFIG_REG_1		0x7

enum {
	IO_EXP_PIN_0 = 0,
	IO_EXP_PIN_1 = 1,
	IO_EXP_PIN_2 = 2,
	IO_EXP_PIN_3 = 3,
	IO_EXP_PIN_4 = 4,
	IO_EXP_PIN_5 = 5,
	IO_EXP_PIN_6 = 6,
	IO_EXP_PIN_7 = 7,
};

#endif
