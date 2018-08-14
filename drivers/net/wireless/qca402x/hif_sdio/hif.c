/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* This file was originally distributed by Qualcomm Atheros, Inc.
 * before Copyright ownership was assigned to the Linux Foundation.
 */

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "hif_internal.h"
#include "hif.h"

#if defined(DEBUG)
#define hifdebug(fmt, a...)\
	pr_err("hif %s:%d: " fmt, __func__, __LINE__, ##a)
#else
#define hifdebug(args...)
#endif

#define MAX_HIF_DEVICES 2
#define ENABLE_SDIO_TIMEOUT 100 /* ms */

static unsigned int hif_mmcbuswidth;
EXPORT_SYMBOL(hif_mmcbuswidth);
module_param(hif_mmcbuswidth, uint, 0644);
MODULE_PARM_DESC(hif_mmcbuswidth, "Set MMC driver Bus Width: 1-1Bit, 4-4Bit, 8-8Bit");

static unsigned int hif_mmcclock;
EXPORT_SYMBOL(hif_mmcclock);
module_param(hif_mmcclock, uint, 0644);
MODULE_PARM_DESC(hif_mmcclock, "Set MMC driver Clock value");

static unsigned int hif_writecccr1;
module_param(hif_writecccr1, uint, 0644);
static unsigned int hif_writecccr1value;
module_param(hif_writecccr1value, uint, 0644);

static unsigned int hif_writecccr2;
module_param(hif_writecccr2, uint, 0644);
static unsigned int hif_writecccr2value;
module_param(hif_writecccr2value, uint, 0644);

static unsigned int hif_writecccr3;
module_param(hif_writecccr3, uint, 0644);
static unsigned int hif_writecccr3value;
module_param(hif_writecccr3value, uint, 0644);

static unsigned int hif_writecccr4;
module_param(hif_writecccr4, uint, 0644);

static unsigned int hif_writecccr4value;
module_param(hif_writecccr4value, uint, 0644);

static int hif_device_inserted(struct sdio_func *func,
			       const struct sdio_device_id *id);
static void hif_device_removed(struct sdio_func *func);
static void *add_hif_device(struct sdio_func *func);
static struct hif_device *get_hif_device(struct sdio_func *func);
static void del_hif_device(struct hif_device *device);
static int func0_CMD52_write_byte(struct mmc_card *card, unsigned int address,
				  unsigned char byte);
static int func0_CMD52_read_byte(struct mmc_card *card, unsigned int address,
				 unsigned char *byte);
static void hif_stop_hif_task(struct hif_device *device);
static struct bus_request *hif_allocate_bus_request(void *device);
static void hif_free_bus_request(struct hif_device *device,
				 struct bus_request *busrequest);
static void hif_add_to_req_list(struct hif_device *device,
				struct bus_request *busrequest);

static int hif_reset_sdio_on_unload;
module_param(hif_reset_sdio_on_unload, int, 0644);

static u32 hif_forcedriverstrength = 1; /* force driver strength to type D */

static const struct sdio_device_id hif_sdio_id_table[] = {
	{SDIO_DEVICE(SDIO_ANY_ID,
	SDIO_ANY_ID)}, /* QCA402x IDs are hardwired to 0 */
	{/* null */},
};

MODULE_DEVICE_TABLE(sdio, hif_sdio_id_table);

static struct sdio_driver hif_sdio_driver = {
	.name = "hif_sdio",
	.id_table = hif_sdio_id_table,
	.probe = hif_device_inserted,
	.remove = hif_device_removed,
};

/* make sure we unregister only when registered. */
/* TBD: synchronization needed.... */
/* device->completion_task, registered, ... */
static int registered;

static struct cbs_from_os hif_callbacks;

static struct hif_device *hif_devices[MAX_HIF_DEVICES];

static int hif_disable_func(struct hif_device *device, struct sdio_func *func);
static int hif_enable_func(struct hif_device *device, struct sdio_func *func);

static int hif_sdio_register_driver(struct cbs_from_os *callbacks)
{
	/* store the callback handlers */
	hif_callbacks = *callbacks; /* structure copy */

	/* Register with bus driver core */
	registered++;

	return sdio_register_driver(&hif_sdio_driver);
}

static void hif_sdio_unregister_driver(void)
{
	sdio_unregister_driver(&hif_sdio_driver);
	registered--;
}

int hif_init(struct cbs_from_os *callbacks)
{
	int status;

	hifdebug("Enter\n");
	if (!callbacks)
		return HIF_ERROR;

	hifdebug("calling hif_sdio_register_driver\n");
	status = hif_sdio_register_driver(callbacks);
	hifdebug("hif_sdio_register_driver returns %d\n", status);
	if (status != 0)
		return HIF_ERROR;

	return HIF_OK;
}

static int __hif_read_write(struct hif_device *device, u32 address,
			    u8 *buffer, u32 length,
			    u32 request, void *context)
{
	u8 opcode;
	int status = HIF_OK;
	int ret = 0;
	u8 temp[4];

	if (!device || !device->func)
		return HIF_ERROR;

	if (!buffer)
		return HIF_EINVAL;

	if (length == 0)
		return HIF_EINVAL;

	do {
		if (!(request & HIF_EXTENDED_IO)) {
			status = HIF_EINVAL;
			break;
		}

		if (request & HIF_BLOCK_BASIS) {
			if (WARN_ON(length & (HIF_MBOX_BLOCK_SIZE - 1)))
				return HIF_EINVAL;
		} else if (request & HIF_BYTE_BASIS) {
		} else {
			status = HIF_EINVAL;
			break;
		}

		if (request & HIF_FIXED_ADDRESS) {
			opcode = CMD53_FIXED_ADDRESS;
		} else if (request & HIF_INCREMENTAL_ADDRESS) {
			opcode = CMD53_INCR_ADDRESS;
		} else {
			status = HIF_EINVAL;
			break;
		}

		if (request & HIF_WRITE) {
			if (opcode == CMD53_FIXED_ADDRESS) {
				/* TBD: Why special handling? */
				if (length == 1) {
					memset(temp, *buffer, 4);
					ret = sdio_writesb(device->func,
							   address, temp, 4);
				} else {
					ret =
					    sdio_writesb(device->func, address,
							 buffer, length);
				}
			} else {
				ret = sdio_memcpy_toio(device->func, address,
						       buffer, length);
			}
		} else if (request & HIF_READ) {
			if (opcode == CMD53_FIXED_ADDRESS) {
				if (length ==
				    1) { /* TBD: Why special handling? */
					memset(temp, 0, 4);
					ret = sdio_readsb(device->func, temp,
							  address, 4);
					buffer[0] = temp[0];
				} else {
					ret = sdio_readsb(device->func, buffer,
							  address, length);
				}
			} else {
				ret = sdio_memcpy_fromio(device->func, buffer,
							 address, length);
			}
		} else {
			status = HIF_EINVAL; /* Neither read nor write */
			break;
		}

		if (ret) {
			hifdebug("SDIO op returns %d\n", ret);
			status = HIF_ERROR;
		}
	} while (false);

	return status;
}

/* Add busrequest to tail of sdio_request request list */
static void hif_add_to_req_list(struct hif_device *device,
				struct bus_request *busrequest)
{
	unsigned long flags;

	busrequest->next = NULL;

	spin_lock_irqsave(&device->req_qlock, flags);
	if (device->req_qhead)
		device->req_qtail->next = (void *)busrequest;
	else
		device->req_qhead = busrequest;
	device->req_qtail = busrequest;
	spin_unlock_irqrestore(&device->req_qlock, flags);
}

int hif_sync_read(void *hif_device, u32 address, u8 *buffer,
		  u32 length, u32 request, void *context)
{
	int status;
	struct hif_device *device = (struct hif_device *)hif_device;

	if (!device || !device->func)
		return HIF_ERROR;

	sdio_claim_host(device->func);
	status = __hif_read_write(device, address, buffer, length,
				  request & ~HIF_SYNCHRONOUS, NULL);
	sdio_release_host(device->func);
	return status;
}

/* Queue a read/write request and optionally wait for it to complete. */
int hif_read_write(void *hif_device, u32 address, void *buffer,
		   u32 length, u32 req_type, void *context)
{
	struct bus_request *busrequest;
	int status;
	struct hif_device *device = (struct hif_device *)hif_device;

	if (!device || !device->func)
		return HIF_ERROR;

	if (!(req_type & HIF_ASYNCHRONOUS) && !(req_type & HIF_SYNCHRONOUS))
		return HIF_EINVAL;

	/* Serialize all requests through the reqlist and HIFtask */
	busrequest = hif_allocate_bus_request(device);
	if (!busrequest)
		return HIF_ERROR;

	/* TBD: caller may pass buffers ON THE STACK, especially 4 Byte buffers.
	 * If this is a problem on some platforms/drivers, this is one
	 * reasonable
	 * place to handle it. If poentially using DMA
	 * reject large buffers on stack
	 * copy 4B buffers allow register writes (no DMA)
	 */

	busrequest->address = address;
	busrequest->buffer = buffer;
	busrequest->length = length;
	busrequest->req_type = req_type;
	busrequest->context = context;

	hif_add_to_req_list(device, busrequest);
	device->hif_task_work = 1;
	wake_up(&device->hif_wait); /* Notify HIF task */

	if (req_type & HIF_ASYNCHRONOUS)
		return HIF_PENDING;

	/* Synchronous request -- wait for completion. */
	wait_for_completion(&busrequest->comp_req);
	status = busrequest->status;
	hif_free_bus_request(device, busrequest);
	return status;
}

/* add_to_completion_list() - Queue a completed request
 * @device:    context to the hif device.
 * @comple: SDIO bus access request.
 *
 * This function adds an sdio bus access request to the
 * completion list.
 *
 * Return: No return.
 */
static void add_to_completion_list(struct hif_device *device,
				   struct bus_request *comple)
{
	unsigned long flags;

	comple->next = NULL;

	spin_lock_irqsave(&device->compl_qlock, flags);
	if (device->compl_qhead)
		device->compl_qtail->next = (void *)comple;
	else
		device->compl_qhead = comple;

	device->compl_qtail = comple;
	spin_unlock_irqrestore(&device->compl_qlock, flags);
}

/* process_completion_list() - Remove completed requests from
 * the completion list, and invoke the corresponding callbacks.
 *
 * @device:  HIF device handle.
 *
 * Function to clean the completion list.
 *
 * Return: No
 */
static void process_completion_list(struct hif_device *device)
{
	unsigned long flags;
	struct bus_request *next_comple;
	struct bus_request *request;

	/* Pull the entire chain of completions from the list */
	spin_lock_irqsave(&device->compl_qlock, flags);
	request = device->compl_qhead;
	device->compl_qhead = NULL;
	device->compl_qtail = NULL;
	spin_unlock_irqrestore(&device->compl_qlock, flags);

	while (request) {
		int status;
		void *context;

		hifdebug("HIF top of loop\n");
		next_comple = (struct bus_request *)request->next;

		status = request->status;
		context = request->context;
		hif_free_bus_request(device, request);
		device->cbs_from_hif.rw_completion_hdl(context, status);

		request = next_comple;
	}
}

/* completion_task() - Thread to process request completions
 *
 * @param:   context to the hif device.
 *
 * Completed asynchronous requests are added to a completion
 * queue where they are processed by this task. This serves
 * multiple purposes:
 * -minimizes processing by the HIFTask, which allows
 *	that task to keep SDIO busy
 * -allows request processing to be parallelized on
 *	multiprocessor systems
 * -provides a suspendable context for use by the
 *	caller's callback function, though this should
 *	not be abused since it will cause requests to
 *	sit on the completion queue (which makes us
 *	more likely to exhaust free requests).
 *
 * Return: 0 thread exits
 */
static int completion_task(void *param)
{
	struct hif_device *device;

	device = (struct hif_device *)param;
	set_current_state(TASK_INTERRUPTIBLE);

	for (;;) {
		hifdebug("HIF top of loop\n");
		wait_event_interruptible(device->completion_wait,
					 device->completion_work);
		if (!device->completion_work)
			break;

		if (device->completion_shutdown)
			break;

		device->completion_work = 0;
		process_completion_list(device);
	}

	/* Process any remaining completions.
	 * This task should not be shut down
	 * until after all requests are stopped.
	 */
	process_completion_list(device);

	complete_and_exit(&device->completion_exit, 0);
	return 0;
}

/* hif_request_complete() - Completion processing after a request
 * is processed.
 *
 * @device:    device handle.
 * @request:   SIDO bus access request.
 *
 * All completed requests are queued onto a completion list
 * which is processed by complete_task.
 *
 * Return: None.
 */
static inline void hif_request_complete(struct hif_device *device,
					struct bus_request *request)
{
	add_to_completion_list(device, request);
	device->completion_work = 1;
	wake_up(&device->completion_wait);
}

/* hif_stop_completion_thread() - Destroy the completion task
 * @device: device handle.
 *
 * This function will destroy the completion thread.
 *
 * Return: None.
 */
static inline void hif_stop_completion_thread(struct hif_device *device)
{
	if (device->completion_task) {
		init_completion(&device->completion_exit);
		device->completion_shutdown = 1;

		device->completion_work = 1;
		wake_up(&device->completion_wait);
		wait_for_completion(&device->completion_exit);
		device->completion_task = NULL;
	}
}

/* This task tries to keep the SDIO bus as busy as it
 * can. It pulls both requests off the request queue and
 * it uses the underlying sdio API to make them happen.
 *
 * Requests may be one of
 * synchronous (a thread is suspended until it completes)
 * asynchronous (a completion callback will be invoked)
 * and one of
 * reads (from Target SDIO space into Host RAM)
 * writes (from Host RAM into Target SDIO space)
 * and it is to one of
 * Target's mailbox space
 * Target's register space
 * and lots of other choices.
 */
static int hif_task(void *param)
{
	struct hif_device *device;
	struct bus_request *request;
	int status;
	unsigned long flags;

	set_user_nice(current, -3);
	device = (struct hif_device *)param;
	set_current_state(TASK_INTERRUPTIBLE);

	for (;;) {
		hifdebug("top of loop\n");
		/* wait for work */
		wait_event_interruptible(device->hif_wait,
					 device->hif_task_work);
		if (!device->hif_task_work)
			/* interrupted, exit */
			break;

		if (device->hif_shutdown)
			break;

		device->hif_task_work = 0;

		/* We want to hold the host over multiple cmds if possible;
		 * but holding the host blocks card interrupts.
		 */
		sdio_claim_host(device->func);

		for (;;) {
			hifdebug("pull next request\n");
			/* Pull the next request to work on */
			spin_lock_irqsave(&device->req_qlock, flags);
			request = device->req_qhead;
			if (!request) {
				spin_unlock_irqrestore(&device->req_qlock,
						       flags);
				break;
			}

			/* Remove request from queue */
			device->req_qhead = (struct bus_request *)request->next;
			/* Note: No need to clean up req_qtail */

			spin_unlock_irqrestore(&device->req_qlock, flags);

			/* call __hif_read_write to do the work */
			hifdebug("before HIFRW: address=0x%08x buffer=0x%pK\n",
				 request->address, request->buffer);
			hifdebug("before HIFRW: length=%d req_type=0x%08x\n",
				 request->length, request->req_type);

			if (request->req_type & HIF_WRITE) {
				int i;
				int dbgcount;

				if (request->length <= 16)
					dbgcount = request->length;
				else
					dbgcount = 16;

				for (i = 0; i < dbgcount; i++)
					hifdebug("|0x%02x", request->buffer[i]);
				hifdebug("\n");
			}
			status = __hif_read_write(
			    device, request->address, request->buffer,
			    request->length,
			    request->req_type & ~HIF_SYNCHRONOUS, NULL);
			hifdebug("after HIFRW: address=0x%08x buffer=0x%pK\n",
				 request->address, request->buffer);
			hifdebug("after HIFRW: length=%d req_type=0x%08x\n",
				 request->length, request->req_type);

			if (request->req_type & HIF_READ) {
				int i;
				int dbgcount;

				if (request->length <= 16)
					dbgcount = request->length;
				else
					dbgcount = 16;

				for (i = 0; i < dbgcount; i++)
					hifdebug("|0x%02x", request->buffer[i]);
				hifdebug("\n");
			}

			/* When we return, the read/write is done */
			request->status = status;

			if (request->req_type & HIF_ASYNCHRONOUS)
				hif_request_complete(device, request);
			else
				/* notify thread that's waiting on this request
				 */
				complete(&request->comp_req);
		}
		sdio_release_host(device->func);
	}

	complete_and_exit(&device->hif_exit, 0);
	return 0;
}

int hif_configure_device(void *hif_device,
			 enum hif_device_config_opcode opcode,
			 void *config, u32 config_len)
{
	int status = HIF_OK;
	struct hif_device *device = (struct hif_device *)hif_device;

	switch (opcode) {
	case HIF_DEVICE_GET_MBOX_BLOCK_SIZE:
		((u32 *)config)[0] = HIF_MBOX0_BLOCK_SIZE;
		((u32 *)config)[1] = HIF_MBOX1_BLOCK_SIZE;
		((u32 *)config)[2] = HIF_MBOX2_BLOCK_SIZE;
		((u32 *)config)[3] = HIF_MBOX3_BLOCK_SIZE;
		break;

	case HIF_DEVICE_SET_CONTEXT:
		device->context = config;
		break;

	case HIF_DEVICE_GET_CONTEXT:
		if (!config)
			return HIF_ERROR;
		*(void **)config = device->context;
		break;

	default:
		status = HIF_ERROR;
	}

	return status;
}

void hif_shutdown_device(void *device)
{
	if (!device) {
		int i;
		/* since we are unloading the driver, reset all cards
		 * in case the SDIO card is externally powered and we
		 * are unloading the SDIO stack.  This avoids the problem
		 * when the SDIO stack is reloaded and attempts are made
		 * to re-enumerate a card that is already enumerated.
		 */

		/* Unregister with bus driver core */
		if (registered) {
			registered = 0;
			hif_sdio_unregister_driver();
			WARN_ON(1);
			return;
		}

		for (i = 0; i < MAX_HIF_DEVICES; ++i) {
			if (hif_devices[i] && !hif_devices[i]->func) {
				del_hif_device(hif_devices[i]);
				hif_devices[i] = NULL;
			}
		}
	}
}

static void hif_irq_handler(struct sdio_func *func)
{
	int status;
	struct hif_device *device;

	device = get_hif_device(func);
	device->irq_handling = 1;
	/* release the host during ints so we can pick it back up when we
	 * process cmds
	 */
	sdio_release_host(device->func);
	status = device->cbs_from_hif.dsr_hdl(device->cbs_from_hif.context);
	sdio_claim_host(device->func);
	device->irq_handling = 0;
}

static void hif_force_driver_strength(struct sdio_func *func)
{
	unsigned int addr = SDIO_CCCR_DRIVE_STRENGTH;
	unsigned char value = 0;

	if (func0_CMD52_read_byte(func->card, addr, &value))
		goto cmd_fail;

	value = (value & (~(SDIO_DRIVE_DTSx_MASK << SDIO_DRIVE_DTSx_SHIFT))) |
			SDIO_DTSx_SET_TYPE_D;
	if (func0_CMD52_write_byte(func->card, addr, value))
		goto cmd_fail;

	addr = CCCR_SDIO_DRIVER_STRENGTH_ENABLE_ADDR;
	value = 0;
	if (func0_CMD52_read_byte(func->card, addr, &value))
		goto cmd_fail;

	value = (value & (~CCCR_SDIO_DRIVER_STRENGTH_ENABLE_MASK)) |
			CCCR_SDIO_DRIVER_STRENGTH_ENABLE_A |
			CCCR_SDIO_DRIVER_STRENGTH_ENABLE_C |
			CCCR_SDIO_DRIVER_STRENGTH_ENABLE_D;
	if (func0_CMD52_write_byte(func->card, addr, value))
		goto cmd_fail;
	return;
cmd_fail:
	hifdebug("set fail\n");
}

static int hif_set_mmc_buswidth(struct sdio_func *func,
				struct hif_device *device)
{
	int ret = -1;

	if (hif_mmcbuswidth == 1) {
		ret = func0_CMD52_write_byte(func->card, SDIO_CCCR_IF,
					     SDIO_BUS_CD_DISABLE |
					     SDIO_BUS_WIDTH_1BIT);
		if (ret)
			return ret;
		device->host->ios.bus_width = MMC_BUS_WIDTH_1;
		device->host->ops->set_ios(device->host, &device->host->ios);
	} else if (hif_mmcbuswidth == 4 &&
		   (device->host->caps & MMC_CAP_4_BIT_DATA)) {
		ret = func0_CMD52_write_byte(func->card, SDIO_CCCR_IF,
					     SDIO_BUS_CD_DISABLE |
					     SDIO_BUS_WIDTH_4BIT);
		if (ret)
			return ret;
		device->host->ios.bus_width = MMC_BUS_WIDTH_4;
		device->host->ops->set_ios(device->host, &device->host->ios);
	}
#ifdef SDIO_BUS_WIDTH_8BIT
	else if (hif_mmcbuswidth == 8 &&
		 (device->host->caps & MMC_CAP_8_BIT_DATA)) {
		ret = func0_CMD52_write_byte(func->card, SDIO_CCCR_IF,
					     SDIO_BUS_CD_DISABLE |
					     SDIO_BUS_WIDTH_8BIT);
		if (ret)
			return ret;
		device->host->ios.bus_width = MMC_BUS_WIDTH_8;
		device->host->ops->set_ios(device->host, &device->host->ios);
	}
#endif /* SDIO_BUS_WIDTH_8BIT */
	return ret;
}

static int hif_device_inserted(struct sdio_func *func,
			       const struct sdio_device_id *id)
{
	int i;
	int ret = -1;
	struct hif_device *device = NULL;
	int count;

	hifdebug("Enter\n");

	/* dma_mask should be populated here.
	 * Use the parent device's setting.
	 */
	func->dev.dma_mask = mmc_dev(func->card->host)->dma_mask;

	if (!add_hif_device(func))
		return ret;
	device = get_hif_device(func);

	for (i = 0; i < MAX_HIF_DEVICES; ++i) {
		if (!hif_devices[i]) {
			hif_devices[i] = device;
			break;
		}
	}
	if (WARN_ON(i >= MAX_HIF_DEVICES))
		return ret;

	device->id = id;
	device->host = func->card->host;
	device->is_enabled = false;

	{
		u32 clock, clock_set = SDIO_CLOCK_FREQUENCY_DEFAULT;

		sdio_claim_host(func);

		/* force driver strength to type D */
		if (hif_forcedriverstrength == 1)
			hif_force_driver_strength(func);

		if (hif_writecccr1)
			(void)func0_CMD52_write_byte(func->card, hif_writecccr1,
						     hif_writecccr1value);
		if (hif_writecccr2)
			(void)func0_CMD52_write_byte(func->card, hif_writecccr2,
						     hif_writecccr2value);
		if (hif_writecccr3)
			(void)func0_CMD52_write_byte(func->card, hif_writecccr3,
						     hif_writecccr3value);
		if (hif_writecccr4)
			(void)func0_CMD52_write_byte(func->card, hif_writecccr4,
						     hif_writecccr4value);
		/* Set MMC Clock */
		if (hif_mmcclock > 0)
			clock_set = hif_mmcclock;
		if (mmc_card_hs(func->card))
			clock = 50000000;
		else
			clock = func->card->cis.max_dtr;
		if (clock > device->host->f_max)
			clock = device->host->f_max;
		hifdebug("clock is %d", clock);

		/* only when hif_mmcclock module parameter is specified,
		 * set the clock explicitly
		 */
		if (hif_mmcclock > 0) {
			device->host->ios.clock = clock_set;
			device->host->ops->set_ios(device->host,
						   &device->host->ios);
		}
		/* Set MMC Bus Width: 1-1Bit, 4-4Bit, 8-8Bit */
		if (hif_mmcbuswidth > 0)
			ret = hif_set_mmc_buswidth(func, device);

		sdio_release_host(func);
	}

	spin_lock_init(&device->req_free_qlock);
	spin_lock_init(&device->req_qlock);

	/* Initialize the bus requests to be used later */
	memset(device->bus_request, 0, sizeof(device->bus_request));
	for (count = 0; count < BUS_REQUEST_MAX_NUM; count++) {
		init_completion(&device->bus_request[count].comp_req);
		hif_free_bus_request(device, &device->bus_request[count]);
	}
	init_waitqueue_head(&device->hif_wait);
	spin_lock_init(&device->compl_qlock);
	init_waitqueue_head(&device->completion_wait);

	ret = hif_enable_func(device, func);
	if ((ret == HIF_OK) || (ret == HIF_PENDING)) {
		hifdebug("Function is ENABLED");
		return 0;
	}

	for (i = 0; i < MAX_HIF_DEVICES; i++) {
		if (hif_devices[i] == device) {
			hif_devices[i] = NULL;
			break;
		}
	}
	sdio_set_drvdata(func, NULL);
	del_hif_device(device);
	return ret;
}

void hif_un_mask_interrupt(void *hif_device)
{
	struct hif_device *device = (struct hif_device *)hif_device;

	if (!device || !device->func)
		return;

	/* Unmask our function IRQ */
	sdio_claim_host(device->func);
	device->func->card->host->ops->enable_sdio_irq(device->func->card->host,
						       1);
	device->is_intr_enb = true;
	sdio_release_host(device->func);
}

void hif_mask_interrupt(void *hif_device)
{
	struct hif_device *device = (struct hif_device *)hif_device;

	if (!device || !device->func)
		return;

	/* Mask our function IRQ */
	sdio_claim_host(device->func);
	device->func->card->host->ops->enable_sdio_irq(device->func->card->host,
						       0);
	device->is_intr_enb = false;
	sdio_release_host(device->func);
}

static struct bus_request *hif_allocate_bus_request(void *hif_device)
{
	struct bus_request *busrequest;
	unsigned long flag;
	struct hif_device *device = (struct hif_device *)hif_device;

	spin_lock_irqsave(&device->req_free_qlock, flag);
	/* Remove first in list */
	busrequest = device->bus_req_free_qhead;
	if (busrequest)
		device->bus_req_free_qhead =
			(struct bus_request *)busrequest->next;
	spin_unlock_irqrestore(&device->req_free_qlock, flag);

	return busrequest;
}

static void hif_free_bus_request(struct hif_device *device,
				 struct bus_request *busrequest)
{
	unsigned long flag;

	if (!busrequest)
		return;

	busrequest->next = NULL;

	/* Insert first in list */
	spin_lock_irqsave(&device->req_free_qlock, flag);
	busrequest->next = (struct bus_request *)device->bus_req_free_qhead;
	device->bus_req_free_qhead = busrequest;
	spin_unlock_irqrestore(&device->req_free_qlock, flag);
}

static int hif_disable_func(struct hif_device *device, struct sdio_func *func)
{
	int ret;
	int status = HIF_OK;

	device = get_hif_device(func);

	hif_stop_completion_thread(device);
	hif_stop_hif_task(device);

	/* Disable the card */
	sdio_claim_host(device->func);
	ret = sdio_disable_func(device->func);
	if (ret)
		status = HIF_ERROR;

	if (hif_reset_sdio_on_unload && (status == HIF_OK)) {
		/* Reset the SDIO interface.  This is useful in
		 * automated testing where the card does not need
		 * to be removed at the end of the test.  It is
		 * expected that the user will also unload/reload
		 * the host controller driver to force the bus driver
		 * to re-enumerate the slot.
		 */

		/* NOTE : sdio_f0_writeb() cannot be used here, that API only
		 * allows access to undefined registers in the range of:
		 * 0xF0-0xFF
		 */

		ret = func0_CMD52_write_byte(device->func->card,
					     SDIO_CCCR_ABORT, (1 << 3));
		if (ret)
			status = HIF_ERROR;
	}

	sdio_release_host(device->func);

	if (status == HIF_OK)
		device->is_enabled = false;
	return status;
}

static int hif_enable_func(struct hif_device *device, struct sdio_func *func)
{
	int ret = HIF_OK;

	device = get_hif_device(func);

	if (!device)
		return HIF_EINVAL;

	if (!device->is_enabled) {
		/* enable the SDIO function */
		sdio_claim_host(func);

		/* give us some time to enable, in ms */
		func->enable_timeout = ENABLE_SDIO_TIMEOUT;
		ret = sdio_enable_func(func);
		if (ret) {
			sdio_release_host(func);
			return HIF_ERROR;
		}
		ret = sdio_set_block_size(func, HIF_MBOX_BLOCK_SIZE);

		sdio_release_host(func);
		if (ret)
			return HIF_ERROR;
		device->is_enabled = true;

		if (!device->completion_task) {
			device->compl_qhead = NULL;
			device->compl_qtail = NULL;
			device->completion_shutdown = 0;
			device->completion_task = kthread_create(
			    completion_task, (void *)device, "HIFCompl");
			if (IS_ERR(device->completion_task)) {
				device->completion_shutdown = 1;
				return HIF_ERROR;
			}
			wake_up_process(device->completion_task);
		}

		/* create HIF I/O thread */
		if (!device->hif_task) {
			device->hif_shutdown = 0;
			device->hif_task =
			    kthread_create(hif_task, (void *)device, "HIF");
			if (IS_ERR(device->hif_task)) {
				device->hif_shutdown = 1;
				return HIF_ERROR;
			}
			wake_up_process(device->hif_task);
		}
	}

	if (!device->claimed_context) {
		ret = hif_callbacks.dev_inserted_hdl(hif_callbacks.context,
						     device);
		if (ret != HIF_OK) {
			/* Disable the SDIO func & Reset the sdio
			 * for automated tests to move ahead, where
			 * the card does not need to be removed at
			 * the end of the test.
			 */
			hif_disable_func(device, func);
		}
		(void)sdio_claim_irq(func, hif_irq_handler);
	}

	return ret;
}

static void hif_device_removed(struct sdio_func *func)
{
	int i;
	int status = HIF_OK;
	struct hif_device *device;

	device = get_hif_device(func);
	if (!device)
		return;

	for (i = 0; i < MAX_HIF_DEVICES; ++i) {
		if (hif_devices[i] == device)
			hif_devices[i] = NULL;
	}

	if (device->claimed_context) {
		status = hif_callbacks.dev_removed_hdl(
		    device->claimed_context, device);
	}

	/* TBD: Release IRQ (opposite of sdio_claim_irq) */
	hif_mask_interrupt(device);

	if (device->is_enabled)
		status = hif_disable_func(device, func);

	del_hif_device(device);
}

static void *add_hif_device(struct sdio_func *func)
{
	struct hif_device *hifdevice = NULL;

	if (!func)
		return NULL;

	hifdevice = kmalloc(sizeof(*hifdevice), GFP_KERNEL);
	if (!hifdevice)
		return NULL;

	memset(hifdevice, 0, sizeof(*hifdevice));
	hifdevice->func = func;
	sdio_set_drvdata(func, hifdevice);

	return (void *)hifdevice;
}

static struct hif_device *get_hif_device(struct sdio_func *func)
{
	return (struct hif_device *)sdio_get_drvdata(func);
}

static void del_hif_device(struct hif_device *device)
{
	if (!device)
		return;
	kfree(device);
}

void hif_claim_device(void *hif_device, void *context)
{
	struct hif_device *device = (struct hif_device *)hif_device;

	device->claimed_context = context;
}

void hif_release_device(void *hif_device)
{
	struct hif_device *device = (struct hif_device *)hif_device;

	device->claimed_context = NULL;
}

int hif_attach(void *hif_device, struct cbs_from_hif *callbacks)
{
	struct hif_device *device = (struct hif_device *)hif_device;

	if (device->cbs_from_hif.context) {
		/* already in use! */
		return HIF_ERROR;
	}
	device->cbs_from_hif = *callbacks;
	return HIF_OK;
}

static void hif_stop_hif_task(struct hif_device *device)
{
	if (device->hif_task) {
		init_completion(&device->hif_exit);
		device->hif_shutdown = 1;
		device->hif_task_work = 1;
		wake_up(&device->hif_wait);
		wait_for_completion(&device->hif_exit);
		device->hif_task = NULL;
	}
}

/* hif_reset_target() - Reset target device
 * @struct hif_device: pointer to struct hif_device structure
 *
 * Reset the target by invoking power off and power on
 * sequence to bring back target into active state.
 * This API shall be called only when driver load/unload
 * is in progress.
 *
 * Return: 0 on success, error for failure case.
 */
static int hif_reset_target(struct hif_device *hif_device)
{
	int ret;

	if (!hif_device || !hif_device->func || !hif_device->func->card)
		return -ENODEV;
	/* Disable sdio func->pull down WLAN_EN-->pull down DAT_2 line */
	ret = mmc_power_save_host(hif_device->func->card->host);
	if (ret)
		goto done;

	/* pull up DAT_2 line->pull up WLAN_EN-->Enable sdio func */
	ret = mmc_power_restore_host(hif_device->func->card->host);

done:
	return ret;
}

void hif_detach(void *hif_device)
{
	struct hif_device *device = (struct hif_device *)hif_device;

	hif_stop_hif_task(device);
	if (device->ctrl_response_timeout) {
		/* Reset the target by invoking power off and power on sequence
		 * to the card to bring back into active state.
		 */
		if (hif_reset_target(device))
			panic("BUG");
		device->ctrl_response_timeout = false;
	}

	memset(&device->cbs_from_hif, 0, sizeof(device->cbs_from_hif));
}

#define SDIO_SET_CMD52_ARG(arg, rw, func, raw, address, writedata) \
	((arg) = ((((rw) & 1) << 31) | (((func) & 0x7) << 28) | \
		 (((raw) & 1) << 27) | (1 << 26) | \
		 (((address) & 0x1FFFF) << 9) | (1 << 8) | \
		 ((writedata) & 0xFF)))

#define SDIO_SET_CMD52_READ_ARG(arg, func, address) \
	SDIO_SET_CMD52_ARG(arg, 0, (func), 0, address, 0x00)
#define SDIO_SET_CMD52_WRITE_ARG(arg, func, address, value) \
	SDIO_SET_CMD52_ARG(arg, 1, (func), 0, address, value)

static int func0_CMD52_write_byte(struct mmc_card *card, unsigned int address,
				  unsigned char byte)
{
	struct mmc_command ioCmd;
	unsigned long arg;
	int status;

	memset(&ioCmd, 0, sizeof(ioCmd));
	SDIO_SET_CMD52_WRITE_ARG(arg, 0, address, byte);
	ioCmd.opcode = SD_IO_RW_DIRECT;
	ioCmd.arg = arg;
	ioCmd.flags = MMC_RSP_R5 | MMC_CMD_AC;
	status = mmc_wait_for_cmd(card->host, &ioCmd, 0);

	return status;
}

static int func0_CMD52_read_byte(struct mmc_card *card, unsigned int address,
				 unsigned char *byte)
{
	struct mmc_command ioCmd;
	unsigned long arg;
	s32 err;

	memset(&ioCmd, 0, sizeof(ioCmd));
	SDIO_SET_CMD52_READ_ARG(arg, 0, address);
	ioCmd.opcode = SD_IO_RW_DIRECT;
	ioCmd.arg = arg;
	ioCmd.flags = MMC_RSP_R5 | MMC_CMD_AC;

	err = mmc_wait_for_cmd(card->host, &ioCmd, 0);

	if ((!err) && (byte))
		*byte = ioCmd.resp[0] & 0xFF;

	return err;
}

void hif_set_handle(void *hif_handle, void *handle)
{
	struct hif_device *device = (struct hif_device *)hif_handle;

	device->caller_handle = handle;
}

size_t hif_get_device_size(void)
{
	return sizeof(struct hif_device);
}
