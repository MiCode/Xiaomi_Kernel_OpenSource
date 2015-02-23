/*
 *  sst_stream.c - Intel SST Driver for audio engine
 *
 *  Copyright (C) 2008-10 Intel Corp
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
 *  This file contains the stream operations of SST driver
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/pci.h>
#include <linux/firmware.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include "../platform-libs/atom_controls.h"
#include "../platform-libs/atom_pipes.h"
#include "../sst_platform.h"
#include "../platform_ipc_v2.h"
#include "sst.h"
#include "sst_trace.h"

/**
 * sst_alloc_stream - Send msg for a new stream ID
 *
 * @params:	stream params
 * @stream_ops:	operation of stream PB/capture
 * @codec:	codec for stream
 * @device:	device stream to be allocated for
 *
 * This function is called by any function which wants to start
 * a new stream. This also check if a stream exists which is idle
 * it initializes idle stream id to this request
 */
int sst_alloc_stream_ctp(char *params, struct sst_block *block)
{
	struct ipc_post *msg = NULL;
	struct snd_sst_alloc_params alloc_param;
	unsigned int pcm_slot = 0x03, num_ch;
	int str_id;
	struct snd_sst_params *str_params;
	struct snd_sst_stream_params *sparams;
	struct snd_sst_alloc_params_ext *aparams;
	struct stream_info *str_info;
	unsigned int stream_ops, device;
	u8 codec;

	pr_debug("In %s\n", __func__);

	BUG_ON(!params);
	str_params = (struct snd_sst_params *)params;
	stream_ops = str_params->ops;
	codec = str_params->codec;
	device = str_params->device_type;
	sparams = &str_params->sparams;
	aparams = &str_params->aparams;
	num_ch = sst_get_num_channel(str_params);

	pr_debug("period_size = %d\n", aparams->frag_size);
	pr_debug("ring_buf_addr = 0x%x\n", aparams->ring_buf_info[0].addr);
	pr_debug("ring_buf_size = %d\n", aparams->ring_buf_info[0].size);
	pr_debug("In alloc device_type=%d\n", str_params->device_type);
	pr_debug("In alloc sg_count =%d\n", aparams->sg_count);

	str_id = str_params->stream_id;
	if (str_id <= 0)
		return -EBUSY;

	/*allocate device type context*/
	sst_init_stream(&sst_drv_ctx->streams[str_id], codec,
			str_id, stream_ops, pcm_slot);
	/* send msg to FW to allocate a stream */
	if (sst_create_ipc_msg(&msg, true))
		return -ENOMEM;

	alloc_param.str_type.codec_type = codec;
	alloc_param.str_type.str_type = str_params->stream_type;
	alloc_param.str_type.operation = stream_ops;
	alloc_param.str_type.protected_str = 0; /* non drm */
	alloc_param.str_type.time_slots = pcm_slot;
	alloc_param.str_type.reserved = 0;
	alloc_param.str_type.result = 0;
	memcpy(&alloc_param.stream_params, sparams,
			sizeof(struct snd_sst_stream_params));
	memcpy(&alloc_param.alloc_params, aparams,
			sizeof(struct snd_sst_alloc_params_ext));
	block->drv_id = str_id;
	block->msg_id = IPC_IA_ALLOC_STREAM;
	sst_fill_header(&msg->header, IPC_IA_ALLOC_STREAM, 1, str_id);
	msg->header.part.data = sizeof(alloc_param) + sizeof(u32);
	memcpy(msg->mailbox_data, &msg->header, sizeof(u32));
	memcpy(msg->mailbox_data + sizeof(u32), &alloc_param,
			sizeof(alloc_param));
	str_info = &sst_drv_ctx->streams[str_id];
	str_info->num_ch = num_ch;
	sst_add_to_dispatch_list_and_post(sst_drv_ctx, msg);
	return str_id;
}

