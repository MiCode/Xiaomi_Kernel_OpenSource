/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/android_pmem.h>
#include <linux/clk.h>
#include <linux/timer.h>
#include <mach/msm_subsystem_map.h>
#include <media/msm/vidc_type.h>
#include <media/msm/vcd_api.h>
#include <media/msm/vidc_init.h>
#include "vcd_res_tracker_api.h"
#include "vdec_internal.h"



#define DBG(x...) pr_debug(x)
#define INFO(x...) pr_info(x)
#define ERR(x...) pr_err(x)

#define VID_DEC_NAME "msm_vidc_dec"

static char *node_name[2] = {"", "_sec"};
static struct vid_dec_dev *vid_dec_device_p;
static dev_t vid_dec_dev_num;
static struct class *vid_dec_class;

static unsigned int vidc_mmu_subsystem[] = {
	MSM_SUBSYSTEM_VIDEO};
static s32 vid_dec_get_empty_client_index(void)
{
	u32 i, found = false;

	for (i = 0; i < VIDC_MAX_NUM_CLIENTS; i++) {
		if (!vid_dec_device_p->vdec_clients[i].vcd_handle) {
			found = true;
			break;
		}
	}
	if (!found) {
		ERR("%s():ERROR No space for new client\n", __func__);
		return -ENOMEM;
	} else {
		DBG("%s(): available client index = %u\n", __func__, i);
		return i;
	}
}

u32 vid_dec_get_status(u32 status)
{
	u32 vdec_status;

	switch (status) {
	case VCD_ERR_SEQHDR_PARSE_FAIL:
	case VCD_ERR_BITSTREAM_ERR:
		vdec_status = VDEC_S_INPUT_BITSTREAM_ERR;
		break;
	case VCD_S_SUCCESS:
		vdec_status = VDEC_S_SUCCESS;
		break;
	case VCD_ERR_FAIL:
		vdec_status = VDEC_S_EFAIL;
		break;
	case VCD_ERR_ALLOC_FAIL:
		vdec_status = VDEC_S_ENOSWRES;
		break;
	case VCD_ERR_ILLEGAL_OP:
		vdec_status = VDEC_S_EINVALCMD;
		break;
	case VCD_ERR_ILLEGAL_PARM:
		vdec_status = VDEC_S_EBADPARAM;
		break;
	case VCD_ERR_BAD_POINTER:
	case VCD_ERR_BAD_HANDLE:
		vdec_status = VDEC_S_EFATAL;
		break;
	case VCD_ERR_NOT_SUPPORTED:
		vdec_status = VDEC_S_ENOTSUPP;
		break;
	case VCD_ERR_BAD_STATE:
		vdec_status = VDEC_S_EINVALSTATE;
		break;
	case VCD_ERR_BUSY:
		vdec_status = VDEC_S_BUSY;
		break;
	case VCD_ERR_MAX_CLIENT:
		vdec_status = VDEC_S_ENOHWRES;
		break;
	default:
		vdec_status = VDEC_S_EFAIL;
		break;
	}

	return vdec_status;
}

static void vid_dec_notify_client(struct video_client_ctx *client_ctx)
{
	if (client_ctx)
		complete(&client_ctx->event);
}

void vid_dec_vcd_open_done(struct video_client_ctx *client_ctx,
			   struct vcd_handle_container *handle_container)
{
	DBG("vid_dec_vcd_open_done\n");

	if (client_ctx) {
		if (handle_container)
			client_ctx->vcd_handle = handle_container->handle;
		else
			ERR("%s(): ERROR. handle_container is NULL\n",
			    __func__);

		vid_dec_notify_client(client_ctx);
	} else
		ERR("%s(): ERROR. client_ctx is NULL\n", __func__);
}

static void vid_dec_handle_field_drop(struct video_client_ctx *client_ctx,
	u32 event, u32 status, int64_t time_stamp)
{
	struct vid_dec_msg *vdec_msg;

	if (!client_ctx) {
		ERR("%s() NULL pointer\n", __func__);
		return;
	}

	vdec_msg = kzalloc(sizeof(struct vid_dec_msg), GFP_KERNEL);
	if (!vdec_msg) {
		ERR("%s(): cannot allocate vid_dec_msg "
			" buffer\n", __func__);
		return;
	}
	vdec_msg->vdec_msg_info.status_code = vid_dec_get_status(status);
	if (event == VCD_EVT_IND_INFO_FIELD_DROPPED) {
		vdec_msg->vdec_msg_info.msgcode =
			VDEC_MSG_EVT_INFO_FIELD_DROPPED;
		vdec_msg->vdec_msg_info.msgdata.output_frame.time_stamp
		= time_stamp;
		DBG("Send FIELD_DROPPED message to client = %p\n", client_ctx);
	} else {
		ERR("vid_dec_input_frame_done(): invalid event type: "
			"%d\n", event);
		vdec_msg->vdec_msg_info.msgcode = VDEC_MSG_INVALID;
	}
	vdec_msg->vdec_msg_info.msgdatasize =
		sizeof(struct vdec_output_frameinfo);
	mutex_lock(&client_ctx->msg_queue_lock);
	list_add_tail(&vdec_msg->list, &client_ctx->msg_queue);
	mutex_unlock(&client_ctx->msg_queue_lock);
	wake_up(&client_ctx->msg_wait);
}

static void vid_dec_input_frame_done(struct video_client_ctx *client_ctx,
				     u32 event, u32 status,
				     struct vcd_frame_data *vcd_frame_data)
{
	struct vid_dec_msg *vdec_msg;

	if (!client_ctx || !vcd_frame_data) {
		ERR("vid_dec_input_frame_done() NULL pointer\n");
		return;
	}

	kfree(vcd_frame_data->desc_buf);
	vcd_frame_data->desc_buf = NULL;
	vcd_frame_data->desc_size = 0;

	vdec_msg = kzalloc(sizeof(struct vid_dec_msg), GFP_KERNEL);
	if (!vdec_msg) {
		ERR("vid_dec_input_frame_done(): cannot allocate vid_dec_msg "
		    " buffer\n");
		return;
	}

	vdec_msg->vdec_msg_info.status_code = vid_dec_get_status(status);

	if (event == VCD_EVT_RESP_INPUT_DONE) {
		vdec_msg->vdec_msg_info.msgcode =
		    VDEC_MSG_RESP_INPUT_BUFFER_DONE;
		DBG("Send INPUT_DON message to client = %p\n", client_ctx);

	} else if (event == VCD_EVT_RESP_INPUT_FLUSHED) {
		vdec_msg->vdec_msg_info.msgcode = VDEC_MSG_RESP_INPUT_FLUSHED;
		DBG("Send INPUT_FLUSHED message to client = %p\n", client_ctx);
	} else {
		ERR("vid_dec_input_frame_done(): invalid event type: "
			"%d\n", event);
		vdec_msg->vdec_msg_info.msgcode = VDEC_MSG_INVALID;
	}

	vdec_msg->vdec_msg_info.msgdata.input_frame_clientdata =
	    (void *)vcd_frame_data->frm_clnt_data;
	vdec_msg->vdec_msg_info.msgdatasize = sizeof(void *);

	mutex_lock(&client_ctx->msg_queue_lock);
	list_add_tail(&vdec_msg->list, &client_ctx->msg_queue);
	mutex_unlock(&client_ctx->msg_queue_lock);
	wake_up(&client_ctx->msg_wait);
}

static void vid_dec_output_frame_done(struct video_client_ctx *client_ctx,
			u32 event, u32 status,
			struct vcd_frame_data *vcd_frame_data)
{
	struct vid_dec_msg *vdec_msg;

	unsigned long kernel_vaddr = 0, phy_addr = 0, user_vaddr = 0;
	int pmem_fd;
	struct file *file;
	s32 buffer_index = -1;
	enum vdec_picture pic_type;
	u32 ion_flag = 0;
	struct ion_handle *buff_handle = NULL;
	struct vdec_output_frameinfo  *output_frame;

	if (!client_ctx || !vcd_frame_data) {
		ERR("vid_dec_input_frame_done() NULL pointer\n");
		return;
	}

	vdec_msg = kzalloc(sizeof(struct vid_dec_msg), GFP_KERNEL);
	if (!vdec_msg) {
		ERR("vid_dec_input_frame_done(): cannot allocate vid_dec_msg "
		    " buffer\n");
		return;
	}

	vdec_msg->vdec_msg_info.status_code = vid_dec_get_status(status);

	if (event == VCD_EVT_RESP_OUTPUT_DONE)
		vdec_msg->vdec_msg_info.msgcode =
		    VDEC_MSG_RESP_OUTPUT_BUFFER_DONE;
	else if (event == VCD_EVT_RESP_OUTPUT_FLUSHED)
		vdec_msg->vdec_msg_info.msgcode = VDEC_MSG_RESP_OUTPUT_FLUSHED;
	else {
		ERR("QVD: vid_dec_output_frame_done invalid cmd type: "
			"%d\n", event);
		vdec_msg->vdec_msg_info.msgcode = VDEC_MSG_INVALID;
	}

	kernel_vaddr = (unsigned long)vcd_frame_data->virtual;

