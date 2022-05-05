// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * Author: Fish Wu <fish.wu@mediatek.com>
 *
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include <linux/device.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-v4l2.h>
#include <linux/suspend.h>
#include <linux/rtc.h>
#include <soc/mediatek/smi.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include "cmdq-sec.h"
#include "mtk_aie.h"
#include "mtk_imgsys-cmdq.h"

#define FLD
#define AIE_QOS_MAX 5
#define AIE_QOS_RA_IDX 0
#define AIE_QOS_RB_IDX 1
#define AIE_QOS_WA_IDX 2
#define AIE_QOS_WB_IDX 3
#define AIE_QOS_LARB12 4
#define AIE_READ0_AVG_BW 511
#define AIE_READ1_AVG_BW 255
#define AIE_WRITE2_AVG_BW 255
#define AIE_WRITE3_AVG_BW 127
#define AIE_LARB12_AVG_BW 568
#define CHECK_SERVICE_0 0
#define CHECK_SERVICE_1 1
#define CLK_SINGLE 1
#if CHECK_SERVICE_0 //Remove CID
#define V4L2_CID_MTK_AIE_INIT (V4L2_CID_USER_MTK_FD_BASE + 1)
#define V4L2_CID_MTK_AIE_PARAM (V4L2_CID_USER_MTK_FD_BASE + 2)
#define V4L2_CID_MTK_AIE_FD_VER (V4L2_CID_USER_MTK_FD_BASE + 3)
#define V4L2_CID_MTK_AIE_ATTR_VER (V4L2_CID_USER_MTK_FD_BASE + 4)

#define V4L2_CID_MTK_AIE_MAX 4
#endif

struct mtk_aie_user_para g_user_param;
static struct device *aie_pm_dev;
static const struct v4l2_pix_format_mplane mtk_aie_img_fmts[] = {
	{
		.pixelformat = V4L2_PIX_FMT_NV16M, .num_planes = 2,
	},
	{
		.pixelformat = V4L2_PIX_FMT_NV61M, .num_planes = 2,
	},
	{
		.pixelformat = V4L2_PIX_FMT_YUYV, .num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_YVYU, .num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_UYVY, .num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_VYUY, .num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_GREY, .num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_NV12M, .num_planes = 2,
	},
	{
		.pixelformat = V4L2_PIX_FMT_NV12, .num_planes = 1,
	},
};

struct clk_bulk_data ipesys_isp7_aie_clks[] = {
	{ .id = "VCORE_GALS" },
	{ .id = "MAIN_GALS" },
	{ .id = "IMG_IPE" },
	{ .id = "IPE_FDVT" },
	/*{ .id = "IPE_FDVT1" },*/
	{ .id = "IPE_TOP" },
	{ .id = "IPE_SMI_LARB12" },
};
#if CHECK_SERVICE_0
static struct mtk_aie_qos_path aie_qos_path[AIE_QOS_MAX] = {
	{NULL, "l12_fdvt_rda", 0},
	{NULL, "l12_fdvt_rdb", 0},
	{NULL, "l12_fdvt_wra", 0},
	{NULL, "l12_fdvt_wrb", 0},
	{NULL, "l12_subcommon_1", 0}
};
#endif
static int mtk_aie_suspend(struct device *dev)
{
	struct mtk_aie_dev *fd = dev_get_drvdata(dev);
	int ret, num;

	if (pm_runtime_suspended(dev))
		return 0;

	num = atomic_read(&fd->num_composing);
	dev_dbg(dev, "%s: suspend aie job start, num(%d)\n", __func__, num);

	ret = wait_event_timeout
		(fd->flushing_waitq,
		 !(num = atomic_read(&fd->num_composing)),
		 msecs_to_jiffies(MTK_FD_HW_TIMEOUT));
	if (!ret && num) {
		dev_info(dev, "%s: flushing aie job timeout, num(%d)\n",
			__func__, num);

		return -EBUSY;
	}

	dev_dbg(dev, "%s: suspend aie job end, num(%d)\n", __func__, num);

	ret = pm_runtime_put_sync(dev);
	if (ret) {
		dev_info(dev, "%s: pm_runtime_put_sync failed:(%d)\n",
			__func__, ret);
		return ret;
	}

	return 0;
}

static int mtk_aie_resume(struct device *dev)
{
	int ret;

	dev_dbg(dev, "%s: resume aie job start)\n", __func__);

	if (pm_runtime_suspended(dev)) {
		dev_dbg(dev, "%s: pm_runtime_suspended is true, no action\n",
			__func__);
		return 0;
	}

	ret = pm_runtime_get_sync(dev);
	if (ret) {
		dev_info(dev, "%s: pm_runtime_get_sync failed:(%d)\n",
			__func__, ret);
		return ret;
	}

	dev_dbg(dev, "%s: resume aie job end)\n", __func__);
	return 0;
}

#if IS_ENABLED(CONFIG_PM)
static int aie_pm_event(struct notifier_block *notifier,
			unsigned long pm_event, void *unused)
{
	struct timespec64 ts;
	struct rtc_time tm;

	ktime_get_ts64(&ts);
	rtc_time64_to_tm(ts.tv_sec, &tm);

	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:
		return NOTIFY_DONE;
	case PM_RESTORE_PREPARE:
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION:
		return NOTIFY_DONE;
	case PM_SUSPEND_PREPARE: /*enter suspend*/
		mtk_aie_suspend(aie_pm_dev);
		return NOTIFY_DONE;
	case PM_POST_SUSPEND:    /*after resume*/
		mtk_aie_resume(aie_pm_dev);
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static struct notifier_block aie_notifier_block = {
	.notifier_call = aie_pm_event,
	.priority = 0,
};
#endif

#define NUM_FORMATS ARRAY_SIZE(mtk_aie_img_fmts)

static inline struct mtk_aie_ctx *fh_to_ctx(struct v4l2_fh *fh)
{
	return container_of(fh, struct mtk_aie_ctx, fh);
}
#if CHECK_SERVICE_0
static void mtk_aie_mmdvfs_init(struct mtk_aie_dev *fd)
{
	struct mtk_aie_dvfs *dvfs_info = &fd->dvfs_info;
	u64 freq = 0;
	int ret = 0, opp_num = 0, opp_idx = 0, idx = 0, volt;
	struct device_node *np, *child_np = NULL;
	struct of_phandle_iterator it;

	memset((void *)dvfs_info, 0x0, sizeof(struct mtk_aie_dvfs));
	dvfs_info->dev = fd->dev;
	ret = dev_pm_opp_of_add_table(dvfs_info->dev);
	if (ret < 0) {
		dev_info(dvfs_info->dev, "fail to init opp table: %d\n", ret);
		return;
	}
	dvfs_info->reg = devm_regulator_get(dvfs_info->dev, "dvfsrc-vcore");
	if (IS_ERR(dvfs_info->reg)) {
		dev_info(dvfs_info->dev, "can't get dvfsrc-vcore\n");
		return;
	}

	opp_num = regulator_count_voltages(dvfs_info->reg);
	of_for_each_phandle(
		&it, ret, dvfs_info->dev->of_node, "operating-points-v2", NULL, 0) {
		np = of_node_get(it.node);
		if (!np) {
			dev_info(dvfs_info->dev, "of_node_get fail\n");
			return;
		}

		do {
			child_np = of_get_next_available_child(np, child_np);
			if (child_np) {
				of_property_read_u64(child_np, "opp-hz", &freq);
				dvfs_info->clklv[opp_idx][idx] = freq;
				of_property_read_u32(child_np, "opp-microvolt", &volt);
				dvfs_info->voltlv[opp_idx][idx] = volt;
				idx++;
			}
		} while (child_np);
		dvfs_info->clklv_num[opp_idx] = idx;
		dvfs_info->clklv_target[opp_idx] = dvfs_info->clklv[opp_idx][0];
		dvfs_info->clklv_idx[opp_idx] = 0;
		idx = 0;
		opp_idx++;
		of_node_put(np);
	}

	opp_num = opp_idx;
	for (opp_idx = 0; opp_idx < opp_num; opp_idx++) {
		for (idx = 0; idx < dvfs_info->clklv_num[opp_idx]; idx++) {
			dev_dbg(dvfs_info->dev, "[%s] opp=%d, idx=%d, clk=%d volt=%d\n",
				__func__, opp_idx, idx, dvfs_info->clklv[opp_idx][idx],
				dvfs_info->voltlv[opp_idx][idx]);
		}
	}
	dvfs_info->cur_volt = 0;

}

static void mtk_aie_mmdvfs_uninit(struct mtk_aie_dev *fd)
{
	struct mtk_aie_dvfs *dvfs_info = &fd->dvfs_info;
	int volt = 0, ret = 0;

	dev_dbg(dvfs_info->dev, "[%s]\n", __func__);

	dvfs_info->cur_volt = volt;

	ret = regulator_set_voltage(dvfs_info->reg, volt, INT_MAX);

}

static void mtk_aie_mmdvfs_set(struct mtk_aie_dev *fd,
				bool isSet, unsigned int level)
{
	struct mtk_aie_dvfs *dvfs_info = &fd->dvfs_info;
	int volt = 0, ret = 0, idx = 0, opp_idx = 0;

	if (isSet) {
		if (level < dvfs_info->clklv_num[opp_idx])
			idx = level;
	}
	volt = dvfs_info->voltlv[opp_idx][idx];

	if (dvfs_info->cur_volt != volt) {
		dev_dbg(dvfs_info->dev, "[%s] volt change opp=%d, idx=%d, clk=%d volt=%d\n",
			__func__, opp_idx, idx, dvfs_info->clklv[opp_idx][idx],
			dvfs_info->voltlv[opp_idx][idx]);
		ret = regulator_set_voltage(dvfs_info->reg, volt, INT_MAX);
		dvfs_info->cur_volt = volt;
	}
}
#endif
#if CHECK_SERVICE_0
static void mtk_aie_mmqos_init(struct mtk_aie_dev *fd)
{
	struct mtk_aie_qos *qos_info = &fd->qos_info;
	//struct icc_path *path;
	int idx = 0;

	memset((void *)qos_info, 0x0, sizeof(struct mtk_aie_qos));
	qos_info->dev = fd->dev;
	qos_info->qos_path = aie_qos_path;

	for (idx = 0; idx < AIE_QOS_MAX; idx++) {

		qos_info->qos_path[idx].path =
			of_mtk_icc_get(qos_info->dev, qos_info->qos_path[idx].dts_name);

		qos_info->qos_path[idx].bw = 0;
		dev_dbg(qos_info->dev, "[%s] idx=%d, path=%p, name=%s, bw=%d\n",
			__func__, idx,
			qos_info->qos_path[idx].path,
			qos_info->qos_path[idx].dts_name,
			qos_info->qos_path[idx].bw);
	}
}

static void mtk_aie_mmqos_uninit(struct mtk_aie_dev *fd)
{
	struct mtk_aie_qos *qos_info = &fd->qos_info;
	int idx = 0;

	for (idx = 0; idx < AIE_QOS_MAX; idx++) {
		if (qos_info->qos_path[idx].path == NULL) {
			dev_dbg(qos_info->dev, "[%s] path of idx(%d) is NULL\n", __func__, idx);
			continue;
		}
		dev_dbg(qos_info->dev, "[%s] idx=%d, path=%p, bw=%d\n",
			__func__, idx,
			qos_info->qos_path[idx].path,
			qos_info->qos_path[idx].bw);
		qos_info->qos_path[idx].bw = 0;

		mtk_icc_set_bw(qos_info->qos_path[idx].path, 0, 0);
	}
}

static void mtk_aie_mmqos_set(struct mtk_aie_dev *fd,
				bool isSet)
{
	struct mtk_aie_qos *qos_info = &fd->qos_info;
#if CHECK_SERVICE_0
	int r0_bw = 0, r1_bw = 0;
	int w2_bw = 0, w3_bw = 0;
#endif
	int idx = 0, larb12_bw = 0;

	if (isSet) {
#if CHECK_SERVICE_0
		r0_bw = AIE_READ0_AVG_BW;
		r1_bw = AIE_READ1_AVG_BW;
		w2_bw = AIE_WRITE2_AVG_BW;
		w3_bw = AIE_WRITE3_AVG_BW;
#endif
		larb12_bw = AIE_LARB12_AVG_BW;
	}

	for (idx = 0; idx < AIE_QOS_MAX; idx++) {
		if (qos_info->qos_path[idx].path == NULL) {
			dev_info(qos_info->dev, "[%s] path of idx(%d) is NULL\n",
				 __func__, idx);
			continue;
		}
#if CHECK_SERVICE_0
		if (idx == AIE_QOS_RA_IDX &&
		    qos_info->qos_path[idx].bw != r0_bw) {
			dev_info(qos_info->dev, "[%s] idx=%d, path=%p, bw=%d/%d,\n",
				__func__, idx,
				qos_info->qos_path[idx].path,
				qos_info->qos_path[idx].bw, r0_bw);
			qos_info->qos_path[idx].bw = r0_bw;

			mtk_icc_set_bw(qos_info->qos_path[idx].path,
					MBps_to_icc(qos_info->qos_path[idx].bw), 0);
		}

		if (idx == AIE_QOS_RB_IDX &&
		    qos_info->qos_path[idx].bw != r1_bw) {
			dev_info(qos_info->dev, "[%s] idx=%d, path=%p, bw=%d/%d,\n",
				__func__, idx,
				qos_info->qos_path[idx].path,
				qos_info->qos_path[idx].bw, r1_bw);
			qos_info->qos_path[idx].bw = r1_bw;

			mtk_icc_set_bw(qos_info->qos_path[idx].path,
					MBps_to_icc(qos_info->qos_path[idx].bw), 0);
		}

		if (idx == AIE_QOS_WA_IDX &&
		    qos_info->qos_path[idx].bw != w2_bw) {
			dev_info(qos_info->dev, "[%s] idx=%d, path=%p, bw=%d/%d,\n",
				__func__, idx,
				qos_info->qos_path[idx].path,
				qos_info->qos_path[idx].bw, w2_bw);
			qos_info->qos_path[idx].bw = w2_bw;

			mtk_icc_set_bw(qos_info->qos_path[idx].path,
					MBps_to_icc(qos_info->qos_path[idx].bw), 0);
		}

		if (idx == AIE_QOS_WB_IDX &&
		    qos_info->qos_path[idx].bw != w3_bw) {
			dev_info(qos_info->dev, "[%s] idx=%d, path=%p, bw=%d/%d,\n",
				__func__, idx,
				qos_info->qos_path[idx].path,
				qos_info->qos_path[idx].bw, w3_bw);
			qos_info->qos_path[idx].bw = w3_bw;

			mtk_icc_set_bw(qos_info->qos_path[idx].path,
					MBps_to_icc(qos_info->qos_path[idx].bw), 0);
		}
#endif
		if (idx == AIE_QOS_LARB12 &&
		    qos_info->qos_path[idx].bw != larb12_bw) {
			dev_info(qos_info->dev, "[%s] idx=%d, path=%p, bw=%d/%d,\n",
				__func__, idx,
				qos_info->qos_path[idx].path,
				qos_info->qos_path[idx].bw, larb12_bw);
			qos_info->qos_path[idx].bw = larb12_bw;

			mtk_icc_set_bw(qos_info->qos_path[idx].path,
					MBps_to_icc(qos_info->qos_path[idx].bw), 0);
		}
	}
}
#endif
#if CHECK_SERVICE_0
static void mtk_aie_fill_init_param(struct mtk_aie_dev *fd,
				    struct user_init *user_init,
				    struct v4l2_ctrl_handler *hdl)
{
	struct v4l2_ctrl *ctrl;

	ctrl = v4l2_ctrl_find(hdl, V4L2_CID_MTK_AIE_INIT);
	if (ctrl) {
		memcpy(user_init, ctrl->p_new.p_u32, sizeof(struct user_init));
		dev_dbg(fd->dev, "init param : max w:%d, max h:%d",
			user_init->max_img_width, user_init->max_img_height);
		dev_dbg(fd->dev, "init param : p_w:%d, p_h:%d, f thread:%d",
			user_init->pyramid_width, user_init->pyramid_height,
			user_init->feature_thread);
	}
}
#endif
static int mtk_aie_hw_enable(struct mtk_aie_dev *fd)
{
	return aie_init(fd);
}

static void mtk_aie_hw_job_finish(struct mtk_aie_dev *fd,
				  enum vb2_buffer_state vb_state)
{
	struct mtk_aie_ctx *ctx;
	struct vb2_v4l2_buffer *src_vbuf = NULL, *dst_vbuf = NULL;

	ctx = v4l2_m2m_get_curr_priv(fd->m2m_dev);
	src_vbuf = v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	dst_vbuf = v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);

	v4l2_m2m_buf_copy_metadata(src_vbuf, dst_vbuf,
				   V4L2_BUF_FLAG_TSTAMP_SRC_MASK);
	v4l2_m2m_buf_done(src_vbuf, vb_state);
	v4l2_m2m_buf_done(dst_vbuf, vb_state);
	v4l2_m2m_job_finish(fd->m2m_dev, ctx->fh.m2m_ctx);
	complete_all(&fd->fd_job_finished);
}

