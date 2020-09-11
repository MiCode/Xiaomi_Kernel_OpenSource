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

#include <linux/irq.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/freezer.h>
#include <linux/semaphore.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/cpu.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "teei_id.h"
#include <teei_secure_api.h>
#include <backward_driver.h>
#include <nt_smc_call.h>
#include <teei_client_main.h>
#include <utdriver_macro.h>
#include <utdriver_irq.h>
#include <switch_queue.h>
#include <notify_queue.h>
#include <fdrv.h>

#define IMSG_TAG "[tz_driver]"
#include <imsg_log.h>

struct work_entry {
	void *param_p;
	struct work_struct work;
};

struct load_soter_entry {
	unsigned long vfs_addr;
	struct work_struct work;
};

struct comp_link_struct {
	struct list_head c_link;
	unsigned long long lock_p;
	int pid;
};

static struct load_soter_entry load_ent;

void sched_func(struct work_struct *entry)
{
	struct work_entry *sched_work_ent = NULL;
	int retVal = 0;

	sched_work_ent = container_of(entry, struct work_entry, work);

	kfree(sched_work_ent);

	retVal = add_work_entry(SCHED_CALL, 0);
	if (retVal != 0)
		IMSG_ERROR("[%s][%d] add_work_entry function failed!\n",
				__func__, __LINE__);
}

void add_sched_queue(void)
{
	struct work_entry *sched_work_ent = NULL;

	sched_work_ent = kmalloc(sizeof(struct work_entry), GFP_KERNEL);
	if (sched_work_ent == NULL) {
		IMSG_ERROR("TEEI: Failed to malloc sched_work_entry\n");
		return;
	}

	INIT_WORK(&(sched_work_ent->work), sched_func);
	queue_work(secure_wq, &(sched_work_ent->work));
}

static irqreturn_t nt_sched_irq_handler(void)
{
	add_sched_queue();

	return IRQ_HANDLED;
}



static irqreturn_t nt_error_irq_handler(void)
{
	unsigned long error_num = 0;

	error_num = teei_secure_call(N_GET_SE_OS_STATE, 0, 0, 0);
	IMSG_ERROR("secure system ERROR ! error_num = %ld\n",
					(error_num));
	soter_error_flag = 1;

	up(&(boot_sema));

	WARN_ON(1);

	return IRQ_HANDLED;
}


static irqreturn_t nt_boot_irq_handler(void)
{
	if (boot_soter_flag == START_STATUS)
		boot_soter_flag = END_STATUS;

	up(&(boot_sema));

	return IRQ_HANDLED;
}

void secondary_load_func(void)
{
	unsigned long smc_type = 2;

	Flush_Dcache_By_Area((unsigned long)boot_vfs_addr,
			(unsigned long)boot_vfs_addr + VFS_SIZE);

	smc_type = teei_secure_call(N_ACK_T_LOAD_IMG, 0, 0, 0);
	while (smc_type == SMC_CALL_INTERRUPTED_IRQ)
		smc_type = teei_secure_call(NT_SCHED_T, 0, 0, 0);

}


void load_func(struct work_struct *entry)
{
	int retVal = 0;

	vfs_thread_function(boot_vfs_addr, 0, 0);
	retVal = add_work_entry(LOAD_FUNC, 0);
}



void bdrv_work_func(struct work_struct *entry)
{
	struct work_entry *md = container_of(entry, struct work_entry, work);
	struct NQ_entry *NQ_entry_p = md->param_p;

	if (NQ_entry_p->sub_cmd_ID == reetime.sysno)
		reetime.handle(NQ_entry_p);
	else if (NQ_entry_p->sub_cmd_ID == vfs_handler.sysno)
		vfs_handler.handle(NQ_entry_p);

	kfree(NQ_entry_p);
	kfree(md);
}


void teei_handle_bdrv_call(struct NQ_entry *entry)
{
	struct work_entry *work_ent = NULL;
	struct NQ_entry *bdrv_ent = NULL;

	work_ent = kmalloc(sizeof(struct work_entry), GFP_KERNEL);
	if (work_ent == NULL) {
		IMSG_ERROR("NO enough memory for work in %s!\n", __func__);
		return;
	}

	bdrv_ent = kmalloc(sizeof(struct NQ_entry), GFP_KERNEL);
	if (bdrv_ent == NULL) {
		IMSG_ERROR("NO enough memory for bdrv in %s!\n", __func__);
		kfree(work_ent);
		return;
	}

	memcpy(bdrv_ent, entry, sizeof(struct NQ_entry));

	work_ent->param_p = bdrv_ent;

	INIT_WORK(&(work_ent->work), bdrv_work_func);
	queue_work(secure_wq, &(work_ent->work));

}

static int teei_schedule_handle(struct NQ_entry *entry)
{
	unsigned long long block_p = 0;
	int retVal = 0;

	block_p = entry->block_p;

	retVal = add_nq_entry(TEEI_SCHED_CALL, 0, block_p, 0, 0, 0);
	if (retVal != 0)
		IMSG_ERROR("TEEI: Failed to add_nq_entry[%s]\n", __func__);

	retVal = add_work_entry(INVOKE_NQ_CALL, 0);
	if (retVal != 0)
		IMSG_ERROR("TEEI: Failed to add_work_entry[%s]\n", __func__);

	return retVal;
}