	if (vidc_lookup_addr_table(client_ctx, BUFFER_TYPE_OUTPUT,
				      false, &user_vaddr, &kernel_vaddr,
				      &phy_addr, &pmem_fd, &file,
				      &buffer_index) ||
		(vcd_frame_data->flags & VCD_FRAME_FLAG_EOS)) {
		/* Buffer address in user space */
		vdec_msg->vdec_msg_info.msgdata.output_frame.bufferaddr =
		    (u8 *) user_vaddr;
		/* Data length */
		vdec_msg->vdec_msg_info.msgdata.output_frame.len =
		    vcd_frame_data->data_len;
		vdec_msg->vdec_msg_info.msgdata.output_frame.flags =
		    vcd_frame_data->flags;
		/* Timestamp pass-through from input frame */
		vdec_msg->vdec_msg_info.msgdata.output_frame.time_stamp =
		    vcd_frame_data->time_stamp;
		/* Output frame client data */
		vdec_msg->vdec_msg_info.msgdata.output_frame.client_data =
		    (void *)vcd_frame_data->frm_clnt_data;
		/* Associated input frame client data */
		vdec_msg->vdec_msg_info.msgdata.output_frame.
		    input_frame_clientdata =
		    (void *)vcd_frame_data->ip_frm_tag;
		/* Decoded picture width and height */
		vdec_msg->vdec_msg_info.msgdata.output_frame.framesize.
		bottom =
		    vcd_frame_data->dec_op_prop.disp_frm.bottom;
		vdec_msg->vdec_msg_info.msgdata.output_frame.framesize.left =
		    vcd_frame_data->dec_op_prop.disp_frm.left;
		vdec_msg->vdec_msg_info.msgdata.output_frame.framesize.right =
			vcd_frame_data->dec_op_prop.disp_frm.right;
		vdec_msg->vdec_msg_info.msgdata.output_frame.framesize.top =
			vcd_frame_data->dec_op_prop.disp_frm.top;
		if (vcd_frame_data->interlaced) {
			vdec_msg->vdec_msg_info.msgdata.
				output_frame.interlaced_format =
				VDEC_InterlaceInterleaveFrameTopFieldFirst;
		} else {
			vdec_msg->vdec_msg_info.msgdata.
				output_frame.interlaced_format =
				VDEC_InterlaceFrameProgressive;
		}
		/* Decoded picture type */
		switch (vcd_frame_data->frame) {
		case VCD_FRAME_I:
			pic_type = PICTURE_TYPE_I;
			break;
		case VCD_FRAME_P:
			pic_type = PICTURE_TYPE_P;
			break;
		case VCD_FRAME_B:
			pic_type = PICTURE_TYPE_B;
			break;
		case VCD_FRAME_NOTCODED:
			pic_type = PICTURE_TYPE_SKIP;
			break;
		case VCD_FRAME_IDR:
			pic_type = PICTURE_TYPE_IDR;
			break;
		default:
			pic_type = PICTURE_TYPE_UNKNOWN;
		}
		vdec_msg->vdec_msg_info.msgdata.output_frame.pic_type =
			pic_type;
		output_frame = &vdec_msg->vdec_msg_info.msgdata.output_frame;
		output_frame->aspect_ratio_info.aspect_ratio =
			vcd_frame_data->aspect_ratio_info.aspect_ratio;
		output_frame->aspect_ratio_info.par_width =
			vcd_frame_data->aspect_ratio_info.extended_par_width;
		output_frame->aspect_ratio_info.par_height =
			vcd_frame_data->aspect_ratio_info.extended_par_height;
		vdec_msg->vdec_msg_info.msgdatasize =
		    sizeof(struct vdec_output_frameinfo);
	} else {
		ERR("vid_dec_output_frame_done UVA can not be found\n");
		vdec_msg->vdec_msg_info.status_code = VDEC_S_EFATAL;
	}
	if (vcd_frame_data->data_len > 0) {
		ion_flag = vidc_get_fd_info(client_ctx, BUFFER_TYPE_OUTPUT,
				pmem_fd, kernel_vaddr, buffer_index,
				&buff_handle);
		if (ion_flag == CACHED) {
			msm_ion_do_cache_op(client_ctx->user_ion_client,
					buff_handle,
					(unsigned long *) kernel_vaddr,
					(unsigned long)vcd_frame_data->data_len,
					ION_IOC_INV_CACHES);
		}
	}
	mutex_lock(&client_ctx->msg_queue_lock);
	list_add_tail(&vdec_msg->list, &client_ctx->msg_queue);
	mutex_unlock(&client_ctx->msg_queue_lock);
	wake_up(&client_ctx->msg_wait);
}

static void vid_dec_lean_event(struct video_client_ctx *client_ctx,
			       u32 event, u32 status)
{
	struct vid_dec_msg *vdec_msg;

	if (!client_ctx) {
		ERR("%s(): !client_ctx pointer\n", __func__);
		return;
	}

	vdec_msg = kzalloc(sizeof(struct vid_dec_msg), GFP_KERNEL);
	if (!vdec_msg) {
		ERR("%s(): cannot allocate vid_dec_msg buffer\n", __func__);
		return;
	}

	vdec_msg->vdec_msg_info.status_code = vid_dec_get_status(status);

	switch (event) {
	case VCD_EVT_IND_OUTPUT_RECONFIG:
		DBG("msm_vidc_dec: Sending VDEC_MSG_EVT_CONFIG_CHANGED"
			 " to client");
		vdec_msg->vdec_msg_info.msgcode = VDEC_MSG_EVT_CONFIG_CHANGED;
		break;
	case VCD_EVT_IND_RESOURCES_LOST:
		DBG("msm_vidc_dec: Sending VDEC_EVT_RESOURCES_LOST"
			 " to client");
		vdec_msg->vdec_msg_info.msgcode = VDEC_EVT_RESOURCES_LOST;
		break;
	case VCD_EVT_RESP_FLUSH_INPUT_DONE:
		DBG("msm_vidc_dec: Sending VDEC_MSG_RESP_FLUSH_INPUT_DONE"
			 " to client");
		vdec_msg->vdec_msg_info.msgcode =
		    VDEC_MSG_RESP_FLUSH_INPUT_DONE;
		break;
	case VCD_EVT_RESP_FLUSH_OUTPUT_DONE:
		DBG("msm_vidc_dec: Sending VDEC_MSG_RESP_FLUSH_OUTPUT_DONE"
			 " to client");
		vdec_msg->vdec_msg_info.msgcode =
		    VDEC_MSG_RESP_FLUSH_OUTPUT_DONE;
		break;
	case VCD_EVT_IND_HWERRFATAL:
		DBG("msm_vidc_dec: Sending VDEC_MSG_EVT_HW_ERROR"
			 " to client");
		vdec_msg->vdec_msg_info.msgcode = VDEC_MSG_EVT_HW_ERROR;
		break;
	case VCD_EVT_RESP_START:
		DBG("msm_vidc_dec: Sending VDEC_MSG_RESP_START_DONE"
			 " to client");
		vdec_msg->vdec_msg_info.msgcode = VDEC_MSG_RESP_START_DONE;
		break;
	case VCD_EVT_RESP_STOP:
		DBG("msm_vidc_dec: Sending VDEC_MSG_RESP_STOP_DONE"
			 " to client");
		vdec_msg->vdec_msg_info.msgcode = VDEC_MSG_RESP_STOP_DONE;
		break;
	case VCD_EVT_RESP_PAUSE:
		DBG("msm_vidc_dec: Sending VDEC_MSG_RESP_PAUSE_DONE"
			 " to client");
		vdec_msg->vdec_msg_info.msgcode = VDEC_MSG_RESP_PAUSE_DONE;
		break;
	case VCD_EVT_IND_INFO_OUTPUT_RECONFIG:
		DBG("msm_vidc_dec: Sending VDEC_MSG_EVT_INFO_CONFIG_CHANGED"
			 " to client");
		vdec_msg->vdec_msg_info.msgcode =
			 VDEC_MSG_EVT_INFO_CONFIG_CHANGED;
		break;
	default:
		ERR("%s() : unknown event type\n", __func__);
		break;
	}

	vdec_msg->vdec_msg_info.msgdatasize = 0;
	if (client_ctx->stop_sync_cb &&
	   (event == VCD_EVT_RESP_STOP || event == VCD_EVT_IND_HWERRFATAL)) {
		client_ctx->stop_sync_cb = false;
		complete(&client_ctx->event);
		kfree(vdec_msg);
		return;
	}
	mutex_lock(&client_ctx->msg_queue_lock);
	list_add_tail(&vdec_msg->list, &client_ctx->msg_queue);
	mutex_unlock(&client_ctx->msg_queue_lock);
	wake_up(&client_ctx->msg_wait);
}


void vid_dec_vcd_cb(u32 event, u32 status,
		   void *info, size_t sz, void *handle, void *const client_data)
{
	struct video_client_ctx *client_ctx =
	    (struct video_client_ctx *)client_data;

	DBG("Entering %s()\n", __func__);

	if (!client_ctx) {
		ERR("%s(): client_ctx is NULL\n", __func__);
		return;
	}

	client_ctx->event_status = status;

	switch (event) {
	case VCD_EVT_RESP_OPEN:
		vid_dec_vcd_open_done(client_ctx,
				      (struct vcd_handle_container *)
				      info);
		break;
	case VCD_EVT_RESP_INPUT_DONE:
	case VCD_EVT_RESP_INPUT_FLUSHED:
		vid_dec_input_frame_done(client_ctx, event, status,
					 (struct vcd_frame_data *)info);
		break;
	case VCD_EVT_IND_INFO_FIELD_DROPPED:
		if (info)
			vid_dec_handle_field_drop(client_ctx, event,
			status,	*((int64_t *)info));
		else
			pr_err("Wrong Payload for Field dropped\n");
		break;
	case VCD_EVT_RESP_OUTPUT_DONE:
	case VCD_EVT_RESP_OUTPUT_FLUSHED:
		vid_dec_output_frame_done(client_ctx, event, status,
					  (struct vcd_frame_data *)info);
		break;
	case VCD_EVT_RESP_PAUSE:
	case VCD_EVT_RESP_STOP:
	case VCD_EVT_RESP_FLUSH_INPUT_DONE:
	case VCD_EVT_RESP_FLUSH_OUTPUT_DONE:
	case VCD_EVT_IND_OUTPUT_RECONFIG:
	case VCD_EVT_IND_HWERRFATAL:
	case VCD_EVT_IND_RESOURCES_LOST:
	case VCD_EVT_IND_INFO_OUTPUT_RECONFIG:
		vid_dec_lean_event(client_ctx, event, status);
		break;
	case VCD_EVT_RESP_START:
		if (!client_ctx->seq_header_set)
			vid_dec_lean_event(client_ctx, event, status);
		else
			vid_dec_notify_client(client_ctx);
		break;
	default:
		ERR("%s() :  Error - Invalid event type =%u\n", __func__,
		    event);
		break;
	}
}

static u32 vid_dec_set_codec(struct video_client_ctx *client_ctx,
			     enum vdec_codec *vdec_codec)
{
	u32 result = true;
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_codec codec;
	u32 vcd_status = VCD_ERR_FAIL;

	if (!client_ctx || !vdec_codec)
		return false;

	vcd_property_hdr.prop_id = VCD_I_CODEC;
	vcd_property_hdr.sz = sizeof(struct vcd_property_codec);

	switch (*vdec_codec) {
	case VDEC_CODECTYPE_MPEG4:
		codec.codec = VCD_CODEC_MPEG4;
		break;
	case VDEC_CODECTYPE_H264:
		codec.codec = VCD_CODEC_H264;
		break;
	case VDEC_CODECTYPE_DIVX_3:
		codec.codec = VCD_CODEC_DIVX_3;
		break;
	case VDEC_CODECTYPE_DIVX_4:
		codec.codec = VCD_CODEC_DIVX_4;
		break;
	case VDEC_CODECTYPE_DIVX_5:
		codec.codec = VCD_CODEC_DIVX_5;
		break;
	case VDEC_CODECTYPE_DIVX_6:
		codec.codec = VCD_CODEC_DIVX_6;
		break;
	case VDEC_CODECTYPE_XVID:
		codec.codec = VCD_CODEC_XVID;
		break;
	case VDEC_CODECTYPE_H263:
		codec.codec = VCD_CODEC_H263;
		break;
	case VDEC_CODECTYPE_MPEG2:
		codec.codec = VCD_CODEC_MPEG2;
		break;
	case VDEC_CODECTYPE_VC1:
		codec.codec = VCD_CODEC_VC1;
		break;
	case VDEC_CODECTYPE_VC1_RCV:
		codec.codec = VCD_CODEC_VC1_RCV;
		break;
	default:
		result = false;
		break;
	}

