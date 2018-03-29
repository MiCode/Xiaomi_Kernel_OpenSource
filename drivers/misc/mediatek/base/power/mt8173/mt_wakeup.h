/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MT_WAKEUP_
#define _MT_WAKEUP_

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/device.h>
/* #include <mach/irqs.h> */

/* Ordered from most occurences to least */
typedef enum {
	WEV_NONE = -1,
	WEV_RTC,
	WEV_WIFI,
	WEV_WAN,
	WEV_USB,
	WEV_PWR,
	WEV_HALL,
	WEV_BT,
	WEV_CHARGER,
	WEV_MAX,
	WEV_TOTAL = 20
} wakeup_event_t;

/**
 * struct wakeup_event - Representation of a resume event
 *
 * @ev: Event responsible for pm_resume
 * @irq: Irq responsible for pm_resume.
 * @ev_count: Total number of times the event cause pm_resume.
 * @last_time: Last time the event was reported
 * @total_time: Total time between pm_resume triggered by this event to
 *		the following successful pm_suspend
 */

struct wakeup_event {
	const char *name;
	wakeup_event_t event;
	int irq;
	unsigned long count;
	ktime_t last_time;
	ktime_t	total_time;
};

extern void pm_report_resume_irq(int irq);
extern void pm_report_resume_ev(wakeup_event_t ev, int irq);
extern wakeup_event_t pm_get_resume_ev(ktime_t *ts);
#endif