static void mtk_aie_hw_done(struct mtk_aie_dev *fd,
			    enum vb2_buffer_state vb_state)
{
	if (!cancel_delayed_work(&fd->job_timeout_work))
		return;

	mtk_aie_hw_job_finish(fd, vb_state);
	atomic_dec(&fd->num_composing);
	wake_up(&fd->flushing_waitq);
}

static int mtk_aie_ccf_enable(struct device *dev)
{
	struct mtk_aie_dev *fd = dev_get_drvdata(dev);
	int ret;

#ifdef CLK_SINGLE
	ret = clk_prepare_enable(fd->vcore_gals);
	if (ret) {
		dev_info(dev, "Failed to open vcore_gals clk:%d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(fd->main_gals);
	if (ret) {
		dev_info(dev, "Failed to open main_gals clk:%d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(fd->img_ipe);
	if (ret) {
		dev_info(dev, "Failed to open img_ipe clk:%d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(fd->ipe_fdvt);
	if (ret) {
		dev_info(dev, "Failed to open ipe_fdvt clk:%d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(fd->ipe_smi_larb12);
	if (ret) {
		dev_info(dev, "Failed to open ipe_smi_larb12 clk:%d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(fd->ipe_top);
	if (ret) {
		dev_info(dev, "Failed to open ipe_top clk:%d\n", ret);
		return ret;
	}

#else

	ret = clk_bulk_prepare_enable(fd->aie_clk.clk_num, fd->aie_clk.clks);
	if (ret) {
		dev_info("failed to enable AIE clock:%d\n", ret);
		return ret;
	}
#endif
	return 0;


}

static int mtk_aie_ccf_disable(struct device *dev)
{
	struct mtk_aie_dev *fd = dev_get_drvdata(dev);

	clk_disable_unprepare(fd->ipe_top);
	clk_disable_unprepare(fd->ipe_smi_larb12);
	clk_disable_unprepare(fd->ipe_fdvt);
	clk_disable_unprepare(fd->img_ipe);
	clk_disable_unprepare(fd->main_gals);
	clk_disable_unprepare(fd->vcore_gals);
	return 0;
}

static int mtk_aie_hw_connect(struct mtk_aie_dev *fd)
{
	int ret = 0;

	pm_runtime_get_sync((fd->dev));
	//mtk_aie_ccf_enable((fd->dev));
	//mtk_imgsys_pwr(fd->img_pdev, true);

	fd->fd_stream_count++;
	if (fd->fd_stream_count == 1) {
		cmdq_mbox_enable(fd->fdvt_clt->chan);
		ret = mtk_aie_hw_enable(fd);
		if (ret)
			return -EINVAL;
		//mtk_aie_mmdvfs_set(fd, 1, 0);
		//mtk_aie_mmqos_set(fd, 1);

		fd->map_count = 0;
	}

	return 0;
}

static void mtk_aie_hw_disconnect(struct mtk_aie_dev *fd)
{
	if (g_user_param.is_secure == 1 & fd->fd_stream_count == 1) {
		aie_disable_secure_domain(fd);
		cmdq_sec_mbox_stop(fd->fdvt_secure_clt);
	}
	//mtk_aie_ccf_disable(fd->dev);
	pm_runtime_put_sync(fd->dev);
	//mtk_imgsys_pwr(fd->img_pdev, false);
	dev_info(fd->dev, "[%s] stream_count:%d map_count%d\n", __func__,
			fd->fd_stream_count, fd->map_count);
	fd->fd_stream_count--;
	if (fd->fd_stream_count == 0) { //have hw_connect
		//mtk_aie_mmqos_set(fd, 0);
		cmdq_mbox_disable(fd->fdvt_clt->chan);
		//mtk_aie_mmdvfs_set(fd, 0, 0);
		if (fd->map_count == 1) { //have qbuf + map memory
			dma_buf_vunmap(fd->dmabuf, (void *)fd->kva);
			dma_buf_end_cpu_access(fd->dmabuf, DMA_BIDIRECTIONAL);
			dma_buf_put(fd->dmabuf);
			fd->map_count--;
			dev_info(fd->dev, "[%s] stream_count:%d map_count%d\n", __func__,
					fd->fd_stream_count, fd->map_count);
		}
		aie_uninit(fd);
	}
}

static int mtk_aie_hw_job_exec(struct mtk_aie_dev *fd,
			       struct fd_enq_param *fd_param)
{
	reinit_completion(&fd->fd_job_finished);
	schedule_delayed_work(&fd->job_timeout_work,
				msecs_to_jiffies(MTK_FD_HW_TIMEOUT));

	return 0;
}

static int mtk_aie_vb2_buf_out_validate(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *v4l2_buf = to_vb2_v4l2_buffer(vb);

	if (v4l2_buf->field == V4L2_FIELD_ANY)
		v4l2_buf->field = V4L2_FIELD_NONE;
	if (v4l2_buf->field != V4L2_FIELD_NONE)
		return -EINVAL;

	return 0;
}

static int mtk_aie_vb2_buf_prepare(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct vb2_queue *vq = vb->vb2_queue;
	struct mtk_aie_ctx *ctx = vb2_get_drv_priv(vq);
	struct device *dev = ctx->dev;
	struct v4l2_pix_format_mplane *pixfmt;

	switch (vq->type) {
	case V4L2_BUF_TYPE_META_CAPTURE:
		if (vb2_plane_size(vb, 0) < ctx->dst_fmt.buffersize) {
			dev_info(dev, "meta size %lu is too small\n",
				 vb2_plane_size(vb, 0));
			return -EINVAL;
		}
		break;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		pixfmt = &ctx->src_fmt;

		if (vbuf->field == V4L2_FIELD_ANY)
			vbuf->field = V4L2_FIELD_NONE;

		if (vb->num_planes > 2 || vbuf->field != V4L2_FIELD_NONE) {
			dev_info(dev, "plane %d or field %d not supported\n",
				 vb->num_planes, vbuf->field);
			return -EINVAL;
		}

		if (vb2_plane_size(vb, 0) < pixfmt->plane_fmt[0].sizeimage) {
			dev_info(dev, "plane 0 %lu is too small than %x\n",
				 vb2_plane_size(vb, 0),
				 pixfmt->plane_fmt[0].sizeimage);
			return -EINVAL;
		}

		if (pixfmt->num_planes == 2 &&
		    vb2_plane_size(vb, 1) < pixfmt->plane_fmt[1].sizeimage) {
			dev_info(dev, "plane 1 %lu is too small than %x\n",
				 vb2_plane_size(vb, 1),
				 pixfmt->plane_fmt[1].sizeimage);
			return -EINVAL;
		}
		break;
	}

	return 0;
}

static void mtk_aie_vb2_buf_queue(struct vb2_buffer *vb)
{
	struct mtk_aie_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);

	v4l2_m2m_buf_queue(ctx->fh.m2m_ctx, vbuf);
}

static int mtk_aie_vb2_queue_setup(struct vb2_queue *vq,
				   unsigned int *num_buffers,
				   unsigned int *num_planes,
				   unsigned int sizes[],
				   struct device *alloc_devs[])
{
	struct mtk_aie_ctx *ctx = vb2_get_drv_priv(vq);
	unsigned int size[2];
	unsigned int plane;

	switch (vq->type) {
	case V4L2_BUF_TYPE_META_CAPTURE:
		size[0] = ctx->dst_fmt.buffersize;
		break;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		size[0] = ctx->src_fmt.plane_fmt[0].sizeimage;
		size[1] = ctx->src_fmt.plane_fmt[1].sizeimage;
		break;
	}

	if (*num_planes > 2)
		return -EINVAL;
	if (*num_planes == 0) {
		if (vq->type == V4L2_BUF_TYPE_META_CAPTURE) {
			sizes[0] = ctx->dst_fmt.buffersize;
			*num_planes = 1;
			return 0;
		}

		*num_planes = ctx->src_fmt.num_planes;
		if (*num_planes > 2)
			return -EINVAL;
		for (plane = 0; plane < *num_planes; plane++)
			sizes[plane] = ctx->src_fmt.plane_fmt[plane].sizeimage;

		return 0;
	}

	return 0;
}

static int mtk_aie_vb2_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct mtk_aie_ctx *ctx = vb2_get_drv_priv(vq);

	if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		return mtk_aie_hw_connect(ctx->fd_dev);

	return 0;
}

void FDVT_DumpDRAMOut(struct mtk_aie_dev *fd, unsigned int *hw, unsigned int size)
{
	unsigned int i;
	unsigned int comparetimes = size / 4;

	for (i = 0; i < comparetimes; i += 4) {
		dev_info(fd->dev, "0x%08x, 0x%08x, 0x%08x, 0x%08x", hw[i],
						hw[i + 1], hw[i + 2], hw[i + 3]);
	}
	dev_info(fd->dev, "Dump End");
}

static signed int fdvt_dump_reg(struct mtk_aie_dev *fd)
{
	signed int ret = 0;
	int fld_face_num = fd->aie_cfg->fld_face_num;
	unsigned int loop_num = 1;
	int i = 0;

	if (fd->aie_cfg->sel_mode == 3) {
		dev_info(fd->dev, "Blink Addr: %x\n", fd->dma_para->fld_blink_weight_pa);
		for (i = 0; i < 15; i++) {
			dev_info(fd->dev, "[%d]CV Addr: %x\n", i, fd->dma_para->fld_cv_pa[i]);
			dev_info(fd->dev, "[%d]LEAFNODE Addr: %x\n", i,
						fd->dma_para->fld_leafnode_pa[i]);
			dev_info(fd->dev, "[%d]FP Addr: %x\n", i, fd->dma_para->fld_fp_pa[i]);
			dev_info(fd->dev, "[%d]Tree02 Addr: %x\n", i,
						fd->dma_para->fld_tree02_pa[i]);
			dev_info(fd->dev, "[%d]Tree03 Addr: %x\n", i,
						fd->dma_para->fld_tree13_pa[i]);
		}
		dev_info(fd->dev, "OUT Addr: %x\n", fd->dma_para->fld_output_pa);

		dev_info(fd->dev, "- E.");
		dev_info(fd->dev, "FLD Config Info\n");
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)AIE_START_REG,
					(unsigned int)readl(fd->fd_base + AIE_START_REG));

		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)AIE_DMA_CTL_REG,
					(unsigned int)readl(fd->fd_base + AIE_DMA_CTL_REG));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)FLD_EN,
					(unsigned int)readl(fd->fd_base + FLD_EN));

		dev_info(fd->dev, "Width Hieght:\n");
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)FLD_SRC_WD_HT,
					(unsigned int)readl(fd->fd_base + FLD_SRC_WD_HT));

		dev_info(fd->dev, "FLD busy\n");
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)FLD_BUSY,
					(unsigned int)readl(fd->fd_base + FLD_BUSY));

		dev_info(fd->dev, "FLD done\n");
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)FLD_DONE,
		(unsigned int)readl(fd->fd_base + FLD_DONE));
					dev_info(fd->dev, "FLD Crop\n");

		for (i = 0; i < fld_face_num; i++) {
			dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)FLD_BASE_ADDR_FACE_0 +
			i * 0x4, (unsigned int)readl(fd->fd_base + FLD_BASE_ADDR_FACE_0 + i * 0x4));

			dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)fld_face_info_0[i],
				(unsigned int)readl(fd->fd_base + fld_face_info_0[i]));

			dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)fld_face_info_1[i],
				(unsigned int)readl(fd->fd_base + fld_face_info_1[i]));

			dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)fld_face_info_2[i],
				(unsigned int)readl(fd->fd_base + fld_face_info_2[i]));
		}
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_MODEL_PARA1,
					(unsigned int)readl(fd->fd_base + FLD_MODEL_PARA1));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_MODEL_PARA14,
					(unsigned int)readl(fd->fd_base + FLD_MODEL_PARA14));

		for (i = 0; i < FLD_MAX_INPUT; i++) {
			dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) fld_pl_in_addr_0[i],
					(unsigned int)readl(fd->fd_base + fld_pl_in_addr_0[i]));
			dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) fld_pl_in_addr_1[i],
					(unsigned int)readl(fd->fd_base + fld_pl_in_addr_1[i]));
			dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) fld_pl_in_addr_2[i],
					(unsigned int)readl(fd->fd_base + fld_pl_in_addr_2[i]));
			dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) fld_pl_in_addr_3[i],
					(unsigned int)readl(fd->fd_base + fld_pl_in_addr_3[i]));
			dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) fld_sh_in_addr[i],
					(unsigned int)readl(fd->fd_base + fld_sh_in_addr[i]));
		}

		dev_info(fd->dev, "MSB BIT\n");
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_PL_IN_BASE_ADDR_0_0_7_MSB,
				(unsigned int)readl(fd->fd_base + FLD_PL_IN_BASE_ADDR_0_0_7_MSB));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_PL_IN_BASE_ADDR_0_8_15_MSB,
				(unsigned int)readl(fd->fd_base + FLD_PL_IN_BASE_ADDR_0_8_15_MSB));

		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_PL_IN_BASE_ADDR_1_0_7_MSB,
				(unsigned int)readl(fd->fd_base + FLD_PL_IN_BASE_ADDR_1_0_7_MSB));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_PL_IN_BASE_ADDR_1_8_15_MSB,
				(unsigned int)readl(fd->fd_base + FLD_PL_IN_BASE_ADDR_1_8_15_MSB));

		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_PL_IN_BASE_ADDR_2_0_7_MSB,
				(unsigned int)readl(fd->fd_base + FLD_PL_IN_BASE_ADDR_2_0_7_MSB));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_PL_IN_BASE_ADDR_2_8_15_MSB,
				(unsigned int)readl(fd->fd_base + FLD_PL_IN_BASE_ADDR_2_8_15_MSB));

		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_PL_IN_BASE_ADDR_3_0_7_MSB,
				(unsigned int)readl(fd->fd_base + FLD_PL_IN_BASE_ADDR_3_0_7_MSB));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_PL_IN_BASE_ADDR_3_8_15_MSB,
				(unsigned int)readl(fd->fd_base + FLD_PL_IN_BASE_ADDR_3_8_15_MSB));

		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_SH_IN_BASE_ADDR_0_7_MSB,
				(unsigned int)readl(fd->fd_base + FLD_SH_IN_BASE_ADDR_0_7_MSB));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_SH_IN_BASE_ADDR_8_15_MSB,
				(unsigned int)readl(fd->fd_base + FLD_SH_IN_BASE_ADDR_8_15_MSB));

		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_BS_IN_BASE_ADDR_8_15_MSB,
				(unsigned int)readl(fd->fd_base + FLD_BS_IN_BASE_ADDR_8_15_MSB));

		dev_info(fd->dev, "OUT\n");

		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_SH_IN_SIZE_0,
				(unsigned int)readl(fd->fd_base + FLD_SH_IN_SIZE_0));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_SH_IN_STRIDE_0,
				(unsigned int)readl(fd->fd_base + FLD_SH_IN_STRIDE_0));

		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_TR_OUT_BASE_ADDR_0,
				(unsigned int)readl(fd->fd_base + FLD_TR_OUT_BASE_ADDR_0));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_TR_OUT_SIZE_0,
				(unsigned int)readl(fd->fd_base + FLD_TR_OUT_SIZE_0));

		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_TR_OUT_STRIDE_0,
				(unsigned int)readl(fd->fd_base + FLD_TR_OUT_STRIDE_0));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_PP_OUT_BASE_ADDR_0,
				(unsigned int)readl(fd->fd_base + FLD_PP_OUT_BASE_ADDR_0));

		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_PP_OUT_SIZE_0,
				(unsigned int)readl(fd->fd_base + FLD_PP_OUT_SIZE_0));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_PP_OUT_STRIDE_0,
				(unsigned int)readl(fd->fd_base + FLD_PP_OUT_STRIDE_0));

		/*cv score*/
		dev_info(fd->dev, "CV Score\n");
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_BS_BIAS,
						(unsigned int)readl(fd->fd_base + FLD_BS_BIAS));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_CV_FM_RANGE_0,
					(unsigned int)readl(fd->fd_base + FLD_CV_FM_RANGE_0));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_CV_FM_RANGE_1,
					(unsigned int)readl(fd->fd_base + FLD_CV_FM_RANGE_1));

		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_CV_PM_RANGE_0,
					(unsigned int)readl(fd->fd_base + FLD_CV_PM_RANGE_0));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_CV_PM_RANGE_1,
					(unsigned int)readl(fd->fd_base + FLD_CV_PM_RANGE_1));

		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_BS_RANGE_0,
					(unsigned int)readl(fd->fd_base + FLD_BS_RANGE_0));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_BS_RANGE_1,
					(unsigned int)readl(fd->fd_base + FLD_BS_RANGE_1));

		/*input settings*/
		dev_info(fd->dev, "input settings\n");
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_PL_IN_SIZE_0,
					(unsigned int)readl(fd->fd_base + FLD_PL_IN_SIZE_0));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_PL_IN_STRIDE_0,
					(unsigned int)readl(fd->fd_base + FLD_PL_IN_STRIDE_0));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_PL_IN_SIZE_1,
					(unsigned int)readl(fd->fd_base + FLD_PL_IN_SIZE_1));

		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_PL_IN_STRIDE_1,
					(unsigned int)readl(fd->fd_base + FLD_PL_IN_STRIDE_1));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_PL_IN_SIZE_2_0,
					(unsigned int)readl(fd->fd_base + FLD_PL_IN_SIZE_2_0));

		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_PL_IN_STRIDE_2_0,
					(unsigned int)readl(fd->fd_base + FLD_PL_IN_STRIDE_2_0));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_PL_IN_SIZE_2_1,
					(unsigned int)readl(fd->fd_base + FLD_PL_IN_SIZE_2_1));

		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_PL_IN_STRIDE_2_1,
					(unsigned int)readl(fd->fd_base + FLD_PL_IN_STRIDE_2_1));

		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_PL_IN_SIZE_2_2,
					(unsigned int)readl(fd->fd_base + FLD_PL_IN_SIZE_2_2));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_PL_IN_STRIDE_2_2,
					(unsigned int)readl(fd->fd_base + FLD_PL_IN_STRIDE_2_2));

		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_PL_IN_SIZE_3,
					(unsigned int)readl(fd->fd_base + FLD_PL_IN_SIZE_3));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int) FLD_PL_IN_STRIDE_3,
					(unsigned int)readl(fd->fd_base + FLD_PL_IN_STRIDE_3));

	} else {
		dev_info(fd->dev, "- E.");
		dev_info(fd->dev, "FDVT Config Info\n");
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)AIE_START_REG,
		(unsigned int)readl(fd->fd_base + AIE_START_REG));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)AIE_ENABLE_REG,
			(unsigned int)readl(fd->fd_base + AIE_ENABLE_REG));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)AIE_LOOP_REG,
			(unsigned int)readl(fd->fd_base + AIE_LOOP_REG));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)AIE_INT_EN_REG,
			(unsigned int)readl(fd->fd_base + AIE_INT_EN_REG));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)FDVT_SRC_WD_HT,
			(unsigned int)readl(fd->fd_base + FDVT_SRC_WD_HT));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)FDVT_DES_WD_HT,
			(unsigned int)readl(fd->fd_base + FDVT_DES_WD_HT));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)FDVT_DEBUG_INFO_0,
			(unsigned int)readl(fd->fd_base + FDVT_DEBUG_INFO_0));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)FDVT_DEBUG_INFO_1,
			(unsigned int)readl(fd->fd_base + FDVT_DEBUG_INFO_1));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)FDVT_YUV2RGB_CON,
			(unsigned int)readl(fd->fd_base + FDVT_YUV2RGB_CON));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)AIE_RS_CON_BASE_ADR_REG,
			(unsigned int)readl(fd->fd_base + AIE_RS_CON_BASE_ADR_REG));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)AIE_FD_CON_BASE_ADR_REG,
			(unsigned int)readl(fd->fd_base + AIE_FD_CON_BASE_ADR_REG));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)AIE_YUV2RGB_CON_BASE_ADR_REG,
			(unsigned int)readl(fd->fd_base + AIE_YUV2RGB_CON_BASE_ADR_REG));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)FDVT_IN_BASE_ADR_0,
			(unsigned int)readl(fd->fd_base + FDVT_IN_BASE_ADR_0));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)FDVT_IN_BASE_ADR_1,
			(unsigned int)readl(fd->fd_base + FDVT_IN_BASE_ADR_1));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)FDVT_IN_BASE_ADR_2,
			(unsigned int)readl(fd->fd_base + FDVT_IN_BASE_ADR_2));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)FDVT_IN_BASE_ADR_3,
			(unsigned int)readl(fd->fd_base + FDVT_IN_BASE_ADR_3));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)FDVT_OUT_BASE_ADR_0,
			(unsigned int)readl(fd->fd_base + FDVT_OUT_BASE_ADR_0));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)FDVT_OUT_BASE_ADR_1,
			(unsigned int)readl(fd->fd_base + FDVT_OUT_BASE_ADR_1));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)FDVT_OUT_BASE_ADR_2,
			(unsigned int)readl(fd->fd_base + FDVT_OUT_BASE_ADR_2));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)FDVT_OUT_BASE_ADR_3,
			(unsigned int)readl(fd->fd_base + FDVT_OUT_BASE_ADR_3));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)FDVT_KERNEL_BASE_ADR_0,
			(unsigned int)readl(fd->fd_base + FDVT_KERNEL_BASE_ADR_0));
		dev_info(fd->dev, "[0x%08X %08X]\n", (unsigned int)FDVT_KERNEL_BASE_ADR_1,
			(unsigned int)readl(fd->fd_base + FDVT_KERNEL_BASE_ADR_1));