	if (result) {
		vcd_status = vcd_set_property(client_ctx->vcd_handle,
					      &vcd_property_hdr, &codec);
		if (vcd_status)
			result = false;
	}
	return result;
}

static u32 vid_dec_set_output_format(struct video_client_ctx *client_ctx,
				     enum vdec_output_fromat *output_format)
{
	u32 result = true;
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_buffer_format vcd_prop_buffer_format;
	u32 vcd_status = VCD_ERR_FAIL;

	if (!client_ctx || !output_format)
		return false;

	vcd_property_hdr.prop_id = VCD_I_BUFFER_FORMAT;;
	vcd_property_hdr.sz =
	    sizeof(struct vcd_property_buffer_format);

	switch (*output_format) {
	case VDEC_YUV_FORMAT_NV12:
		vcd_prop_buffer_format.buffer_format = VCD_BUFFER_FORMAT_NV12;
		break;
	case VDEC_YUV_FORMAT_TILE_4x2:
		vcd_prop_buffer_format.buffer_format =
		    VCD_BUFFER_FORMAT_TILE_4x2;
		break;
	default:
		result = false;
		break;
	}

	if (result)
		vcd_status = vcd_set_property(client_ctx->vcd_handle,
					      &vcd_property_hdr,
					      &vcd_prop_buffer_format);

	if (vcd_status)
		return false;
	else
		return true;
}

static u32 vid_dec_set_frame_resolution(struct video_client_ctx *client_ctx,
					struct vdec_picsize *video_resoultion)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_frame_size frame_resolution;
	u32 vcd_status = VCD_ERR_FAIL;

	if (!client_ctx || !video_resoultion)
		return false;

	vcd_property_hdr.prop_id = VCD_I_FRAME_SIZE;
	vcd_property_hdr.sz = sizeof(struct vcd_property_frame_size);
	frame_resolution.width = video_resoultion->frame_width;
	frame_resolution.height = video_resoultion->frame_height;
	frame_resolution.stride = video_resoultion->stride;
	frame_resolution.scan_lines = video_resoultion->scan_lines;

	vcd_status = vcd_set_property(client_ctx->vcd_handle,
				      &vcd_property_hdr, &frame_resolution);

	if (vcd_status)
		return false;
	else
		return true;
}

static u32 vid_dec_get_frame_resolution(struct video_client_ctx *client_ctx,
					struct vdec_picsize *video_resoultion)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_frame_size frame_resolution;
	u32 vcd_status = VCD_ERR_FAIL;

	if (!client_ctx || !video_resoultion)
		return false;

	vcd_property_hdr.prop_id = VCD_I_FRAME_SIZE;
	vcd_property_hdr.sz = sizeof(struct vcd_property_frame_size);

	vcd_status = vcd_get_property(client_ctx->vcd_handle, &vcd_property_hdr,
					  &frame_resolution);

	video_resoultion->frame_width = frame_resolution.width;
	video_resoultion->frame_height = frame_resolution.height;
	video_resoultion->scan_lines = frame_resolution.scan_lines;
	video_resoultion->stride = frame_resolution.stride;

	if (vcd_status)
		return false;
	else
		return true;
}

static u32 vid_dec_get_progressive_only(struct video_client_ctx *client_ctx,
					u32 *progressive_only)
{
	struct vcd_property_hdr vcd_property_hdr;
	if (!client_ctx || !progressive_only)
		return false;
	vcd_property_hdr.prop_id = VCD_I_PROGRESSIVE_ONLY;
	vcd_property_hdr.sz = sizeof(u32);
	if (vcd_get_property(client_ctx->vcd_handle, &vcd_property_hdr,
						 progressive_only))
		return false;
	else
		return true;
}

static u32 vid_dec_get_disable_dmx_support(struct video_client_ctx *client_ctx,
					   u32 *disable_dmx)
{

	struct vcd_property_hdr vcd_property_hdr;
	if (!client_ctx || !disable_dmx)
		return false;
	vcd_property_hdr.prop_id = VCD_I_DISABLE_DMX_SUPPORT;
	vcd_property_hdr.sz = sizeof(u32);
	if (vcd_get_property(client_ctx->vcd_handle, &vcd_property_hdr,
						 disable_dmx))
		return false;
	else
		return true;
}
static u32 vid_dec_get_disable_dmx(struct video_client_ctx *client_ctx,
					   u32 *disable_dmx)
{

	struct vcd_property_hdr vcd_property_hdr;
	if (!client_ctx || !disable_dmx)
		return false;
	vcd_property_hdr.prop_id = VCD_I_DISABLE_DMX;
	vcd_property_hdr.sz = sizeof(u32);
	if (vcd_get_property(client_ctx->vcd_handle, &vcd_property_hdr,
						 disable_dmx))
		return false;
	else
		return true;
}

static u32 vid_dec_set_disable_dmx(struct video_client_ctx *client_ctx)
{

	struct vcd_property_hdr vcd_property_hdr;
	u32 vcd_disable_dmx;
	if (!client_ctx)
		return false;
	vcd_property_hdr.prop_id = VCD_I_DISABLE_DMX;
	vcd_property_hdr.sz = sizeof(u32);
	vcd_disable_dmx = true;
	DBG("%s() : Setting Disable DMX: %d\n",
		__func__, vcd_disable_dmx);

	if (vcd_set_property(client_ctx->vcd_handle, &vcd_property_hdr,
						 &vcd_disable_dmx))
		return false;
	else
		return true;
}

static u32 vid_dec_set_picture_order(struct video_client_ctx *client_ctx,
					u32 *picture_order)
{
	struct vcd_property_hdr vcd_property_hdr;
	u32 vcd_status = VCD_ERR_FAIL, vcd_picture_order, ret = true;
	if (!client_ctx || !picture_order)
		return false;
	vcd_property_hdr.prop_id = VCD_I_OUTPUT_ORDER;
	vcd_property_hdr.sz = sizeof(u32);
	if (*picture_order == VDEC_ORDER_DISPLAY)
		vcd_picture_order = VCD_DEC_ORDER_DISPLAY;
	else if (*picture_order == VDEC_ORDER_DECODE)
		vcd_picture_order = VCD_DEC_ORDER_DECODE;
	else
		ret = false;
	if (ret) {
		DBG("%s() : Setting output picture order: %d\n",
		    __func__, vcd_picture_order);
		vcd_status = vcd_set_property(client_ctx->vcd_handle,
				      &vcd_property_hdr, &vcd_picture_order);
		if (vcd_status != VCD_S_SUCCESS)
			ret = false;
	}
	return ret;
}

static u32 vid_dec_set_frame_rate(struct video_client_ctx *client_ctx,
					struct vdec_framerate *frame_rate)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_frame_rate vcd_frame_rate;
	u32 vcd_status = VCD_ERR_FAIL;

	if (!client_ctx || !frame_rate)
		return false;

	vcd_property_hdr.prop_id = VCD_I_FRAME_RATE;
	vcd_property_hdr.sz = sizeof(struct vcd_property_frame_rate);
	vcd_frame_rate.fps_numerator = frame_rate->fps_numerator;
	vcd_frame_rate.fps_denominator = frame_rate->fps_denominator;

	vcd_status = vcd_set_property(client_ctx->vcd_handle,
				      &vcd_property_hdr, &vcd_frame_rate);

	if (vcd_status)
		return false;
	else
		return true;
}

static u32 vid_dec_set_extradata(struct video_client_ctx *client_ctx,
					u32 *extradata_flag)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_meta_data_enable vcd_meta_data;
	u32 vcd_status = VCD_ERR_FAIL;
	if (!client_ctx || !extradata_flag)
		return false;
	vcd_property_hdr.prop_id = VCD_I_METADATA_ENABLE;
	vcd_property_hdr.sz = sizeof(struct vcd_property_meta_data_enable);
	vcd_meta_data.meta_data_enable_flag = *extradata_flag;
	vcd_status = vcd_set_property(client_ctx->vcd_handle,
				      &vcd_property_hdr, &vcd_meta_data);
	if (vcd_status)
		return false;
	else
		return true;
}

static u32 vid_dec_set_idr_only_decoding(struct video_client_ctx *client_ctx)
{
	struct vcd_property_hdr vcd_property_hdr;
	u32 vcd_status = VCD_ERR_FAIL;
	u32 enable = true;
	if (!client_ctx)
		return false;
	vcd_property_hdr.prop_id = VCD_I_DEC_PICTYPE;
	vcd_property_hdr.sz = sizeof(u32);
	vcd_status = vcd_set_property(client_ctx->vcd_handle,
			&vcd_property_hdr, &enable);
	if (vcd_status)
		return false;
	return true;
}

static u32 vid_dec_set_h264_mv_buffers(struct video_client_ctx *client_ctx,
					struct vdec_h264_mv *mv_data)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_h264_mv_buffer *vcd_h264_mv_buffer = NULL;
	struct msm_mapped_buffer *mapped_buffer = NULL;
	u32 vcd_status = VCD_ERR_FAIL;
	u32 len = 0, flags = 0;
	struct file *file;
	int rc = 0;
	unsigned long ionflag = 0;
	unsigned long buffer_size = 0;
	unsigned long iova = 0;

	if (!client_ctx || !mv_data)
		return false;

	vcd_property_hdr.prop_id = VCD_I_H264_MV_BUFFER;
	vcd_property_hdr.sz = sizeof(struct vcd_property_h264_mv_buffer);
	vcd_h264_mv_buffer = &client_ctx->vcd_h264_mv_buffer;

	memset(&client_ctx->vcd_h264_mv_buffer, 0,
		   sizeof(struct vcd_property_h264_mv_buffer));
	vcd_h264_mv_buffer->size = mv_data->size;
	vcd_h264_mv_buffer->count = mv_data->count;
	vcd_h264_mv_buffer->pmem_fd = mv_data->pmem_fd;
	vcd_h264_mv_buffer->offset = mv_data->offset;

	if (!vcd_get_ion_status()) {
		if (get_pmem_file(vcd_h264_mv_buffer->pmem_fd,
			(unsigned long *) (&(vcd_h264_mv_buffer->
			physical_addr)),
			(unsigned long *) (&vcd_h264_mv_buffer->
						kernel_virtual_addr),
			(unsigned long *) (&len), &file)) {
			ERR("%s(): get_pmem_file failed\n", __func__);
			return false;
		}
		put_pmem_file(file);
		flags = MSM_SUBSYSTEM_MAP_IOVA;
		mapped_buffer = msm_subsystem_map_buffer(
			(unsigned long)vcd_h264_mv_buffer->physical_addr, len,
				flags, vidc_mmu_subsystem,
				sizeof(vidc_mmu_subsystem)/
				sizeof(unsigned int));
		if (IS_ERR(mapped_buffer)) {
			pr_err("buffer map failed");
			return false;
		}
		vcd_h264_mv_buffer->client_data = (void *) mapped_buffer;
		vcd_h264_mv_buffer->dev_addr = (u8 *)mapped_buffer->iova[0];
	} else {
		client_ctx->h264_mv_ion_handle = ion_import_fd(
					client_ctx->user_ion_client,
					vcd_h264_mv_buffer->pmem_fd);
		if (!client_ctx->h264_mv_ion_handle) {
			ERR("%s(): get_ION_handle failed\n", __func__);
			goto import_ion_error;
		}
		rc = ion_handle_get_flags(client_ctx->user_ion_client,
					client_ctx->h264_mv_ion_handle,
					&ionflag);
		if (rc) {
			ERR("%s():get_ION_flags fail\n",
					 __func__);
			goto import_ion_error;
		}
		vcd_h264_mv_buffer->kernel_virtual_addr = (u8 *) ion_map_kernel(
			client_ctx->user_ion_client,
			client_ctx->h264_mv_ion_handle,
			ionflag);
		if (!vcd_h264_mv_buffer->kernel_virtual_addr) {
			ERR("%s(): get_ION_kernel virtual addr failed\n",
				 __func__);
			goto import_ion_error;
		}
		rc = ion_map_iommu(client_ctx->user_ion_client,
				client_ctx->h264_mv_ion_handle,
				VIDEO_DOMAIN, VIDEO_MAIN_POOL,
				SZ_4K, 0, (unsigned long *)&iova,
				(unsigned long *)&buffer_size, UNCACHED, 0);
		if (rc) {
			ERR("%s():get_ION_kernel physical addr fail\n",
					 __func__);
			goto ion_map_error;
		}
		vcd_h264_mv_buffer->physical_addr = (u8 *) iova;
		vcd_h264_mv_buffer->client_data = NULL;
		vcd_h264_mv_buffer->dev_addr = (u8 *) iova;
	}
	DBG("Virt: %p, Phys %p, fd: %d", vcd_h264_mv_buffer->
		kernel_virtual_addr, vcd_h264_mv_buffer->physical_addr,
		vcd_h264_mv_buffer->pmem_fd);
	DBG("Dev addr %p", vcd_h264_mv_buffer->dev_addr);
	vcd_status = vcd_set_property(client_ctx->vcd_handle,
				      &vcd_property_hdr, vcd_h264_mv_buffer);

	if (vcd_status)
		return false;
	else
		return true;
