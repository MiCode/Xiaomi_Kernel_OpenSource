/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/

#include <media/v4l2-subdev.h>
#include <mach/iommu_domains.h>
#include "enc-subdev.h"
#include "wfd-util.h"
#include <media/msm/vcd_api.h>
#include <media/msm/vidc_init.h>
#include <media/msm/vcd_property.h>
#include <linux/time.h>
#include <linux/ktime.h>

#define VID_ENC_MAX_ENCODER_CLIENTS 1
#define MAX_NUM_CTRLS 20

struct venc_inst {
	struct video_client_ctx venc_client;
	void *cbdata;
	void (*op_buffer_done)(void *cookie, u32 status,
			struct vb2_buffer *buf);
	void (*ip_buffer_done)(void *cookie, u32 status,
			struct mem_region *mregion);
	u32 width;
	u32 height;
	int secure;
};

struct venc {
	s32 device_handle;
	void *virt_base;
	struct venc_inst venc_clients[VID_ENC_MAX_ENCODER_CLIENTS];
	struct mutex lock;
	struct ion_client *iclient;
};

static struct venc venc_p;

static void *venc_map_dev_base_addr(void *device_name)
{
		return venc_p.virt_base;
}

static void venc_interrupt_deregister(void)
{
}

static void venc_interrupt_register(void *device_name)
{
}

static void venc_interrupt_clear(void)
{
}

int venc_load_fw(struct v4l2_subdev *sd)
{
	return !vidc_load_firmware();
}

static u32 venc_get_empty_client_index(void)
{
	u32 i;
	u32 found = false;

	for (i = 0; i < VID_ENC_MAX_ENCODER_CLIENTS; i++) {
		if (!venc_p.venc_clients[i].venc_client.vcd_handle) {
			found = true;
			break;
		}
	}
	if (!found) {
		WFD_MSG_ERR("%s():ERROR No space for new client\n",
				__func__);
		return -ENOMEM;
	}
	WFD_MSG_INFO("%s(): available client index = %u\n",
				__func__, i);
	return i;
}

int venc_init(struct v4l2_subdev *sd, u32 val)
{
	struct vcd_init_config vcd_init_config;
	mutex_init(&venc_p.lock);
	venc_p.virt_base = vidc_get_ioaddr();
	vcd_init_config.device_name = "VIDC";
	vcd_init_config.map_dev_base_addr = venc_map_dev_base_addr;
	vcd_init_config.interrupt_clr = venc_interrupt_clear;
	vcd_init_config.register_isr = venc_interrupt_register;
	vcd_init_config.deregister_isr = venc_interrupt_deregister;
	vcd_init(&vcd_init_config, &venc_p.device_handle);
	return 0;
}

static void venc_notify_client(struct video_client_ctx *client_ctx)
{
	if (client_ctx)
		complete(&client_ctx->event);
}

static void venc_open_done(struct video_client_ctx *client_ctx,
	struct vcd_handle_container *handle_container)
{
	if (client_ctx) {
		if (handle_container)
			client_ctx->vcd_handle = handle_container->handle;
		else
			WFD_MSG_ERR("handle_container is NULL\n");
		venc_notify_client(client_ctx);
	} else
		WFD_MSG_ERR("ERROR. client_ctx is NULL");
}

static void venc_start_done(struct video_client_ctx *client_ctx, u32 status)
{
	if (client_ctx)
		venc_notify_client(client_ctx);
	else
		WFD_MSG_ERR("ERROR. client_ctx is NULL");
}

static void venc_stop_done(struct video_client_ctx *client_ctx, u32 status)
{
	WFD_MSG_DBG("Inside venc_stop_done: E\n");
	if (client_ctx)
		venc_notify_client(client_ctx);
	else
		WFD_MSG_ERR("ERROR. client_ctx is NULL");
	WFD_MSG_DBG("Inside venc_stop_done: X\n");
}

static void venc_cb(u32 event, u32 status, void *info, u32 size, void *handle,
		void *const client_data)
{
	struct venc_inst *inst = client_data;
	struct video_client_ctx *client_ctx = &inst->venc_client;
	struct vb2_buffer *vbuf;
	struct mem_region *mregion;
	struct vcd_frame_data *frame_data = (struct vcd_frame_data *)info;

	if (!client_ctx) {
		WFD_MSG_ERR("Client context is NULL\n");
		return;
	}
	client_ctx->event_status = status;
	switch (event) {
	case VCD_EVT_RESP_OPEN:
		WFD_MSG_DBG("EVENT: open done = %d\n", event);
		venc_open_done(client_ctx,
				(struct vcd_handle_container *)info);
		break;
	case VCD_EVT_RESP_INPUT_DONE:
	case VCD_EVT_RESP_INPUT_FLUSHED:
		WFD_MSG_DBG("EVENT: input done = %d\n", event);
		mregion = (struct mem_region *)
			frame_data->frm_clnt_data;
		inst->ip_buffer_done(inst->cbdata, status, mregion);
		break;
	case VCD_EVT_RESP_OUTPUT_DONE:
	case VCD_EVT_RESP_OUTPUT_FLUSHED:
		WFD_MSG_DBG("EVENT: output done = %d\n", event);
		vbuf = (struct vb2_buffer *)
			frame_data->frm_clnt_data;
		vbuf->v4l2_planes[0].bytesused =
			frame_data->data_len;

		switch (frame_data->frame) {
		case VCD_FRAME_I:
		case VCD_FRAME_IDR:
			vbuf->v4l2_buf.flags |= V4L2_BUF_FLAG_KEYFRAME;
			break;
		case VCD_FRAME_P:
			vbuf->v4l2_buf.flags |= V4L2_BUF_FLAG_PFRAME;
			break;
		case VCD_FRAME_B:
			vbuf->v4l2_buf.flags |= V4L2_BUF_FLAG_BFRAME;
			break;
		default:
			break;
		}

		vbuf->v4l2_buf.timestamp =
			ns_to_timeval(frame_data->time_stamp);

		WFD_MSG_DBG("bytes used %d, ts: %d.%d, frame type is %d\n",
				frame_data->data_len,
				(int)vbuf->v4l2_buf.timestamp.tv_sec,
				(int)vbuf->v4l2_buf.timestamp.tv_usec,
				frame_data->frame);

		/*
		 * Output buffers are enc-subdev and vcd's problem, so
		 * if buffer is cached, need to flush before giving to
		 * client. So doing the dirty stuff in this little context
		 */
		{
			unsigned long kvaddr, phys_addr;
			s32 buffer_index = -1, ion_flags = 0;
			struct ion_handle *ion_handle;
			int pmem_fd;
			struct file *filp;
			bool rc;

			rc = vidc_lookup_addr_table(client_ctx,
					BUFFER_TYPE_OUTPUT, true,
					(unsigned long *)&frame_data->
					frm_clnt_data, &kvaddr, &phys_addr,
					&pmem_fd, &filp, &buffer_index);

			if (rc)
				ion_flags = vidc_get_fd_info(client_ctx,
					BUFFER_TYPE_OUTPUT, pmem_fd,
					kvaddr, buffer_index, &ion_handle);
			else
				WFD_MSG_ERR("Got an output buffer that we "
						"couldn't recognize!\n");

			if (msm_ion_do_cache_op(client_ctx->user_ion_client,
				ion_handle, &kvaddr, frame_data->data_len,
				ION_IOC_CLEAN_INV_CACHES))
				WFD_MSG_ERR("OP buffer flush failed\n");

		}

		inst->op_buffer_done(inst->cbdata, status, vbuf);
		break;
	case VCD_EVT_RESP_START:
		WFD_MSG_DBG("EVENT: start done = %d\n", event);
		venc_start_done(client_ctx, status);
		/*TODO: should wait for this event*/
		break;
	case VCD_EVT_RESP_STOP:
		WFD_MSG_DBG("EVENT: not expected = %d\n", event);
		venc_stop_done(client_ctx, status);
		break;
	case VCD_EVT_RESP_PAUSE:
	case VCD_EVT_RESP_FLUSH_INPUT_DONE:
	case VCD_EVT_RESP_FLUSH_OUTPUT_DONE:
	case VCD_EVT_IND_OUTPUT_RECONFIG:
		WFD_MSG_DBG("EVENT: not expected = %d\n", event);
		break;
	case VCD_EVT_IND_HWERRFATAL:
	case VCD_EVT_IND_RESOURCES_LOST:
		WFD_MSG_DBG("EVENT: error = %d\n", event);
		break;
	default:
		WFD_MSG_ERR("Invalid event type = %u\n", event);
		break;
	}
}

