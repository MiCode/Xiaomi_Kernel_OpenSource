/*
 *  bq27520_battery.h
 *  fuel-gauge systems for lithium-ion (Li+) batteries
 *
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
 *  Nathan Zhang <nathanz@nvidia.com>
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __BQ27520_BATTERY_H_
#define __BQ27520_BATTERY_H_

struct bq27520_platform_data {
	unsigned long full_capacity; /* in mAh */
	unsigned long full_energy; /* in mWh */
	unsigned long taper_rate;
	unsigned long terminate_voltage; /* in mV */
	unsigned long v_at_chg_term; /* in mV */
	const char *tz_name;
};

#endif
