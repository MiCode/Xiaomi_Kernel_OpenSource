// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/component.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include <linux/platform_data/mtk_ccd.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include <linux/rpmsg/mtk_ccd_rpmsg.h>
#include <linux/platform_data/mtk_ccd_controls.h>
#include <linux/platform_data/mtk_ccd.h>

#include <linux/types.h>
#include <linux/videodev2.h>
#include <media/videobuf2-v4l2.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-mc.h>
#include <media/v4l2-subdev.h>

#include "mtk_cam.h"
#include "mtk_cam-regs.h"
#include "mtk_cam-smem.h"
#include "mtk_cam-pool.h"

#define MTK_CAM_STOP_HW_TIMEOUT			(33 * USEC_PER_MSEC)

/* FIXME for CIO pad id */
#define MTK_CAM_CIO_PAD_SRC		PAD_SRC_RAW0
#define MTK_CAM_CIO_PAD_SINK		MTK_RAW_SINK

static const struct of_device_id mtk_cam_of_ids[] = {
	{.compatible = "mediatek,camisp",},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_cam_of_ids);

static void mtk_cam_dev_job_done(struct mtk_cam_device *cam,
				 struct mtk_cam_request *req,
				 enum vb2_buffer_state state)
{
	struct media_request_object *obj, *obj_prev;
	unsigned long flags;
	u64 ts_eof = ktime_get_boottime_ns();

	dev_dbg(cam->dev, "job done request:%s frame_seq:%d state:%d ctx_used:%d\n",
		req->req.debug_str, req->frame_seq_no, state, req->ctx_used);
	list_for_each_entry_safe(obj, obj_prev, &req->req.objects, list) {
		struct vb2_buffer *vb;
		struct mtk_cam_buffer *buf;
		struct mtk_cam_video_device *node;

		if (!vb2_request_object_is_buffer(obj))
			continue;
		vb = container_of(obj, struct vb2_buffer, req_obj);
		buf = mtk_cam_vb2_buf_to_dev_buf(vb);
		node = mtk_cam_vbq_to_vdev(vb->vb2_queue);
		spin_lock_irqsave(&node->buf_list_lock, flags);
		list_del(&buf->list);
		spin_unlock_irqrestore(&node->buf_list_lock, flags);
		buf->vbb.sequence = req->frame_seq_no;
		if (V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type))
			vb->timestamp = ts_eof;
		else
			vb->timestamp = req->timestamp;
		vb2_buffer_done(&buf->vbb.vb2_buf, state);
	}
}

struct mtk_cam_request *mtk_cam_dev_get_req(struct mtk_cam_device *cam,
					    struct mtk_cam_ctx *ctx,
					    unsigned int frame_seq_no)
{
	struct mtk_cam_request *req, *req_prev;
	unsigned long flags;

	spin_lock_irqsave(&cam->running_job_lock, flags);
	list_for_each_entry_safe(req, req_prev, &cam->running_job_list, list) {
		dev_dbg(cam->dev, "frame_seq:%d[ctx_used=%d/ctx:%d]\n",
			req->frame_seq_no, req->ctx_used, ctx->stream_id);

		/* Match by the en-queued request number */
		if (req->ctx_used & (1 << ctx->stream_id) &&
		    req->frame_seq_no == frame_seq_no) {
			dev_dbg(cam->dev, "get frame_seq:%d[ctx:%d]\n",
				req->frame_seq_no, ctx->stream_id);
			spin_unlock_irqrestore(&cam->running_job_lock, flags);
			return req;
		}
	}
	spin_unlock_irqrestore(&cam->running_job_lock, flags);

	return NULL;
}

void mtk_cam_dequeue_req_frame(struct mtk_cam_device *cam,
			       struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_request *req, *req_prev;
	struct mtk_camsys_sensor_ctrl *sensor_ctrl =
		&cam->camsys_ctrl.sensor_ctrl[ctx->stream_id];
	unsigned int frame_seq_no = ctx->dequeued_frame_seq_no;
	unsigned long flags;
	enum vb2_buffer_state buf_state = VB2_BUF_STATE_DONE;

	spin_lock_irqsave(&cam->running_job_lock, flags);
	list_for_each_entry_safe(req, req_prev, &cam->running_job_list, list) {
		dev_dbg(cam->dev, "frame_seq:%d[ctx_used=%d], de-queue frame_seq:%d\n",
			req->frame_seq_no, req->ctx_used, frame_seq_no);

		if (!(req->ctx_used & (1 << ctx->stream_id)))
			continue;

		/* Match by the en-queued request number */
		if (req->frame_seq_no == frame_seq_no) {
			cam->running_job_count--;
			if (req->state.estate == E_STATE_DONE_MISMATCH)
				buf_state = VB2_BUF_STATE_ERROR;
			if (ctx->sensor) {
				spin_lock(&sensor_ctrl->camsys_state_lock);
				list_del(&req->state.state_element);
				spin_unlock(&sensor_ctrl->camsys_state_lock);
			}
			mtk_cam_dev_job_done(cam, req, buf_state);

			list_del(&req->list);
			break;
		} else if (req->frame_seq_no < frame_seq_no) {
			cam->running_job_count--;
			/* Pass to user space for frame drop */
			mtk_cam_dev_job_done(cam, req, VB2_BUF_STATE_ERROR);
			dev_dbg(cam->dev, "frame_seq:%d drop\n",
				 req->frame_seq_no);
			list_del(&req->list);
		}
	}
	spin_unlock_irqrestore(&cam->running_job_lock, flags);
}

void mtk_cam_dev_req_cleanup(struct mtk_cam_device *cam)
{
	struct mtk_cam_request *req, *req_prev;
	unsigned long flags;
	unsigned int i;

	/* FIXME we should not clean all requests from each stream */

	dev_dbg(cam->dev, "%s\n", __func__);

	for (i = 0; i < cam->max_stream_num; i++) {
		if (!cam->ctxs[i].streaming) {
			struct list_head *pending = &cam->pending_job_list;
			struct list_head *running = &cam->running_job_list;

			spin_lock_irqsave(&cam->pending_job_lock, flags);
			list_for_each_entry_safe(req, req_prev, pending, list) {
				if (req->ctx_used &
				   (1 << cam->ctxs[i].stream_id) ||
				   !req->ctx_used)
					list_del(&req->list);
			}
			spin_unlock_irqrestore(&cam->pending_job_lock, flags);

			spin_lock_irqsave(&cam->running_job_lock, flags);
			list_for_each_entry_safe(req, req_prev, running, list) {
				if (req->ctx_used &
				    (1 << cam->ctxs[i].stream_id) ||
				    !req->ctx_used)
					list_del(&req->list);
			}
			spin_unlock_irqrestore(&cam->running_job_lock, flags);
		}
	}
}