static long venc_open(struct v4l2_subdev *sd, void *arg)
{
	u32 client_index;
	int rc = 0;
	struct venc_inst *inst;
	struct video_client_ctx *client_ctx;
	struct venc_msg_ops *vmops  =  arg;
	int flags = 0;
	mutex_lock(&venc_p.lock);
	client_index = venc_get_empty_client_index();
	if (client_index < 0) {
		WFD_MSG_ERR("No free clients, client_index = %d\n",
				client_index);
		rc = -ENODEV;
		goto no_free_client;
	}
	inst = &venc_p.venc_clients[client_index];
	client_ctx = &inst->venc_client;
	init_completion(&client_ctx->event);
	mutex_init(&client_ctx->msg_queue_lock);
	mutex_init(&client_ctx->enrty_queue_lock);
	INIT_LIST_HEAD(&client_ctx->msg_queue);
	init_waitqueue_head(&client_ctx->msg_wait);
	inst->op_buffer_done = vmops->op_buffer_done;
	inst->ip_buffer_done = vmops->ip_buffer_done;
	inst->cbdata = vmops->cbdata;
	inst->secure = vmops->secure;
	if (vmops->secure) {
		WFD_MSG_ERR("OPENING SECURE SESSION\n");
		flags |= VCD_CP_SESSION;
	}
	if (vcd_get_ion_status()) {
		client_ctx->user_ion_client = vcd_get_ion_client();
		if (!client_ctx->user_ion_client) {
			WFD_MSG_ERR("vcd_open ion get client failed");
			return -EFAULT;
		}
	}

	rc = vcd_open(venc_p.device_handle, false, venc_cb,
				inst, flags);
	if (rc) {
		WFD_MSG_ERR("vcd_open failed, rc = %d\n", rc);
		rc = -ENODEV;
		goto no_free_client;
	}
	wait_for_completion(&client_ctx->event);
	if (client_ctx->event_status) {
		WFD_MSG_ERR("callback for vcd_open returned error: %u",
				client_ctx->event_status);
		goto no_free_client;
	}
	WFD_MSG_ERR("NOTE: client_ctx = %p\n", client_ctx);
	vmops->cookie = inst;
	sd->dev_priv = inst;
no_free_client:
	mutex_unlock(&venc_p.lock);
	return rc;
}

static long venc_close(struct v4l2_subdev *sd, void *arg)
{
	long rc = 0;
	struct venc_inst *inst;
	struct video_client_ctx *client_ctx = NULL;
	mutex_lock(&venc_p.lock);
	inst = sd->dev_priv;
	client_ctx = &inst->venc_client;
	if (!client_ctx || !client_ctx->vcd_handle) {
		WFD_MSG_ERR("Invalid client context in close\n");
		rc = -ENODEV;
		goto end;
	}
	rc = vcd_close(client_ctx->vcd_handle);
	if (rc) {
		WFD_MSG_ERR("Failed to close encoder subdevice\n");
		goto end;
	}
	memset((void *)client_ctx, 0,
			sizeof(struct video_client_ctx));
end:
	mutex_unlock(&venc_p.lock);
	return rc;
}

static long venc_get_buffer_req(struct v4l2_subdev *sd, void *arg)
{
	int rc = 0;
	struct v4l2_requestbuffers *b = arg;
	struct vcd_buffer_requirement buf_req;
	struct venc_inst *inst = sd->dev_priv;
	struct video_client_ctx *client_ctx = &inst->venc_client;
	if (!client_ctx) {
		WFD_MSG_ERR("Invalid client context");
		rc = -EINVAL;
		goto err;
	}
	rc = vcd_get_buffer_requirements(client_ctx->vcd_handle,
			VCD_BUFFER_OUTPUT, &buf_req);
	if (rc) {
		WFD_MSG_ERR("Failed to get out buf reqs rc = %d", rc);
		goto err;
	}
	b->count = buf_req.actual_count;
err:
	return rc;
}

static long venc_set_buffer_req(struct v4l2_subdev *sd, void *arg)
{
	int rc = 0;
	struct bufreq *b = arg;
	struct vcd_buffer_requirement buf_req;
	struct venc_inst *inst = sd->dev_priv;
	struct video_client_ctx *client_ctx = &inst->venc_client;
	int aligned_width, aligned_height;
	if (!client_ctx) {
		WFD_MSG_ERR("Invalid client context");
		rc = -EINVAL;
		goto err;
	}
	aligned_width = ALIGN(b->width, 16);
	aligned_height = ALIGN(b->height, 16);

	if (aligned_width != b->width) {
		WFD_MSG_ERR("Width not 16 byte aligned\n");
		rc = -EINVAL;
		goto err;
	}

	buf_req.actual_count = b->count;
	buf_req.min_count = b->count;
	buf_req.max_count = b->count;
	buf_req.sz = ALIGN(aligned_height * aligned_width, SZ_2K)
		+ ALIGN(aligned_height * aligned_width * 1/2, SZ_2K);
	buf_req.align = SZ_4K;
	inst->width = b->width;
	inst->height = b->height;
	rc = vcd_set_buffer_requirements(client_ctx->vcd_handle,
			VCD_BUFFER_INPUT, &buf_req);
	if (rc) {
		WFD_MSG_ERR("Failed to get out buf reqs rc = %d", rc);
		goto err;
	}
	b->size = buf_req.sz;
err:
	return rc;
}

static long venc_start(struct v4l2_subdev *sd)
{
	struct venc_inst *inst = sd->dev_priv;
	struct video_client_ctx *client_ctx = &inst->venc_client;
	int rc;
	if (!client_ctx) {
		WFD_MSG_ERR("Client context is NULL");
		return -EINVAL;
	}
	rc = vcd_encode_start(client_ctx->vcd_handle);
	if (rc) {
		WFD_MSG_ERR("vcd_encode_start failed, rc = %d\n", rc);
		goto err;
	}
	wait_for_completion(&client_ctx->event);
	if (client_ctx->event_status)
		WFD_MSG_ERR("callback for vcd_encode_start returned error: %u",
				client_ctx->event_status);
err:
	return rc;
}

static long venc_stop(struct v4l2_subdev *sd)
{
	struct venc_inst *inst = sd->dev_priv;
	struct video_client_ctx *client_ctx = &inst->venc_client;
	int rc;
	if (!client_ctx) {
		WFD_MSG_ERR("Client context is NULL");
		return -EINVAL;
	}
	rc = vcd_stop(client_ctx->vcd_handle);
	wait_for_completion(&client_ctx->event);
	return rc;
}

static long venc_set_codec(struct video_client_ctx *client_ctx, __s32 codec)
{
	struct vcd_property_codec vcd_property_codec;
	struct vcd_property_hdr vcd_property_hdr;
	vcd_property_hdr.prop_id = VCD_I_CODEC;
	vcd_property_hdr.sz = sizeof(struct vcd_property_codec);
	vcd_property_codec.codec = VCD_CODEC_H264;

	switch (codec) {
	case V4L2_PIX_FMT_H264:
		vcd_property_codec.codec = VCD_CODEC_H264;
		break;
	case V4L2_PIX_FMT_MPEG4:
		vcd_property_codec.codec = VCD_CODEC_MPEG4;
		break;
	default:
		WFD_MSG_ERR("Codec not supported, defaulting to h264\n");
		break;
	}
	return vcd_set_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &vcd_property_codec);
}

static long venc_set_codec_level(struct video_client_ctx *client_ctx,
					__s32 codec, __s32 level)
{
	struct vcd_property_level vcd_property_level;
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_codec vcd_property_codec;

	int rc = 0;
	int mpeg4_base = VCD_LEVEL_MPEG4_0;
	int h264_base = VCD_LEVEL_H264_1;

	/* Validate params */
	vcd_property_hdr.prop_id = VCD_I_CODEC;
	vcd_property_hdr.sz = sizeof(struct vcd_property_codec);
	rc = vcd_get_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &vcd_property_codec);

	if (rc < 0) {
		WFD_MSG_ERR("Error getting codec property");
		rc = -EINVAL;
		goto err;
	}

	if (!((vcd_property_codec.codec == VCD_CODEC_H264
		&& codec == V4L2_CID_MPEG_VIDEO_H264_LEVEL) ||
		(vcd_property_codec.codec == VCD_CODEC_MPEG4
		&& codec == V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL))) {
		WFD_MSG_ERR("Attempting to set %d for codec type %d",
			codec, vcd_property_codec.codec);
		rc = -EINVAL;
		goto err;
	}

	/* Set property */
	vcd_property_hdr.prop_id = VCD_I_LEVEL;
	vcd_property_hdr.sz = sizeof(struct vcd_property_level);

	if (codec == V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL) {
		vcd_property_level.level = mpeg4_base + level;

		if (vcd_property_level.level < VCD_LEVEL_MPEG4_0
			|| vcd_property_level.level > VCD_LEVEL_MPEG4_X) {
			WFD_MSG_ERR("Level (%d) out of range"
					"for codec (%d)\n", level, codec);

			rc = -EINVAL;
			goto err;
		}
	} else if (codec == V4L2_CID_MPEG_VIDEO_H264_LEVEL) {
		vcd_property_level.level = h264_base + level;

		if (vcd_property_level.level < VCD_LEVEL_H264_1
			|| vcd_property_level.level > VCD_LEVEL_H264_5p1) {
			WFD_MSG_ERR("Level (%d) out of range"
					"for codec (%d)\n", level, codec);

			rc = -EINVAL;
			goto err;
		}
	} else {
		WFD_MSG_ERR("Codec (%d) not supported, not setting level (%d)",
				codec, level);
		rc = -ENOTSUPP;
		goto err;
	}

	rc = vcd_set_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &vcd_property_level);
