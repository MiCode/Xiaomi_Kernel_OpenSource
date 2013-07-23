/* Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
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
#include <linux/poll.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

#include <linux/clk.h>
#include <linux/timer.h>
#include <mach/iommu_domains.h>
#include <media/msm/vidc_type.h>
#include <media/msm/vcd_api.h>
#include <media/msm/vidc_init.h>
#include "mpq_dvb_video_internal.h"


#define DBG(x...) pr_debug(x)
#define INFO(x...) pr_info(x)
#define ERR(x...) pr_err(x)

#define MPQ_VID_DEC_NAME "mpq_vidc_dec"

static char vid_thread_names[DVB_MPQ_NUM_VIDEO_DEVICES][10] = {
				"dvb-vid-0",
				"dvb-vid-1",
				"dvb-vid-2",
				"dvb-vid-3",
};

static enum scan_format map_scan_type(enum vdec_interlaced_format type);
static int mpq_int_vid_dec_decode_frame(struct video_client_ctx *client_ctx,
				struct video_data_buffer *input_frame);
static int mpq_int_vid_dec_get_buffer_req(struct video_client_ctx *client_ctx,
				  struct video_buffer_req *vdec_buf_req);

static struct mpq_dvb_video_dev *mpq_dvb_video_device;

static int mpq_get_dev_frm_client(struct video_client_ctx *client_ctx,
				struct mpq_dvb_video_inst **dev_inst)
{
	int i;

	for (i = 0; i < DVB_MPQ_NUM_VIDEO_DEVICES; i++) {
		if (mpq_dvb_video_device->dev_inst[i].client_ctx ==
						client_ctx) {
			*dev_inst = &mpq_dvb_video_device->dev_inst[i];
			break;
		}
	}

	if (i == DVB_MPQ_NUM_VIDEO_DEVICES)
		return -ENODEV;

	return 0;
}

static u32 mpq_int_check_bcast_mq(struct mpq_dmx_src_data *dmx_data)
{
	u32 islist_empty = 0;
	mutex_lock(&dmx_data->msg_queue_lock);
	islist_empty = list_empty(&dmx_data->msg_queue);
	mutex_unlock(&dmx_data->msg_queue_lock);

	return !islist_empty;
}

static void mpq_get_frame_and_write(struct mpq_dvb_video_inst *dev_inst,
				unsigned int free_buf)
{
	struct mpq_dmx_src_data *dmx_data = dev_inst->dmx_src_data;
	struct mpq_streambuffer *streambuff = dmx_data->stream_buffer;
	struct mpq_streambuffer_packet_header pkt_hdr;
	struct mpq_adapter_video_meta_data meta_data;
	ssize_t indx = -1;
	ssize_t bytes_read = 0;
	size_t pktlen = 0;
	int frame_found = true;
	unsigned long kernel_vaddr, phy_addr, user_vaddr;
	int pmem_fd;
	struct file *file;
	s32 buffer_index = -1;
	size_t size = 0;

	do {
		wait_event_interruptible(streambuff->packet_data.queue,
			(!dvb_ringbuffer_empty(&streambuff->packet_data) ||
			streambuff->packet_data.error != 0) ||
			kthread_should_stop());

		if (kthread_should_stop()) {
			DBG("STOP signal Received\n");
			return;
		}

		DBG("Received Free Buffer : %d\n", free_buf);

		indx = mpq_streambuffer_pkt_next(streambuff, -1, &pktlen);

		if (-1 == indx) {
			DBG("Invalid Index -1\n");
			return;
		}

		bytes_read = mpq_streambuffer_pkt_read(streambuff, indx,
			&pkt_hdr, (u8 *)&meta_data);

		switch (meta_data.packet_type) {
		case DMX_FRAMING_INFO_PACKET:
			switch (meta_data.info.framing.pattern_type) {
			case DMX_IDX_H264_SPS:
			case DMX_IDX_MPEG_SEQ_HEADER:
			case DMX_IDX_VC1_SEQ_HEADER:
			case DMX_IDX_H264_ACCESS_UNIT_DEL:
			case DMX_IDX_H264_SEI:
				DBG("SPS FOUND\n");
				frame_found = false;
				break;
			case DMX_IDX_H264_PPS:
			case DMX_IDX_MPEG_GOP:
			case DMX_IDX_VC1_ENTRY_POINT:
				DBG("PPS FOUND\n");
				frame_found = false;
				break;
			case DMX_IDX_H264_IDR_START:
			case DMX_IDX_H264_NON_IDR_START:
			case DMX_IDX_MPEG_I_FRAME_START:
			case DMX_IDX_MPEG_P_FRAME_START:
			case DMX_IDX_MPEG_B_FRAME_START:
			case DMX_IDX_VC1_FRAME_START:
				DBG("FRAME FOUND\n");
				frame_found = true;
				break;
			default:
				break;
			}
			user_vaddr = (unsigned long)
				dmx_data->in_buffer[free_buf].bufferaddr;
			vidc_lookup_addr_table(dev_inst->client_ctx,
				BUFFER_TYPE_INPUT, true, &user_vaddr,
				&kernel_vaddr,	&phy_addr, &pmem_fd, &file,
				&buffer_index);
			bytes_read = 0;
			bytes_read = mpq_streambuffer_data_read(streambuff,
						(u8 *)(kernel_vaddr + size),
						pkt_hdr.raw_data_len);
			DBG("Data Read : %d from Packet Size : %d\n",
				bytes_read, pkt_hdr.raw_data_len);
			mpq_streambuffer_pkt_dispose(streambuff, indx, 0);
			size +=	pkt_hdr.raw_data_len;
			dmx_data->in_buffer[free_buf].pts =
			(meta_data.info.framing.pts_dts_info.pts_exist) ?
			(meta_data.info.framing.pts_dts_info.pts) : 0;
			if (frame_found) {
				dmx_data->in_buffer[free_buf].buffer_len =
									size;
				dmx_data->in_buffer[free_buf].client_data =
							(void *)free_buf;
				DBG("Size of Data Submitted : %d\n", size);
				mpq_int_vid_dec_decode_frame(
						dev_inst->client_ctx,
						&dmx_data->in_buffer[free_buf]);
			}
			break;
		case DMX_EOS_PACKET:
			break;
		case DMX_PES_PACKET:
		case DMX_MARKER_PACKET:
			break;
		default:
			break;
		}
	} while (!frame_found);

}

static int mpq_bcast_data_handler(void *arg)
{
	struct mpq_dvb_video_inst *dev_inst = arg;
	struct mpq_dmx_src_data *dmx_data = dev_inst->dmx_src_data;
	struct mpq_bcast_msg *pMesg;
	struct mpq_bcast_msg_info msg = {0};

	do {
		wait_event_interruptible(dmx_data->msg_wait,
					((dmx_data->stream_buffer != NULL) &&
					mpq_int_check_bcast_mq(dmx_data)) ||
					kthread_should_stop());

		if (kthread_should_stop()) {
			DBG("STOP signal Received\n");
			break;
		}

		mutex_lock(&dmx_data->msg_queue_lock);
		if (!list_empty(&dmx_data->msg_queue)) {
			pMesg = list_first_entry(&dmx_data->msg_queue,
					struct mpq_bcast_msg, list);
			list_del(&pMesg->list);
			memcpy(&msg, &pMesg->info,
				sizeof(struct mpq_bcast_msg_info));
			kfree(pMesg);
		}
		mutex_unlock(&dmx_data->msg_queue_lock);

		switch (msg.code) {
		case MPQ_BCAST_MSG_IBD:
			DBG("Received IBD Mesg for :%d\n", msg.data);
			mpq_get_frame_and_write(dev_inst, msg.data);
			break;
		default:
			DBG("Received Mesg : %d\n", msg.code);
		}
	} while (1);

	return 0;
}

static s32 mpq_int_vid_dec_get_empty_client_index(void)
{
	u32 i, found = false;

	for (i = 0; i < VIDC_MAX_NUM_CLIENTS; i++) {
		if (!mpq_dvb_video_device->vdec_clients[i].vcd_handle) {
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

static u32 mpq_int_vid_dec_get_status(u32 status)
{
	u32 vdec_status;

	switch (status) {
	case VCD_ERR_SEQHDR_PARSE_FAIL:
	case VCD_ERR_BITSTREAM_ERR:
		vdec_status = VIDEO_STATUS_BITSTREAM_ERROR;
		break;
	case VCD_S_SUCCESS:
		vdec_status = VIDEO_STATUS_SUCESS;
		break;
	case VCD_ERR_FAIL:
		vdec_status = VIDEO_STATUS_FAILED;
		break;
	case VCD_ERR_ALLOC_FAIL:
	case VCD_ERR_MAX_CLIENT:
		vdec_status = VIDEO_STATUS_NORESOURCE;
		break;
	case VCD_ERR_ILLEGAL_OP:
		vdec_status = VIDEO_STATUS_INVALID_CMD;
		break;
	case VCD_ERR_ILLEGAL_PARM:
		vdec_status = VIDEO_STATUS_INVALID_PARAM;
		break;
	case VCD_ERR_BAD_POINTER:
	case VCD_ERR_BAD_HANDLE:
		vdec_status = VIDEO_STATUS_INVALID_HANDLE;
		break;
	case VCD_ERR_NOT_SUPPORTED:
		vdec_status = VIDEO_STATUS_NO_SUPPORT;
		break;
	case VCD_ERR_BAD_STATE:
		vdec_status = VIDEO_STATUS_INVALID_STATE;
		break;
	case VCD_ERR_BUSY:
		vdec_status = VIDEO_STATUS_BUSY;
		break;
	default:
		vdec_status = VIDEO_STATUS_FAILED;
		break;
	}

	return vdec_status;
}

static void mpq_int_vid_dec_notify_client(struct video_client_ctx *client_ctx)
{
	if (client_ctx)
		complete(&client_ctx->event);
}

static void mpq_int_vid_dec_vcd_open_done(struct video_client_ctx *client_ctx,
			   struct vcd_handle_container *handle_container)
{
	if (client_ctx) {
		if (handle_container)
			client_ctx->vcd_handle = handle_container->handle;
		else
			DBG("%s(): ERROR. handle_container is NULL\n",
			    __func__);

		mpq_int_vid_dec_notify_client(client_ctx);
	} else
		DBG("%s(): ERROR. client_ctx is NULL\n", __func__);
}

static void mpq_int_vid_dec_handle_field_drop(
	struct video_client_ctx *client_ctx,
	u32 event, u32 status, int64_t time_stamp)
{
	struct vid_dec_msg *vdec_msg;

	if (!client_ctx) {
		DBG("%s() NULL pointer\n", __func__);
		return;
	}

	vdec_msg = kzalloc(sizeof(struct vid_dec_msg), GFP_KERNEL);
	if (!vdec_msg) {
		DBG("%s(): cannot allocate vid_dec_msg "\
			" buffer\n", __func__);
		return;
	}
	vdec_msg->vdec_msg_info.status_code =
		mpq_int_vid_dec_get_status(status);
	if (event == VCD_EVT_IND_INFO_FIELD_DROPPED) {
		vdec_msg->vdec_msg_info.msgcode =
			VDEC_MSG_EVT_INFO_FIELD_DROPPED;
		vdec_msg->vdec_msg_info.msgdata.output_frame.time_stamp
		= time_stamp;
		DBG("Send FIELD_DROPPED message to client = %p\n",
						client_ctx);
	} else {
		DBG("mpq_int_vid_dec_input_frame_done(): "\
			"invalid event type: %d\n", event);
		vdec_msg->vdec_msg_info.msgcode = VDEC_MSG_INVALID;
	}
	vdec_msg->vdec_msg_info.msgdatasize =
		sizeof(struct vdec_output_frameinfo);
	mutex_lock(&client_ctx->msg_queue_lock);
	list_add_tail(&vdec_msg->list, &client_ctx->msg_queue);
	mutex_unlock(&client_ctx->msg_queue_lock);
	wake_up(&client_ctx->msg_wait);
}

static void mpq_int_vid_dec_input_frame_done(
			struct video_client_ctx *client_ctx, u32 event,
			u32 status, struct vcd_frame_data *vcd_frame_data)
{
	struct vid_dec_msg *vdec_msg;
	struct mpq_bcast_msg *bcast_msg;
	struct mpq_dvb_video_inst *dev_inst;
	struct mpq_dmx_src_data *dmx_data;
	int rc = 0;

	if (!client_ctx || !vcd_frame_data) {
		DBG("mpq_int_vid_dec_input_frame_done() NULL pointer\n");
		return;
	}

	kfree(vcd_frame_data->desc_buf);
	vcd_frame_data->desc_buf = NULL;
	vcd_frame_data->desc_size = 0;

	rc = mpq_get_dev_frm_client(client_ctx, &dev_inst);
	if (rc) {
		DBG("Failed to obtain device instance\n");
		return;
	}

	if (dev_inst->source == VIDEO_SOURCE_MEMORY) {
		vdec_msg = kzalloc(sizeof(struct vid_dec_msg), GFP_KERNEL);
		if (!vdec_msg) {
			DBG("mpq_int_vid_dec_input_frame_done(): "\
			"cannot allocate vid_dec_msg buffer\n");
			return;
		}

		vdec_msg->vdec_msg_info.status_code =
				mpq_int_vid_dec_get_status(status);

		if (event == VCD_EVT_RESP_INPUT_DONE) {
			vdec_msg->vdec_msg_info.msgcode =
			VDEC_MSG_RESP_INPUT_BUFFER_DONE;
			DBG("Send INPUT_DON message to client = %p\n",
						client_ctx);

		} else if (event == VCD_EVT_RESP_INPUT_FLUSHED) {
			vdec_msg->vdec_msg_info.msgcode =
					VDEC_MSG_RESP_INPUT_FLUSHED;
			DBG("Send INPUT_FLUSHED message to client = %p\n",
						client_ctx);
		} else {
			DBG("mpq_int_vid_dec_input_frame_done(): "\
				"invalid event type: %d\n", event);
			vdec_msg->vdec_msg_info.msgcode = VDEC_MSG_INVALID;
		}

		vdec_msg->vdec_msg_info.msgdata.input_frame_clientdata =
			(void *)vcd_frame_data->frm_clnt_data;
		vdec_msg->vdec_msg_info.msgdatasize = sizeof(void *);

		mutex_lock(&client_ctx->msg_queue_lock);
		list_add_tail(&vdec_msg->list, &client_ctx->msg_queue);
		mutex_unlock(&client_ctx->msg_queue_lock);
		wake_up(&client_ctx->msg_wait);
	} else {
		if (event == VCD_EVT_RESP_INPUT_DONE) {
			bcast_msg = kzalloc(sizeof(struct mpq_bcast_msg),
						GFP_KERNEL);
			if (!bcast_msg) {
				DBG("mpq_int_vid_dec_input_frame_done(): "\
				"cannot allocate mpq_bcast_msg buffer\n");
				return;
			}

			bcast_msg->info.code = MPQ_BCAST_MSG_IBD;
			bcast_msg->info.data =
				(unsigned int)vcd_frame_data->frm_clnt_data;

			dmx_data = dev_inst->dmx_src_data;

			mutex_lock(&dmx_data->msg_queue_lock);
			list_add_tail(&bcast_msg->list, &dmx_data->msg_queue);
			mutex_unlock(&dmx_data->msg_queue_lock);
			wake_up(&dmx_data->msg_wait);
		}
	}
}

static void mpq_int_vid_dec_output_frame_done(
			struct video_client_ctx *client_ctx,
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
		DBG("mpq_int_vid_dec_input_frame_done() NULL pointer\n");
		return;
	}

	vdec_msg = kzalloc(sizeof(struct vid_dec_msg), GFP_KERNEL);
	if (!vdec_msg) {
		DBG("mpq_int_vid_dec_input_frame_done(): "\
		    "cannot allocate vid_dec_msg buffer\n");
		return;
	}

	vdec_msg->vdec_msg_info.status_code =
			mpq_int_vid_dec_get_status(status);

	if (event == VCD_EVT_RESP_OUTPUT_DONE)
		vdec_msg->vdec_msg_info.msgcode =
		    VDEC_MSG_RESP_OUTPUT_BUFFER_DONE;
	else if (event == VCD_EVT_RESP_OUTPUT_FLUSHED)
		vdec_msg->vdec_msg_info.msgcode = VDEC_MSG_RESP_OUTPUT_FLUSHED;
	else {
		DBG("QVD: mpq_int_vid_dec_output_frame_done"\
			"invalid cmd type : %d\n", event);
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
			vcd_frame_data->aspect_ratio_info.par_width;
		output_frame->aspect_ratio_info.par_height =
			vcd_frame_data->aspect_ratio_info.par_height;
		vdec_msg->vdec_msg_info.msgdatasize =
		    sizeof(struct vdec_output_frameinfo);
	} else {
		DBG("mpq_int_vid_dec_output_frame_done UVA"\
				"can not be found\n");
		vdec_msg->vdec_msg_info.status_code = VDEC_S_EFATAL;
	}
	if (vcd_frame_data->data_len > 0) {
		ion_flag = vidc_get_fd_info(client_ctx, BUFFER_TYPE_OUTPUT,
				pmem_fd, kernel_vaddr, buffer_index,
				&buff_handle);
		if (ion_flag == ION_FLAG_CACHED && buff_handle) {
			msm_ion_do_cache_op(
				client_ctx->user_ion_client,
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

static void mpq_int_vid_dec_lean_event(struct video_client_ctx *client_ctx,
			       u32 event, u32 status)
{
	struct vid_dec_msg *vdec_msg;

	if (!client_ctx) {
		DBG("%s(): !client_ctx pointer\n", __func__);
		return;
	}

	vdec_msg = kzalloc(sizeof(struct vid_dec_msg), GFP_KERNEL);
	if (!vdec_msg) {
		DBG("%s(): cannot allocate vid_dec_msg buffer\n",
					__func__);
		return;
	}

	vdec_msg->vdec_msg_info.status_code =
			mpq_int_vid_dec_get_status(status);

	switch (event) {
	case VCD_EVT_IND_OUTPUT_RECONFIG:
		DBG("msm_vidc_dec: Sending VDEC_MSG_EVT_CONFIG_CHANGED"\
			 " to client");
		vdec_msg->vdec_msg_info.msgcode = VDEC_MSG_EVT_CONFIG_CHANGED;
		break;
	case VCD_EVT_IND_RESOURCES_LOST:
		DBG("msm_vidc_dec: Sending VDEC_EVT_RESOURCES_LOST"\
			 " to client");
		vdec_msg->vdec_msg_info.msgcode = VDEC_EVT_RESOURCES_LOST;
		break;
	case VCD_EVT_RESP_FLUSH_INPUT_DONE:
		DBG("msm_vidc_dec: Sending VDEC_MSG_RESP_FLUSH_INPUT_DONE"\
			 " to client");
		vdec_msg->vdec_msg_info.msgcode =
		    VDEC_MSG_RESP_FLUSH_INPUT_DONE;
		break;
	case VCD_EVT_RESP_FLUSH_OUTPUT_DONE:
		DBG("msm_vidc_dec: Sending VDEC_MSG_RESP_FLUSH_OUTPUT_DONE"\
			 " to client");
		vdec_msg->vdec_msg_info.msgcode =
		    VDEC_MSG_RESP_FLUSH_OUTPUT_DONE;
		break;
	case VCD_EVT_IND_HWERRFATAL:
		DBG("msm_vidc_dec: Sending VDEC_MSG_EVT_HW_ERROR"\
			 " to client");
		vdec_msg->vdec_msg_info.msgcode = VDEC_MSG_EVT_HW_ERROR;
		break;
	case VCD_EVT_RESP_START:
		DBG("msm_vidc_dec: Sending VDEC_MSG_RESP_START_DONE"\
			 " to client");
		vdec_msg->vdec_msg_info.msgcode = VDEC_MSG_RESP_START_DONE;
		break;
	case VCD_EVT_RESP_STOP:
		DBG("msm_vidc_dec: Sending VDEC_MSG_RESP_STOP_DONE"\
			 " to client");
		vdec_msg->vdec_msg_info.msgcode = VDEC_MSG_RESP_STOP_DONE;
		break;
	case VCD_EVT_RESP_PAUSE:
		DBG("msm_vidc_dec: Sending VDEC_MSG_RESP_PAUSE_DONE"\
			 " to client");
		vdec_msg->vdec_msg_info.msgcode = VDEC_MSG_RESP_PAUSE_DONE;
		break;
	case VCD_EVT_IND_INFO_OUTPUT_RECONFIG:
		DBG("msm_vidc_dec: Sending VDEC_MSG_EVT_INFO_CONFIG_CHANGED"\
			 " to client");
		vdec_msg->vdec_msg_info.msgcode =
			 VDEC_MSG_EVT_INFO_CONFIG_CHANGED;
		break;
	default:
		DBG("%s() : unknown event type\n", __func__);
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

static void mpq_int_state_play(struct video_client_ctx *client_ctx,
			       u32 event, u32 status)
{
	struct mpq_bcast_msg *bcast_msg;
	struct mpq_dvb_video_inst *dev_inst;
	int i;
	int rc = 0;
	struct mpq_dmx_src_data *dmx_data = NULL;

	if (!client_ctx->seq_header_set) {
		rc = mpq_get_dev_frm_client(client_ctx, &dev_inst);
		if (rc) {
			DBG("Failed to get dev_instance in %s\n", __func__);
			return;
		}

		if (VIDEO_SOURCE_DEMUX == dev_inst->source) {
			dmx_data = dev_inst->dmx_src_data;
			for (i = 0; i < DVB_VID_NUM_IN_BUFFERS; i++) {
				bcast_msg = kzalloc(
						sizeof(struct mpq_bcast_msg),
						GFP_KERNEL);
				if (!bcast_msg) {
					DBG("cannot allocate mpq_bcast_msg"\
						"buffer\n");
					return;
				}

				bcast_msg->info.code = MPQ_BCAST_MSG_IBD;
				bcast_msg->info.data = (unsigned int)i;

				mutex_lock(&dmx_data->msg_queue_lock);
				list_add_tail(&bcast_msg->list,
						&dmx_data->msg_queue);
				mutex_unlock(&dmx_data->msg_queue_lock);
				wake_up(&dmx_data->msg_wait);
			}
		}
		mpq_int_vid_dec_lean_event(client_ctx, event, status);
	} else
		mpq_int_vid_dec_notify_client(client_ctx);

}

static void mpq_int_vid_dec_vcd_cb(u32 event, u32 status,
		   void *info, size_t sz, void *handle,
		   void *const client_data)
{
	struct video_client_ctx *client_ctx = client_data;

	DBG("Entering %s()\n", __func__);

	if (!client_ctx) {
		DBG("%s(): client_ctx is NULL\n", __func__);
		return;
	}

	client_ctx->event_status = status;

	switch (event) {
	case VCD_EVT_RESP_OPEN:
		mpq_int_vid_dec_vcd_open_done(client_ctx,
				      (struct vcd_handle_container *)
				      info);
		break;
	case VCD_EVT_RESP_INPUT_DONE:
	case VCD_EVT_RESP_INPUT_FLUSHED:
		mpq_int_vid_dec_input_frame_done(client_ctx, event, status,
					 (struct vcd_frame_data *)info);
		break;
	case VCD_EVT_IND_INFO_FIELD_DROPPED:
		if (info)
			mpq_int_vid_dec_handle_field_drop(client_ctx, event,
			status,	*((int64_t *)info));
		else
			DBG("Wrong Payload for Field dropped\n");
		break;
	case VCD_EVT_RESP_OUTPUT_DONE:
	case VCD_EVT_RESP_OUTPUT_FLUSHED:
		mpq_int_vid_dec_output_frame_done(client_ctx, event, status,
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
		mpq_int_vid_dec_lean_event(client_ctx, event, status);
		break;
	case VCD_EVT_RESP_START:
		mpq_int_state_play(client_ctx, event, status);
		break;
	default:
		DBG("%s() :  Error - Invalid event type =%u\n", __func__,
		    event);
		break;
	}
}

static int mpq_int_vid_dec_set_cont_on_reconfig(
			struct video_client_ctx *client_ctx)
{
	struct vcd_property_hdr vcd_property_hdr;
	u32 vcd_status = VCD_ERR_FAIL;
	u32 enable = true;
	if (!client_ctx)
		return -EINVAL;
	vcd_property_hdr.prop_id = VCD_I_CONT_ON_RECONFIG;
	vcd_property_hdr.sz = sizeof(u32);
	vcd_status = vcd_set_property(client_ctx->vcd_handle,
		&vcd_property_hdr, &enable);
	if (vcd_status)
		return -EIO;
	return 0;
}

static int mpq_int_vid_dec_set_frame_resolution(
				struct video_client_ctx *client_ctx,
				struct vdec_picsize *video_resoultion)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_frame_size frame_resolution;
	u32 vcd_status = VCD_ERR_FAIL;

	if (!client_ctx || !video_resoultion)
		return -EINVAL;

	vcd_property_hdr.prop_id = VCD_I_FRAME_SIZE;
	vcd_property_hdr.sz = sizeof(struct vcd_property_frame_size);
	frame_resolution.width = video_resoultion->frame_width;
	frame_resolution.height = video_resoultion->frame_height;
	frame_resolution.stride = video_resoultion->stride;
	frame_resolution.scan_lines = video_resoultion->scan_lines;

	vcd_status = vcd_set_property(client_ctx->vcd_handle,
				      &vcd_property_hdr, &frame_resolution);

	if (vcd_status)
		return -EIO;
	else
		return 0;
}

static int mpq_int_set_out_buffer_req(struct video_client_ctx *client_ctx,
					struct video_buffer_req *vdec_buf_req)
{
	struct vcd_buffer_requirement buffer_req;
	u32 vcd_status = VCD_ERR_FAIL;

	buffer_req.actual_count = vdec_buf_req->num_output_buffers;
	buffer_req.align = vdec_buf_req->output_buf_prop.alignment;
	buffer_req.max_count = vdec_buf_req->num_output_buffers;
	buffer_req.min_count = vdec_buf_req->num_output_buffers;
	buffer_req.sz = vdec_buf_req->output_buf_prop.buf_size;

	vcd_status = vcd_set_buffer_requirements(client_ctx->vcd_handle,
			VCD_BUFFER_OUTPUT, &buffer_req);
	if (vcd_status)
		return -EFAULT;
	else
		return 0;
}

static int mpq_int_set_full_hd_frame_resolution(
				struct video_client_ctx *client_ctx)
{
	struct vdec_picsize pic_res;
	int rc;

	pic_res.frame_height = 1080;
	pic_res.frame_width  = 1920;
	pic_res.scan_lines   = 1080;
	pic_res.stride       = 1920;

	rc = mpq_int_vid_dec_set_frame_resolution(client_ctx,
						&pic_res);
	if (rc)
		DBG("Failed in mpq_int_vid_dec_set_frame_resolution : %d\n",\
			rc);

	rc = mpq_int_vid_dec_set_cont_on_reconfig(client_ctx);
	if (rc)
		DBG("Failed in mpq_int_vid_dec_set_cont_on_reconfig : %d\n",\
			rc);

	return rc;

}

static int mpq_int_vid_dec_get_frame_resolution(
			struct video_client_ctx *client_ctx,
			struct video_pic_res *video_resoultion)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_frame_size frame_resolution;
	u32 vcd_status = VCD_ERR_FAIL;

	if (!client_ctx || !video_resoultion)
		return -EINVAL;

	vcd_property_hdr.prop_id = VCD_I_FRAME_SIZE;
	vcd_property_hdr.sz = sizeof(struct vcd_property_frame_size);

	vcd_status = vcd_get_property(client_ctx->vcd_handle,
				&vcd_property_hdr, &frame_resolution);

	video_resoultion->width  = frame_resolution.width;
	video_resoultion->height = frame_resolution.height;
	video_resoultion->scan_lines = frame_resolution.scan_lines;
	video_resoultion->stride = frame_resolution.stride;

	if (vcd_status)
		return -EINVAL;
	else
		return 0;
}

static int mpq_int_vid_dec_get_codec(struct video_client_ctx *client_ctx,
					enum video_codec_t *video_codec)
{
	unsigned int result = 0;
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_codec codec;

	if ((client_ctx == NULL) || (video_codec == NULL))
		return -EINVAL;

	vcd_property_hdr.prop_id = VCD_I_CODEC;
	vcd_property_hdr.sz = sizeof(struct vcd_property_codec);

	result = vcd_get_property(client_ctx->vcd_handle,
					&vcd_property_hdr, &codec);
	if (result)
		return -EINVAL;

	switch (codec.codec) {
	case VCD_CODEC_MPEG4:
		*video_codec = VIDEO_CODECTYPE_MPEG4;
		break;
	case VCD_CODEC_H264:
		*video_codec = VIDEO_CODECTYPE_H264;
		break;
	case VCD_CODEC_MPEG2:
		*video_codec = VIDEO_CODECTYPE_MPEG2;
		break;
	case VCD_CODEC_VC1:
		*video_codec = VIDEO_CODECTYPE_VC1;
		break;
	default:
		*video_codec = VIDEO_CODECTYPE_NONE;
		break;
	}

	return result;
}

static int mpq_int_vid_dec_set_codec(struct video_client_ctx *client_ctx,
				enum video_codec_t video_codec)
{
	unsigned int result = 0;
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_codec codec;
	unsigned int vcd_status = VCD_ERR_FAIL;

	if (client_ctx == NULL)
		return -EINVAL;

	vcd_property_hdr.prop_id = VCD_I_CODEC;
	vcd_property_hdr.sz = sizeof(struct vcd_property_codec);

	switch (video_codec) {
	case VIDEO_CODECTYPE_MPEG4:
		codec.codec = VCD_CODEC_MPEG4;
		break;
	case VIDEO_CODECTYPE_H264:
		codec.codec = VCD_CODEC_H264;
		break;
	case VIDEO_CODECTYPE_MPEG2:
		codec.codec = VCD_CODEC_MPEG2;
		break;
	case VIDEO_CODECTYPE_VC1:
		codec.codec = VCD_CODEC_VC1;
		break;
	default:
		result = -EINVAL;
		break;
	}

	if (!result) {
		vcd_status = vcd_set_property(client_ctx->vcd_handle,
					      &vcd_property_hdr, &codec);
		if (vcd_status)
			result = -EINVAL;
	}

	result = mpq_int_set_full_hd_frame_resolution(client_ctx);

	return result;
}

static int mpq_int_vid_dec_set_output_format(
		struct video_client_ctx *client_ctx,
		enum video_out_format_t format)
{
	unsigned int result = 0;
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_buffer_format vcd_prop_buffer_format;
	unsigned int vcd_status = VCD_ERR_FAIL;

	if (client_ctx == NULL)
		return -EINVAL;

	vcd_property_hdr.prop_id = VCD_I_BUFFER_FORMAT;
	vcd_property_hdr.sz = sizeof(struct vcd_property_buffer_format);

	switch (format) {
	case VIDEO_YUV_FORMAT_NV12:
		vcd_prop_buffer_format.buffer_format = VCD_BUFFER_FORMAT_NV12;
		break;
	case VIDEO_YUV_FORMAT_TILE_4x2:
		vcd_prop_buffer_format.buffer_format =
					VCD_BUFFER_FORMAT_TILE_4x2;
		break;
	default:
		result = -EINVAL;
		break;
	}

	if (!result)
		vcd_status = vcd_set_property(client_ctx->vcd_handle,
					      &vcd_property_hdr,
					      &vcd_prop_buffer_format);

	if (vcd_status)
		return -EINVAL;

	return 0;

}

static int mpq_int_vid_dec_get_output_format(
			struct video_client_ctx *client_ctx,
			enum video_out_format_t *format)
{
	unsigned int result = 0;
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_buffer_format vcd_prop_buffer_format;

	if ((client_ctx == NULL) || (format == NULL))
		return -EINVAL;

	vcd_property_hdr.prop_id = VCD_I_BUFFER_FORMAT;
	vcd_property_hdr.sz = sizeof(struct vcd_property_buffer_format);

	result = vcd_get_property(client_ctx->vcd_handle, &vcd_property_hdr,
					&vcd_prop_buffer_format);

	if (result)
		return -EINVAL;

	switch (vcd_prop_buffer_format.buffer_format) {
	case VCD_BUFFER_FORMAT_NV12:
		*format = VIDEO_YUV_FORMAT_NV12;
		break;
	case VCD_BUFFER_FORMAT_TILE_4x2:
		*format = VIDEO_YUV_FORMAT_TILE_4x2;
		break;
	default:
		result = -EINVAL;
		break;
	}

	return result;
}

static int mpq_int_vid_dec_set_h264_mv_buffers(
				struct video_client_ctx *client_ctx,
				struct video_h264_mv *mv_data)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_h264_mv_buffer *vcd_h264_mv_buffer = NULL;
	u32 vcd_status = VCD_ERR_FAIL;
	int rc = 0;
	unsigned long ionflag = 0;
	unsigned long buffer_size = 0;
	unsigned long iova = 0;

	if (!client_ctx || !mv_data)
		return -EINVAL;

	vcd_property_hdr.prop_id = VCD_I_H264_MV_BUFFER;
	vcd_property_hdr.sz = sizeof(struct vcd_property_h264_mv_buffer);
	vcd_h264_mv_buffer = &client_ctx->vcd_h264_mv_buffer;

	memset(&client_ctx->vcd_h264_mv_buffer, 0,
		   sizeof(struct vcd_property_h264_mv_buffer));
	vcd_h264_mv_buffer->size = mv_data->size;
	vcd_h264_mv_buffer->count = mv_data->count;
	vcd_h264_mv_buffer->pmem_fd = mv_data->ion_fd;
	vcd_h264_mv_buffer->offset = mv_data->offset;

	if (!vcd_get_ion_status()) {
		pr_err("PMEM not available");
		return -EINVAL;
	} else {
		client_ctx->h264_mv_ion_handle = ion_import_dma_buf(
					client_ctx->user_ion_client,
					vcd_h264_mv_buffer->pmem_fd);
		if (IS_ERR_OR_NULL(client_ctx->h264_mv_ion_handle)) {
			DBG("%s(): get_ION_handle failed\n", __func__);
			goto import_ion_error;
		}
		rc = ion_handle_get_flags(client_ctx->user_ion_client,
					client_ctx->h264_mv_ion_handle,
					&ionflag);
		if (rc) {
			DBG("%s():get_ION_flags fail\n",
					 __func__);
			goto import_ion_error;
		}
		vcd_h264_mv_buffer->kernel_virtual_addr =
			(u8 *) ion_map_kernel(client_ctx->user_ion_client,
					client_ctx->h264_mv_ion_handle);
		if (!vcd_h264_mv_buffer->kernel_virtual_addr) {
			DBG("%s(): get_ION_kernel virtual addr failed\n",
				 __func__);
			goto import_ion_error;
		}

		rc = ion_map_iommu(client_ctx->user_ion_client,
				client_ctx->h264_mv_ion_handle,
				VIDEO_DOMAIN, VIDEO_MAIN_POOL,
				SZ_4K, 0, (unsigned long *)&iova,
				(unsigned long *)&buffer_size,
				0, 0);
		if (rc) {
			DBG("%s():get_ION_kernel physical addr fail\n",
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
		return -EIO;
	else
		return 0;
ion_map_error:
	if (vcd_h264_mv_buffer->kernel_virtual_addr) {
		ion_unmap_kernel(client_ctx->user_ion_client,
				client_ctx->h264_mv_ion_handle);
		vcd_h264_mv_buffer->kernel_virtual_addr = NULL;
	}
	if (!IS_ERR_OR_NULL(client_ctx->h264_mv_ion_handle)) {
		ion_free(client_ctx->user_ion_client,
			client_ctx->h264_mv_ion_handle);
		 client_ctx->h264_mv_ion_handle = NULL;
	}
import_ion_error:
	return -EIO;
}

static int mpq_int_vid_dec_get_h264_mv_buffer_size(
				struct video_client_ctx *client_ctx,
				struct video_mv_buff_size *mv_buff)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_buffer_size h264_mv_buffer_size;
	u32 vcd_status = VCD_ERR_FAIL;

	if (!client_ctx || !mv_buff)
		return -EINVAL;

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
		return -EIO;
	else
		return 0;
}

static int mpq_int_vid_dec_free_h264_mv_buffers(
				struct video_client_ctx *client_ctx)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_buffer_size h264_mv_buffer_size;
	u32 vcd_status = VCD_ERR_FAIL;

	if (!client_ctx)
		return -EINVAL;

	vcd_property_hdr.prop_id = VCD_I_FREE_H264_MV_BUFFER;
	vcd_property_hdr.sz = sizeof(struct vcd_property_buffer_size);

	vcd_status = vcd_set_property(client_ctx->vcd_handle,
				      &vcd_property_hdr, &h264_mv_buffer_size);

	if (!IS_ERR_OR_NULL(client_ctx->h264_mv_ion_handle)) {
		ion_unmap_kernel(client_ctx->user_ion_client,
					client_ctx->h264_mv_ion_handle);
		ion_unmap_iommu(client_ctx->user_ion_client,
				client_ctx->h264_mv_ion_handle,
				VIDEO_DOMAIN,
				VIDEO_MAIN_POOL);
		ion_free(client_ctx->user_ion_client,
					client_ctx->h264_mv_ion_handle);
		 client_ctx->h264_mv_ion_handle = NULL;
	}

	if (vcd_status)
		return -EIO;
	else
		return 0;
}


static int mpq_int_vid_dec_get_buffer_req(struct video_client_ctx *client_ctx,
				  struct video_buffer_req *vdec_buf_req)
{
	u32 vcd_status = VCD_ERR_FAIL;
	struct vcd_buffer_requirement vcd_buf_req;

	if (!client_ctx || !vdec_buf_req)
		return -EINVAL;

	vcd_status = vcd_get_buffer_requirements(client_ctx->vcd_handle,
					VCD_BUFFER_INPUT, &vcd_buf_req);

	if (vcd_status)
		return -EINVAL;

	vdec_buf_req->input_buf_prop.alignment  = vcd_buf_req.align;
	vdec_buf_req->input_buf_prop.buf_poolid = vcd_buf_req.buf_pool_id;
	vdec_buf_req->input_buf_prop.buf_size   = vcd_buf_req.sz;
	vdec_buf_req->num_input_buffers         = vcd_buf_req.actual_count;

	vcd_status = vcd_get_buffer_requirements(client_ctx->vcd_handle,
					VCD_BUFFER_OUTPUT, &vcd_buf_req);

	if (vcd_status)
		return -EINVAL;

	vdec_buf_req->output_buf_prop.alignment  = vcd_buf_req.align;
	vdec_buf_req->output_buf_prop.buf_poolid = vcd_buf_req.buf_pool_id;
	vdec_buf_req->output_buf_prop.buf_size   = vcd_buf_req.sz;
	vdec_buf_req->num_output_buffers         = vcd_buf_req.actual_count;

	return 0;
}

static int mpq_int_vid_dec_set_buffer_req(struct video_client_ctx *client_ctx,
				  struct video_buffer_req vdec_buf_req)
{
	int rc = 0;
	struct video_buffer_req vdec_req;

	rc = mpq_int_vid_dec_get_buffer_req(client_ctx, &vdec_req);
	if (rc)
		DBG("Failed in mpq_int_vid_dec_get_buffer_req : %d\n", rc);

	vdec_req.num_output_buffers = vdec_buf_req.num_output_buffers;
	DBG(" num_output_buffers Set to %u\n", vdec_buf_req.num_output_buffers);
	if (!vdec_buf_req.num_output_buffers)
		return -EINVAL;
	rc = mpq_int_set_out_buffer_req(client_ctx, &vdec_req);
	if (rc)
		DBG("Failed in mpq_int_set_out_buffer_req  %d\n", rc);

	return 0;
}

static int mpq_int_vid_dec_set_buffer(struct mpq_dvb_video_inst *dev_inst,
			      struct video_data_buffer *data_buffer,
			      enum buffer_dir dir_buffer)
{
	struct video_client_ctx *client_ctx =
		(struct video_client_ctx *)dev_inst->client_ctx;
	enum vcd_buffer_type buffer = VCD_BUFFER_INPUT;
	u32 vcd_status = VCD_ERR_FAIL;
	unsigned long kernel_vaddr, buf_adr_offset = 0, length;
	int buffer_num = 0;

	if (!client_ctx || !data_buffer)
		return -EINVAL;

	if (dir_buffer == BUFFER_TYPE_OUTPUT) {
		buffer = VCD_BUFFER_OUTPUT;
		buf_adr_offset = (unsigned long)data_buffer->offset;
	}
	length = data_buffer->buffer_len;
	/*If buffer cannot be set, ignore */
	if (!vidc_insert_addr_table(client_ctx, dir_buffer,
		(unsigned long)data_buffer->bufferaddr,
		&kernel_vaddr, data_buffer->ion_fd,
		buf_adr_offset, MAX_VIDEO_NUM_OF_BUFF, length)) {
		ERR("%s() : user_virt_addr = %p cannot be set.",
		    __func__, data_buffer->bufferaddr);
		return -EINVAL;
	}

	vcd_status = vcd_set_buffer(client_ctx->vcd_handle,
		buffer, (u8 *) kernel_vaddr, data_buffer->buffer_len);

	if (!vcd_status) {
		mutex_lock(&client_ctx->enrty_queue_lock);
		if ((VIDEO_SOURCE_DEMUX == dev_inst->source) &&
			(BUFFER_TYPE_INPUT == dir_buffer)) {
			buffer_num = client_ctx->num_of_input_buffers - 1;
			memcpy(&dev_inst->dmx_src_data->in_buffer[buffer_num],
				data_buffer,
				sizeof(struct video_data_buffer));
		}
		mutex_unlock(&client_ctx->enrty_queue_lock);
		return 0;
	} else
		return -EINVAL;
}