/* FIXME: should move following to raw's implementation. */
static int config_img_fmt(struct mtk_cam_device *cam,
			  struct mtk_cam_video_device *node,
			  struct mtkcam_ipi_img_output *out_fmt, int sd_width,
			  int sd_height)
{
	const struct v4l2_format *cfg_fmt = &node->active_fmt;

	/* Check output & input image size dimension */
	if (node->desc.dma_port == MTKCAM_IPI_RAW_IMGO &&
	    (cfg_fmt->fmt.pix_mp.width > sd_width ||
			cfg_fmt->fmt.pix_mp.height > sd_height)) {
		dev_dbg(cam->dev, "ctx: %d cfg size is larger than sensor\n",
			node->ctx->stream_id);
		return -EINVAL;
	} else if ((node->desc.dma_port == MTKCAM_IPI_RAW_RRZO) &&
			(((cfg_fmt->fmt.pix_mp.width * 100 / sd_width) <
			MTK_ISP_MIN_RESIZE_RATIO) ||
			((cfg_fmt->fmt.pix_mp.height * 100 / sd_height) <
			MTK_ISP_MIN_RESIZE_RATIO))) {
		dev_dbg(cam->dev, "ctx: %d resize ratio is less than %d%%\n",
			node->ctx->stream_id, MTK_ISP_MIN_RESIZE_RATIO);
		return -EINVAL;
	}

	out_fmt->fmt.format =
		mtk_cam_get_img_fmt(cfg_fmt->fmt.pix_mp.pixelformat);
	if (out_fmt->fmt.format == MTK_CAM_IMG_FMT_UNKNOWN) {
		dev_dbg(cam->dev, "ctx: %d, node:%d unknown pixel fmt:%d\n",
			node->ctx->stream_id, node->desc.dma_port,
			cfg_fmt->fmt.pix_mp.pixelformat);
		return -EINVAL;
	}
	dev_dbg(cam->dev, "ctx: %d node:%d img_fmt:0x%x\n",
		node->ctx->stream_id, node->desc.dma_port, out_fmt->fmt.format);

	out_fmt->fmt.s.w = cfg_fmt->fmt.pix_mp.width;
	out_fmt->fmt.s.h = cfg_fmt->fmt.pix_mp.height;

	/* TODO: support multi-plane stride */
	out_fmt->fmt.stride[0] = cfg_fmt->fmt.pix_mp.plane_fmt[0].bytesperline;

	out_fmt->crop.p.x = 0;
	out_fmt->crop.p.y = 0;
	/*FIXME for crop size */
	out_fmt->crop.s.w = out_fmt->fmt.s.w;
	out_fmt->crop.s.h = out_fmt->fmt.s.h;

	dev_dbg(cam->dev,
		"ctx: %d node:%d size=%0dx%0d, stride:%d, crop=%0dx%0d\n",
		node->ctx->stream_id, node->desc.dma_port, out_fmt->fmt.s.w,
		out_fmt->fmt.s.h, out_fmt->fmt.stride[0], out_fmt->crop.s.w,
		out_fmt->crop.s.h);

	return 0;
}

s32 get_format_request_fd(struct v4l2_pix_format_mplane *fmt_mp)
{
	int field;
	int reserved_fields = 4;
	s32 request_fd = 0;

	for (field = 0; field < reserved_fields; field++) {
		request_fd +=
			fmt_mp->reserved[field] << BITS_PER_BYTE * field;
		fmt_mp->reserved[field] = 0;
	}

	return request_fd;
}

void set_format_request_fd(struct v4l2_pix_format_mplane *fmt_mp, s32 request_fd)
{
	u8 *reserved = fmt_mp->reserved;

	reserved[0] = request_fd & 0x000000FF;
	reserved[1] = request_fd & 0x0000FF00;
	reserved[2] = request_fd & 0x00FF0000;
	reserved[3] = request_fd & 0xFF000000;
}

static int mtk_cam_req_update(struct mtk_cam_device *cam,
			      struct mtk_cam_request *req)
{
	struct media_request_object *obj, *obj_prev;

	dev_dbg(cam->dev, "update request:%s\n", req->req.debug_str);

	list_for_each_entry_safe(obj, obj_prev, &req->req.objects, list) {
		struct vb2_buffer *vb;
		struct mtk_cam_buffer *buf;
		struct mtk_cam_video_device *node;
		struct v4l2_subdev_format sd_fmt;
		int sd_width, sd_height, ret;
		struct mtkcam_ipi_img_output *out_fmt;
		struct v4l2_format *fmt;
		struct media_request *request;
		__s32 fd;

		if (!vb2_request_object_is_buffer(obj))
			continue;
		vb = container_of(obj, struct vb2_buffer, req_obj);
		buf = mtk_cam_vb2_buf_to_dev_buf(vb);
		node = mtk_cam_vbq_to_vdev(vb->vb2_queue);

		/* update buffer format */
		switch (node->desc.dma_port) {
		case MTKCAM_IPI_RAW_IMGO:
			sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
			sd_fmt.pad = PAD_SRC_RAW0;
			ret = v4l2_subdev_call(node->ctx->seninf, pad,
					       get_fmt, NULL, &sd_fmt);
			if (ret) {
				dev_dbg(cam->dev,
					"seninf(%s) g_fmt failed:%d\n",
					node->ctx->seninf->name, ret);
				return ret;
			}

			fd = get_format_request_fd(&node->pending_fmt.fmt.pix_mp);
			if (fd > 0) {
				request = media_request_get_by_fd
						(&cam->media_dev, fd);

				if (request == &req->req) {
					fmt = &node->pending_fmt;
					video_try_fmt(node, fmt);
					node->active_fmt = *fmt;
				}
			}

			sd_width = sd_fmt.format.width;
			sd_height = sd_fmt.format.height;

			out_fmt = &req->frame_params
					.img_outs[node->desc.id-MTK_RAW_SOURCE_BEGIN];
			out_fmt->uid.pipe_id = node->uid.pipe_id;
			out_fmt->uid.id = MTKCAM_IPI_RAW_IMGO;
			ret = config_img_fmt(cam, node, out_fmt,
						sd_width, sd_height);
			if (ret)
				return ret;
			break;

		case MTKCAM_IPI_RAW_RRZO:
			sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
			sd_fmt.pad = PAD_SRC_RAW0;
			ret = v4l2_subdev_call(node->ctx->seninf, pad,
						get_fmt, NULL, &sd_fmt);
			if (ret) {
				dev_dbg(cam->dev,
					"seninf(%s) g_fmt failed:%d\n",
					node->ctx->seninf->name, ret);
				return ret;
			}

			fd = get_format_request_fd(&node->pending_fmt.fmt.pix_mp);
			if (fd > 0) {
				request = media_request_get_by_fd(
					&cam->media_dev, fd);

				if (request == &req->req) {
					fmt = &node->pending_fmt;
					video_try_fmt(node, fmt);
					node->active_fmt = *fmt;
				}
			}

			sd_width = sd_fmt.format.width;
			sd_height = sd_fmt.format.height;

			out_fmt = &req->frame_params
					.img_outs[node->desc.id-MTK_RAW_SOURCE_BEGIN];
			out_fmt->uid.pipe_id = node->uid.pipe_id;
			out_fmt->uid.id = MTKCAM_IPI_RAW_RRZO;
			ret = config_img_fmt(cam, node, out_fmt,
					     sd_width, sd_height);
			if (ret)
				return ret;
			break;

		default:
			dev_dbg(cam->dev, "buffer with invalid port\n");
			break;
		}
	}

	return 0;
}