err:
	return rc;
}

static long venc_get_codec_level(struct video_client_ctx *client_ctx,
					__s32 codec, __s32 *level)
{
	struct vcd_property_level vcd_property_level;
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_codec vcd_property_codec;

	int rc = 0;
	int mpeg4_base = VCD_LEVEL_MPEG4_0;
	int h264_base = VCD_LEVEL_H264_1;

	/* Validate params */
	vcd_property_hdr.prop_id = VCD_I_CODEC;
	vcd_property_hdr.sz = sizeof(struct vcd_property_codec);
	rc = vcd_get_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &vcd_property_codec);

	if (rc < 0) {
		WFD_MSG_ERR("Error getting codec property");
		rc = -EINVAL;
		goto err;
	}

	if (!((vcd_property_codec.codec == VCD_CODEC_H264
		&& codec == V4L2_CID_MPEG_VIDEO_H264_LEVEL) ||
		(vcd_property_codec.codec == VCD_CODEC_MPEG4
		&& codec == V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL))) {
		WFD_MSG_ERR("Attempting to get %d for codec type %d",
			codec, vcd_property_codec.codec);
		rc = -EINVAL;
		goto err;
	}

	vcd_property_hdr.prop_id = VCD_I_LEVEL;
	vcd_property_hdr.sz = sizeof(struct vcd_property_level);

	rc = vcd_get_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &vcd_property_level);
	if (rc < 0) {
		rc = -EINVAL;
		goto err;
	}

	if (codec == V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL) {
		*level = vcd_property_level.level - mpeg4_base;
	} else if (codec == V4L2_CID_MPEG_VIDEO_H264_LEVEL) {
		*level = vcd_property_level.level - h264_base;
	} else {
		WFD_MSG_ERR("Codec (%d) not supported", codec);
		rc = -ENOTSUPP;
		goto err;
	}

err:
	return rc;
}

static long venc_set_codec_profile(struct video_client_ctx *client_ctx,
					__s32 codec, __s32 profile)
{
	struct vcd_property_profile vcd_property_profile;
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_codec vcd_property_codec;
	struct vcd_property_i_period vcd_property_i_period;
	int rc = 0;

	/* Validate params */
	vcd_property_hdr.prop_id = VCD_I_CODEC;
	vcd_property_hdr.sz = sizeof(struct vcd_property_codec);
	rc = vcd_get_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &vcd_property_codec);

	if (rc < 0) {
		WFD_MSG_ERR("Error getting codec property");
		rc = -EINVAL;
		goto err_set_profile;
	}

	if (!((vcd_property_codec.codec == VCD_CODEC_H264
		&& codec == V4L2_CID_MPEG_VIDEO_H264_PROFILE) ||
		(vcd_property_codec.codec == VCD_CODEC_MPEG4
		&& codec == V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE))) {
		WFD_MSG_ERR("Attempting to set %d for codec type %d",
			codec, vcd_property_codec.codec);
		rc = -EINVAL;
		goto err_set_profile;
	}

	/* Set property */
	vcd_property_hdr.prop_id = VCD_I_PROFILE;
	vcd_property_hdr.sz = sizeof(struct vcd_property_profile);

	if (codec == V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE) {
		switch (profile) {
		case V4L2_MPEG_VIDEO_MPEG4_PROFILE_SIMPLE:
			vcd_property_profile.profile = VCD_PROFILE_MPEG4_SP;
			break;
		case V4L2_MPEG_VIDEO_MPEG4_PROFILE_ADVANCED_SIMPLE:
			vcd_property_profile.profile = VCD_PROFILE_MPEG4_ASP;
			break;
		default:
			WFD_MSG_ERR("Profile %d not supported,"
					"defaulting to simple (%d)",
					profile, VCD_PROFILE_MPEG4_SP);
			vcd_property_profile.profile = VCD_PROFILE_MPEG4_SP;
			break;
		}
	} else if (codec == V4L2_CID_MPEG_VIDEO_H264_PROFILE) {
		switch (profile) {
		case V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE:
			vcd_property_profile.profile =
				VCD_PROFILE_H264_BASELINE;
			break;
		case V4L2_MPEG_VIDEO_H264_PROFILE_MAIN:
			vcd_property_profile.profile = VCD_PROFILE_H264_MAIN;
			break;
		case V4L2_MPEG_VIDEO_H264_PROFILE_HIGH:
			vcd_property_profile.profile = VCD_PROFILE_H264_HIGH;
			break;
		default:
			WFD_MSG_ERR("Profile %d not supported,"
					"defaulting to baseline (%d)",
					profile, VCD_PROFILE_H264_BASELINE);
			vcd_property_profile.profile =
				VCD_PROFILE_H264_BASELINE;
			break;
		}
	} else {
		WFD_MSG_ERR("Codec (%d) not supported,"
				"not setting profile (%d)",
				codec, profile);
		rc = -ENOTSUPP;
		goto err_set_profile;
	}

	rc = vcd_set_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &vcd_property_profile);

	/* Disable B-frames, since VSG doesn't support out of order i/p bufs */
	vcd_property_hdr.prop_id = VCD_I_INTRA_PERIOD;
	vcd_property_hdr.sz = sizeof(struct vcd_property_i_period);

	rc = vcd_get_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &vcd_property_i_period);
	if (rc) {
		WFD_MSG_ERR("Error getting I-period property");
		goto err_set_profile;
	}
	vcd_property_i_period.b_frames = 0;
	rc = vcd_set_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &vcd_property_i_period);
	if (rc) {
		WFD_MSG_ERR("Error setting I-period property");
		goto err_set_profile;
	}

err_set_profile:
	return rc;
}

static long venc_get_codec_profile(struct video_client_ctx *client_ctx,
		__s32 codec, __s32 *profile)
{
	struct vcd_property_profile vcd_property_profile;
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_codec vcd_property_codec;
	int rc = 0;

	/* Validate params */
	vcd_property_hdr.prop_id = VCD_I_CODEC;
	vcd_property_hdr.sz = sizeof(struct vcd_property_codec);
	rc = vcd_get_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &vcd_property_codec);

	if (rc < 0) {
		WFD_MSG_ERR("Error getting codec property");
		rc = -EINVAL;
		goto err;
	}

	if (!((vcd_property_codec.codec == VCD_CODEC_H264
		&& codec == V4L2_CID_MPEG_VIDEO_H264_PROFILE) ||
		(vcd_property_codec.codec == VCD_CODEC_MPEG4
		&& codec == V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE))) {
		WFD_MSG_ERR("Attempting to set %d for codec type %d",
			codec, vcd_property_codec.codec);
		rc = -EINVAL;
		goto err;
	}

	/* Set property */
	vcd_property_hdr.prop_id = VCD_I_PROFILE;
	vcd_property_hdr.sz = sizeof(struct vcd_property_profile);

	rc = vcd_get_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &vcd_property_profile);

	if (rc < 0) {
		WFD_MSG_ERR("Unable to get property");
		rc = -EINVAL;
		goto err;
	}

	switch (vcd_property_profile.profile) {
	case VCD_PROFILE_MPEG4_SP:
		*profile = V4L2_MPEG_VIDEO_MPEG4_PROFILE_SIMPLE;
		break;
	case VCD_PROFILE_MPEG4_ASP:
		*profile = V4L2_MPEG_VIDEO_MPEG4_PROFILE_ADVANCED_SIMPLE;
		break;
	case VCD_PROFILE_H264_BASELINE:
		*profile = V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE;
		break;
	case VCD_PROFILE_H264_MAIN:
		*profile = V4L2_MPEG_VIDEO_H264_PROFILE_MAIN;
		break;
	case VCD_PROFILE_H264_HIGH:
		*profile = V4L2_MPEG_VIDEO_H264_PROFILE_HIGH;
		break;
	default:
		WFD_MSG_ERR("Unexpected profile");
		rc = -EINVAL;
		goto err;
		break;
	}
err:
	return rc;
}

static long venc_set_h264_intra_period(struct video_client_ctx *client_ctx,
		__s32 period)
{
	struct vcd_property_i_period vcd_property_i_period;
	struct vcd_property_codec vcd_property_codec;
	struct vcd_property_hdr vcd_property_hdr;
	int rc = 0;

	vcd_property_hdr.prop_id = VCD_I_CODEC;
	vcd_property_hdr.sz = sizeof(struct vcd_property_codec);

	rc = vcd_get_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &vcd_property_codec);

	if (rc < 0) {
		WFD_MSG_ERR("Error getting codec property\n");
		goto err;
	}

	if (vcd_property_codec.codec != VCD_CODEC_H264) {
		rc = -ENOTSUPP;
		WFD_MSG_ERR("Control not supported for non H264 codec\n");
		goto err;
	}

	vcd_property_hdr.prop_id = VCD_I_INTRA_PERIOD;
	vcd_property_hdr.sz = sizeof(struct vcd_property_i_period);

	vcd_property_i_period.p_frames = period - 1;
	vcd_property_i_period.b_frames = 0;

	rc = vcd_set_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &vcd_property_i_period);

	if (rc < 0) {
		WFD_MSG_ERR("Error setting intra period\n");
		goto err;
	}