static int mpq_int_vid_dec_free_buffer(struct video_client_ctx *client_ctx,
				struct video_data_buffer *data_buffer,
				enum buffer_dir dir_buffer)

{
	enum vcd_buffer_type buffer = VCD_BUFFER_INPUT;
	u32 vcd_status = VCD_ERR_FAIL;
	unsigned long kernel_vaddr;

	if (!client_ctx || !data_buffer)
		return -EINVAL;

	if (dir_buffer == BUFFER_TYPE_OUTPUT)
		buffer = VCD_BUFFER_OUTPUT;

	/*If buffer NOT set, ignore */
	if (!vidc_delete_addr_table(client_ctx, dir_buffer,
				(unsigned long)data_buffer->bufferaddr,
				&kernel_vaddr)) {
		DBG("%s() : user_virt_addr = %p has not been set.",
		    __func__, data_buffer->bufferaddr);
		return 0;
	}
	vcd_status = vcd_free_buffer(client_ctx->vcd_handle, buffer,
					 (u8 *)kernel_vaddr);

	if (vcd_status)
		return -EIO;
	else
		return 0;
}

static int mpq_int_vid_dec_pause_resume(struct video_client_ctx *client_ctx,
					u32 pause)
{
	u32 vcd_status;

	if (!client_ctx) {
		DBG("%s(): Invalid client_ctx\n", __func__);
		return -EINVAL;
	}

	if (pause) {
		DBG("msm_vidc_dec: PAUSE command from client = %p\n",
			 client_ctx);
		vcd_status = vcd_pause(client_ctx->vcd_handle);
	} else {
		DBG("msm_vidc_dec: RESUME command from client = %p\n",
			 client_ctx);
		vcd_status = vcd_resume(client_ctx->vcd_handle);
	}

	if (vcd_status)
		return -EIO;
	else
		return 0;
}