#if CHECK_SERVICE_1
		dev_info(fd->dev,
			"fdmode_fdvt_yuv2rgb_config:	0x%x, fdmode_fdvt_yuv2rgb_config_size:	%d",
			fd->base_para->fd_yuv2rgb_cfg_va, fd->fd_yuv2rgb_cfg_size);
		FDVT_DumpDRAMOut(fd, (u32 *)fd->base_para->fd_yuv2rgb_cfg_va,
								fd->fd_yuv2rgb_cfg_size);
		dev_info(fd->dev,
			"fdmode_fdvt_rs_config:	  0x%x, fdmode_fdvt_rs_config_size:	 %d",
			fd->base_para->fd_rs_cfg_va, fd->fd_rs_cfg_size);
		FDVT_DumpDRAMOut(fd, (u32 *)fd->base_para->fd_rs_cfg_va, fd->fd_rs_cfg_size);

		loop_num = (unsigned int)readl(fd->fd_base + FDVT_DEBUG_INFO_0) & 0xFF;

		dev_info(fd->dev,
			"fdmode_fdvt_fd_config:	0x%x, fdmode_fdvt_fd_config_size:	%d",
			(unsigned int *)fd->base_para->fd_fd_cfg_va,
			((fd->fd_fd_cfg_size)/87) * loop_num);
		FDVT_DumpDRAMOut(fd, (u32 *)fd->base_para->fd_fd_cfg_va,
			((fd->fd_fd_cfg_size)/87) * loop_num);

		dev_info(fd->dev, "FDVT DMA Debug Info\n");

		writel(((unsigned int)readl(fd->fd_base + FDVT_CTRL_REG)) & 0xFFFFF00B,
					fd->fd_base + DMA_DEBUG_SEL_REG); //0x3f4
		writel(((unsigned int)readl(fd->fd_base + FDVT_CTRL_REG)) & 0xFFFF1FFF,
					fd->fd_base + FDVT_CTRL_REG); //0x0098 bit[15:13] = 0
		dev_info(fd->dev, "[FDVT_CTRL]: 0x%08X %08X\n",
		  (fd->fd_base + FDVT_CTRL_REG),
		  (unsigned int)readl(fd->fd_base + FDVT_CTRL_REG));
		dev_info(fd->dev, "[FDVT_DEBUG_INFO_2 - %x]: 0x%08X %08X\n", i,
			(fd->fd_base + FDVT_DEBUG_INFO_2),
			(unsigned int)readl(fd->fd_base + FDVT_DEBUG_INFO_2));

		writel(((unsigned int)readl(fd->fd_base + FDVT_CTRL_REG)) & 0xFFFFF00C,
			fd->fd_base + DMA_DEBUG_SEL_REG);
		writel(((unsigned int)readl(fd->fd_base + FDVT_CTRL_REG)) & 0xFFFF1FFF,
			fd->fd_base + FDVT_CTRL_REG); //0x0098 bit[15:13] = 0
		dev_info(fd->dev, "[FDVT_CTRL]: 0x%08X %08X\n",
		  (fd->fd_base + FDVT_CTRL_REG),
		  (unsigned int)readl(fd->fd_base + FDVT_CTRL_REG));
		dev_info(fd->dev, "[FDVT_DEBUG_INFO_2 - %x]: 0x%08X %08X\n", i,
			(fd->fd_base + FDVT_DEBUG_INFO_2),
			(unsigned int)readl(fd->fd_base + FDVT_DEBUG_INFO_2));

		writel(((unsigned int)readl(fd->fd_base + FDVT_CTRL_REG)) & 0xFFFFF00D,
			fd->fd_base + DMA_DEBUG_SEL_REG);
		writel(((unsigned int)readl(fd->fd_base + FDVT_CTRL_REG)) & 0xFFFF1FFF,
			fd->fd_base + FDVT_CTRL_REG); //0x0098 bit[15:13] = 0
		dev_info(fd->dev, "[FDVT_CTRL]: 0x%08X %08X\n",
		  (fd->fd_base + FDVT_CTRL_REG),
		  (unsigned int)readl(fd->fd_base + FDVT_CTRL_REG));
		dev_info(fd->dev, "[FDVT_DEBUG_INFO_2 - %x]: 0x%08X %08X\n", i,
			(fd->fd_base + FDVT_DEBUG_INFO_2),
			(unsigned int)readl(fd->fd_base + FDVT_DEBUG_INFO_2));

		writel(((unsigned int)readl(fd->fd_base + FDVT_CTRL_REG)) & 0xFFFFF00E,
			fd->fd_base + DMA_DEBUG_SEL_REG);
		writel(((unsigned int)readl(fd->fd_base + FDVT_CTRL_REG)) & 0xFFFF1FFF,
			fd->fd_base + FDVT_CTRL_REG); //0x0098 bit[15:13] = 0
		dev_info(fd->dev, "[FDVT_CTRL]: 0x%08X %08X\n",
		  (fd->fd_base + FDVT_CTRL_REG),
		  (unsigned int)readl(fd->fd_base + FDVT_CTRL_REG));
		dev_info(fd->dev, "[FDVT_DEBUG_INFO_2 - %x]: 0x%08X %08X\n", i,
			(fd->fd_base + FDVT_DEBUG_INFO_2),
			(unsigned int)readl(fd->fd_base + FDVT_DEBUG_INFO_2));

		writel(((unsigned int)readl(fd->fd_base + FDVT_CTRL_REG)) & 0xFFFF1FFF,
			fd->fd_base + FDVT_CTRL_REG);
		dev_info(fd->dev, "[FDVT_CTRL - %x]: 0x%08X %08X\n", i,
		  (fd->fd_base + FDVT_CTRL_REG),
		  (unsigned int)readl(fd->fd_base + FDVT_CTRL_REG));

		writel((((unsigned int)readl(fd->fd_base + DMA_DEBUG_SEL_REG)) &
		   0xFFFFFF00) | 0x13, fd->fd_base + DMA_DEBUG_SEL_REG);

		for (i = 0; i <= 0x27; i++) {
			if (i > 0x7 && i < 0x10)
				continue;
			writel((((unsigned int)readl(fd->fd_base + DMA_DEBUG_SEL_REG)) &
			   0xFFFF00FF) | (i << 8), fd->fd_base + DMA_DEBUG_SEL_REG);
			dev_info(fd->dev, "[FDVT_DEBUG_SEL - %x]: 0x%08X %08X\n", i,
				(fd->fd_base + DMA_DEBUG_SEL_REG),
				(unsigned int)readl(fd->fd_base + DMA_DEBUG_SEL_REG));

			dev_info(fd->dev, "[FDVT_DEBUG_INFO_2 - %x]: 0x%08X %08X\n", i,
				(fd->fd_base + FDVT_DEBUG_INFO_2),
				(unsigned int)readl(fd->fd_base + FDVT_DEBUG_INFO_2));
		}

		dev_info(fd->dev, "FDVT SMI Debug Info\n");
		dev_info(fd->dev, "FDVT Write FDVT_A_DMA_DEBUG_SEL[15:8] = 0x1\n");
		dev_info(fd->dev, "FDVT Write FDVT_A_DMA_DEBUG_SEL[23:16] = 0x0\n");
		writel((((unsigned int)readl(fd->fd_base + DMA_DEBUG_SEL_REG)) &
			0xFFFF00FF) | (1 << 8), fd->fd_base + DMA_DEBUG_SEL_REG);
		writel(((unsigned int)readl(fd->fd_base + DMA_DEBUG_SEL_REG))
			& 0xFF00FFFF, fd->fd_base + DMA_DEBUG_SEL_REG);

		for (i = 1; i <= 0xe; i++) {
			writel((((unsigned int)readl(fd->fd_base + DMA_DEBUG_SEL_REG)) &
				0xFFFFFF00) | i, fd->fd_base + DMA_DEBUG_SEL_REG);
			dev_info(fd->dev, "[FDVT_DEBUG_SEL SMI - %x]: 0x%08X %08X\n", i,
				(fd->fd_base + DMA_DEBUG_SEL_REG),
				(unsigned int)readl(fd->fd_base + DMA_DEBUG_SEL_REG));
			dev_info(fd->dev, "[FDVT_DEBUG_INFO_2 SMI - %x]: 0x%08X %08X\n", i,
				(fd->fd_base + FDVT_DEBUG_INFO_2),
				(unsigned int)readl(fd->fd_base + FDVT_DEBUG_INFO_2));
		}

		dev_info(fd->dev, "FDVT fifo_debug_data_case1\n");
		dev_info(fd->dev, "FDVT Write FDVT_A_DMA_DEBUG_SEL[15:8] = 0x2\n");
		dev_info(fd->dev, "FDVT Write FDVT_A_DMA_DEBUG_SEL[23:16] = 0x1\n");
		writel((((unsigned int)readl(fd->fd_base + DMA_DEBUG_SEL_REG)) &
			0xFFFF00FF) | (2 << 8), fd->fd_base + DMA_DEBUG_SEL_REG);
		writel((((unsigned int)readl(fd->fd_base + DMA_DEBUG_SEL_REG)) &
			0xFF00FFFF) | (1 << 16), fd->fd_base + DMA_DEBUG_SEL_REG);

		for (i = 1; i <= 0xe; i++) {
			writel((((unsigned int)readl(fd->fd_base + DMA_DEBUG_SEL_REG)) &
				0xFFFFFF00) | i, fd->fd_base + DMA_DEBUG_SEL_REG);
			dev_info(fd->dev, "[FDVT_DEBUG_SEL SMI - %x]: 0x%08X %08X\n", i,
				(fd->fd_base + DMA_DEBUG_SEL_REG),
				(unsigned int)readl(fd->fd_base + DMA_DEBUG_SEL_REG));
			dev_info(fd->dev, "[FDVT_DEBUG_INFO_2 SMI - %x]: 0x%08X %08X\n", i,
				(fd->fd_base + FDVT_DEBUG_INFO_2),
				(unsigned int)readl(fd->fd_base + FDVT_DEBUG_INFO_2));
		}

		dev_info(fd->dev, "FDVT fifo_debug_data_case3\n");
		dev_info(fd->dev, "FDVT Write FDVT_A_DMA_DEBUG_SEL[15:8] = 0x2\n");
		dev_info(fd->dev, "FDVT Write FDVT_A_DMA_DEBUG_SEL[23:16] = 0x3\n");
		writel((((unsigned int)readl(fd->fd_base + DMA_DEBUG_SEL_REG)) &
			0xFFFF00FF) | (2 << 8), fd->fd_base + DMA_DEBUG_SEL_REG);
		writel((((unsigned int)readl(fd->fd_base + DMA_DEBUG_SEL_REG)) &
			0xFF00FFFF) | (3 << 16), fd->fd_base + DMA_DEBUG_SEL_REG);

		for (i = 1; i <= 0xe; i++) {
			writel((((unsigned int)readl(fd->fd_base + DMA_DEBUG_SEL_REG)) &
			   0xFFFFFF00) | i, fd->fd_base + DMA_DEBUG_SEL_REG);
			dev_info(fd->dev, "[FDVT_DEBUG_SEL SMI - %x]: 0x%08X %08X\n", i,
				(fd->fd_base + DMA_DEBUG_SEL_REG),
				(unsigned int)readl(fd->fd_base + DMA_DEBUG_SEL_REG));
			dev_info(fd->dev, "[FDVT_DEBUG_INFO_2 SMI - %x]: 0x%08X %08X\n", i,
				(fd->fd_base + FDVT_DEBUG_INFO_2),
				(unsigned int)readl(fd->fd_base + FDVT_DEBUG_INFO_2));
		}