void mtk_cam_dev_req_try_queue(struct mtk_cam_device *cam)
{
	struct mtk_cam_request *req, *req_prev;
	unsigned long flags;
	int ret;

	if (!cam->streaming_ctx) {
		dev_dbg(cam->dev, "streams are off\n");
		return;
	}

	spin_lock_irqsave(&cam->pending_job_lock, flags);
	list_for_each_entry_safe(req, req_prev, &cam->pending_job_list, list) {
		if (likely(req->ctx_used)) {
			if (cam->running_job_count >=
			    MTK_CAM_MAX_RUNNING_JOBS) {
				dev_dbg(cam->dev, "jobs are full\n");
				break;
			}
			cam->running_job_count++;

			list_del(&req->list);
			spin_lock(&cam->running_job_lock);
			list_add_tail(&req->list, &cam->running_job_list);
			spin_unlock(&cam->running_job_lock);
			ret = mtk_cam_req_update(cam, req);
			if (ret) {
				spin_unlock_irqrestore
					(&cam->pending_job_lock, flags);
				return;
			}
			mtk_cam_dev_req_enqueue(cam, req);
		}
	}
	spin_unlock_irqrestore(&cam->pending_job_lock, flags);
}

static struct media_request *mtk_cam_req_alloc(struct media_device *mdev)
{
	struct mtk_cam_request *cam_req;

	cam_req = kzalloc(sizeof(*cam_req), GFP_KERNEL);

	return &cam_req->req;
}

static void mtk_cam_req_free(struct media_request *req)
{
	struct mtk_cam_request *cam_req = to_mtk_cam_req(req);

	kfree(cam_req);
}

static void mtk_cam_req_queue(struct media_request *req)
{
	struct mtk_cam_request *cam_req = to_mtk_cam_req(req);
	struct mtk_cam_device *cam =
		container_of(req->mdev, struct mtk_cam_device, media_dev);
	struct mtk_camsys_ctrl *camsys_ctrl = &cam->camsys_ctrl;
	unsigned long flags;
	/* update frame_params's dma_bufs in mtk_cam_vb2_buf_queue */
	vb2_request_queue(req);

	/* add to pending job list */
	spin_lock_irqsave(&cam->pending_job_lock, flags);
	list_add_tail(&cam_req->list, &cam->pending_job_list);
	if (camsys_ctrl->link_change_state == LINK_CHANGE_PREPARING) {
		camsys_ctrl->link_change_state = LINK_CHANGE_QUEUED;
		camsys_ctrl->link_change_req = cam_req;
	}
	spin_unlock_irqrestore(&cam->pending_job_lock, flags);

	mtk_cam_dev_req_try_queue(cam);
}

static const struct media_device_ops mtk_cam_dev_ops = {
	.link_notify = v4l2_pipeline_link_notify,
	.req_alloc = mtk_cam_req_alloc,
	.req_free = mtk_cam_req_free,
	.req_validate = vb2_request_validate,
	.req_queue = mtk_cam_req_queue,
};

static int mtk_cam_of_rproc(struct mtk_cam_device *cam)
{
	struct device *dev = cam->dev;
	int ret;

	ret = of_property_read_u32(dev->of_node, "mediatek,ccd",
				   &cam->rproc_phandle);
	if (ret) {
		dev_dbg(dev, "fail to get rproc_phandle:%d\n", ret);
		return -EINVAL;
	}

	return 0;
}

static struct device *mtk_cam_find_raw_dev(struct mtk_cam_device *cam,
					   unsigned int raw_mask)
{
	struct mtk_cam_ctx *ctx;
	unsigned int i;

	for (i = 0; i < cam->num_mtkcam_sub_drivers; i++) {
		if (raw_mask & (1 << i)) {
			ctx = cam->ctxs + i;
			/* FIXME: correct TWIN case */
			return ctx->pipe->raw->devs[i];
		}
	}

	return NULL;
}

static void isp_composer_uninit(struct mtk_cam_device *cam)
{
	struct mtk_ccd *ccd = cam->rproc_handle->priv;

	mtk_destroy_client_msgdevice(ccd->rpmsg_subdev, &cam->rpmsg_channel);
	cam->rpmsg_dev = NULL;
	mtk_cam_working_buf_pool_release(cam);
	rproc_shutdown(cam->rproc_handle);
	rproc_put(cam->rproc_handle);
	cam->rproc_handle = NULL;
}

static int isp_composer_handler(struct rpmsg_device *rpdev, void *data,
				int len, void *priv, u32 src)
{
	struct mtk_cam_device *cam = (struct mtk_cam_device *)priv;
	struct device *dev = cam->dev;
	struct mtkcam_ipi_event *ipi_msg;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_working_buf_entry *buf_entry;
	unsigned long flags;

	ipi_msg = (struct mtkcam_ipi_event *)data;
	if (!ipi_msg)
		return -EINVAL;

	if (len < offsetofend(struct mtkcam_ipi_event, ack_data)) {
		dev_dbg(dev, "wrong IPI len:%d\n", len);
		return -EINVAL;
	}

	if (ipi_msg->cmd_id != CAM_CMD_ACK ||
	    (ipi_msg->ack_data.ack_cmd_id != CAM_CMD_FRAME &&
	     ipi_msg->ack_data.ack_cmd_id != CAM_CMD_DESTROY_SESSION))
		return -EINVAL;

	if (ipi_msg->ack_data.ack_cmd_id == CAM_CMD_FRAME) {
		ctx = &cam->ctxs[ipi_msg->cookie.session_id];
		spin_lock(&ctx->using_buffer_list.lock);
		ctx->composed_frame_seq_no = ipi_msg->cookie.frame_no;
		dev_dbg(dev, "ctx:%d ack frame_num:%d\n",
			ctx->stream_id, ctx->composed_frame_seq_no);

		/* get from using list */
		if (list_empty(&ctx->using_buffer_list.list)) {
			spin_unlock(&ctx->using_buffer_list.lock);
			return -EINVAL;
		}
		buf_entry = list_first_entry(&ctx->using_buffer_list.list,
					     struct mtk_cam_working_buf_entry,
					     list_entry);
		list_del(&buf_entry->list_entry);
		ctx->using_buffer_list.cnt--;
		buf_entry->cq_size =
			ipi_msg->ack_data.frame_result.cq_desc_size;
		dev_dbg(dev, "composed_size:%d\n", buf_entry->cq_size);

		if (ctx->composed_frame_seq_no == 1) {
			struct mtk_cam_request *req;
			struct mtk_raw_device *raw_dev;
			struct device *dev;

			spin_lock_irqsave(&ctx->processing_buffer_list.lock,
					  flags);
			list_add_tail(&buf_entry->list_entry,
				      &ctx->processing_buffer_list.list);
			ctx->processing_buffer_list.cnt++;
			spin_unlock_irqrestore
				(&ctx->processing_buffer_list.lock, flags);
			spin_unlock(&ctx->using_buffer_list.lock);
			req = mtk_cam_dev_get_req(cam, ctx,
						  ctx->composed_frame_seq_no);

			dev = mtk_cam_find_raw_dev(cam, req->ctx_used);
			if (!dev) {
				dev_dbg(dev, "frm#1 raw device not found\n");
				return -EINVAL;
			}

			raw_dev = dev_get_drvdata(dev);
			apply_cq(raw_dev,
				 buf_entry->buffer.iova,
				buf_entry->cq_size, 1);
			return 0;
		}
		spin_lock_irqsave(&ctx->composed_buffer_list.lock,
				  flags);
		list_add_tail(&buf_entry->list_entry,
			      &ctx->composed_buffer_list.list);
		ctx->composed_buffer_list.cnt++;
		spin_unlock_irqrestore(&ctx->composed_buffer_list.lock, flags);
		spin_unlock(&ctx->using_buffer_list.lock);
	} else if (ipi_msg->ack_data.ack_cmd_id == CAM_CMD_DESTROY_SESSION) {
		if (cam->rproc_handle && !cam->composer_cnt) {
			drain_workqueue(cam->composer_wq);
			destroy_workqueue(cam->composer_wq);
			cam->composer_wq = NULL;
			drain_workqueue(cam->link_change_wq);
			destroy_workqueue(cam->link_change_wq);
			cam->link_change_wq = NULL;
		}
	}

	return 0;
}

