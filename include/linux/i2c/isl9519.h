/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
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
#ifndef __ISL9519_H__
#define __ISL9519_H__

/**
 * struct isl_platform_data
 * @chgcurrent:	max current the islchip can draw
 * @valid_irq:			interrupt for insertion/removal notification
 * @valid_n_gpio:		gpio to debounce insertion/removal
 * @valid_config:		machine specific func to configure gpio line
 * @max_system_voltage:		the max voltage isl should charge battery to
 * @min_system_voltage:		the min voltage isl should trkl charge the
 *				battery
 * @term_current:		the batt current when isl charging should stop
 * @input_current:		the max current isl should pull from the adapter
 */
struct isl_platform_data {
	int chgcurrent;
	int valid_n_gpio;
	int (*chg_detection_config) (void);
	int max_system_voltage;
	int min_system_voltage;
	int term_current;
	int input_current;
};

#endif
