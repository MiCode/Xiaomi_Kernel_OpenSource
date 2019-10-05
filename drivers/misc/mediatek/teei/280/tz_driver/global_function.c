/*
 * Copyright (c) 2015-2017 MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/types.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/freezer.h>
#include <linux/semaphore.h>

#include "nt_smc_call.h"
#include "global_function.h"
#include "sched_status.h"
#include "utdriver_macro.h"
#include "teei_log.h"
#include "switch_queue.h"

#define IMSG_TAG "[tz_driver]"
#include <imsg_log.h>

void nt_sched_t_call(void)
{
	int retVal = 0;

	retVal = add_work_entry(SCHED_CALL, 0);
	if (retVal != 0)
		IMSG_ERROR("[%s][%d] add_work_entry function failed!\n",
				__func__, __LINE__);
}