#endif
	}
	return ret;
}

static void mtk_aie_job_timeout_work(struct work_struct *work)
{
	struct mtk_aie_dev *fd =
		container_of(work, struct mtk_aie_dev, job_timeout_work.work);

	dev_info(fd->dev, "FD Job timeout!");

	dev_info(fd->dev, "AIE mode:%d fmt:%d w:%d h:%d s:%d pw%d ph%d np%d dg%d",
		 fd->aie_cfg->sel_mode,
		 fd->aie_cfg->src_img_fmt,
		 fd->aie_cfg->src_img_width,
		 fd->aie_cfg->src_img_height,
		 fd->aie_cfg->src_img_stride,
		 fd->aie_cfg->pyramid_base_width,
		 fd->aie_cfg->pyramid_base_height,
		 fd->aie_cfg->number_of_pyramid,
		 fd->aie_cfg->rotate_degree);
	dev_info(fd->dev, "roi%d x1:%d y1:%d x2:%d y2:%d pad%d l%d r%d d%d u%d f%d",
		 fd->aie_cfg->en_roi,
		 fd->aie_cfg->src_roi.x1,
		 fd->aie_cfg->src_roi.y1,
		 fd->aie_cfg->src_roi.x2,
		 fd->aie_cfg->src_roi.y2,
		 fd->aie_cfg->en_padding,
		 fd->aie_cfg->src_padding.left,
		 fd->aie_cfg->src_padding.right,
		 fd->aie_cfg->src_padding.down,
		 fd->aie_cfg->src_padding.up,
		 fd->aie_cfg->freq_level);

	dev_info(fd->dev, "%s result result1: %x, %x, %x", __func__,
		 readl(fd->fd_base + AIE_RESULT_0_REG),
		 readl(fd->fd_base + AIE_RESULT_1_REG),
		 readl(fd->fd_base + AIE_DMA_CTL_REG));

	dev_info(fd->dev, "%s interrupt status: %x", __func__,
		 readl(fd->fd_base + AIE_INT_EN_REG));

	if (fd->aie_cfg->sel_mode == 1)
		dev_info(fd->dev, "[ATTRMODE] w_idx = %d, r_idx = %d\n",
			 fd->attr_para->w_idx, fd->attr_para->r_idx);
	fdvt_dump_reg(fd);

	aie_irqhandle(fd);
	aie_reset(fd);
	mtk_aie_hw_job_finish(fd, VB2_BUF_STATE_ERROR);
	atomic_dec(&fd->num_composing);
	wake_up(&fd->flushing_waitq);
}