static int mpq_int_vid_dec_start_stop(struct video_client_ctx *client_ctx,
					u32 start)
{
	struct vid_dec_msg *vdec_msg = NULL;
	u32 vcd_status;

	DBG("Inside %s()", __func__);
	if (!client_ctx) {
		DBG("Invalid client_ctx\n");
		return -EINVAL;
	}

	if (start) {
		if (client_ctx->seq_header_set) {
			DBG("%s(): Seq Hdr set: Send START_DONE to client",
				 __func__);
			vdec_msg = kzalloc(sizeof(*vdec_msg), GFP_KERNEL);
			if (!vdec_msg) {
				DBG("mpq_int_vid_dec_start_stop:"\
				    " cannot allocate buffer\n");
				return -ENOMEM;
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
				DBG("%s(): vcd_decode_start failed."\
				    " vcd_status = %u\n", __func__,
				    vcd_status);
				return -EIO;
			}
		}
	} else {
		DBG("%s(): Calling vcd_stop()", __func__);
		mutex_lock(&mpq_dvb_video_device->lock);
		vcd_status = VCD_ERR_FAIL;
		if (!client_ctx->stop_called) {
			client_ctx->stop_called = true;
			vcd_status = vcd_stop(client_ctx->vcd_handle);
		}
		if (vcd_status) {
			DBG("%s(): vcd_stop failed.  vcd_status = %u\n",
				__func__, vcd_status);
			mutex_unlock(&mpq_dvb_video_device->lock);
			return -EIO;
		}
		DBG("Send STOP_DONE message to client = %p\n", client_ctx);
		mutex_unlock(&mpq_dvb_video_device->lock);
	}
	return 0;
}