static void isp_tx_frame_worker(struct work_struct *work)
{
	struct mtkcam_ipi_event event;
	struct mtkcam_ipi_session_cookie *session = &event.cookie;
	struct mtkcam_ipi_frame_param *frame_data = &event.frame_data;
	struct mtk_cam_request *req =
		container_of(work, struct mtk_cam_request, frame_work);
	struct mtkcam_ipi_frame_param *frame_params = &req->frame_params;
	struct mtk_cam_device *cam =
		container_of(req->req.mdev, struct mtk_cam_device, media_dev);
	unsigned int i;
	struct mtk_cam_working_buf_entry *buf_entry;

	memset(&event, 0, sizeof(event));
	event.cmd_id = CAM_CMD_FRAME;

	/* FIXME: handle 1 request to 2 stream mapping */
	for (i = 0; i < cam->max_stream_num; i++) {
		if ((1 << cam->ctxs[i].stream_id) & req->ctx_used) {
			session->session_id =
				cam->ctxs[i].stream_id;

			/* prepare working buffer */
			buf_entry = mtk_cam_working_buf_get(cam);
			if (!buf_entry) {
				pr_info("%s: no free working buffer available\n",
					__func__);
				return;
			}

			/* put to using list */
			spin_lock(&cam->ctxs[i].using_buffer_list.lock);
			list_add_tail(&buf_entry->list_entry,
				      &cam->ctxs[i].using_buffer_list.list);
			cam->ctxs[i].using_buffer_list.cnt++;
			spin_unlock(&cam->ctxs[i].using_buffer_list.lock);
		}
	}
	session->frame_no = req->frame_seq_no;
	memcpy(frame_data, frame_params, sizeof(*frame_params));

	/* TODO: save to relative ctx using buffer list */
	frame_data->cur_workbuf_offset = buf_entry->buffer.addr_offset;
	frame_data->cur_workbuf_size = buf_entry->buffer.size;

	rpmsg_send(cam->rpmsg_dev->rpdev.ept, &event, sizeof(event));
	dev_dbg(cam->dev, "rpmsg_send id: %d\n", event.cmd_id);
}

void mtk_cam_dev_req_enqueue(struct mtk_cam_device *cam,
			     struct mtk_cam_request *req)
{
	struct mtk_cam_ctx *ctx;
	unsigned int i;
	/* Accumulated frame sequence number */
	for (i = 0; i < cam->max_stream_num; i++) {
		if (req->ctx_used & (1 << cam->ctxs[i].stream_id)) {
			ctx = &cam->ctxs[i];

			/* FIXME: for 1 request 2 stream case */
			req->frame_seq_no = ++ctx->enqueued_frame_seq_no;
			req->state.estate = E_STATE_READY;
			if (ctx->sensor && req->frame_seq_no == 1)
				mtk_cam_initial_sensor_setup(req, ctx);
			break;
		}
	}
	INIT_WORK(&req->frame_work, isp_tx_frame_worker);
	queue_work(cam->composer_wq, &req->frame_work);
	dev_dbg(cam->dev, "enqueue fd:%s frame_seq_no:%d job cnt:%d\n",
		req->req.debug_str, req->frame_seq_no,
		cam->running_job_count);
}

void isp_composer_create_session(struct mtk_cam_device *cam,
				 struct mtk_cam_ctx *ctx)
{
	struct mtkcam_ipi_event event;
	struct mtkcam_ipi_session_cookie *session = &event.cookie;
	struct mtkcam_ipi_session_param	*session_data = &event.session_data;

	memset(&event, 0, sizeof(event));
	event.cmd_id = CAM_CMD_CREATE_SESSION;
	session->session_id = ctx->stream_id;
	session_data->workbuf.iova = cam->working_buf_mem_iova;
	/* TODO: separated with SCP case */
	session_data->workbuf.ccd_fd = cam->working_buf_mem_fd;
	session_data->workbuf.size = cam->working_buf_mem_size;

	rpmsg_send(cam->rpmsg_dev->rpdev.ept, &event, sizeof(event));
	dev_dbg(cam->dev, "rpmsg_send id: %d\n", event.cmd_id);
}

static void isp_composer_destroy_session(struct mtk_cam_device *cam,
					 struct mtk_cam_ctx *ctx)
{
	struct mtkcam_ipi_event event;
	struct mtkcam_ipi_session_cookie *session = &event.cookie;

	memset(&event, 0, sizeof(event));
	event.cmd_id = CAM_CMD_DESTROY_SESSION;
	session->session_id = ctx->stream_id;
	rpmsg_send(cam->rpmsg_dev->rpdev.ept, &event, sizeof(event));
	dev_dbg(cam->dev, "rpmsg_send id: %d\n", event.cmd_id);
}

static void
isp_composer_hw_config(struct mtk_cam_device *cam,
		       struct mtk_cam_ctx *ctx,
		       struct mtkcam_ipi_config_param *config_param)
{
	struct mtkcam_ipi_event event;
	struct mtkcam_ipi_session_cookie *session = &event.cookie;
	struct mtkcam_ipi_config_param *config = &event.config_data;

	memset(&event, 0, sizeof(event));
	event.cmd_id = CAM_CMD_CONFIG;
	session->session_id = ctx->stream_id;
	memcpy(config, config_param, sizeof(*config_param));
	rpmsg_send(cam->rpmsg_dev->rpdev.ept, &event, sizeof(event));
	dev_dbg(cam->dev, "rpmsg_send id: %d\n", event.cmd_id);
}

static int
mtk_cam_raw_pipeline_config(struct mtk_cam_ctx *ctx,
			    struct mtkcam_ipi_input_param *cfg_in_param)
{
	struct mtk_raw_pipeline *pipe = ctx->pipe;
	struct mtk_raw *raw = pipe->raw;
	unsigned int i;
	int ret;

	/* reset pm_runtime during streaming dynamic change */
	if (ctx->streaming) {
		for (i = 0; i < ARRAY_SIZE(raw->devs); i++)
			if (pipe->enabled_raw & 1 << i)
				pm_runtime_put(raw->devs[i]);
	}

	ret = mtk_cam_raw_select(pipe, cfg_in_param);
	if (ret) {
		dev_dbg(raw->cam_dev, "failed select raw: %d\n",
			ctx->stream_id);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(raw->devs); i++)
		if (pipe->enabled_raw & 1 << i)
			pm_runtime_get_sync(raw->devs[i]);

