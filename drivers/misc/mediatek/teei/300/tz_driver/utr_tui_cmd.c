/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
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

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <mach/mt_clkmgr.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/cpu.h>
#include "teei_id.h"
#include "sched_status.h"
#include "nt_smc_call.h"
#include "utr_tui_cmd.h"
#include "tpd.h"
#include <linux/leds.h>
#include "teei_common.h"
#include "switch_queue.h"
#include "teei_client_main.h"
#include "backward_driver.h"
#include "utdriver_macro.h"
#include "../teei_fp/fp_func.h"
#include <teei_secure_api.h>

#define IMSG_TAG "[tz_driver]"
#include <imsg_log.h>

unsigned long tui_display_message_buff;
unsigned long tui_notice_message_buff;

unsigned long create_tui_buff(int buff_size, unsigned int fdrv_type)
{
	struct teei_fdrv fdrv;
	long retVal = 0;

	fdrv.call_type = fdrv_type;
	fdrv.buff_size = buff_size;

	retVal = create_fdrv(&fdrv);
	if (retVal != 0) {
		IMSG_ERROR("[%s][%d] TEEI: Failed to call the create_fdrv!\n",
				__func__, __LINE__);
		return 0;
	}

	return fdrv.buf;
}

int send_tui_display_command(unsigned long type)
{
	struct teei_fdrv fdrv;
	int retVal = 0;

	fdrv.call_type = TUI_DISPLAY_SYS_NO;
	fdrv.buff_size = type;

	retVal = fdrv_notify(&fdrv);

	if (retVal != 0) {
		IMSG_ERROR("[%s][%d] Failed to call the fdrv_notify [%d]!\n",
				__func__, __LINE__, retVal);
		return retVal;
	}

	return 0;
}


int send_tui_notice_command(unsigned long share_memory_size)
{
	struct teei_fdrv fdrv;
	int retVal = 0;

	fdrv.call_type = TUI_NOTICE_SYS_NO;
	fdrv.buff_size = share_memory_size;

	retVal = fdrv_notify(&fdrv);

	if (retVal != 0) {
		IMSG_ERROR("[%s][%d] Failed to call the fdrv_notify [%d]!\n",
				__func__, __LINE__, retVal);
		return retVal;
	}

	return 0;
}


/* TODO */
int send_power_down_cmd(void)
{
	int retVal = 0;

	return retVal;
}

int wait_for_power_down(void)
{
	struct sched_param param = { .sched_priority = 4 };

	sched_setscheduler(current, SCHED_RR, &param);

	do {
		set_current_state(TASK_INTERRUPTIBLE);
		if ((power_down_flag == 1) && (enter_tui_flag)) {
			mtkfb_set_backlight_level(0);
			send_power_down_cmd();
			IMSG_DEBUG("[%s][%d]catch a power_down_flag!!!\n",
						__func__, __LINE__);
		}
		power_down_flag = 0;
		msleep_interruptible(30);
		set_current_state(TASK_RUNNING);
	} while (!kthread_should_stop());

	return 0;
}

int tui_notify_reboot(struct notifier_block *this, unsigned long code, void *x)
{
	if ((code == SYS_RESTART) && enter_tui_flag) {
		mtkfb_set_backlight_level(0);
		send_power_down_cmd();
		IMSG_DEBUG("[%s][%d]catch a ree_reboot_signal!!!\n",
						__func__, __LINE__);
	}
	return NOTIFY_OK;
}
