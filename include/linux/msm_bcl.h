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

#ifndef __MSM_BCL_H
#define __MSM_BCL_H

#define BCL_NAME_MAX_LEN 20

enum bcl_trip_type {
	BCL_HIGH_TRIP,
	BCL_LOW_TRIP,
	BCL_TRIP_MAX,
};

enum bcl_param {
	BCL_PARAM_VOLTAGE,
	BCL_PARAM_CURRENT,
	BCL_PARAM_MAX,
};

struct bcl_threshold {
	int                     trip_value;
	enum bcl_trip_type      type;
	void                    *trip_data;
	void (*trip_notify)     (enum bcl_trip_type, int, void *);
};
struct bcl_param_data;
struct bcl_driver_ops {
	int (*read)             (int *);
	int (*set_high_trip)    (int);
	int (*get_high_trip)    (int *);
	int (*set_low_trip)     (int);
	int (*get_low_trip)     (int *);
	int (*disable)          (void);
	int (*enable)           (void);
	int (*notify)           (struct bcl_param_data *, int,
					enum bcl_trip_type);
};

struct bcl_param_data {
	char                    name[BCL_NAME_MAX_LEN];
	struct device           device;
	struct bcl_driver_ops   *ops;
	int                     high_trip;
	int                     low_trip;
	int                     last_read_val;
	bool                    registered;
	struct kobj_attribute   val_attr;
	struct kobj_attribute   high_trip_attr;
	struct kobj_attribute   low_trip_attr;
	struct attribute_group  bcl_attr_gp;
	struct bcl_threshold    *thresh[BCL_TRIP_MAX];
};

#ifdef CONFIG_MSM_BCL_CTL
struct bcl_param_data *msm_bcl_register_param(enum bcl_param,
	struct bcl_driver_ops *, char *);
int msm_bcl_unregister_param(struct bcl_param_data *);
int msm_bcl_enable(void);
int msm_bcl_disable(void);
int msm_bcl_set_threshold(enum bcl_param, enum bcl_trip_type,
	struct bcl_threshold *);
int msm_bcl_read(enum bcl_param, int *);
#else
static inline struct bcl_param_data *msm_bcl_register_param(
	enum bcl_param param_type, struct bcl_driver_ops *ops, char *name)
{
	return NULL;
}
static inline int msm_bcl_unregister_param(struct bcl_param_data *data)
{
	return -ENOSYS;
}
static inline int msm_bcl_enable(void)
{
	return -ENOSYS;
}
static inline int msm_bcl_disable(void)
{
	return -ENOSYS;
}
static inline int msm_bcl_set_threshold(enum bcl_param param_type,
	enum bcl_trip_type type,
	struct bcl_threshold *inp_thresh)
{
	return -ENOSYS;
}
static inline int msm_bcl_read(enum bcl_param param_type, int *vbat_value)
{
	return -ENOSYS;
}
#endif

#endif /*__MSM_BCL_H*/
