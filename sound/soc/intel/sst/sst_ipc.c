/*
 *  sst_ipc.c - Intel SST Driver for audio engine
 *
 *  Copyright (C) 2008-10 Intel Corporation
 *  Authors:	Vinod Koul <vinod.koul@intel.com>
 *		Harsha Priya <priya.harsha@intel.com>
 *		Dharageswari R <dharageswari.r@intel.com>
 *		KP Jeeja <jeeja.kp@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This file defines all ipc functions
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/pci.h>
#include <linux/firmware.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <sound/intel_sst_ioctl.h>
#include "../sst_platform.h"
#include "../platform_ipc_v2.h"
#include "sst.h"
#include "sst_trace.h"

void sst_dump_to_buffer(const void *from, size_t len, char *buf)
{
	int i, end;
	const unsigned char *cmd = from;

	if (len == 0) {
		buf[0] = '\0';
		return;
	}

	for (end = len - 1; end >= 0; end--)
		if (cmd[end])
			break;
	end++;

	buf += snprintf(buf, 3, "%02x", cmd[0]);
	for (i = 1; i < len; i++) {
		buf += snprintf(buf, 4, " %02x", cmd[i]);
		if (i == end && end != len - 1) {
			sprintf(buf, "...");
			break;
		}
	}
}

struct sst_block *sst_create_block(struct intel_sst_drv *ctx,
					u32 msg_id, u32 drv_id)
{
	struct sst_block *msg = NULL;

	pr_debug("in %s\n", __func__);
	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	if (!msg) {
		pr_err("kzalloc block failed\n");
		return NULL;
	}
	msg->condition = false;
	msg->on = true;
	msg->msg_id = msg_id;
	msg->drv_id = drv_id;
	spin_lock_bh(&ctx->block_lock);
	list_add_tail(&msg->node, &ctx->block_list);
	spin_unlock_bh(&ctx->block_lock);

	return msg;
}

int sst_wake_up_block(struct intel_sst_drv *ctx, int result,
		u32 drv_id, u32 ipc, void *data, u32 size)
{
	struct sst_block *block = NULL;

	pr_debug("in %s\n", __func__);
	spin_lock_bh(&ctx->block_lock);
	list_for_each_entry(block, &ctx->block_list, node) {
		pr_debug("Block ipc %d, drv_id %d\n", block->msg_id,
							block->drv_id);
		if (block->msg_id == ipc && block->drv_id == drv_id) {
			pr_debug("free up the block\n");
			block->ret_code = result;
			block->data = data;
			block->size = size;
			block->condition = true;
			spin_unlock_bh(&ctx->block_lock);
			wake_up(&ctx->wait_queue);
			return 0;
		}
	}
	spin_unlock_bh(&ctx->block_lock);
	pr_debug("Block not found or a response is received for a short message for ipc %d, drv_id %d\n",
			ipc, drv_id);
	return -EINVAL;
}

int sst_free_block(struct intel_sst_drv *ctx, struct sst_block *freed)
{
	struct sst_block *block = NULL, *__block;

	pr_debug("in %s\n", __func__);
	spin_lock_bh(&ctx->block_lock);
	list_for_each_entry_safe(block, __block, &ctx->block_list, node) {
		if (block == freed) {
			list_del(&freed->node);
			kfree(freed->data);
			freed->data = NULL;
			kfree(freed);
			spin_unlock_bh(&ctx->block_lock);
			return 0;
		}
	}
	spin_unlock_bh(&ctx->block_lock);
	return -EINVAL;
}

/*
 * sst_send_runtime_param - send runtime param to SST
 *
 * this function sends the runtime parameter to sst dsp engine
 */