static void mtk_aie_job_wait_finish(struct mtk_aie_dev *fd)
{
	wait_for_completion_timeout(&fd->fd_job_finished, msecs_to_jiffies(3000));
}

static void mtk_aie_vb2_stop_streaming(struct vb2_queue *vq)
{
	struct mtk_aie_ctx *ctx = vb2_get_drv_priv(vq);
	struct mtk_aie_dev *fd = ctx->fd_dev;
	struct vb2_v4l2_buffer *vb;
	struct v4l2_m2m_ctx *m2m_ctx = ctx->fh.m2m_ctx;
	struct v4l2_m2m_queue_ctx *queue_ctx;

	dev_info(fd->dev, "STREAM STOP\n");

	mtk_aie_job_wait_finish(fd);
	queue_ctx = V4L2_TYPE_IS_OUTPUT(vq->type) ? &m2m_ctx->out_q_ctx
						  : &m2m_ctx->cap_q_ctx;
	while ((vb = v4l2_m2m_buf_remove(queue_ctx)))
		v4l2_m2m_buf_done(vb, VB2_BUF_STATE_ERROR);

	if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		mtk_aie_hw_disconnect(fd);
}

static void mtk_aie_vb2_request_complete(struct vb2_buffer *vb)
{
	struct mtk_aie_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);

	v4l2_ctrl_request_complete(vb->req_obj.req, &ctx->hdl);
}

static int mtk_aie_querycap(struct file *file, void *fh,
			    struct v4l2_capability *cap)
{
	struct mtk_aie_dev *fd = video_drvdata(file);
	struct device *dev = fd->dev;

	strscpy(cap->driver, dev_driver_string(dev), sizeof(cap->driver));
	strscpy(cap->card, dev_driver_string(dev), sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s",
		 dev_name(fd->dev));

	return 0;
}

static int mtk_aie_enum_fmt_out_mp(struct file *file, void *fh,
				   struct v4l2_fmtdesc *f)
{
	if (f->index >= NUM_FORMATS)
		return -EINVAL;

	f->pixelformat = mtk_aie_img_fmts[f->index].pixelformat;
	return 0;
}

static void mtk_aie_fill_pixfmt_mp(struct v4l2_pix_format_mplane *dfmt,
				   const struct v4l2_pix_format_mplane *sfmt)
{
	dfmt->field = V4L2_FIELD_NONE;
	dfmt->colorspace = V4L2_COLORSPACE_BT2020;
	dfmt->num_planes = sfmt->num_planes;
	dfmt->ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	dfmt->quantization = V4L2_QUANTIZATION_DEFAULT;
	dfmt->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(dfmt->colorspace);

	/* Keep user setting as possible */
	dfmt->width = clamp(dfmt->width, MTK_FD_OUTPUT_MIN_WIDTH,
			    MTK_FD_OUTPUT_MAX_WIDTH);
	dfmt->height = clamp(dfmt->height, MTK_FD_OUTPUT_MIN_HEIGHT,
			     MTK_FD_OUTPUT_MAX_HEIGHT);

	if (sfmt->num_planes == 2) {
		dfmt->plane_fmt[0].sizeimage =
			dfmt->height * dfmt->plane_fmt[0].bytesperline;
		dfmt->plane_fmt[1].sizeimage =
			dfmt->height * dfmt->plane_fmt[1].bytesperline;
		if (sfmt->pixelformat == V4L2_PIX_FMT_NV12M)
			dfmt->plane_fmt[1].sizeimage =
				dfmt->height * dfmt->plane_fmt[1].bytesperline /
				2;
	} else {
		dfmt->plane_fmt[0].sizeimage =
			dfmt->height * dfmt->plane_fmt[0].bytesperline;
		if (sfmt->pixelformat == V4L2_PIX_FMT_NV12)
			dfmt->plane_fmt[0].sizeimage =
				dfmt->height * dfmt->plane_fmt[0].bytesperline *
				3 / 2;
	}
}

static const struct v4l2_pix_format_mplane *mtk_aie_find_fmt(u32 format)
{
	unsigned int i;

	for (i = 0; i < NUM_FORMATS; i++) {
		if (mtk_aie_img_fmts[i].pixelformat == format)
			return &mtk_aie_img_fmts[i];
	}

	return NULL;
}

static int mtk_aie_try_fmt_out_mp(struct file *file, void *fh,
				  struct v4l2_format *f)
{
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	const struct v4l2_pix_format_mplane *fmt;

	fmt = mtk_aie_find_fmt(pix_mp->pixelformat);
	if (!fmt)
		fmt = &mtk_aie_img_fmts[0]; /* Get default img fmt */

	mtk_aie_fill_pixfmt_mp(pix_mp, fmt);
	return 0;
}

static int mtk_aie_g_fmt_out_mp(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct mtk_aie_ctx *ctx = fh_to_ctx(fh);

	f->fmt.pix_mp = ctx->src_fmt;

	return 0;
}

static int mtk_aie_s_fmt_out_mp(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct mtk_aie_ctx *ctx = fh_to_ctx(fh);
	struct vb2_queue *vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, f->type);

	/* Change not allowed if queue is streaming. */
	if (vb2_is_streaming(vq)) {
		dev_info(ctx->dev, "Failed to set format, vb2 is busy\n");
		return -EBUSY;
	}

	mtk_aie_try_fmt_out_mp(file, fh, f);
	ctx->src_fmt = f->fmt.pix_mp;

	return 0;
}

