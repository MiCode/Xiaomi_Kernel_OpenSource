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
#include "nt_smc_call.h"
#include "utdriver_macro.h"
#include "sched_status.h"
#include "teei_log.h"
#include "teei_common.h"
#include "teei_client_main.h"
#include "global_function.h"
#include "backward_driver.h"
#include "irq_register.h"
#include "teei_smc_call.h"
#include "teei_cancel_cmd.h"
#ifdef TUI_SUPPORT
#include "utr_tui_cmd.h"
#endif

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
	struct ut_smc_call_work usc_work = {
		KTHREAD_WORK_INIT(usc_work.work, switch_fn),
		.data = buff,
	};

	if (!kthread_queue_work(&ut_fastcall_worker, &usc_work.work))
		return -1;
	kthread_flush_work(&usc_work.work);
	return 0;
}

static int check_work_type(int work_type)
{
	switch (work_type) {
	case NEW_CAPI_CALL:
	case CAPI_CALL:
	case FDRV_CALL:
	case BDRV_CALL:
	case SCHED_CALL:
	case LOAD_FUNC:
	case INIT_CMD_CALL:
	case BOOT_STAGE1:
	case BOOT_STAGE2:
	case INVOKE_FASTCALL:
	case LOAD_TEE:
	case LOCK_PM_MUTEX:
	case UNLOCK_PM_MUTEX:
	case SWITCH_CORE:
	case NT_DUMP_T:
#ifdef TUI_SUPPORT
	case POWER_DOWN_CALL:
#endif
		return 0;
	default:
		return -EINVAL;
	}
}

int handle_dump_call(void *buff)
{
	IMSG_DEBUG("[%s][%d] handle_dump_call begin.\n", __func__, __LINE__);
	teei_secure_call(NT_SCHED_T, 0x9527, 0, 0);
	IMSG_DEBUG("[%s][%d] handle_dump_call end.\n", __func__, __LINE__);
	return 0;
}

void handle_lock_pm_mutex(struct mutex *lock)
{
	if (ut_pm_count == 0)
		mutex_lock(lock);

	ut_pm_count++;
}


void handle_unlock_pm_mutex(struct mutex *lock)
{
	ut_pm_count--;

	if (ut_pm_count == 0)
		mutex_unlock(lock);
}