static int sst_send_runtime_param(struct snd_sst_runtime_params *params)
{
	struct ipc_post *msg = NULL;
	int ret_val;

	pr_debug("Enter:%s\n", __func__);
	ret_val = sst_create_ipc_msg(&msg, true);
	if (ret_val)
		return ret_val;
	sst_fill_header(&msg->header, IPC_IA_SET_RUNTIME_PARAMS, 1,
							params->str_id);
	msg->header.part.data = sizeof(u32) + sizeof(*params) - sizeof(params->addr)
				+ params->size;
	memcpy(msg->mailbox_data, &msg->header.full, sizeof(u32));
	memcpy(msg->mailbox_data + sizeof(u32), params, sizeof(*params)
				- sizeof(params->addr));
	/* driver doesn't need to send address, so overwrite addr with data */
	memcpy(msg->mailbox_data + sizeof(u32) + sizeof(*params)
			- sizeof(params->addr),
			params->addr, params->size);
	sst_add_to_dispatch_list_and_post(sst_drv_ctx, msg);
	return 0;
}

void sst_post_message_mrfld(struct work_struct *work)
{
	struct ipc_post *msg;
	union ipc_header_mrfld header;
	unsigned long irq_flags;

	pr_debug("Enter:%s\n", __func__);
	spin_lock_irqsave(&sst_drv_ctx->ipc_spin_lock, irq_flags);
	/* check list */
	if (list_empty(&sst_drv_ctx->ipc_dispatch_list)) {
		/* queue is empty, nothing to send */
		spin_unlock_irqrestore(&sst_drv_ctx->ipc_spin_lock, irq_flags);
		pr_debug("Empty msg queue... NO Action\n");
		return;
	}

	/* check busy bit */
	header.full = sst_shim_read64(sst_drv_ctx->shim, SST_IPCX);
	if (header.p.header_high.part.busy) {
		spin_unlock_irqrestore(&sst_drv_ctx->ipc_spin_lock, irq_flags);
		pr_debug("Busy not free... post later\n");
		return;
	}
	/* copy msg from list */
	msg = list_entry(sst_drv_ctx->ipc_dispatch_list.next,
			struct ipc_post, node);
	list_del(&msg->node);
	pr_debug("sst: size: = %x\n", msg->mrfld_header.p.header_low_payload);
	if (msg->mrfld_header.p.header_high.part.large)
		memcpy_toio(sst_drv_ctx->ipc_mailbox + SST_MAILBOX_SEND,
			    msg->mailbox_data, msg->mrfld_header.p.header_low_payload);

	trace_sst_ipc("POST  ->", msg->mrfld_header.p.header_high.full,
				  msg->mrfld_header.p.header_low_payload,
				  msg->mrfld_header.p.header_high.part.drv_id);
	trace_sst_ipc_mailbox(msg->mailbox_data, msg->mrfld_header.p.header_low_payload);
	sst_shim_write64(sst_drv_ctx->shim, SST_IPCX, msg->mrfld_header.full);
	spin_unlock_irqrestore(&sst_drv_ctx->ipc_spin_lock, irq_flags);
	pr_debug("sst: Post message: header = %x\n",
					msg->mrfld_header.p.header_high.full);
	kfree(msg->mailbox_data);
	kfree(msg);
	return;
}

/**
* sst_post_message - Posts message to SST
*
* @work: Pointer to work structure
*
* This function is called by any component in driver which
* wants to send an IPC message. This will post message only if
* busy bit is free
*/
void sst_post_message_mfld(struct work_struct *work)
{
	struct ipc_post *msg;
	union ipc_header header;
	unsigned long irq_flags;

	pr_debug("Enter:%s\n", __func__);

	spin_lock_irqsave(&sst_drv_ctx->ipc_spin_lock, irq_flags);
	/* check list */
	if (list_empty(&sst_drv_ctx->ipc_dispatch_list)) {
		/* queue is empty, nothing to send */
		spin_unlock_irqrestore(&sst_drv_ctx->ipc_spin_lock, irq_flags);
		pr_debug("Empty msg queue... NO Action\n");
		return;
	}

	/* check busy bit */
	header.full = sst_shim_read(sst_drv_ctx->shim, sst_drv_ctx->ipc_reg.ipcx);
	if (header.part.busy) {
		spin_unlock_irqrestore(&sst_drv_ctx->ipc_spin_lock, irq_flags);
		pr_debug("Busy not free... Post later\n");
		return;
	}
	/* copy msg from list */
	msg = list_entry(sst_drv_ctx->ipc_dispatch_list.next,
			struct ipc_post, node);
	list_del(&msg->node);
	pr_debug("size: = %x\n", msg->header.part.data);
	if (msg->header.part.large)
		memcpy_toio(sst_drv_ctx->ipc_mailbox + SST_MAILBOX_SEND,
			msg->mailbox_data, msg->header.part.data);

	sst_shim_write(sst_drv_ctx->shim, sst_drv_ctx->ipc_reg.ipcx, msg->header.full);
	spin_unlock_irqrestore(&sst_drv_ctx->ipc_spin_lock, irq_flags);
	pr_debug("Posted message: header = %x\n", msg->header.full);

	kfree(msg->mailbox_data);
	kfree(msg);
	return;
}

