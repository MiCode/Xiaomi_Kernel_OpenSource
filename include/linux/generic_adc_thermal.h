/*
 * include/linux/generic_adc_thermal.h
 *
 * Generic ADC thermal driver
 *
 * Copyright (c) 2013, NVIDIA Corporation. All rights reserved.
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

#ifndef GENERIC_ADC_THERMAL_H
#define GENERIC_ADC_THERMAL_H

struct gadc_thermal_platform_data {
	const char *iio_channel_name;
	const char *tz_name;
	int temp_offset; /* mC */
	int (*adc_to_temp)(struct gadc_thermal_platform_data *pdata, int val);
};
#endif /* GENERIC_ADC_THERMAL_H */
