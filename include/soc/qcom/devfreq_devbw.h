/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2014, 2019, The Linux Foundation. All rights reserved.
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