int sst_sync_post_message_mrfld(struct ipc_post *msg)
{
	union ipc_header_mrfld header;
	unsigned int loop_count = 0;
	int retval = 0;
	unsigned long irq_flags;

	pr_debug("Enter:%s\n", __func__);
	spin_lock_irqsave(&sst_drv_ctx->ipc_spin_lock, irq_flags);

	/* check busy bit */
	header.full = sst_shim_read64(sst_drv_ctx->shim, SST_IPCX);
	while (header.p.header_high.part.busy) {
		if (loop_count > 10) {
			pr_err("sst: Busy wait failed, cant send this msg\n");
			retval = -EBUSY;
			goto out;
		}
		udelay(500);
		loop_count++;
		header.full = sst_shim_read64(sst_drv_ctx->shim, SST_IPCX);
	}
	pr_debug("sst: Post message: header = %x\n",
					msg->mrfld_header.p.header_high.full);
	pr_debug("sst: size = 0x%x\n", msg->mrfld_header.p.header_low_payload);
	if (msg->mrfld_header.p.header_high.part.large)
		memcpy_toio(sst_drv_ctx->ipc_mailbox + SST_MAILBOX_SEND,
			msg->mailbox_data, msg->mrfld_header.p.header_low_payload);

	trace_sst_ipc("POST  ->", msg->mrfld_header.p.header_high.full,
				  msg->mrfld_header.p.header_low_payload,
				  msg->mrfld_header.p.header_high.part.drv_id);
	trace_sst_ipc_mailbox(msg->mailbox_data, msg->mrfld_header.p.header_low_payload);
	sst_shim_write64(sst_drv_ctx->shim, SST_IPCX, msg->mrfld_header.full);

out:
	spin_unlock_irqrestore(&sst_drv_ctx->ipc_spin_lock, irq_flags);
	kfree(msg->mailbox_data);
	kfree(msg);
	return retval;
}

/* use this for trigger ops to post syncronous msgs
 */
int sst_sync_post_message_mfld(struct ipc_post *msg)
{
	union ipc_header header;
	unsigned int loop_count = 0;
	int retval = 0;
	unsigned long irq_flags;

	pr_debug("Enter:%s\n", __func__);
	spin_lock_irqsave(&sst_drv_ctx->ipc_spin_lock, irq_flags);

	/* check busy bit */
	header.full = sst_shim_read(sst_drv_ctx->shim, sst_drv_ctx->ipc_reg.ipcx);
	while (header.part.busy) {
		if (loop_count > 10) {
			pr_err("busy wait failed, cant send this msg\n");
			retval = -EBUSY;
			goto out;
		}
		udelay(500);
		loop_count++;
		header.full = sst_shim_read(sst_drv_ctx->shim, sst_drv_ctx->ipc_reg.ipcx);
	}
	pr_debug("sst: Post message: header = %x\n", msg->header.full);
	if (msg->header.part.large)
		memcpy_toio(sst_drv_ctx->ipc_mailbox + SST_MAILBOX_SEND,
			msg->mailbox_data, msg->header.part.data);
	sst_shim_write(sst_drv_ctx->shim, sst_drv_ctx->ipc_reg.ipcx, msg->header.full);

out:
	spin_unlock_irqrestore(&sst_drv_ctx->ipc_spin_lock, irq_flags);
	kfree(msg->mailbox_data);
	kfree(msg);

	return retval;
}