static int mpq_int_vid_dec_decode_frame(struct video_client_ctx *client_ctx,
				struct video_data_buffer *input_frame)
{
	struct vcd_frame_data vcd_input_buffer;
	unsigned long kernel_vaddr, phy_addr, user_vaddr;
	int pmem_fd;
	struct file *file;
	s32 buffer_index = -1;
	u32 vcd_status = VCD_ERR_FAIL;
	u32 ion_flag = 0;
	struct ion_handle *buff_handle = NULL;

	if (!client_ctx || !input_frame)
		return -EINVAL;

	user_vaddr = (unsigned long)input_frame->bufferaddr;

	if (vidc_lookup_addr_table(client_ctx, BUFFER_TYPE_INPUT,
				      true, &user_vaddr, &kernel_vaddr,
				      &phy_addr, &pmem_fd, &file,
				      &buffer_index)) {

		/* kernel_vaddr  is found. send the frame to VCD */
		memset((void *)&vcd_input_buffer, 0,
		       sizeof(struct vcd_frame_data));
		vcd_input_buffer.virtual =
		    (u8 *) (kernel_vaddr + input_frame->offset);
		vcd_input_buffer.offset = 0;
		vcd_input_buffer.frm_clnt_data =
		    (u32) input_frame->client_data;
		vcd_input_buffer.ip_frm_tag =
		    (u32) input_frame->client_data;
		vcd_input_buffer.data_len = input_frame->buffer_len;
		vcd_input_buffer.time_stamp = input_frame->pts;
		/* Rely on VCD using the same flags as OMX */
		vcd_input_buffer.flags = 0;
		vcd_input_buffer.desc_buf = NULL;
		vcd_input_buffer.desc_size = 0;
		if (vcd_input_buffer.data_len > 0) {
			ion_flag = vidc_get_fd_info(client_ctx,
						BUFFER_TYPE_INPUT,
						pmem_fd,
						kernel_vaddr,
						buffer_index,
						&buff_handle);
			if (ion_flag == ION_FLAG_CACHED && buff_handle) {
				msm_ion_do_cache_op(
				client_ctx->user_ion_client,
				buff_handle,
				(unsigned long *)kernel_vaddr,
				(unsigned long) vcd_input_buffer.data_len,
				ION_IOC_CLEAN_CACHES);
			}
		}
		vcd_status = vcd_decode_frame(client_ctx->vcd_handle,
					      &vcd_input_buffer);
		if (!vcd_status)
			return 0;
		else {
			DBG("%s(): vcd_decode_frame failed = %u\n", __func__,
			    vcd_status);
			return -EIO;
		}

	} else {
		DBG("%s(): kernel_vaddr not found\n", __func__);
		return -EIO;
	}
}