err:
	return rc;
}

static long venc_get_h264_intra_period(struct video_client_ctx *client_ctx,
		__s32 *period)
{
	struct vcd_property_i_period vcd_property_i_period;
	struct vcd_property_codec vcd_property_codec;
	struct vcd_property_hdr vcd_property_hdr;
	int rc = 0;

	vcd_property_hdr.prop_id = VCD_I_CODEC;
	vcd_property_hdr.sz = sizeof(struct vcd_property_codec);

	rc = vcd_get_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &vcd_property_codec);

	if (rc < 0) {
		WFD_MSG_ERR("Error getting codec property\n");
		goto err;
	}

	if (vcd_property_codec.codec != VCD_CODEC_H264) {
		rc = -ENOTSUPP;
		WFD_MSG_ERR("Control not supported for non H264 codec\n");
		goto err;
	}

	vcd_property_hdr.prop_id = VCD_I_INTRA_PERIOD;
	vcd_property_hdr.sz = sizeof(struct vcd_property_i_period);

	rc = vcd_get_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &vcd_property_i_period);

	if (rc < 0) {
		WFD_MSG_ERR("Error getting intra period\n");
		goto err;
	}

	*period = vcd_property_i_period.p_frames + 1;
err:
	return rc;
}

static long venc_request_frame(struct video_client_ctx *client_ctx, __s32 type)
{
	struct vcd_property_req_i_frame vcd_property_req_i_frame;
	struct vcd_property_hdr vcd_property_hdr;

	int rc = 0;
	switch (type) {
	case V4L2_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE_DISABLED:
		/*So...nothing to do?*/
		break;
	case V4L2_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE_I_FRAME:
		vcd_property_hdr.prop_id = VCD_I_REQ_IFRAME;
		vcd_property_hdr.sz = sizeof(struct vcd_property_req_i_frame);
		vcd_property_req_i_frame.req_i_frame = 1;

		rc = vcd_set_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &vcd_property_req_i_frame);
		break;
	case V4L2_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE_NOT_CODED:
	default:
		rc = -ENOTSUPP;
	}

	return rc;
}

static long venc_set_bitrate(struct video_client_ctx *client_ctx,
			__s32 bitrate)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_target_bitrate bit_rate;
	if (!client_ctx || !bitrate)
		return -EINVAL;

	vcd_property_hdr.prop_id = VCD_I_TARGET_BITRATE;
	vcd_property_hdr.sz =
			sizeof(struct vcd_property_target_bitrate);
	bit_rate.target_bitrate = bitrate;
	return vcd_set_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &bit_rate);
}

static long venc_get_bitrate(struct video_client_ctx *client_ctx,
			__s32 *bitrate)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_target_bitrate bit_rate;
	int rc = 0;

	if (!client_ctx || !bitrate)
		return -EINVAL;

	vcd_property_hdr.prop_id = VCD_I_TARGET_BITRATE;
	vcd_property_hdr.sz =
			sizeof(struct vcd_property_target_bitrate);
	rc = vcd_get_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &bit_rate);

	if (rc < 0) {
		WFD_MSG_ERR("Failed getting property for bitrate");
		return rc;
	}

	*bitrate = bit_rate.target_bitrate;
	return rc;
}

static long venc_set_bitrate_mode(struct video_client_ctx *client_ctx,
			__s32 mode)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_rate_control rate_control;
	int rc = 0;

	if (!client_ctx) {
		rc = -EINVAL;
		goto err;
	}

	vcd_property_hdr.prop_id = VCD_I_RATE_CONTROL;
	vcd_property_hdr.sz = sizeof(struct vcd_property_rate_control);
	/*
	 * XXX: V4L doesn't seem have a control to toggle between CFR
	 * and VFR, so assuming worse case VFR.
	 */
	switch (mode) {
	case V4L2_MPEG_VIDEO_BITRATE_MODE_VBR:
		rate_control.rate_control = VCD_RATE_CONTROL_VBR_VFR;
		break;
	case V4L2_MPEG_VIDEO_BITRATE_MODE_CBR:
		rate_control.rate_control = VCD_RATE_CONTROL_CBR_VFR;
		break;
	default:
		WFD_MSG_ERR("unknown bitrate mode %d", mode);
		rc = -EINVAL;
		goto err;
	}

	rc = vcd_set_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &rate_control);
err:
	return rc;
}

static long venc_get_bitrate_mode(struct video_client_ctx *client_ctx,
			__s32 *mode)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_rate_control rate_control;
	int rc = 0;

	if (!client_ctx)
		return -EINVAL;

	vcd_property_hdr.prop_id = VCD_I_RATE_CONTROL;
	vcd_property_hdr.sz = sizeof(struct vcd_property_rate_control);
	rc = vcd_get_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &rate_control);

	switch (rate_control.rate_control) {
	case VCD_RATE_CONTROL_CBR_VFR:
	case VCD_RATE_CONTROL_CBR_CFR:
		*mode = V4L2_MPEG_VIDEO_BITRATE_MODE_CBR;
		break;
	case VCD_RATE_CONTROL_VBR_VFR:
	case VCD_RATE_CONTROL_VBR_CFR:
		*mode = V4L2_MPEG_VIDEO_BITRATE_MODE_VBR;
		break;
	default:
		WFD_MSG_ERR("unknown bitrate mode %d",
				rate_control.rate_control);
		return -EINVAL;
	}

	return 0;
}

static long venc_set_frame_size(struct video_client_ctx *client_ctx,
				u32 height, u32 width)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_frame_size frame_size;
	vcd_property_hdr.prop_id = VCD_I_FRAME_SIZE;
	vcd_property_hdr.sz =
		sizeof(struct vcd_property_frame_size);
	frame_size.height = height;
	frame_size.width = width;
	return vcd_set_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &frame_size);
}

static long venc_set_format(struct v4l2_subdev *sd, void *arg)
{
	struct venc_inst *inst;
	struct video_client_ctx *client_ctx;
	struct v4l2_format *fmt = arg;
	struct vcd_buffer_requirement buf_req;
	int rc = 0;

	inst = sd->dev_priv;
	client_ctx = &inst->venc_client;
	if (!inst || !client_ctx || !fmt) {
		WFD_MSG_ERR("Invalid parameters\n");
		return -EINVAL;
	}
	rc = venc_set_codec(client_ctx, fmt->fmt.pix.pixelformat);
	if (rc) {
		WFD_MSG_ERR("Failed to set codec, rc = %d\n", rc);
		goto err;
	}

	rc = venc_set_frame_size(client_ctx, fmt->fmt.pix.height,
				fmt->fmt.pix.width);
	if (rc) {
		WFD_MSG_ERR("Failed to set frame size, rc = %d\n", rc);
		goto err;
	}
	rc = vcd_get_buffer_requirements(client_ctx->vcd_handle,
			VCD_BUFFER_OUTPUT, &buf_req);
	if (rc) {
		WFD_MSG_ERR("Failed to get buf requrements, rc = %d\n", rc);
		goto err;
	}
	fmt->fmt.pix.sizeimage = buf_req.sz;
err:
	return rc;
}

static long venc_set_framerate(struct v4l2_subdev *sd,
				void *arg)
{
	struct venc_inst *inst = sd->dev_priv;
	struct video_client_ctx *client_ctx = &inst->venc_client;
	struct v4l2_fract *frate = arg;
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_frame_rate vcd_frame_rate;
	int rc;
	vcd_property_hdr.prop_id = VCD_I_FRAME_RATE;
	vcd_property_hdr.sz =
				sizeof(struct vcd_property_frame_rate);
	/* v4l2 passes in "fps" as "spf", so take reciprocal*/
	vcd_frame_rate.fps_denominator = frate->numerator;
	vcd_frame_rate.fps_numerator = frate->denominator;
	rc = vcd_set_property(client_ctx->vcd_handle,
					&vcd_property_hdr, &vcd_frame_rate);
	if (rc)
		WFD_MSG_ERR("Failed to set frame rate, rc = %d\n", rc);
	return rc;
}

