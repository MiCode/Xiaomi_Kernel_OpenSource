/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 *  boot logger: drivers/misc/mtprof/bootprof
 * interface: /proc/bootprof
 */
#ifndef _BOOTPROF_H_
#define _BOOTPROF_H_

#if IS_BUILTIN(CONFIG_MTPROF)
#include <linux/platform_device.h>
#include <linux/sched/clock.h>

#ifndef BOOTPROF_TIME_LOG_START
#define BOOTPROF_TIME_LOG_START(ts) \
	({ts = sched_clock(); })
#endif

#ifndef BOOTPROF_TIME_LOG_END
#define BOOTPROF_TIME_LOG_END(ts) \
	({ts = sched_clock() - ts; })
#endif

void bootprof_initcall(initcall_t fn, unsigned long long ts);
void bootprof_probe(unsigned long long ts, struct device *dev,
		    struct device_driver *drv, unsigned long probe);
void bootprof_pdev_register(unsigned long long ts,
			    struct platform_device *pdev);

extern void bootprof_log_boot(char *str);

#else /*CONFIG_MTPROF*/
#define BOOTPROF_TIME_LOG_START(ts)
#define BOOTPROF_TIME_LOG_END(ts)
#define bootprof_probe(ts, dev, drv, probe)
#define bootprof_pdev_register(ts, pdev)
#define bootprof_initcall(fn, ts)
#define bootprof_log_boot(str)
#endif /*CONFIG_MTPROF*/

#endif /*_BOOTPROF_H_*/