ion_map_error:
	if (vcd_h264_mv_buffer->kernel_virtual_addr)
		ion_unmap_kernel(client_ctx->user_ion_client,
				client_ctx->h264_mv_ion_handle);
	if (client_ctx->h264_mv_ion_handle)
		ion_free(client_ctx->user_ion_client,
			client_ctx->h264_mv_ion_handle);
import_ion_error:
	return false;
}

static u32 vid_dec_set_cont_on_reconfig(struct video_client_ctx *client_ctx)
{
	struct vcd_property_hdr vcd_property_hdr;
	u32 vcd_status = VCD_ERR_FAIL;
	u32 enable = true;
	if (!client_ctx)
		return false;
	vcd_property_hdr.prop_id = VCD_I_CONT_ON_RECONFIG;
	vcd_property_hdr.sz = sizeof(u32);
	vcd_status = vcd_set_property(client_ctx->vcd_handle,
		&vcd_property_hdr, &enable);
	if (vcd_status)
		return false;
	return true;
}

static u32 vid_dec_get_h264_mv_buffer_size(struct video_client_ctx *client_ctx,
					struct vdec_mv_buff_size *mv_buff)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_buffer_size h264_mv_buffer_size;
	u32 vcd_status = VCD_ERR_FAIL;

	if (!client_ctx || !mv_buff)
		return false;

	vcd_property_hdr.prop_id = VCD_I_GET_H264_MV_SIZE;
	vcd_property_hdr.sz = sizeof(struct vcd_property_buffer_size);

	h264_mv_buffer_size.width = mv_buff->width;
	h264_mv_buffer_size.height = mv_buff->height;

	vcd_status = vcd_get_property(client_ctx->vcd_handle,
				      &vcd_property_hdr, &h264_mv_buffer_size);

	mv_buff->width = h264_mv_buffer_size.width;
	mv_buff->height = h264_mv_buffer_size.height;
	mv_buff->size = h264_mv_buffer_size.size;
	mv_buff->alignment = h264_mv_buffer_size.alignment;

	if (vcd_status)
		return false;
	else
		return true;
}

static u32 vid_dec_free_h264_mv_buffers(struct video_client_ctx *client_ctx)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_buffer_size h264_mv_buffer_size;
	u32 vcd_status = VCD_ERR_FAIL;

	if (!client_ctx)
		return false;
	if (client_ctx->vcd_h264_mv_buffer.client_data)
		msm_subsystem_unmap_buffer((struct msm_mapped_buffer *)
		client_ctx->vcd_h264_mv_buffer.client_data);

	vcd_property_hdr.prop_id = VCD_I_FREE_H264_MV_BUFFER;
	vcd_property_hdr.sz = sizeof(struct vcd_property_buffer_size);

	vcd_status = vcd_set_property(client_ctx->vcd_handle,
				      &vcd_property_hdr, &h264_mv_buffer_size);

	if (client_ctx->h264_mv_ion_handle != NULL) {
		ion_unmap_kernel(client_ctx->user_ion_client,
					client_ctx->h264_mv_ion_handle);
		ion_unmap_iommu(client_ctx->user_ion_client,
				client_ctx->h264_mv_ion_handle,
				VIDEO_DOMAIN,
				VIDEO_MAIN_POOL);
		ion_free(client_ctx->user_ion_client,
					client_ctx->h264_mv_ion_handle);
	}

	if (vcd_status)
		return false;
	else
		return true;
}

static u32 vid_dec_get_buffer_req(struct video_client_ctx *client_ctx,
				  struct vdec_allocatorproperty *vdec_buf_req)
{
	u32 vcd_status = VCD_ERR_FAIL;
	struct vcd_buffer_requirement vcd_buf_req;

	if (!client_ctx || !vdec_buf_req)
		return false;

	if (vdec_buf_req->buffer_type == VDEC_BUFFER_TYPE_INPUT) {
		vcd_status = vcd_get_buffer_requirements(client_ctx->vcd_handle,
							 VCD_BUFFER_INPUT,
							 &vcd_buf_req);
	} else {
		vcd_status = vcd_get_buffer_requirements(client_ctx->vcd_handle,
							 VCD_BUFFER_OUTPUT,
							 &vcd_buf_req);
	}

	if (vcd_status) {
		return false;
	} else {
		vdec_buf_req->mincount = vcd_buf_req.min_count;
		vdec_buf_req->maxcount = vcd_buf_req.max_count;
		vdec_buf_req->actualcount = vcd_buf_req.actual_count;
		vdec_buf_req->buffer_size = vcd_buf_req.sz;
		vdec_buf_req->alignment = vcd_buf_req.align;
		vdec_buf_req->buf_poolid = vcd_buf_req.buf_pool_id;

		return true;
	}
}

static u32 vid_dec_set_buffer(struct video_client_ctx *client_ctx,
			      struct vdec_setbuffer_cmd *buffer_info)
{
	enum vcd_buffer_type buffer = VCD_BUFFER_INPUT;
	enum buffer_dir dir_buffer = BUFFER_TYPE_INPUT;
	u32 vcd_status = VCD_ERR_FAIL;
	unsigned long kernel_vaddr, buf_adr_offset = 0, length;

	if (!client_ctx || !buffer_info)
		return false;

	if (buffer_info->buffer_type == VDEC_BUFFER_TYPE_OUTPUT) {
		dir_buffer = BUFFER_TYPE_OUTPUT;
		buffer = VCD_BUFFER_OUTPUT;
		buf_adr_offset = (unsigned long)buffer_info->buffer.offset;
	}
	length = buffer_info->buffer.buffer_len;
	/*If buffer cannot be set, ignore */
	if (!vidc_insert_addr_table(client_ctx, dir_buffer,
		(unsigned long)buffer_info->buffer.bufferaddr,
		&kernel_vaddr, buffer_info->buffer.pmem_fd,
		buf_adr_offset, MAX_VIDEO_NUM_OF_BUFF, length)) {
		DBG("%s() : user_virt_addr = %p cannot be set.",
		    __func__, buffer_info->buffer.bufferaddr);
		return false;
	}
	vcd_status = vcd_set_buffer(client_ctx->vcd_handle,
		buffer, (u8 *) kernel_vaddr,
		buffer_info->buffer.buffer_len);

	if (!vcd_status)
		return true;
	else
		return false;
}


static u32 vid_dec_free_buffer(struct video_client_ctx *client_ctx,
			      struct vdec_setbuffer_cmd *buffer_info)
{
	enum vcd_buffer_type buffer = VCD_BUFFER_INPUT;
	enum buffer_dir dir_buffer = BUFFER_TYPE_INPUT;
	u32 vcd_status = VCD_ERR_FAIL;
	unsigned long kernel_vaddr;

	if (!client_ctx || !buffer_info)
		return false;

	if (buffer_info->buffer_type == VDEC_BUFFER_TYPE_OUTPUT) {
		dir_buffer = BUFFER_TYPE_OUTPUT;
		buffer = VCD_BUFFER_OUTPUT;
	}

	/*If buffer NOT set, ignore */
	if (!vidc_delete_addr_table(client_ctx, dir_buffer,
				(unsigned long)buffer_info->buffer.bufferaddr,
				&kernel_vaddr)) {
		DBG("%s() : user_virt_addr = %p has not been set.",
		    __func__, buffer_info->buffer.bufferaddr);
		return true;
	}
	vcd_status = vcd_free_buffer(client_ctx->vcd_handle, buffer,
					 (u8 *)kernel_vaddr);

	if (!vcd_status)
		return true;
	else
		return false;
}

static u32 vid_dec_pause_resume(struct video_client_ctx *client_ctx, u32 pause)
{
  u32 vcd_status;

	if (!client_ctx) {
		ERR("\n %s(): Invalid client_ctx", __func__);
		return false;
	}

	if (pause) {
		DBG("msm_vidc_dec: PAUSE command from client = %p\n",
			 client_ctx);
		vcd_status = vcd_pause(client_ctx->vcd_handle);
	} else{
		DBG("msm_vidc_dec: RESUME command from client = %p\n",
			 client_ctx);
		vcd_status = vcd_resume(client_ctx->vcd_handle);
	}

	if (vcd_status)
		return false;

	return true;

}

