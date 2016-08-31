/*
 * include/linux/regulator/max77663-regulator.h
 * Maxim LDO and Buck regulators driver
 *
 * Copyright 2011-2012 Maxim Integrated Products, Inc.
 * Copyright (C) 2011-2012 NVIDIA Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 */

#ifndef __LINUX_REGULATOR_MAX77663_REGULATOR_H__
#define __LINUX_REGULATOR_MAX77663_REGULATOR_H__

#include <linux/regulator/machine.h>

#define max77663_rails(_name)	"max77663_"#_name

enum max77663_regulator_id {
	MAX77663_REGULATOR_ID_SD0,
	MAX77663_REGULATOR_ID_DVSSD0,
	MAX77663_REGULATOR_ID_SD1,
	MAX77663_REGULATOR_ID_DVSSD1,
	MAX77663_REGULATOR_ID_SD2,
	MAX77663_REGULATOR_ID_SD3,
	MAX77663_REGULATOR_ID_SD4,
	MAX77663_REGULATOR_ID_LDO0,
	MAX77663_REGULATOR_ID_LDO1,
	MAX77663_REGULATOR_ID_LDO2,
	MAX77663_REGULATOR_ID_LDO3,
	MAX77663_REGULATOR_ID_LDO4,
	MAX77663_REGULATOR_ID_LDO5,
	MAX77663_REGULATOR_ID_LDO6,
	MAX77663_REGULATOR_ID_LDO7,
	MAX77663_REGULATOR_ID_LDO8,
	MAX77663_REGULATOR_ID_NR,
};

/* FPS Power Up/Down Period */
enum max77663_regulator_fps_power_period {
	FPS_POWER_PERIOD_0,
	FPS_POWER_PERIOD_1,
	FPS_POWER_PERIOD_2,
	FPS_POWER_PERIOD_3,
	FPS_POWER_PERIOD_4,
	FPS_POWER_PERIOD_5,
	FPS_POWER_PERIOD_6,
	FPS_POWER_PERIOD_7,
	FPS_POWER_PERIOD_DEF = -1,
};

/* FPS Time Period */
enum max77663_regulator_fps_time_period {
	FPS_TIME_PERIOD_20US,
	FPS_TIME_PERIOD_40US,
	FPS_TIME_PERIOD_80US,
	FPS_TIME_PERIOD_160US,
	FPS_TIME_PERIOD_320US,
	FPS_TIME_PERIOD_640US,
	FPS_TIME_PERIOD_1280US,
	FPS_TIME_PERIOD_2560US,
	FPS_TIME_PERIOD_DEF = -1,
};

/* FPS Enable Source */
enum max77663_regulator_fps_en_src {
	FPS_EN_SRC_EN0,
	FPS_EN_SRC_EN1,
	FPS_EN_SRC_SW,
	FPS_EN_SRC_RSVD,
};

/* FPS Source */
enum max77663_regulator_fps_src {
	FPS_SRC_0,
	FPS_SRC_1,
	FPS_SRC_2,
	FPS_SRC_NONE,
	FPS_SRC_DEF = -1,
};

/*
 * Flags
 */
/* SD0 is controlled by EN2 */
#define EN2_CTRL_SD0		0x01

/* SD Slew Rate */
#define SD_SLEW_RATE_SLOWEST	0x02	/*  13.75mV/us */
#define SD_SLEW_RATE_SLOW	0x04	/*  27.50mV/us */
#define SD_SLEW_RATE_FAST	0x08	/*  55.00mV/us */
#define SD_SLEW_RATE_FASTEST	0x10	/* 100.00mV/us */
#define SD_SLEW_RATE_MASK	0x1E

/* SD Forced PWM Mode */
#define SD_FORCED_PWM_MODE	0x20

/* SD Failling Slew Rate Active-Discharge Mode */
#define SD_FSRADE_DISABLE	0x40

/* Group Low-Power Mode */
#define GLPM_ENABLE		0x80

/* Tracking for LDO4 */
#define LDO4_EN_TRACKING	0x100

struct max77663_regulator_fps_cfg {
	enum max77663_regulator_fps_src src;
	enum max77663_regulator_fps_en_src en_src;
	enum max77663_regulator_fps_time_period time_period;
};

struct max77663_regulator_platform_data {
	struct regulator_init_data *reg_init_data;
	int id;
	enum max77663_regulator_fps_src fps_src;
	enum max77663_regulator_fps_power_period fps_pu_period;
	enum max77663_regulator_fps_power_period fps_pd_period;

	int num_fps_cfgs;
	struct max77663_regulator_fps_cfg *fps_cfgs;

	unsigned int flags;
};

#endif /* __LINUX_REGULATOR_MAX77663_REGULATOR_H__ */
