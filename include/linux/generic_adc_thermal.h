/*
 * include/linux/generic_adc_thermal.h
 *
 * Generic ADC thermal driver
 *
 * Copyright (c) 2013-2014, NVIDIA Corporation. All rights reserved.
 *
 * Author: Jinyoung Park <jinyoungp@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#ifndef _LINUX_GENERIC_ADC_THERMAL_H
#define _LINUX_GENERIC_ADC_THERMAL_H

#include <linux/platform_data/thermal_sensors.h>

struct gadc_thermal_platform_data {
	const char *iio_channel_name;
	const char *tz_name;
	int temp_offset; /* mC */
	bool dual_mode;
	int (*adc_to_temp)(struct gadc_thermal_platform_data *pdata, int val,
			   int val2);
	int *adc_temp_lookup;
	unsigned int lookup_table_size;
	int first_index_temp;
	int last_index_temp;

	/* zone parameters */
	int polling_delay;
	struct thermal_zone_params *tzp;

	/* trip info */
	int num_trips;
	struct thermal_trip_info trips[THERMAL_MAX_TRIPS];
};
#endif /* _LINUX_GENERIC_ADC_THERMAL_H */