	if (ret < 0) {
		dev_dbg(raw->cam_dev,
			"failed at pm_runtime_get_sync: %s\n",
			dev_driver_string(raw->devs[i]));
		for (i = i - 1; i >= 0; i--)
			if (pipe->enabled_raw & 1 << i)
				pm_runtime_put(raw->devs[i]);
		return ret;
	}

	ctx->used_raw_dev = pipe->enabled_raw;
	dev_dbg(raw->cam_dev, "ctx_id %d used_raw_dev %d pipe_id %d\n",
		ctx->stream_id, ctx->used_raw_dev, pipe->id);
	return 0;
}

/* FIXME: modified from v5: should move to raw */
int mtk_cam_dev_config(struct mtk_cam_ctx *ctx, unsigned int streaming)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct device *dev = cam->dev;
	struct mtkcam_ipi_config_param config_param;
	struct mtkcam_ipi_input_param *cfg_in_param;
	int pixel_mode;
	struct mtk_raw_pipeline *pipe = ctx->pipe;
	struct mtk_raw *raw = pipe->raw;
	struct v4l2_mbus_framefmt *mf = &pipe->cfg[MTK_RAW_SINK].mbus_fmt;
	struct device *dev_raw;
	struct mtk_raw_device *raw_dev;
	struct v4l2_format *img_fmt;
	unsigned int i;
	int ret;

	memset(&config_param, 0, sizeof(config_param));

	/* Update cfg_in_param */
	cfg_in_param = &config_param.input;
	mtk_cam_seninf_get_pixelmode(ctx->seninf, PAD_SRC_RAW0, &pixel_mode);
	cfg_in_param->pixel_mode = pixel_mode;
	/* TODO: data pattern from meta buffer per frame setting */
	cfg_in_param->data_pattern = MTKCAM_IPI_SENSOR_PATTERN_NORMAL;
	img_fmt = &pipe->vdev_nodes[MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM]
		.active_fmt;
	cfg_in_param->in_crop.s.w = ALIGN(img_fmt->fmt.pix_mp.width, 4);
	cfg_in_param->in_crop.s.h = ALIGN(img_fmt->fmt.pix_mp.height, 4);
	dev_dbg(dev, "sink pad code:0x%x\n", mf->code);
	cfg_in_param->raw_pixel_id = mtk_cam_get_sensor_pixel_id(mf->code);
	cfg_in_param->fmt = mtk_cam_get_sensor_fmt(mf->code);
	if (cfg_in_param->fmt == MTK_CAM_IMG_FMT_UNKNOWN ||
	    cfg_in_param->raw_pixel_id == MTK_CAM_RAW_PXL_ID_UNKNOWN) {
		dev_dbg(dev, "unknown sd code:%d\n", mf->code);
		return -EINVAL;
	}

	ret = mtk_cam_raw_pipeline_config(ctx, cfg_in_param);
	if (ret)
		return ret;

	config_param.n_maps = 1;
	config_param.maps[0].pipe_id = ctx->pipe->id;
	config_param.maps[0].dev_mask = ctx->used_raw_dev;

	dev_raw = mtk_cam_find_raw_dev(cam, ctx->used_raw_dev);
	if (!dev_raw) {
		dev_dbg(dev, "config raw device not found\n");
		return -EINVAL;
	}
	raw_dev = dev_get_drvdata(dev_raw);
	for (i = 0; i < RAW_PIPELINE_NUM; i++)
		if (raw->pipelines[i].enabled_raw & 1 << raw_dev->id) {
			/* FIXME: TWIN case */
			raw_dev->pipeline = &raw->pipelines[i];
			break;
		}

	if (!streaming)
		reset(raw_dev);
	isp_composer_hw_config(cam, ctx, &config_param);
	dev_dbg(dev, "raw %d %s done\n", raw_dev->id, __func__);

	return 0;
}

static int isp_composer_init(struct mtk_cam_device *cam)
{
	struct device *dev = cam->dev;
	struct mtk_ccd *ccd;
	struct rproc_subdev *rpmsg_subdev;
	struct rpmsg_channel_info *msg = &cam->rpmsg_channel;
	int ret;

	cam->rproc_handle = rproc_get_by_phandle(cam->rproc_phandle);
	if (!cam->rproc_handle) {
		dev_dbg(dev, "fail to get rproc_handle\n");
		return -EINVAL;
	}

	ret = rproc_boot(cam->rproc_handle);
	if (ret) {
		dev_dbg(dev, "failed to rproc_boot:%d\n", ret);
		goto fail_rproc_put;
	}

	ret = mtk_cam_working_buf_pool_init(cam);
	if (ret) {
		dev_dbg(dev, "failed to reserve DMA memory:%d\n", ret);
		goto fail_shutdown;
	}

	ccd = (struct mtk_ccd *)cam->rproc_handle->priv;
	rpmsg_subdev = ccd->rpmsg_subdev;

	snprintf(msg->name, RPMSG_NAME_SIZE, "mtk-camsys\0\n");
	msg->src = CCD_IPI_ISP_MAIN;

	cam->rpmsg_dev = mtk_create_client_msgdevice(rpmsg_subdev, msg);
	if (!cam->rpmsg_dev) {
		ret = -EINVAL;
		goto fail_mem_uninit;
	}
	cam->rpmsg_dev->rpdev.ept = rpmsg_create_ept(&cam->rpmsg_dev->rpdev,
						     isp_composer_handler,
						     cam, *msg);
	if (IS_ERR(cam->rpmsg_dev->rpdev.ept)) {
		ret = -EINVAL;
		goto fail_mem_uninit;
	}

	cam->composer_wq =
		alloc_ordered_workqueue(dev_name(cam->dev),
					__WQ_LEGACY | WQ_MEM_RECLAIM |
					WQ_FREEZABLE);
	if (!cam->composer_wq) {
		dev_dbg(dev, "failed to alloc composer workqueue\n");
		ret = -ENOMEM;
		goto fail_mem_uninit;
	}

	cam->link_change_wq =
		alloc_ordered_workqueue(dev_name(cam->dev),
					__WQ_LEGACY | WQ_MEM_RECLAIM |
					WQ_FREEZABLE);
	if (!cam->link_change_wq) {
		dev_dbg(dev, "failed to alloc composer workqueue\n");
		destroy_workqueue(cam->composer_wq);
		ret = -ENOMEM;
		goto fail_mem_uninit;
	}

	return 0;

fail_mem_uninit:
	mtk_cam_working_buf_pool_release(cam);
fail_shutdown:
	rproc_shutdown(cam->rproc_handle);
fail_rproc_put:
	rproc_put(cam->rproc_handle);
	cam->rproc_handle = NULL;

	return ret;
}

static int mtk_cam_pm_suspend(struct device *dev)
{
	int ret;

	dev_dbg(dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev))
		return 0;

	ret = pm_runtime_force_suspend(dev);
	if (ret)
		dev_dbg(dev, "failed to force suspend:%d\n", ret);

	return ret;
}

static int mtk_cam_pm_resume(struct device *dev)
{
	int ret;

	dev_dbg(dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev))
		return 0;

	ret = pm_runtime_force_resume(dev);
	return ret;
}

