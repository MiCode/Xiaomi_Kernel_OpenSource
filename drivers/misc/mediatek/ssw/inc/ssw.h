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

#ifndef __SSW_H__
#define __SSW_H__
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/delay.h>

#include <linux/kdev_t.h>
#include <linux/ctype.h>
#include <linux/cdev.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#if defined(CONFIG_MTK_LEGACY)
#include <mt-plat/mt_gpio.h>
#endif

#define SSW_DBG(format, args...)    pr_debug("[ccci/ssw]" format, ##args)
/*------------------------Error Code---------------------------------------*/
#define SSW_SUCCESS (0)
#define SSW_INVALID_PARA		(-1)

enum {
	SSW_INVALID = 0xFFFFFFFF,
	SSW_INTERN = 0,
	SSW_EXT_FXLA2203 = 1,
	SSW_EXT_SINGLE_COMMON = 2,
	SSW_EXT_DUAL_1X2 = 3,
	SSW_EXT_SINGLE_2X2 = 4,

	SSW_RESTORE = 0x5AA5,
};

#endif
