// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/component.h>
#include <linux/freezer.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include <linux/platform_data/mtk_ccd.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include <linux/rpmsg/mtk_ccd_rpmsg.h>
#include <linux/mtk_ccd_controls.h>
#include <linux/regulator/consumer.h>

#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/kthread.h>
#include <linux/media.h>
#include <linux/jiffies.h>
#include <media/videobuf2-v4l2.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-mc.h>
#include <media/v4l2-subdev.h>
#include <media/media-entity.h>
#include <uapi/linux/sched/types.h>

#include <trace/hooks/v4l2core.h>
#include <trace/hooks/v4l2mc.h>

#include <mtk_heap.h>

#include "mtk_cam.h"
#include "mtk_cam-plat.h"
//#include "mtk_cam-ctrl.h"
#include "mtk_cam-larb.h"
#include "mtk_cam-tg-flash.h"
#include "mtk_camera-v4l2-controls.h"
#include "mtk_camera-videodev2.h"
#include "mtk_cam-hsf.h"
#include "mtk_cam-trace.h"
#include "mtk_cam-ufbc-def.h"
#include "mtk_cam-timesync.h"
#include "mtk_cam-job.h"

#ifdef CONFIG_VIDEO_MTK_ISP_CAMSYS_DUBUG
static unsigned int debug_ae = 1;
#else
static unsigned int debug_ae;
#endif

module_param(debug_ae, uint, 0644);
MODULE_PARM_DESC(debug_ae, "activates debug ae info");

/* Zero out the end of the struct pointed to by p.  Everything after, but
 * not including, the specified field is cleared.
 */
#define CLEAR_AFTER_FIELD(p, field) \
	memset((u8 *)(p) + offsetof(typeof(*(p)), field) + sizeof((p)->field), \
	0, sizeof(*(p)) - offsetof(typeof(*(p)), field) -sizeof((p)->field))

static const struct of_device_id mtk_cam_of_ids[] = {
	{.compatible = "mediatek,mt6985-camisp", .data = &mt6985_data},
	{.compatible = "mediatek,mt6886-camisp", .data = NULL},
	{}
};

MODULE_DEVICE_TABLE(of, mtk_cam_of_ids);

static struct device *camsys_root_dev;
struct device *mtk_cam_root_dev(void)
{
	return camsys_root_dev;
}

static int mtk_cam_req_try_update_used_ctx(struct media_request *req);

static int set_dev_to_arr(struct device **arr, int num,
			     int idx, struct device *dev)
{
	if (idx < 0 || idx >= num) {
		dev_info(dev, "failed to update engine idx=%d/%d\n", idx, num);
		return -1;
	}

	if (dev)
		arr[idx] = dev;
	return 0;
}

int mtk_cam_set_dev_raw(struct device *dev, int idx,
			 struct device *raw, struct device *yuv)
{
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);
	struct mtk_cam_engines *eng = &cam_dev->engines;

	return set_dev_to_arr(eng->raw_devs, eng->num_raw_devices, idx, raw) ||
		set_dev_to_arr(eng->yuv_devs, eng->num_raw_devices, idx, yuv);
}

int mtk_cam_set_dev_sv(struct device *dev, int idx, struct device *sv)
{
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);
	struct mtk_cam_engines *eng = &cam_dev->engines;

	return set_dev_to_arr(eng->sv_devs, eng->num_camsv_devices, idx, sv);
}

int mtk_cam_set_dev_mraw(struct device *dev, int idx, struct device *mraw)
{
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);
	struct mtk_cam_engines *eng = &cam_dev->engines;

	return set_dev_to_arr(eng->mraw_devs, eng->num_mraw_devices, idx, mraw);
}

static DEFINE_SPINLOCK(larb_probe_lock);
int mtk_cam_set_dev_larb(struct device *dev, struct device *larb)
{
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);
	struct mtk_cam_engines *eng = &cam_dev->engines;
	int i;

	spin_lock(&larb_probe_lock);
	for (i = 0; i < eng->num_larb_devices; ++i) {
		if (!eng->larb_devs[i]) {
			eng->larb_devs[i] = larb;
			break;
		}
	}
	spin_unlock(&larb_probe_lock);
	return (i < eng->num_larb_devices) ? 0 : 1;
}

struct device *mtk_cam_get_larb(struct device *dev, int larb_id)
{
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);
	struct mtk_cam_engines *eng = &cam_dev->engines;
	struct device *larb_dev;
	int i;

	for (i = 0; i < eng->num_larb_devices; ++i) {
		larb_dev = eng->larb_devs[i];
		if (larb_dev && larb_id != mtk_cam_larb_id(larb_dev))
			return larb_dev;
	}

	dev_info(dev, "failed to get larb %d\n", larb_id);
	return NULL;
}

static bool _get_permit_to_queue(struct mtk_cam_device *cam)
{
	return !atomic_cmpxchg(&cam->is_queuing, 0, 1);
}

static void _put_permit_to_queue(struct mtk_cam_device *cam)
{
	atomic_set(&cam->is_queuing, 0);
}

static void append_to_running_list(struct mtk_cam_device *cam,
				   struct mtk_cam_request *req)
{
	int cnt;

	media_request_get(&req->req);

	spin_lock(&cam->running_job_lock);
	cnt = ++cam->running_job_count;
	list_add_tail(&req->list, &cam->running_job_list);
	spin_unlock(&cam->running_job_lock);

	dev_info(cam->dev, "%s: req:%s ++running cnt %d\n",
		 __func__, req->req.debug_str, cnt);
}

static void remove_frome_running_list(struct mtk_cam_device *cam,
				      struct mtk_cam_request *req)
{
	int cnt;

	spin_lock(&cam->running_job_lock);
	cnt = --cam->running_job_count;
	list_del(&req->list);
	spin_unlock(&cam->running_job_lock);

	dev_info(cam->dev, "%s: req:%s --running cnt %d\n",
		 __func__, req->req.debug_str, cnt);

	media_request_put(&req->req);

}

static void update_ctxs_available_jobs(struct mtk_cam_device *cam)
{
	struct mtk_cam_ctx *ctx;
	int streaming_ctx;
	int i;

	spin_lock(&cam->streaming_lock);
	streaming_ctx = cam->streaming_ctx;
	spin_unlock(&cam->streaming_lock);

	for (i = 0; i < cam->max_stream_num; i++) {
		ctx = &cam->ctxs[i];

		if (!USED_MASK_HAS(&streaming_ctx, stream, i)) {
			ctx->available_jobs = 0;
			continue;
		}

		ctx->available_jobs = mtk_cam_pool_available_cnt(&ctx->job_pool);
	}
}

static bool mtk_cam_test_available_jobs(struct mtk_cam_device *cam,
				       int stream_mask)
{
	struct mtk_cam_ctx *ctx;
	int i;

	for (i = 0; i < cam->max_stream_num; i++) {
		ctx = &cam->ctxs[i];

		if (!USED_MASK_HAS(&stream_mask, stream, i))
			continue;

		if (!ctx->available_jobs)
			goto fail_to_fetch_job;

		--ctx->available_jobs;
	}

	return true;

fail_to_fetch_job:
	dev_info(cam->dev, "%s: ctx %d has no available job\n",
		 __func__, ctx->stream_id);

	for (i = i - 1; i >= 0; i--) {
		if (!USED_MASK_HAS(&stream_mask, stream, i))
			continue;
		++ctx->available_jobs;
	}
	return false;
}

static int mtk_cam_get_reqs_to_enque(struct mtk_cam_device *cam,
				     struct list_head *enqueue_list)
{
	struct mtk_cam_request *req, *req_prev;
	int cnt;

	/*
	 * for each req in pending_list
	 *    try update req's ctx
	 *    if all required ctxs are streaming
	 *    if all required ctxs have available job
	 *    => ready to enque
	 */
	update_ctxs_available_jobs(cam);

	cnt = 0;
	spin_lock(&cam->pending_job_lock);
	list_for_each_entry_safe(req, req_prev, &cam->pending_job_list, list) {
		int used_ctx;

		if (mtk_cam_req_try_update_used_ctx(&req->req))
			continue;

		used_ctx = mtk_cam_req_used_ctx(&req->req);
		if (!mtk_cam_are_all_streaming(cam, used_ctx))
			continue;

		if (!mtk_cam_test_available_jobs(cam, used_ctx))
			continue;

		cnt++;
		list_del(&req->list);
		list_add_tail(&req->list, enqueue_list);
	}
	spin_unlock(&cam->pending_job_lock);

	return cnt;
}

static int mtk_cam_enque_list(struct mtk_cam_device *cam,
			      struct list_head *enqueue_list)
{
	struct mtk_cam_request *req, *req_prev;

	list_for_each_entry_safe(req, req_prev, enqueue_list, list) {

		append_to_running_list(cam, req);
		mtk_cam_dev_req_enqueue(cam, req);
	}

	return 0;
}

/* Note:
 * this funciton will be called from userspace's context & workqueue
 */
void mtk_cam_dev_req_try_queue(struct mtk_cam_device *cam)
{
	struct list_head enqueue_list;

	if (!_get_permit_to_queue(cam))
		return;

	if (!mtk_cam_is_any_streaming(cam))
		goto put_permit;

	INIT_LIST_HEAD(&enqueue_list);
	if (!mtk_cam_get_reqs_to_enque(cam, &enqueue_list))
		goto put_permit;

	mtk_cam_enque_list(cam, &enqueue_list);

put_permit:
	_put_permit_to_queue(cam);
}

static struct media_request *mtk_cam_req_alloc(struct media_device *mdev)
{
	struct mtk_cam_request *cam_req;

	cam_req = vzalloc(sizeof(*cam_req));

	spin_lock_init(&cam_req->buf_lock);
	//mutex_init(&cam_req->fs.op_lock);

	return &cam_req->req;
}

static void mtk_cam_req_free(struct media_request *req)
{
	struct mtk_cam_request *cam_req = to_mtk_cam_req(req);

	vfree(cam_req);
}