static long venc_set_qp_value(struct video_client_ctx *client_ctx,
		__s32 frametype, __s32 qp)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_session_qp vcd_property_session_qp;
	int rc = 0;

	if (!client_ctx) {
		WFD_MSG_ERR("Invalid parameters\n");
		return -EINVAL;
	}

	vcd_property_hdr.prop_id = VCD_I_SESSION_QP;
	vcd_property_hdr.sz = sizeof(vcd_property_session_qp);

	rc = vcd_get_property(client_ctx->vcd_handle, &vcd_property_hdr,
			&vcd_property_session_qp);

	if (rc) {
		WFD_MSG_ERR("Failed to get session qp\n");
		goto err;
	}

	switch (frametype) {
	case V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP:
		vcd_property_session_qp.i_frame_qp = qp;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP:
		vcd_property_session_qp.p_frame_qp = qp;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP:
		vcd_property_session_qp.b_frame_qp = qp;
		break;
	case V4L2_CID_MPEG_VIDEO_H263_I_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_H263_P_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_H263_B_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_MPEG4_I_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_MPEG4_P_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_MPEG4_B_FRAME_QP:
		rc = -ENOTSUPP;
		goto err;
	default:
		rc = -EINVAL;
		goto err;
	}


	rc = vcd_set_property(client_ctx->vcd_handle, &vcd_property_hdr,
			&vcd_property_session_qp);

	if (rc) {
		WFD_MSG_ERR("Failed to set session qp\n");
		goto err;
	}
err:
	return rc;
}

static long venc_get_qp_value(struct video_client_ctx *client_ctx,
		__s32 frametype, __s32 *qp)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_session_qp vcd_property_session_qp;
	int rc = 0;

	if (!client_ctx) {
		WFD_MSG_ERR("Invalid parameters\n");
		return -EINVAL;
	}

	vcd_property_hdr.prop_id = VCD_I_SESSION_QP;
	vcd_property_hdr.sz = sizeof(vcd_property_session_qp);

	rc = vcd_get_property(client_ctx->vcd_handle, &vcd_property_hdr,
			&vcd_property_session_qp);

	if (rc) {
		WFD_MSG_ERR("Failed to get session qp\n");
		goto err;
	}

	switch (frametype) {
	case V4L2_CID_MPEG_VIDEO_MPEG4_I_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_H263_I_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP:
		*qp = vcd_property_session_qp.i_frame_qp;
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_P_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_H263_P_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP:
		*qp = vcd_property_session_qp.p_frame_qp;
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_B_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_H263_B_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP:
		*qp = vcd_property_session_qp.b_frame_qp;
		break;
	default:
		rc = -EINVAL;
		goto err;
	}

err:
	return rc;
}

static long venc_set_qp_range(struct video_client_ctx *client_ctx,
		__s32 type, __s32 qp)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_qp_range vcd_property_qp_range;
	int rc = 0;

	if (!client_ctx) {
		WFD_MSG_ERR("Invalid parameters\n");
		return -EINVAL;
	}

	vcd_property_hdr.prop_id = VCD_I_QP_RANGE;
	vcd_property_hdr.sz = sizeof(vcd_property_qp_range);

	rc = vcd_get_property(client_ctx->vcd_handle, &vcd_property_hdr,
			&vcd_property_qp_range);

	if (rc) {
		WFD_MSG_ERR("Failed to get qp range\n");
		goto err;
	}

	switch (type) {
	case V4L2_CID_MPEG_VIDEO_MPEG4_MIN_QP:
	case V4L2_CID_MPEG_VIDEO_H263_MIN_QP:
	case V4L2_CID_MPEG_VIDEO_H264_MIN_QP:
		vcd_property_qp_range.min_qp = qp;
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_MAX_QP:
	case V4L2_CID_MPEG_VIDEO_H263_MAX_QP:
	case V4L2_CID_MPEG_VIDEO_H264_MAX_QP:
		vcd_property_qp_range.max_qp = qp;
		break;
	default:
		rc = -EINVAL;
		goto err;
	}

	rc = vcd_set_property(client_ctx->vcd_handle, &vcd_property_hdr,
			&vcd_property_qp_range);

	if (rc) {
		WFD_MSG_ERR("Failed to set qp range\n");
		goto err;
	}
err:
	return rc;
}

static long venc_get_qp_range(struct video_client_ctx *client_ctx,
		__s32 type, __s32 *qp)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_qp_range vcd_property_qp_range;
	int rc = 0;

	if (!client_ctx) {
		WFD_MSG_ERR("Invalid parameters\n");
		return -EINVAL;
	}

	vcd_property_hdr.prop_id = VCD_I_QP_RANGE;
	vcd_property_hdr.sz = sizeof(vcd_property_qp_range);

	rc = vcd_get_property(client_ctx->vcd_handle, &vcd_property_hdr,
			&vcd_property_qp_range);

	if (rc) {
		WFD_MSG_ERR("Failed to get qp range\n");
		goto err;
	}

	switch (type) {
	case V4L2_CID_MPEG_VIDEO_MPEG4_MIN_QP:
	case V4L2_CID_MPEG_VIDEO_H263_MIN_QP:
	case V4L2_CID_MPEG_VIDEO_H264_MIN_QP:
		*qp = vcd_property_qp_range.min_qp;
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_MAX_QP:
	case V4L2_CID_MPEG_VIDEO_H263_MAX_QP:
	case V4L2_CID_MPEG_VIDEO_H264_MAX_QP:
		*qp = vcd_property_qp_range.max_qp;
		break;
	default:
		rc = -EINVAL;
		goto err;
	}

	rc = vcd_set_property(client_ctx->vcd_handle, &vcd_property_hdr,
			&vcd_property_qp_range);

	if (rc) {
		WFD_MSG_ERR("Failed to set qp range\n");
		goto err;
	}
err:
	return rc;
}
static long venc_set_max_perf_level(struct video_client_ctx *client_ctx,
		int val)
{
	int rc = 0;
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_perf_level perf;
	vcd_property_hdr.prop_id = VCD_REQ_PERF_LEVEL;
	vcd_property_hdr.sz =
		sizeof(struct vcd_property_perf_level);
	perf.level = VCD_PERF_LEVEL2;
	rc = vcd_set_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &perf);
	return rc;
}
static long venc_set_header_mode(struct video_client_ctx *client_ctx,
		__s32 mode)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_sps_pps_for_idr_enable sps_pps_for_idr_enable;
	int rc = 0;

	if (!client_ctx) {
		WFD_MSG_ERR("Invalid parameters\n");
		rc = -EINVAL;
		goto err;
	}

	vcd_property_hdr.prop_id = VCD_I_ENABLE_SPS_PPS_FOR_IDR;
	vcd_property_hdr.sz = sizeof(sps_pps_for_idr_enable);
	switch (mode) {
	case V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE:
		sps_pps_for_idr_enable.sps_pps_for_idr_enable_flag = 0;
		break;
	case V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_I_FRAME:
		sps_pps_for_idr_enable.sps_pps_for_idr_enable_flag = 1;
		break;
	case V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_1ST_FRAME:
	default:
		WFD_MSG_ERR("Video header mode %d not supported\n",
				mode);
		rc = -ENOTSUPP;
		goto err;
	}

	rc =  vcd_set_property(client_ctx->vcd_handle, &vcd_property_hdr,
			&sps_pps_for_idr_enable);
	if (rc) {
		WFD_MSG_ERR("Failed to set enable_sps_pps_for_idr\n");
		goto err;
	}
err:
	return rc;
}

static long venc_get_header_mode(struct video_client_ctx *client_ctx,
		__s32 *mode)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_sps_pps_for_idr_enable sps_pps_for_idr_enable;
	int rc = 0;

	if (!client_ctx) {
		WFD_MSG_ERR("Invalid parameters\n");
		rc = -EINVAL;
		goto err;
	}

	vcd_property_hdr.prop_id = VCD_I_ENABLE_SPS_PPS_FOR_IDR;
	vcd_property_hdr.sz = sizeof(sps_pps_for_idr_enable);
	rc =  vcd_get_property(client_ctx->vcd_handle, &vcd_property_hdr,
			&sps_pps_for_idr_enable);
	if (rc) {
		WFD_MSG_ERR("Failed to get sps/pps for idr enable\n");
		goto err;
	}

	*mode = sps_pps_for_idr_enable.sps_pps_for_idr_enable_flag ?
		V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_I_FRAME :
		V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE;
err:
	return rc;
}

