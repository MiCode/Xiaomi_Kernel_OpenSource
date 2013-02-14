/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
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

#ifndef _TDISC_SHINETSU_H_
#define _TDISC_SHINETSU_H_

struct tdisc_abs_values {
	int	x_max;
	int	y_max;
	int	x_min;
	int	y_min;
	int	pressure_max;
	int	pressure_min;
};

struct tdisc_platform_data {
	int	(*tdisc_setup) (void);
	void	(*tdisc_release) (void);
	int	(*tdisc_enable) (void);
	int	(*tdisc_disable)(void);
	int	tdisc_wakeup;
	int	tdisc_gpio;
	bool	tdisc_report_keys;
	bool	tdisc_report_relative;
	bool	tdisc_report_absolute;
	bool	tdisc_report_wheel;
	bool	tdisc_reverse_x;
	bool	tdisc_reverse_y;
	struct	tdisc_abs_values *tdisc_abs;
};

#endif /* _TDISC_SHINETSU_H_ */