static void mtk_cam_req_reset(struct media_request *req)
{
	struct mtk_cam_request *cam_req = to_mtk_cam_req(req);

	cam_req->used_ctx = 0;
	cam_req->used_pipe = 0;

	INIT_LIST_HEAD(&cam_req->buf_list);
	/* todo: init fs */
}

/* may fail to get contexts if not stream-on yet */
static int mtk_cam_req_try_update_used_ctx(struct media_request *req)
{
	struct mtk_cam_request *cam_req = to_mtk_cam_req(req);
	struct mtk_cam_device *cam =
		container_of(req->mdev, struct mtk_cam_device, media_dev);
	struct mtk_cam_v4l2_pipelines *ppls = &cam->pipelines;
	int pipe_used = cam_req->used_pipe;
	int used_ctx = 0;
	bool not_streamon_yet = false;
	struct mtk_cam_ctx *ctx;
	int i;

	if (cam_req->used_ctx)
		return 0;

	for (i = 0; i < ppls->num_raw; i++)
		if (USED_MASK_HAS(&pipe_used, raw, i)) {
			ctx = mtk_cam_find_ctx(cam,
					       &ppls->raw[i].subdev.entity);

			if (!ctx) {
				/* not all pipes are stream-on */
				not_streamon_yet = true;
				break;
			}

			USED_MASK_SET(&used_ctx, stream, ctx->stream_id);
		}

	/* todo: camsv */
	/* todo: mraw */

	dev_dbg(cam->dev, "%s: req %s used_ctx 0x%x used_pipe 0x%x\n",
		__func__, req->debug_str, used_ctx, pipe_used);
	cam_req->used_ctx = !not_streamon_yet ? used_ctx : 0;
	return !not_streamon_yet ? 0 : -1;
}

static int _req_list_ctrl_handlers(struct media_request *req,
				   struct media_request_object **arr_obj,
				   int arr_size)
{
	struct media_request_object *obj;
	int n = 0;

	list_for_each_entry(obj, &req->objects, list) {
		if (vb2_request_object_is_buffer(obj))
			continue;

		if (WARN_ON(n == arr_size))
			return -1;

		arr_obj[n] = obj;
		++n;
	}
	return n;
}

static int _req_has_ctrl_hdl(struct mtk_cam_request *req,
			      struct v4l2_ctrl_handler *hdl)
{
	int i;

	for (i = 0; i < req->ctrl_objs_nr; i++)
		if (req->ctrl_objs[i]->priv == hdl)
			return i;
	return -1;
}

static int mtk_cam_req_get_ctrl_handlers(struct media_request *req)
{
	struct mtk_cam_request *cam_req = to_mtk_cam_req(req);

	cam_req->ctrl_objs_nr =
		_req_list_ctrl_handlers(req,
					cam_req->ctrl_objs,
					ARRAY_SIZE(cam_req->ctrl_objs));
	return 0;
}

static int mtk_cam_setup_pipe_ctrl(struct media_request *req)
{
	struct mtk_cam_request *cam_req = to_mtk_cam_req(req);
	struct mtk_cam_device *cam =
		container_of(req->mdev, struct mtk_cam_device, media_dev);
	struct mtk_cam_v4l2_pipelines *ppls = &cam->pipelines;
	struct v4l2_ctrl_handler *hdl;
	int i, ret;

	if (!cam_req->ctrl_objs_nr)
		return 0;

	ret = 0;
	for (i = 0; i < ppls->num_raw; i++) {
		int obj_idx;
		struct media_request_object *obj;

		if (!USED_MASK_HAS(&cam_req->used_pipe, raw, i))
			continue;

		hdl = &ppls->raw[i].ctrl_handler;

		obj_idx = _req_has_ctrl_hdl(cam_req, hdl);
		if (obj_idx < 0)
			continue;

		ret = v4l2_ctrl_request_setup(req, hdl);
		if (ret) {
			dev_info(cam->dev, "failed to setup ctrl of %s\n",
				ppls->raw[i].subdev.entity.name);
			break;
		}

		obj = cam_req->ctrl_objs[obj_idx];

		if (obj->ops)
			obj->ops->unbind(obj);
		media_request_object_complete(obj);
	}

	return ret;
}

static void mtk_cam_clone_pipe_data_to_req(struct media_request *req)
{
	struct mtk_cam_request *cam_req = to_mtk_cam_req(req);
	struct mtk_cam_device *cam =
		container_of(req->mdev, struct mtk_cam_device, media_dev);
	struct mtk_cam_v4l2_pipelines *ppls = &cam->pipelines;
	int i;


	for (i = 0; i < ppls->num_raw; i++) {
		struct mtk_raw_request_data *data;
		struct mtk_raw_pipeline *raw;
		struct mtk_raw_pad_config *pad;

		if (!USED_MASK_HAS(&cam_req->used_pipe, raw, i))
			continue;

		data = &cam_req->raw_data[i];
		raw = &ppls->raw[i];
		pad = &raw->pad_cfg[MTK_RAW_SINK];

		data->ctrl = raw->ctrl_data;
		data->sink.width = pad->mbus_fmt.width;
		data->sink.height = pad->mbus_fmt.height;
		data->sink.mbus_code = pad->mbus_fmt.code;

		/* todo: support tg crop */
		//data->sink.crop = pad->crop;
		data->sink.crop = (struct v4l2_rect) {
			.left = 0,
			.top = 0,
			.width = pad->mbus_fmt.width,
			.height = pad->mbus_fmt.height,
		};
	}
}

static void mtk_cam_req_queue(struct media_request *req)
{
	struct mtk_cam_request *cam_req = to_mtk_cam_req(req);
	struct mtk_cam_device *cam =
		container_of(req->mdev, struct mtk_cam_device, media_dev);

	// reset req
	mtk_cam_req_reset(req);

	/* update following in mtk_cam_vb2_buf_queue (.buf_queue)
	 *   add mtk_cam_buffer to req->buf_list
	 *   req->used_pipe
	 */
	vb2_request_queue(req);
	WARN_ON(!cam_req->used_pipe);

	if (mtk_cam_req_try_update_used_ctx(req)) {
		dev_info(cam->dev,
			 "req %s enqueued before stream-on\n", req->debug_str);
	}

	/* parse ctrl handlers via used_pipe */
	mtk_cam_req_get_ctrl_handlers(req);

	/* setup ctrl handler */
	WARN_ON(mtk_cam_setup_pipe_ctrl(req));

	mtk_cam_clone_pipe_data_to_req(req);

	spin_lock(&cam->pending_job_lock);
	list_add_tail(&cam_req->list, &cam->pending_job_list);
	spin_unlock(&cam->pending_job_lock);

	mtk_cam_dev_req_try_queue(cam);
}

static int mtk_cam_link_notify(struct media_link *link, u32 flags,
			      unsigned int notification)
{
	return v4l2_pipeline_link_notify(link, flags, notification);
}

static const struct media_device_ops mtk_cam_dev_ops = {
	.link_notify = mtk_cam_link_notify,
	.req_alloc = mtk_cam_req_alloc,
	.req_free = mtk_cam_req_free,
	.req_validate = vb2_request_validate,
	.req_queue = mtk_cam_req_queue,
};

static int mtk_cam_get_ccu_phandle(struct mtk_cam_device *cam)
{
	struct device *dev = cam->dev;
	struct device_node *node;
	int ret = 0;

	node = of_find_compatible_node(NULL, NULL, "mediatek,camera_camsys_ccu");
	if (node == NULL) {
		dev_info(dev, "of_find mediatek,camera_camsys_ccu fail\n");
		ret = PTR_ERR(node);
		goto out;
	}

	ret = of_property_read_u32(node, "mediatek,ccu_rproc",
				   &cam->rproc_ccu_phandle);
	if (ret) {
		dev_info(dev, "fail to get ccu rproc_phandle:%d\n", ret);
		ret = -EINVAL;
		goto out;
	}

out:
	return ret;
}

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

static struct mtk_cam_job *mtk_cam_ctx_fetch_job(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_pool_job pool_job;

	if (WARN_ON(mtk_cam_pool_fetch(&ctx->job_pool,
				       &pool_job, sizeof(pool_job))))
		return NULL;

	pool_job.job_data->pool_job = pool_job;
	return data_to_job(pool_job.job_data);
}

int mtk_cam_dev_req_enqueue(struct mtk_cam_device *cam,
			    struct mtk_cam_request *req)
{
	int used_ctx = req->used_ctx;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_job *job;
	int i;

	WARN_ON(!used_ctx);

	/*
	 * for each context involved:
	 *   pack into job
	 *     fetch ipi/cq buffer
	 *   select hw resources
	 *   ipi - config
	 *   ipi - frame
	 */
	for (i = 0; i < cam->max_stream_num && used_ctx; ++i) {

		if (!USED_MASK_HAS(&used_ctx, stream, i))
			continue;

		USED_MASK_UNSET(&used_ctx, stream, i);

		ctx = &cam->ctxs[i];

		job = mtk_cam_ctx_fetch_job(ctx);
		if (!job) {
			dev_info(cam->dev, "failed to get job from ctx %d\n",
				 ctx->stream_id);
			return -1;
		}

		if (mtk_cam_job_pack(job, ctx, req)) {
			mtk_cam_job_return(job);
			return -1;
		}

		// enque to ctrl ; job will send ipi
		mtk_cam_ctrl_job_enque(&ctx->cam_ctrl, job);
	}

	return 0;
}

static void dump_request_status(struct mtk_cam_request *cam_req,
				bool check_buffer)
{
	struct media_request *req = &cam_req->req;
	struct device *dev = req->mdev->dev;
	unsigned long flags;
	int num_incomplete;
	struct mtk_cam_buffer *buf;

	/* media request internal */
	spin_lock_irqsave(&req->lock, flags);
	num_incomplete = req->num_incomplete_objects;
	spin_unlock_irqrestore(&req->lock, flags);

	if (!num_incomplete)
		return;

	dev_info(dev, "req:%s num_incomplete: %d\n",
		 req->debug_str, num_incomplete);

	if (check_buffer) {
		struct mtk_cam_video_device *node;

		spin_lock(&cam_req->buf_lock);
		list_for_each_entry(buf, &cam_req->buf_list, list) {
			node = mtk_cam_buf_to_vdev(buf);

			dev_info(dev, "not done buf: %s\n", node->desc.name);
		}
		spin_unlock(&cam_req->buf_lock);
	}
}

