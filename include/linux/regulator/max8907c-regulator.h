/* linux/regulator/max8907c-regulator.h
 *
 * Functions to access MAX8907C power management chip.
 *
 * Copyright (C) 2010 Gyungoh Yoo <jack.yoo@maxim-ic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_REGULATOR_MAX8907C_H
#define __LINUX_REGULATOR_MAX8907C_H

/* IDs */
#define MAX8907C_SD1    0
#define MAX8907C_SD2    1
#define MAX8907C_SD3    2
#define MAX8907C_LDO1   3
#define MAX8907C_LDO2   4
#define MAX8907C_LDO3   5
#define MAX8907C_LDO4   6
#define MAX8907C_LDO5   7
#define MAX8907C_LDO6   8
#define MAX8907C_LDO7   9
#define MAX8907C_LDO8   10
#define MAX8907C_LDO9   11
#define MAX8907C_LDO10  12
#define MAX8907C_LDO11  13
#define MAX8907C_LDO12  14
#define MAX8907C_LDO13  15
#define MAX8907C_LDO14  16
#define MAX8907C_LDO15  17
#define MAX8907C_LDO16  18
#define MAX8907C_LDO17  19
#define MAX8907C_LDO18  20
#define MAX8907C_LDO19  21
#define MAX8907C_LDO20  22
#define MAX8907C_OUT5V  23
#define MAX8907C_OUT33V 24
#define MAX8907C_BBAT   25
#define MAX8907C_SDBY   26
#define MAX8907C_VRTC   27
#define MAX8907C_WLED   27

/*
 * struct max8907c_chip_regulator_data - max8907c chip regulator data.
 *
 * @enable_time_us: microseconds delay required after enabling rail regulator.
 */
struct max8907c_chip_regulator_data {
	int enable_time_us;
};
#endif
