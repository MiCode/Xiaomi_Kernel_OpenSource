/*
 * include/linux/therm_est.h
 *
 * Copyright (c) 2010-2014, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _LINUX_THERM_EST_H
#define _LINUX_THERM_EST_H

#include <linux/workqueue.h>
#include <linux/thermal.h>
#include <linux/platform_data/thermal_sensors.h>

#define HIST_LEN (20)

#define MAX_ACTIVE_STATES	10
#define MAX_TIMER_TRIPS		10

struct therm_est_subdevice {
	void *dev_data;
	struct thermal_zone_device *sub_thz;
	long coeffs[HIST_LEN];
	long hist[HIST_LEN];
};

/*
 * Timer trip provides a way to change trip temp dynamically based on timestamp
 * when the trip is enabled.
 * - Timer trip can be various numbers on a trip.
 * - If the trip is enabled, then timer will be started with time_after delay
 *   in the corresponding timer trip. After the timer expires, trip_temp and
 *   hysteresis in the corresponding timer trip will be used to trip_temp for
 *   the trip.
 * - When the timer has expired, index of timer trip will be increased a step
 *   and then start the timer with time_after delay in newly indexed timer trip.
 * - When temp is below trip temp, index of timer trip will be decreased a step
 *   and then stop the timer and start the timer with time_after delay in newly
 *   indexed timer trip.
 * - The timer will be stopped if there is no more next timer trip on the trip,
 *   or the trip is disabled.
 */
struct therm_est_timer_trip {
	long time_after; /* in msec */
	long trip_temp;
	long hysteresis;
};

struct therm_est_timer_trip_info {
	int trip; /* trip point on thermal zone to apply timer trip */
	int num_timers;
	struct therm_est_timer_trip timers[MAX_TIMER_TRIPS];
	int cur; /* index of current timer trip */
	s64 last_tripped; /* timestamp when the trip is enabled, in msec */
};

struct therm_est_data {
	/* trip point info */
	int num_trips;
	struct thermal_trip_info *trips;

	/* timer trip info */
	int num_timer_trips;
	struct therm_est_timer_trip_info *timer_trips;

	/* zone parameters */
	struct thermal_zone_params *tzp;
	long toffset;
	long polling_period;
	int passive_delay;
	int tc1;
	int tc2;
	int ndevs;
	struct therm_est_subdevice *devs;
	int use_activator;
};

struct therm_fan_est_subdevice {
	void *dev_data;
	int (*get_temp)(void *, long *);
	long coeffs[HIST_LEN];
	long hist[HIST_LEN];
};

struct therm_fan_est_data {
	long toffset;
	long polling_period;
	int ndevs;
	char *cdev_type;
	int active_trip_temps[MAX_ACTIVE_STATES];
	int active_hysteresis[MAX_ACTIVE_STATES];
	struct therm_fan_est_subdevice devs[];
};
#endif /* _LINUX_THERM_EST_H */
