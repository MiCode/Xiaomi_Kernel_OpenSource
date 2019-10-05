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

/*
 *  boot logger: drivers/misc/mtprof/bootprof
 * interface: /proc/bootprof
 */
#ifndef _BOOTPROF_H_
#define _BOOTPROF_H_
#ifdef CONFIG_SCHEDSTATS
extern void log_boot(char *str);
#else
#define log_boot(str)
#endif

//#include <linux/sched.h>
//#include <linux/sched_clock.h>
#include <linux/sched/clock.h>

#ifndef TIME_LOG_START
#define TIME_LOG_START() \
	({ts = sched_clock(); })
#endif

#ifndef TIME_LOG_END
#define TIME_LOG_END() \
	({ts = sched_clock() - ts; })
#endif

#include <linux/platform_device.h>
void bootprof_initcall(initcall_t fn, unsigned long long ts);
void bootprof_probe(unsigned long long ts, struct device *dev,
		    struct device_driver *drv, unsigned long probe);
void bootprof_pdev_register(unsigned long long ts,
			    struct platform_device *pdev);
#endif
