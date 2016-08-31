/*
 * Copyright (c) 2014, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>

#ifndef __IIO_LS_SYSFS_H__
#define __IIO_LS_SYSFS_H__

#define MAX_CHAN 2

enum prop {
	VENDOR,
	NUM_PROP,
};

enum channel_prop {
	MAX_RANGE,
	INTEGRATION_TIME,
	RESOLUTION,
	POWER_CONSUMED,
	MAX_CHAN_PROP,
};

struct lightsensor_spec {
	const char *prop[NUM_PROP];
	const char *chan_prop[MAX_CHAN][MAX_CHAN_PROP];
};

#ifdef CONFIG_LS_SYSFS
extern void fill_ls_attrs(struct lightsensor_spec *, struct attribute **);
#else
void fill_ls_attrs(struct lightsensor_spec *meta, struct attribute **attrs)
{
}
#endif

#endif /* __IIO_LS_SYSFS_H__ */