int sst_alloc_stream_mrfld(char *params, struct sst_block *block)
{
	struct ipc_post *msg = NULL;
	struct snd_sst_alloc_mrfld alloc_param;
	struct ipc_dsp_hdr dsp_hdr;
	struct snd_sst_params *str_params;
	struct snd_sst_tstamp fw_tstamp;
	unsigned int str_id, pipe_id, pvt_id, task_id;
	u32 len = 0;
	struct stream_info *str_info;
	int i, num_ch;

	pr_debug("In %s\n", __func__);
	BUG_ON(!params);

	str_params = (struct snd_sst_params *)params;
	memset(&alloc_param, 0, sizeof(alloc_param));
	alloc_param.operation = str_params->ops;
	alloc_param.codec_type = str_params->codec;
	alloc_param.sg_count = str_params->aparams.sg_count;
	alloc_param.ring_buf_info[0].addr = str_params->aparams.ring_buf_info[0].addr;
	alloc_param.ring_buf_info[0].size = str_params->aparams.ring_buf_info[0].size;
	alloc_param.frag_size = str_params->aparams.frag_size;

	memcpy(&alloc_param.codec_params, &str_params->sparams,
			sizeof(struct snd_sst_stream_params));

	/* fill channel map params for multichannel support.
	 * Ideally channel map should be received from upper layers
	 * for multichannel support.
	 * Currently hardcoding as per FW reqm.
	 */
	num_ch = sst_get_num_channel(str_params);
	pr_debug("%s num_channel = %d\n", __func__, num_ch);
	for (i = 0; i < 8; i++) {
		if (i < num_ch)
			alloc_param.codec_params.uc.pcm_params.channel_map[i] = i;
		else
			alloc_param.codec_params.uc.pcm_params.channel_map[i] = 0xFF;
	}

	str_id = str_params->stream_id;
	pipe_id = str_params->device_type;
	task_id = str_params->task;
	sst_drv_ctx->streams[str_id].pipe_id = pipe_id;
	sst_drv_ctx->streams[str_id].task_id = task_id;
	sst_drv_ctx->streams[str_id].num_ch = num_ch;

	pvt_id = sst_assign_pvt_id(sst_drv_ctx);
	if (sst_drv_ctx->info.lpe_viewpt_rqd)
		alloc_param.ts = sst_drv_ctx->info.mailbox_start +
			sst_drv_ctx->tstamp + (str_id * sizeof(fw_tstamp));
	else
		alloc_param.ts = sst_drv_ctx->mailbox_add +
			sst_drv_ctx->tstamp + (str_id * sizeof(fw_tstamp));

	pr_debug("alloc tstamp location = 0x%x\n", alloc_param.ts);
	pr_debug("assigned pipe id 0x%x to task %d\n", pipe_id, task_id);

	/*allocate device type context*/
	sst_init_stream(&sst_drv_ctx->streams[str_id], alloc_param.codec_type,
			str_id, alloc_param.operation, 0);
	/* send msg to FW to allocate a stream */
	if (sst_create_ipc_msg(&msg, true))
		return -ENOMEM;

	block->drv_id = pvt_id;
	block->msg_id = IPC_CMD;

	sst_fill_header_mrfld(&msg->mrfld_header, IPC_CMD,
			      task_id, 1, pvt_id);
	pr_debug("header:%x\n", msg->mrfld_header.p.header_high.full);
	msg->mrfld_header.p.header_high.part.res_rqd = 1;

	len = msg->mrfld_header.p.header_low_payload = sizeof(alloc_param) + sizeof(dsp_hdr);
	sst_fill_header_dsp(&dsp_hdr, IPC_IA_ALLOC_STREAM_MRFLD, pipe_id, sizeof(alloc_param));
	memcpy(msg->mailbox_data, &dsp_hdr, sizeof(dsp_hdr));
	memcpy(msg->mailbox_data + sizeof(dsp_hdr), &alloc_param,
			sizeof(alloc_param));
	trace_sst_stream("ALLOC ->", str_id, pipe_id);
	str_info = &sst_drv_ctx->streams[str_id];
	pr_debug("header:%x\n", msg->mrfld_header.p.header_high.full);
	pr_debug("response rqd: %x", msg->mrfld_header.p.header_high.part.res_rqd);
	pr_debug("calling post_message\n");
	pr_debug("Alloc for str %d pipe %#x\n", str_id, pipe_id);

	sst_add_to_dispatch_list_and_post(sst_drv_ctx, msg);
	return str_id;
}