static long venc_set_multislicing_mode(struct video_client_ctx *client_ctx,
			__u32 control, __s32 value)
{
	int rc = 0;
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_frame_size vcd_frame_size;
	struct vcd_buffer_requirement vcd_buf_req;
	struct vcd_property_multi_slice vcd_multi_slice;

	if (!client_ctx) {
		WFD_MSG_ERR("Invalid parameters\n");
		rc = -EINVAL;
		goto set_multislicing_mode_fail;
	}

	vcd_property_hdr.prop_id = VCD_I_FRAME_SIZE;
	vcd_property_hdr.sz =
		sizeof(vcd_frame_size);
	rc = vcd_get_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &vcd_frame_size);

	if (rc) {
		WFD_MSG_ERR("Failed to get frame size\n");
		goto set_multislicing_mode_fail;
	}

	rc = vcd_get_buffer_requirements(client_ctx->vcd_handle,
			VCD_BUFFER_OUTPUT, &vcd_buf_req);

	if (rc) {
		WFD_MSG_ERR("Failed to get buf reqs\n");
		goto set_multislicing_mode_fail;
	}

	vcd_property_hdr.prop_id = VCD_I_MULTI_SLICE;
	vcd_property_hdr.sz = sizeof(vcd_multi_slice);
	rc = vcd_get_property(client_ctx->vcd_handle, &vcd_property_hdr,
			&vcd_multi_slice);
	if (rc) {
		WFD_MSG_ERR("Failed to get multi slice\n");
		goto set_multislicing_mode_fail;
	}

	switch (control) {
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES:
		if (vcd_multi_slice.m_slice_sel !=
				VCD_MSLICE_BY_BYTE_COUNT) {
			WFD_MSG_ERR("Not in proper mode\n");
			goto set_multislicing_mode_fail;
		}
		vcd_multi_slice.m_slice_size = value;
		break;

	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB:
		if (vcd_multi_slice.m_slice_sel !=
				VCD_MSLICE_BY_MB_COUNT) {
			WFD_MSG_ERR("Not in proper mode\n");
			goto set_multislicing_mode_fail;
		}
		vcd_multi_slice.m_slice_size = value;
		break;

	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE:
		switch (value) {
		case V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE:
			vcd_multi_slice.m_slice_sel = VCD_MSLICE_OFF;
			break;
		case V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_MB:
			vcd_multi_slice.m_slice_sel = VCD_MSLICE_BY_MB_COUNT;
			/* Just a temporary size until client calls
			 * V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB */
			vcd_multi_slice.m_slice_size =
				(vcd_frame_size.stride / 16) *
				(vcd_frame_size.scan_lines / 16);
			break;
		case V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_BYTES:
			vcd_multi_slice.m_slice_sel = VCD_MSLICE_BY_BYTE_COUNT;
			/* Just a temporary size until client calls
			 * V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES */
			vcd_multi_slice.m_slice_size = vcd_buf_req.sz;
			break;
		default:
			WFD_MSG_ERR("Unrecognized mode %d\n", value);
			rc = -ENOTSUPP;
			goto set_multislicing_mode_fail;
		}

		break;
	default:
		rc = -EINVAL;
		goto set_multislicing_mode_fail;
	}

	rc = vcd_set_property(client_ctx->vcd_handle, &vcd_property_hdr,
			&vcd_multi_slice);
	if (rc) {
		WFD_MSG_ERR("Failed to set multi slice\n");
		goto set_multislicing_mode_fail;
	}

set_multislicing_mode_fail:
	return rc;
}

static long venc_get_multislicing_mode(struct video_client_ctx *client_ctx,
			__u32 control, __s32 *value)
{
	int rc = 0;
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_frame_size vcd_frame_size;
	struct vcd_buffer_requirement vcd_buf_req;
	struct vcd_property_multi_slice vcd_multi_slice;

	if (!client_ctx) {
		WFD_MSG_ERR("Invalid parameters\n");
		rc = -EINVAL;
		goto get_multislicing_mode_fail;
	}

	vcd_property_hdr.prop_id = VCD_I_FRAME_SIZE;
	vcd_property_hdr.sz =
		sizeof(vcd_frame_size);
	rc = vcd_get_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &vcd_frame_size);

	if (rc) {
		WFD_MSG_ERR("Failed to get frame size\n");
		goto get_multislicing_mode_fail;
	}

	vcd_property_hdr.prop_id = VCD_I_MULTI_SLICE;
	vcd_property_hdr.sz = sizeof(vcd_multi_slice);
	rc = vcd_get_property(client_ctx->vcd_handle, &vcd_property_hdr,
			&vcd_multi_slice);
	if (rc) {
		WFD_MSG_ERR("Failed to get multi slice\n");
		goto get_multislicing_mode_fail;
	}

	rc = vcd_get_buffer_requirements(client_ctx->vcd_handle,
			VCD_BUFFER_OUTPUT, &vcd_buf_req);

	if (rc) {
		WFD_MSG_ERR("Failed to get buf reqs\n");
		goto get_multislicing_mode_fail;
	}

	switch (control) {
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES:
		if (vcd_multi_slice.m_slice_sel == VCD_MSLICE_BY_BYTE_COUNT)
			*value = vcd_multi_slice.m_slice_size;
		else {
			WFD_MSG_ERR("Invalid query when in slice mode %d\n",
					vcd_multi_slice.m_slice_sel);
			rc = -EINVAL;
			goto get_multislicing_mode_fail;
		}
		break;

	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB:
		if (vcd_multi_slice.m_slice_sel == VCD_MSLICE_BY_MB_COUNT)
			*value = vcd_multi_slice.m_slice_size;
		else {
			WFD_MSG_ERR("Invalid query when in slice mode %d\n",
					vcd_multi_slice.m_slice_sel);
			rc = -EINVAL;
			goto get_multislicing_mode_fail;
		}
		break;

	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE:
		switch (vcd_multi_slice.m_slice_sel) {
		case VCD_MSLICE_OFF:
			*value = V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE;
			break;
		case VCD_MSLICE_BY_MB_COUNT:
			*value = V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_MB;
			break;
		case VCD_MSLICE_BY_BYTE_COUNT:
			*value = V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_BYTES;
			break;
		default:
			WFD_MSG_ERR("Encoder in an unknown mode %d\n",
					vcd_multi_slice.m_slice_sel);
			rc = -ENOENT;
			goto get_multislicing_mode_fail;

		}
		break;
	default:
		rc = -EINVAL;
		goto get_multislicing_mode_fail;
	}

get_multislicing_mode_fail:
	return rc;
}

static long venc_set_entropy_mode(struct video_client_ctx *client_ctx,
		__s32 value)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_entropy_control entropy_control;
	int rc = 0;

	if (!client_ctx) {
		WFD_MSG_ERR("Invalid parameters\n");
		rc = -EINVAL;
		goto set_entropy_mode_fail;
	}

	vcd_property_hdr.prop_id = VCD_I_ENTROPY_CTRL;
	vcd_property_hdr.sz = sizeof(entropy_control);

	switch (value) {
	case V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CAVLC:
		entropy_control.entropy_sel = VCD_ENTROPY_SEL_CAVLC;
		break;
	case V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CABAC:
		entropy_control.entropy_sel = VCD_ENTROPY_SEL_CABAC;
		entropy_control.cabac_model = VCD_CABAC_MODEL_NUMBER_0;
		break;
	default:
		WFD_MSG_ERR("Entropy type %d not supported\n", value);
		rc = -ENOTSUPP;
		goto set_entropy_mode_fail;
	}
	rc = vcd_set_property(client_ctx->vcd_handle, &vcd_property_hdr,
			&entropy_control);
	if (rc) {
		WFD_MSG_ERR("Failed to set entropy mode\n");
		goto set_entropy_mode_fail;
	}

set_entropy_mode_fail:
	return rc;
}

static long venc_get_entropy_mode(struct video_client_ctx *client_ctx,
		__s32 *value)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_entropy_control entropy_control;
	int rc = 0;

	if (!client_ctx || !value) {
		WFD_MSG_ERR("Invalid parameters\n");
		rc = -EINVAL;
		goto get_entropy_mode_fail;
	}

	vcd_property_hdr.prop_id = VCD_I_ENTROPY_CTRL;
	vcd_property_hdr.sz = sizeof(entropy_control);

	rc = vcd_get_property(client_ctx->vcd_handle, &vcd_property_hdr,
			&entropy_control);

	if (rc) {
		WFD_MSG_ERR("Failed to get entropy mode\n");
		goto get_entropy_mode_fail;
	}

	switch (entropy_control.entropy_sel) {
	case VCD_ENTROPY_SEL_CAVLC:
		*value = V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CAVLC;
		break;
	case VCD_ENTROPY_SEL_CABAC:
		*value = V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CABAC;
		break;
	default:
		WFD_MSG_ERR("Entropy type %d not known\n",
				entropy_control.entropy_sel);
		rc = -EINVAL;
		goto get_entropy_mode_fail;
	}
get_entropy_mode_fail:
	return rc;
}

