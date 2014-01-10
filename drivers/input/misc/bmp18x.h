/*  Copyright (c) 2010  Christoph Mair <christoph.mair@gmail.com>
    Copyright (c) 2011  Bosch Sensortec GmbH
    Copyright (c) 2011  Unixphere AB

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#ifndef _BMP18X_H
#define _BMP18X_H
#include <linux/sensors.h>

#define BMP18X_NAME "bmp18x"

/**
 * struct bmp18x_platform_data - represents platform data for the bmp18x driver
 * @chip_id: Configurable chip id for non-default chip revisions
 * @default_oversampling: Default oversampling value to be used at startup,
 * value range is 0-3 with rising sensitivity.
 * @default_sw_oversampling: Default software oversampling value to be used
 * at startup,value range is 0(Disabled) or 1(Enabled). Only take effect
 * when default_oversampling is 3.
 * @temp_measurement_period: Temperature measurement period (milliseconds), set
 * to zero if unsure.
 * @init_hw: Callback for hw specific startup
 * @deinit_hw: Callback for hw specific shutdown
 */

struct bmp18x_bus_ops {
	int	(*read_block)(void *client, u8 reg, int len, char *buf);
	int	(*read_byte)(void *client, u8 reg);
	int	(*write_byte)(void *client, u8 reg, u8 value);
};

struct bmp18x_data_bus {
	const struct bmp18x_bus_ops *bops;
	void	*client;
};

struct bmp18x_calibration_data {
	s16 AC1, AC2, AC3;
	u16 AC4, AC5, AC6;
	s16 B1, B2;
	s16 MB, MC, MD;
};

/* Each client has this additional data */
struct bmp18x_data {
	struct	bmp18x_data_bus data_bus;
	struct	device *dev;
	struct	mutex lock;
	struct	bmp18x_calibration_data calibration;
	struct	sensors_classdev cdev;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct	early_suspend early_suspend;
#endif
	struct	input_dev	*input;
	struct	delayed_work work;

	u8	oversampling_setting;
	u8	sw_oversampling_setting;
	u32	raw_temperature;
	u32	raw_pressure;
	u32	temp_measurement_period;
	u32	last_temp_measurement;
	s32	b6; /* calculated temperature correction coefficient */
	u32	delay;
	u32	enable;
	u32	power_enabled;
};

struct bmp18x_platform_data {
	u8	chip_id;
	u8	default_oversampling;
	u8	default_sw_oversampling;
	u32	temp_measurement_period;
	u32	power_enabled;
	int	(*init_hw)(struct bmp18x_data_bus *);
	void	(*deinit_hw)(struct bmp18x_data_bus *);
	int	(*set_power)(struct bmp18x_data*, int);
};

int bmp18x_probe(struct device *dev, struct bmp18x_data_bus *data_bus);
int bmp18x_remove(struct device *dev);
#ifdef CONFIG_PM
int bmp18x_enable(struct device *dev);
int bmp18x_disable(struct device *dev);
#endif

#endif
