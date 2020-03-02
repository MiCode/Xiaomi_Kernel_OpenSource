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
#include "teei_id.h"
#include "sched_status.h"
#include "irq_register.h"
#include "nt_smc_call.h"
#include "teei_id.h"
#include "teei_common.h"
#include "teei_log.h"
#include "utdriver_macro.h"
#include "notify_queue.h"
#include "switch_queue.h"
#include "teei_client_main.h"
#include "utdriver_irq.h"
#include "notify_queue.h"
#include "global_function.h"
#include <teei_secure_api.h>

#define IMSG_TAG "[tz_driver]"
#include <imsg_log.h>

static struct load_soter_entry load_ent;
static struct work_entry work_ent;
static struct work_entry sched_work_ent;

#if 0
static struct teei_smc_cmd *get_response_smc_cmd(void)
{
	struct NQ_entry *nq_ent = NULL;

	nq_ent = (struct NQ_entry *)get_nq_entry((unsigned char *)t_nt_buffer);
	if (nq_ent == NULL)
		return NULL;

	return (struct teei_smc_cmd *)phys_to_virt(
				(unsigned long)(nq_ent->buffer_addr));
}
#endif

void sched_func(struct work_struct *entry)
{
	down(&(smc_lock));
	nt_sched_t_call();
}

void add_sched_queue(void)
{
	INIT_WORK(&(sched_work_ent.work), sched_func);
	queue_work(secure_wq, &(sched_work_ent.work));
}

static irqreturn_t nt_sched_irq_handler(void)
{
	up(&(smc_lock));
	add_sched_queue();

	return IRQ_HANDLED;
}

static irqreturn_t nt_soter_irq_handler(int irq, void *dev)
{
	irq_call_flag = GLSCH_HIGH;
	up(&smc_lock);
	if (teei_config_flag == 1)
		complete(&global_down_lock);

	return IRQ_HANDLED;
}


int register_soter_irq_handler(int irq)
{
	return request_irq(irq, nt_soter_irq_handler,
				0, "tz_drivers_service", NULL);
}


static irqreturn_t nt_error_irq_handler(void)
{
	unsigned long error_num = 0;

	error_num = teei_secure_call(N_GET_SE_OS_STATE, 0, 0, 0);
	IMSG_ERROR("secure system ERROR ! error_num = %ld\n",
					(error_num - 4294967296));
	soter_error_flag = 1;
	up(&(boot_sema));
	up(&smc_lock);
	return IRQ_HANDLED;
}

static irqreturn_t nt_fp_ack_handler(void)
{
	fp_call_flag = GLSCH_NONE;
	up(&fdrv_sema);
	up(&smc_lock);
	return IRQ_HANDLED;
}

int get_bdrv_id(void)
{
	int driver_id = 0;

	Invalidate_Dcache_By_Area(bdrv_message_buff,
				bdrv_message_buff + MESSAGE_LENGTH);

	driver_id = *((int *)bdrv_message_buff);
	return driver_id;
}

void add_bdrv_queue(int bdrv_id)
{
	work_ent.call_no = bdrv_id;
	INIT_WORK(&(work_ent.work), work_func);
	queue_work(secure_wq, &(work_ent.work));
}

static irqreturn_t nt_bdrv_handler(void)
{
	int bdrv_id = 0;

	up(&(smc_lock));
	bdrv_id = get_bdrv_id();
	add_bdrv_queue(bdrv_id);

	return IRQ_HANDLED;
}

static irqreturn_t nt_boot_irq_handler(void)
{
	if (boot_soter_flag == START_STATUS) {
		IMSG_DEBUG("boot irq  handler if\n");
		boot_soter_flag = END_STATUS;
		up(&smc_lock);
		up(&(boot_sema));
	} else {
		IMSG_DEBUG("boot irq hanler else\n");
		forward_call_flag = GLSCH_NONE;
		up(&smc_lock);
		up(&(boot_sema));
	}

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
	down(&smc_lock);
	retVal = add_work_entry(LOAD_FUNC, 0);
}

void work_func(struct work_struct *entry)
{

	struct work_entry *md = container_of(entry, struct work_entry, work);
	int sys_call_num = md->call_no;

	if (sys_call_num == reetime.sysno)
		reetime.handle(&reetime);
	else if (sys_call_num == vfs_handler.sysno)
		vfs_handler.handle(&vfs_handler);
}

static irqreturn_t nt_switch_irq_handler(void)
{
	struct message_head *msg_head = NULL;

	if (boot_soter_flag == START_STATUS) {
		INIT_WORK(&(load_ent.work), load_func);
		queue_work(secure_wq, &(load_ent.work));
		up(&smc_lock);

		return IRQ_HANDLED;

	} else {
		Invalidate_Dcache_By_Area(message_buff,
					message_buff + MESSAGE_LENGTH);

		msg_head = (struct message_head *)message_buff;

		if (msg_head->message_type == FAST_CALL_TYPE) {
			return IRQ_HANDLED;
		} else if (msg_head->message_type == STANDARD_CALL_TYPE) {
			/* Get the smc_cmd struct */
			if (msg_head->child_type == VDRV_CALL_TYPE) {
				work_ent.call_no = msg_head->param_length;
				INIT_WORK(&(work_ent.work), work_func);
				queue_work(secure_wq, &(work_ent.work));
				up(&smc_lock);
#if 0
			} else if (msg_head->child_type == FDRV_ACK_TYPE) {
				/*
				 * if(forward_call_flag == GLSCH_NONE)
				 *	forward_call_flag = GLSCH_NEG;
				 * else
				 *	forward_call_flag = GLSCH_NONE;
				 */
				up(&boot_sema);
				up(&smc_lock);
#endif
			} else if (msg_head->child_type == NQ_CALL_TYPE) {
				forward_call_flag = GLSCH_NONE;
				notify_smc_completed();
				up(&smc_lock);
#ifdef TUI_SUPPORT
			} else if (msg_head->child_type == TUI_NOTICE_SYS_NO) {
				forward_call_flag = GLSCH_NONE;
				up(&(tui_notify_sema));
				up(&(smc_lock));
#endif
			} else {
				IMSG_ERROR("[%s][%d] Unknown child_type!\n",
							__func__, __LINE__);
			}

			return IRQ_HANDLED;
		}

		IMSG_ERROR("[%s][%d] Unknown IRQ!\n", __func__, __LINE__);
		return IRQ_NONE;
	}
}

static irqreturn_t ut_drv_irq_handler(int irq, void *dev)
{
	int irq_id = 0;
	int retVal = 0;
	struct tz_driver_state *s = get_tz_drv_state();

	/* Get the interrupt ID */
	irq_id = teei_secure_call(N_GET_NON_IRQ_NUM, 0, 0, 0);

	switch (irq_id) {
	case SCHED_IRQ:
		retVal = nt_sched_irq_handler();
		break;
	case SWITCH_IRQ:
		retVal = nt_switch_irq_handler();
		break;
	case BDRV_IRQ:
		retVal = nt_bdrv_handler();
		break;
	case TEEI_LOG_IRQ:
		retVal = -EINVAL;
		IMSG_ERROR("tlog not support\n");
		break;
	case FP_ACK_IRQ:
		retVal = nt_fp_ack_handler();
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

int register_ut_irq_handler(int irq)
{
	return request_irq(irq, ut_drv_irq_handler,
				0, "tz_drivers_service", NULL);
}