static long venc_set_input_buffer(struct v4l2_subdev *sd, void *arg)
{
	struct mem_region *mregion = arg;
	struct venc_inst *inst = sd->dev_priv;
	unsigned long paddr, kvaddr, temp;
	struct video_client_ctx *client_ctx = &inst->venc_client;
	int rc = 0;

	if (!client_ctx || !mregion) {
		WFD_MSG_ERR("Invalid input\n");
		rc = -EINVAL;
		goto ins_table_fail;
	}

	kvaddr = (unsigned long)mregion->kvaddr;
	paddr = (unsigned long)mregion->paddr;

	if (!kvaddr || !paddr) {
		WFD_MSG_ERR("Invalid addresses\n");
		rc = -EINVAL;
		goto ins_table_fail;
	}

	/*
	 * Just a note: the third arg of vidc_insert_\
	 * addr_table_kernel is supposed to be a userspace
	 * address that is used as a key in the table. As
	 * these bufs never leave the kernel, we need to have
	 * an unique value to use as a key.  So re-using kernel
	 * virtual addr for this purpose
	 */
	rc = vidc_insert_addr_table_kernel(client_ctx,
		BUFFER_TYPE_INPUT, kvaddr, kvaddr,
		paddr, 32, mregion->size);

	if (rc == (u32)false) {
		WFD_MSG_ERR("Failed to insert input buffer into table\n");
		rc = -EFAULT;
		goto ins_table_fail;
	}

	rc = vcd_set_buffer(client_ctx->vcd_handle,
			VCD_BUFFER_INPUT, (u8 *)kvaddr,
			mregion->size);

	if (rc) {
		WFD_MSG_ERR("Failed to set input buffer\n");
		rc = -EFAULT;
		goto set_input_buf_fail;
	}


	return rc;

set_input_buf_fail:
	vidc_delete_addr_table(client_ctx, BUFFER_TYPE_INPUT,
			kvaddr, &temp);
ins_table_fail:
	return rc;
}

static long venc_set_output_buffer(struct v4l2_subdev *sd, void *arg)
{
	int rc = 0;
	struct venc_inst *inst = sd->dev_priv;
	struct video_client_ctx *client_ctx = &inst->venc_client;
	struct mem_region *mregion = arg;
	if (!client_ctx || !mregion) {
		WFD_MSG_ERR("Invalid input\n");
		return -EINVAL;
	}
	WFD_MSG_DBG("size = %u, offset = %u fd = %d\n", mregion->size,
				mregion->offset, mregion->fd);
	rc = vidc_insert_addr_table(client_ctx, BUFFER_TYPE_OUTPUT,
					mregion->cookie,
					(unsigned long *)&mregion->kvaddr,
					mregion->fd,
					mregion->offset,
					32,
					mregion->size);
	if (rc == (u32)false) {
		WFD_MSG_ERR("Failed to insert outbuf in table\n");
		rc = -EINVAL;
		goto err;
	}
	WFD_MSG_DBG("size = %u, %p\n", mregion->size, mregion->kvaddr);

	rc = vcd_set_buffer(client_ctx->vcd_handle,
				    VCD_BUFFER_OUTPUT, (u8 *) mregion->kvaddr,
				    mregion->size);
	if (rc)
		WFD_MSG_ERR("Failed to set outbuf on encoder\n");
err:
	return rc;
}

static long venc_fill_outbuf(struct v4l2_subdev *sd, void *arg)
{
	int rc = 0;
	struct venc_inst *inst = sd->dev_priv;
	struct video_client_ctx *client_ctx = &inst->venc_client;
	struct mem_region *mregion = arg;
	struct vcd_frame_data vcd_frame = {0};
	unsigned long kernel_vaddr, phy_addr, user_vaddr;
	int pmem_fd;
	struct file *file;
	s32 buffer_index = -1;

	user_vaddr = mregion->cookie;
	rc = vidc_lookup_addr_table(client_ctx, BUFFER_TYPE_OUTPUT,
			true, &user_vaddr,
			&kernel_vaddr, &phy_addr, &pmem_fd, &file,
			&buffer_index);
	if (!rc) {
		WFD_MSG_ERR("Address lookup failed\n");
		goto err;
	}
	vcd_frame.virtual = (u8 *) kernel_vaddr;
	vcd_frame.frm_clnt_data = mregion->cookie;
	vcd_frame.alloc_len = mregion->size;

	rc = vcd_fill_output_buffer(client_ctx->vcd_handle,	&vcd_frame);
	if (rc)
		WFD_MSG_ERR("Failed to fill output buffer on encoder");
err:
	return rc;
}

static long venc_encode_frame(struct v4l2_subdev *sd, void *arg)
{
	int rc = 0;
	struct venc_inst *inst = sd->dev_priv;
	struct video_client_ctx *client_ctx = &inst->venc_client;
	struct venc_buf_info *venc_buf = arg;
	struct mem_region *mregion = venc_buf->mregion;
	struct vcd_frame_data vcd_input_buffer = {0};

	vcd_input_buffer.virtual = mregion->kvaddr;
	vcd_input_buffer.frm_clnt_data = (u32)mregion;
	vcd_input_buffer.ip_frm_tag = (u32)mregion;
	vcd_input_buffer.data_len = mregion->size;
	vcd_input_buffer.time_stamp = venc_buf->timestamp;
	vcd_input_buffer.offset = 0;

	rc = vcd_encode_frame(client_ctx->vcd_handle,
			&vcd_input_buffer);

	if (rc)
		WFD_MSG_ERR("encode frame failed\n");
	return rc;
}

static long venc_alloc_recon_buffers(struct v4l2_subdev *sd, void *arg)
{
	int rc = 0;
	struct venc_inst *inst = sd->dev_priv;
	struct video_client_ctx *client_ctx = &inst->venc_client;
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_buffer_size control;
	struct vcd_property_enc_recon_buffer *ctrl = NULL;
	unsigned long phy_addr;
	int i = 0;
	int flags = 0;
	u32 len;
	control.width = inst->width;
	control.height = inst->height;
	vcd_property_hdr.prop_id = VCD_I_GET_RECON_BUFFER_SIZE;
	vcd_property_hdr.sz = sizeof(struct vcd_property_buffer_size);

	rc = vcd_get_property(client_ctx->vcd_handle,
					&vcd_property_hdr, &control);
	if (rc) {
		WFD_MSG_ERR("Failed to get recon buf size\n");
		goto err;
	}
	flags = ION_HEAP(ION_CP_MM_HEAP_ID);
	flags |= inst->secure ? ION_SECURE : ION_HEAP(ION_IOMMU_HEAP_ID);

	if (vcd_get_ion_status()) {
		for (i = 0; i < 4; ++i) {
			ctrl = &client_ctx->recon_buffer[i];
			ctrl->buffer_size = control.size;
			ctrl->pmem_fd = 0;
			ctrl->offset = 0;
			ctrl->user_virtual_addr = (void *)i;
			client_ctx->recon_buffer_ion_handle[i]
				= ion_alloc(client_ctx->user_ion_client,
			control.size, SZ_8K, flags);

			ctrl->kernel_virtual_addr = ion_map_kernel(
				client_ctx->user_ion_client,
				client_ctx->recon_buffer_ion_handle[i],	0);

			rc = ion_map_iommu(client_ctx->user_ion_client,
				client_ctx->recon_buffer_ion_handle[i],
				VIDEO_DOMAIN, VIDEO_MAIN_POOL, SZ_4K,
				0, &phy_addr, (unsigned long *)&len, 0, 0);
			if (rc) {
				WFD_MSG_ERR("Failed to allo recon buffers\n");
				break;
			}
			ctrl->physical_addr =  (u8 *) phy_addr;
			ctrl->dev_addr = ctrl->physical_addr;
			vcd_property_hdr.prop_id = VCD_I_RECON_BUFFERS;
			vcd_property_hdr.sz =
				sizeof(struct vcd_property_enc_recon_buffer);
			rc = vcd_set_property(client_ctx->vcd_handle,
					&vcd_property_hdr, ctrl);
			if (rc) {
				WFD_MSG_ERR("Failed to set recon buffers\n");
				break;
			}
		}
	} else {
		WFD_MSG_ERR("PMEM not suported\n");
		return -ENOMEM;
	}
err:
	return rc;
}

static long venc_free_output_buffer(struct v4l2_subdev *sd, void *arg)
{
	int rc = 0;
	struct venc_inst *inst = sd->dev_priv;
	struct video_client_ctx *client_ctx = &inst->venc_client;
	struct mem_region *mregion = arg;
	unsigned long kernel_vaddr, user_vaddr;

	if (!client_ctx || !mregion) {
		WFD_MSG_ERR("Invalid input\n");
		return -EINVAL;
	}

	user_vaddr = mregion->cookie;
	rc = vidc_delete_addr_table(client_ctx, BUFFER_TYPE_OUTPUT,
				user_vaddr,
				&kernel_vaddr);
	if (!rc) {
		WFD_MSG_ERR("Failed to delete buf from address table\n");
		return -EINVAL;
	}
	return vcd_free_buffer(client_ctx->vcd_handle, VCD_BUFFER_OUTPUT,
					 (u8 *)kernel_vaddr);
}

