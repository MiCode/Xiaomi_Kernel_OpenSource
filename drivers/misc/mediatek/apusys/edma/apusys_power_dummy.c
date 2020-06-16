// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/err.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/spinlock_types.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/init.h>
#include <linux/workqueue.h>
#include <linux/sched/clock.h>

#include "apusys_power_user.h"



int apu_device_power_suspend(enum DVFS_USER user, int is_suspend)
{
	return 0;
}

int apu_device_power_off(enum DVFS_USER user)
{
	return 0;
}


int apu_device_power_on(enum DVFS_USER user)
{
		return 0;
}

int apu_power_device_register(enum DVFS_USER user, struct platform_device *pdev)
{

	return 0;
}


bool apusys_power_check(void)
{
	return true;

}

int apu_power_callback_device_register(enum POWER_CALLBACK_USER user,
void (*power_on_callback)(void *para), void (*power_off_callback)(void *para))
{
	return 0;
}

void apu_power_device_unregister(enum DVFS_USER user)
{

}

void apu_power_callback_device_unregister(enum POWER_CALLBACK_USER user)
{

}


