// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kthread.h>
#include <linux/vmalloc.h>
#include <linux/semaphore.h>
#include <linux/time.h>
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

