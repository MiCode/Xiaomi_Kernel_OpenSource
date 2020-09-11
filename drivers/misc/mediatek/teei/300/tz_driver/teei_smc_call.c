/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * Copyright (C) 2020 XiaoMi, Inc.
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
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include <linux/completion.h>
#include "teei_id.h"
#include "teei_common.h"
#include "teei_smc_call.h"
#include "nt_smc_call.h"
#include "utdriver_macro.h"
#include "teei_log.h"
#include "notify_queue.h"
#include "switch_queue.h"
#include "teei_client_main.h"
#include "teei_smc_struct.h"
#include "sched_status.h"
#include <teei_secure_api.h>
#include <linux/sched/clock.h>
#include <linux/vmalloc.h>

#define IMSG_TAG "[tz_driver]"
#include <imsg_log.h>

struct comp_link_struct {
	struct list_head c_link;
	unsigned long long lock_p;
	int pid;
};

DEFINE_MUTEX(g_comp_link_lock);

static int teei_smc(void)
{
	unsigned long long smc_type = 2;

	smc_type = teei_secure_call(N_INVOKE_T_NQ, 0, 0, 0);
	while (smc_type == SMC_CALL_INTERRUPTED_IRQ)
		smc_type = teei_secure_call(NT_SCHED_T, 0, 0, 0);

	return 0;
}

void teei_handle_nq_call(struct NQ_entry *entry)
{

	struct completion *bp = NULL;

	bp = (struct completion *)(entry->block_p);
	if (bp == NULL) {
		IMSG_ERROR("NO block pointer in the NQ_entry!\n");
		return;
	}

	complete(bp);
}

int handle_notify_queue_call(void)
{
	return teei_smc();
}

int add_comp_to_link(struct completion *comp, int pid)
{
	struct comp_link_struct *entry  = NULL;

	list_for_each_entry(entry, &(g_block_link), c_link) {
		if (entry->pid == pid) {
			entry->lock_p = (unsigned long long)comp;
			return 0;
		}
	}

	entry = vmalloc(sizeof(struct comp_link_struct));
	if (entry == NULL) {
		IMSG_ERROR("TEEI: NO memory for struct comp_link_struct!\n");
		return -ENOMEM;
	}

	memset(entry, 0, sizeof(struct comp_link_struct));

	entry->pid = pid;
	entry->lock_p = (unsigned long long)comp;

	INIT_LIST_HEAD(&(entry->c_link));

	list_add(&(entry->c_link), &(g_block_link));

	return 0;
}

int teei_forward_call(unsigned long long cmd, unsigned long long cmd_addr,
			unsigned long long size)
{
	struct completion *wait_completion = NULL;
	int retVal = 0;

	KATRACE_BEGIN("teei_forward_call");

	wait_completion = kmalloc(sizeof(struct completion), GFP_KERNEL);

	init_completion(wait_completion);

#ifdef TEEI_MUTIL_TA_DEBUG
	mutex_lock(&g_comp_link_lock);
	retVal = add_comp_to_link(wait_completion, current->pid);
	mutex_unlock(&g_comp_link_lock);
#endif

	cpus_read_lock();

	retVal = add_nq_entry(NEW_CAPI_CALL, cmd,
				(unsigned long long)(wait_completion),
				cmd_addr, size, 0);
	if (retVal != 0) {
		IMSG_ERROR("TEEI: Failed to add one nq to n_t_buffer\n");
		cpus_read_unlock();
		kfree(wait_completion);
		KATRACE_END("teei_forward_call");
		return retVal;
	}

	retVal = add_work_entry(INVOKE_NQ_CALL, 0);
	if (retVal != 0) {
		IMSG_ERROR("TEEI: Failed to add_work_entry[%s]\n", __func__);
		cpus_read_unlock();
		kfree(wait_completion);
		KATRACE_END("teei_forward_call");
		return retVal;
	}

	wait_for_completion(wait_completion);

	cpus_read_unlock();

	kfree(wait_completion);
	KATRACE_END("teei_forward_call");

	return 0;
}
EXPORT_SYMBOL(teei_forward_call);