void mtk_cam_req_buffer_done(struct mtk_cam_request *req,
			     int pipe_id, int node_id, int buf_state, u64 ts)
{
	struct device *dev = req->req.mdev->dev;
	struct mtk_cam_video_device *node;
	struct mtk_cam_buffer *buf, *buf_prev;
	struct list_head done_list;
	bool is_buf_empty;
	bool debug_req_status = 0;

	INIT_LIST_HEAD(&done_list);
	is_buf_empty = false;

	spin_lock(&req->buf_lock);

	list_for_each_entry_safe(buf, buf_prev, &req->buf_list, list) {

		node = mtk_cam_buf_to_vdev(buf);

		if (node->uid.pipe_id != pipe_id)
			continue;

		if (node_id != -1 && node->uid.id != node_id)
			continue;

		list_del(&buf->list);
		list_add_tail(&buf->list, &done_list);
	}
	is_buf_empty = list_empty(&req->buf_list);

	spin_unlock(&req->buf_lock);

	dev_info(dev, "%s: req:%s pipe_id:%d, node_id:%d, ts:%lld, is_empty %d\n",
		 __func__, req->req.debug_str,
		 pipe_id, node_id, ts, is_buf_empty);

	if (list_empty(&done_list)) {
		dev_info(dev,
			 "%s: req:%s failed to find pipe_id:%d, node_id:%d, ts:%lld, is_empty %d\n",
			 __func__, req->req.debug_str,
			 pipe_id, node_id, ts, is_buf_empty);
		return;
	}

	if (is_buf_empty) {
		/* assume: all ctrls are finished before buffers */
		dev_info(dev, "%s: req:%s all buf done\n",
			 __func__, req->req.debug_str);

		// remove from running job list
		remove_frome_running_list(dev_get_drvdata(dev), req);
	}

	list_for_each_entry(buf, &done_list, list) {
		buf->vbb.vb2_buf.timestamp = ts;
		vb2_buffer_done(&buf->vbb.vb2_buf, buf_state);
	}

	/* note: it's dangerous to access req after vb2_buffer_done, debug only */
	if (unlikely(debug_req_status))
		dump_request_status(req, true);
}

static int get_ipi_id(int stream_id)
{
	int ipi_id = stream_id + CCD_IPI_ISP_MAIN;

	if (WARN_ON(ipi_id < CCD_IPI_ISP_MAIN || ipi_id > CCD_IPI_ISP_TRICAM))
		return -1;

	return ipi_id;
}

#if CCD_READY
int isp_composer_create_session(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtkcam_ipi_event event;
	struct mtkcam_ipi_session_cookie *session = &event.cookie;
	struct mtkcam_ipi_session_param	*session_data = &event.session_data;
	int ret;

	memset(&event, 0, sizeof(event));
	event.cmd_id = CAM_CMD_CREATE_SESSION;
	session->session_id = ctx->stream_id;

	session_data->workbuf.iova = ctx->cq_buffer.daddr;
	session_data->workbuf.ccd_fd = mtk_cam_device_buf_fd(&ctx->cq_buffer);
	session_data->workbuf.size = ctx->cq_buffer.size;

	session_data->msg_buf.iova = 0; /* no need */
	session_data->msg_buf.ccd_fd = mtk_cam_device_buf_fd(&ctx->ipi_buffer);
	session_data->msg_buf.size = ctx->ipi_buffer.size;

	ret = rpmsg_send(ctx->rpmsg_dev->rpdev.ept, &event, sizeof(event));
	dev_info(cam->dev,
		"%s: rpmsg_send id: %d, cq_buf(fd:%d,sz:%d, msg_buf(fd:%d,sz%d) ret(%d)\n",
		__func__, event.cmd_id, session_data->workbuf.ccd_fd,
		session_data->workbuf.size, session_data->msg_buf.ccd_fd,
		session_data->msg_buf.size,
		ret);

	return ret;
}

void isp_composer_destroy_session(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtkcam_ipi_event event;
	struct mtkcam_ipi_session_cookie *session = &event.cookie;

	memset(&event, 0, sizeof(event));
	event.cmd_id = CAM_CMD_DESTROY_SESSION;
	session->session_id = ctx->stream_id;
	rpmsg_send(ctx->rpmsg_dev->rpdev.ept, &event, sizeof(event));

	dev_info(cam->dev, "rpmsg_send: DESTROY_SESSION\n");
}

/* forward decl. */
static int isp_composer_handler(struct rpmsg_device *rpdev, void *data,
				int len, void *priv, u32 src);
static int isp_composer_init(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct device *dev = cam->dev;
	struct mtk_ccd *ccd;
	struct rproc_subdev *rpmsg_subdev;
	struct rpmsg_channel_info *msg = &ctx->rpmsg_channel;
	int ipi_id;

	/* Create message client */
	ccd = (struct mtk_ccd *)cam->rproc_handle->priv;
	rpmsg_subdev = ccd->rpmsg_subdev;

	ipi_id = get_ipi_id(ctx->stream_id);
	if (ipi_id < 0)
		return -EINVAL;

	ctx->ipi_id = ipi_id;

	snprintf(msg->name, RPMSG_NAME_SIZE, "mtk-camsys\%d", ctx->stream_id);
	msg->src = ctx->ipi_id;

	ctx->rpmsg_dev = mtk_create_client_msgdevice(rpmsg_subdev, msg);
	if (!ctx->rpmsg_dev)
		return -EINVAL;

	ctx->rpmsg_dev->rpdev.ept = rpmsg_create_ept(&ctx->rpmsg_dev->rpdev,
						     isp_composer_handler,
						     cam, *msg);
	if (IS_ERR(ctx->rpmsg_dev->rpdev.ept)) {
		dev_info(dev, "%s failed rpmsg_create_ept, ctx:%d\n",
			 __func__, ctx->stream_id);
		goto faile_release_msg_dev;
	}

	dev_info(dev, "%s initialized composer of ctx:%d\n",
		 __func__, ctx->stream_id);

	return 0;

faile_release_msg_dev:
	mtk_destroy_client_msgdevice(rpmsg_subdev, &ctx->rpmsg_channel);
	ctx->rpmsg_dev = NULL;
	return -EINVAL;
}

static void isp_composer_uninit(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_ccd *ccd = cam->rproc_handle->priv;

	mtk_destroy_client_msgdevice(ccd->rpmsg_subdev, &ctx->rpmsg_channel);
	ctx->rpmsg_dev = NULL;
}
#endif

__maybe_unused static int isp_composer_handle_ack(struct mtk_cam_device *cam,
				   struct mtkcam_ipi_event *ipi_msg)
{
	/* TODO */
	return 0;
}

static int mtk_cam_power_rproc(struct mtk_cam_device *cam, int on)
{
	int ret = 0;

	if (on) {
		WARN_ON(cam->rproc_handle);

		/* Get the remote proc device of composers */
		cam->rproc_handle = rproc_get_by_phandle(cam->rproc_phandle);
		if (!cam->rproc_handle) {
			dev_info(cam->dev, "fail to get rproc_handle\n");
			return -1;
		}

		/* Power on remote proc device of composers*/
		ret = rproc_boot(cam->rproc_handle);
		if (ret)
			dev_info(cam->dev, "failed to rproc_boot:%d\n", ret);
	} else {
		cam->rproc_handle = NULL;

		rproc_shutdown(cam->rproc_handle);
		rproc_put(cam->rproc_handle);
	}

	return ret;
}

static int mtk_cam_power_ctrl_ccu(struct device *dev, int on_off)
{
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);
	//struct mtk_camsys_dvfs *dvfs_info = &cam_dev->camsys_ctrl.dvfs_info;
	int ret;

	if (on_off) {
		ret = mtk_cam_get_ccu_phandle(cam_dev);
		if (ret)
			goto out;
		cam_dev->rproc_ccu_handle = rproc_get_by_phandle(cam_dev->rproc_ccu_phandle);
		if (cam_dev->rproc_ccu_handle == NULL) {
			dev_info(dev, "Get ccu handle fail\n");
			ret = PTR_ERR(cam_dev->rproc_ccu_handle);
			goto out;
		}

		ret = rproc_boot(cam_dev->rproc_ccu_handle);
		if (ret)
			dev_info(dev, "boot ccu rproc fail\n");

		//if (dvfs_info->reg_vmm) {
		//	if (regulator_enable(dvfs_info->reg_vmm)) {
		//		dev_info(dev, "regulator_enable fail\n");
		//		goto out;
		//	}
		//}
	} else {
		//if (dvfs_info->reg_vmm && regulator_is_enabled(dvfs_info->reg_vmm))
		//	regulator_disable(dvfs_info->reg_vmm);

		if (cam_dev->rproc_ccu_handle) {
			rproc_shutdown(cam_dev->rproc_ccu_handle);
			ret = 0;
		} else
			ret = -EINVAL;
	}
out:
	return ret;
}

static int mtk_cam_initialize(struct mtk_cam_device *cam)
{
	int ret;

	if (atomic_add_return(1, &cam->initialize_cnt) > 1)
		return 0;

	dev_info(cam->dev, "camsys initialize\n");

	mtk_cam_power_ctrl_ccu(cam->dev, 1);

	pm_runtime_get_sync(cam->dev); /* use resume_and_get instead */

	ret = mtk_cam_power_rproc(cam, 1);
	if (ret)
		return ret; //TODO: goto

	// TODO: debug_fs

	return ret;
}

static int mtk_cam_uninitialize(struct mtk_cam_device *cam)
{
	if (atomic_sub_and_test(1, &cam->initialize_cnt))
		return 0;

	dev_info(cam->dev, "camsys uninitialize\n");

	mtk_cam_power_rproc(cam, 0);
	pm_runtime_put_sync(cam->dev);

	mtk_cam_power_ctrl_ccu(cam->dev, 0);

	return 0;
}