static long venc_free_input_buffer(struct v4l2_subdev *sd, void *arg)
{
	int del_rc = 0, free_rc = 0;
	struct venc_inst *inst = sd->dev_priv;
	struct video_client_ctx *client_ctx = &inst->venc_client;
	struct mem_region *mregion = arg;
	unsigned long vidc_kvaddr;

	if (!client_ctx || !mregion) {
		WFD_MSG_ERR("Invalid input\n");
		return -EINVAL;
	}

	del_rc = vidc_delete_addr_table(client_ctx, BUFFER_TYPE_INPUT,
				(unsigned long)mregion->kvaddr,
				&vidc_kvaddr);
	/*
	 * Even if something went wrong in when
	 * deleting from table, call vcd_free_buf
	 */
	if (del_rc == (u32)false) {
		WFD_MSG_ERR("Failed to delete buf from address table\n");
		del_rc = -ENOKEY;
	} else if ((u8 *)vidc_kvaddr != mregion->kvaddr) {
		WFD_MSG_ERR("Failed to find expected buffer\n");
		del_rc = -EINVAL;
	} else
		del_rc = 0;

	free_rc = vcd_free_buffer(client_ctx->vcd_handle, VCD_BUFFER_INPUT,
					 (u8 *)vidc_kvaddr);

	if (free_rc) {
		WFD_MSG_ERR("Failed to free buffer from encoder\n");
		free_rc = -EINVAL;
	}

	return del_rc ? del_rc : free_rc;
}

static long venc_free_recon_buffers(struct v4l2_subdev *sd, void *arg)
{
	int rc = 0;
	struct venc_inst *inst = sd->dev_priv;
	struct video_client_ctx *client_ctx = &inst->venc_client;
	struct vcd_property_hdr vcd_property_hdr;
	int i;

	if (vcd_get_ion_status()) {
		for (i = 0; i < 4; i++) {
			vcd_property_hdr.prop_id = VCD_I_FREE_RECON_BUFFERS;
			vcd_property_hdr.sz =
				sizeof(struct vcd_property_buffer_size);
			rc = vcd_set_property(client_ctx->vcd_handle,
			&vcd_property_hdr, &client_ctx->recon_buffer[i]);
			if (rc)
				WFD_MSG_ERR("Failed to free recon buffer\n");

			if (client_ctx->recon_buffer_ion_handle[i]) {
				ion_unmap_iommu(client_ctx->user_ion_client,
					 client_ctx->recon_buffer_ion_handle[i],
					 VIDEO_DOMAIN, VIDEO_MAIN_POOL);
				ion_unmap_kernel(client_ctx->user_ion_client,
					client_ctx->recon_buffer_ion_handle[i]);
				ion_free(client_ctx->user_ion_client,
					client_ctx->recon_buffer_ion_handle[i]);
				client_ctx->recon_buffer_ion_handle[i] = NULL;
			}
		}
	}
	return rc;
}

static long venc_set_property(struct v4l2_subdev *sd, void *arg)
{
	int rc = 0;
	struct venc_inst *inst = sd->dev_priv;
	struct v4l2_control *ctrl = arg;
	struct video_client_ctx *client_ctx = &inst->venc_client;
	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDEO_BITRATE:
		rc = venc_set_bitrate(client_ctx, ctrl->value);
		break;
	case V4L2_CID_MPEG_VIDEO_BITRATE_MODE:
		rc = venc_set_bitrate_mode(client_ctx, ctrl->value);
		break;
	case V4L2_CID_MPEG_VIDEO_H264_I_PERIOD:
		rc = venc_set_h264_intra_period(client_ctx, ctrl->value);
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		rc = venc_set_codec_level(client_ctx, ctrl->id, ctrl->value);
		break;
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		rc = venc_set_codec_profile(client_ctx, ctrl->id, ctrl->value);
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE:
		rc = venc_request_frame(client_ctx, ctrl->value);
		break;
	case V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_H263_I_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_H263_P_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_H263_B_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_MPEG4_I_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_MPEG4_P_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_MPEG4_B_FRAME_QP:
		rc = venc_set_qp_value(client_ctx, ctrl->id, ctrl->value);
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_MIN_QP:
	case V4L2_CID_MPEG_VIDEO_MPEG4_MAX_QP:
	case V4L2_CID_MPEG_VIDEO_H263_MIN_QP:
	case V4L2_CID_MPEG_VIDEO_H263_MAX_QP:
	case V4L2_CID_MPEG_VIDEO_H264_MIN_QP:
	case V4L2_CID_MPEG_VIDEO_H264_MAX_QP:
		rc = venc_set_qp_range(client_ctx, ctrl->id, ctrl->value);
		break;
	case V4L2_CID_MPEG_VIDEO_HEADER_MODE:
		rc = venc_set_header_mode(client_ctx, ctrl->value);
		break;
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES:
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB:
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE:
		rc = venc_set_multislicing_mode(client_ctx, ctrl->id,
				ctrl->value);
		break;
	case V4L2_CID_MPEG_QCOM_SET_PERF_LEVEL:
		rc = venc_set_max_perf_level(client_ctx, ctrl->value);
		break;
	case V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE:
		rc = venc_set_entropy_mode(client_ctx, ctrl->value);
		break;
	default:
		WFD_MSG_ERR("Set property not suported: %d\n", ctrl->id);
		rc = -ENOTSUPP;
		break;
	}
	return rc;
}

static long venc_get_property(struct v4l2_subdev *sd, void *arg)
{
	int rc = 0;
	struct venc_inst *inst = sd->dev_priv;
	struct v4l2_control *ctrl = arg;
	struct video_client_ctx *client_ctx = &inst->venc_client;

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDEO_BITRATE:
		rc = venc_get_bitrate(client_ctx, &ctrl->value);
		break;
	case V4L2_CID_MPEG_VIDEO_BITRATE_MODE:
		rc = venc_get_bitrate_mode(client_ctx, &ctrl->value);
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		rc = venc_get_codec_level(client_ctx, ctrl->id, &ctrl->value);
		break;
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		rc = venc_get_codec_profile(client_ctx, ctrl->id, &ctrl->value);
		break;
	case V4L2_CID_MPEG_VIDEO_H264_I_PERIOD:
		rc = venc_get_h264_intra_period(client_ctx, &ctrl->value);
		break;
	case V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_H263_I_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_H263_P_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_H263_B_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_MPEG4_I_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_MPEG4_P_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_MPEG4_B_FRAME_QP:
		rc = venc_get_qp_value(client_ctx, ctrl->id, &ctrl->value);
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_MIN_QP:
	case V4L2_CID_MPEG_VIDEO_MPEG4_MAX_QP:
	case V4L2_CID_MPEG_VIDEO_H263_MIN_QP:
	case V4L2_CID_MPEG_VIDEO_H263_MAX_QP:
	case V4L2_CID_MPEG_VIDEO_H264_MIN_QP:
	case V4L2_CID_MPEG_VIDEO_H264_MAX_QP:
		rc = venc_get_qp_range(client_ctx, ctrl->id, &ctrl->value);
		break;
	case V4L2_CID_MPEG_VIDEO_HEADER_MODE:
		rc = venc_get_header_mode(client_ctx, &ctrl->value);
		break;
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES:
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB:
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE:
		rc = venc_get_multislicing_mode(client_ctx, ctrl->id,
				&ctrl->value);
		break;
	case V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE:
		rc = venc_get_entropy_mode(client_ctx, &ctrl->value);
		break;
	default:
		WFD_MSG_ERR("Get property not suported: %d\n", ctrl->id);
		rc = -ENOTSUPP;
		break;
	}
	return rc;
}

long venc_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	long rc = 0;
	switch (cmd) {
	case OPEN:
		rc = venc_open(sd, arg);
		break;
	case CLOSE:
		rc = venc_close(sd, arg);
		break;
	case ENCODE_START:
		rc = venc_start(sd);
		break;
	case ENCODE_FRAME:
		venc_encode_frame(sd, arg);
		break;
	case ENCODE_STOP:
		rc = venc_stop(sd);
		break;
	case SET_PROP:
		rc = venc_set_property(sd, arg);
		break;
	case GET_PROP:
		rc = venc_get_property(sd, arg);
		break;
	case GET_BUFFER_REQ:
		rc = venc_get_buffer_req(sd, arg);
		break;
	case SET_BUFFER_REQ:
		rc = venc_set_buffer_req(sd, arg);
		break;
	case FREE_BUFFER:
		break;
	case FILL_OUTPUT_BUFFER:
		rc = venc_fill_outbuf(sd, arg);
		break;
	case SET_FORMAT:
		rc = venc_set_format(sd, arg);
		break;
	case SET_FRAMERATE:
		rc = venc_set_framerate(sd, arg);
		break;
	case SET_INPUT_BUFFER:
		rc = venc_set_input_buffer(sd, arg);
		break;
	case SET_OUTPUT_BUFFER:
		rc = venc_set_output_buffer(sd, arg);
		break;
	case ALLOC_RECON_BUFFERS:
		rc = venc_alloc_recon_buffers(sd, arg);
		break;
	case FREE_OUTPUT_BUFFER:
		rc = venc_free_output_buffer(sd, arg);
		break;
	case FREE_INPUT_BUFFER:
		rc = venc_free_input_buffer(sd, arg);
		break;
	case FREE_RECON_BUFFERS:
		rc = venc_free_recon_buffers(sd, arg);
		break;
	default:
		rc = -1;
		break;
	}
	return rc;
}