static int mtk_aie_enum_fmt_meta_cap(struct file *file, void *fh,
				     struct v4l2_fmtdesc *f)
{
	struct mtk_aie_dev *fd = video_drvdata(file);

	dev_info(fd->dev, "mtk_aie_g_fmt_meta_cap start!\n");
	if (f->index)
		return -EINVAL;

	strscpy(f->description, "Face detection result",
		sizeof(f->description));

	f->pixelformat = V4L2_META_FMT_MTFD_RESULT;
	f->flags = 0;

	return 0;
}

static int mtk_aie_g_fmt_meta_cap(struct file *file, void *fh,
				  struct v4l2_format *f)
{
	struct mtk_aie_dev *fd = video_drvdata(file);

	dev_info(fd->dev, "mtk_aie_g_fmt_meta_cap start!\n");
	f->fmt.meta.dataformat = V4L2_META_FMT_MTFD_RESULT;
	f->fmt.meta.buffersize = sizeof(struct aie_enq_info);

	return 0;
}

int mtk_aie_vidioc_qbuf(struct file *file, void *priv,
				  struct v4l2_buffer *buf)
{
	struct mtk_aie_dev *fd = video_drvdata(file);
	unsigned long long ret = 0;
	/* dev_info(fd->dev, "[%s] start!:%x %x\n", __func__, buf->length, fd->map_count); */

	if (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) { /*IMG & data*/
		if (!fd->map_count) {

			fd->dmabuf = dma_buf_get(buf->m.planes[buf->length-1].m.fd);
			if (IS_ERR(fd->dmabuf) || fd->dmabuf == NULL) {
				dev_info(fd->dev, "%s, dma_buf_getad failed\n", __func__);
				return -ENOMEM;
			}

			ret = dma_buf_begin_cpu_access(fd->dmabuf, DMA_BIDIRECTIONAL);
			if (ret < 0) {
				dev_info(fd->dev, "%s, begin_cpu_access failed\n", __func__);
				ret = -ENOMEM;
				goto ERROR_PUTBUF;
			}

			ret = (u64)dma_buf_vmap(fd->dmabuf);
			if (!ret) {
				dev_info(fd->dev, "%s, map kernel va failed\n", __func__);
				ret = -ENOMEM;
				goto ERROR;
			}
			fd->kva = ret;
			memcpy((char *)&g_user_param, (char *)fd->kva, sizeof(g_user_param));
			fd->base_para->rpn_anchor_thrd = (signed short)
						(g_user_param.feature_threshold & 0x0000FFFF);
			fd->base_para->max_img_height = (signed short)
						(g_user_param.max_img_height & 0x0000FFFF);
			fd->base_para->max_img_width = (signed short)
						(g_user_param.max_img_width & 0x0000FFFF);
			fd->base_para->pyramid_width = (signed short)
						(g_user_param.pyramid_width & 0x0000FFFF);
			fd->base_para->pyramid_height = (signed short)
						(g_user_param.pyramid_height & 0x0000FFFF);
			fd->base_para->max_pyramid_width = (signed short)
						(g_user_param.pyramid_width & 0x0000FFFF);
			fd->base_para->max_pyramid_height = (signed short)
						(g_user_param.pyramid_height & 0x0000FFFF);

			dev_info(fd->dev, "AIE QBUF!: %d\n", g_user_param.is_secure);

			if (g_user_param.is_secure) {
				dev_info(fd->dev, "AIE SECURE MODE INIT!\n");
				config_aie_cmdq_secure_init(fd);
				aie_enable_secure_domain(fd);
			}

			ret = aie_alloc_aie_buf(fd);
			if (ret)
				return ret;
			fd->map_count++;
		} else {
			memcpy((char *)&g_user_param, (char *)fd->kva, sizeof(g_user_param));
		}

	}

	return v4l2_m2m_ioctl_qbuf(file, priv, buf);

ERROR:
	dma_buf_end_cpu_access(fd->dmabuf, DMA_BIDIRECTIONAL);
ERROR_PUTBUF:
	dma_buf_put(fd->dmabuf);

	return ret;
}

static const struct vb2_ops mtk_aie_vb2_ops = {
	.queue_setup = mtk_aie_vb2_queue_setup,
	.buf_out_validate = mtk_aie_vb2_buf_out_validate,
	.buf_prepare = mtk_aie_vb2_buf_prepare,
	.buf_queue = mtk_aie_vb2_buf_queue,
	.start_streaming = mtk_aie_vb2_start_streaming,
	.stop_streaming = mtk_aie_vb2_stop_streaming,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.buf_request_complete = mtk_aie_vb2_request_complete,
};

static const struct v4l2_ioctl_ops mtk_aie_v4l2_video_out_ioctl_ops = {
	.vidioc_querycap = mtk_aie_querycap,
	.vidioc_enum_fmt_vid_out = mtk_aie_enum_fmt_out_mp,
	.vidioc_g_fmt_vid_out_mplane = mtk_aie_g_fmt_out_mp,
	.vidioc_s_fmt_vid_out_mplane = mtk_aie_s_fmt_out_mp,
	.vidioc_try_fmt_vid_out_mplane = mtk_aie_try_fmt_out_mp,
	.vidioc_enum_fmt_meta_cap = mtk_aie_enum_fmt_meta_cap,
	.vidioc_g_fmt_meta_cap = mtk_aie_g_fmt_meta_cap,
	.vidioc_s_fmt_meta_cap = mtk_aie_g_fmt_meta_cap,
	.vidioc_try_fmt_meta_cap = mtk_aie_g_fmt_meta_cap,
	.vidioc_reqbufs = v4l2_m2m_ioctl_reqbufs,
	.vidioc_create_bufs = v4l2_m2m_ioctl_create_bufs,
	.vidioc_expbuf = v4l2_m2m_ioctl_expbuf,
	.vidioc_prepare_buf = v4l2_m2m_ioctl_prepare_buf,
	.vidioc_querybuf = v4l2_m2m_ioctl_querybuf,
	.vidioc_qbuf = mtk_aie_vidioc_qbuf,
	.vidioc_dqbuf = v4l2_m2m_ioctl_dqbuf,
	.vidioc_streamon = v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff = v4l2_m2m_ioctl_streamoff,
	.vidioc_subscribe_event = v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

static int mtk_aie_queue_init(void *priv, struct vb2_queue *src_vq,
			      struct vb2_queue *dst_vq)
{
	struct mtk_aie_ctx *ctx = priv;
	int ret;

	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_MMAP | VB2_DMABUF;
	src_vq->supports_requests = true;
	src_vq->drv_priv = ctx;
	src_vq->ops = &mtk_aie_vb2_ops;
	src_vq->mem_ops = &vb2_dma_contig_memops;
	src_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock = &ctx->fd_dev->vfd_lock;
	src_vq->dev = ctx->fd_dev->v4l2_dev.dev;

	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	dst_vq->type = V4L2_BUF_TYPE_META_CAPTURE;
	dst_vq->io_modes = VB2_MMAP | VB2_DMABUF;
	dst_vq->drv_priv = ctx;
	dst_vq->ops = &mtk_aie_vb2_ops;
	dst_vq->mem_ops = &vb2_dma_contig_memops;
	dst_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock = &ctx->fd_dev->vfd_lock;
	dst_vq->dev = ctx->fd_dev->v4l2_dev.dev;

	return vb2_queue_init(dst_vq);
}
#if CHECK_SERVICE_0 //Remove CID
static struct v4l2_ctrl_config mtk_aie_controls[] = {
	{
		.id = V4L2_CID_MTK_AIE_INIT,
		.name = "FD detection init",
		.type = V4L2_CTRL_TYPE_U32,
		.min = 0,
		.max = 0xffffffff,
		.step = 1,
		.def = 0,
		.dims = {sizeof(struct user_init) / 4},
	},
	{
		.id = V4L2_CID_MTK_AIE_PARAM,
		.name = "FD detection param",
		.type = V4L2_CTRL_TYPE_U32,
		.min = 0,
		.max = 0xffffffff,
		.step = 1,
		.def = 0,
		.dims = {sizeof(struct user_param) / 4},
	},
	{
		.id = V4L2_CID_MTK_AIE_FD_VER,
		.name = "FD detection fd ver",
		.type = V4L2_CTRL_TYPE_U32,
		.min = 0,
		.max = 0xffffffff,
		.step = 1,
		.def = FD_VERSION,
		.dims = {1},
	},
	{
		.id = V4L2_CID_MTK_AIE_ATTR_VER,
		.name = "FD detection attr ver",
		.type = V4L2_CTRL_TYPE_U32,
		.min = 0,
		.max = 0xffffffff,
		.step = 1,
		.def = ATTR_VERSION,
		.dims = {1},
	},
};

static int mtk_aie_ctrls_setup(struct mtk_aie_ctx *ctx)
{
	struct v4l2_ctrl_handler *hdl = &ctx->hdl;
	int i;

	v4l2_ctrl_handler_init(hdl, V4L2_CID_MTK_AIE_MAX);
	if (hdl->error)
		return hdl->error;

	for (i = 0; i < ARRAY_SIZE(mtk_aie_controls); i++) {
		v4l2_ctrl_new_custom(hdl, &mtk_aie_controls[i], ctx);
		if (hdl->error) {
			v4l2_ctrl_handler_free(hdl);
			dev_info(ctx->dev, "Failed to register controls:%d", i);
			return hdl->error;
		}
	}

	ctx->fh.ctrl_handler = &ctx->hdl;
	v4l2_ctrl_handler_setup(hdl);

	return 0;
}
#endif
static void init_ctx_fmt(struct mtk_aie_ctx *ctx)
{
	struct v4l2_pix_format_mplane *src_fmt = &ctx->src_fmt;
	struct v4l2_meta_format *dst_fmt = &ctx->dst_fmt;

	/* Initialize M2M source fmt */
	src_fmt->width = MTK_FD_OUTPUT_MAX_WIDTH;
	src_fmt->height = MTK_FD_OUTPUT_MAX_HEIGHT;
	mtk_aie_fill_pixfmt_mp(src_fmt, &mtk_aie_img_fmts[0]);

	/* Initialize M2M destination fmt */
	dst_fmt->buffersize = sizeof(struct aie_enq_info);
	dst_fmt->dataformat = V4L2_META_FMT_MTFD_RESULT;
}

/*
 * V4L2 file operations.
 */
static int mtk_vfd_open(struct file *filp)
{
	struct mtk_aie_dev *fd = video_drvdata(filp);
	struct video_device *vdev = video_devdata(filp);
	struct mtk_aie_ctx *ctx;
	int ret;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->fd_dev = fd;
	ctx->dev = fd->dev;
	fd->ctx = ctx;

	v4l2_fh_init(&ctx->fh, vdev);
	filp->private_data = &ctx->fh;

	init_ctx_fmt(ctx);
#if CHECK_SERVICE_0 //Remove CID
	ret = mtk_aie_ctrls_setup(ctx);
	if (ret) {
		dev_info(ctx->dev, "Failed to set up controls:%d\n", ret);
		goto err_fh_exit;
	}
#endif
	ctx->fh.m2m_ctx =
		v4l2_m2m_ctx_init(fd->m2m_dev, ctx, &mtk_aie_queue_init);
	if (IS_ERR(ctx->fh.m2m_ctx)) {
		ret = PTR_ERR(ctx->fh.m2m_ctx);
		goto err_free_ctrl_handler;
	}

	v4l2_fh_add(&ctx->fh);

	return 0;

err_free_ctrl_handler:
	v4l2_ctrl_handler_free(&ctx->hdl);
#if CHECK_SERVICE_0 //Remove CID
err_fh_exit:
#endif
	v4l2_fh_exit(&ctx->fh);
	kfree(ctx);

	return ret;
}

static int mtk_vfd_release(struct file *filp)
{
	struct mtk_aie_ctx *ctx =
		container_of(filp->private_data, struct mtk_aie_ctx, fh);

	v4l2_m2m_ctx_release(ctx->fh.m2m_ctx);

	v4l2_ctrl_handler_free(&ctx->hdl);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);

	kfree(ctx);

	return 0;
}