static int isp_composer_handler(struct rpmsg_device *rpdev, void *data,
				int len, void *priv, u32 src)
{
	struct mtk_cam_device *cam = (struct mtk_cam_device *)priv;
	struct device *dev = cam->dev;
	struct mtkcam_ipi_event *ipi_msg;
	struct mtk_cam_ctx *ctx;

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
		//MTK_CAM_TRACE_BEGIN(BASIC, "ipi_frame_ack:%d",
		//		    ipi_msg->cookie.frame_no);
		mtk_cam_ctrl_job_composed(&ctx->cam_ctrl,
			ipi_msg->cookie.frame_no, &ipi_msg->ack_data.frame_result);

		//MTK_CAM_TRACE_END(BASIC);
		return 0;

	} else if (ipi_msg->ack_data.ack_cmd_id == CAM_CMD_DESTROY_SESSION) {
		ctx = &cam->ctxs[ipi_msg->cookie.session_id];
		complete(&ctx->session_complete);
		dev_info(dev, "%s:ctx(%d): session destroyed",
			 __func__, ctx->stream_id);
	}

	return 0;
}

static int mtk_cam_in_ctx(struct mtk_cam_ctx *ctx, struct media_entity *entity)
{
	return entity->pipe == &ctx->pipeline;
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

static struct mtk_cam_ctx *mtk_cam_get_ctx(struct mtk_cam_device *cam)
{
	struct mtk_cam_ctx *ctx;

	/* TODO: select available ctx & ctx pool */
	ctx = &cam->ctxs[0];

	dev_info(cam->dev, "%s: workaround. not implemented yet\n", __func__);
	return ctx;
}

static void mtk_cam_ctx_put(struct mtk_cam_ctx *ctx)
{
	/* todo */
}

static int _find_raw_sd_idx(struct mtk_raw_pipeline *raw, int num_raw,
			    struct v4l2_subdev *sd)
{
	int i;

	for (i = 0; i < num_raw; i++)
		if (sd == &raw->subdev)
			return i;
	return -1;
}

static void mtk_cam_ctx_match_pipe_subdevs(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_v4l2_pipelines *ppls = &ctx->cam->pipelines;
	struct v4l2_subdev **sd = ctx->pipe_subdevs;
	int idx;

	ctx->raw_subdev_idx = -1;

	while (*sd) {

		idx = _find_raw_sd_idx(ppls->raw, ppls->num_raw, *sd);
		if (idx >= 0)
			ctx->raw_subdev_idx = idx;

		/* TODO: camsv/mraw */

		++sd;
	}
}

static int mtk_cam_ctx_pipeline_start(struct mtk_cam_ctx *ctx,
				      struct media_entity *entity)
{
	struct device *dev = ctx->cam->dev;
	struct media_graph *graph;
	struct v4l2_subdev **target_sd;
	struct mtk_cam_video_device *mtk_vdev;
	int i, ret;

	ret = media_pipeline_start(entity, &ctx->pipeline);
	if (ret) {
		dev_info(dev,
			 "%s:failed %s in media_pipeline_start:%d\n",
			 __func__, entity->name, ret);
		return ret;
	}

	/* traverse to update used subdevs & number of nodes */
	graph = &ctx->pipeline.graph;
	media_graph_walk_start(graph, entity);

	i = 0;
	while ((entity = media_graph_walk_next(graph))) {
		dev_dbg(dev, "linked entity %s\n", entity->name);

		target_sd = NULL;

		switch (entity->function) {
		case MEDIA_ENT_F_IO_V4L: /* node */
			ctx->enabled_node_cnt++;
			mtk_vdev = media_entity_to_mtk_vdev(entity);
			if (is_raw_subdev(mtk_vdev->uid.pipe_id))
				ctx->has_raw_subdev = 1;

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
			dev_info(dev, "duplicated subdevs!!!\n");
			goto fail_stop_pipeline;
		}

		if (is_media_entity_v4l2_subdev(entity))
			*target_sd = media_entity_to_v4l2_subdev(entity);
	}

	mtk_cam_ctx_match_pipe_subdevs(ctx);

	return 0;

fail_stop_pipeline:
	media_pipeline_stop(entity);
	return -EPIPE;
}

static void mtk_cam_ctx_pipeline_stop(struct mtk_cam_ctx *ctx,
				      struct media_entity *entity)
{
	int i;

	media_pipeline_stop(entity);

	ctx->sensor = NULL;
	ctx->seninf = NULL;
	for (i = 0; i < MAX_PIPES_PER_STREAM; i++)
		ctx->pipe_subdevs[i] = NULL;
}

static int mtk_cam_ctx_alloc_workers(struct mtk_cam_ctx *ctx)
{
	struct device *dev = ctx->cam->dev;

	kthread_init_worker(&ctx->sensor_worker);
	ctx->sensor_worker_task = kthread_run(kthread_worker_fn,
					      &ctx->sensor_worker,
					      "sensor_worker-%d",
					      ctx->stream_id);
	if (IS_ERR(ctx->sensor_worker_task)) {
		dev_info(dev, "%s:ctx(%d): could not create sensor_worker_task\n",
			 __func__, ctx->stream_id);
		return -1;
	}

	sched_set_fifo(ctx->sensor_worker_task);

	ctx->composer_wq = alloc_ordered_workqueue(dev_name(dev),
						   WQ_HIGHPRI | WQ_FREEZABLE);
	if (!ctx->composer_wq) {
		dev_info(dev, "failed to alloc composer workqueue\n");
		goto fail_uninit_sensor_worker_task;
	}

	ctx->frame_done_wq =
			alloc_ordered_workqueue(dev_name(dev),
						WQ_HIGHPRI | WQ_FREEZABLE);
	if (!ctx->frame_done_wq) {
		dev_info(dev, "failed to alloc frame_done workqueue\n");
		goto fail_uninit_composer_wq;
	}

	return 0;

fail_uninit_sensor_worker_task:
	kthread_stop(ctx->sensor_worker_task);
	ctx->sensor_worker_task = NULL;
fail_uninit_composer_wq:
	destroy_workqueue(ctx->composer_wq);
	return -1;
}

static void mtk_cam_ctx_destroy_workers(struct mtk_cam_ctx *ctx)
{
	kthread_stop(ctx->sensor_worker_task);
	ctx->sensor_worker_task = NULL;

	destroy_workqueue(ctx->composer_wq);
	destroy_workqueue(ctx->frame_done_wq);
}

static int _alloc_pool(const char *name,
		       struct mtk_cam_device_buf *buf,
		       struct mtk_cam_pool *pool,
		       struct device *dev, int size, int num, bool cacheable)
{
	int total_size = size * num;
	struct dma_buf *dbuf;
	int ret;

	if (cacheable)
		dbuf = mtk_cam_cached_buffer_alloc(total_size);
	else
		dbuf = mtk_cam_noncached_buffer_alloc(total_size);

	if (!dbuf)
		return -1;

	mtk_dma_buf_set_name(dbuf, name);

	ret = mtk_cam_device_buf_init(buf, dbuf, dev, total_size)
		|| mtk_cam_device_buf_vmap(buf);

	/* since mtk_cam_device_buf already increase refcnt */
	dma_heap_buffer_free(dbuf);
	if (ret)
		return ret;

	ret = mtk_cam_buffer_pool_alloc(pool, buf, num);
	if (ret)
		goto fail_device_buf_uninit;

	return 0;

fail_device_buf_uninit:
	mtk_cam_device_buf_uninit(buf);
	return ret;
}

static void _destroy_pool(struct mtk_cam_device_buf *buf,
			  struct mtk_cam_pool *pool)
{
	mtk_cam_pool_destroy(pool);
	mtk_cam_device_buf_uninit(buf);
}

/* for cq working buffers */
#define CQ_BUF_SIZE  0x10000
#define CAM_CQ_BUF_NUM JOB_NUM_PER_STREAM
#define IPI_FRAME_BUF_SIZE ALIGN(sizeof(struct mtkcam_ipi_frame_param), SZ_1K)
static int mtk_cam_ctx_alloc_pool(struct mtk_cam_ctx *ctx)
{
	struct device *dev_to_attach;
	int ret;

	dev_to_attach = ctx->cam->engines.raw_devs[0];

	ret = _alloc_pool("CAM_MEM_CQ_ID", &ctx->cq_buffer, &ctx->cq_pool,
			  dev_to_attach, CQ_BUF_SIZE, CAM_CQ_BUF_NUM,
			  false);
	if (ret)
		return ret;

	ret = _alloc_pool("CAM_MEM_MSG_ID", &ctx->ipi_buffer, &ctx->ipi_pool,
			  dev_to_attach, IPI_FRAME_BUF_SIZE, CAM_CQ_BUF_NUM,
			  false);
	if (ret)
		goto fail_destroy_cq;

	return 0;

fail_destroy_cq:
	_destroy_pool(&ctx->cq_buffer, &ctx->cq_pool);
	return ret;
}

static void mtk_cam_ctx_destroy_pool(struct mtk_cam_ctx *ctx)
{
	_destroy_pool(&ctx->cq_buffer, &ctx->cq_pool);
	_destroy_pool(&ctx->ipi_buffer, &ctx->ipi_pool);
}

static int mtk_cam_ctx_prepare_session(struct mtk_cam_ctx *ctx)
{
	int ret;

	ret = isp_composer_init(ctx);
	if (ret)
		return ret;

	init_completion(&ctx->session_complete);
	ret = isp_composer_create_session(ctx);
	if (ret) {
		complete(&ctx->session_complete);
		isp_composer_uninit(ctx);
		return -EBUSY;
	}
	ctx->session_created = 1;
	return ret;
}

static int mtk_cam_ctx_unprepare_session(struct mtk_cam_ctx *ctx)
{
	struct device *dev = ctx->cam->dev;

	if (ctx->session_created) {
		int ret;

		dev_dbg(dev, "%s:ctx(%d): wait for session destroy\n",
			__func__, ctx->stream_id);

		isp_composer_destroy_session(ctx);

		ret = wait_for_completion_killable(&ctx->session_complete);
		if (ret < 0)
			dev_info(dev, "%s:ctx(%d): got killed\n",
				 __func__, ctx->stream_id);

		isp_composer_uninit(ctx);

		ctx->session_created = 0;
	}
	return 0;
}

static bool _ctx_find_subdev(struct mtk_cam_ctx *ctx, struct v4l2_subdev *target)
{
	struct v4l2_subdev *subdev;
	int j;

	for (j = 0, subdev = ctx->pipe_subdevs[0];
	     j < MAX_PIPES_PER_STREAM; j++, subdev++) {
		if (!subdev)
			break;

		if (subdev == target)
			return true;
	}

	return false;
}

static void mtk_cam_update_pipe_used(struct mtk_cam_ctx *ctx,
				     struct mtk_cam_v4l2_pipelines *ppls)
{
	int i;

	for (i = 0; i < ppls->num_raw; i++)
		if (_ctx_find_subdev(ctx, &ppls->raw[i].subdev))
			USED_MASK_SET(&ctx->used_pipe, raw, ppls->raw[i].id);

#ifdef NOT_YET
	for (i = 0; i < ppls->num_camsv; i++)
		if (_ctx_find_subdev(ctx, &ppls->camsv[i].subdev))
			USED_MASK_SET(&ctx->used_pipe, camsv, ppls->camsv[i].id);

	for (i = 0; i < ppls->num_mraw; i++)
		if (_ctx_find_subdev(ctx, &ppls->mraw[i].subdev))
			USED_MASK_SET(&ctx->used_pipe, mraw, ppls->mraw[i].id);
#endif

	dev_info(ctx->cam->dev, "%s: ctx %d pipe_used %x\n",
		 __func__, ctx->stream_id, ctx->used_pipe);
}

static void mtk_cam_ctx_reset(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	unsigned int stream_id = ctx->stream_id;

	/* clear all, except cam & stream_id */
	memset(ctx, 0, sizeof(*ctx));
	ctx->cam = cam;
	ctx->stream_id = stream_id;
}

static void config_pool_job(void *data, int index, void *element)
{
	struct mtk_cam_job_data *job_data = data;
	struct mtk_cam_pool_job *job_element = element;

	job_element->job_data = job_data + index;
}

static int mtk_cam_ctx_init_job_pool(struct mtk_cam_ctx *ctx)
{
	int ret;

	ret = mtk_cam_pool_alloc(&ctx->job_pool,
				 sizeof(struct mtk_cam_pool_job),
				 JOB_NUM_PER_STREAM);
	if (ret) {
		dev_info(ctx->cam->dev, "failed to alloc job pool of ctx %d\n",
			 ctx->stream_id);
		return ret;
	}

	return mtk_cam_pool_config(&ctx->job_pool, config_pool_job, ctx->jobs);
}

struct mtk_cam_ctx *mtk_cam_start_ctx(struct mtk_cam_device *cam,
				      struct mtk_cam_video_device *node)
{
	struct device *dev = cam->dev;
	struct media_entity *entity = &node->vdev.entity;
	struct mtk_cam_ctx *ctx;

	ctx = mtk_cam_find_ctx(cam, entity);
	if (ctx) /* has been started */
		return ctx;

	dev_info(dev, "%s: by node %s\n", __func__, entity->name);

	if (mtk_cam_initialize(cam) < 0)
		return NULL;

	ctx = mtk_cam_get_ctx(cam);
	if (!ctx)
		goto fail_uninitialze;

	mtk_cam_ctx_reset(ctx);

	if (mtk_cam_ctx_pipeline_start(ctx, entity))
		goto fail_ctx_put;

	if (mtk_cam_ctx_alloc_workers(ctx))
		goto fail_pipeline_stop;

	if (mtk_cam_ctx_alloc_pool(ctx))
		goto fail_destroy_workers;

	if (ctx->has_raw_subdev) {
		if (mtk_cam_ctx_prepare_session(ctx))
			goto fail_destroy_pools;
	}

	if (mtk_cam_ctx_init_job_pool(ctx))
		goto fail_unprepare_session;

	mtk_cam_update_pipe_used(ctx, &cam->pipelines);
	mtk_cam_ctrl_start(&ctx->cam_ctrl, ctx);

	WARN_ON(mtk_cam_mark_streaming(cam, ctx->stream_id));

	return ctx;

fail_unprepare_session:
	mtk_cam_ctx_unprepare_session(ctx);
fail_destroy_pools:
	mtk_cam_ctx_destroy_pool(ctx);
fail_destroy_workers:
	mtk_cam_ctx_destroy_workers(ctx);
fail_pipeline_stop:
	mtk_cam_ctx_pipeline_stop(ctx, entity);
fail_ctx_put:
	mtk_cam_ctx_put(ctx);
fail_uninitialze:
	mtk_cam_uninitialize(cam);

	WARN(1, "%s: failed\n", __func__);
	return NULL;
}

void mtk_cam_stop_ctx(struct mtk_cam_ctx *ctx, struct media_entity *entity)
{
	struct mtk_cam_device *cam = ctx->cam;

	if (!mtk_cam_in_ctx(ctx, entity))
		return;

	dev_info(cam->dev, "%s: by node %s\n", __func__, entity->name);

	WARN_ON(mtk_cam_unmark_streaming(cam, ctx->stream_id));

	mtk_cam_ctrl_stop(&ctx->cam_ctrl);

	/* note: should await all jobs composer's ack before unprepare session */
	mtk_cam_ctx_unprepare_session(ctx);
	mtk_cam_ctx_destroy_pool(ctx);
	mtk_cam_ctx_destroy_workers(ctx);
	mtk_cam_ctx_pipeline_stop(ctx, entity);
	mtk_cam_pool_destroy(&ctx->job_pool);

	if (ctx->used_engine) {
		mtk_cam_pm_runtime_engines(&cam->engines, ctx->used_engine, 0);
		mtk_cam_release_engine(ctx->cam, ctx->used_engine);
	}

	ctx->used_pipe = 0;

	mtk_cam_uninitialize(cam);
}

int mtk_cam_ctx_all_nodes_streaming(struct mtk_cam_ctx *ctx)
{
	return ctx->streaming_node_cnt == ctx->enabled_node_cnt;
}

int mtk_cam_ctx_all_nodes_idle(struct mtk_cam_ctx *ctx)
{
	return ctx->streaming_node_cnt == 0;
}

static int ctx_stream_on_pipe_subdev(struct mtk_cam_ctx *ctx, int enable)
{
	struct device *dev = ctx->cam->dev;
	int i, ret;

	for (i = 0; i < MAX_PIPES_PER_STREAM && ctx->pipe_subdevs[i]; i++) {

		ret = v4l2_subdev_call(ctx->pipe_subdevs[i], video,
				       s_stream, enable);
		if (ret) {
			dev_info(dev, "failed to stream_on %d, %d: %d\n",
				 ctx->pipe_subdevs[i]->name, enable, ret);
			goto fail_pipe_off;
		}
	}

	return ret;

fail_pipe_off:

	if (enable)
		for (i = i - 1; i >= 0 && ctx->pipe_subdevs[i]; i--)
			v4l2_subdev_call(ctx->pipe_subdevs[i], video,
					 s_stream, 0);
	return ret;
}

#ifdef NOT_USE_YET
int PipeIDtoTGIDX(int pipe_id)
{
	/* camsv/mraw's cammux id is defined in its own dts */

	switch (pipe_id) {
	case MTKCAM_SUBDEV_RAW_0:
		return 0;
	case MTKCAM_SUBDEV_RAW_1:
		return 1;
	case MTKCAM_SUBDEV_RAW_2:
		return 2;
	default:
		break;
	}
	return -1;
}
#endif

int ctx_stream_on_seninf_sensor(struct mtk_cam_ctx *ctx, int enable)
{
	struct device *dev = ctx->cam->dev;
	struct v4l2_subdev *seninf = ctx->seninf;
	int seninf_pad;
	int pixel_mode;
	int tg_idx;
	int ret;


	if (WARN_ON(!seninf))
		return -1;

	/* RAW */
	seninf_pad = PAD_SRC_RAW0;
	pixel_mode = 1; /* FIXME: pixel mode */
	tg_idx = 0; // PipeIDtoTGIDX(raw_dev->id)


	mtk_cam_seninf_set_camtg(seninf, seninf_pad, tg_idx);

	ret = mtk_cam_seninf_set_pixelmode(seninf, seninf_pad, pixel_mode);

	ret = v4l2_subdev_call(ctx->seninf, video, s_stream, enable);
	if (ret) {
		dev_info(dev, "ctx %d failed to stream_on %s %d:%d\n",
			 ctx->stream_id, seninf->name, enable, ret);
		return -EPERM;
	}
	return ret;
}

int mtk_cam_ctx_stream_on(struct mtk_cam_ctx *ctx)
{
	int ret;

	dev_info(ctx->cam->dev, "%s: stream %d\n", __func__, ctx->stream_id);

	/* if already stream on */
	if (atomic_cmpxchg(&ctx->streaming, 0, 1))
		return 0;

	ret = ctx_stream_on_pipe_subdev(ctx, 1);

	mtk_cam_dev_req_try_queue(ctx->cam);

	//ctx_stream_on_seninf_sensor later
	mtk_cam_dvfs_update_clk(ctx->cam);
	/* note
	 * 1. collect 1st request info
	 * 2. select HW engines
	 * 3. set cam mux
	 * 4. stream on seninf
	 */

	return 0;
}

int mtk_cam_ctx_stream_off(struct mtk_cam_ctx *ctx)
{

	/* if already stream off */
	if (!atomic_cmpxchg(&ctx->streaming, 1, 0))
		return 0;

	/* TODO */

	// seninf
	ctx_stream_on_seninf_sensor(ctx, 0);

	ctx_stream_on_pipe_subdev(ctx, 0);

	return 0;
}

int mtk_cam_ctx_send_raw_event(struct mtk_cam_ctx *ctx,
			       struct v4l2_event *event)
{
	struct v4l2_subdev *sd;

	WARN_ON(ctx->raw_subdev_idx < 0);

	sd = ctx->pipe_subdevs[ctx->raw_subdev_idx];
	v4l2_event_queue(sd->devnode, event);
	return 0;
}

void mtk_cam_ctx_job_finish(struct mtk_cam_ctx *ctx, struct mtk_cam_job *job)
{
	CALL_JOB(job, finalize);
	mtk_cam_job_return(job);
}

static int _dynamic_link_seninf_pipe(struct device *dev,
				     struct media_entity *seninf, u16 src_pad,
				     struct media_entity *pipe, u16 sink_pad)
{
	int ret;

	dev_info(dev, "create pad link %s %s\n", seninf->name, pipe->name);
	ret = media_create_pad_link(seninf, src_pad,
				    pipe, sink_pad,
				    MEDIA_LNK_FL_DYNAMIC);
	if (ret) {
		dev_info(dev, "failed to create pad link %s %s err:%d\n",
			 seninf->name, pipe->name, ret);
		return ret;
	}

	return 0;
}

static int config_bridge_pad_links(struct mtk_cam_device *cam,
				   struct v4l2_subdev *seninf)
{
	struct device *dev = cam->dev;
	struct mtk_cam_v4l2_pipelines *ppl = &cam->pipelines;
	struct media_entity *pipe_entity;
	unsigned int i;//, j;
	int ret;

	/* seninf <-> raw */

	for (i = 0; i < ppl->num_raw; i++) {
		pipe_entity = &ppl->raw[i].subdev.entity;

		ret = _dynamic_link_seninf_pipe(dev,
						&seninf->entity,
						PAD_SRC_RAW0,
						pipe_entity,
						MTK_RAW_SINK);
		if (ret)
			return ret;
	}

	/* seninf <-> camsv */
#ifdef NOT_READY
	for (i = 0; i < ppl->num_camsv; i++) {
		pipe_entity = &ppl->camsv[i].subdev.entity;

		ret = _dynamic_link_seninf_pipe(dev,
						&seninf->entity,
						PAD_SRC_RAW0,
						pipe_entity,
						MTK_CAMSV_SINK);
		if (ret)
			return ret;

#if PDAF_READY
		for (j = PAD_SRC_PDAF0; j <= PAD_SRC_PDAF5; j++) {
			ret = _dynamic_link_seninf_pipe(dev,
							&seninf->entity,
							j,
							pipe_entity,
							MTK_CAMSV_SINK);
			if (ret)
				return ret;
		}
#endif
	}

	/* seninf <-> mraw */

	for (i = 0; i < ppl->num_mraw; i++) {
		for (j = PAD_SRC_PDAF0; j <= PAD_SRC_PDAF5; j++) {
			ret = _dynamic_link_seninf_pipe(dev,
							&seninf->entity,
							j,
							pipe_entity,
							MTK_MRAW_SINK);
			if (ret)
				return ret;
		}
	}
#endif

	return 0;
}

static int mtk_cam_create_links(struct mtk_cam_device *cam)
{
	struct v4l2_subdev *sd;
	int i, num;
	int ret;

	num = cam->engines.num_seninf_devices;
	i = 0;
	v4l2_device_for_each_subdev(sd, &cam->v4l2_dev) {
		if (i < num &&
		    sd->entity.function == MEDIA_ENT_F_VID_IF_BRIDGE) {
			ret = config_bridge_pad_links(cam, sd);
			i++;
		}
	}

	return ret;
}

static int mtk_alloc_pipelines(struct mtk_cam_device *cam)
{
	struct mtk_cam_v4l2_pipelines *ppls = &cam->pipelines;
	int ret = 0;

	ppls->num_raw = GET_PLAT_V4L2(raw_pipeline_num);
	ppls->raw = mtk_raw_pipeline_create(cam->dev, ppls->num_raw);
	if (ppls->num_raw && !ppls->raw) {
		dev_info(cam->dev, "%s: failed at alloc raw pipelines: %d\n",
			 __func__, ppls->num_raw);
		ret = -ENOMEM;
	}

	dev_info(cam->dev, "pipeline num: raw %d camsv %d mraw %d\n",
		 ppls->num_raw,
		 ppls->num_camsv,
		 ppls->num_mraw);
	return ret;
}

static int mtk_cam_master_bind(struct device *dev)
{
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);
	struct media_device *media_dev = &cam_dev->media_dev;
	int ret;

	//dev_info(dev, "%s\n", __func__);

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

	ret = mtk_raw_setup_dependencies(&cam_dev->engines);
	if (ret) {
		dev_dbg(dev, "Failed to mtk_raw_setup_dependencies: %d\n", ret);
		goto fail_unbind_all;
	}

#ifdef NOT_READY
	ret = mtk_camsv_setup_dependencies(&cam_dev->sv, &cam_dev->larb);
	if (ret) {
		dev_dbg(dev, "Failed to mtk_camsv_setup_dependencies: %d\n", ret);
		goto fail_remove_dependencies;
	}

	ret = mtk_mraw_setup_dependencies(&cam_dev->mraw, &cam_dev->larb);
	if (ret) {
		dev_dbg(dev, "Failed to mtk_mraw_setup_dependencies: %d\n", ret);
		goto fail_remove_dependencies;
	}
#endif

	ret = mtk_alloc_pipelines(cam_dev);
	if (ret)
		dev_info(dev, "failed to update pipeline num\n");


	ret = mtk_raw_register_entities(cam_dev->pipelines.raw,
					cam_dev->pipelines.num_raw,
					&cam_dev->v4l2_dev);
	if (ret) {
		dev_dbg(dev, "Failed to init raw subdevs: %d\n", ret);
		goto fail_remove_dependencies;
	}

#ifdef NOT_READY
	ret = mtk_camsv_register_entities(&cam_dev->sv, &cam_dev->v4l2_dev);
	if (ret) {
		dev_dbg(dev, "Failed to init camsv subdevs: %d\n", ret);
		goto fail_unreg_raw_entities;
	}

	ret = mtk_mraw_register_entities(&cam_dev->mraw, &cam_dev->v4l2_dev);
	if (ret) {
		dev_dbg(dev, "Failed to init mraw subdevs: %d\n", ret);
		goto fail_unreg_camsv_entities;
	}
#endif

	mtk_cam_create_links(cam_dev);
	/* Expose all subdev's nodes */
	ret = v4l2_device_register_subdev_nodes(&cam_dev->v4l2_dev);
	if (ret) {
		dev_dbg(dev, "Failed to register subdev nodes\n");
		goto fail_unreg_mraw_entities;
	}

	mtk_cam_dvfs_init(cam_dev);

	dev_info(dev, "%s success\n", __func__);
	return 0;

fail_unreg_mraw_entities:
#ifdef NOT_READY
	mtk_mraw_unregister_entities(&cam_dev->mraw);

fail_unreg_camsv_entities:
	mtk_camsv_unregister_entities(&cam_dev->sv);

fail_unreg_raw_entities:
#endif
	mtk_raw_unregister_entities(cam_dev->pipelines.raw,
				    cam_dev->pipelines.num_raw);

fail_remove_dependencies:
	/* nothing to do for now */

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

	//dev_info(dev, "%s\n", __func__);

	mtk_raw_unregister_entities(cam_dev->pipelines.raw,
				    cam_dev->pipelines.num_raw);
#ifdef NOT_READY
	mtk_camsv_unregister_entities(&cam_dev->sv);
	//TODO: mraw
#endif
	mtk_cam_dvfs_uninit(cam_dev);
	component_unbind_all(dev, cam_dev);

	media_device_unregister(&cam_dev->media_dev);
	v4l2_device_unregister(&cam_dev->v4l2_dev);
	media_device_cleanup(&cam_dev->media_dev);
}

