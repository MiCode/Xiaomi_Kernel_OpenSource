// SPDX-License-Identifier: GPL-2.0-only
/*
 * FTS Capacitive touch screen controller (FingerTipS)
 *
 * Copyright (C) 2016-2019, STMicroelectronics Limited.
 * Authors: AMG(Analog Mems Group) <marco.cali@st.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 *
 **************************************************************************
 **                        STMicroelectronics                            **
 **************************************************************************
 **                        marco.cali@st.com                             **
 **************************************************************************
 *                                                                        *
 *                  FTS Utility for mesuring/handling the time            *
 *                                                                        *
 **************************************************************************
 ***************************************************************************
 */

#include <linux/init.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <stdarg.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/serio.h>
#include <linux/time.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/power_supply.h>
#include <linux/firmware.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
//#include <linux/sec_sysfs.h>

#include "ftsCrossCompile.h"
#include "ftsTime.h"


void startStopWatch(struct StopWatch *w)
{
	w->start = current_kernel_time();
}

void stopStopWatch(struct StopWatch *w)
{
	w->end = current_kernel_time();
}

int elapsedMillisecond(struct StopWatch *w)
{
	int result;

	result = ((w->end.tv_sec - w->start.tv_sec) * 1000)
		+ (w->end.tv_nsec - w->start.tv_nsec) / 1000000;
	return result;
}

int elapsedNanosecond(struct StopWatch *w)
{
	int result;

	result = ((w->end.tv_sec - w->start.tv_sec) * 1000000000)
		+ (w->end.tv_nsec - w->start.tv_nsec);
	return result;
}

char *timestamp()
{
	char *result = NULL;

	result = (char *)kmalloc_array(1, sizeof(char), GFP_KERNEL);
	if (result == NULL)
		return NULL;
	result[0] = ' ';
	return result;
}

void stdelay(unsigned long ms)
{
	msleep(ms);
}