/**
* sst_stream_stream - Send msg for a pausing stream
* @str_id:	 stream ID
*
* This function is called by any function which wants to start
* a stream.
*/
int sst_start_stream(int str_id)
{
	int retval = 0, pvt_id;
	u32 len = 0;
	struct ipc_post *msg = NULL;
	struct ipc_dsp_hdr dsp_hdr;
	struct stream_info *str_info;

	pr_debug("sst_start_stream for %d\n", str_id);
	str_info = get_stream_info(str_id);
	if (!str_info)
		return -EINVAL;
	if (str_info->status != STREAM_RUNNING)
		return -EBADRQC;

	if (sst_create_ipc_msg(&msg, true))
		return -ENOMEM;

	if (!sst_drv_ctx->use_32bit_ops) {
		pvt_id = sst_assign_pvt_id(sst_drv_ctx);
		pr_debug("pvt_id = %d, pipe id = %d, task = %d\n",
			 pvt_id, str_info->pipe_id, str_info->task_id);
		sst_fill_header_mrfld(&msg->mrfld_header, IPC_CMD,
				      str_info->task_id, 1, pvt_id);

		len = sizeof(u16) + sizeof(dsp_hdr);
		msg->mrfld_header.p.header_low_payload = len;
		sst_fill_header_dsp(&dsp_hdr, IPC_IA_START_STREAM_MRFLD,
				str_info->pipe_id, sizeof(u16));
		memcpy(msg->mailbox_data, &dsp_hdr, sizeof(dsp_hdr));
		memset(msg->mailbox_data + sizeof(dsp_hdr), 0, sizeof(u16));
		trace_sst_stream("START ->", str_id, str_info->pipe_id);
		pr_info("Start for str %d pipe %#x\n", str_id, str_info->pipe_id);

	} else {
		pr_debug("fill START_STREAM for CTP\n");
		sst_fill_header(&msg->header, IPC_IA_START_STREAM, 1, str_id);
		msg->header.part.data =  sizeof(u32) + sizeof(u32);
		memcpy(msg->mailbox_data, &msg->header, sizeof(u32));
		memset(msg->mailbox_data + sizeof(u32), 0, sizeof(u32));
	}
	sst_drv_ctx->ops->sync_post_message(msg);
	return retval;
}

int sst_send_byte_stream_mrfld(void *sbytes)
{
	struct ipc_post *msg = NULL;
	struct snd_sst_bytes_v2 *bytes = (struct snd_sst_bytes_v2 *) sbytes;
	u32 length;
	int pvt_id, ret = 0;
	struct sst_block *block = NULL;

	pr_debug("%s: type:%u ipc_msg:%u block:%u task_id:%u pipe: %#x length:%#x\n",
		__func__, bytes->type, bytes->ipc_msg,
		bytes->block, bytes->task_id,
		bytes->pipe_id, bytes->len);

	/* need some err check as this is user data, perhpas move this to the
	 * platform driver and pass the struct
	 */
	if (sst_create_ipc_msg(&msg, true))
		return -ENOMEM;

	pvt_id = sst_assign_pvt_id(sst_drv_ctx);
	sst_fill_header_mrfld(&msg->mrfld_header, bytes->ipc_msg, bytes->task_id,
			      1, pvt_id);
	msg->mrfld_header.p.header_high.part.res_rqd = bytes->block;
	length = bytes->len;
	msg->mrfld_header.p.header_low_payload = length;
	pr_debug("length is %d\n", length);
	memcpy(msg->mailbox_data, &bytes->bytes, bytes->len);
	trace_sst_stream("BYTES ->", bytes->type, bytes->pipe_id);
	if (bytes->block) {
		block = sst_create_block(sst_drv_ctx, bytes->ipc_msg, pvt_id);
		if (block == NULL) {
			kfree(msg);
			return -ENOMEM;
		}
	}
	sst_add_to_dispatch_list_and_post(sst_drv_ctx, msg);
	pr_debug("msg->mrfld_header.p.header_low_payload:%d", msg->mrfld_header.p.header_low_payload);
	if (bytes->block) {
		ret = sst_wait_timeout(sst_drv_ctx, block);
		if (ret) {
			pr_err("%s: fw returned err %d\n", __func__, ret);
			sst_free_block(sst_drv_ctx, block);
			return ret;
		}
	}
	if (bytes->type == SND_SST_BYTES_GET) {
		/* copy the reply and send back
		 * we need to update only sz and payload
		 */
		if (bytes->block) {
			unsigned char *r = block->data;
			pr_debug("read back %d bytes", bytes->len);
			memcpy(bytes->bytes, r, bytes->len);
			trace_sst_stream("BYTES <-", bytes->type, bytes->pipe_id);
		}
	}
	if (bytes->block)
		sst_free_block(sst_drv_ctx, block);
	return 0;
}