static int compare_dev(struct device *dev, void *data)
{
	return dev == (struct device *)data;
}

static void mtk_cam_match_remove(struct device *dev)
{
	(void) dev;
}

static int add_match_by_driver(struct device *dev,
			       struct component_match **match,
			       struct platform_driver *drv)
{
	struct device *p = NULL, *d;
	int num = 0;

	do {
		d = platform_find_device_by_driver(p, &drv->driver);
		put_device(p);
		p = d;
		if (!d)
			break;

		component_match_add(dev, match, compare_dev, d);
		num++;
	} while (true);

	return num;
}

static int mtk_cam_alloc_for_engine(struct device *dev)
{
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);
	struct mtk_cam_engines *eng = &cam_dev->engines;
	struct device **dev_arr;
	int num = eng->num_raw_devices * 2 /* raw + yuv */
		+ eng->num_camsv_devices
		+ eng->num_mraw_devices
		+ eng->num_larb_devices;

	dev_arr = devm_kzalloc(dev, sizeof(*dev) * num, GFP_KERNEL);
	if (!dev_arr)
		return -ENOMEM;

	eng->raw_devs = dev_arr;
	dev_arr += eng->num_raw_devices;

	eng->yuv_devs = dev_arr;
	dev_arr += eng->num_raw_devices;

	eng->sv_devs = dev_arr;
	dev_arr += eng->num_camsv_devices;

	eng->mraw_devs = dev_arr;
	dev_arr += eng->num_mraw_devices;

	eng->larb_devs = dev_arr;
	dev_arr += eng->num_larb_devices;

	return 0;
}