static u32 vid_dec_start_stop(struct video_client_ctx *client_ctx, u32 start)
{
	struct vid_dec_msg *vdec_msg = NULL;
	u32 vcd_status;

	DBG("msm_vidc_dec: Inside %s()", __func__);
	if (!client_ctx) {
		ERR("\n Invalid client_ctx");
		return false;
	}

	if (start) {
		if (client_ctx->seq_header_set) {
			DBG("%s(): Seq Hdr set: Send START_DONE to client",
				 __func__);
			vdec_msg = kzalloc(sizeof(*vdec_msg), GFP_KERNEL);
			if (!vdec_msg) {
				ERR("vid_dec_start_stop: cannot allocate"
				    "buffer\n");
				return false;
			}
			vdec_msg->vdec_msg_info.msgcode =
			    VDEC_MSG_RESP_START_DONE;
			vdec_msg->vdec_msg_info.status_code = VDEC_S_SUCCESS;
			vdec_msg->vdec_msg_info.msgdatasize = 0;
			mutex_lock(&client_ctx->msg_queue_lock);
			list_add_tail(&vdec_msg->list, &client_ctx->msg_queue);
			mutex_unlock(&client_ctx->msg_queue_lock);

			wake_up(&client_ctx->msg_wait);

			DBG("Send START_DONE message to client = %p\n",
			    client_ctx);

		} else {
			DBG("%s(): Calling decode_start()", __func__);
			vcd_status =
			    vcd_decode_start(client_ctx->vcd_handle, NULL);

			if (vcd_status) {
				ERR("%s(): vcd_decode_start failed."
				    " vcd_status = %u\n", __func__, vcd_status);
				return false;
			}
		}
	} else {
		DBG("%s(): Calling vcd_stop()", __func__);
		mutex_lock(&vid_dec_device_p->lock);
		vcd_status = VCD_ERR_FAIL;
		if (!client_ctx->stop_called) {
			client_ctx->stop_called = true;
			vcd_status = vcd_stop(client_ctx->vcd_handle);
		}
		if (vcd_status) {
			ERR("%s(): vcd_stop failed.  vcd_status = %u\n",
				__func__, vcd_status);
			mutex_unlock(&vid_dec_device_p->lock);
			return false;
		}
		DBG("Send STOP_DONE message to client = %p\n", client_ctx);
		mutex_unlock(&vid_dec_device_p->lock);
	}
	return true;
}

static u32 vid_dec_decode_frame(struct video_client_ctx *client_ctx,
				struct vdec_input_frameinfo *input_frame_info,
				u8 *desc_buf, u32 desc_size)
{
	struct vcd_frame_data vcd_input_buffer;
	unsigned long kernel_vaddr, phy_addr, user_vaddr;
	int pmem_fd;
	struct file *file;
	s32 buffer_index = -1;
	u32 vcd_status = VCD_ERR_FAIL;
	u32 ion_flag = 0;
	struct ion_handle *buff_handle = NULL;

	if (!client_ctx || !input_frame_info)
		return false;

	user_vaddr = (unsigned long)input_frame_info->bufferaddr;

	if (vidc_lookup_addr_table(client_ctx, BUFFER_TYPE_INPUT,
				      true, &user_vaddr, &kernel_vaddr,
				      &phy_addr, &pmem_fd, &file,
				      &buffer_index)) {

		/* kernel_vaddr  is found. send the frame to VCD */
		memset((void *)&vcd_input_buffer, 0,
		       sizeof(struct vcd_frame_data));
		vcd_input_buffer.virtual =
		    (u8 *) (kernel_vaddr + input_frame_info->pmem_offset);
		vcd_input_buffer.offset = input_frame_info->offset;
		vcd_input_buffer.frm_clnt_data =
		    (u32) input_frame_info->client_data;
		vcd_input_buffer.ip_frm_tag =
		    (u32) input_frame_info->client_data;
		vcd_input_buffer.data_len = input_frame_info->datalen;
		vcd_input_buffer.time_stamp = input_frame_info->timestamp;
		/* Rely on VCD using the same flags as OMX */
		vcd_input_buffer.flags = input_frame_info->flags;
		vcd_input_buffer.desc_buf = desc_buf;
		vcd_input_buffer.desc_size = desc_size;
		if (vcd_input_buffer.data_len > 0) {
			ion_flag = vidc_get_fd_info(client_ctx,
						BUFFER_TYPE_INPUT,
						pmem_fd,
						kernel_vaddr,
						buffer_index,
						&buff_handle);
			if (ion_flag == CACHED) {
				msm_ion_do_cache_op(client_ctx->user_ion_client,
				buff_handle,
				(unsigned long *)kernel_vaddr,
				(unsigned long) vcd_input_buffer.data_len,
				ION_IOC_CLEAN_CACHES);
			}
		}
		vcd_status = vcd_decode_frame(client_ctx->vcd_handle,
					      &vcd_input_buffer);
		if (!vcd_status)
			return true;
		else {
			ERR("%s(): vcd_decode_frame failed = %u\n", __func__,
			    vcd_status);
			return false;
		}

	} else {
		ERR("%s(): kernel_vaddr not found\n", __func__);
		return false;
	}
}

static u32 vid_dec_fill_output_buffer(struct video_client_ctx *client_ctx,
		struct vdec_fillbuffer_cmd *fill_buffer_cmd)
{
	unsigned long kernel_vaddr, phy_addr, user_vaddr;
	int pmem_fd;
	struct file *file;
	s32 buffer_index = -1;
	u32 vcd_status = VCD_ERR_FAIL;
	struct ion_handle *buff_handle = NULL;

	struct vcd_frame_data vcd_frame;

	if (!client_ctx || !fill_buffer_cmd)
		return false;

	user_vaddr = (unsigned long)fill_buffer_cmd->buffer.bufferaddr;

	if (vidc_lookup_addr_table(client_ctx, BUFFER_TYPE_OUTPUT,
				      true, &user_vaddr, &kernel_vaddr,
				      &phy_addr, &pmem_fd, &file,
				      &buffer_index)) {

		memset((void *)&vcd_frame, 0,
		       sizeof(struct vcd_frame_data));
		vcd_frame.virtual = (u8 *) kernel_vaddr;
		vcd_frame.frm_clnt_data = (u32) fill_buffer_cmd->client_data;
		vcd_frame.alloc_len = fill_buffer_cmd->buffer.buffer_len;
		vcd_frame.ion_flag = vidc_get_fd_info(client_ctx,
						 BUFFER_TYPE_OUTPUT,
						pmem_fd, kernel_vaddr,
						buffer_index,
						&buff_handle);
		vcd_frame.buff_ion_handle = buff_handle;
		vcd_status = vcd_fill_output_buffer(client_ctx->vcd_handle,
						    &vcd_frame);
		if (!vcd_status)
			return true;
		else {
			ERR("%s(): vcd_fill_output_buffer failed = %u\n",
			    __func__, vcd_status);
			return false;
		}
	} else {
		ERR("%s(): kernel_vaddr not found\n", __func__);
		return false;
	}
}


static u32 vid_dec_flush(struct video_client_ctx *client_ctx,
			 enum vdec_bufferflush flush_dir)
{
	u32 vcd_status = VCD_ERR_FAIL;

	DBG("msm_vidc_dec: %s() called with dir = %u", __func__,
		 flush_dir);
	if (!client_ctx) {
		ERR("\n Invalid client_ctx");
		return false;
	}

	switch (flush_dir) {
	case VDEC_FLUSH_TYPE_INPUT:
		vcd_status = vcd_flush(client_ctx->vcd_handle, VCD_FLUSH_INPUT);
		break;
	case VDEC_FLUSH_TYPE_OUTPUT:
		vcd_status = vcd_flush(client_ctx->vcd_handle,
				       VCD_FLUSH_OUTPUT);
		break;
	case VDEC_FLUSH_TYPE_ALL:
		vcd_status = vcd_flush(client_ctx->vcd_handle, VCD_FLUSH_ALL);
		break;
	default:
		ERR("%s(): Inavlid flush cmd. flush_dir = %u\n", __func__,
		    flush_dir);
		return false;
		break;
	}

	if (!vcd_status)
		return true;
	else {
		ERR("%s(): vcd_flush failed. vcd_status = %u "
		    " flush_dir = %u\n", __func__, vcd_status, flush_dir);
		return false;
	}
}

static u32 vid_dec_msg_pending(struct video_client_ctx *client_ctx)
{
	u32 islist_empty = 0;
	mutex_lock(&client_ctx->msg_queue_lock);
	islist_empty = list_empty(&client_ctx->msg_queue);
	mutex_unlock(&client_ctx->msg_queue_lock);

	if (islist_empty) {
		DBG("%s(): vid_dec msg queue empty\n", __func__);
		if (client_ctx->stop_msg) {
			DBG("%s(): List empty and Stop Msg set\n",
				__func__);
			return client_ctx->stop_msg;
		}
	} else
		DBG("%s(): vid_dec msg queue Not empty\n", __func__);

	return !islist_empty;
}

static int vid_dec_get_next_msg(struct video_client_ctx *client_ctx,
				struct vdec_msginfo *vdec_msg_info)
{
	int rc;
	struct vid_dec_msg *vid_dec_msg = NULL;

	if (!client_ctx)
		return false;

	rc = wait_event_interruptible(client_ctx->msg_wait,
				      vid_dec_msg_pending(client_ctx));
	if (rc < 0) {
		DBG("rc = %d, stop_msg = %u\n", rc, client_ctx->stop_msg);
		return rc;
	} else if (client_ctx->stop_msg) {
		DBG("rc = %d, stop_msg = %u\n", rc, client_ctx->stop_msg);
		return -EIO;
	}

	mutex_lock(&client_ctx->msg_queue_lock);
	if (!list_empty(&client_ctx->msg_queue)) {
		DBG("%s(): After Wait\n", __func__);
		vid_dec_msg = list_first_entry(&client_ctx->msg_queue,
					       struct vid_dec_msg, list);
		list_del(&vid_dec_msg->list);
		memcpy(vdec_msg_info, &vid_dec_msg->vdec_msg_info,
		       sizeof(struct vdec_msginfo));
		kfree(vid_dec_msg);
	}
	mutex_unlock(&client_ctx->msg_queue_lock);
	return 0;
}