void schedule_work_func(struct work_struct *entry)
{
	struct work_entry *md = container_of(entry, struct work_entry, work);
	struct NQ_entry *NQ_entry_p = md->param_p;

	teei_schedule_handle(NQ_entry_p);

	kfree(md);
}


void teei_handle_schedule_call(struct NQ_entry *entry)
{
	struct work_entry *work_ent = NULL;

	work_ent = kmalloc(sizeof(struct work_entry), GFP_KERNEL);
	if (work_ent == NULL) {
		IMSG_ERROR("NO enough memory for work in %s!\n", __func__);
		return;
	}

	work_ent->param_p = entry;
	INIT_WORK(&(work_ent->work), schedule_work_func);
	queue_work(secure_wq, &(work_ent->work));

}

void teei_handle_capi_call(struct NQ_entry *entry)
{
	struct completion *bp = NULL;

#ifdef TEEI_MUTIL_TA_DEBUG
	struct comp_link_struct *link_ent  = NULL;
	int block_got_flag = 0;

	list_for_each_entry(link_ent, &(g_block_link), c_link) {
		if (link_ent->lock_p == entry->block_p) {
			block_got_flag = 1;
			link_ent->lock_p = 0;
			break;
		}
	}

	if (block_got_flag == 0) {
		IMSG_PRINTK("TEEI NOT FOUND the block_p %llu\n",
							entry->block_p);
		IMSG_PRINTK("TEEI Show the lock point link list\n");

		list_for_each_entry(link_ent, &(g_block_link), c_link) {
			IMSG_PRINTK("link entry pid = [%d] lock_p = [%llu]\n",
				link_ent->pid, link_ent->lock_p);
		}

		show_t_nt_queue();
	}
#endif

	bp = (struct completion *)(entry->block_p);
	if (bp == NULL) {
		IMSG_ERROR("The block_p of entry is NULL!\n");
		return;
	}

	complete(bp);
}

static int nt_switch_irq_handler(void)
{

	if (boot_soter_flag == START_STATUS) {
		INIT_WORK(&(load_ent.work), load_func);
		queue_work(secure_wq, &(load_ent.work));

		return 0;

	} else {
		struct NQ_entry *entry = NULL;
		unsigned long long cmd_id = 0;

		entry = get_nq_entry();
		if (entry == NULL) {
			IMSG_ERROR("Can NOT get entry from t_nt_buffer!\n");
			return 0;
		}

		cmd_id = entry->cmd_ID;

		switch (cmd_id) {
		case TEEI_CREAT_FDRV:
		case TEEI_CREAT_BDRV:
		case TEEI_LOAD_TEE:
			up(&boot_sema);
			break;
		case TEEI_FDRV_CALL:
			teei_handle_fdrv_call(entry);
			break;
		case NEW_CAPI_CALL:
			teei_handle_capi_call(entry);
			break;
		case TEEI_BDRV_CALL:
			teei_handle_bdrv_call(entry);
			break;
		case TEEI_SCHED_CALL:
			teei_handle_schedule_call(entry);
			break;
#ifdef TUI_SUPPORT
		case TUI_NOTICE_SYS_NO:
			up(&(tui_notify_sema));
			break;
#endif
		default:
			IMSG_ERROR("[%s][%d] Unknown command ID!\n",
						__func__, __LINE__);
		}

		return 0;
	}
}

static int ut_smc_handler(struct notifier_block *nb,
				unsigned long action, void *data)
{
	int irq_id = 0;
	int retVal = 0;
	/* struct tz_driver_state *s = get_tz_drv_state(); */
	if (action != TZ_CALL_RETURNED)
		return NOTIFY_DONE;

	/* Get the interrupt ID */
	irq_id = teei_secure_call(N_GET_NON_IRQ_NUM, 0, 0, 0);

	switch (irq_id) {
	case SCHED_IRQ:
		retVal = nt_sched_irq_handler();
		break;
	case SWITCH_IRQ:
		retVal = nt_switch_irq_handler();
		break;
	case SOTER_ERROR_IRQ:
		retVal = nt_error_irq_handler();
		break;
	case BOOT_IRQ:
		retVal = nt_boot_irq_handler();
		break;
	default:
		retVal = -EINVAL;
		IMSG_ERROR("get undefine IRQ from secure OS!\n");
	}

	return retVal;
}

int register_ut_smc_handler(struct notifier_block *nb)
{
	int retVal = 0;

	nb->notifier_call = ut_smc_handler;
	retVal = tz_call_notifier_register(nb);
	if (retVal != 0) {
		IMSG_ERROR("Failed to register tz driver call notifier\n");
		return retVal;
	}

	return 0;
}

static irqreturn_t ut_drv_irq_handler(int irq, void *dev)
{
	return IRQ_HANDLED;
}

int register_ut_irq_handler(int irq)
{
	return request_irq(irq, ut_drv_irq_handler,
				0, "tz_drivers_service", NULL);
}


