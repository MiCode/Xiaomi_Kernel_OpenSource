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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/freezer.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include <linux/notifier.h>
#include <fdrv.h>
#include <linux/vmalloc.h>
#include "nt_smc_call.h"
#include "utdriver_macro.h"
#include "sched_status.h"
#include "teei_log.h"
#include "teei_common.h"
#include "teei_client_main.h"
#include "backward_driver.h"
#include "irq_register.h"
#include "teei_smc_call.h"
#include "teei_cancel_cmd.h"
#ifdef TUI_SUPPORT
#include "utr_tui_cmd.h"
#endif

#include <notify_queue.h>
#include <teei_secure_api.h>

#define IMSG_TAG "[tz_driver]"
#include <imsg_log.h>

struct switch_head_struct {
	struct list_head head;
};

struct switch_call_struct {
	int switch_type;
	unsigned long buff_addr;
};

static void switch_fn(struct kthread_work *work);

static struct switch_call_struct *create_switch_call_struct(void)
{
	struct switch_call_struct *tmp_entry = NULL;

	tmp_entry = kmalloc(sizeof(struct switch_call_struct), GFP_KERNEL);

	if (tmp_entry == NULL)
		IMSG_ERROR("[%s][%d] kmalloc failed!!!\n", __func__, __LINE__);

	return tmp_entry;
}

static int init_switch_call_struct(struct switch_call_struct *ent,
				int work_type, unsigned long buff)
{
	if (ent == NULL) {
		IMSG_ERROR("[%s][%d] the paraments are wrong!\n",
						__func__, __LINE__);
		return -EINVAL;
	}

	ent->switch_type = work_type;
	ent->buff_addr = buff;

	return 0;
}

static int destroy_switch_call_struct(struct switch_call_struct *ent)
{
	kfree(ent);

	return 0;
}

struct ut_smc_call_work {
	struct kthread_work work;
	void *data;
};

static int ut_smc_call(void *buff)
{
	struct ut_smc_call_work *usc_work = NULL;

	usc_work = kmalloc(sizeof(struct ut_smc_call_work), GFP_KERNEL);

	if (usc_work == NULL) {
		IMSG_ERROR("[%s][%d] NO enough memory for use_work\n",
				__func__, __LINE__);
		return -ENOMEM;
	}

#if KERNEL_VERSION(4, 9, 0) <= LINUX_VERSION_CODE
	kthread_init_work(&(usc_work->work), switch_fn);
#else
	init_kthread_work(&(usc_work->work), switch_fn);
#endif
	usc_work->data = buff;


#if KERNEL_VERSION(4, 9, 0) <= LINUX_VERSION_CODE
	if (!kthread_queue_work(&ut_fastcall_worker, &usc_work->work))
#else
	if (!queue_kthread_work(&ut_fastcall_worker, &usc_work->work))
#endif
		return -1;

	return 0;
}

static int check_work_type(int work_type)
{
	return 0;
}

int handle_dump_call(void *buff)
{
	IMSG_DEBUG("[%s][%d] begin.\n", __func__, __LINE__);
	teei_secure_call(NT_SCHED_T, 0x9527, 0, 0);
	IMSG_DEBUG("[%s][%d] end.\n", __func__, __LINE__);

	return 0;
}


int add_work_entry(int work_type, unsigned long buff)
{
	struct switch_call_struct *work_entry = NULL;
	int retVal = 0;

	/* with a wmb() */
	/* wmb(); */

	retVal = check_work_type(work_type);
	if (retVal != 0) {
		IMSG_ERROR("[%s][%d] with wrong work_type!\n",
						__func__, __LINE__);
		return -EINVAL;
	}

	work_entry = create_switch_call_struct();
	if (work_entry == NULL) {
		IMSG_ERROR("[%s][%d] There is no enough memory!\n",
							__func__, __LINE__);
		return -EINVAL;
	}

	retVal = init_switch_call_struct(work_entry, work_type, buff);
	if (retVal != 0) {
		IMSG_ERROR("[%s][%d] init_switch_call_struct failed!\n",
							__func__, __LINE__);
		destroy_switch_call_struct(work_entry);
		return retVal;
	}

	retVal = ut_smc_call((void *)work_entry);

	/* with a rmb() */
	/* rmb(); */

	return retVal;
}

int get_call_type(struct switch_call_struct *ent)
{
	if (ent == NULL)
		return -EINVAL;
	return ent->switch_type;
}

int handle_sched_call(void *buff)
{
	unsigned long smc_type = 2;

	smc_type = teei_secure_call(NT_SCHED_T, 0, 0, 0);
	while (smc_type == SMC_CALL_INTERRUPTED_IRQ)
		smc_type = teei_secure_call(NT_SCHED_T, 0, 0, 0);

	return 0;
}


#ifdef TUI_SUPPORT
int handler_power_down_call(void *buff)
{
	unsigned long smc_type = 5;

	smc_type = teei_secure_call(NT_CANCEL_T_TUI, 0, 0, 0);
	while (smc_type == SMC_CALL_INTERRUPTED_IRQ)
		smc_type = teei_secure_call(NT_SCHED_T, 0, 0, 0);

	return 0;
}
#endif

static void switch_fn(struct kthread_work *work)
{
	struct ut_smc_call_work *switch_work = NULL;
	struct switch_call_struct *switch_ent = NULL;
	int call_type = 0;
	int retVal = 0;
	struct tz_driver_state *s = get_tz_drv_state();

	KATRACE_BEGIN("teei_switch_fn");

	switch_work = container_of(work, struct ut_smc_call_work, work);

	switch_ent = (struct switch_call_struct *)switch_work->data;

	call_type = get_call_type(switch_ent);

	switch (call_type) {
	case LOAD_FUNC:
		secondary_load_func();
		break;
	case BOOT_STAGE1:
		secondary_boot_stage1((void *)(switch_ent->buff_addr));
		break;
	case INIT_CMD_CALL:
		secondary_init_cmdbuf((void *)(switch_ent->buff_addr));
		break;
	case INVOKE_NQ_CALL:
		retVal = handle_notify_queue_call();
		if (retVal < 0)
			IMSG_ERROR("[%s][%d] fail to handle new ClientAPI!\n",
							__func__, __LINE__);
		break;

	case SCHED_CALL:
		retVal = handle_sched_call((void *)(switch_ent->buff_addr));
		if (retVal < 0)
			IMSG_ERROR("[%s][%d] fail to handle sched-Call!\n",
							__func__, __LINE__);
		break;

	case SWITCH_CORE:
		handle_switch_core((int)(switch_ent->buff_addr));
		break;
	case MOVE_CORE:
		handle_move_core((int)(switch_ent->buff_addr));
		break;
	case NT_DUMP_T:
		retVal = handle_dump_call((void *)(switch_ent->buff_addr));
		if (retVal < 0)
			IMSG_ERROR("[%s][%d] fail to handle dump-Call!\n",
							__func__, __LINE__);
		break;
	default:
		IMSG_ERROR("switch fn handles a undefined call!\n");
		break;
	}

	if (call_type != SWITCH_CORE)
		atomic_notifier_call_chain(&s->notifier,
					TZ_CALL_RETURNED, NULL);

	retVal = destroy_switch_call_struct(switch_ent);
	if (retVal != 0) {
		IMSG_ERROR("[%s][%d] destroy_switch_call_struct failed %d!\n",
						__func__, __LINE__, retVal);
	}

	kfree(switch_work);

	KATRACE_END("teei_switch_fn");
}