static const struct v4l2_file_operations fd_video_fops = {
	.owner = THIS_MODULE,
	.open = mtk_vfd_open,
	.release = mtk_vfd_release,
	.poll = v4l2_m2m_fop_poll,
	.unlocked_ioctl = video_ioctl2,
	.mmap = v4l2_m2m_fop_mmap,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = v4l2_compat_ioctl32,
#endif

};
#if CHECK_SERVICE_0
static void mtk_aie_fill_user_param(struct mtk_aie_dev *fd,
				    struct user_param *user_param,
				    struct v4l2_ctrl_handler *hdl)
{
	struct v4l2_ctrl *ctrl;

	ctrl = v4l2_ctrl_find(hdl, V4L2_CID_MTK_AIE_PARAM);
	if (ctrl)
		memcpy(user_param, ctrl->p_new.p_u32, sizeof(struct user_param));
}
#endif
static void mtk_aie_device_run(void *priv)
{
	struct mtk_aie_ctx *ctx = priv;
	struct mtk_aie_dev *fd = ctx->fd_dev;
	struct vb2_v4l2_buffer *src_buf, *dst_buf;
	struct fd_enq_param fd_param;
	void *plane_vaddr;
	unsigned long long img_y = 0;
	unsigned long long img_uv = 0;
	unsigned int img_msb = 0;
	unsigned int set_msb_bit = 0;
	int ret = 0;

	src_buf = v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);
	dst_buf = v4l2_m2m_next_dst_buf(ctx->fh.m2m_ctx);

	img_y = vb2_dma_contig_plane_dma_addr(&src_buf->vb2_buf, 0);
	fd_param.src_img[0].dma_addr = img_y & 0xffffffff;

	if (ctx->src_fmt.num_planes == 2) {
		img_uv = vb2_dma_contig_plane_dma_addr(&src_buf->vb2_buf, 1);
		fd_param.src_img[1].dma_addr = img_uv & 0xffffffff;
	}
	fd->img_msb_y = (img_y & 0Xf00000000) >> 32;

	vb2_dma_contig_plane_dma_addr(&src_buf->vb2_buf, 1);
#if CHECK_SERVICE_0 //Remove CID
	mtk_aie_fill_user_param(fd, &fd_param.user_param, &ctx->hdl);
#endif
	plane_vaddr = vb2_plane_vaddr(&dst_buf->vb2_buf, 0);

	fd->aie_cfg = (struct aie_enq_info *)plane_vaddr;

	fd->aie_cfg->sel_mode = g_user_param.user_param.fd_mode;
	fd->aie_cfg->src_img_fmt = g_user_param.user_param.src_img_fmt;
	fd->aie_cfg->src_img_width = g_user_param.user_param.src_img_width;
	fd->aie_cfg->src_img_height = g_user_param.user_param.src_img_height;
	fd->aie_cfg->src_img_stride = g_user_param.user_param.src_img_stride;
	fd->aie_cfg->pyramid_base_width = g_user_param.user_param.pyramid_base_width;
	fd->aie_cfg->pyramid_base_height = g_user_param.user_param.pyramid_base_height;
	fd->aie_cfg->number_of_pyramid = g_user_param.user_param.number_of_pyramid;
	fd->aie_cfg->rotate_degree = g_user_param.user_param.rotate_degree;
	fd->aie_cfg->en_roi = g_user_param.user_param.en_roi;
	fd->aie_cfg->src_roi.x1 = g_user_param.user_param.src_roi_x1;
	fd->aie_cfg->src_roi.y1 = g_user_param.user_param.src_roi_y1;
	fd->aie_cfg->src_roi.x2 = g_user_param.user_param.src_roi_x2;
	fd->aie_cfg->src_roi.y2 = g_user_param.user_param.src_roi_y2;
	fd->aie_cfg->en_padding = g_user_param.user_param.en_padding;
	fd->aie_cfg->src_padding.left = g_user_param.user_param.src_padding_left;
	fd->aie_cfg->src_padding.right = g_user_param.user_param.src_padding_right;
	fd->aie_cfg->src_padding.down = g_user_param.user_param.src_padding_down;
	fd->aie_cfg->src_padding.up = g_user_param.user_param.src_padding_up;
	fd->aie_cfg->freq_level = g_user_param.user_param.freq_level;

	if (fd->aie_cfg->sel_mode == 3) {
		fd->aie_cfg->fld_face_num = g_user_param.user_param.fld_face_num;
		memcpy(fd->aie_cfg->fld_input, g_user_param.user_param.fld_input,
		sizeof(struct FLD_CROP_RIP_ROP)*g_user_param.user_param.fld_face_num);
	}
	if (!(fd->aie_cfg->sel_mode == 3) & (fd->aie_cfg->src_img_fmt == FMT_YUV420_1P)) {
		/*FLD Just have Y*/
		fd_param.src_img[1].dma_addr = fd_param.src_img[0].dma_addr +
			g_user_param.user_param.src_img_stride *
			g_user_param.user_param.src_img_height;
		fd->img_msb_uv = ((img_y +
			g_user_param.user_param.src_img_stride *
			g_user_param.user_param.src_img_height) &
			0Xf00000000) >> 32;
	}

	fd->aie_cfg->src_img_addr = fd_param.src_img[0].dma_addr;
	fd->aie_cfg->src_img_addr_uv = fd_param.src_img[1].dma_addr;

	if (!(fd->aie_cfg->sel_mode == 3)) {
		ret = aie_prepare(fd, fd->aie_cfg);//fld just setting debug param
	} else {

		img_msb = fd->img_msb_y;  //MASK MSB-BIT
		set_msb_bit = img_msb | img_msb << 4 | img_msb << 8 | img_msb << 12;
		set_msb_bit = set_msb_bit | set_msb_bit << 16;

		writel(set_msb_bit, fd->fd_base + FLD_BASE_ADDR_FACE_0_7_MSB);
		set_msb_bit = set_msb_bit & 0xfffffff;
		writel(set_msb_bit, fd->fd_base + FLD_BASE_ADDR_FACE_8_14_MSB);
		//for UT
		//writel(0x33333333, fd->fd_base + FLD_BASE_ADDR_FACE_0_7_MSB);
		//writel(0x03333333, fd->fd_base + FLD_BASE_ADDR_FACE_8_14_MSB);

		fd->fld_para->sel_mode = fd->aie_cfg->sel_mode;
		fd->fld_para->img_height = fd->aie_cfg->src_img_height;
		fd->fld_para->img_width = fd->aie_cfg->src_img_width;
		fd->fld_para->face_num = fd->aie_cfg->fld_face_num;
		fd->fld_para->src_img_addr = fd->aie_cfg->src_img_addr;
		memcpy(fd->fld_para->fld_input, fd->aie_cfg->fld_input,
			sizeof(struct FLD_CROP_RIP_ROP) * fd->aie_cfg->fld_face_num);
	}

	/* mmdvfs */
	//mtk_aie_mmdvfs_set(fd, 1, fd->aie_cfg->freq_level);

	/* Complete request controls if any */
	v4l2_ctrl_request_complete(src_buf->vb2_buf.req_obj.req, &ctx->hdl);

	atomic_inc(&fd->num_composing);

	mtk_aie_hw_job_exec(fd, &fd_param);

	if (ret) {
		dev_info(fd->dev, "Failed to prepare aie setting\n");
		return;
	}

	aie_execute(fd, fd->aie_cfg);
}

static struct v4l2_m2m_ops fd_m2m_ops = {
	.device_run = mtk_aie_device_run,
};

static const struct media_device_ops fd_m2m_media_ops = {
	.req_validate = vb2_request_validate,
	.req_queue = v4l2_m2m_request_queue,
};

static int mtk_aie_video_device_register(struct mtk_aie_dev *fd)
{
	struct video_device *vfd = &fd->vfd;
	struct v4l2_m2m_dev *m2m_dev = fd->m2m_dev;
	struct device *dev = fd->dev;
	int ret;

	vfd->fops = &fd_video_fops;
	vfd->release = video_device_release;
	vfd->lock = &fd->vfd_lock;
	vfd->v4l2_dev = &fd->v4l2_dev;
	vfd->vfl_dir = VFL_DIR_M2M;
	vfd->device_caps = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_OUTPUT_MPLANE |
			   V4L2_CAP_META_CAPTURE;
	vfd->ioctl_ops = &mtk_aie_v4l2_video_out_ioctl_ops;

	strscpy(vfd->name, dev_driver_string(dev), sizeof(vfd->name));

	video_set_drvdata(vfd, fd);

	ret = video_register_device(vfd, VFL_TYPE_VIDEO, 0);
	if (ret) {
		dev_info(dev, "Failed to register video device\n");
		goto err_free_dev;
	}

	ret = v4l2_m2m_register_media_controller(
		m2m_dev, vfd, MEDIA_ENT_F_PROC_VIDEO_STATISTICS);
	if (ret) {
		dev_info(dev, "Failed to init mem2mem media controller\n");
		goto err_unreg_video;
	}
	return 0;

err_unreg_video:
	video_unregister_device(vfd);
err_free_dev:
	video_device_release(vfd);
	return ret;
}

static int mtk_aie_dev_larb_init(struct mtk_aie_dev *fd)
{
	struct device_node *node;
	struct platform_device *pdev;
	struct device_link *link;

	node = of_parse_phandle(fd->dev->of_node, "mediatek,larb", 0);
	if (!node)
		return -EINVAL;
	pdev = of_find_device_by_node(node);
	if (WARN_ON(!pdev)) {
		of_node_put(node);
		return -EINVAL;
	}
	of_node_put(node);

	fd->larb = &pdev->dev;

	link = device_link_add(fd->dev, &pdev->dev,
					DL_FLAG_PM_RUNTIME | DL_FLAG_STATELESS);
	if (!link) {
		dev_info(fd->dev, "unable to link SMI LARB idx %d\n");
		return -EINVAL;
	}

	return 0;
}

static int mtk_aie_dev_v4l2_init(struct mtk_aie_dev *fd)
{
	struct media_device *mdev = &fd->mdev;
	struct device *dev = fd->dev;
	int ret;

	ret = v4l2_device_register(dev, &fd->v4l2_dev);
	if (ret) {
		dev_info(dev, "Failed to register v4l2 device\n");
		return ret;
	}

	fd->m2m_dev = v4l2_m2m_init(&fd_m2m_ops);
	if (IS_ERR(fd->m2m_dev)) {
		dev_info(dev, "Failed to init mem2mem device\n");
		ret = PTR_ERR(fd->m2m_dev);
		goto err_unreg_v4l2_dev;
	}

	mdev->dev = dev;
	strscpy(mdev->model, dev_driver_string(dev), sizeof(mdev->model));
	snprintf(mdev->bus_info, sizeof(mdev->bus_info), "platform:%s",
		 dev_name(dev));
	media_device_init(mdev);
	mdev->ops = &fd_m2m_media_ops;
	fd->v4l2_dev.mdev = mdev;

	ret = mtk_aie_video_device_register(fd);
	if (ret) {
		dev_info(dev, "Failed to register video device\n");
		goto err_cleanup_mdev;
	}

	ret = media_device_register(mdev);
	if (ret) {
		dev_info(dev, "Failed to register mem2mem media device\n");
		goto err_unreg_vdev;
	}

	return 0;

err_unreg_vdev:
	v4l2_m2m_unregister_media_controller(fd->m2m_dev);
	video_unregister_device(&fd->vfd);
	video_device_release(&fd->vfd);
err_cleanup_mdev:
	media_device_cleanup(mdev);
	v4l2_m2m_release(fd->m2m_dev);
err_unreg_v4l2_dev:
	v4l2_device_unregister(&fd->v4l2_dev);
	return ret;
}

