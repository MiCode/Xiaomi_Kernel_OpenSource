// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#include <linux/kernel.h>
#include <linux/completion.h>
#include <linux/vmalloc.h>

#include <linux/cpu.h>
#include <linux/slab.h>
#include "teei_smc_call.h"
#include "utdriver_macro.h"
#include "notify_queue.h"
#include "switch_queue.h"
#include "teei_common.h"
#include "nt_smc_call.h"
#include "teei_client_main.h"

#define IMSG_TAG "[tz_driver]"
#include <imsg_log.h>
int teei_forward_call(unsigned long long cmd, unsigned long long cmd_addr,
				unsigned long long size)
{
	struct completion *wait_completion = NULL;
	int retVal = 0;

	KATRACE_BEGIN("teei_forward_call");

	teei_cpus_read_lock();

	wait_completion = kmalloc(sizeof(struct completion), GFP_KERNEL);
	if (wait_completion == NULL) {
		IMSG_ERROR("TEEI: Failed to alloc completion[%s]\n", __func__);
		teei_cpus_read_unlock();
		return -ENOMEM;
	}

	init_completion(wait_completion);

	retVal = add_work_entry(SMC_CALL_TYPE, N_INVOKE_T_NQ, 0, 0, 0);
	if (retVal != 0) {
		IMSG_ERROR("TEEI: Failed to add_work_entry[%s]\n", __func__);
		kfree(wait_completion);
		teei_cpus_read_unlock();
		KATRACE_END("teei_forward_call");
		return retVal;
	}

	retVal = add_nq_entry(NEW_CAPI_CALL, cmd,
				(unsigned long long)(wait_completion),
				cmd_addr, size, 0);
	if (retVal != 0) {
		IMSG_ERROR("TEEI: Failed to add one nq to n_t_buffer\n");
		kfree(wait_completion);
		teei_cpus_read_unlock();
		KATRACE_END("teei_forward_call");
		return retVal;
	}

	teei_notify_switch_fn();

	wait_for_completion(wait_completion);

	kfree(wait_completion);

	teei_cpus_read_unlock();

	KATRACE_END("teei_forward_call");

	return 0;
}
EXPORT_SYMBOL(teei_forward_call);
