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

#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/types.h>
#include <linux/cpu.h>
#include "nt_smc_call.h"
#include "backward_driver.h"
#include "teei_id.h"
#include "sched_status.h"
#include "utdriver_macro.h"
#include "teei_log.h"
#include "teei_common.h"
#include "switch_queue.h"
#include "teei_client_main.h"
#include <linux/time.h>
#include <teei_secure_api.h>

#define IMSG_TAG "[tz_driver]"
#include <imsg_log.h>

#define		GET_UPTIME	(1)
#define		GET_SYSTIME	(0)

struct bdrv_call_struct {
	int bdrv_call_type;
	struct service_handler *handler;
	int retVal;
};

static long register_shared_param_buf(struct service_handler *handler);
unsigned char *daulOS_VFS_share_mem;
unsigned char *vfs_flush_address;
struct service_handler reetime;

void set_ack_vdrv_cmd(unsigned int sys_num)
{
	if (boot_soter_flag == START_STATUS) {
		struct message_head msg_head;
		struct ack_vdrv_struct ack_body;

		memset(&msg_head, 0, sizeof(struct message_head));

		msg_head.invalid_flag = VALID_TYPE;
		msg_head.message_type = STANDARD_CALL_TYPE;
		msg_head.child_type = N_ACK_T_INVOKE_DRV_CMD;
		msg_head.param_length = sizeof(struct ack_vdrv_struct);

		ack_body.sysno = sys_num;

		memcpy((void *)message_buff, (void *)(&msg_head),
					sizeof(struct message_head));
		memcpy((void *)(message_buff + sizeof(struct message_head)),
			(void *)(&ack_body), sizeof(struct ack_vdrv_struct));

		Flush_Dcache_By_Area((unsigned long)message_buff,
				(unsigned long)message_buff + MESSAGE_SIZE);
	} else {
		*((int *)bdrv_message_buff) = sys_num;
		Flush_Dcache_By_Area((unsigned long)bdrv_message_buff,
			(unsigned long)bdrv_message_buff + MESSAGE_SIZE);
	}
}

void secondary_invoke_fastcall(void *info)
{
	unsigned long smc_type = 2;

	smc_type = teei_secure_call(N_INVOKE_T_FAST_CALL, 0, 0, 0);
	while (smc_type == SMC_CALL_INTERRUPTED_IRQ)
		smc_type = teei_secure_call(NT_SCHED_T, 0, 0, 0);
}

void invoke_fastcall(void)
{
	forward_call_flag = GLSCH_LOW;
	add_work_entry(INVOKE_FASTCALL, 0);
}

static long register_shared_param_buf(struct service_handler *handler)
{

	long retVal = 0;
	struct message_head msg_head;
	struct create_vdrv_struct msg_body;
	struct ack_fast_call_struct msg_ack;

	if ((unsigned char *)message_buff == NULL) {
		IMSG_ERROR("[%s][%d]: There is NO command buffer!.\n",
					__func__, __LINE__);
		return -EINVAL;
	}

	if (handler->size > VDRV_MAX_SIZE) {
		IMSG_ERROR("[%s][%d]: The vDrv buffer is too large.\n",
					__FILE__, __LINE__);
		return -EINVAL;
	}

#ifdef UT_DMA_ZONE
	handler->param_buf = (void *)__get_free_pages(GFP_KERNEL | GFP_DMA,
				get_order(ROUND_UP(handler->size, SZ_4K)));
#else
	handler->param_buf = (void *)__get_free_pages(GFP_KERNEL,
				get_order(ROUND_UP(handler->size, SZ_4K)));
#endif
	if ((unsigned char *)(handler->param_buf) == NULL) {
		IMSG_ERROR("[%s][%d]: kmalloc vdrv_buffer failed.\n",
							__FILE__, __LINE__);
		return -ENOMEM;
	}

	memset((void *)(&msg_head), 0, sizeof(struct message_head));
	memset((void *)(&msg_body), 0, sizeof(struct create_vdrv_struct));
	memset((void *)(&msg_ack), 0, sizeof(struct ack_fast_call_struct));

	msg_head.invalid_flag = VALID_TYPE;
	msg_head.message_type = FAST_CALL_TYPE;
	msg_head.child_type = FAST_CREAT_VDRV;
	msg_head.param_length = sizeof(struct create_vdrv_struct);
	msg_body.vdrv_type = handler->sysno;
	msg_body.vdrv_phy_addr = virt_to_phys(handler->param_buf);
	msg_body.vdrv_size = handler->size;

	/* Notify the T_OS that there is ctl_buffer to be created. */
	memcpy((void *)message_buff, (void *)(&msg_head),
				sizeof(struct message_head));

	memcpy((void *)(message_buff + sizeof(struct message_head)),
		(void *)(&msg_body), sizeof(struct create_vdrv_struct));

	Flush_Dcache_By_Area((unsigned long)message_buff,
				(unsigned long)message_buff + MESSAGE_SIZE);

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
			&& (msg_head.child_type == FAST_ACK_CREAT_VDRV)) {
		retVal = msg_ack.retVal;

		if (retVal == 0)
			return retVal;
	} else
		retVal = -EAGAIN;

	/* Release the resource and return. */
	free_pages((unsigned long)(handler->param_buf),
				get_order(ROUND_UP(handler->size, SZ_4K)));
	handler->param_buf = NULL;

	return retVal;
}

/******************************TIME**************************************/
static long reetime_init(struct service_handler *handler)
{
	return register_shared_param_buf(handler);
}

static void reetime_deinit(struct service_handler *handler)
{
}

