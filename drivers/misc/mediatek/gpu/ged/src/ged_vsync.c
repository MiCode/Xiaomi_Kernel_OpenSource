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

#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include "ged_vsync.h"
#include "ged_base.h"
#include "ged_type.h"
#include "primary_display.h"

GED_ERROR ged_vsync_wait()
{
    int ret = 0;
    disp_session_vsync_config vsync_config;
	printk("[vsync] Wait...\n");
    ret = primary_display_wait_for_vsync(&vsync_config);
    if (ret < 0)
    {
        printk("[vsync] fail, ret = %d\n", ret);
        return GED_ERROR_FAIL;
    }
    
    printk("[vsync] success, ret = %d\n", ret);
	return GED_OK;
}