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

/* FIXME for CIO pad id */
#define MTK_CAM_CIO_PAD_SRC		PAD_SRC_RAW0
#define MTK_CAM_CIO_PAD_SINK		MTK_RAW_SINK

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

void mtk_cam_dev_try_queue(struct mtk_cam_device *cam)
{
	//TODO
	// 1. get runnable requests
	// 2. call mtk_cam_dev_req_enqueue
}

static struct media_request *mtk_cam_req_alloc(struct media_device *mdev)
{
	struct mtk_cam_request *cam_req;

	cam_req = vzalloc(sizeof(*cam_req));
	//spin_lock_init(&cam_req->done_status_lock);
	//mutex_init(&cam_req->fs.op_lock);

	return &cam_req->req;
}

static void mtk_cam_req_free(struct media_request *req)
{
	struct mtk_cam_request *cam_req = to_mtk_cam_req(req);

	vfree(cam_req);
}

static void mtk_cam_req_queue(struct media_request *req)
{
	struct mtk_cam_request *cam_req = to_mtk_cam_req(req);
	struct mtk_cam_device *cam =
		container_of(req->mdev, struct mtk_cam_device, media_dev);

	// reset req

	/* update frame_params's dma_bufs in mtk_cam_vb2_buf_queue */
	vb2_request_queue(req);

	spin_lock(&cam->pending_job_lock);
	list_add_tail(&cam_req->list, &cam->pending_job_list);
	spin_unlock(&cam->pending_job_lock);

	mtk_cam_dev_req_enqueue(cam, cam_req);
}

static int mtk_cam_link_notify(struct media_link *link, u32 flags,
			      unsigned int notification)
{
	//TODO
	return 0;
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

#ifdef NOT_READY
static void isp_tx_frame_worker(struct work_struct *work)
{
	//TODO
}
#endif

void mtk_cam_dev_req_enqueue(struct mtk_cam_device *cam,
			     struct mtk_cam_request *req)
{
	//TODO
	// collect info
	// packing into job
	// add to job list
}

#ifdef NOT_READY
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
	dev_dbg(cam->dev, "%s sw_feature:%d\n", __func__, config->sw_feature);

	rpmsg_send(ctx->rpmsg_dev->rpdev.ept, &event, sizeof(event));
	dev_dbg(cam->dev, "rpmsg_send id: %d\n", event.cmd_id);

	/* For debug dump file */
	//memcpy(&ctx->config_params, config_param, sizeof(*config_param));
	//dev_dbg(cam->dev, "%s:ctx(%d): save config_param to ctx, sz:%d\n",
	//	__func__, ctx->stream_id, sizeof(*config_param));
}
#endif