static int mtk_cam_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "- %s\n", __func__);
	return 0;
}

static int mtk_cam_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "- %s\n", __func__);
	return 0;
}

struct mtk_cam_ctx *mtk_cam_find_ctx(struct mtk_cam_device *cam,
				     struct media_entity *entity)
{
	unsigned int i;

	for (i = 0;  i < cam->max_stream_num; i++) {
		if (entity->pipe == &cam->ctxs[i].pipeline)
			return &cam->ctxs[i];
	}

	return NULL;
}

struct mtk_cam_ctx *mtk_cam_start_ctx(struct mtk_cam_device *cam,
				      struct mtk_cam_video_device *node)
{
	struct mtk_cam_ctx *ctx = node->ctx;
	struct media_graph *graph;
	struct v4l2_subdev **target_sd;
	int ret, i;
	struct media_entity *entity = &node->vdev.entity;

	pr_info("%s\n", __func__);
	ctx->used_raw_dmas |= node->desc.dma_port;

	if (!cam->rproc_handle) {
		ctx->enqueued_frame_seq_no = 0;
		ctx->composed_frame_seq_no = 0;
		ctx->dequeued_frame_seq_no = 0;
		cam->running_job_count = 0;

		ret = isp_composer_init(cam);
		if (ret)
			return NULL;
		pm_runtime_get_sync(cam->dev);
	}

	ret = media_pipeline_start(entity, &ctx->pipeline);
	if (ret) {
		dev_dbg(cam->dev, "failed to start pipeline:%d\n", ret);
		goto fail_uninit_composer;
	}

	/* traverse to update used subdevs & number of nodes */
	graph = &ctx->pipeline.graph;
	media_graph_walk_start(graph, entity);

	i = 0;
	while ((entity = media_graph_walk_next(graph))) {
		dev_dbg(cam->dev, "linked entity %s\n", entity->name);

		target_sd = NULL;

		switch (entity->function) {
		case MEDIA_ENT_F_IO_V4L:
			ctx->enabled_node_cnt++;
			break;
		case MEDIA_ENT_F_PROC_VIDEO_PIXEL_FORMATTER: /* pipeline */
			if (i >= MAX_PIPES_PER_STREAM)
				goto fail_stop_pipeline;

			target_sd = ctx->pipe_subdevs + i;
			i++;
			break;
		case MEDIA_ENT_F_VID_IF_BRIDGE: /* seninf */
			target_sd = &ctx->seninf;
			break;
		case MEDIA_ENT_F_CAM_SENSOR:
			target_sd = &ctx->sensor;
			break;
		default:
			break;
		}

		if (!target_sd)
			continue;

		if (*target_sd) {
			dev_dbg(cam->dev, "duplicated subdevs!!!\n");
			goto fail_stop_pipeline;
		}

		if (is_media_entity_v4l2_subdev(entity))
			*target_sd = media_entity_to_v4l2_subdev(entity);
	}

	return ctx;

fail_stop_pipeline:
	media_pipeline_stop(entity);
fail_uninit_composer:
	isp_composer_uninit(cam);
	return NULL;
}

static void isp_composer_uninit_wait(struct mtk_cam_device *cam)
{
	unsigned long end = jiffies + msecs_to_jiffies(100);

	while (time_before(jiffies, end) && cam->composer_wq) {
		dev_dbg(cam->dev, "wait composer session destroy\n");
		usleep_range(0, 1000);
	}

	isp_composer_uninit(cam);

	pm_runtime_mark_last_busy(cam->dev);
	pm_runtime_put_autosuspend(cam->dev);
}

void mtk_cam_stop_ctx(struct mtk_cam_ctx *ctx, struct media_entity *entity)
{
	struct mtk_cam_device *cam = ctx->cam;
	unsigned int i;

	pr_info("%s\n", __func__);
	media_pipeline_stop(entity);

	if (!cam->streaming_ctx) {
		struct v4l2_subdev *sd;

		v4l2_device_for_each_subdev(sd, &cam->v4l2_dev) {
			if (sd->entity.function == MEDIA_ENT_F_VID_IF_BRIDGE) {
				int ret;

				ret = v4l2_subdev_call(sd, video, s_stream, 0);
				if (ret)
					dev_dbg(cam->dev,
						"failed to streamoff %s:%d\n",
						sd->name, ret);
				sd->entity.stream_count = 0;
				sd->entity.pipe = NULL;
			} else if (sd->entity.function ==
						MEDIA_ENT_F_CAM_SENSOR) {
				sd->entity.stream_count = 0;
				sd->entity.pipe = NULL;
			}
		}
	}

	ctx->enabled_node_cnt = 0;
	ctx->streaming_node_cnt = 0;
	ctx->sensor = NULL;
	ctx->prev_sensor = NULL;
	ctx->seninf = NULL;
	ctx->enqueued_frame_seq_no = 0;
	ctx->composed_frame_seq_no = 0;

	INIT_LIST_HEAD(&ctx->using_buffer_list.list);
	INIT_LIST_HEAD(&ctx->composed_buffer_list.list);
	INIT_LIST_HEAD(&ctx->processing_buffer_list.list);

	for (i = 0; i < MAX_PIPES_PER_STREAM; i++)
		ctx->pipe_subdevs[i] = NULL;

	if (ctx->cam->rproc_handle && !ctx->cam->composer_cnt)
		isp_composer_uninit_wait(ctx->cam);
}

int mtk_cam_ctx_stream_on(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct device *dev;
	struct mtk_raw_device *raw_dev;
	int i, ret;
	unsigned long flags;

	dev_info(cam->dev, "ctx %d stream on\n", ctx->stream_id);

	if (ctx->streaming) {
		dev_dbg(cam->dev, "ctx-%d is already streaming on\n",
			 ctx->stream_id);
		return 0;
	}

	cam->composer_cnt++;

	mtk_cam_seninf_set_camtg(ctx->seninf, PAD_SRC_RAW0, ctx->stream_id);
	/* todo: backend support one pixel mode only */
	dev_info(cam->dev, "only support pixel mode 0");
	mtk_cam_seninf_set_pixelmode(ctx->seninf, PAD_SRC_RAW0, 0x0);
	ret = v4l2_subdev_call(ctx->seninf, video, s_stream, 1);
	if (ret) {
		dev_dbg(cam->dev, "failed to stream on seninf %s:%d\n",
			ctx->seninf->name, ret);
		return ret;
	}

	for (i = 0; i < MAX_PIPES_PER_STREAM && ctx->pipe_subdevs[i]; i++) {
		ret = v4l2_subdev_call(ctx->pipe_subdevs[i], video,
				       s_stream, 1);
		if (ret) {
			dev_dbg(cam->dev, "failed to stream on %d: %d\n",
				ctx->pipe_subdevs[i]->name, ret);
			goto fail_pipe_off;
		}
	}

	/**
	 * TODO: validate pad's setting of each pipes
	 * return -EPIPE if failed
	 */
	ret = mtk_cam_dev_config(ctx, 0);
	if (ret)
		return ret;

	dev = mtk_cam_find_raw_dev(cam, ctx->used_raw_dev);
	if (!dev) {
		dev_dbg(cam->dev, "streamon raw device not found\n");
		goto fail_pipe_off;
	}
	raw_dev = dev_get_drvdata(dev);
	initialize(raw_dev);

	spin_lock_irqsave(&ctx->streaming_lock, flags);
	ctx->streaming = true;
	cam->streaming_ctx |= 1 << ctx->stream_id;
	spin_unlock_irqrestore(&ctx->streaming_lock, flags);

	ret = mtk_camsys_ctrl_start(ctx);
	if (ret) {
		ctx->streaming = false;
		cam->streaming_ctx &= ~(1 << ctx->stream_id);
		goto fail_pipe_off;
	}
	mtk_cam_dev_req_try_queue(cam);

	dev_dbg(cam->dev, "streamed on camsys ctx:%d\n", ctx->stream_id);

	return 0;

fail_pipe_off:
	isp_composer_destroy_session(cam, ctx);
	cam->composer_cnt--;
	for (i = i - 1; i >= 0; i--)
		v4l2_subdev_call(ctx->pipe_subdevs[i], video, s_stream, 0);

	v4l2_subdev_call(ctx->seninf, video, s_stream, 0);

	return ret;
}

