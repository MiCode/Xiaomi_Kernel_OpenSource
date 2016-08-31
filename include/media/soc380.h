/*
 * include/media/soc380.h
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION, All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __SOC380_H__
#define __SOC380_H__

#include <linux/ioctl.h>  /* For IOCTL macros */

#define SOC380_IOCTL_SET_MODE		_IOW('o', 1, struct soc380_mode)
#define SOC380_IOCTL_GET_STATUS		_IOR('o', 2, struct soc380_status)

struct soc380_mode {
	int xres;
	int yres;
};

struct soc380_status {
	int data;
	int status;
};

#ifdef __KERNEL__
struct soc380_platform_data {
	int (*power_on)(struct device *);
	int (*power_off)(struct device *);
	const char *mclk_name;
};
#endif /* __KERNEL__ */

#endif  /* __SOC380_H__ */

