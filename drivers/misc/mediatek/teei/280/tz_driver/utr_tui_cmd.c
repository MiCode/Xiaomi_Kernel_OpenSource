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
	long retVal = 0;
	unsigned long temp_addr = 0;
	struct message_head msg_head;
	struct create_fdrv_struct msg_body;
	struct ack_fast_call_struct msg_ack;

	if ((unsigned char *)message_buff == NULL) {
		IMSG_ERROR("[%s][%d]: There is NO command buffer!.\n",
						__func__, __LINE__);
		return (unsigned long)NULL;
	}

#ifdef UT_DMA_ZONE
	temp_addr = (unsigned long) __get_free_pages(GFP_KERNEL | GFP_DMA,
					get_order(ROUND_UP(buff_size, SZ_4K)));
#else
	temp_addr = (unsigned long) __get_free_pages(GFP_KERNEL,
					get_order(ROUND_UP(buff_size, SZ_4K)));
#endif

	if (temp_addr == 0) {
		IMSG_ERROR("[%s][%d]: kmalloc fp drv buffer failed.\n",
						__FILE__, __LINE__);
		return (unsigned long)NULL;
	}

	memset(&msg_head, 0, sizeof(struct message_head));
	memset(&msg_body, 0, sizeof(struct create_fdrv_struct));
	memset(&msg_ack, 0, sizeof(struct ack_fast_call_struct));

	msg_head.invalid_flag = VALID_TYPE;
	msg_head.message_type = FAST_CALL_TYPE;
	msg_head.child_type = FAST_CREAT_FDRV;
	msg_head.param_length = sizeof(struct create_fdrv_struct);

	msg_body.fdrv_type = fdrv_type;
	msg_body.fdrv_phy_addr = virt_to_phys((void *)temp_addr);
	msg_body.fdrv_size = buff_size;

	/* Notify the T_OS that there is ctl_buffer to be created. */
	memcpy((void *)message_buff, &msg_head, sizeof(struct message_head));
	memcpy((void *)(message_buff + sizeof(struct message_head)),
				&msg_body, sizeof(struct create_fdrv_struct));

	Flush_Dcache_By_Area((unsigned long)message_buff,
				(unsigned long)message_buff + MESSAGE_SIZE);

	/* Call the smc_fast_call */
	/* get_online_cpus(); */

	ut_pm_mutex_lock(&pm_mutex);
	down(&(smc_lock));
	invoke_fastcall();
	down(&(boot_sema));
	ut_pm_mutex_unlock(&pm_mutex);

	/* put_online_cpus(); */

	Invalidate_Dcache_By_Area((unsigned long)message_buff,
				(unsigned long)message_buff + MESSAGE_SIZE);

	memcpy(&msg_head, (void *)message_buff, sizeof(struct message_head));

	memcpy(&msg_ack, (void *)(message_buff + sizeof(struct message_head)),
				sizeof(struct ack_fast_call_struct));

	/* Check the response from T_OS. */
	if ((msg_head.message_type == FAST_CALL_TYPE)
			&& (msg_head.child_type == FAST_ACK_CREAT_FDRV)) {
		retVal = msg_ack.retVal;
		if (retVal == 0) {
			IMSG_ERROR("[%s][%d]: %s end.\n",
				__func__, __LINE__, __func__);
			return temp_addr;
		}
	} else
		retVal = 0;

	/* Release the resource and return. */
	free_pages(temp_addr, get_order(ROUND_UP(buff_size, SZ_4K)));

	IMSG_ERROR("[%s][%d]: %s failed!\n", __func__, __LINE__, __func__);
	return retVal;
}


int try_send_tui_command(void)
{
	int result = down_trylock(&api_lock);

	if (result == 0) {
		up(&api_lock);
		return 1;
	}

	result = down_trylock(&fdrv_lock);
	if (result == 0) {
		up(&fdrv_lock);
		return 2;
	}

	return 0;
}


void set_tui_display_command(unsigned long type)
{
	struct fdrv_message_head fdrv_msg_head;

	memset(&fdrv_msg_head, 0, sizeof(struct fdrv_message_head));

	if (type == TUI_NOTICE_SYS_NO)
		fdrv_msg_head.driver_type = TUI_NOTICE_SYS_NO;
	else
		fdrv_msg_head.driver_type = TUI_DISPLAY_SYS_NO;

	fdrv_msg_head.fdrv_param_length = sizeof(unsigned int);

	memcpy((void *)fdrv_message_buff,
			&fdrv_msg_head, sizeof(struct fdrv_message_head));

	Flush_Dcache_By_Area((unsigned long)fdrv_message_buff,
			(unsigned long)fdrv_message_buff + MESSAGE_SIZE);
}