int sst_send_probe_bytes(struct intel_sst_drv *sst)
{
	struct ipc_post *msg = NULL;
	struct sst_block *block;
	int ret_val = 0;

	ret_val = sst_create_block_and_ipc_msg(&msg, true, sst,
			&block, IPC_IA_DBG_SET_PROBE_PARAMS, 0);
	if (ret_val) {
		pr_err("Can't allocate block/msg: Probe Byte Stream\n");
		return ret_val;
	}

	sst_fill_header(&msg->header, IPC_IA_DBG_SET_PROBE_PARAMS, 1, 0);

	msg->header.part.data = sizeof(u32) + sst->probe_bytes->len;
	memcpy(msg->mailbox_data, &msg->header.full, sizeof(u32));
	memcpy(msg->mailbox_data + sizeof(u32), sst->probe_bytes->bytes,
				sst->probe_bytes->len);

	sst_add_to_dispatch_list_and_post(sst, msg);
	ret_val = sst_wait_timeout(sst, block);
	sst_free_block(sst, block);
	if (ret_val)
		pr_err("set probe stream param..timeout!\n");
	return ret_val;
}

/*
 * sst_pause_stream - Send msg for a pausing stream
 * @str_id:	 stream ID
 *
 * This function is called by any function which wants to pause
 * an already running stream.
 */
int sst_pause_stream(int str_id)
{
	int retval = 0, pvt_id, len;
	struct ipc_post *msg = NULL;
	struct stream_info *str_info;
	struct intel_sst_ops *ops;
	struct sst_block *block;
	struct ipc_dsp_hdr dsp_hdr;

	pr_debug("SST DBG:sst_pause_stream for %d\n", str_id);
	str_info = get_stream_info(str_id);
	if (!str_info)
		return -EINVAL;
	ops = sst_drv_ctx->ops;
	if (str_info->status == STREAM_PAUSED)
		return 0;
	if (str_info->status == STREAM_RUNNING ||
		str_info->status == STREAM_INIT) {
		if (str_info->prev == STREAM_UN_INIT)
			return -EBADRQC;
		if (!sst_drv_ctx->use_32bit_ops) {
			pvt_id = sst_assign_pvt_id(sst_drv_ctx);
			retval = sst_create_block_and_ipc_msg(&msg, true,
					sst_drv_ctx, &block, IPC_CMD, pvt_id);
			if (retval)
				return retval;
			sst_fill_header_mrfld(&msg->mrfld_header, IPC_CMD,
					str_info->task_id, 1, pvt_id);
			msg->mrfld_header.p.header_high.part.res_rqd = 1;
			len = sizeof(dsp_hdr);
			msg->mrfld_header.p.header_low_payload = len;
			sst_fill_header_dsp(&dsp_hdr, IPC_IA_PAUSE_STREAM_MRFLD,
						str_info->pipe_id, 0);
			memcpy(msg->mailbox_data, &dsp_hdr, sizeof(dsp_hdr));
			trace_sst_stream("PAUSE ->", str_id, str_info->pipe_id);
		} else {
			retval = sst_create_block_and_ipc_msg(&msg, false,
					sst_drv_ctx, &block,
					IPC_IA_PAUSE_STREAM, str_id);
			if (retval)
				return retval;
			sst_fill_header(&msg->header, IPC_IA_PAUSE_STREAM,
								0, str_id);
		}
		sst_add_to_dispatch_list_and_post(sst_drv_ctx, msg);
		retval = sst_wait_timeout(sst_drv_ctx, block);
		sst_free_block(sst_drv_ctx, block);
		if (retval == 0) {
			str_info->prev = str_info->status;
			str_info->status = STREAM_PAUSED;
		} else if (retval == SST_ERR_INVALID_STREAM_ID) {
			retval = -EINVAL;
			mutex_lock(&sst_drv_ctx->stream_lock);
			sst_clean_stream(str_info);
			mutex_unlock(&sst_drv_ctx->stream_lock);
		}
	} else {
		retval = -EBADRQC;
		pr_debug("SST DBG:BADRQC for stream\n ");
	}

	return retval;
}

