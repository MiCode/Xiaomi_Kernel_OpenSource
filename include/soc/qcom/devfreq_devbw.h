/*
 * Copyright (c) 2014, 2018 The Linux Foundation. All rights reserved.
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

#ifndef _DEVFREQ_DEVBW_H
#define _DEVFREQ_DEVBW_H

#include <linux/devfreq.h>

#ifdef CONFIG_QCOM_DEVFREQ_DEVBW
int devfreq_add_devbw(struct device *dev);
int devfreq_remove_devbw(struct device *dev);
int devfreq_suspend_devbw(struct device *dev);
int devfreq_resume_devbw(struct device *dev);
#else
static inline int devfreq_add_devbw(struct device *dev)
{
	return 0;
}
static inline int devfreq_remove_devbw(struct device *dev)
{
	return 0;
}
static inline int devfreq_suspend_devbw(struct device *dev)
{
	return 0;
}
static inline int devfreq_resume_devbw(struct device *dev)
{
	return 0;
}
#endif

#endif /* _DEVFREQ_DEVBW_H */
