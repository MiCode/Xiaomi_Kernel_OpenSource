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
#include <linux/delay.h>
#include <linux/cpu.h>
#include <fdrv.h>
#include "teei_id.h"
#include "sched_status.h"
#include "nt_smc_call.h"
#include "teei_log.h"
#include "teei_common.h"
#include "switch_queue.h"
#include "teei_client_main.h"
#include "backward_driver.h"
#include "utdriver_macro.h"

#include <teei_secure_api.h>

#define IMSG_TAG "[tz_driver]"
#include <imsg_log.h>

static LIST_HEAD(fdrv_list);

static int create_fdrv(struct teei_fdrv *fdrv)
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


	if (fdrv->buff_size > VDRV_MAX_SIZE) {
		IMSG_ERROR("[%s][%d]: fDrv buffer is too large.\n",
				__FILE__, __LINE__);
		return (unsigned long)NULL;
	}

#ifdef UT_DMA_ZONE
	temp_addr = (unsigned long) __get_free_pages(GFP_KERNEL | GFP_DMA,
				get_order(ROUND_UP(fdrv->buff_size, SZ_4K)));
#else
	temp_addr = (unsigned long) __get_free_pages(GFP_KERNEL,
				get_order(ROUND_UP(fdrv->buff_size, SZ_4K)));
#endif
	if ((unsigned char *)temp_addr == NULL) {
		IMSG_ERROR("[%s][%d]: kmalloc fdrv buffer failed.\n",
				__FILE__, __LINE__);
		return (unsigned long)NULL;
	}

	memset((void *)(&msg_head), 0, sizeof(struct message_head));
	memset((void *)(&msg_body), 0, sizeof(struct create_fdrv_struct));
	memset((void *)(&msg_ack), 0, sizeof(struct ack_fast_call_struct));

	msg_head.invalid_flag = VALID_TYPE;
	msg_head.message_type = FAST_CALL_TYPE;
	msg_head.child_type = FAST_CREAT_FDRV;
	msg_head.param_length = sizeof(struct create_fdrv_struct);

	msg_body.fdrv_type = fdrv->call_type;
	msg_body.fdrv_phy_addr = virt_to_phys((void *)temp_addr);
	msg_body.fdrv_size = fdrv->buff_size;


	/* Notify the T_OS that there is ctl_buffer to be created. */
	memcpy((void *)message_buff, (void *)(&msg_head),
					sizeof(struct message_head));

	memcpy((void *)(message_buff + sizeof(struct message_head)),
			(void *)(&msg_body),
			sizeof(struct create_fdrv_struct));

	Flush_Dcache_By_Area((unsigned long)message_buff,
				(unsigned long)message_buff + MESSAGE_SIZE);

	/* Call the smc_fast_call */
	down(&(smc_lock));

	invoke_fastcall();

	down(&(boot_sema));

	Invalidate_Dcache_By_Area((unsigned long)message_buff,
				(unsigned long)message_buff + MESSAGE_SIZE);

	memcpy((void *)(&msg_head), (void *)message_buff,
			sizeof(struct message_head));

	memcpy((void *)(&msg_ack),
			(void *)(message_buff + sizeof(struct message_head)),
			sizeof(struct ack_fast_call_struct));

	/* Check the response from T_OS. */
	if ((msg_head.message_type == FAST_CALL_TYPE)
		&& (msg_head.child_type == FAST_ACK_CREAT_FDRV)) {
		retVal = msg_ack.retVal;

		if (retVal == 0)
			fdrv->buf = (void *)temp_addr;
	} else
		retVal = -EINVAL;

	if (retVal < 0) {
		/* Release the resource and return. */
		free_pages(temp_addr,
			get_order(ROUND_UP(fdrv->buff_size, SZ_4K)));

		IMSG_ERROR("%s failed (%ld)\n", __func__, retVal);
	}

	return retVal;
}

static void set_command(struct teei_fdrv *fdrv)
{
	struct fdrv_message_head fdrv_msg_head;

	memset((void *)(&fdrv_msg_head), 0, sizeof(struct fdrv_message_head));

	fdrv_msg_head.driver_type = fdrv->call_type;
	fdrv_msg_head.fdrv_param_length = sizeof(unsigned int);

	memcpy((void *)fdrv_message_buff,
		(void *)(&fdrv_msg_head), sizeof(struct fdrv_message_head));

	Flush_Dcache_By_Area((unsigned long)fdrv_message_buff,
			(unsigned long)fdrv_message_buff + MESSAGE_SIZE);
}

static int send_command(struct teei_fdrv *fdrv)
{
	uint32_t smc_type;

	set_command(fdrv);
	Flush_Dcache_By_Area((unsigned long)fdrv->buf,
				(unsigned long)fdrv->buf + fdrv->buff_size);

	fp_call_flag = GLSCH_HIGH;

	smc_type = teei_secure_call(N_INVOKE_T_DRV, 0, 0, 0);

	while (smc_type == SMC_CALL_INTERRUPTED_IRQ)
		smc_type = teei_secure_call(NT_SCHED_T, 0, 0, 0);

	return 0;
}

void register_fdrv(struct teei_fdrv *fdrv)
{
	list_add_tail(&fdrv->list, &fdrv_list);
}
EXPORT_SYMBOL(register_fdrv);

int fdrv_notify(struct teei_fdrv *fdrv)
{
	struct fdrv_call_struct fdrv_ent;
	int retVal = 0;

	down(&fdrv_lock);
	lock_system_sleep();
	down(&smc_lock);

	IMSG_ENTER();

	if (teei_config_flag == 1)
		complete(&global_down_lock);

	fdrv_ent.fdrv_call_type = fdrv->call_type;
	fdrv_ent.fdrv_call_buff_size = fdrv->buff_size;

	/* with a wmb() */
	wmb();

	Flush_Dcache_By_Area((unsigned long)&fdrv_ent,
		(unsigned long)&fdrv_ent + sizeof(struct fdrv_call_struct));

	retVal = add_work_entry(FDRV_CALL, (unsigned long)(&fdrv_ent));
	if (retVal != 0) {
		up(&smc_lock);
		unlock_system_sleep();
		up(&fdrv_lock);
		return retVal;
	}

	down(&fdrv_sema);

	/* with a rmb() */
	rmb();

	Invalidate_Dcache_By_Area((unsigned long)fdrv->buf,
				(unsigned long)fdrv->buf + fdrv->buff_size);

	unlock_system_sleep();
	up(&fdrv_lock);

	IMSG_LEAVE();
	return fdrv_ent.retVal;
}
EXPORT_SYMBOL(fdrv_notify);

int create_all_fdrv(void)
{
	struct teei_fdrv *fdrv;

	list_for_each_entry(fdrv, &fdrv_list, list) {
		int ret = create_fdrv(fdrv);

		if (ret)
			return ret;
	}

	return 0;
}

int __call_fdrv(struct fdrv_call_struct *fdrv_ent)
{
	struct teei_fdrv *fdrv;

	list_for_each_entry(fdrv, &fdrv_list, list) {
		if (fdrv->call_type == fdrv_ent->fdrv_call_type)
			return send_command(fdrv);
	}

	return -EINVAL;
}