static long vid_dec_ioctl(struct file *file,
			 unsigned cmd, unsigned long u_arg)
{
	struct video_client_ctx *client_ctx = NULL;
	struct vdec_ioctl_msg vdec_msg;
	u32 vcd_status;
	unsigned long kernel_vaddr, phy_addr, len;
	unsigned long ker_vaddr;
	struct file *pmem_file;
	u32 result = true;
	void __user *arg = (void __user *)u_arg;
	int rc = 0;
	size_t ion_len;

	DBG("%s\n", __func__);
	if (_IOC_TYPE(cmd) != VDEC_IOCTL_MAGIC)
		return -ENOTTY;

	client_ctx = (struct video_client_ctx *)file->private_data;
	if (!client_ctx) {
		ERR("!client_ctx. Cannot attach to device handle\n");
		return -ENODEV;
	}

	switch (cmd) {
	case VDEC_IOCTL_SET_CODEC:
	{
		enum vdec_codec vdec_codec;
		DBG("VDEC_IOCTL_SET_CODEC\n");
		if (copy_from_user(&vdec_msg, arg, sizeof(vdec_msg)))
			return -EFAULT;
		if (copy_from_user(&vdec_codec,	vdec_msg.in,
						   sizeof(vdec_codec)))
			return -EFAULT;
		DBG("setting code type = %u\n", vdec_codec);
		result = vid_dec_set_codec(client_ctx, &vdec_codec);
		if (!result)
			return -EIO;
		break;
	}
	case VDEC_IOCTL_SET_OUTPUT_FORMAT:
	{
		enum vdec_output_fromat output_format;
		DBG("VDEC_IOCTL_SET_OUTPUT_FORMAT\n");
		if (copy_from_user(&vdec_msg, arg, sizeof(vdec_msg)))
			return -EFAULT;
		if (copy_from_user(&output_format, vdec_msg.in,
						   sizeof(output_format)))
			return -EFAULT;

		result = vid_dec_set_output_format(client_ctx, &output_format);

		if (!result)
			return -EIO;
		break;
	}
	case VDEC_IOCTL_SET_PICRES:
	{
		struct vdec_picsize video_resoultion;
		DBG("VDEC_IOCTL_SET_PICRES\n");
		if (copy_from_user(&vdec_msg, arg, sizeof(vdec_msg)))
			return -EFAULT;
		if (copy_from_user(&video_resoultion, vdec_msg.in,
						   sizeof(video_resoultion)))
			return -EFAULT;
		result =
		vid_dec_set_frame_resolution(client_ctx, &video_resoultion);
		if (!result)
			return -EIO;
		break;
	}
	case VDEC_IOCTL_GET_PICRES:
	{
		struct vdec_picsize video_resoultion;
		DBG("VDEC_IOCTL_GET_PICRES\n");
		if (copy_from_user(&vdec_msg, arg, sizeof(vdec_msg)))
			return -EFAULT;
		if (copy_from_user(&video_resoultion, vdec_msg.out,
						   sizeof(video_resoultion)))
			return -EFAULT;

		result = vid_dec_get_frame_resolution(client_ctx,
					&video_resoultion);

		if (result) {
			if (copy_to_user(vdec_msg.out, &video_resoultion,
					sizeof(video_resoultion)))
				return -EFAULT;
		} else
			return -EIO;
		break;
	}
	case VDEC_IOCTL_SET_BUFFER_REQ:
	{
		struct vdec_allocatorproperty vdec_buf_req;
		struct vcd_buffer_requirement buffer_req;
		DBG("VDEC_IOCTL_SET_BUFFER_REQ\n");
		if (copy_from_user(&vdec_msg, arg, sizeof(vdec_msg)))
			return -EFAULT;

		if (copy_from_user(&vdec_buf_req, vdec_msg.in,
				   sizeof(vdec_buf_req)))
			return -EFAULT;

		buffer_req.actual_count = vdec_buf_req.actualcount;
		buffer_req.align = vdec_buf_req.alignment;
		buffer_req.max_count = vdec_buf_req.maxcount;
		buffer_req.min_count = vdec_buf_req.mincount;
		buffer_req.sz = vdec_buf_req.buffer_size;

		switch (vdec_buf_req.buffer_type) {
		case VDEC_BUFFER_TYPE_INPUT:
			vcd_status =
			vcd_set_buffer_requirements(client_ctx->vcd_handle,
				VCD_BUFFER_INPUT, &buffer_req);
			break;
		case VDEC_BUFFER_TYPE_OUTPUT:
			vcd_status =
			vcd_set_buffer_requirements(client_ctx->vcd_handle,
				VCD_BUFFER_OUTPUT, &buffer_req);
			break;
		default:
			vcd_status = VCD_ERR_BAD_POINTER;
			break;
		}

		if (vcd_status)
			return -EFAULT;
		break;
	}
	case VDEC_IOCTL_GET_BUFFER_REQ:
	{
		struct vdec_allocatorproperty vdec_buf_req;
		DBG("VDEC_IOCTL_GET_BUFFER_REQ\n");
		if (copy_from_user(&vdec_msg, arg, sizeof(vdec_msg)))
			return -EFAULT;
		if (copy_from_user(&vdec_buf_req, vdec_msg.out,
				   sizeof(vdec_buf_req)))
			return -EFAULT;

		result = vid_dec_get_buffer_req(client_ctx, &vdec_buf_req);

		if (result) {
			if (copy_to_user(vdec_msg.out, &vdec_buf_req,
					sizeof(vdec_buf_req)))
				return -EFAULT;
		} else
			return -EIO;
		break;
	}
	case VDEC_IOCTL_SET_BUFFER:
	{
		struct vdec_setbuffer_cmd setbuffer;
		DBG("VDEC_IOCTL_SET_BUFFER\n");
		if (copy_from_user(&vdec_msg, arg, sizeof(vdec_msg)))
			return -EFAULT;
		if (copy_from_user(&setbuffer, vdec_msg.in,
				sizeof(setbuffer)))
			return -EFAULT;
		result = vid_dec_set_buffer(client_ctx, &setbuffer);
		if (!result)
			return -EIO;
		break;
	}
	case VDEC_IOCTL_FREE_BUFFER:
	{
		struct vdec_setbuffer_cmd setbuffer;
		DBG("VDEC_IOCTL_FREE_BUFFER\n");
		if (copy_from_user(&vdec_msg, arg, sizeof(vdec_msg)))
			return -EFAULT;
		if (copy_from_user(&setbuffer, vdec_msg.in,
				sizeof(setbuffer)))
			return -EFAULT;
		result = vid_dec_free_buffer(client_ctx, &setbuffer);
		if (!result)
			return -EIO;
		break;
	}
	case VDEC_IOCTL_CMD_START:
	{
		DBG(" VDEC_IOCTL_CMD_START\n");
		result = vid_dec_start_stop(client_ctx, true);
		if (!result)
			return -EIO;
		break;
	}
	case VDEC_IOCTL_CMD_STOP:
	{
		DBG("VDEC_IOCTL_CMD_STOP\n");
		result = vid_dec_start_stop(client_ctx, false);
		if (!result)
			return -EIO;
		break;
	}
	case VDEC_IOCTL_CMD_PAUSE:
	{
		result = vid_dec_pause_resume(client_ctx, true);
		if (!result)
			return -EIO;
		break;
	}
	case VDEC_IOCTL_CMD_RESUME:
	{
		DBG("VDEC_IOCTL_CMD_PAUSE\n");
		result = vid_dec_pause_resume(client_ctx, false);

		if (!result)
			return -EIO;
		break;
	}
	case VDEC_IOCTL_DECODE_FRAME:
	{
		struct vdec_input_frameinfo input_frame_info;
		u8 *desc_buf = NULL;
		u32 desc_size = 0;
		DBG("VDEC_IOCTL_DECODE_FRAME\n");
		if (copy_from_user(&vdec_msg, arg, sizeof(vdec_msg)))
			return -EFAULT;
		if (copy_from_user(&input_frame_info, vdec_msg.in,
				   sizeof(input_frame_info)))
			return -EFAULT;
		if (client_ctx->dmx_disable) {
			if (input_frame_info.desc_addr) {
				desc_size = input_frame_info.desc_size;
				desc_buf = kzalloc(desc_size, GFP_KERNEL);
				if (desc_buf) {
					if (copy_from_user(desc_buf,
						input_frame_info.desc_addr,
							desc_size)) {
						kfree(desc_buf);
						desc_buf = NULL;
						return -EFAULT;
					}
				}
			} else
				return -EINVAL;
		}
		result = vid_dec_decode_frame(client_ctx, &input_frame_info,
					desc_buf, desc_size);

		if (!result) {
			kfree(desc_buf);
			desc_buf = NULL;
			return -EIO;
		}
		break;
	}
	case VDEC_IOCTL_FILL_OUTPUT_BUFFER:
	{
		struct vdec_fillbuffer_cmd fill_buffer_cmd;
		DBG("VDEC_IOCTL_FILL_OUTPUT_BUFFER\n");
		if (copy_from_user(&vdec_msg, arg, sizeof(vdec_msg)))
			return -EFAULT;
		if (copy_from_user(&fill_buffer_cmd, vdec_msg.in,
				   sizeof(fill_buffer_cmd)))
			return -EFAULT;
		result = vid_dec_fill_output_buffer(client_ctx,
							&fill_buffer_cmd);
		if (!result)
			return -EIO;
		break;
	}
	case VDEC_IOCTL_CMD_FLUSH:
	{
		enum vdec_bufferflush flush_dir;
		DBG("VDEC_IOCTL_CMD_FLUSH\n");
		if (copy_from_user(&vdec_msg, arg, sizeof(vdec_msg)))
			return -EFAULT;
		if (copy_from_user(&flush_dir, vdec_msg.in,
				   sizeof(flush_dir)))
			return -EFAULT;
		result = vid_dec_flush(client_ctx, flush_dir);
		if (!result)
			return -EIO;
		break;
	}
	case VDEC_IOCTL_GET_NEXT_MSG:
	{
		struct vdec_msginfo vdec_msg_info;
		DBG("VDEC_IOCTL_GET_NEXT_MSG\n");
		if (copy_from_user(&vdec_msg, arg, sizeof(vdec_msg)))
			return -EFAULT;
		result = vid_dec_get_next_msg(client_ctx, &vdec_msg_info);
		if (result)
			return result;
		if (copy_to_user(vdec_msg.out, &vdec_msg_info,
					sizeof(vdec_msg_info)))
			return -EFAULT;
		break;
	}
	case VDEC_IOCTL_STOP_NEXT_MSG:
	{
		DBG("VDEC_IOCTL_STOP_NEXT_MSG\n");
		client_ctx->stop_msg = 1;
		wake_up(&client_ctx->msg_wait);
		break;
	}
	case VDEC_IOCTL_SET_SEQUENCE_HEADER:
	{
		struct vdec_seqheader seq_header;
		struct vcd_sequence_hdr vcd_seq_hdr;
		unsigned long ionflag;
		DBG("VDEC_IOCTL_SET_SEQUENCE_HEADER\n");
		if (copy_from_user(&vdec_msg, arg, sizeof(vdec_msg))) {
			ERR("Copy from user vdec_msg failed\n");
			return -EFAULT;
		}
		if (copy_from_user(&seq_header,	vdec_msg.in,
				   sizeof(seq_header))) {
			ERR("Copy from user seq_header failed\n");
			return -EFAULT;
		}
		if (!seq_header.seq_header_len) {
			ERR("Seq Len is Zero\n");
			return -EFAULT;
		}

		if (!vcd_get_ion_status()) {
			if (get_pmem_file(seq_header.pmem_fd,
				  &phy_addr, &kernel_vaddr, &len, &pmem_file)) {
				ERR("%s(): get_pmem_file failed\n", __func__);
				return false;
			}
			put_pmem_file(pmem_file);
		} else {
			client_ctx->seq_hdr_ion_handle = ion_import_fd(
				client_ctx->user_ion_client,
				seq_header.pmem_fd);
			if (!client_ctx->seq_hdr_ion_handle) {
				ERR("%s(): get_ION_handle failed\n", __func__);
				return false;
			}
			rc = ion_handle_get_flags(client_ctx->user_ion_client,
						client_ctx->seq_hdr_ion_handle,
						&ionflag);
			if (rc) {
				ERR("%s():get_ION_flags fail\n",
							 __func__);
				ion_free(client_ctx->user_ion_client,
					client_ctx->seq_hdr_ion_handle);
				return false;
			}
			ker_vaddr = (unsigned long) ion_map_kernel(
				client_ctx->user_ion_client,
				client_ctx->seq_hdr_ion_handle, ionflag);
			if (!ker_vaddr) {
				ERR("%s():get_ION_kernel virtual addr fail\n",
							 __func__);
				ion_free(client_ctx->user_ion_client,
					client_ctx->seq_hdr_ion_handle);
				return false;
			}
			kernel_vaddr = ker_vaddr;
			rc = ion_phys(client_ctx->user_ion_client,
					client_ctx->seq_hdr_ion_handle,
					&phy_addr, &ion_len);
			if (rc) {
				ERR("%s():get_ION_kernel physical addr fail\n",
						 __func__);
				ion_unmap_kernel(client_ctx->user_ion_client,
					client_ctx->seq_hdr_ion_handle);
				ion_free(client_ctx->user_ion_client,
					client_ctx->seq_hdr_ion_handle);
				return false;
			}
			len = ion_len;
		}
		vcd_seq_hdr.sequence_header_len = seq_header.seq_header_len;
		kernel_vaddr += (unsigned long)seq_header.pmem_offset;
		vcd_seq_hdr.sequence_header = (u8 *)kernel_vaddr;
		if (!vcd_seq_hdr.sequence_header) {
			ERR("Sequence Header pointer failed\n");
			return -EFAULT;
		}
		client_ctx->seq_header_set = true;
		if (vcd_decode_start(client_ctx->vcd_handle, &vcd_seq_hdr)) {
			ERR("Decode start Failed\n");
			client_ctx->seq_header_set = false;
			return -EFAULT;
		}
		DBG("Wait Client completion Sequence Header\n");
		wait_for_completion(&client_ctx->event);
		vcd_seq_hdr.sequence_header = NULL;
		if (client_ctx->event_status) {
			ERR("Set Seq Header status is failed");
			return -EFAULT;
		}
		if (vcd_get_ion_status()) {
			if (client_ctx->seq_hdr_ion_handle) {
				ion_unmap_kernel(client_ctx->user_ion_client,
						client_ctx->seq_hdr_ion_handle);
				ion_free(client_ctx->user_ion_client,
					client_ctx->seq_hdr_ion_handle);
			}
		}
		break;
	}
	case VDEC_IOCTL_GET_NUMBER_INSTANCES:
	{
		DBG("VDEC_IOCTL_GET_NUMBER_INSTANCES\n");
		if (copy_from_user(&vdec_msg, arg, sizeof(vdec_msg)))
			return -EFAULT;
		if (copy_to_user(vdec_msg.out,
			&vid_dec_device_p->num_clients, sizeof(u32)))
			return -EFAULT;
		break;
	}
	case VDEC_IOCTL_GET_INTERLACE_FORMAT:
	{
		u32 progressive_only, interlace_format;
		DBG("VDEC_IOCTL_GET_INTERLACE_FORMAT\n");
		if (copy_from_user(&vdec_msg, arg, sizeof(vdec_msg)))
			return -EFAULT;
		result = vid_dec_get_progressive_only(client_ctx,
					&progressive_only);
		if (result) {
			interlace_format = progressive_only ?
				VDEC_InterlaceFrameProgressive :
				VDEC_InterlaceInterleaveFrameTopFieldFirst;
			if (copy_to_user(vdec_msg.out, &interlace_format,
					sizeof(u32)))
				return -EFAULT;
		} else
			return -EIO;
		break;
	}

	case VDEC_IOCTL_GET_DISABLE_DMX_SUPPORT:
	{
		u32 disable_dmx;
		DBG("VDEC_IOCTL_GET_DISABLE_DMX_SUPPORT\n");
		if (copy_from_user(&vdec_msg, arg, sizeof(vdec_msg)))
			return -EFAULT;
		result = vid_dec_get_disable_dmx_support(client_ctx,
					&disable_dmx);
		if (result) {
			if (copy_to_user(vdec_msg.out, &disable_dmx,
					sizeof(u32)))
				return -EFAULT;
		} else
			return -EIO;
		break;
	}
	case VDEC_IOCTL_GET_DISABLE_DMX:
	{
		u32 disable_dmx;
		DBG("VDEC_IOCTL_GET_DISABLE_DMX\n");
		if (copy_from_user(&vdec_msg, arg, sizeof(vdec_msg)))
			return -EFAULT;
		result = vid_dec_get_disable_dmx(client_ctx,
					&disable_dmx);
		if (result) {
			if (copy_to_user(vdec_msg.out, &disable_dmx,
					sizeof(u32)))
				return -EFAULT;
		} else
			return -EIO;
		break;
	}
	case VDEC_IOCTL_SET_DISABLE_DMX:
	{
		DBG("VDEC_IOCTL_SET_DISABLE_DMX\n");
		result =  vid_dec_set_disable_dmx(client_ctx);
		if (!result)
			return -EIO;
		client_ctx->dmx_disable = 1;
		break;
	}
	case VDEC_IOCTL_SET_PICTURE_ORDER:
	{
		u32 picture_order;
		DBG("VDEC_IOCTL_SET_PICTURE_ORDER\n");
		if (copy_from_user(&vdec_msg, arg, sizeof(vdec_msg)))
			return -EFAULT;
		if (copy_from_user(&picture_order, vdec_msg.in,
						   sizeof(u32)))
			return -EFAULT;
		result =  vid_dec_set_picture_order(client_ctx, &picture_order);
		if (!result)
			return -EIO;
		break;
	}
	case VDEC_IOCTL_SET_FRAME_RATE:
	{
		struct vdec_framerate frame_rate;
		DBG("VDEC_IOCTL_SET_FRAME_RATE\n");
		if (copy_from_user(&vdec_msg, arg, sizeof(vdec_msg)))
			return -EFAULT;
		if (copy_from_user(&frame_rate, vdec_msg.in,
						   sizeof(frame_rate)))
			return -EFAULT;
		result = vid_dec_set_frame_rate(client_ctx, &frame_rate);
		if (!result)
			return -EIO;
		break;
	}
	case VDEC_IOCTL_SET_EXTRADATA:
	{
		u32 extradata_flag;
		DBG("VDEC_IOCTL_SET_EXTRADATA\n");
		if (copy_from_user(&vdec_msg, arg, sizeof(vdec_msg)))
			return -EFAULT;
		if (copy_from_user(&extradata_flag, vdec_msg.in,
						   sizeof(u32)))
			return -EFAULT;
		result = vid_dec_set_extradata(client_ctx, &extradata_flag);
		if (!result)
			return -EIO;
		break;
	}
	case VDEC_IOCTL_SET_H264_MV_BUFFER:
	{
		struct vdec_h264_mv mv_data;
		DBG("VDEC_IOCTL_SET_H264_MV_BUFFER\n");
		if (copy_from_user(&vdec_msg, arg, sizeof(vdec_msg)))
			return -EFAULT;
		if (copy_from_user(&mv_data, vdec_msg.in,
						   sizeof(mv_data)))
			return -EFAULT;
		result = vid_dec_set_h264_mv_buffers(client_ctx, &mv_data);

		if (!result)
			return -EIO;
		break;
	}
	case VDEC_IOCTL_FREE_H264_MV_BUFFER:
	{
		DBG("VDEC_IOCTL_FREE_H264_MV_BUFFER\n");
		result = vid_dec_free_h264_mv_buffers(client_ctx);
		if (!result)
			return -EIO;
		break;
	}
	case VDEC_IOCTL_GET_MV_BUFFER_SIZE:
	{
		struct vdec_mv_buff_size mv_buff;
		DBG("VDEC_IOCTL_GET_MV_BUFFER_SIZE\n");
		if (copy_from_user(&vdec_msg, arg, sizeof(vdec_msg)))
			return -EFAULT;
		if (copy_from_user(&mv_buff, vdec_msg.out,
						   sizeof(mv_buff)))
			return -EFAULT;
		result = vid_dec_get_h264_mv_buffer_size(client_ctx, &mv_buff);
		if (result) {
			DBG(" Returning W: %d, H: %d, S: %d, A: %d",
				mv_buff.width, mv_buff.height,
				mv_buff.size, mv_buff.alignment);
			if (copy_to_user(vdec_msg.out, &mv_buff,
					sizeof(mv_buff)))
				return -EFAULT;
		} else
			return -EIO;
		break;
	}
	case VDEC_IOCTL_SET_IDR_ONLY_DECODING:
	{
		result = vid_dec_set_idr_only_decoding(client_ctx);
		if (!result)
			return -EIO;
		break;
	}
	case VDEC_IOCTL_SET_CONT_ON_RECONFIG:
	{
		result = vid_dec_set_cont_on_reconfig(client_ctx);
		if (!result)
			return -EIO;
		break;
	}
	default:
		ERR("%s(): Unsupported ioctl\n", __func__);
		return -ENOTTY;
		break;
	}

	return 0;
}