int add_work_entry(int work_type, unsigned long buff)
{
	struct switch_call_struct *work_entry = NULL;
	int retVal = 0;

	retVal = check_work_type(work_type);
	if (retVal != 0) {
		IMSG_ERROR("[%s][%d] with wrong work_type!\n",
						__func__, __LINE__);
		return retVal;
	}

	work_entry = create_switch_call_struct();
	if (work_entry == NULL) {
		IMSG_ERROR("[%s][%d] There is no enough memory!\n",
							__func__, __LINE__);
		return -ENOMEM;
	}

	retVal = init_switch_call_struct(work_entry, work_type, buff);
	if (retVal != 0) {
		IMSG_ERROR("[%s][%d] init_switch_call_struct failed!\n",
							__func__, __LINE__);
		destroy_switch_call_struct(work_entry);
		return retVal;
	}

	retVal = ut_smc_call((void *)work_entry);

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


int handle_capi_call(void *buff)
{
	struct smc_call_struct *cd = NULL;

	cd = (struct smc_call_struct *)buff;

	/* with a rmb() */
	rmb();

	cd->retVal = __teei_smc_call(cd->local_cmd,
			cd->teei_cmd_type,
			cd->dev_file_id,
			cd->svc_id,
			cd->cmd_id,
			cd->context,
			cd->enc_id,
			cd->cmd_buf,
			cd->cmd_len,
			cd->resp_buf,
			cd->resp_len,
			cd->meta_data,
			cd->info_data,
			cd->info_len,
			cd->ret_resp_len,
			cd->error_code,
			cd->psema);

	/* with a wmb() */
	wmb();

	return 0;
}

int handle_fdrv_call(void *buff)
{
	struct fdrv_call_struct *cd = NULL;

	cd = (struct fdrv_call_struct *)buff;

	/* with a rmb() */
	rmb();

	switch (cd->fdrv_call_type) {
	case CANCEL_SYS_NO:
		cd->retVal = __send_cancel_command(cd->fdrv_call_buff_size);
		break;
#ifdef TUI_SUPPORT
	case TUI_DISPLAY_SYS_NO:
		cd->retVal = __send_tui_display_command(
						cd->fdrv_call_buff_size);
		break;
	case TUI_NOTICE_SYS_NO:
		cd->retVal = __send_tui_notice_command(cd->fdrv_call_buff_size);
		break;
#endif
	default:
		cd->retVal = __call_fdrv(cd);
	}

	/* with a wmb() */
	wmb();

	return 0;
}

struct bdrv_call_struct {
	int bdrv_call_type;
	struct service_handler *handler;
	int retVal;
};

int handle_bdrv_call(void *buff)
{
	struct bdrv_call_struct *cd = NULL;

	cd = (struct bdrv_call_struct *)buff;

	/* with a rmb() */
	rmb();

	switch (cd->bdrv_call_type) {
	case VFS_SYS_NO:
		cd->retVal = __vfs_handle(cd->handler);
		kfree(buff);
		break;
	case REETIME_SYS_NO:
		cd->retVal = __reetime_handle(cd->handler);
		kfree(buff);
		break;
	default:
		cd->retVal = -EINVAL;
	}

	/* with a wmb() */
	wmb();

	return 0;
}

int handle_switch_call(void *buff)
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
	case BOOT_STAGE2:
		secondary_boot_stage2(NULL);
		break;
	case INVOKE_FASTCALL:
		secondary_invoke_fastcall(NULL);
		break;
	case LOAD_TEE:
		secondary_load_tee(NULL);
		break;
	case CAPI_CALL:
		retVal = handle_capi_call((void *)(switch_ent->buff_addr));
		if (retVal < 0)
			IMSG_ERROR("[%s][%d] fail to handle ClientAPI!\n",
							__func__, __LINE__);
		break;
	case NEW_CAPI_CALL:
		retVal = handle_new_capi_call((void *)(switch_ent->buff_addr));
		if (retVal < 0)
			IMSG_ERROR("[%s][%d] fail to handle new ClientAPI!\n",
							__func__, __LINE__);
		break;
	case FDRV_CALL:
		retVal = handle_fdrv_call((void *)(switch_ent->buff_addr));
		if (retVal < 0)
			IMSG_ERROR("[%s][%d] fail to handle F-driver!\n",
							__func__, __LINE__);
		break;
	case BDRV_CALL:
		retVal = handle_bdrv_call((void *)(switch_ent->buff_addr));
		if (retVal < 0)
			IMSG_ERROR("[%s][%d] fail to handle B-driver!\n",
							__func__, __LINE__);
		break;
	case SCHED_CALL:
		retVal = handle_sched_call((void *)(switch_ent->buff_addr));
		if (retVal < 0)
			IMSG_ERROR("[%s][%d] fail to handle sched-Call!\n",
							__func__, __LINE__);
		break;
	case LOCK_PM_MUTEX:
		handle_lock_pm_mutex((struct mutex *)(switch_ent->buff_addr));
		break;
	case UNLOCK_PM_MUTEX:
		handle_unlock_pm_mutex((struct mutex *)(switch_ent->buff_addr));
		break;
#ifdef TUI_SUPPORT
	case POWER_DOWN_CALL:
		retVal = handler_power_down_call(
				(void *)(switch_ent->buff_addr));
		if (retVal < 0)
			IMSG_ERROR("[%s][%d] fail to handle power_down-Call!\n",
							__func__, __LINE__);
		break;
#endif
	case SWITCH_CORE:
		handle_switch_core((int)(switch_ent->buff_addr));
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

	IMSG_DEBUG("%s: Dump TZ_LOG, call_type 0x%x\n", __func__, call_type);
	atomic_notifier_call_chain(&s->notifier, TZ_CALL_RETURNED, NULL);
	retVal = destroy_switch_call_struct(switch_ent);
	if (retVal != 0)
		IMSG_ERROR("[%s][%d] destroy_switch_call_struct failed %d!\n",
						__func__, __LINE__, retVal);
}