/*
 * sst_clear_interrupt - clear the SST FW interrupt
 *
 * This function clears the interrupt register after the interrupt
 * bottom half is complete allowing next interrupt to arrive
 */
void intel_sst_clear_intr_mfld(void)
{
	union interrupt_reg isr;
	union interrupt_reg imr;
	union ipc_header clear_ipc;
	unsigned long irq_flags;

	spin_lock_irqsave(&sst_drv_ctx->ipc_spin_lock, irq_flags);
	imr.full = sst_shim_read(sst_drv_ctx->shim, SST_IMRX);
	isr.full = sst_shim_read(sst_drv_ctx->shim, SST_ISRX);
	/*  write 1 to clear  */;
	isr.part.busy_interrupt = 1;
	sst_shim_write(sst_drv_ctx->shim, SST_ISRX, isr.full);
	/* Set IA done bit */
	clear_ipc.full = sst_shim_read(sst_drv_ctx->shim, sst_drv_ctx->ipc_reg.ipcd);
	clear_ipc.part.busy = 0;
	clear_ipc.part.done = 1;
	clear_ipc.part.data = IPC_ACK_SUCCESS;
	sst_shim_write(sst_drv_ctx->shim, sst_drv_ctx->ipc_reg.ipcd, clear_ipc.full);
	/* un mask busy interrupt */
	imr.part.busy_interrupt = 0;
	imr.part.done_interrupt = 0;
	sst_shim_write(sst_drv_ctx->shim, SST_IMRX, imr.full);
	spin_unlock_irqrestore(&sst_drv_ctx->ipc_spin_lock, irq_flags);
}


void intel_sst_clear_intr_mrfld(void)
{
	union interrupt_reg_mrfld isr;
	union interrupt_reg_mrfld imr;
	union ipc_header_mrfld clear_ipc;
	unsigned long irq_flags;

	spin_lock_irqsave(&sst_drv_ctx->ipc_spin_lock, irq_flags);
	imr.full = sst_shim_read64(sst_drv_ctx->shim, SST_IMRX);
	isr.full = sst_shim_read64(sst_drv_ctx->shim, SST_ISRX);

	/*  write 1 to clear  */
	isr.part.busy_interrupt = 1;
	sst_shim_write64(sst_drv_ctx->shim, SST_ISRX, isr.full);

	/* Set IA done bit */
	clear_ipc.full = sst_shim_read64(sst_drv_ctx->shim, SST_IPCD);

	clear_ipc.p.header_high.part.busy = 0;
	clear_ipc.p.header_high.part.done = 1;
	clear_ipc.p.header_low_payload = IPC_ACK_SUCCESS;
	sst_shim_write64(sst_drv_ctx->shim, SST_IPCD, clear_ipc.full);
	/* un mask busy interrupt */
	imr.part.busy_interrupt = 0;
	sst_shim_write64(sst_drv_ctx->shim, SST_IMRX, imr.full);
	spin_unlock_irqrestore(&sst_drv_ctx->ipc_spin_lock, irq_flags);
}


/*
 * process_fw_init - process the FW init msg
 *
 * @msg: IPC message mailbox data from FW
 *
 * This function processes the FW init msg from FW
 * marks FW state and prints debug info of loaded FW
 */
