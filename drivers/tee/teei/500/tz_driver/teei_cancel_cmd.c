// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include "teei_cancel_cmd.h"
#include "teei_id.h"
#include "sched_status.h"
#include "nt_smc_call.h"
#include "teei_log.h"
#include "teei_common.h"
#include "utdriver_macro.h"
#include "switch_queue.h"
#include "teei_client_main.h"
#include "backward_driver.h"
#include <fdrv.h>

#define IMSG_TAG "[tz_driver]"
#include <imsg_log.h>
#include <teei_secure_api.h>

struct cancel_command_struct {
	unsigned long mem_size;
	int retVal;
};

struct cancel_command_struct cancel_command_entry;
unsigned long cancel_message_buff;

int send_cancel_command(unsigned long share_memory_size)
{
	struct teei_fdrv fdrv;
	int retVal = 0;

	fdrv.call_type = CANCEL_SYS_NO;
	fdrv.buff_size = share_memory_size;


	retVal = fdrv_notify(&fdrv);
	if (retVal != 0) {
		IMSG_ERROR("[%s][%d] Failed to call the fdrv_notify [%d]!\n",
				__func__, __LINE__, retVal);
		return retVal;
	}

	return 0;
}


unsigned long create_cancel_fdrv(int buff_size)
{
	struct teei_fdrv fdrv;
	long retVal = 0;

	fdrv.call_type = CANCEL_SYS_NO;
	fdrv.buff_size = buff_size;

	retVal = create_fdrv(&fdrv);
	if (retVal != 0) {
		IMSG_ERROR("[%s][%d] TEEI: Failed to call the create_fdrv!\n",
				__func__, __LINE__);
		return 0;
	}

	return (unsigned long)fdrv.buf;
}