static struct component_match *mtk_cam_match_add(struct device *dev)
{
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);
	struct mtk_cam_engines *eng = &cam_dev->engines;
	struct component_match *match = NULL;
	int yuv_num;

	eng->num_raw_devices =
		add_match_by_driver(dev, &match, &mtk_cam_raw_driver);

	yuv_num = add_match_by_driver(dev, &match, &mtk_cam_yuv_driver);

	eng->num_larb_devices =
		add_match_by_driver(dev, &match, &mtk_cam_larb_driver);

#ifdef NOT_READY
	eng->num_camsv_devices =
		add_match_by_driver(dev, &match, &mtk_cam_sv_driver);

	eng->num_mraw_devices =
		add_match_by_driver(dev, &match, &mtk_cam_mraw_driver);
#endif

	eng->num_seninf_devices =
		add_match_by_driver(dev, &match, &seninf_pdrv);

	if (IS_ERR(match) || mtk_cam_alloc_for_engine(dev))
		mtk_cam_match_remove(dev);

	dev_info(dev, "#: raw %d, yuv %d, larb %d, sv %d, seninf %d, mraw %d\n",
		 eng->num_raw_devices, yuv_num,
		 eng->num_larb_devices,
		 eng->num_camsv_devices,
		 eng->num_seninf_devices,
		 eng->num_mraw_devices);

	return match ? match : ERR_PTR(-ENODEV);
}