int __reetime_handle(struct service_handler *handler)
{
	struct timeval tv;
	void *ptr = NULL;
	int tv_sec;
	int tv_usec;
	unsigned long smc_type = 2;
	struct timespec tp;
	int time_type = 0;

	ptr = handler->param_buf;
	Invalidate_Dcache_By_Area((unsigned long)ptr, ptr + 4);
	time_type = *((int *)ptr);
	if (time_type == GET_UPTIME) {
		get_monotonic_boottime(&tp);
		tv_sec = tp.tv_sec;
		*((int *)ptr) = tv_sec;
		tv_usec = tp.tv_nsec/1000;
		*((int *)ptr + 1) = tv_usec;
	} else if (time_type == GET_SYSTIME) {
		do_gettimeofday(&tv);
		tv_sec = tv.tv_sec;
		*((int *)ptr) = tv_sec;
		tv_usec = tv.tv_usec;
		*((int *)ptr + 1) = tv_usec;
	}


	Flush_Dcache_By_Area((unsigned long)handler->param_buf,
			(unsigned long)handler->param_buf + handler->size);

	set_ack_vdrv_cmd(handler->sysno);

	smc_type = teei_secure_call(N_ACK_T_INVOKE_DRV, 0, 0, 0);
	while (smc_type == SMC_CALL_INTERRUPTED_IRQ)
		smc_type = teei_secure_call(NT_SCHED_T, 0, 0, 0);

	return 0;
}

static int reetime_handle(struct service_handler *handler)
{
	int retVal = 0;
	struct bdrv_call_struct *reetime_bdrv_ent = NULL;

	down(&smc_lock);

	reetime_bdrv_ent = kmalloc(sizeof(struct bdrv_call_struct), GFP_KERNEL);
	reetime_bdrv_ent->handler = handler;
	reetime_bdrv_ent->bdrv_call_type = REETIME_SYS_NO;

	/* with a wmb() */
	wmb();

	retVal = add_work_entry(BDRV_CALL, (unsigned long)reetime_bdrv_ent);
	if (retVal != 0) {
		up(&smc_lock);
		return retVal;
	}

	/* with a rmb() */
	rmb();

	return 0;
}

/********************************************************************
 *                      VFS functions                               *
 ********************************************************************/
struct service_handler vfs_handler;
static unsigned long para_vaddr;
static unsigned long buff_vaddr;


int vfs_thread_function(unsigned long virt_addr,
			unsigned long para_vaddr, unsigned long buff_vaddr)
{
	Invalidate_Dcache_By_Area((unsigned long)virt_addr,
					virt_addr + VFS_SIZE);
	daulOS_VFS_share_mem = (unsigned char *)virt_addr;
#ifdef VFS_RDWR_SEM
	up(&VFS_rd_sem);
	down_interruptible(&VFS_wr_sem);
#else
	complete(&VFS_rd_comp);
	wait_for_completion_interruptible(&VFS_wr_comp);
#endif
	return 0;
}

static long vfs_init(struct service_handler *handler) /*! init service */
{
	long retVal = 0;

	retVal = register_shared_param_buf(handler);
	vfs_flush_address = handler->param_buf;

	return retVal;
}

static void vfs_deinit(struct service_handler *handler) /*! stop service  */
{
}

int __vfs_handle(struct service_handler *handler) /*! invoke handler */
{
	unsigned long smc_type = 2;

	Flush_Dcache_By_Area((unsigned long)handler->param_buf,
			(unsigned long)handler->param_buf + handler->size);

	set_ack_vdrv_cmd(handler->sysno);

	smc_type = teei_secure_call(N_ACK_T_INVOKE_DRV, 0, 0, 0);
	while (smc_type == SMC_CALL_INTERRUPTED_IRQ)
		smc_type = teei_secure_call(NT_SCHED_T, 0, 0, 0);

	return 0;
}

static int vfs_handle(struct service_handler *handler)
{
	int retVal = 0;

	struct bdrv_call_struct *vfs_bdrv_ent = NULL;

	vfs_thread_function((unsigned long)(handler->param_buf),
						para_vaddr, buff_vaddr);

	down(&smc_lock);
	vfs_bdrv_ent = kmalloc(sizeof(struct bdrv_call_struct), GFP_KERNEL);
	vfs_bdrv_ent->handler = handler;
	vfs_bdrv_ent->bdrv_call_type = VFS_SYS_NO;

	/* with a wmb() */
	wmb();

	Flush_Dcache_By_Area((unsigned long)vfs_bdrv_ent,
		(unsigned long)vfs_bdrv_ent + sizeof(struct bdrv_call_struct));
	retVal = add_work_entry(BDRV_CALL, (unsigned long)vfs_bdrv_ent);
	if (retVal != 0) {
		up(&smc_lock);
		return retVal;
	}
	/* with a rmb() */
	rmb();

	return 0;
}

long init_all_service_handlers(void)
{
	long retVal = 0;

	reetime.init = reetime_init;
	reetime.deinit = reetime_deinit;
	reetime.handle = reetime_handle;
	reetime.size = 0x1000;
	reetime.sysno  = 7;

	vfs_handler.init = vfs_init;
	vfs_handler.deinit = vfs_deinit;
	vfs_handler.handle = vfs_handle;
	vfs_handler.size = 0x80000;
	vfs_handler.sysno = 8;

	retVal = reetime.init(&reetime);
	if (retVal < 0) {
		IMSG_ERROR("[%s][%d] init reetime service failed!\n",
					__func__, __LINE__);
		return retVal;
	}

	retVal = vfs_handler.init(&vfs_handler);
	if (retVal < 0) {
		IMSG_ERROR("[%s][%d] init vfs service failed!\n",
					__func__, __LINE__);
		return retVal;
	}

	return 0;
}