static int mpq_int_vid_dec_fill_output_buffer(
		struct video_client_ctx *client_ctx,
		struct video_data_buffer *fill_buffer)
{
	unsigned long kernel_vaddr, phy_addr, user_vaddr;
	int pmem_fd;
	struct file *file;
	s32 buffer_index = -1;
	u32 vcd_status = VCD_ERR_FAIL;
	struct ion_handle *buff_handle = NULL;

	struct vcd_frame_data vcd_frame;

	if (!client_ctx || !fill_buffer)
		return -EINVAL;

	user_vaddr = (unsigned long)fill_buffer->bufferaddr;

	if (vidc_lookup_addr_table(client_ctx, BUFFER_TYPE_OUTPUT,
				      true, &user_vaddr, &kernel_vaddr,
				      &phy_addr, &pmem_fd, &file,
				      &buffer_index)) {

		memset((void *)&vcd_frame, 0,
		       sizeof(struct vcd_frame_data));
		vcd_frame.virtual = (u8 *) kernel_vaddr;
		vcd_frame.frm_clnt_data = (u32) fill_buffer->client_data;
		vcd_frame.alloc_len = fill_buffer->buffer_len;
		vcd_frame.ion_flag = vidc_get_fd_info(client_ctx,
						 BUFFER_TYPE_OUTPUT,
						pmem_fd, kernel_vaddr,
						buffer_index,
						&buff_handle);
		vcd_frame.buff_ion_handle = buff_handle;
		vcd_status = vcd_fill_output_buffer(client_ctx->vcd_handle,
						    &vcd_frame);
		if (!vcd_status)
			return 0;
		else {
			DBG("%s(): vcd_fill_output_buffer failed = %u\n",
			    __func__, vcd_status);
			return -EINVAL;
		}
	} else {
		DBG("%s(): kernel_vaddr not found\n", __func__);
		return -EIO;
	}
}


static int mpq_int_vid_dec_flush(struct video_client_ctx *client_ctx,
			 enum vdec_bufferflush flush_dir)
{
	u32 vcd_status = VCD_ERR_FAIL;

	DBG("msm_vidc_dec: %s() called with dir = %u", __func__,
		 flush_dir);
	if (!client_ctx) {
		DBG("Invalid client_ctx\n");
		return -EINVAL;
	}

	switch (flush_dir) {
	case VDEC_FLUSH_TYPE_INPUT:
		vcd_status = vcd_flush(client_ctx->vcd_handle,
					VCD_FLUSH_INPUT);
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
		return -EINVAL;
		break;
	}