static const struct component_master_ops mtk_cam_master_ops = {
	.bind = mtk_cam_master_bind,
	.unbind = mtk_cam_master_unbind,
};

static void mtk_cam_ctx_init(struct mtk_cam_ctx *ctx,
			     struct mtk_cam_device *cam,
			     unsigned int stream_id)
{
	ctx->cam = cam;
	ctx->stream_id = stream_id;
}

static int mtk_cam_v4l2_subdev_link_validate(struct v4l2_subdev *sd,
				      struct media_link *link,
				      struct v4l2_subdev_format *source_fmt,
				      struct v4l2_subdev_format *sink_fmt,
				      bool bypass_size_check)
{
	bool pass = true;

	/* The width, height and code must match. */
	if (source_fmt->format.width != sink_fmt->format.width) {
		dev_dbg(sd->entity.graph_obj.mdev->dev,
			"%s: width does not match (source %u, sink %u)\n",
			__func__,
			source_fmt->format.width, sink_fmt->format.width);
		pass = (bypass_size_check) ? true : false;
	}

	if (source_fmt->format.height != sink_fmt->format.height) {
		dev_dbg(sd->entity.graph_obj.mdev->dev,
			"%s: height does not match (source %u, sink %u)\n",
			__func__,
			source_fmt->format.height, sink_fmt->format.height);
		pass = (bypass_size_check) ? true : false;
	}

	if (source_fmt->format.code != sink_fmt->format.code) {
		dev_info(sd->entity.graph_obj.mdev->dev,
			"%s: warn: media bus code does not match (source 0x%8.8x, sink 0x%8.8x)\n",
			__func__,
			source_fmt->format.code, sink_fmt->format.code);
	}

	if (pass)
		return 0;

	dev_dbg(sd->entity.graph_obj.mdev->dev,
		"%s: link was \"%s\":%u -> \"%s\":%u\n", __func__,
		link->source->entity->name, link->source->index,
		link->sink->entity->name, link->sink->index);

	return -EPIPE;
}

int mtk_cam_link_validate(struct v4l2_subdev *sd,
			  struct media_link *link,
			  struct v4l2_subdev_format *source_fmt,
			  struct v4l2_subdev_format *sink_fmt)
{
	struct device *dev;
	int ret = 0;

	dev = sd->v4l2_dev->dev;

	ret = mtk_cam_v4l2_subdev_link_validate(sd, link, source_fmt, sink_fmt, false);
	if (ret)
		dev_info(dev, "%s: link validate failed pad/code/w/h: SRC(%d/0x%x/%d/%d), SINK(%d:0x%x/%d/%d)\n",
			 sd->name, source_fmt->pad, source_fmt->format.code,
			 source_fmt->format.width, source_fmt->format.height,
			 sink_fmt->pad, sink_fmt->format.code,
			 sink_fmt->format.width, sink_fmt->format.height);

	return ret;
}

int mtk_cam_seninf_link_validate(struct v4l2_subdev *sd,
			  struct media_link *link,
			  struct v4l2_subdev_format *source_fmt,
			  struct v4l2_subdev_format *sink_fmt)
{
	struct device *dev;
	int ret = 0;

	dev = sd->v4l2_dev->dev;

	ret = mtk_cam_v4l2_subdev_link_validate(sd, link, source_fmt, sink_fmt, true);
	if (ret)
		dev_info(dev, "%s: link validate failed pad/code/w/h: SRC(%d/0x%x/%d/%d), SINK(%d:0x%x/%d/%d)\n",
			 sd->name, source_fmt->pad, source_fmt->format.code,
			 source_fmt->format.width, source_fmt->format.height,
			 sink_fmt->pad, sink_fmt->format.code,
			 sink_fmt->format.width, sink_fmt->format.height);

	return ret;
}

int mtk_cam_sv_link_validate(struct v4l2_subdev *sd,
			  struct media_link *link,
			  struct v4l2_subdev_format *source_fmt,
			  struct v4l2_subdev_format *sink_fmt)
{
	struct device *dev;
	int ret = 0;

	dev = sd->v4l2_dev->dev;

	ret = mtk_cam_v4l2_subdev_link_validate(sd, link, source_fmt, sink_fmt, true);
	if (ret)
		dev_info(dev, "%s: link validate failed pad/code/w/h: SRC(%d/0x%x/%d/%d), SINK(%d:0x%x/%d/%d)\n",
			 sd->name, source_fmt->pad, source_fmt->format.code,
			 source_fmt->format.width, source_fmt->format.height,
			 sink_fmt->pad, sink_fmt->format.code,
			 sink_fmt->format.width, sink_fmt->format.height);

	return ret;
}

int mtk_cam_mraw_link_validate(struct v4l2_subdev *sd,
			  struct media_link *link,
			  struct v4l2_subdev_format *source_fmt,
			  struct v4l2_subdev_format *sink_fmt)
{
	struct device *dev;
	int ret = 0;

	dev = sd->v4l2_dev->dev;

	ret = mtk_cam_v4l2_subdev_link_validate(sd, link, source_fmt, sink_fmt, true);
	if (ret)
		dev_info(dev, "%s: link validate failed pad/code/w/h: SRC(%d/0x%x/%d/%d), SINK(%d:0x%x/%d/%d)\n",
			 sd->name, source_fmt->pad, source_fmt->format.code,
			 source_fmt->format.width, source_fmt->format.height,
			 sink_fmt->pad, sink_fmt->format.code,
			 sink_fmt->format.width, sink_fmt->format.height);

	return ret;
}

static int mtk_cam_debug_fs_init(struct mtk_cam_device *cam)
{
#ifdef NOT_READY
	/**
	 * The dump buffer size depdends on the meta buffer size
	 * which is variable among devices using different type of sensors
	 * , e.g. PD's statistic buffers.
	 */
	int dump_mem_size = MTK_CAM_DEBUG_DUMP_HEADER_MAX_SIZE +
			    CQ_BUF_SIZE +
			    mtk_cam_get_meta_size(MTKCAM_IPI_RAW_META_STATS_CFG) +
			    RAW_STATS_CFG_VARIOUS_SIZE +
			    sizeof(struct mtkcam_ipi_frame_param) +
			    sizeof(struct mtkcam_ipi_config_param) *
			    RAW_PIPELINE_NUM;

	cam->debug_fs = mtk_cam_get_debugfs();
	if (!cam->debug_fs)
		return 0;

	cam->debug_fs->ops->init(cam->debug_fs, cam, dump_mem_size);
	cam->debug_wq = alloc_ordered_workqueue(dev_name(cam->dev),
						__WQ_LEGACY | WQ_MEM_RECLAIM |
						WQ_FREEZABLE);
	cam->debug_exception_wq = alloc_ordered_workqueue(dev_name(cam->dev),
						__WQ_LEGACY | WQ_MEM_RECLAIM |
						WQ_FREEZABLE);
	init_waitqueue_head(&cam->debug_exception_waitq);

	if (!cam->debug_wq || !cam->debug_exception_wq)
		return -EINVAL;
#endif
	return 0;
}

static void mtk_cam_debug_fs_deinit(struct mtk_cam_device *cam)
{
#ifdef NOT_READY
	drain_workqueue(cam->debug_wq);
	destroy_workqueue(cam->debug_wq);
	drain_workqueue(cam->debug_exception_wq);
	destroy_workqueue(cam->debug_exception_wq);

	if (cam->debug_fs)
		cam->debug_fs->ops->deinit(cam->debug_fs);
#endif
}

int mtk_cam_mark_streaming(struct mtk_cam_device *cam, int stream_id)
{
	int stream_mask = 1 << stream_id;
	int err_stream;

	spin_lock(&cam->streaming_lock);
	err_stream = cam->streaming_ctx & stream_mask;
	if (!err_stream)
		cam->streaming_ctx |= stream_mask;
	spin_unlock(&cam->streaming_lock);

	if (err_stream)
		dev_info(cam->dev, "%s: streams 0x%x are already streaming\n",
			 __func__, err_stream);
	return err_stream ? -1 : 0;
}

int mtk_cam_unmark_streaming(struct mtk_cam_device *cam, int stream_id)
{
	int stream_mask = 1 << stream_id;
	int err_stream;

	spin_lock(&cam->streaming_lock);
	err_stream = (cam->streaming_ctx & stream_mask) ^ stream_mask;
	cam->streaming_ctx &= ~stream_mask;
	spin_unlock(&cam->streaming_lock);

	if (err_stream)
		dev_info(cam->dev, "%s: streams 0x%x are not streaming\n",
			 __func__, err_stream);
	return err_stream ? -1 : 0;
}

bool mtk_cam_is_any_streaming(struct mtk_cam_device *cam)
{
	bool res;

	spin_lock(&cam->streaming_lock);
	res = !!cam->streaming_ctx;
	spin_unlock(&cam->streaming_lock);

	return res;
}

bool mtk_cam_are_all_streaming(struct mtk_cam_device *cam, int stream_mask)
{
	int streaming_ctx;
	bool res;

	spin_lock(&cam->streaming_lock);
	streaming_ctx = cam->streaming_ctx;
	spin_unlock(&cam->streaming_lock);

	res = USED_MASK_CONTAINS(&streaming_ctx, &stream_mask);

	if (!res)
		dev_info(cam->dev,
			 "%s: ctx not ready: streaming 0x%x desried 0x%x\n",
			 __func__, streaming_ctx, stream_mask);
	return res;
}

