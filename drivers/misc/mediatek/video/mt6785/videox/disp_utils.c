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

#include <linux/kthread.h>
#include <linux/vmalloc.h>
#include <linux/semaphore.h>
#include <linux/time.h>
#ifdef CONFIG_MTK_M4U
#include "m4u.h"
#endif
#include <linux/delay.h>
#include <linux/slab.h>
#include "disp_drv_log.h"
#include "disp_utils.h"

int disp_sw_mutex_lock(struct mutex *m)
{
	mutex_lock(m);
	return 0;
}

int disp_mutex_trylock(struct mutex *m)
{
	int ret = 0;

	ret = mutex_trylock(m);
	return ret;
}

int disp_sw_mutex_unlock(struct mutex *m)
{
	mutex_unlock(m);
	return 0;
}

int disp_msleep(unsigned int ms)
{
	msleep(ms);
	return 0;
}

long int disp_get_time_us(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return (t.tv_sec & 0xFFF) * 1000000 + t.tv_usec;
}