static u32 vid_dec_close_client(struct video_client_ctx *client_ctx)
{
	struct vid_dec_msg *vdec_msg;
	u32 vcd_status;

	DBG("msm_vidc_dec: Inside %s()", __func__);
	if (!client_ctx || (!client_ctx->vcd_handle)) {
		ERR("\n Invalid client_ctx");
		return false;
	}

	mutex_lock(&vid_dec_device_p->lock);
	if (!client_ctx->stop_called) {
		client_ctx->stop_called = true;
		client_ctx->stop_sync_cb = true;
		vcd_status = vcd_stop(client_ctx->vcd_handle);
		DBG("\n Stuck at the stop call");
		if (!vcd_status)
			wait_for_completion(&client_ctx->event);
		DBG("\n Came out of wait event");
	}
	mutex_lock(&client_ctx->msg_queue_lock);
	while (!list_empty(&client_ctx->msg_queue)) {
		DBG("%s(): Delete remaining entries\n", __func__);
		vdec_msg = list_first_entry(&client_ctx->msg_queue,
						   struct vid_dec_msg, list);
		if (vdec_msg) {
			list_del(&vdec_msg->list);
			kfree(vdec_msg);
		}
	}
	mutex_unlock(&client_ctx->msg_queue_lock);
	vcd_status = vcd_close(client_ctx->vcd_handle);

	if (vcd_status) {
		mutex_unlock(&vid_dec_device_p->lock);
		return false;
	}
	client_ctx->user_ion_client = NULL;
	memset((void *)client_ctx, 0, sizeof(struct video_client_ctx));
	vid_dec_device_p->num_clients--;
	mutex_unlock(&vid_dec_device_p->lock);
	return true;
}