static int get_engine_full_set(struct mtk_cam_engines *engines)
{
	int set, i;

	for (i = 0; i < engines->num_raw_devices; i++)
		USED_MASK_SET(&set, raw, i);
	for (i = 0; i < engines->num_camsv_devices; i++)
		USED_MASK_SET(&set, camsv, i);
	for (i = 0; i < engines->num_mraw_devices; i++)
		USED_MASK_SET(&set, mraw, i);
	return set;
}

int mtk_cam_get_available_engine(struct mtk_cam_device *cam)
{
	int full_set, occupied;

	spin_lock(&cam->streaming_lock);
	occupied = cam->engines.occupied_engine;
	spin_unlock(&cam->streaming_lock);

	full_set = get_engine_full_set(&cam->engines);
	return full_set & ~occupied;
}

int mtk_cam_update_engine_status(struct mtk_cam_device *cam, int engine_mask,
				 bool available)
{
	int err_mask, occupied;

	spin_lock(&cam->streaming_lock);

	occupied = cam->engines.occupied_engine;
	if (available) {
		err_mask = (occupied & engine_mask) ^ engine_mask;
		occupied &= ~engine_mask;
	} else {
		err_mask = occupied & engine_mask;
		occupied |= engine_mask;
	}

	if (!err_mask)
		cam->engines.occupied_engine = occupied;

	spin_unlock(&cam->streaming_lock);

	if (WARN_ON(err_mask)) {
		dev_info(cam->dev, "%s: set %d, engine 0x%08x err 0x%08x\n",
			 __func__, available, engine_mask);
		return -1;
	}

	dev_info(cam->dev, "%s: mark engine 0x%08x available %d\n",
		 __func__, engine_mask, available);
	return 0;
}

static int loop_each_engine(struct mtk_cam_engines *eng,
			    int engine_mask, int (*func)(struct device *dev))
{
	int engine_used = engine_mask;
	int submask;
	int i;

	submask = USED_MASK_GET_SUBMASK(&engine_used, raw);
	for (i = 0; i < eng->num_raw_devices && submask; i++) {
		if (!USED_MASK_HAS(&submask, raw, i))
			continue;
		func(eng->raw_devs[i]);
	}

	submask = USED_MASK_GET_SUBMASK(&engine_used, camsv);
	for (i = 0; i < eng->num_camsv_devices; i++) {
		if (!USED_MASK_HAS(&submask, camsv, i))
			continue;
		func(eng->sv_devs[i]);
	}

	submask = USED_MASK_GET_SUBMASK(&engine_used, mraw);
	for (i = 0; i < eng->num_mraw_devices; i++) {
		if (!USED_MASK_HAS(&submask, mraw, i))
			continue;
		func(eng->mraw_devs[i]);
	}

	return 0;
}

int mtk_cam_pm_runtime_engines(struct mtk_cam_engines *eng,
			       int engine_mask, int enable)
{
	if (enable)
		loop_each_engine(eng, engine_mask, pm_runtime_resume_and_get);
	else
		loop_each_engine(eng, engine_mask, pm_runtime_put_sync);

	return 0;
}

static int register_sub_drivers(struct device *dev)
{
	struct component_match *match = NULL;
	int ret;

	ret = platform_driver_register(&mtk_cam_larb_driver);
	if (ret) {
		dev_info(dev, "%s mtk_cam_larb_driver fail\n", __func__);
		goto REGISTER_LARB_FAIL;
	}

	ret = platform_driver_register(&seninf_pdrv);
	if (ret) {
		dev_info(dev, "%s seninf_pdrv fail\n", __func__);
		goto REGISTER_SENINF_FAIL;
	}

	ret = platform_driver_register(&seninf_core_pdrv);
	if (ret) {
		dev_info(dev, "%s seninf_core_pdrv fail\n", __func__);
		goto REGISTER_SENINF_CORE_FAIL;
	}

#ifdef NOT_READY
	ret = platform_driver_register(&mtk_cam_sv_driver);
	if (ret) {
		dev_info(dev, "%s mtk_cam_sv_driver fail\n", __func__);
		goto REGISTER_CAMSV_FAIL;
	}

	ret = platform_driver_register(&mtk_cam_mraw_driver);
	if (ret) {
		dev_info(dev, "%s mtk_cam_mraw_driver fail\n", __func__);
		goto REGISTER_MRAW_FAIL;
	}
#endif

	ret = platform_driver_register(&mtk_cam_raw_driver);
	if (ret) {
		dev_info(dev, "%s mtk_cam_raw_driver fail\n", __func__);
		goto REGISTER_RAW_FAIL;
	}

	ret = platform_driver_register(&mtk_cam_yuv_driver);
	if (ret) {
		dev_info(dev, "%s mtk_cam_raw_driver fail\n", __func__);
		goto REGISTER_YUV_FAIL;
	}

	match = mtk_cam_match_add(dev);
	if (IS_ERR(match)) {
		ret = PTR_ERR(match);
		goto ADD_MATCH_FAIL;
	}

	ret = component_master_add_with_match(dev, &mtk_cam_master_ops, match);
	if (ret < 0)
		goto MASTER_ADD_MATCH_FAIL;

	return 0;

MASTER_ADD_MATCH_FAIL:
	mtk_cam_match_remove(dev);

ADD_MATCH_FAIL:
	platform_driver_unregister(&mtk_cam_yuv_driver);

REGISTER_YUV_FAIL:
	platform_driver_unregister(&mtk_cam_raw_driver);

REGISTER_RAW_FAIL:
	//platform_driver_unregister(&mtk_cam_mraw_driver);

//REGISTER_MRAW_FAIL:
	//platform_driver_unregister(&mtk_cam_sv_driver);

//REGISTER_CAMSV_FAIL:
	platform_driver_unregister(&seninf_core_pdrv);

REGISTER_SENINF_CORE_FAIL:
	platform_driver_unregister(&seninf_pdrv);

REGISTER_SENINF_FAIL:
	platform_driver_unregister(&mtk_cam_larb_driver);

REGISTER_LARB_FAIL:
	return ret;
}

static int mtk_cam_probe(struct platform_device *pdev)
{
	struct mtk_cam_device *cam_dev;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret;
	unsigned int i;
	const struct camsys_platform_data *platform_data;

	//dev_info(dev, "%s\n", __func__);
	platform_data = of_device_get_match_data(dev);
	if (!platform_data) {
		dev_info(dev, "Error: failed to get match data\n");
		return -ENODEV;
	}
	set_platform_data(platform_data);
	dev_info(dev, "platform = %s\n", platform_data->platform);

	camsys_root_dev = dev;

	/* initialize structure */
	cam_dev = devm_kzalloc(dev, sizeof(*cam_dev), GFP_KERNEL);
	if (!cam_dev)
		return -ENOMEM;

	if (dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34)))
		dev_info(dev, "%s: No suitable DMA available\n", __func__);

	if (!dev->dma_parms) {
		dev->dma_parms =
			devm_kzalloc(dev, sizeof(*dev->dma_parms), GFP_KERNEL);
		if (!dev->dma_parms)
			return -ENOMEM;
	}

	if (dev->dma_parms) {
		ret = dma_set_max_seg_size(dev, UINT_MAX);
		if (ret)
			dev_info(dev, "Failed to set DMA segment size\n");
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_info(dev, "failed to get mem\n");
		return -ENODEV;
	}

	cam_dev->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(cam_dev->base)) {
		dev_dbg(dev, "failed to map register base\n");
		return PTR_ERR(cam_dev->base);
	}

	cam_dev->dev = dev;
	dev_set_drvdata(dev, cam_dev);

	/* FIXME: decide max raw stream num by seninf num */
	cam_dev->max_stream_num = 8; /* TODO: how */
	cam_dev->ctxs = devm_kcalloc(dev, cam_dev->max_stream_num,
				     sizeof(*cam_dev->ctxs), GFP_KERNEL);
	if (!cam_dev->ctxs)
		return -ENOMEM;

	for (i = 0; i < cam_dev->max_stream_num; i++)
		mtk_cam_ctx_init(cam_dev->ctxs + i, cam_dev, i);

	spin_lock_init(&cam_dev->streaming_lock);
	spin_lock_init(&cam_dev->pending_job_lock);
	spin_lock_init(&cam_dev->running_job_lock);
	INIT_LIST_HEAD(&cam_dev->pending_job_list);
	INIT_LIST_HEAD(&cam_dev->running_job_list);

	pm_runtime_enable(dev);

	ret = mtk_cam_of_rproc(cam_dev);
	if (ret)
		goto fail_return;

	ret = register_sub_drivers(dev);
	if (ret) {
		dev_info(dev, "fail to register_sub_drivers\n");
		goto fail_return;
	}

	ret = mtk_cam_debug_fs_init(cam_dev);
	if (ret < 0)
		goto fail_match_remove;

	return 0;

fail_match_remove:
	mtk_cam_match_remove(dev);

fail_return:

	return ret;
}

static int mtk_cam_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);

	pm_runtime_disable(dev);

	component_master_del(dev, &mtk_cam_master_ops);
	mtk_cam_match_remove(dev);

	mtk_cam_debug_fs_deinit(cam_dev);

#ifdef NOT_READY
	platform_driver_unregister(&mtk_cam_mraw_driver);
	platform_driver_unregister(&mtk_cam_sv_driver);
#endif
	platform_driver_unregister(&mtk_cam_raw_driver);
	platform_driver_unregister(&mtk_cam_larb_driver);
	platform_driver_unregister(&seninf_core_pdrv);
	platform_driver_unregister(&seninf_pdrv);

	return 0;
}

static int mtk_cam_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "- %s\n", __func__);

	return 0;
}

static int mtk_cam_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "- %s\n", __func__);

	mtk_cam_timesync_init(true);

	return 0;
}

static const struct dev_pm_ops mtk_cam_pm_ops = {
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