int __send_tui_display_command(unsigned long type)
{
	unsigned long smc_type = 2;
	uint32_t datalen = 0;

	set_tui_display_command(type);
	if (type == TUI_NOTICE_SYS_NO) {
		memcpy(&datalen, (void *) tui_notice_message_buff,
			sizeof(uint32_t));

		Flush_Dcache_By_Area((unsigned long)tui_notice_message_buff,
			tui_notice_message_buff +  sizeof(uint32_t) + datalen);

	} else {
		memcpy(&datalen, (void *) tui_display_message_buff,
			sizeof(uint32_t));

		Flush_Dcache_By_Area((unsigned long)tui_display_message_buff,
			tui_display_message_buff + sizeof(uint32_t)+datalen);
	}

	fp_call_flag = GLSCH_HIGH;

	smc_type = teei_secure_call(N_INVOKE_T_DRV, 0, 0, 0);
	while (smc_type == SMC_CALL_INTERRUPTED_IRQ)
		smc_type = teei_secure_call(NT_SCHED_T, 0, 0, 0);

	return 0;
}

void set_tui_notice_command(unsigned long memory_size)
{
	struct message_head msg_head;

	memset(&msg_head, 0, sizeof(struct message_head));

	msg_head.invalid_flag = VALID_TYPE;
	msg_head.message_type = STANDARD_CALL_TYPE;
	msg_head.child_type = TUI_NOTICE_SYS_NO;

	memcpy((void *)message_buff, &msg_head, sizeof(struct message_head));

	Flush_Dcache_By_Area((unsigned long)message_buff,
				(unsigned long)message_buff + MESSAGE_SIZE);
}

int __send_tui_notice_command(unsigned long share_memory_size)
{
	unsigned long smc_type = 2;
	uint32_t datalen = 0;

	set_tui_notice_command(share_memory_size);
	memcpy(&datalen, (void *) tui_notice_message_buff, sizeof(uint32_t));

	Flush_Dcache_By_Area((unsigned long)tui_notice_message_buff,
		tui_notice_message_buff +  sizeof(uint32_t) + datalen);

	forward_call_flag = GLSCH_LOW;
	smc_type = teei_secure_call(N_ACK_T_INVOKE_DRV, 0, 0, 0);
	while (smc_type == SMC_CALL_INTERRUPTED_IRQ)
		smc_type = teei_secure_call(NT_SCHED_T, 0, 0, 0);
	return 0;
}

int send_tui_display_command(unsigned long type)
{
	struct fdrv_call_struct fdrv_ent;
	int retVal = 0;

	down(&fdrv_lock);
	ut_pm_mutex_lock(&pm_mutex);

	down(&smc_lock);

	if (teei_config_flag == 1)
		complete(&global_down_lock);

	fdrv_ent.fdrv_call_type = TUI_DISPLAY_SYS_NO;
	fdrv_ent.fdrv_call_buff_size = type;
	/* with a wmb() */
	wmb();

	Flush_Dcache_By_Area((unsigned long)&fdrv_ent,
		(unsigned long)&fdrv_ent + sizeof(struct fdrv_call_struct));

	retVal = add_work_entry(FDRV_CALL, (unsigned long)(&fdrv_ent));

	if (retVal != 0) {
		up(&smc_lock);
		ut_pm_mutex_unlock(&pm_mutex);
		up(&fdrv_lock);
		return retVal;
	}

	down(&fdrv_sema);

	/* with a rmb() */
	rmb();

	if (type == TUI_NOTICE_SYS_NO)
		Invalidate_Dcache_By_Area(
			(unsigned long)tui_notice_message_buff,
			tui_notice_message_buff + TUI_NOTICE_BUFFER);
	else
		Invalidate_Dcache_By_Area(
			(unsigned long)tui_display_message_buff,
			tui_display_message_buff + TUI_DISPLAY_BUFFER);

	ut_pm_mutex_unlock(&pm_mutex);
	up(&fdrv_lock);

	return fdrv_ent.retVal;
}


int send_tui_notice_command(unsigned long share_memory_size)
{
	struct fdrv_call_struct fdrv_ent;
	int retVal = 0;

	down(&api_lock);
	ut_pm_mutex_lock(&pm_mutex);

	down(&smc_lock);

	if (teei_config_flag == 1)
		complete(&global_down_lock);

	fdrv_ent.fdrv_call_type = TUI_NOTICE_SYS_NO;
	fdrv_ent.fdrv_call_buff_size = share_memory_size;
	/* with a wmb() */
	wmb();

	Flush_Dcache_By_Area((unsigned long)&fdrv_ent,
		(unsigned long)&fdrv_ent + sizeof(struct fdrv_call_struct));

	retVal = add_work_entry(FDRV_CALL, (unsigned long)(&fdrv_ent));
	if (retVal != 0) {
		up(&smc_lock);
		ut_pm_mutex_unlock(&pm_mutex);
		up(&api_lock);
		return retVal;
	}

	down(&tui_notify_sema);

	/* with a rmb() */
	rmb();

	Invalidate_Dcache_By_Area((unsigned long)tui_notice_message_buff,
				tui_notice_message_buff + TUI_NOTICE_BUFFER);

	ut_pm_mutex_unlock(&pm_mutex);
	up(&api_lock);

	return fdrv_ent.retVal;
}

int send_power_down_cmd(void)
{
	int retVal = 0;

	ut_pm_mutex_lock(&pm_mutex);

	down(&smc_lock);
	retVal = add_work_entry(POWER_DOWN_CALL, 0);
	up(&smc_lock);

	ut_pm_mutex_unlock(&pm_mutex);

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