/**
 * sst_resume_stream - Send msg for resuming stream
 * @str_id:		stream ID
 *
 * This function is called by any function which wants to resume
 * an already paused stream.
 */
int sst_resume_stream(int str_id)
{
	int retval = 0;
	struct ipc_post *msg = NULL;
	struct stream_info *str_info;
	struct intel_sst_ops *ops;
	struct sst_block *block = NULL;
	int pvt_id, len;
	struct ipc_dsp_hdr dsp_hdr;

	pr_debug("SST DBG:sst_resume_stream for %d\n", str_id);
	str_info = get_stream_info(str_id);
	if (!str_info)
		return -EINVAL;
	ops = sst_drv_ctx->ops;
	if (str_info->status == STREAM_RUNNING)
			return 0;
	if (str_info->status == STREAM_PAUSED) {
		if (!sst_drv_ctx->use_32bit_ops) {
			pvt_id = sst_assign_pvt_id(sst_drv_ctx);
			retval = sst_create_block_and_ipc_msg(&msg, true,
					sst_drv_ctx, &block, IPC_CMD, pvt_id);
			if (retval)
				return retval;
			sst_fill_header_mrfld(&msg->mrfld_header, IPC_CMD,
					str_info->task_id, 1, pvt_id);
			msg->mrfld_header.p.header_high.part.res_rqd = 1;
			len = sizeof(dsp_hdr);
			msg->mrfld_header.p.header_low_payload = len;
			sst_fill_header_dsp(&dsp_hdr,
						IPC_IA_RESUME_STREAM_MRFLD,
						str_info->pipe_id, 0);
			memcpy(msg->mailbox_data, &dsp_hdr, sizeof(dsp_hdr));
			trace_sst_stream("RESUME->", str_id, str_info->pipe_id);
		} else {
			retval = sst_create_block_and_ipc_msg(&msg, false,
					sst_drv_ctx, &block,
					IPC_IA_RESUME_STREAM, str_id);
			if (retval)
				return retval;
			sst_fill_header(&msg->header, IPC_IA_RESUME_STREAM,
								0, str_id);
		}
		sst_add_to_dispatch_list_and_post(sst_drv_ctx, msg);
		retval = sst_wait_timeout(sst_drv_ctx, block);
		sst_free_block(sst_drv_ctx, block);
		if (!retval) {
			if (str_info->prev == STREAM_RUNNING)
				str_info->status = STREAM_RUNNING;
			else
				str_info->status = STREAM_INIT;
			str_info->prev = STREAM_PAUSED;
		} else if (retval == -SST_ERR_INVALID_STREAM_ID) {
			retval = -EINVAL;
			mutex_lock(&sst_drv_ctx->stream_lock);
			sst_clean_stream(str_info);
			mutex_unlock(&sst_drv_ctx->stream_lock);
		}
	} else {
		retval = -EBADRQC;
		pr_err("SST ERR: BADQRC for stream\n");
	}

	return retval;
}


/**
 * sst_drop_stream - Send msg for stopping stream
 * @str_id:		stream ID
 *
 * This function is called by any function which wants to stop
 * a stream.
 */
int sst_drop_stream(int str_id)
{
	int retval = 0, pvt_id;
	struct stream_info *str_info;
	struct ipc_post *msg = NULL;
	struct ipc_dsp_hdr dsp_hdr;

	pr_debug("SST DBG:sst_drop_stream for %d\n", str_id);
	str_info = get_stream_info(str_id);
	if (!str_info)
		return -EINVAL;

	if (str_info->status != STREAM_UN_INIT) {

		if (sst_drv_ctx->use_32bit_ops == true) {
			str_info->prev = STREAM_UN_INIT;
			str_info->status = STREAM_INIT;
			str_info->cumm_bytes = 0;
			sst_send_sync_msg(IPC_IA_DROP_STREAM, str_id);
		} else {
			if (sst_create_ipc_msg(&msg, true))
				return -ENOMEM;
			str_info->prev = STREAM_UN_INIT;
			str_info->status = STREAM_INIT;
			str_info->cumm_bytes = 0;
			pvt_id = sst_assign_pvt_id(sst_drv_ctx);
			sst_fill_header_mrfld(&msg->mrfld_header, IPC_CMD,
					      str_info->task_id, 1, pvt_id);

			msg->mrfld_header.p.header_low_payload = sizeof(dsp_hdr);
			sst_fill_header_dsp(&dsp_hdr, IPC_IA_DROP_STREAM_MRFLD,
					str_info->pipe_id, 0);
			memcpy(msg->mailbox_data, &dsp_hdr, sizeof(dsp_hdr));
			trace_sst_stream("STOP  ->", str_id, str_info->pipe_id);
			pr_info("Stop for str %d pipe %#x\n", str_id, str_info->pipe_id);

			sst_drv_ctx->ops->sync_post_message(msg);
		}
	} else {
		retval = -EBADRQC;
		pr_debug("BADQRC for stream, state %x\n", str_info->status);
	}
	return retval;
}