int mtk_cam_ctx_stream_off(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct device *dev;
	struct mtk_raw_device *raw_dev;
	unsigned int i;
	int ret;

	if (!ctx->streaming) {
		dev_dbg(cam->dev, "ctx-%d is already streaming off\n",
			 ctx->stream_id);
		return 0;
	}

	cam->composer_cnt--;
	ctx->streaming = false;
	cam->streaming_ctx &= ~(1 << ctx->stream_id);

	ret = v4l2_subdev_call(ctx->seninf, video, s_stream, 0);
	if (ret) {
		dev_dbg(cam->dev, "failed to stream off %s:%d\n",
			ctx->seninf->name, ret);
		return -EPERM;
	}

	for (i = 0; i < MAX_PIPES_PER_STREAM && ctx->pipe_subdevs[i]; i++) {
		ret = v4l2_subdev_call(ctx->pipe_subdevs[i], video,
				       s_stream, 0);
		if (ret) {
			dev_dbg(cam->dev, "failed to stream off %d: %d\n",
				ctx->pipe_subdevs[i]->name, ret);
			return -EPERM;
		}
	}

	dev = mtk_cam_find_raw_dev(cam, ctx->used_raw_dev);
	if (!dev) {
		dev_dbg(cam->dev, "streamoff raw device not found\n");
		goto fail_stream_off;
	}
	raw_dev = dev_get_drvdata(dev);
	stream_on(raw_dev, 0);
	mtk_camsys_ctrl_stop(ctx);
fail_stream_off:
	/* FIXME: need to receive destroy ack then to destroy rproc_phandle */
	isp_composer_destroy_session(cam, ctx);

	dev_dbg(cam->dev, "streamed off camsys ctx:%d\n", ctx->stream_id);

	return 0;
}

static int config_bridge_pad_links(struct mtk_cam_device *cam,
				   struct v4l2_subdev *seninf)
{
	struct media_entity *pipe_entity;
	unsigned int i;
	int ret;

	for (i = 0; i < cam->max_stream_num; i++) {
		pipe_entity = &cam->raw.pipelines[i].subdev.entity;

		dev_info(cam->dev, "create pad link %s %s\n",
			 seninf->entity.name, pipe_entity->name);

		ret = media_create_pad_link(&seninf->entity,
					    MTK_CAM_CIO_PAD_SRC,
					    pipe_entity,
					    MTK_CAM_CIO_PAD_SINK,
					    MEDIA_LNK_FL_DYNAMIC);

		if (ret) {
			dev_dbg(cam->dev,
				"failed to create pad link %s %s err:%d\n",
				seninf->entity.name, pipe_entity->name,
				ret);
			return ret;
		}
	}

	return 0;
}

static int mtk_cam_create_links(struct mtk_cam_device *cam)
{
	struct v4l2_subdev *sd;
	unsigned int i;
	int ret;

	i = 0;
	v4l2_device_for_each_subdev(sd, &cam->v4l2_dev) {
		if (i < cam->num_mtkcam_seninf_drivers &&
		    sd->entity.function == MEDIA_ENT_F_VID_IF_BRIDGE)
			ret = config_bridge_pad_links(cam, sd);

		i++;
	}

	return ret;
}

static int mtk_cam_master_bind(struct device *dev)
{
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);
	struct media_device *media_dev = &cam_dev->media_dev;
	int ret;

	dev_info(dev, "%s\n", __func__);

	media_dev->dev = cam_dev->dev;
	strscpy(media_dev->model, dev_driver_string(dev),
		sizeof(media_dev->model));
	snprintf(media_dev->bus_info, sizeof(media_dev->bus_info),
		 "platform:%s", dev_name(dev));
	media_dev->hw_revision = 0;
	media_dev->ops = &mtk_cam_dev_ops;
	media_device_init(media_dev);

	cam_dev->v4l2_dev.mdev = media_dev;
	ret = v4l2_device_register(cam_dev->dev, &cam_dev->v4l2_dev);
	if (ret) {
		dev_dbg(dev, "Failed to register V4L2 device: %d\n", ret);
		goto fail_media_device_cleanup;
	}

	ret = media_device_register(media_dev);
	if (ret) {
		dev_dbg(dev, "Failed to register media device: %d\n",
			ret);
		goto fail_v4l2_device_unreg;
	}

	ret = component_bind_all(dev, cam_dev);
	if (ret) {
		dev_dbg(dev, "Failed to bind all component: %d\n", ret);
		goto fail_media_device_unreg;
	}

	ret = mtk_raw_register_entities(&cam_dev->raw, &cam_dev->v4l2_dev);
	if (ret) {
		dev_dbg(dev, "Failed to init raw subdevs: %d\n", ret);
		goto fail_unbind_all;
	}

	/* TODO: bind camsv ? */
	/* TODO: bind mraw ? */

	mtk_cam_create_links(cam_dev);
	/* Expose all subdev's nodes */
	ret = v4l2_device_register_subdev_nodes(&cam_dev->v4l2_dev);
	if (ret) {
		dev_dbg(dev, "Failed to register subdev nodes\n");
		goto fail_unreg_entities;
	}
	mtk_cam_dvfs_init(cam_dev);
	pm_runtime_set_autosuspend_delay(dev, 2 * MTK_CAM_STOP_HW_TIMEOUT);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_enable(dev);

	return 0;

fail_unreg_entities:
	mtk_raw_unregister_entities(&cam_dev->raw);

fail_unbind_all:
	component_unbind_all(dev, cam_dev);

fail_media_device_unreg:
	media_device_unregister(&cam_dev->media_dev);

fail_v4l2_device_unreg:
	v4l2_device_unregister(&cam_dev->v4l2_dev);

fail_media_device_cleanup:
	media_device_cleanup(&cam_dev->media_dev);

	return ret;
}

static void mtk_cam_master_unbind(struct device *dev)
{
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);

	dev_info(dev, "%s\n", __func__);

	mtk_raw_unregister_entities(&cam_dev->raw);
	mtk_cam_dvfs_uninit(cam_dev);
	component_unbind_all(dev, cam_dev);

	media_device_unregister(&cam_dev->media_dev);
	v4l2_device_unregister(&cam_dev->v4l2_dev);
	media_device_cleanup(&cam_dev->media_dev);

	pm_runtime_dont_use_autosuspend(dev);
	pm_runtime_disable(dev);
}