int vid_dec_open_client(struct video_client_ctx **vid_clnt_ctx, int flags)
{
	int rc = 0;
	s32 client_index;
	struct video_client_ctx *client_ctx = NULL;
	u8 client_count;

	if (!vid_clnt_ctx) {
		ERR("Invalid input\n");
		return -EINVAL;
	}
	*vid_clnt_ctx = NULL;
	client_count = vcd_get_num_of_clients();
	if (client_count == VIDC_MAX_NUM_CLIENTS) {
		ERR("ERROR : vid_dec_open() max number of clients"
			"limit reached\n");
		rc = -ENOMEM;
		goto client_failure;
	}

	DBG(" Virtual Address of ioremap is %p\n", vid_dec_device_p->virt_base);
	if (!vid_dec_device_p->num_clients) {
		if (!vidc_load_firmware()) {
			rc = -ENOMEM;
			goto client_failure;
		}
	}

	client_index = vid_dec_get_empty_client_index();
	if (client_index == -1) {
		ERR("%s() : No free clients client_index == -1\n", __func__);
		rc = -ENOMEM;
		goto client_failure;
	}
	client_ctx = &vid_dec_device_p->vdec_clients[client_index];
	vid_dec_device_p->num_clients++;
	init_completion(&client_ctx->event);
	mutex_init(&client_ctx->msg_queue_lock);
	mutex_init(&client_ctx->enrty_queue_lock);
	INIT_LIST_HEAD(&client_ctx->msg_queue);
	init_waitqueue_head(&client_ctx->msg_wait);
	client_ctx->stop_msg = 0;
	client_ctx->stop_called = false;
	client_ctx->stop_sync_cb = false;
	client_ctx->dmx_disable = 0;
	if (vcd_get_ion_status()) {
		client_ctx->user_ion_client = vcd_get_ion_client();
		if (!client_ctx->user_ion_client) {
			ERR("vcd_open ion client get failed");
			rc = -ENOMEM;
			goto client_failure;
		}
	}
	rc = vcd_open(vid_dec_device_p->device_handle, true,
				  vid_dec_vcd_cb, client_ctx, flags);
	if (!rc) {
		wait_for_completion(&client_ctx->event);
		if (client_ctx->event_status) {
			ERR("callback for vcd_open returned error: %u",
				client_ctx->event_status);
			rc = -ENODEV;
			goto client_failure;
		}
	} else {
		ERR("vcd_open returned error: %u", rc);
		goto client_failure;
	}
	client_ctx->seq_header_set = false;
	*vid_clnt_ctx = client_ctx;
client_failure:
	return rc;
}

static int vid_dec_open_secure(struct inode *inode, struct file *file)
{
	int rc = 0;
	struct video_client_ctx *client_ctx;
	mutex_lock(&vid_dec_device_p->lock);
	rc = vid_dec_open_client(&client_ctx, VCD_CP_SESSION);
	if (rc)
		goto error;
	if (!client_ctx) {
		rc = -ENOMEM;
		goto error;
	}

	file->private_data = client_ctx;
	if (res_trk_open_secure_session()) {
		ERR("Secure session operation failure\n");
		rc = -EACCES;
		goto error;
	}
	mutex_unlock(&vid_dec_device_p->lock);
	return 0;
error:
	mutex_unlock(&vid_dec_device_p->lock);
	return rc;
}

static int vid_dec_open(struct inode *inode, struct file *file)
{
	int rc = 0;
	struct video_client_ctx *client_ctx;
	INFO("msm_vidc_dec: Inside %s()", __func__);
	mutex_lock(&vid_dec_device_p->lock);
	rc = vid_dec_open_client(&client_ctx, 0);
	if (rc) {
		mutex_unlock(&vid_dec_device_p->lock);
		return rc;
	}
	if (!client_ctx) {
		mutex_unlock(&vid_dec_device_p->lock);
		return -ENOMEM;
	}

	file->private_data = client_ctx;
	mutex_unlock(&vid_dec_device_p->lock);
	return rc;
}

static int vid_dec_release_secure(struct inode *inode, struct file *file)
{
	struct video_client_ctx *client_ctx = file->private_data;

	INFO("msm_vidc_dec: Inside %s()", __func__);
	vidc_cleanup_addr_table(client_ctx, BUFFER_TYPE_OUTPUT);
	vidc_cleanup_addr_table(client_ctx, BUFFER_TYPE_INPUT);
	vid_dec_close_client(client_ctx);
	vidc_release_firmware();
#ifndef USE_RES_TRACKER
	vidc_disable_clk();
#endif
	INFO("msm_vidc_dec: Return from %s()", __func__);
	return 0;
}

static int vid_dec_release(struct inode *inode, struct file *file)
{
	struct video_client_ctx *client_ctx = file->private_data;

	INFO("msm_vidc_dec: Inside %s()", __func__);
	vidc_cleanup_addr_table(client_ctx, BUFFER_TYPE_OUTPUT);
	vidc_cleanup_addr_table(client_ctx, BUFFER_TYPE_INPUT);
	vid_dec_close_client(client_ctx);
	vidc_release_firmware();
#ifndef USE_RES_TRACKER
	vidc_disable_clk();
#endif
	INFO("msm_vidc_dec: Return from %s()", __func__);
	return 0;
}

static const struct file_operations vid_dec_fops[2] = {
	{
		.owner = THIS_MODULE,
		.open = vid_dec_open,
		.release = vid_dec_release,
		.unlocked_ioctl = vid_dec_ioctl,
	},
	{
		.owner = THIS_MODULE,
		.open = vid_dec_open_secure,
		.release = vid_dec_release_secure,
		.unlocked_ioctl = vid_dec_ioctl,
	},

};

void vid_dec_interrupt_deregister(void)
{
}

void vid_dec_interrupt_register(void *device_name)
{
}

void vid_dec_interrupt_clear(void)
{
}

void *vid_dec_map_dev_base_addr(void *device_name)
{
	return vid_dec_device_p->virt_base;
}

static int vid_dec_vcd_init(void)
{
	int rc;
	struct vcd_init_config vcd_init_config;
	u32 i;

	/* init_timer(&hw_timer); */
	DBG("msm_vidc_dec: Inside %s()", __func__);
	vid_dec_device_p->num_clients = 0;

	for (i = 0; i < VIDC_MAX_NUM_CLIENTS; i++) {
		memset((void *)&vid_dec_device_p->vdec_clients[i], 0,
		       sizeof(vid_dec_device_p->vdec_clients[i]));
	}

	mutex_init(&vid_dec_device_p->lock);
	vid_dec_device_p->virt_base = vidc_get_ioaddr();
	DBG("%s() : base address for VIDC core %u\n", __func__, \
		(int)vid_dec_device_p->virt_base);

	if (!vid_dec_device_p->virt_base) {
		ERR("%s() : ioremap failed\n", __func__);
		return -ENOMEM;
	}

	vcd_init_config.device_name = "VIDC";
	vcd_init_config.map_dev_base_addr = vid_dec_map_dev_base_addr;
	vcd_init_config.interrupt_clr = vid_dec_interrupt_clear;
	vcd_init_config.register_isr = vid_dec_interrupt_register;
	vcd_init_config.deregister_isr = vid_dec_interrupt_deregister;
	vcd_init_config.timer_create = vidc_timer_create;
	vcd_init_config.timer_release = vidc_timer_release;
	vcd_init_config.timer_start = vidc_timer_start;
	vcd_init_config.timer_stop = vidc_timer_stop;

	rc = vcd_init(&vcd_init_config, &vid_dec_device_p->device_handle);

	if (rc) {
		ERR("%s() : vcd_init failed\n", __func__);
		return -ENODEV;
	}
	return 0;
}

static int __init vid_dec_init(void)
{
	int rc = 0, i = 0, j = 0;
	struct device *class_devp;

	DBG("msm_vidc_dec: Inside %s()", __func__);
	vid_dec_device_p = kzalloc(sizeof(struct vid_dec_dev), GFP_KERNEL);
	if (!vid_dec_device_p) {
		ERR("%s Unable to allocate memory for vid_dec_dev\n",
		       __func__);
		return -ENOMEM;
	}

	rc = alloc_chrdev_region(&vid_dec_dev_num, 0, NUM_OF_DRIVER_NODES,
		VID_DEC_NAME);
	if (rc < 0) {
		ERR("%s: alloc_chrdev_region Failed rc = %d\n",
		       __func__, rc);
		goto error_vid_dec_alloc_chrdev_region;
	}

	vid_dec_class = class_create(THIS_MODULE, VID_DEC_NAME);
	if (IS_ERR(vid_dec_class)) {
		rc = PTR_ERR(vid_dec_class);
		ERR("%s: couldn't create vid_dec_class rc = %d\n",
		       __func__, rc);

		goto error_vid_dec_class_create;
	}
	for (i = 0; i < NUM_OF_DRIVER_NODES; i++) {
		class_devp = device_create(vid_dec_class, NULL,
						(vid_dec_dev_num + i),
						NULL, VID_DEC_NAME "%s",
						node_name[i]);

		if (IS_ERR(class_devp)) {
			rc = PTR_ERR(class_devp);
			ERR("%s: class device_create failed %d\n",
				   __func__, rc);
			if (!i)
				goto error_vid_dec_class_device_create;
			else
				goto error_vid_dec_cdev_add;
		}

	  vid_dec_device_p->device[i] = class_devp;

		cdev_init(&vid_dec_device_p->cdev[i], &vid_dec_fops[i]);
		vid_dec_device_p->cdev[i].owner = THIS_MODULE;
		rc = cdev_add(&(vid_dec_device_p->cdev[i]),
				(vid_dec_dev_num+i), 1);

		if (rc < 0) {
			ERR("%s: cdev_add failed %d\n", __func__, rc);
			goto error_vid_dec_cdev_add;
		}
	}
	vid_dec_vcd_init();
	return 0;

error_vid_dec_cdev_add:
	for (j = i-1; j >= 0; j--)
		cdev_del(&(vid_dec_device_p->cdev[j]));
	device_destroy(vid_dec_class, vid_dec_dev_num);
error_vid_dec_class_device_create:
	class_destroy(vid_dec_class);
error_vid_dec_class_create:
	unregister_chrdev_region(vid_dec_dev_num, NUM_OF_DRIVER_NODES);
error_vid_dec_alloc_chrdev_region:
	kfree(vid_dec_device_p);
	return rc;
}

static void __exit vid_dec_exit(void)
{
	int i = 0;
	INFO("msm_vidc_dec: Inside %s()", __func__);
	for (i = 0; i < NUM_OF_DRIVER_NODES; i++)
		cdev_del(&(vid_dec_device_p->cdev[i]));
	device_destroy(vid_dec_class, vid_dec_dev_num);
	class_destroy(vid_dec_class);
	unregister_chrdev_region(vid_dec_dev_num, NUM_OF_DRIVER_NODES);
	kfree(vid_dec_device_p);
	DBG("msm_vidc_dec: Return from %s()", __func__);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Video decoder driver");
MODULE_VERSION("1.0");

module_init(vid_dec_init);
module_exit(vid_dec_exit);