static void process_fw_init(void *msg)
{
	struct ipc_header_fw_init *init =
		(struct ipc_header_fw_init *)msg;
	int retval = 0;

	pr_debug("*** FW Init msg came***\n");
	if (init->result) {
		sst_drv_ctx->sst_state =  SST_RESET;
		pr_debug("FW Init failed, Error %x\n", init->result);
		pr_err("FW Init failed, Error %x\n", init->result);
		retval = init->result;
		goto ret;
	}
	pr_info("FW Version %02x.%02x.%02x.%02x\n",
		init->fw_version.type, init->fw_version.major,
		init->fw_version.minor, init->fw_version.build);
	pr_debug("Build date %s Time %s\n",
			init->build_info.date, init->build_info.time);

	/* If there any runtime parameter to set, send it */
	if (sst_drv_ctx->runtime_param.param.addr)
		sst_send_runtime_param(&(sst_drv_ctx->runtime_param.param));

ret:
	sst_wake_up_block(sst_drv_ctx, retval, FW_DWNL_ID, 0 , NULL, 0);
}
/**
* sst_process_message_mfld - Processes message from SST
*
* @work:	Pointer to work structure
*
* This function is scheduled by ISR
* It take a msg from process_queue and does action based on msg
*/
void sst_process_message_mfld(struct ipc_post *msg)
{
	int str_id;
	struct stream_info *stream;

	str_id = msg->header.part.str_id;
	pr_debug("IPC process for %x\n", msg->header.full);
	/* based on msg in list call respective handler */
	switch (msg->header.part.msg_id) {
	case IPC_SST_PERIOD_ELAPSED:
		if (sst_validate_strid(str_id)) {
			pr_err("stream id %d invalid\n", str_id);
			break;
		}
		stream = &sst_drv_ctx->streams[str_id];
		if (stream->period_elapsed)
			stream->period_elapsed(stream->pcm_substream);
		break;
	case IPC_SST_BUF_UNDER_RUN:
	case IPC_SST_BUF_OVER_RUN:
		if (sst_validate_strid(str_id)) {
			pr_err("stream id %d invalid\n", str_id);
			break;
		}
		pr_err("Buffer under/overrun for %d\n",
				msg->header.part.str_id);
		pr_err("Got Underrun & not to send data...ignore\n");
		break;

	case IPC_SST_FRAGMENT_ELPASED: {
		pr_debug("IPC_SST_FRAGMENT_ELPASED for %d", str_id);
		sst_cdev_fragment_elapsed(str_id);
		break;
	}

	case IPC_IA_PRINT_STRING:
		pr_debug("been asked to print something by fw\n");
		/* TBD */
		break;

	case IPC_IA_FW_INIT_CMPLT: {
		/* send next data to FW */
		process_fw_init(msg->mailbox_data);
		break;
	}

	case IPC_SST_STREAM_PROCESS_FATAL_ERR:
		if (sst_validate_strid(str_id)) {
			pr_err("stream id %d invalid\n", str_id);
			break;
		}
		pr_err("codec fatal error %x stream %d...\n",
				msg->header.full, msg->header.part.str_id);
		pr_err("Dropping the stream\n");
		sst_drop_stream(msg->header.part.str_id);
		break;
	default:
		/* Illegal case */
		pr_err("Unhandled msg %x header %x\n",
		msg->header.part.msg_id, msg->header.full);
	}
	return;
}

/**
* sst_process_message - Processes message from SST
*
* @work:	Pointer to work structure
*
* This function is scheduled by ISR
* It take a msg from process_queue and does action based on msg
*/

void sst_process_message_mrfld(struct ipc_post *msg)
{
	int str_id;

	str_id = msg->mrfld_header.p.header_high.part.drv_id;

	pr_debug("IPC process message header %x payload %x\n",
			msg->mrfld_header.p.header_high.full,
			msg->mrfld_header.p.header_low_payload);

	return;
}