static int compare_dev(struct device *dev, void *data)
{
	return dev == (struct device *)data;
}

static void mtk_cam_match_remove(struct device *dev)
{
	struct device_link *link;

	list_for_each_entry(link, &dev->links.consumers, s_node)
		device_link_del(link);
}

static struct component_match *mtk_cam_match_add(struct mtk_cam_device *cam_dev)
{
	struct component_match *match = NULL;
	int num_mtkcam_seninf = 0;
	int num_mtkcam_sub_drivers = 0;
	struct platform_driver *drv;
	struct device *p = NULL, *d;

	drv =  &mtk_cam_raw_driver;
	do {
		d = platform_find_device_by_driver(p, &drv->driver);
		put_device(p);
		p = d;
		if (!d)
			break;

		component_match_add(cam_dev->dev,
				    &match, compare_dev, d);
		num_mtkcam_sub_drivers++;
	} while (true);
	cam_dev->num_mtkcam_sub_drivers = num_mtkcam_sub_drivers;

	drv =  &seninf_pdrv;
	p = NULL;
	do {
		d = platform_find_device_by_driver(p, &drv->driver);
		put_device(p);
		p = d;
		if (!d)
			break;

		component_match_add(cam_dev->dev,
				    &match, compare_dev, d);
		num_mtkcam_seninf++;
	} while (true);
	cam_dev->num_mtkcam_seninf_drivers = num_mtkcam_seninf;

	if (IS_ERR(match))
		mtk_cam_match_remove(cam_dev->dev);

	return match ? match : ERR_PTR(-ENODEV);
}

static const struct component_master_ops mtk_cam_master_ops = {
	.bind = mtk_cam_master_bind,
	.unbind = mtk_cam_master_unbind,
};

static void mtk_cam_ctx_init(struct mtk_cam_ctx *ctx,
			     struct mtk_cam_device *cam,
			     int stream_id)
{
	cam->streaming_ctx = 0;
	ctx->cam = cam;
	ctx->stream_id = stream_id;
	ctx->sensor = NULL;
	ctx->prev_sensor = NULL;

	INIT_LIST_HEAD(&ctx->using_buffer_list.list);
	INIT_LIST_HEAD(&ctx->composed_buffer_list.list);
	INIT_LIST_HEAD(&ctx->processing_buffer_list.list);
	spin_lock_init(&ctx->using_buffer_list.lock);
	spin_lock_init(&ctx->composed_buffer_list.lock);
	spin_lock_init(&ctx->processing_buffer_list.lock);
	spin_lock_init(&ctx->streaming_lock);
}

static int mtk_cam_probe(struct platform_device *pdev)
{
	struct mtk_cam_device *cam_dev;
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct component_match *match = NULL;
	int ret, i;

	dev_info(dev, "%s\n", __func__);

	/* initialize structure */
	cam_dev = devm_kzalloc(dev, sizeof(*cam_dev), GFP_KERNEL);
	if (!cam_dev)
		return -ENOMEM;

	if (dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34)))
		dev_dbg(dev, "%s: No suitable DMA available\n", __func__);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_dbg(dev, "failed to get mem\n");
		return -ENODEV;
	}

	cam_dev->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(cam_dev->base)) {
		dev_dbg(dev, "failed to map register base\n");
		return PTR_ERR(cam_dev->base);
	}
	dev_dbg(dev, "cam_dev, map_addr=0x%pK\n", cam_dev->base);

	cam_dev->dev = dev;
	dev_set_drvdata(dev, cam_dev);

	cam_dev->composer_cnt = 0;
	cam_dev->num_mtkcam_seninf_drivers = 0;

	ret = platform_driver_register(&seninf_pdrv);
	if (ret)
		return ret;

	ret = platform_driver_register(&seninf_core_pdrv);
	if (ret) {
		platform_driver_unregister(&seninf_pdrv);
		return ret;
	}

	ret = platform_driver_register(&mtk_cam_raw_driver);
	if (ret) {
		platform_driver_unregister(&seninf_core_pdrv);
		platform_driver_unregister(&seninf_pdrv);
		return ret;
	}

	/* FIXME: decide max raw stream num by seninf num */
	cam_dev->max_stream_num = RAW_PIPELINE_NUM;
	cam_dev->ctxs = devm_kcalloc(dev, cam_dev->max_stream_num,
				     sizeof(*cam_dev->ctxs), GFP_KERNEL);
	if (!cam_dev->ctxs)
		return -ENOMEM;

	for (i = 0; i < cam_dev->max_stream_num; i++)
		mtk_cam_ctx_init(cam_dev->ctxs + i, cam_dev, i);

	cam_dev->running_job_count = 0;
	spin_lock_init(&cam_dev->pending_job_lock);
	spin_lock_init(&cam_dev->running_job_lock);
	spin_lock_init(&cam_dev->camsys_ctrl.link_change_lock);
	INIT_LIST_HEAD(&cam_dev->pending_job_list);
	INIT_LIST_HEAD(&cam_dev->running_job_list);

	mutex_init(&cam_dev->op_lock);

	ret = mtk_cam_of_rproc(cam_dev);
	if (ret)
		goto fail_destroy_mutex;
	match = mtk_cam_match_add(cam_dev);
	if (IS_ERR(match)) {
		ret = PTR_ERR(match);
		goto fail_destroy_mutex;
	}

	ret = component_master_add_with_match(dev, &mtk_cam_master_ops, match);
	if (ret < 0)
		goto fail_match_remove;

	return 0;

fail_match_remove:
	mtk_cam_match_remove(dev);

fail_destroy_mutex:
	mutex_destroy(&cam_dev->op_lock);

	return ret;
}

static int mtk_cam_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);

	component_master_del(dev, &mtk_cam_master_ops);
	mtk_cam_match_remove(dev);

	mutex_destroy(&cam_dev->op_lock);

	platform_driver_unregister(&mtk_cam_raw_driver);
	platform_driver_unregister(&seninf_core_pdrv);
	platform_driver_unregister(&seninf_pdrv);

	return 0;
}

static const struct dev_pm_ops mtk_cam_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_cam_pm_suspend, mtk_cam_pm_resume)
	SET_RUNTIME_PM_OPS(mtk_cam_runtime_suspend, mtk_cam_runtime_resume,
			   NULL)
};

static struct platform_driver mtk_cam_driver = {
	.probe   = mtk_cam_probe,
	.remove  = mtk_cam_remove,
	.driver  = {
		.name  = "mtk-cam",
		.of_match_table = of_match_ptr(mtk_cam_of_ids),
		.pm     = &mtk_cam_pm_ops,
	}
};

static int __init mtk_cam_init(void)
{
	int ret;

	ret = platform_driver_register(&mtk_cam_driver);
	return ret;
}

static void __exit mtk_cam_exit(void)
{
	platform_driver_unregister(&mtk_cam_driver);
}

module_init(mtk_cam_init);
module_exit(mtk_cam_exit);

MODULE_DESCRIPTION("Camera ISP driver");
MODULE_LICENSE("GPL");