/**
 * sst_next_track: notify next track
 * @str_id:		stream ID
 *
 * This function is called by any function which wants to
 * set next track. Current this is NOP as FW doest care
 */
int sst_next_track(void)
{
	pr_debug("SST DBG: next_track");
	return 0;
}

/**
* sst_drain_stream - Send msg for draining stream
* @str_id:		stream ID
*
* This function is called by any function which wants to drain
* a stream.
*/
int sst_drain_stream(int str_id, bool partial_drain)
{
	int retval = 0, pvt_id, len;
	struct ipc_post *msg = NULL;
	struct stream_info *str_info;
	struct intel_sst_ops *ops;
	struct sst_block *block = NULL;
	struct ipc_dsp_hdr dsp_hdr;

	pr_debug("SST DBG:sst_drain_stream for %d\n", str_id);
	str_info = get_stream_info(str_id);
	if (!str_info)
		return -EINVAL;
	ops = sst_drv_ctx->ops;
	if (str_info->status != STREAM_RUNNING &&
		str_info->status != STREAM_INIT &&
		str_info->status != STREAM_PAUSED) {
			pr_err("SST ERR: BADQRC for stream = %d\n",
				       str_info->status);
			return -EBADRQC;
	}

	if (!sst_drv_ctx->use_32bit_ops) {
		pvt_id = sst_assign_pvt_id(sst_drv_ctx);
		retval = sst_create_block_and_ipc_msg(&msg, true,
				sst_drv_ctx, &block, IPC_CMD, pvt_id);
		if (retval)
			return retval;
		sst_fill_header_mrfld(&msg->mrfld_header, IPC_CMD,
				str_info->task_id, 1, pvt_id);
		pr_debug("header:%x\n",
			(unsigned int)msg->mrfld_header.p.header_high.full);
		msg->mrfld_header.p.header_high.part.res_rqd = 1;

		len = sizeof(u8) + sizeof(dsp_hdr);
		msg->mrfld_header.p.header_low_payload = len;
		sst_fill_header_dsp(&dsp_hdr, IPC_IA_DRAIN_STREAM_MRFLD,
					str_info->pipe_id, sizeof(u8));
		memcpy(msg->mailbox_data, &dsp_hdr, sizeof(dsp_hdr));
		memcpy(msg->mailbox_data + sizeof(dsp_hdr),
				&partial_drain, sizeof(u8));
		trace_sst_stream("DRAIN ->", str_id, str_info->pipe_id);
	} else {
		retval = sst_create_block_and_ipc_msg(&msg, false,
				sst_drv_ctx, &block,
				IPC_IA_DRAIN_STREAM, str_id);
		if (retval)
			return retval;
		sst_fill_header(&msg->header, IPC_IA_DRAIN_STREAM, 0, str_id);
		msg->header.part.data = partial_drain;
	}
	sst_add_to_dispatch_list_and_post(sst_drv_ctx, msg);
	/* with new non blocked drain implementation in core we dont need to
	 * wait for respsonse, and need to only invoke callback for drain
	 * complete
	 */

	sst_free_block(sst_drv_ctx, block);
	return retval;
}

/**
 * sst_free_stream - Frees a stream
 * @str_id:		stream ID
 *
 * This function is called by any function which wants to free
 * a stream.
 */