static int is_valid_ipi_id(int ipi_id)
{
	return !(ipi_id < CCD_IPI_ISP_MAIN || ipi_id > CCD_IPI_ISP_TRICAM);
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

static void isp_destroy_session_work(struct work_struct *work)
{
	struct mtk_cam_ctx *ctx = container_of(work, struct mtk_cam_ctx,
					       session_work);

	isp_composer_destroy_session(ctx);
}

__maybe_unused static void isp_composer_destroy_session_async(struct mtk_cam_ctx *ctx)
{
	struct work_struct *w = &ctx->session_work;

	INIT_WORK(w, isp_destroy_session_work);
	queue_work(ctx->composer_wq, w);
}

/* forward decl. */
static int isp_composer_handler(struct rpmsg_device *rpdev, void *data,
				int len, void *priv, u32 src);
__maybe_unused static int isp_composer_init(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct device *dev = cam->dev;
	struct mtk_ccd *ccd;
	struct rproc_subdev *rpmsg_subdev;
	struct rpmsg_channel_info *msg = &ctx->rpmsg_channel;

	/* Create message client */
	ccd = (struct mtk_ccd *)cam->rproc_handle->priv;
	rpmsg_subdev = ccd->rpmsg_subdev;

	if (!is_valid_ipi_id(ctx->stream_id)) {
		dev_info(dev, "invalid ipi_id %d\n", ctx->stream_id);
		return -EINVAL;
	}

	snprintf(msg->name, RPMSG_NAME_SIZE, "mtk-camsys\%d", ctx->stream_id);
	msg->src = ctx->stream_id;

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

__maybe_unused static void isp_composer_uninit(struct mtk_cam_ctx *ctx)
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

__maybe_unused static int mtk_cam_power_ctrl_ccu(struct device *dev, int on_off)
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

	//mtk_cam_power_ctrl_ccu(cam->dev, 1);

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

	//mtk_cam_power_ctrl_ccu(cam->dev, 0);

	return 0;
}

static int isp_composer_handler(struct rpmsg_device *rpdev, void *data,
				int len, void *priv, u32 src)
{
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

	ret = mtk_cam_device_buf_init(buf, dbuf, dev, total_size);

	/* since mtk_cam_device_buf already increase refcnt */
	dma_heap_buffer_free(dbuf);
	if (ret)
		return ret;

	ret = mtk_cam_pool_alloc(pool, buf, num);
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
#define CAM_CQ_BUF_NUM 16
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
		dev_dbg(dev, "%s:ctx(%d): wait for session destroy\n",
			__func__, ctx->stream_id);
		if (wait_for_completion_timeout(&ctx->session_complete,
						msecs_to_jiffies(1000)) == 0)
			dev_info(dev, "%s:ctx(%d): complete timeout\n",
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

	pr_info("%s: TMP ctx %d pipe_used %x\n",
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

	mtk_cam_update_pipe_used(ctx, &cam->pipelines);

	return ctx;

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
	return NULL;
}

void mtk_cam_stop_ctx(struct mtk_cam_ctx *ctx, struct media_entity *entity)
{
	struct mtk_cam_device *cam = ctx->cam;

	if (!mtk_cam_in_ctx(ctx, entity))
		return;

	dev_info(cam->dev, "%s: by node %s\n", __func__, entity->name);

	mtk_cam_ctx_unprepare_session(ctx);
	mtk_cam_ctx_destroy_pool(ctx);
	mtk_cam_ctx_destroy_workers(ctx);
	mtk_cam_ctx_pipeline_stop(ctx, entity);

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

int mtk_cam_call_seninf_set_pixelmode(struct mtk_cam_ctx *ctx,
				      struct v4l2_subdev *sd,
				      int pad_id, int pixel_mode)
{
	int ret;

	ret = mtk_cam_seninf_set_pixelmode(sd, pad_id, pixel_mode);
	dev_dbg(ctx->cam->dev,
		"%s:ctx(%d): seninf(%s): pad(%d), pixel_mode(%d)\n, ret(%d)",
		__func__, ctx->stream_id, sd->name, pad_id, pixel_mode,
		ret);

	return ret;
}

#ifdef NOT_READY
static struct mtk_cam_request *
fetch_request_from_pending(struct mtk_cam_device *cam, struct mtk_cam_ctx *ctx)
{
	return NULL;
}
#endif

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

__maybe_unused static int ctx_stream_on_seninf_sensor(struct mtk_cam_ctx *ctx, int enable)
{
	struct device *dev = ctx->cam->dev;
	int ret;

	dev_info(dev, "%f: todo\n", __func__);
	//ret = mtk_cam_seninf_set_pixelmode(sd, pad_id, pixel_mode);

	//mtk_cam_call_seninf_set_pixelmode(ctx, ctx->seninf,
	//				  PAD_SRC_RAW0, tgo_pxl_mode);

	ret = v4l2_subdev_call(ctx->seninf, video, s_stream, enable);
	if (ret) {
		dev_info(dev, "failed to stream_on %s %d:%d\n",
			 ctx->seninf->name, enable, ret);
		return -EPERM;
	}
	return ret;
}

int mtk_cam_ctx_stream_on(struct mtk_cam_ctx *ctx)
{
	int ret;

	/* if already stream on */
	if (atomic_cmpxchg(&ctx->streaming, 0, 1))
		return 0;

	ret = ctx_stream_on_pipe_subdev(ctx, 1);

	mtk_cam_dev_try_queue(ctx->cam);

	//ctx_stream_on_seninf_sensor

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

	// seninf
	//ctx_stream_on_seninf_sensor

	ctx_stream_on_pipe_subdev(ctx, 0);

	return 0;
}

static int config_bridge_pad_links(struct mtk_cam_device *cam,
				   struct v4l2_subdev *seninf)
{
#ifdef NOT_READY
	struct media_entity *pipe_entity;
	unsigned int i, j;
	int ret;

	for (i = 0; i < cam->max_stream_num; i++) {
		if (i >= MTKCAM_SUBDEV_RAW_START &&
			i < (MTKCAM_SUBDEV_RAW_START + cam->num_raw_devices)) {
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
		} else if (i >= MTKCAM_SUBDEV_CAMSV_START && i < MTKCAM_SUBDEV_CAMSV_END) {
			pipe_entity = &cam->sv.pipelines[i-MTKCAM_SUBDEV_CAMSV_START].subdev.entity;

			dev_info(cam->dev, "create pad link %s %s\n",
				seninf->entity.name, pipe_entity->name);

			ret = media_create_pad_link(&seninf->entity,
					PAD_SRC_RAW0,
					pipe_entity,
					MTK_CAMSV_SINK,
					MEDIA_LNK_FL_DYNAMIC);

			if (ret) {
				dev_dbg(cam->dev,
					"failed to create pad link %s %s err:%d\n",
					seninf->entity.name, pipe_entity->name,
					ret);
				return ret;
			}

#if PDAF_READY
			for (j = PAD_SRC_PDAF0; j <= PAD_SRC_PDAF5; j++) {
				ret = media_create_pad_link(&seninf->entity,
						j,
						pipe_entity,
						MTK_CAMSV_SINK,
						MEDIA_LNK_FL_DYNAMIC);

				if (ret) {
					dev_dbg(cam->dev,
						"failed to create pad link %s %s err:%d\n",
						seninf->entity.name, pipe_entity->name,
						ret);
					return ret;
				}
			}
#endif
		} else if (i >= MTKCAM_SUBDEV_MRAW_START && i < MTKCAM_SUBDEV_MRAW_END) {
			pipe_entity =
				&cam->mraw.pipelines[i - MTKCAM_SUBDEV_MRAW_START].subdev.entity;

			dev_info(cam->dev, "create pad link %s %s\n",
				seninf->entity.name, pipe_entity->name);

			for (j = PAD_SRC_PDAF0; j <= PAD_SRC_PDAF5; j++) {
				ret = media_create_pad_link(&seninf->entity,
						j,
						pipe_entity,
						MTK_MRAW_SINK,
						MEDIA_LNK_FL_DYNAMIC);

				if (ret) {
					dev_dbg(cam->dev,
						"failed to create pad link %s %s err:%d\n",
						seninf->entity.name, pipe_entity->name,
						ret);
					return ret;
				}
			}
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

	//mtk_cam_dvfs_init(cam_dev);

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
	//mtk_cam_dvfs_uninit(cam_dev);
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
	cam_dev->max_stream_num = MTKCAM_SUBDEV_MAX;
	cam_dev->ctxs = devm_kcalloc(dev, cam_dev->max_stream_num,
				     sizeof(*cam_dev->ctxs), GFP_KERNEL);
	if (!cam_dev->ctxs)
		return -ENOMEM;

	for (i = 0; i < cam_dev->max_stream_num; i++)
		mtk_cam_ctx_init(cam_dev->ctxs + i, cam_dev, i);

	spin_lock_init(&cam_dev->pending_job_lock);
	spin_lock_init(&cam_dev->running_job_lock);
	INIT_LIST_HEAD(&cam_dev->pending_job_list);
	INIT_LIST_HEAD(&cam_dev->running_job_list);

	mutex_init(&cam_dev->queue_lock);

	pm_runtime_enable(dev);

	ret = mtk_cam_of_rproc(cam_dev);
	if (ret)
		goto fail_destroy_mutex;

	ret = register_sub_drivers(dev);
	if (ret) {
		dev_info(dev, "fail to register_sub_drivers\n");
		goto fail_destroy_mutex;
	}

	ret = mtk_cam_debug_fs_init(cam_dev);
	if (ret < 0)
		goto fail_match_remove;

	return 0;

fail_match_remove:
	mtk_cam_match_remove(dev);

fail_destroy_mutex:
	mutex_destroy(&cam_dev->queue_lock);

	return ret;
}

static int mtk_cam_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);

	pm_runtime_disable(dev);

	component_master_del(dev, &mtk_cam_master_ops);
	mtk_cam_match_remove(dev);

	mutex_destroy(&cam_dev->queue_lock);
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

#if IS_ENABLED(CONFIG_MTK_CAMSYS_VEND_HOOK)

static void media_device_setup_link_hook(void *data, struct media_link *link,
					 struct media_link_desc *linkd, int *ret)
{
	int ret_value;

	pr_debug("%s req_fd:%d\n", __func__, linkd->reserved[0]);
	link->android_vendor_data1 = linkd->reserved[0];
	ret_value = __media_entity_setup_link(link, linkd->flags);
	link->android_vendor_data1 = 0;
	*ret = (ret_value < 0) ? ret_value : 1;
	pr_debug("%s ret:%d\n", __func__, *ret);
}

static void clear_reserved_fmt_fields_hook(void *data, struct v4l2_format *p,
					   int *ret)
{
	*ret = 1;
}

static void fill_ext_fmtdesc_hook(void *data, struct v4l2_fmtdesc *p, const char **descr)
{
	switch (p->pixelformat) {
	case V4L2_PIX_FMT_YUYV10:
		*descr = "YUYV 4:2:2 10 bits";
		break;
	case V4L2_PIX_FMT_YVYU10:
		*descr = "YVYU 4:2:2 10 bits";
		break;
	case V4L2_PIX_FMT_UYVY10:
		*descr = "UYVY 4:2:2 10 bits";
		break;
	case V4L2_PIX_FMT_VYUY10:
		*descr = "VYUY 4:2:2 10 bits";
		break;
	case V4L2_PIX_FMT_NV12_10:
		*descr = "Y/CbCr 4:2:0 10 bits";
		break;
	case V4L2_PIX_FMT_NV21_10:
		*descr = "Y/CrCb 4:2:0 10 bits";
		break;
	case V4L2_PIX_FMT_NV16_10:
		*descr = "Y/CbCr 4:2:2 10 bits";
		break;
	case V4L2_PIX_FMT_NV61_10:
		*descr = "Y/CrCb 4:2:2 10 bits";
		break;
	case V4L2_PIX_FMT_NV12_12:
		*descr = "Y/CbCr 4:2:0 12 bits";
		break;
	case V4L2_PIX_FMT_NV21_12:
		*descr = "Y/CrCb 4:2:0 12 bits";
		break;
	case V4L2_PIX_FMT_NV16_12:
		*descr = "Y/CbCr 4:2:2 12 bits";
		break;
	case V4L2_PIX_FMT_NV61_12:
		*descr = "Y/CrCb 4:2:2 12 bits";
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR10:
		*descr = "10-bit Bayer BGGR MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGBRG10:
		*descr = "10-bit Bayer GBRG MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGRBG10:
		*descr = "10-bit Bayer GRBG MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SRGGB10:
		*descr = "10-bit Bayer RGGB MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR12:
		*descr = "12-bit Bayer BGGR MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGBRG12:
		*descr = "12-bit Bayer GBRG MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGRBG12:
		*descr = "12-bit Bayer GRBG MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SRGGB12:
		*descr = "12-bit Bayer RGGB MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR14:
		*descr = "14-bit Bayer BGGR MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGBRG14:
		*descr = "14-bit Bayer GBRG MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGRBG14:
		*descr = "14-bit Bayer GRBG MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SRGGB14:
		*descr = "14-bit Bayer RGGB MTISP Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR8F:
		*descr = "8-bit Enhanced BGGR Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGBRG8F:
		*descr = "8-bit Enhanced GBRG Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGRBG8F:
		*descr = "8-bit Enhanced GRBG Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SRGGB8F:
		*descr = "8-bit Enhanced RGGB Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR10F:
		*descr = "10-bit Enhanced BGGR Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGBRG10F:
		*descr = "10-bit Enhanced GBRG Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGRBG10F:
		*descr = "10-bit Enhanced GRBG Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SRGGB10F:
		*descr = "10-bit Enhanced RGGB Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR12F:
		*descr = "12-bit Enhanced BGGR Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGBRG12F:
		*descr = "12-bit Enhanced GBRG Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGRBG12F:
		*descr = "12-bit Enhanced GRBG Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SRGGB12F:
		*descr = "12-bit Enhanced RGGB Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR14F:
		*descr = "14-bit Enhanced BGGR Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGBRG14F:
		*descr = "14-bit Enhanced GBRG Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGRBG14F:
		*descr = "14-bit Enhanced GRBG Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SRGGB14F:
		*descr = "14-bit Enhanced RGGB Packed";
		break;
	case V4L2_PIX_FMT_MTISP_NV12_10P:
		*descr = "Y/CbCr 4:2:0 10 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_NV21_10P:
		*descr = "Y/CrCb 4:2:0 10 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_NV16_10P:
		*descr = "Y/CbCr 4:2:2 10 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_NV61_10P:
		*descr = "Y/CrCb 4:2:2 10 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_YUYV10P:
		*descr = "YUYV 4:2:2 10 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_YVYU10P:
		*descr = "YVYU 4:2:2 10 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_UYVY10P:
		*descr = "UYVY 4:2:2 10 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_VYUY10P:
		*descr = "VYUY 4:2:2 10 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_NV12_12P:
		*descr = "Y/CbCr 4:2:0 12 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_NV21_12P:
		*descr = "Y/CrCb 4:2:0 12 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_NV16_12P:
		*descr = "Y/CbCr 4:2:2 12 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_NV61_12P:
		*descr = "Y/CrCb 4:2:2 12 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_YUYV12P:
		*descr = "YUYV 4:2:2 12 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_YVYU12P:
		*descr = "YVYU 4:2:2 12 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_UYVY12P:
		*descr = "UYVY 4:2:2 12 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_VYUY12P:
		*descr = "VYUY 4:2:2 12 bits packed";
		break;
	case V4L2_PIX_FMT_MTISP_NV12_UFBC:
		*descr = "YCbCr 420 8 bits compress";
		break;
	case V4L2_PIX_FMT_MTISP_NV21_UFBC:
		*descr = "YCrCb 420 8 bits compress";
		break;
	case V4L2_PIX_FMT_MTISP_NV12_10_UFBC:
		*descr = "YCbCr 420 10 bits compress";
		break;
	case V4L2_PIX_FMT_MTISP_NV21_10_UFBC:
		*descr = "YCrCb 420 10 bits compress";
		break;
	case V4L2_PIX_FMT_MTISP_NV12_12_UFBC:
		*descr = "YCbCr 420 12 bits compress";
		break;
	case V4L2_PIX_FMT_MTISP_NV21_12_UFBC:
		*descr = "YCrCb 420 12 bits compress";
		break;
	case V4L2_PIX_FMT_MTISP_BAYER8_UFBC:
		*descr = "RAW 8 bits compress";
		break;
	case V4L2_PIX_FMT_MTISP_BAYER10_UFBC:
		*descr = "RAW 10 bits compress";
		break;
	case V4L2_PIX_FMT_MTISP_BAYER12_UFBC:
		*descr = "RAW 12 bits compress";
		break;
	case V4L2_PIX_FMT_MTISP_BAYER14_UFBC:
		*descr = "RAW 14 bits compress";
		break;
	case V4L2_META_FMT_MTISP_3A:
		*descr = "AE/AWB Histogram";
		break;
	case V4L2_META_FMT_MTISP_AF:
		*descr = "AF Histogram";
		break;
	case V4L2_META_FMT_MTISP_LCS:
		*descr = "Local Contrast Enhancement Stat";
		break;
	case V4L2_META_FMT_MTISP_LMV:
		*descr = "Local Motion Vector Histogram";
		break;
	case V4L2_META_FMT_MTISP_PARAMS:
		*descr = "MTK ISP Tuning Metadata";
		break;
	case V4L2_PIX_FMT_MTISP_SGRB8F:
		*descr = "8-bit 3 plane GRB Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGRB10F:
		*descr = "10-bit 3 plane GRB Packed";
		break;
	case V4L2_PIX_FMT_MTISP_SGRB12F:
		*descr = "12-bit 3 plane GRB Packed";
		break;
	default:
		break;
	}
}

static void clear_mask_adjust_hook(void *data, unsigned int ctrl, int *n)
{
	if (ctrl == VIDIOC_S_SELECTION)
		*n = offsetof(struct v4l2_selection, reserved[1]);
}

static void v4l2subdev_set_fmt_hook(void *data,
	struct v4l2_subdev *sd, struct v4l2_subdev_state *state,
	struct v4l2_subdev_format *format, int *ret)
{
	int retval = 0;
	struct media_entity entity = sd->entity;

	pr_debug("%s entity_name:%s req_fd:%d\n", __func__, entity.name,
		format->reserved[0]);

	retval = v4l2_subdev_call(sd, pad, set_fmt, state, format);
	*ret = (retval < 0) ? retval : 1;
	pr_debug("%s *ret:%d\n", __func__, *ret);
}

static void v4l2subdev_set_selection_hook(void *data,
	struct v4l2_subdev *sd, struct v4l2_subdev_state *state,
	struct v4l2_subdev_selection *sel, int *ret)
{
	int retval = 0;

	retval = v4l2_subdev_call(sd, pad, set_selection, state, sel);
	*ret = (retval < 0) ? retval : 1;
}

static void v4l2subdev_set_frame_interval_hook(void *data,
	struct v4l2_subdev *sd, struct v4l2_subdev_frame_interval *fi,
	int *ret)
{
	int retval = 0;

	retval = v4l2_subdev_call(sd, video, s_frame_interval, fi);
	*ret = (retval < 0) ? retval : 1;
}

static void mtk_cam_trace_init(void)
{
	int ret = 0;

	ret = register_trace_android_rvh_media_device_setup_link(
			media_device_setup_link_hook, NULL);
	if (ret)
		pr_info("register android_rvh_media_device_setup_link failed!\n");
	ret = register_trace_android_vh_clear_reserved_fmt_fields(
			clear_reserved_fmt_fields_hook, NULL);
	if (ret)
		pr_info("register android_vh_clear_reserved_fmt_fields failed!\n");
	ret = register_trace_android_vh_fill_ext_fmtdesc(
			fill_ext_fmtdesc_hook, NULL);
	if (ret)
		pr_info("register android_vh_v4l_fill_fmtdesc failed!\n");
	ret = register_trace_android_vh_clear_mask_adjust(
			clear_mask_adjust_hook, NULL);
	if (ret)
		pr_info("register android_vh_clear_mask_adjust failed!\n");
	ret = register_trace_android_rvh_v4l2subdev_set_fmt(
			v4l2subdev_set_fmt_hook, NULL);
	if (ret)
		pr_info("register android_rvh_v4l2subdev_set_fmt failed!\n");
	ret = register_trace_android_rvh_v4l2subdev_set_selection(
			v4l2subdev_set_selection_hook, NULL);
	if (ret)
		pr_info("register android_rvh_v4l2subdev_set_selection failed!\n");
	ret = register_trace_android_rvh_v4l2subdev_set_frame_interval(
			v4l2subdev_set_frame_interval_hook, NULL);
	if (ret)
		pr_info("register android_rvh_v4l2subdev_set_frame_interval failed!\n");
}

static void mtk_cam_trace_exit(void)
{
	unregister_trace_android_vh_clear_reserved_fmt_fields(clear_reserved_fmt_fields_hook, NULL);
	unregister_trace_android_vh_fill_ext_fmtdesc(fill_ext_fmtdesc_hook, NULL);
	unregister_trace_android_vh_clear_mask_adjust(clear_mask_adjust_hook, NULL);
}

#endif

static int __init mtk_cam_init(void)
{
	int ret;
#if IS_ENABLED(CONFIG_MTK_CAMSYS_VEND_HOOK)
	mtk_cam_trace_init();
#endif
	ret = platform_driver_register(&mtk_cam_driver);
	return ret;
}

static void __exit mtk_cam_exit(void)
{
	platform_driver_unregister(&mtk_cam_driver);
#if IS_ENABLED(CONFIG_MTK_CAMSYS_VEND_HOOK)
	mtk_cam_trace_exit();
#endif
}

module_init(mtk_cam_init);
module_exit(mtk_cam_exit);

MODULE_DESCRIPTION("Camera ISP driver");
MODULE_LICENSE("GPL");
