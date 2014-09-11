/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __LMH_INTERFACE_H
#define __LMH_INTERFACE_H

#define LMH_NAME_MAX			20
#define LMH_POLLING_MSEC		30

enum lmh_trip_type {
	LMH_LOW_TRIP,
	LMH_HIGH_TRIP,
	LMH_TRIP_MAX,
};

enum lmh_monitor_state {
	LMH_ISR_DISABLED,
	LMH_ISR_MONITOR,
	LMH_ISR_POLLING,
	LMH_ISR_NR,
};

struct lmh_sensor_ops {
	int (*read) (struct lmh_sensor_ops *, long *);
	int (*reset_interrupt) (struct lmh_sensor_ops *);
	int (*enable_hw_log) (uint32_t, uint32_t);
	int (*disable_hw_log) (void);
	void (*interrupt_notify) (struct lmh_sensor_ops *, long);
};

struct lmh_device_ops {
	int (*get_available_levels) (struct lmh_device_ops *, int *);
	int (*get_curr_level) (struct lmh_device_ops *, int *);
	int (*set_level) (struct lmh_device_ops *, int);
};

static int lmh_poll_interval = LMH_POLLING_MSEC;
#ifdef CONFIG_LIMITS_MONITOR
int lmh_get_all_dev_levels(char *, int *);
int lmh_set_dev_level(char *, int);
int lmh_get_curr_level(char *, int *);
int lmh_sensor_register(char *, struct lmh_sensor_ops *);
void lmh_sensor_deregister(struct lmh_sensor_ops *);
int lmh_device_register(char *, struct lmh_device_ops *);
void lmh_device_deregister(struct lmh_device_ops *);
#else
static inline int lmh_get_all_dev_levels(char *device_name, int *level)
{
	return -ENOSYS;
}

static inline int lmh_set_dev_level(char *device_name, int level)
{
	return -ENOSYS;
}

static inline int lmh_get_curr_level(char *device_name, int *level)
{
	return -ENOSYS;
}

static inline int lmh_sensor_register(char *sensor_name,
	struct lmh_sensor_ops *ops)
{
	return -ENOSYS;
}

static inline void lmh_sensor_deregister(struct lmh_sensor_ops *ops)
{
	return;
}

static inline int lmh_device_register(char *device_name,
	struct lmh_device_ops *ops)
{
	return -ENOSYS;
}

static inline void lmh_device_deregister(struct lmh_device_ops *ops)
{
	return;
}
#endif

#endif /*__LMH_INTERFACE_H*/