int sst_free_stream(int str_id)
{
	int retval = 0;
	unsigned int pvt_id;
	struct ipc_post *msg = NULL;
	struct stream_info *str_info;
	struct intel_sst_ops *ops;
	unsigned long irq_flags;
	struct ipc_dsp_hdr dsp_hdr;
	struct sst_block *block;

	pr_debug("SST DBG:sst_free_stream for %d\n", str_id);

	mutex_lock(&sst_drv_ctx->sst_lock);
	if (sst_drv_ctx->sst_state == SST_RESET) {
		mutex_unlock(&sst_drv_ctx->sst_lock);
		return -ENODEV;
	}
	mutex_unlock(&sst_drv_ctx->sst_lock);
	str_info = get_stream_info(str_id);
	if (!str_info)
		return -EINVAL;
	ops = sst_drv_ctx->ops;

	mutex_lock(&str_info->lock);
	if (str_info->status != STREAM_UN_INIT) {
		str_info->prev =  str_info->status;
		str_info->status = STREAM_UN_INIT;
		mutex_unlock(&str_info->lock);

		if (!sst_drv_ctx->use_32bit_ops) {
			pvt_id = sst_assign_pvt_id(sst_drv_ctx);
			retval = sst_create_block_and_ipc_msg(&msg, true,
					sst_drv_ctx, &block, IPC_CMD, pvt_id);
			if (retval)
				return retval;

			sst_fill_header_mrfld(&msg->mrfld_header, IPC_CMD,
					      str_info->task_id, 1, pvt_id);
			msg->mrfld_header.p.header_low_payload =
							sizeof(dsp_hdr);
			sst_fill_header_dsp(&dsp_hdr, IPC_IA_FREE_STREAM_MRFLD,
						str_info->pipe_id,  0);
			memcpy(msg->mailbox_data, &dsp_hdr, sizeof(dsp_hdr));
			trace_sst_stream("FREE  ->", str_id, str_info->pipe_id);
			pr_info("Free for str %d pipe %#x\n", str_id, str_info->pipe_id);

		} else {
			retval = sst_create_block_and_ipc_msg(&msg, false,
						sst_drv_ctx, &block,
						IPC_IA_FREE_STREAM, str_id);
			if (retval)
				return retval;
			sst_fill_header(&msg->header, IPC_IA_FREE_STREAM,
								 0, str_id);
		}
		spin_lock_irqsave(&sst_drv_ctx->ipc_spin_lock, irq_flags);
		list_add_tail(&msg->node, &sst_drv_ctx->ipc_dispatch_list);
		spin_unlock_irqrestore(&sst_drv_ctx->ipc_spin_lock, irq_flags);
		if (!sst_drv_ctx->use_32bit_ops) {
			/*FIXME: do we need to wake up drain stream here,
			 * how to get the pvt_id and msg_id
			 */
		} else {
			sst_wake_up_block(sst_drv_ctx, 0, str_id,
				IPC_IA_DRAIN_STREAM, NULL, 0);
		}
		ops->post_message(&sst_drv_ctx->ipc_post_msg_wq);
		retval = sst_wait_timeout(sst_drv_ctx, block);
		pr_debug("sst: wait for free returned %d\n", retval);
		mutex_lock(&sst_drv_ctx->stream_lock);
		sst_clean_stream(str_info);
		mutex_unlock(&sst_drv_ctx->stream_lock);
		pr_debug("SST DBG:Stream freed\n");
		sst_free_block(sst_drv_ctx, block);
	} else {
		mutex_unlock(&str_info->lock);
		retval = -EBADRQC;
		pr_debug("SST DBG:BADQRC for stream\n");
	}

	return retval;
}

int sst_request_vtsv_file(char *fname, struct intel_sst_drv *ctx,
		void **out_file, u32 *out_size)
{
	int retval = 0;
	const struct firmware *file;
	void *ddr_virt_addr;
	unsigned long file_base;

	if (!ctx->pdata->lib_info) {
		pr_err("lib_info pointer NULL\n");
		return -EINVAL;
	}

	pr_debug("Requesting VTSV file %s now...\n", fname);
	retval = request_firmware(&file, fname, ctx->dev);
	if (file == NULL) {
		pr_err("VTSV file is returning as null\n");
		return -EINVAL;
	}
	if (retval) {
		pr_err("request fw failed %d\n", retval);
		return retval;
	}

	if ((*out_file == NULL) || (*out_size < file->size)) {
		retval = sst_get_next_lib_mem(&ctx->lib_mem_mgr, file->size,
			&file_base);
		*out_file = (void *)file_base;
	}
	ddr_virt_addr = (unsigned char *)ctx->ddr +
		(unsigned long)(*out_file - ctx->pdata->lib_info->mod_base);
	memcpy(ddr_virt_addr, file->data, file->size);

	*out_size = file->size;
	release_firmware(file);
	return 0;
}