	if (!vcd_status)
		return 0;
	else {
		DBG("%s(): vcd_flush failed. vcd_status = %u "\
		    " flush_dir = %u\n", __func__, vcd_status, flush_dir);
		return -EIO;
	}
}

static u32 mpq_int_vid_dec_msg_pending(struct video_client_ctx *client_ctx)
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
	} else {
		DBG("%s(): vid_dec msg queue Not empty\n", __func__);
	}

	return !islist_empty;
}

static int mpq_int_vid_dec_get_next_msg(struct video_client_ctx *client_ctx,
				struct vdec_msginfo *vdec_msg_info)
{
	struct vid_dec_msg *vid_dec_msg = NULL;

	if (!client_ctx)
		return -EINVAL;

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

static u32 mpq_int_vid_dec_close_client(struct video_client_ctx *client_ctx)
{
	struct vid_dec_msg *vdec_msg;
	u32 vcd_status;

	DBG("msm_vidc_dec: Inside %s()", __func__);
	if (!client_ctx || (!client_ctx->vcd_handle)) {
		DBG("Invalid client_ctx\n");
		return false;
	}

	mutex_lock(&mpq_dvb_video_device->lock);
	if (!client_ctx->stop_called) {
		client_ctx->stop_called = true;
		client_ctx->stop_sync_cb = true;
		vcd_status = vcd_stop(client_ctx->vcd_handle);
		DBG("Stuck at the stop call\n");
		if (!vcd_status)
			wait_for_completion(&client_ctx->event);
		DBG("Came out of wait event\n");
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
		mutex_unlock(&mpq_dvb_video_device->lock);
		return false;
	}
	client_ctx->user_ion_client = NULL;
	mutex_destroy(&client_ctx->msg_queue_lock);
	mutex_destroy(&client_ctx->enrty_queue_lock);
	memset((void *)client_ctx, 0, sizeof(struct video_client_ctx));
	mpq_dvb_video_device->num_clients--;
	mutex_unlock(&mpq_dvb_video_device->lock);
	return true;
}

static int mpq_int_vid_dec_open_client(struct video_client_ctx **vid_clnt_ctx,
					int flags)
{
	int rc = 0;
	s32 client_index;
	struct video_client_ctx *client_ctx = NULL;
	u8 client_count;

	if (!vid_clnt_ctx) {
		DBG("Invalid input\n");
		return -EINVAL;
	}
	*vid_clnt_ctx = NULL;
	client_count = vcd_get_num_of_clients();
	if (client_count == VIDC_MAX_NUM_CLIENTS) {
		ERR("ERROR : vid_dec_open() max number of clients"\
			"limit reached\n");
		return -ENOMEM;
	}

	DBG(" Virtual Address of ioremap is %p\n",
				mpq_dvb_video_device->virt_base);
	if (!mpq_dvb_video_device->num_clients)
		if (!vidc_load_firmware())
			return -ENOMEM;

	client_index = mpq_int_vid_dec_get_empty_client_index();
	if (client_index == -1) {
		DBG("%s() : No free clients client_index == -1\n", __func__);
		vidc_release_firmware();
		return -ENOMEM;
	}
	client_ctx = &mpq_dvb_video_device->vdec_clients[client_index];
	mpq_dvb_video_device->num_clients++;
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
	rc = vcd_open(mpq_dvb_video_device->device_handle, true,
				  mpq_int_vid_dec_vcd_cb, client_ctx, flags);
	if (!rc) {
		wait_for_completion(&client_ctx->event);
		if (client_ctx->event_status) {
			DBG("callback for vcd_open returned error: %u",
				client_ctx->event_status);
			rc = -ENODEV;
			goto client_failure;
		}
	} else {
		DBG("vcd_open returned error: %u", rc);
		goto client_failure;
	}
	client_ctx->seq_header_set = false;
	*vid_clnt_ctx = client_ctx;

	return 0;

client_failure:
	vidc_release_firmware();
	mpq_dvb_video_device->num_clients--;
	mutex_destroy(&client_ctx->msg_queue_lock);
	mutex_destroy(&client_ctx->enrty_queue_lock);
	memset((void *)client_ctx, 0, sizeof(struct video_client_ctx));
	return rc;
}

static int mpq_dvb_video_open(struct inode *inode, struct file *file)
{
	int rc;
	struct mpq_dvb_video_inst *dev_inst   = NULL;
	struct dvb_device         *device     = NULL;

	DBG("Inside %s()", __func__);
	mutex_lock(&mpq_dvb_video_device->lock);

	/* Open the dvb/video instance */
	rc = dvb_generic_open(inode, file);
	if (rc) {
		DBG("Failed in dvb_generic_open with return value :%d\n",
					rc);
		mutex_unlock(&mpq_dvb_video_device->lock);
		return rc;

	}

	device   = (struct dvb_device *)file->private_data;
	dev_inst = (struct mpq_dvb_video_inst *)device->priv;

	if (dev_inst->client_ctx) {
		dvb_generic_release(inode, file);
		mutex_unlock(&mpq_dvb_video_device->lock);
		return -EEXIST;
	}

	rc = mpq_int_vid_dec_open_client(&dev_inst->client_ctx, 0);
	if (rc) {
		dvb_generic_release(inode, file);
		mutex_unlock(&mpq_dvb_video_device->lock);
		return rc;
	}

	if (!dev_inst->client_ctx) {
		dvb_generic_release(inode, file);
		mutex_unlock(&mpq_dvb_video_device->lock);
		return -ENOMEM;
	}

	/* Set default source to memory for easier handling */
	dev_inst->source = VIDEO_SOURCE_MEMORY;

	mutex_unlock(&mpq_dvb_video_device->lock);

	return rc;
}

static int mpq_dvb_video_term_dmx_src(struct mpq_dvb_video_inst *dev_inst)
{

	struct mpq_dmx_src_data *dmx_data = dev_inst->dmx_src_data;

	if (NULL == dmx_data)
		return 0;

	kthread_stop(dmx_data->data_task);
	mutex_destroy(&dmx_data->msg_queue_lock);

	kfree(dmx_data);
	dev_inst->dmx_src_data = NULL;

	return 0;

}

static int mpq_dvb_video_release(struct inode *inode, struct file *file)
{
	struct dvb_device *device = file->private_data;
	struct mpq_dvb_video_inst *dev_inst = device->priv;

	vidc_cleanup_addr_table(dev_inst->client_ctx, BUFFER_TYPE_OUTPUT);
	vidc_cleanup_addr_table(dev_inst->client_ctx, BUFFER_TYPE_INPUT);
	if (dev_inst->source == VIDEO_SOURCE_DEMUX)
		mpq_dvb_video_term_dmx_src(dev_inst);
	mpq_int_vid_dec_close_client(dev_inst->client_ctx);
	memset((void *)dev_inst, 0, sizeof(struct mpq_dvb_video_inst));
	vidc_release_firmware();
	dvb_generic_release(inode, file);
	return 0;
}

static void *mpq_int_vid_dec_map_dev_base_addr(void *device_name)
{
	return mpq_dvb_video_device->virt_base;
}

static int mpq_int_vid_dec_vcd_init(void)
{
	int rc;
	struct vcd_init_config vcd_init_config;
	u32 i;

	/* init_timer(&hw_timer); */
	DBG("msm_vidc_dec: Inside %s()", __func__);
	mpq_dvb_video_device->num_clients = 0;

	for (i = 0; i < VIDC_MAX_NUM_CLIENTS; i++) {
		memset((void *)&mpq_dvb_video_device->vdec_clients[i], 0,
		       sizeof(mpq_dvb_video_device->vdec_clients[i]));
	}

	mutex_init(&mpq_dvb_video_device->lock);
	mpq_dvb_video_device->virt_base = vidc_get_ioaddr();
	DBG("%s() : base address for VIDC core %u\n", __func__, \
		(int)mpq_dvb_video_device->virt_base);

	if (!mpq_dvb_video_device->virt_base) {
		DBG("%s() : ioremap failed\n", __func__);
		return -ENOMEM;
	}

	vcd_init_config.device_name = "MPQ_VIDC";
	vcd_init_config.map_dev_base_addr = mpq_int_vid_dec_map_dev_base_addr;
	vcd_init_config.interrupt_clr = NULL;
	vcd_init_config.register_isr = NULL;
	vcd_init_config.deregister_isr = NULL;
	vcd_init_config.timer_create = vidc_timer_create;
	vcd_init_config.timer_release = vidc_timer_release;
	vcd_init_config.timer_start = vidc_timer_start;
	vcd_init_config.timer_stop = vidc_timer_stop;

	rc = vcd_init(&vcd_init_config,
			&mpq_dvb_video_device->device_handle);

	if (rc) {
		DBG("%s() : vcd_init failed\n", __func__);
		mutex_destroy(&mpq_dvb_video_device->lock);
		return -ENODEV;
	}
	return 0;
}

static int mpq_int_vdec_get_fps(struct video_client_ctx *client_ctx,
				unsigned int *fps)
{
	struct vcd_property_hdr vcd_property_hdr;
	struct vcd_property_frame_rate vcd_frame_rate;
	u32 vcd_status = VCD_ERR_FAIL;

	if (NULL == fps)
		return -EINVAL;

	vcd_property_hdr.prop_id = VCD_I_FRAME_RATE;
	vcd_property_hdr.sz = sizeof(struct vcd_property_frame_rate);
	vcd_frame_rate.fps_numerator = 0;
	vcd_frame_rate.fps_denominator = 1;

	*fps = 0;
	vcd_status = vcd_get_property(client_ctx->vcd_handle,
				      &vcd_property_hdr, &vcd_frame_rate);

	if (vcd_status)
		return -EINVAL;
	else {
		*fps = (vcd_frame_rate.fps_numerator * 1000)
			/(vcd_frame_rate.fps_denominator);
		return 0;
	}
}

static int mpq_dvb_video_command_handler(struct mpq_dvb_video_inst *dev_inst,
					void *parg)
{
	struct video_client_ctx *client_ctx = dev_inst->client_ctx;
	struct video_command *cmd = parg;
	int rc = 0;

	if (cmd == NULL)
		return -EINVAL;

	switch (cmd->cmd) {
	case VIDEO_CMD_SET_CODEC:
		DBG("cmd : VIDEO_CMD_SET_CODEC\n");
		rc = mpq_int_vid_dec_set_codec(client_ctx, cmd->codec);
		break;
	case VIDEO_CMD_GET_CODEC:
		DBG("cmd : VIDEO_CMD_GET_CODEC\n");
		rc = mpq_int_vid_dec_get_codec(client_ctx, &cmd->codec);
		break;
	case VIDEO_CMD_SET_OUTPUT_FORMAT:
		DBG("cmd : VIDEO_CMD_SET_OUTPUT_FORMAT\n");
		rc = mpq_int_vid_dec_set_output_format(client_ctx,
							cmd->format);
		break;
	case VIDEO_CMD_GET_OUTPUT_FORMAT:
		DBG("cmd : VIDEO_CMD_GET_OUTPUT_FORMAT\n");
		rc = mpq_int_vid_dec_get_output_format(client_ctx,
							&cmd->format);
		break;
	case VIDEO_CMD_GET_PIC_RES:
		DBG("cmd : VIDEO_CMD_GET_PIC_RES\n");
		rc = mpq_int_vid_dec_get_frame_resolution(client_ctx,
						&cmd->frame_res);
		break;
	case VIDEO_CMD_SET_INPUT_BUFFERS:
		DBG("cmd : VIDEO_CMD_SET_INPUT_BUFFERS\n");
		rc = mpq_int_vid_dec_set_buffer(dev_inst, &cmd->buffer,
						BUFFER_TYPE_INPUT);
		break;
	case VIDEO_CMD_SET_OUTPUT_BUFFERS:
		DBG("cmd : VIDEO_CMD_SET_OUTPUT_BUFFERS\n");
		rc = mpq_int_vid_dec_set_buffer(dev_inst, &cmd->buffer,
						BUFFER_TYPE_OUTPUT);
		break;
	case VIDEO_CMD_FREE_INPUT_BUFFERS:
		DBG("cmd : VIDEO_CMD_FREE_INPUT_BUFFERS\n");
		rc = mpq_int_vid_dec_free_buffer(client_ctx, &cmd->buffer,
						BUFFER_TYPE_INPUT);
		break;
	case VIDEO_CMD_FREE_OUTPUT_BUFFERS:
		DBG("cmd : VIDEO_CMD_FREE_OUTPUT_BUFFERS\n");
		rc = mpq_int_vid_dec_free_buffer(client_ctx, &cmd->buffer,
							BUFFER_TYPE_OUTPUT);
		break;
	case VIDEO_CMD_GET_BUFFER_REQ:
		DBG("cmd : VIDEO_CMD_GET_BUFFER_REQ\n");
		rc = mpq_int_vid_dec_get_buffer_req(client_ctx, &cmd->buf_req);
		break;
	case VIDEO_CMD_SET_BUFFER_COUNT:
		DBG("cmd : VIDEO_CMD_SET_BUFFER_COUNT\n");
		rc = mpq_int_vid_dec_set_buffer_req(client_ctx, cmd->buf_req);
		break;
	case VIDEO_CMD_READ_RAW_OUTPUT:
		DBG("cmd : VIDEO_CMD_READ_RAW_OUTPUT\n");
		rc = mpq_int_vid_dec_fill_output_buffer(client_ctx,
							&cmd->buffer);
		break;
	case VIDEO_CMD_SET_H264_MV_BUFFER:
		DBG("cmd : VIDEO_CMD_SET_H264_MV_BUFFER\n");
		rc = mpq_int_vid_dec_set_h264_mv_buffers(client_ctx,
							&cmd->mv_buffer_prop);
		break;
	case VIDEO_CMD_GET_H264_MV_BUFFER:
		DBG("cmd : VIDEO_CMD_GET_H264_MV_BUFFER\n");
		rc = mpq_int_vid_dec_get_h264_mv_buffer_size(client_ctx,
							&cmd->mv_buffer_req);
		break;
	case VIDEO_CMD_FREE_H264_MV_BUFFER:
		DBG("cmd : VIDEO_CMD_FREE_H264_MV_BUFFER\n");
		rc = mpq_int_vid_dec_free_h264_mv_buffers(client_ctx);
		break;
	case VIDEO_CMD_CLEAR_INPUT_BUFFER:
		DBG("cmd : VIDEO_CMD_CLEAR_INPUT_BUFFER\n");
		rc = mpq_int_vid_dec_flush(client_ctx, VDEC_FLUSH_TYPE_INPUT);
		break;
	case VIDEO_CMD_CLEAR_OUTPUT_BUFFER:
		DBG("cmd : VIDEO_CMD_CLEAR_OUTPUT_BUFFER\n");
		rc = mpq_int_vid_dec_flush(client_ctx, VDEC_FLUSH_TYPE_OUTPUT);
		break;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}


static ssize_t mpq_dvb_video_write(struct file *file, const char __user *buf,
					size_t count, loff_t *ppos)
{
	int rc = 0;
	struct dvb_device *device = file->private_data;
	struct mpq_dvb_video_inst *dev_inst = NULL;

	struct video_data_buffer *input_frame =
				(struct video_data_buffer *)buf;

	if ((device == NULL) || (input_frame == NULL))
		return -EINVAL;

	dev_inst = device->priv;
	if (dev_inst == NULL)
		return -EINVAL;

	rc = mpq_int_vid_dec_decode_frame(dev_inst->client_ctx, input_frame);
	if (rc)
		return -EIO;

	return input_frame->buffer_len;
}

static int mpq_dvb_video_get_event(struct video_client_ctx *client_ctx,
				struct video_event *ev)
{
	int rc;
	struct vdec_msginfo vdec_msg_info = {};

	memset(ev, 0, sizeof(struct video_event));

	rc = mpq_int_vid_dec_get_next_msg(client_ctx, &vdec_msg_info);
	if (rc)
		return rc;

	ev->status = vdec_msg_info.status_code;
	/* Map the Message here */
	switch (vdec_msg_info.msgcode) {
	case VDEC_MSG_INVALID:
		DBG("VDEC_MSG_INVALID\n");
		break;
	case VDEC_MSG_RESP_INPUT_BUFFER_DONE:
		DBG("VIDEO_EVENT_INPUT_BUFFER_DONE\n");
		ev->type = VIDEO_EVENT_INPUT_BUFFER_DONE;
		ev->u.buffer.client_data =
				vdec_msg_info.msgdata.input_frame_clientdata;
		break;
	case VDEC_MSG_RESP_OUTPUT_BUFFER_DONE:
		DBG("VIDEO_EVENT_OUTPUT_BUFFER_DONE\n");
		ev->type = VIDEO_EVENT_OUTPUT_BUFFER_DONE;
		ev->u.buffer.bufferaddr  =
				vdec_msg_info.msgdata.output_frame.bufferaddr;
		ev->u.buffer.buffer_len  =
				vdec_msg_info.msgdata.output_frame.len;
		ev->u.buffer.ip_buffer_tag = vdec_msg_info.msgdata.\
					output_frame.input_frame_clientdata;
		ev->u.buffer.client_data = vdec_msg_info.msgdata.\
					output_frame.client_data;
		ev->u.buffer.pts         =
				vdec_msg_info.msgdata.output_frame.time_stamp;
		ev->u.buffer.offset      =
				vdec_msg_info.msgdata.output_frame.offset;
		ev->u.buffer.interlaced_format = map_scan_type(vdec_msg_info.\
				msgdata.output_frame.interlaced_format);
		break;
	case VDEC_MSG_RESP_START_DONE:
		DBG("VIDEO_EVENT_DECODER_PLAYING\n");
		ev->type = VIDEO_EVENT_DECODER_PLAYING;
		break;
	case VDEC_MSG_RESP_STOP_DONE:
		DBG("VIDEO_EVENT_DECODER_FREEZED\n");
		ev->type = VIDEO_EVENT_DECODER_STOPPED;
		break;
	case VDEC_MSG_RESP_PAUSE_DONE:
		DBG("VDEC_MSG_RESP_PAUSE_DONE\n");
		ev->type = VIDEO_EVENT_DECODER_FREEZED;
		break;
	case VDEC_MSG_RESP_RESUME_DONE:
		DBG("VIDEO_EVENT_DECODER_RESUMED\n");
		ev->type = VIDEO_EVENT_DECODER_RESUMED;
		break;
	case VDEC_MSG_EVT_CONFIG_CHANGED:
	case VDEC_MSG_EVT_INFO_CONFIG_CHANGED:
		DBG("VIDEO_EVENT_SEQ_HDR_FOUND\n");
		ev->type = VIDEO_EVENT_SEQ_HDR_FOUND;
		break;
	case VDEC_MSG_RESP_FLUSH_OUTPUT_DONE:
		DBG("VIDEO_EVENT_OUTPUT_FLUSH_DONE\n");
		ev->type = VIDEO_EVENT_OUTPUT_FLUSH_DONE;
		break;
	case VDEC_MSG_RESP_OUTPUT_FLUSHED:
		DBG("VIDEO_EVENT_OUTPUT_FLUSHED\n");
		ev->type = VIDEO_EVENT_OUTPUT_FLUSHED;
		ev->u.buffer.bufferaddr  =
				vdec_msg_info.msgdata.output_frame.bufferaddr;
		ev->u.buffer.buffer_len  =
				vdec_msg_info.msgdata.output_frame.len;
		ev->u.buffer.ip_buffer_tag = vdec_msg_info.msgdata.\
				output_frame.input_frame_clientdata;
		ev->u.buffer.client_data = vdec_msg_info.msgdata.\
				output_frame.client_data;
		ev->u.buffer.pts         =
				vdec_msg_info.msgdata.output_frame.time_stamp;
		ev->u.buffer.offset      =
				vdec_msg_info.msgdata.output_frame.offset;
		ev->u.buffer.interlaced_format = map_scan_type(vdec_msg_info.\
				msgdata.output_frame.interlaced_format);
		break;
	case VDEC_MSG_RESP_FLUSH_INPUT_DONE:
		DBG("VIDEO_EVENT_INPUT_FLUSH_DONE\n");
		ev->type = VIDEO_EVENT_INPUT_FLUSH_DONE;
		break;
	case VDEC_MSG_RESP_INPUT_FLUSHED:
		DBG("VIDEO_EVENT_INPUT_FLUSHED\n");
		ev->type = VIDEO_EVENT_INPUT_FLUSHED;
		ev->u.buffer.client_data =
				vdec_msg_info.msgdata.input_frame_clientdata;
		break;
	}
	return 0;
}

static enum scan_format map_scan_type(enum vdec_interlaced_format type)
{
	if (type == VDEC_InterlaceFrameProgressive)
		return INTERLACE_FRAME_PROGRESSIVE;
	if (type == VDEC_InterlaceInterleaveFrameTopFieldFirst)
		return INTERLACE_INTERLEAVE_FRAME_TOP_FIELD_FIRST;
	if (type == VDEC_InterlaceInterleaveFrameBottomFieldFirst)
		return INTERLACE_INTERLEAVE_FRAME_BOTTOM_FIELD_FIRST;
	return INTERLACE_FRAME_PROGRESSIVE;
}

static int mpq_dvb_video_play(struct mpq_dvb_video_inst *dev_inst)
{
	return mpq_int_vid_dec_start_stop(dev_inst->client_ctx, true);
}

static int mpq_dvb_video_stop(struct video_client_ctx *client_ctx)
{
	return mpq_int_vid_dec_start_stop(client_ctx, false);
}

static void mpq_dvb_video_get_stream_if(
				enum mpq_adapter_stream_if interface_id,
				void *arg)
{
	struct mpq_dvb_video_inst *dev_inst = arg;

	DBG("In mpq_dvb_video_get_stream_if : %d\n", interface_id);

	mpq_adapter_get_stream_if(interface_id,
			&dev_inst->dmx_src_data->stream_buffer);

	wake_up(&dev_inst->dmx_src_data->msg_wait);
}

static int mpq_dvb_video_init_dmx_src(struct mpq_dvb_video_inst *dev_inst,
					int device_id)
{
	int rc;

	dev_inst->dmx_src_data =  kzalloc(sizeof(struct mpq_dmx_src_data),
						GFP_KERNEL);
	if (dev_inst->dmx_src_data == NULL)
		return -ENOMEM;

	rc = mpq_adapter_get_stream_if(
		(enum mpq_adapter_stream_if)device_id,
		&dev_inst->dmx_src_data->stream_buffer);

	if (rc) {
		kfree(dev_inst->dmx_src_data);
		return -ENODEV;
	} else if (dev_inst->dmx_src_data->stream_buffer == NULL) {
		DBG("Stream Buffer is NULL. Resigtering Notifier.\n");
		rc = mpq_adapter_notify_stream_if(
			(enum mpq_adapter_stream_if)device_id,
			mpq_dvb_video_get_stream_if,
			(void *)dev_inst);
		if (rc) {
			kfree(dev_inst->dmx_src_data);
			return -ENODEV;
		}
	}

	mutex_init(&dev_inst->dmx_src_data->msg_queue_lock);
	INIT_LIST_HEAD(&dev_inst->dmx_src_data->msg_queue);
	init_waitqueue_head(&dev_inst->dmx_src_data->msg_wait);

	dev_inst->dmx_src_data->data_task = kthread_run(
			mpq_bcast_data_handler,	(void *)dev_inst,
			vid_thread_names[device_id]);

	return 0;
}

static int mpq_dvb_video_set_source(struct dvb_device *device,
				video_stream_source_t source)
{
	int rc = 0;
	struct mpq_dvb_video_inst *dev_inst =
			(struct mpq_dvb_video_inst *)device->priv;

	if (dev_inst->source == source)
		return rc;

	if ((VIDEO_SOURCE_MEMORY == source) &&
		(VIDEO_SOURCE_DEMUX == dev_inst->source))
		mpq_dvb_video_term_dmx_src(dev_inst);

	dev_inst->source = source;
	if (VIDEO_SOURCE_DEMUX == source)
		rc = mpq_dvb_video_init_dmx_src(dev_inst, device->id);

	return rc;
}

static int mpq_dvb_video_ioctl(struct file *file,
				unsigned int cmd, void *parg)
{
	int rc;
	struct dvb_device *device = (struct dvb_device *)file->private_data;
	struct video_client_ctx *client_ctx = NULL;
	struct mpq_dvb_video_inst *dev_inst = NULL;

	if (device == NULL)
		return -EINVAL;

	dev_inst = (struct mpq_dvb_video_inst *)device->priv;
	if (dev_inst == NULL)
		return -EINVAL;

	client_ctx = (struct video_client_ctx *)dev_inst->client_ctx;
	if (client_ctx == NULL)
		return -EINVAL;

	switch (cmd) {
	case VIDEO_PLAY:
		DBG("ioctl : VIDEO_PLAY\n");
		rc = mpq_dvb_video_play(dev_inst);
		break;
	case VIDEO_STOP:
		DBG("ioctl : VIDEO_STOP\n");
		rc = mpq_dvb_video_stop(client_ctx);
		break;
	case VIDEO_FREEZE:
		DBG("ioctl : VIDEO_FREEZE\n");
		rc = mpq_int_vid_dec_pause_resume(client_ctx, true);
		break;
	case VIDEO_CONTINUE:
		DBG("ioctl : VIDEO_CONTINUE\n");
		rc = mpq_int_vid_dec_pause_resume(client_ctx, false);
		break;
	case VIDEO_CLEAR_BUFFER:
		DBG("ioctl : VIDEO_CLEAR_BUFFER\n");
		rc = mpq_int_vid_dec_flush(client_ctx,
				VDEC_FLUSH_TYPE_ALL);
		break;
	case VIDEO_COMMAND:
	case VIDEO_TRY_COMMAND:
		DBG("ioctl : VIDEO_COMMAND\n");
		rc = mpq_dvb_video_command_handler(dev_inst, parg);
		break;
	case VIDEO_GET_FRAME_RATE:
		DBG("ioctl : VIDEO_GET_FRAME_RATE\n");
		rc = mpq_int_vdec_get_fps(client_ctx,
				(unsigned int *)parg);
		break;
	case VIDEO_GET_EVENT:
		DBG("ioctl : VIDEO_GET_EVENT\n");
		rc = mpq_dvb_video_get_event(client_ctx,
				(struct video_event *)parg);
		break;
	case VIDEO_SELECT_SOURCE:
		DBG("ioctl : VIDEO_SELECT_SOURCE\n");
		rc = mpq_dvb_video_set_source(device,
				(video_stream_source_t)parg);
		break;
	default:
		ERR("Invalid IOCTL\n");
		rc = -EINVAL;
	}

	return rc;

}

static unsigned int mpq_dvb_video_poll(struct file *file, poll_table *wait)
{
	struct dvb_device *device = file->private_data;
	struct video_client_ctx *client_ctx = NULL;
	struct mpq_dvb_video_inst *dev_inst = NULL;
	unsigned int mask = 0;

	DBG("In %s\n", __func__);

	if (device == NULL)
		return -EINVAL;

	dev_inst = device->priv;
	if (dev_inst == NULL)
		return -EINVAL;

	client_ctx = dev_inst->client_ctx;
	if (client_ctx == NULL)
		return -EINVAL;

	poll_wait(file, &client_ctx->msg_wait, wait);
	if (mpq_int_vid_dec_msg_pending(client_ctx))
		mask |= POLLIN;
	else
		mask = 0;

	return mask;
}

/*
 * Driver Registration.
 */
static const struct file_operations mpq_dvb_video_fops = {
	.owner		= THIS_MODULE,
	.write		= mpq_dvb_video_write,
	.unlocked_ioctl	= dvb_generic_ioctl,
	.open		= mpq_dvb_video_open,
	.release	= mpq_dvb_video_release,
	.poll		= mpq_dvb_video_poll,
	.llseek		= noop_llseek,
};

static const struct dvb_device mpq_dvb_video_device_ctrl = {
	.priv		= NULL,
	.users		= 4,
	.readers	= 4,	/* arbitrary */
	.writers	= 4,
	.fops		= &mpq_dvb_video_fops,
	.kernel_ioctl	= mpq_dvb_video_ioctl,
};

static int __init mpq_dvb_video_init(void)
{
	int rc, i = 0, j;

	mpq_dvb_video_device = kzalloc(sizeof(struct mpq_dvb_video_dev),
				GFP_KERNEL);
	if (!mpq_dvb_video_device) {
		ERR("%s Unable to allocate memory for mpq_dvb_video_dev\n",
		       __func__);
		return -ENOMEM;
	}

	mpq_dvb_video_device->mpq_adapter = mpq_adapter_get();
	if (!mpq_dvb_video_device->mpq_adapter) {
		ERR("%s Unable to get MPQ Adapter\n", __func__);
		rc = -ENODEV;
		goto free_region;
	}

	rc = mpq_int_vid_dec_vcd_init();
	if (rc)
		goto free_region;

	for (i = 0; i < DVB_MPQ_NUM_VIDEO_DEVICES; i++) {

		rc = dvb_register_device(mpq_dvb_video_device->mpq_adapter,
				&mpq_dvb_video_device->dev_inst[i].video_dev,
				&mpq_dvb_video_device_ctrl,
				&mpq_dvb_video_device->dev_inst[i],
				DVB_DEVICE_VIDEO);

		if (rc) {
			ERR("Failed in %s with at %d return value :%d\n",
				__func__, i, rc);
			goto free_region;
		}

	}

	return 0;

free_region:
	for (j = 0; j < i; j++)
		dvb_unregister_device(
			mpq_dvb_video_device->dev_inst[j].video_dev);

	kfree(mpq_dvb_video_device);
	return rc;
}

static void __exit mpq_dvb_video_exit(void)
{
	int i;

	for (i = 0; i < DVB_MPQ_NUM_VIDEO_DEVICES; i++)
		dvb_unregister_device(
			mpq_dvb_video_device->dev_inst[i].video_dev);

	mutex_destroy(&mpq_dvb_video_device->lock);
	kfree(mpq_dvb_video_device);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MPQ DVB Video driver");

module_init(mpq_dvb_video_init);
module_exit(mpq_dvb_video_exit);
