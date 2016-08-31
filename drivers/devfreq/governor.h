/*
 * governor.h - internal header for devfreq governors.
 *
 * Copyright (C) 2011 Samsung Electronics
 *	MyungJoo Ham <myungjoo.ham@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This header is for devfreq governors in drivers/devfreq/
 */

#ifndef _GOVERNOR_H
#define _GOVERNOR_H

#include <linux/devfreq.h>

#define to_devfreq(DEV)	container_of((DEV), struct devfreq, dev)

/* Devfreq events */
#define DEVFREQ_GOV_START			0x1
#define DEVFREQ_GOV_STOP			0x2
#define DEVFREQ_GOV_INTERVAL			0x3
#define DEVFREQ_GOV_SUSPEND			0x4
#define DEVFREQ_GOV_RESUME			0x5

#if defined(CONFIG_PM_DEVFREQ)

/* Caution: devfreq->lock must be locked before calling update_devfreq */
extern int update_devfreq(struct devfreq *devfreq);

#else /* !CONFIG_PM_DEVFREQ */

static inline int update_devfreq(struct devfreq *devfreq)
{
	return -EINVAL;
}

#endif /* !CONFIG_PM_DEVFREQ */

#if defined(CONFIG_PM_DEVFREQ)

extern void devfreq_monitor_start(struct devfreq *devfreq);
extern void devfreq_monitor_stop(struct devfreq *devfreq);
extern void devfreq_monitor_suspend(struct devfreq *devfreq);
extern void devfreq_monitor_resume(struct devfreq *devfreq);
extern void devfreq_interval_update(struct devfreq *devfreq,
					unsigned int *delay);

extern int devfreq_add_governor(struct devfreq_governor *governor);
extern int devfreq_remove_governor(struct devfreq_governor *governor);

#else /* !CONFIG_PM_DEVFREQ */

static inline void devfreq_monitor_start(struct devfreq *devfreq)
{
}

static inline void devfreq_monitor_stop(struct devfreq *devfreq)
{
}

static inline void devfreq_monitor_suspend(struct devfreq *devfreq)
{
}

static inline void devfreq_monitor_resume(struct devfreq *devfreq)
{
}

static inline void devfreq_interval_update(struct devfreq *devfreq,
					   unsigned int *delay)
{
}

static inline int devfreq_add_governor(struct devfreq_governor *governor)
{
	return 0;
}

static inline int devfreq_remove_governor(struct devfreq_governor *governor)
{
	return 0;
}

#endif /* CONFIG_PM_DEVFREQ */

#endif /* _GOVERNOR_H */