int sst_format_vtsv_message(struct intel_sst_drv *ctx,
	struct ipc_post **msgptr, struct sst_block **block)
{
	int retval = 0, pvt_id, len;
	struct ipc_dsp_hdr dsp_hdr;
	struct snd_sst_vtsv_info vinfo;
	struct ipc_post *msg;

	BUG_ON((unsigned long)(ctx->vcache.file1_in_mem) & 0xffffffff00000000ULL);
	BUG_ON((unsigned long)(ctx->vcache.file2_in_mem) & 0xffffffff00000000ULL);

	vinfo.vfiles[0].addr = (u32)((unsigned long)ctx->vcache.file1_in_mem
				& 0xffffffff);
	vinfo.vfiles[0].size = ctx->vcache.size1;
	vinfo.vfiles[1].addr = (u32)((unsigned long)ctx->vcache.file2_in_mem
				& 0xffffffff);
	vinfo.vfiles[1].size = ctx->vcache.size2;
	if (vinfo.vfiles[0].addr == 0 || vinfo.vfiles[1].addr == 0) {
		pr_err("%s: invalid address for vtsv libs\n", __func__);
		return -EINVAL;
	}

	/* Create the vtsv message */
	pvt_id = sst_assign_pvt_id(ctx);
	retval = sst_create_block_and_ipc_msg(msgptr, true,
			ctx, block, IPC_CMD, pvt_id);
	if (retval)
		return retval;
	msg = *msgptr;
	sst_fill_header_mrfld(&msg->mrfld_header, IPC_CMD,
			SST_TASK_AWARE, 1, pvt_id);
	pr_debug("header:%x\n",
			(unsigned int)msg->mrfld_header.p.header_high.full);
	msg->mrfld_header.p.header_high.part.res_rqd = 1;

	len = sizeof(vinfo) + sizeof(dsp_hdr);
	msg->mrfld_header.p.header_low_payload = len;
	sst_fill_header_dsp(&dsp_hdr, IPC_IA_VTSV_UPDATE_MODULES,
		(SST_DFW_PATH_INDEX_VAD_OUT >> SST_DFW_PATH_ID_SHIFT),
		sizeof(u8));
	dsp_hdr.mod_id = SST_ALGO_VTSV;
	memcpy(msg->mailbox_data, &dsp_hdr, sizeof(dsp_hdr));
	memcpy(msg->mailbox_data + sizeof(dsp_hdr),
			&vinfo, sizeof(vinfo));
	return 0;
}

int sst_cache_vtsv_libs(struct intel_sst_drv *ctx)
{
	int retval;
	char buff[SST_MAX_VTSV_PATH_BUF_LEN];

	snprintf(buff, sizeof(buff), "%s/%s", ctx->vtsv_path.bytes,
						"vtsv_net.bin");

	/* Download both the data files */
	retval = sst_request_vtsv_file(buff, ctx,
			&ctx->vcache.file1_in_mem, &ctx->vcache.size1);
	if (retval) {
		pr_err("vtsv data file1 request failed %d\n", retval);
		return retval;
	}

	snprintf(buff, sizeof(buff), "%s/%s", ctx->vtsv_path.bytes,
						"vtsv_grammar.bin");

	retval = sst_request_vtsv_file(buff, ctx,
			&ctx->vcache.file2_in_mem, &ctx->vcache.size2);
	if (retval) {
		pr_err("vtsv data file2 request failed %d\n", retval);
		return retval;
	}
	return retval;
}

int sst_send_vtsv_data_to_fw(struct intel_sst_drv *ctx)
{
	int retval = 0;
	struct ipc_post *msg = NULL;
	struct sst_block *block = NULL;

	retval = sst_format_vtsv_message(ctx, &msg, &block);
	if (retval) {
		pr_err("vtsv msg format failed %d\n", retval);
		return retval;
	}
	sst_add_to_dispatch_list_and_post(ctx, msg);
	retval = sst_wait_timeout(ctx, block);
	if (retval)
		pr_err("vtsv msg send to fw failed %d\n", retval);

	sst_free_block(ctx, block);
	return retval;
}