static int send_vtsv_result_event(void *data, int size)
{
	/* Add 2 bytes for valid data in bytecontrol */
	u8 result[VTSV_MAX_TOTAL_RESULT_ARRAY_SIZE];
	struct sst_platform_cb_params	cb_params;

	if (!data) {
		pr_err("Data pointer Null into %s\n", __func__);
		return -EINVAL;
	}
	if (size > VTSV_MAX_TOTAL_RESULT_SIZE) {
		pr_err("VTSV result size exceeds expected value\n");
		return -EINVAL;
	}

	/* First element in byte controls which is 16 bits is size of the data
	 */
	result[0] = size;
	/* Second element is reserved just to maintain 16-bit alignment. App
	 * parses VTSV  data in terms of 16-bit key value pair
	 */
	result[1] = 0x0;

	/* We are filling actual data from index 2 since first two bytes in
	 * array specify valid data size in an array
	 */
	memcpy(&result[2], (char *)data, size);
	cb_params.event =  SST_PLATFORM_VTSV_READ_EVENT;
	cb_params.params = &result;

	print_hex_dump_bytes(__func__, DUMP_PREFIX_NONE, (char *)data, size);

	return sst_platform_cb(&cb_params);
}

static void process_fw_async_msg(struct ipc_post *msg)
{
	u32 msg_id;
	int str_id;
	int res_size, ret;
	u32 data_size, i;
	void *data_offset;
	struct stream_info *stream;
	union ipc_header_high msg_high;
	u32 msg_low, pipe_id;

	msg_high = msg->mrfld_header.p.header_high;
	msg_low = msg->mrfld_header.p.header_low_payload;
	msg_id = ((struct ipc_dsp_hdr *)msg->mailbox_data)->cmd_id;
	data_offset = (msg->mailbox_data + sizeof(struct ipc_dsp_hdr));
	data_size =  msg_low - (sizeof(struct ipc_dsp_hdr));

	switch (msg_id) {
	case IPC_SST_PERIOD_ELAPSED_MRFLD:
		pipe_id = ((struct ipc_dsp_hdr *)msg->mailbox_data)->pipe_id;
		str_id = get_stream_id_mrfld(pipe_id);
		if (str_id > 0) {
			pr_debug("Period elapsed rcvd for pipe id 0x%x\n", pipe_id);
			stream = &sst_drv_ctx->streams[str_id];
			if (stream->period_elapsed)
				stream->period_elapsed(stream->pcm_substream);
			if (stream->compr_cb)
				stream->compr_cb(stream->compr_cb_param);
			sst_update_timer(sst_drv_ctx);

		}
		break;

	case IPC_IA_DRAIN_STREAM_MRFLD:
		pipe_id = ((struct ipc_dsp_hdr *)msg->mailbox_data)->pipe_id;
		str_id = get_stream_id_mrfld(pipe_id);
		if (str_id > 0) {
			stream = &sst_drv_ctx->streams[str_id];
			if (stream->drain_notify)
				stream->drain_notify(stream->drain_cb_param);
		}
		break;

	case IPC_IA_FW_ASYNC_ERR_MRFLD:
		pr_err("FW sent async error msg:\n");
		for (i = 0; i < (data_size/4); i++)
			pr_err("0x%x\n", (*((unsigned int *)data_offset + i)));
		break;

	case IPC_IA_VTSV_DETECTED:
		res_size = data_size;
		ret = send_vtsv_result_event(data_offset, res_size);
		if (ret)
			pr_err("VTSV uevent send failed: %d\n", ret);
		else
			pr_debug("VTSV uevent sent\n");
		break;

	case IPC_IA_FW_INIT_CMPLT_MRFLD:
		process_fw_init(data_offset);
		break;

	case IPC_IA_BUF_UNDER_RUN_MRFLD:
		pipe_id = ((struct ipc_dsp_hdr *)msg->mailbox_data)->pipe_id;
		str_id = get_stream_id_mrfld(pipe_id);
		if (str_id > 0)
			pr_err("Buffer under-run for pipe:%#x str_id:%d\n",
					pipe_id, str_id);
		break;

	default:
		pr_err("Unrecognized async msg from FW msg_id %#x\n", msg_id);
	}
}

