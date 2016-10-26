/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __DRMGSL_PWRCTL_H
#define __DRMGSL_PWRCTL_H

struct drmgsl_pwrlevel {
	unsigned int gpu_freq;
	unsigned int bus_freq;
	unsigned int bus_min;
	unsigned int bus_max;
};

struct drmgsl_pwrctl {
	struct drmgsl_pwrlevel *pwrlevels;
	unsigned int level_num;
	unsigned int active_pwrlevel;
	unsigned int previous_pwrlevel;
	unsigned int thermal_pwrlevel;
	unsigned int default_pwrlevel;
	unsigned int wakeup_maxpwrlevel;
	unsigned int max_pwrlevel;
	unsigned int min_pwrlevel;
	unsigned int num_pwrlevels;
	unsigned long interval_timeout;
};

#endif /* __DRMGSL_PWRCTRL_H */