static void mtk_aie_dev_v4l2_release(struct mtk_aie_dev *fd)
{
	v4l2_m2m_unregister_media_controller(fd->m2m_dev);
	video_unregister_device(&fd->vfd);
	video_device_release(&fd->vfd);
	media_device_cleanup(&fd->mdev);
	v4l2_m2m_release(fd->m2m_dev);
	v4l2_device_unregister(&fd->v4l2_dev);
}
#if CHECK_SERVICE_0
static irqreturn_t mtk_aie_irq(int irq, void *data)
{
	struct mtk_aie_dev *fd = (struct mtk_aie_dev *)data;

	aie_irqhandle(fd);

	queue_work(fd->frame_done_wq, &fd->req_work.work);

	return IRQ_HANDLED;
}
#endif
static void mtk_aie_frame_done_worker(struct work_struct *work)
{
	struct mtk_aie_req_work *req_work = (struct mtk_aie_req_work *)work;
	struct mtk_aie_dev *fd = (struct mtk_aie_dev *)req_work->fd_dev;

	if (fd->reg_cfg.fd_mode == 0) {
		fd->reg_cfg.hw_result = readl(fd->fd_base + AIE_RESULT_0_REG);
		fd->reg_cfg.hw_result1 = readl(fd->fd_base + AIE_RESULT_1_REG);
	}

	if (fd->aie_cfg->sel_mode == 0)
		aie_get_fd_result(fd, fd->aie_cfg);
	else if (fd->aie_cfg->sel_mode == 1)
		aie_get_attr_result(fd, fd->aie_cfg);
	else if (fd->aie_cfg->sel_mode == 3)
		aie_get_fld_result(fd, fd->aie_cfg);

	mtk_aie_hw_done(fd, VB2_BUF_STATE_DONE);
}

static int mtk_aie_probe(struct platform_device *pdev)
{
	struct mtk_aie_dev *fd;
	struct device *dev = &pdev->dev;

	struct resource *res;
	//int irq;
	int ret;
	struct device_node *img_node;
	struct platform_device *img_pdev;

	dev_info(dev, "probe start\n");

	fd = devm_kzalloc(&pdev->dev, sizeof(*fd), GFP_KERNEL);

	memset(fd, 0, sizeof(*fd));
	if (!fd) {
		dev_info(dev, "devm_kzalloc fail!\n");
		return -ENOMEM;
	}
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

	dev_set_drvdata(dev, fd);
	fd->dev = dev;
#if CHECK_SERVICE_0
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_info(dev, "Failed to get irq by platform: %d\n", irq);
		return irq;
	}

	ret = devm_request_irq(dev, irq, mtk_aie_irq, IRQF_SHARED,
			       dev_driver_string(dev), fd);
	if (ret) {
		dev_info(dev, "Failed to request irq\n");
		return ret;
	}
#endif
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	fd->fd_base = devm_ioremap_resource(dev, res);

#ifdef CLK_SINGLE

	if (IS_ERR(fd->fd_base)) {
		dev_info(dev, "Failed to get fd reg base\n");
		return PTR_ERR(fd->fd_base);
	}

	ret = mtk_aie_dev_larb_init(fd);
	if (ret) {
		dev_info(dev, "Failed to init larb : %d\n", ret);
		return ret;
	}

	fd->vcore_gals = devm_clk_get(dev, "VCORE_GALS");
	if (IS_ERR(fd->vcore_gals)) {
		dev_info(dev, "Failed to get vcore_gals clock\n");
		return PTR_ERR(fd->vcore_gals);
	}

	fd->main_gals = devm_clk_get(dev, "MAIN_GALS");
	if (IS_ERR(fd->main_gals)) {
		dev_info(dev, "Failed to get main_gals clock\n");
		return PTR_ERR(fd->main_gals);
	}

	fd->img_ipe = devm_clk_get(dev, "IMG_IPE");
	if (IS_ERR(fd->img_ipe)) {
		dev_info(dev, "Failed to get img_ipe clock\n");
		return PTR_ERR(fd->img_ipe);
	}

	fd->ipe_top = devm_clk_get(dev, "IPE_TOP");
	if (IS_ERR(fd->ipe_top)) {
		dev_info(dev, "Failed to get ipe_top clock\n");
		return PTR_ERR(fd->ipe_top);
	}

	fd->ipe_fdvt = devm_clk_get(dev, "IPE_FDVT");
	if (IS_ERR(fd->ipe_fdvt)) {
		dev_info(dev, "Failed to get ipe_fdvt clock\n");
		return PTR_ERR(fd->ipe_fdvt);
	}
#if CHECK_SERVICE_0
	fd->ipe_fdvt1 = devm_clk_get(dev, "IPE_FDVT1");
	if (IS_ERR(fd->ipe_fdvt1)) {
		dev_info(dev, "Failed to get ipe_fdvt1 clock\n");
		return PTR_ERR(fd->ipe_fdvt1);
	}
#endif
	fd->ipe_smi_larb12 = devm_clk_get(dev, "IPE_SMI_LARB12");
	if (IS_ERR(fd->ipe_smi_larb12)) {
		dev_info(dev, "Failed to get ipe_smi_larb12 clock\n");
		return PTR_ERR(fd->ipe_smi_larb12);
	}

	fd->fdvt_clt = cmdq_mbox_create(dev, 0);
	dev_info(dev, "cmdq fd->fdvt_clt:%x\n", fd->fdvt_clt);
	if (!fd->fdvt_clt)
		dev_info(dev, "cmdq mbox create fail\n");
	else
		dev_info(dev, "cmdq mbox create done\n");
#else
	fd->aie_clk.clk_num = ARRAY_SIZE(ipesys_isp7_aie_clks);
	fd->aie_clk.clks = ipesys_isp7_aie_clks;
	ret = devm_clk_bulk_get(&pdev->dev, fd->aie_clk.clk_num, fd->aie_clk.clks);
	if (ret) {
		dev_info("failed to get raw AIE clock: %d\n", ret);
		return ret;
	}
#endif

	fd->fdvt_secure_clt = cmdq_mbox_create(dev, 1);

	if (!fd->fdvt_secure_clt)
		dev_info(dev, "cmdq mbox create fail\n");
	else
		dev_info(dev, "cmdq mbox create done\n");

	of_property_read_u32(pdev->dev.of_node, "fdvt_frame_done", &(fd->fdvt_event_id));
	dev_info(dev, "fdvt event id is %d\n", fd->fdvt_event_id);

	of_property_read_u32(pdev->dev.of_node, "sw_sync_token_tzmp_aie_wait",
				&(fd->fdvt_sec_wait));
	dev_info(dev, "fdvt_sec_wait is %d\n", fd->fdvt_sec_wait);
	of_property_read_u32(pdev->dev.of_node, "sw_sync_token_tzmp_aie_set",
				&(fd->fdvt_sec_set));
	dev_info(dev, "fdvt_sec_set is %d\n", fd->fdvt_sec_set);

	mutex_init(&fd->vfd_lock);
	init_completion(&fd->fd_job_finished);
	INIT_DELAYED_WORK(&fd->job_timeout_work, mtk_aie_job_timeout_work);
	init_waitqueue_head(&fd->flushing_waitq);
	atomic_set(&fd->num_composing, 0);

	fd->frame_done_wq =
			alloc_ordered_workqueue(dev_name(fd->dev),
						WQ_HIGHPRI | WQ_FREEZABLE);
	if (!fd->frame_done_wq) {
		dev_info(fd->dev, "failed to alloc frame_done workqueue\n");
		mutex_destroy(&fd->vfd_lock);
		return -ENOMEM;
	}

	INIT_WORK(&fd->req_work.work, mtk_aie_frame_done_worker);
	fd->req_work.fd_dev = fd;

	//mtk_aie_mmdvfs_init(fd);
	//mtk_aie_mmqos_init(fd);
	pm_runtime_enable(dev);
	ret = mtk_aie_dev_v4l2_init(fd);
	if (ret) {
		dev_info(dev, "Failed to init v4l2 device: %d\n", ret);
		goto err_destroy_mutex;
	}

	aie_pm_dev = &pdev->dev;
#if IS_ENABLED(CONFIG_PM)
	ret = register_pm_notifier(&aie_notifier_block);
	if (ret) {
		dev_info(dev, "failed to register notifier block.\n");
		return ret;
	}
#endif

	img_node = of_parse_phandle(pdev->dev.of_node, "mediatek,imgsys_fw", 0);
	if (!img_node) {
		dev_info(dev,
			"%s: img node not found\n", __func__);
		return -EINVAL;
	}
	img_pdev = of_find_device_by_node(img_node);
	if (!img_pdev) {
		of_node_put(img_node);
		dev_info(dev,
			"%s: img device not found\n", __func__);
		return -EINVAL;
	}
	of_node_put(img_node);
	fd->img_pdev = img_pdev;
	dev_info(dev, "AIE : Success to %s >W<\n", __func__);

	return 0;

err_destroy_mutex:
	pm_runtime_disable(fd->dev);
	//mtk_aie_mmdvfs_uninit(fd);
	//mtk_aie_mmqos_uninit(fd);
	destroy_workqueue(fd->frame_done_wq);
	mutex_destroy(&fd->vfd_lock);

	return ret;
}

static int mtk_aie_remove(struct platform_device *pdev)
{
	struct mtk_aie_dev *fd = dev_get_drvdata(&pdev->dev);

	mtk_aie_dev_v4l2_release(fd);
	pm_runtime_disable(&pdev->dev);
	//mtk_aie_mmdvfs_uninit(fd);
	//mtk_aie_mmqos_uninit(fd);
	destroy_workqueue(fd->frame_done_wq);
	fd->frame_done_wq = NULL;
	mutex_destroy(&fd->vfd_lock);

	return 0;
}


static int mtk_aie_runtime_suspend(struct device *dev)
{

	dev_info(dev, "%s: runtime suspend aie job)\n", __func__);
	mtk_aie_ccf_disable(dev);
	return 0;
}

static int mtk_aie_runtime_resume(struct device *dev)
{
	dev_info(dev, "%s: empty runtime resume aie job)\n", __func__);
	mtk_aie_ccf_enable(dev);

	return 0;
}

static const struct dev_pm_ops mtk_aie_pm_ops = {
		SET_RUNTIME_PM_OPS(mtk_aie_runtime_suspend,
				   mtk_aie_runtime_resume, NULL)};

static const struct of_device_id mtk_aie_of_ids[] = {
	{
		.compatible = "mediatek,aie-hw3.0",
	},
	{} };
MODULE_DEVICE_TABLE(of, mtk_aie_of_ids);

static struct platform_driver mtk_aie_driver = {
	.probe = mtk_aie_probe,
	.remove = mtk_aie_remove,
	.driver = {
		.name = "mtk-aie-5.3",
		.of_match_table = of_match_ptr(mtk_aie_of_ids),
		.pm = &mtk_aie_pm_ops,
	} };

module_platform_driver(mtk_aie_driver);
MODULE_AUTHOR("Fish Wu <fish.wu@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mediatek AIE driver");