void sst_process_reply_mrfld(struct ipc_post *msg)
{
	unsigned int drv_id;
	void *data;
	union ipc_header_high msg_high;
	u32 msg_low;
	struct ipc_dsp_hdr *dsp_hdr;
	unsigned int cmd_id;

	msg_high = msg->mrfld_header.p.header_high;
	msg_low = msg->mrfld_header.p.header_low_payload;

	pr_debug("IPC process message header %x payload %x\n",
			msg->mrfld_header.p.header_high.full,
			msg->mrfld_header.p.header_low_payload);

	drv_id = msg_high.part.drv_id;

	/* Check for async messages */
	if (drv_id == SST_ASYNC_DRV_ID) {
		/* FW sent async large message */
		process_fw_async_msg(msg);
		goto end;
	}

	/* FW sent short error response for an IPC */
	if (msg_high.part.result && drv_id && !msg_high.part.large) {
		/* 32-bit FW error code in msg_low */
		pr_err("FW sent error response 0x%x", msg_low);
		sst_wake_up_block(sst_drv_ctx, msg_high.part.result,
			msg_high.part.drv_id,
			msg_high.part.msg_id, NULL, 0);
		goto end;
	}

	/* Process all valid responses */
	/* if it is a large message, the payload contains the size to
	 * copy from mailbox */
	if (msg_high.part.large) {

		if (!msg_low) {
			pr_err("payload size is 0 for large message\n");
			pr_err("IPC header %#x has %#x payload\n",
					msg_high.full, msg_low);

			sst_wake_up_block(sst_drv_ctx, msg_high.part.result,
					msg_high.part.drv_id,
					msg_high.part.msg_id, NULL, 0);
			WARN_ON(1);
			return;
		}

		data = kzalloc(msg_low, GFP_KERNEL);
		if (!data)
			goto end;
		memcpy(data, (void *) msg->mailbox_data, msg_low);
		/* Copy command id so that we can use to put sst to reset */
		dsp_hdr = (struct ipc_dsp_hdr *)data;
		cmd_id = dsp_hdr->cmd_id;
		pr_debug("cmd_id %d\n", dsp_hdr->cmd_id);
		if (sst_wake_up_block(sst_drv_ctx, msg_high.part.result,
				msg_high.part.drv_id,
				msg_high.part.msg_id, data, msg_low))
			kfree(data);
		if (cmd_id == IPC_SST_VB_RESET)
			sst_set_fw_state_locked(sst_drv_ctx, SST_RESET);
	} else {
		sst_wake_up_block(sst_drv_ctx, msg_high.part.result,
				msg_high.part.drv_id,
				msg_high.part.msg_id, NULL, 0);
	}

end:
	return;
}

/**
* sst_process_reply - Processes reply message from SST
*
* @work:	Pointer to work structure
*
* This function is scheduled by ISR
* It take a reply msg from response_queue and
* does action based on msg
*/
void sst_process_reply_mfld(struct ipc_post *msg)
{
	void *data;
	int str_id;
	struct stream_info *stream;


	str_id = msg->header.part.str_id;

	pr_debug("sst: IPC process reply for %x\n", msg->header.full);

	/* handle drain notify first */
	if (msg->header.part.msg_id == IPC_IA_DRAIN_STREAM) {
		pr_debug("drain message notify\n");
		if (str_id > 0) {
			stream = &sst_drv_ctx->streams[str_id];
			if (stream->drain_notify)
				stream->drain_notify(stream->drain_cb_param);
		}
		return;
	}


	if (!msg->header.part.large) {
		if (!msg->header.part.data)
			pr_debug("Success\n");
		else
			pr_err("Error from firmware: %d\n", msg->header.part.data);
		sst_wake_up_block(sst_drv_ctx, msg->header.part.data,
				str_id, msg->header.part.msg_id, NULL, 0);
	} else {
		pr_debug("Allocating %d\n", msg->header.part.data);
		data = kzalloc(msg->header.part.data, GFP_KERNEL);
		if (!data) {
			pr_err("sst: mem alloc failed\n");
			return;
		}

		memcpy(data, (void *)msg->mailbox_data, msg->header.part.data);
		if (sst_wake_up_block(sst_drv_ctx, 0, str_id,
				msg->header.part.msg_id, data,
				msg->header.part.data))
			kfree(data);
	}
	return;
}
