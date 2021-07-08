// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>

#include "mtk-interconnect.h"
#include "mtk_cam.h"
#include "mtk_cam-dvfs_qos.h"
#include "mtk_cam-meta.h"

enum raw_qos_port_id {
	imgo_r1 = 0,
	cqi_r1,
	cqi_r2,
	bpci_r1,
	lsci_r1,
	rawi_r2,/* 5 */
	rawi_r3,
	ufdi_r2,
	ufdi_r3,
	rawi_r4,
	rawi_r5,/* 10 */
	aai_r1,
	fho_r1,
	aao_r1,
	tsfso_r1,
	flko_r1,/* 15 */
	yuvo_r1,
	yuvo_r3,
	yuvco_r1,
	yuvo_r2,
	rzh1n2to_r1,/* 20 */
	drzs4no_r1,
	tncso_r1,
	raw_qos_port_num
};

enum sv_qos_port_id {
	sv_imgo = 0,
	sv_qos_port_num
};

struct raw_mmqos {
	char *port[raw_qos_port_num];
};

struct sv_mmqos {
	char *port[sv_qos_port_num];
};

static struct raw_mmqos raw_qos[] = {
	[RAW_A] = {
		.port = {
			"l16_imgo_r1",
			"l16_cqi_r1",
			"l16_cqi_r2",
			"l16_bpci_r1",
			"l16_lsci_r1",
			"l16_rawi_r2",
			"l16_rawi_r3",
			"l16_ufdi_r2",
			"l16_ufdi_r3",
			"l16_rawi_r4",
			"l16_rawi_r5",
			"l16_aai_r1",
			"l16_fho_r1",
			"l16_aao_r1",
			"l16_tsfso_r1",
			"l16_flko_r1",
			"l17_yuvo_r1",
			"l17_yuvo_r3",
			"l17_yuvco_r1",
			"l17_yuvo_r2",
			"l17_rzh1n2to_r1",
			"l17_drzs4no_r1",
			"l17_tncso_r1"
		},
	},
	[RAW_B] = {
		.port = {
			"l27_imgo_r1",
			"l27_cqi_r1",
			"l27_cqi_r2",
			"l27_bpci_r1",
			"l27_lsci_r1",
			"l27_rawi_r2",
			"l27_rawi_r3",
			"l27_ufdi_r2",
			"l27_ufdi_r3",
			"l27_rawi_r4",
			"l27_rawi_r5",
			"l27_aai_r1",
			"l27_fho_r1",
			"l27_aao_r1",
			"l27_tsfso_r1",
			"l27_flko_r1",
			"l28_yuvo_r1",
			"l28_yuvo_r3",
			"l28_yuvco_r1",
			"l28_yuvo_r2",
			"l28_rzh1n2to_r1",
			"l28_drzs4no_r1",
			"l28_tncso_r1"
		},
	},
};

static struct sv_mmqos sv_qos[] = {
	[CAMSV_0] = {
		.port = {
			"l13_imgo_a0"
		},
	},
	[CAMSV_1] = {
		.port = {
			"l14_imgo_a1"
		},
	},
	[CAMSV_2] = {
		.port = {
			"l13_imgo_b0"
		},
	},
	[CAMSV_3] = {
		.port = {
			"l13_imgo_b1"
		},
	},
	[CAMSV_4] = {
		.port = {
			"l14_imgo_c0"
		},
	},
	[CAMSV_5] = {
		.port = {
			"l14_imgo_c1"
		},
	},
};

static void mtk_cam_dvfs_enumget_clktarget(struct mtk_cam_device *cam)
{
	struct mtk_camsys_dvfs *dvfs = &cam->camsys_ctrl.dvfs_info;
	int clk_streaming_max = dvfs->clklv[0];
	int i;

	for (i = MTKCAM_SUBDEV_RAW_START;  i < MTKCAM_SUBDEV_RAW_END; i++) {
		if (cam->ctxs[i].streaming && cam->ctxs[i].pipe) {
			struct mtk_cam_resource_config *res =
				&cam->ctxs[i].pipe->res_config;
			if (clk_streaming_max < res->clk_target)
				clk_streaming_max = res->clk_target;
			dev_dbg(cam->dev, "on ctx:%d clk needed:%d", i, res->clk_target);
		} else {
			dev_dbg(cam->dev, "on ctx:%d not streaming or pipe null", i);
		}
	}
	dvfs->clklv_target = clk_streaming_max;
	dev_dbg(cam->dev, "[%s] dvfs->clk=%d", __func__, dvfs->clklv_target);
}

static void mtk_cam_dvfs_get_clkidx(struct mtk_cam_device *cam)
{
	struct mtk_camsys_dvfs *dvfs = &cam->camsys_ctrl.dvfs_info;

	u64 freq_cur = 0;
	int i;

	freq_cur = dvfs->clklv_target;
	for (i = 0; i < dvfs->clklv_num; i++) {
		if (freq_cur == dvfs->clklv[i])
			dvfs->clklv_idx = i;
	}
	dev_dbg(cam->dev, "[%s] get clk=%d, idx=%d",
		 __func__, dvfs->clklv_target, dvfs->clklv_idx);
}

void mtk_cam_dvfs_update_clk(struct mtk_cam_device *cam)
{
	struct mtk_camsys_dvfs *dvfs = &cam->camsys_ctrl.dvfs_info;

	mtk_cam_dvfs_enumget_clktarget(cam);
	mtk_cam_dvfs_get_clkidx(cam);
	dev_dbg(cam->dev, "[%s] update idx:%d clk:%d volt:%d", __func__,
		dvfs->clklv_idx, dvfs->clklv_target, dvfs->voltlv[dvfs->clklv_idx]);
	regulator_set_voltage(dvfs->reg, dvfs->voltlv[dvfs->clklv_idx], INT_MAX);
}

void mtk_cam_dvfs_uninit(struct mtk_cam_device *cam)
{
	struct mtk_camsys_dvfs *dvfs_info = &cam->camsys_ctrl.dvfs_info;

	if (dvfs_info->clklv_num)
		dev_pm_opp_of_remove_table(dvfs_info->dev);
	dev_info(cam->dev, "[%s]\n", __func__);
}

void mtk_cam_dvfs_init(struct mtk_cam_device *cam)
{
	struct mtk_camsys_dvfs *dvfs_info = &cam->camsys_ctrl.dvfs_info;
	struct dev_pm_opp *opp;
	unsigned long freq = 0;
	int ret = 0, clk_num = 0, i = 0;

	memset((void *)dvfs_info, 0x0, sizeof(struct mtk_camsys_dvfs));
	dvfs_info->dev = cam->dev;
	ret = dev_pm_opp_of_add_table(dvfs_info->dev);
	if (ret < 0) {
		dev_dbg(dvfs_info->dev, "fail to init opp table: %d\n", ret);
		return;
	}
	dvfs_info->reg = devm_regulator_get_optional(dvfs_info->dev, "dvfsrc-vcore");
	if (IS_ERR(dvfs_info->reg)) {
		dev_dbg(dvfs_info->dev, "can't get dvfsrc-vcore\n");
		return;
	}
	clk_num = dev_pm_opp_get_opp_count(dvfs_info->dev);
	while (!IS_ERR(opp = dev_pm_opp_find_freq_ceil(dvfs_info->dev, &freq))) {
		dvfs_info->clklv[i] = freq;
		dvfs_info->voltlv[i] = dev_pm_opp_get_voltage(opp);
		freq++;
		i++;
		dev_pm_opp_put(opp);
	}
	dvfs_info->clklv_num = clk_num;
	dvfs_info->clklv_target = dvfs_info->clklv[0];
	dvfs_info->clklv_idx = 0;
	for (i = 0; i < dvfs_info->clklv_num; i++) {
		dev_info(cam->dev, "[%s] idx=%d, clk=%d volt=%d\n",
			 __func__, i, dvfs_info->clklv[i], dvfs_info->voltlv[i]);
	}
	mtk_cam_qos_init(cam);
}

#define MTK_CAM_QOS_LSCI_TABLE_MAX_SIZE (32768)
#define MTK_CAM_QOS_CACI_TABLE_MAX_SIZE (32768)
#define BW_B2KB_WITH_RATIO(value) ((value) * 4 / 3 / 1024)

void mtk_cam_qos_bw_calc(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_camsys_dvfs *dvfs_info = &cam->camsys_ctrl.dvfs_info;
	struct mtk_cam_video_device *vdev;
	struct mtk_raw_pipeline *pipe = ctx->pipe;
	struct mtk_raw_device *raw_dev = get_master_raw_dev(cam, pipe);
	struct v4l2_subdev_frame_interval fi;
	struct v4l2_subdev_format sd_fmt;
	struct v4l2_ctrl *ctrl;
	struct raw_mmqos *raw_mmqos;
	struct sv_mmqos *sv_mmqos;
	unsigned int ipi_video_id;
	int engine_id = raw_dev->id;
	unsigned int qos_port_id;
	unsigned int ipi_fmt;
	int i, pixel_bits, num_plane;
	unsigned long vblank, fps, height, BW_MB_s;

	raw_mmqos = raw_qos + engine_id;
	dev_info(cam->dev, "[%s] engine_id(%d) enable_dmas(0x%lx)\n",
			 __func__, engine_id, pipe->enabled_dmas);
	if (ctx->sensor) {
		fi.pad = 1;
		fi.reserved[0] = V4L2_SUBDEV_FORMAT_ACTIVE;
		v4l2_subdev_call(ctx->sensor, video, g_frame_interval, &fi);
		fps = fi.interval.denominator / fi.interval.numerator;
		pipe->res_config.interval.denominator = fi.interval.denominator;
		pipe->res_config.interval.numerator = fi.interval.numerator;
		ctrl = v4l2_ctrl_find(ctx->sensor->ctrl_handler, V4L2_CID_VBLANK);
		vblank = v4l2_ctrl_g_ctrl(ctrl);
		sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
		sd_fmt.pad = PAD_SRC_RAW0;
		v4l2_subdev_call(ctx->seninf, pad, get_fmt, NULL, &sd_fmt);
		height = sd_fmt.format.height;
		dev_info(cam->dev, "[%s] FPS:%lu/%lu:%lu, H:%lu, VB:%lu\n",
			 __func__, fi.interval.denominator, fi.interval.numerator,
			 fps, height, vblank);
	}
	for (i = 0; i < MTKCAM_IPI_RAW_ID_MAX; i++) {
		if (pipe->enabled_dmas & 1<<i)
			ipi_video_id = i;
		else
			continue;
		/* update buffer internal address */
		switch (ipi_video_id) {
		case MTKCAM_IPI_RAW_IMGO:
			qos_port_id = engine_id * raw_qos_port_num + imgo_r1;
			vdev = &pipe->vdev_nodes[MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM];
			ipi_fmt = mtk_cam_get_img_fmt(vdev->active_fmt.fmt.pix_mp.pixelformat);
			pixel_bits = mtk_cam_get_pixel_bits(ipi_fmt);
			num_plane = mtk_cam_get_plane_num(ipi_fmt);
			BW_MB_s = vdev->active_fmt.fmt.pix_mp.width * fps *
						(vblank + height) * pixel_bits * num_plane / 8;
			dvfs_info->qos_bw_avg[qos_port_id] += BW_MB_s;
			dev_info(cam->dev, "[%16s] qos_idx:%2d ipifmt/bits/plane/w : %2d/%2d/%d/%5d BW(B/s):%lu\n",
				raw_mmqos->port[qos_port_id % raw_qos_port_num],
				qos_port_id, ipi_fmt,
				pixel_bits, num_plane,
				vdev->active_fmt.fmt.pix_mp.width,
				BW_MB_s);
			break;
		case MTKCAM_IPI_RAW_YUVO_1:
			qos_port_id = engine_id * raw_qos_port_num + yuvo_r1;
			vdev = &pipe->vdev_nodes[MTK_RAW_YUVO_1_OUT - MTK_RAW_SINK_NUM];
			ipi_fmt = mtk_cam_get_img_fmt(vdev->active_fmt.fmt.pix_mp.pixelformat);
			pixel_bits = mtk_cam_get_pixel_bits(ipi_fmt);
			num_plane = mtk_cam_get_plane_num(ipi_fmt);
			BW_MB_s = vdev->active_fmt.fmt.pix_mp.width * fps *
						(vblank + height) * pixel_bits * num_plane / 8;
			dvfs_info->qos_bw_avg[qos_port_id] += BW_MB_s;
			dev_info(cam->dev, "[%16s] qos_idx:%2d ipifmt/bits/plane/w : %2d/%2d/%d/%5d BW(B/s):%lu\n",
				raw_mmqos->port[qos_port_id % raw_qos_port_num],
				qos_port_id, ipi_fmt,
				pixel_bits, num_plane,
				vdev->active_fmt.fmt.pix_mp.width,
				BW_MB_s);
			break;
		case MTKCAM_IPI_RAW_YUVO_3:
			qos_port_id = engine_id * raw_qos_port_num + yuvo_r3;
			vdev = &pipe->vdev_nodes[MTK_RAW_YUVO_3_OUT - MTK_RAW_SINK_NUM];
			ipi_fmt = mtk_cam_get_img_fmt(vdev->active_fmt.fmt.pix_mp.pixelformat);
			pixel_bits = mtk_cam_get_pixel_bits(ipi_fmt);
			num_plane = mtk_cam_get_plane_num(ipi_fmt);
			BW_MB_s = vdev->active_fmt.fmt.pix_mp.width * fps *
						(vblank + height) * pixel_bits * num_plane / 8;
			dvfs_info->qos_bw_avg[qos_port_id] += BW_MB_s;
			dev_info(cam->dev, "[%16s] qos_idx:%2d ipifmt/bits/plane/w : %2d/%2d/%d/%5d BW(B/s):%lu\n",
				raw_mmqos->port[qos_port_id % raw_qos_port_num],
				qos_port_id, ipi_fmt,
				pixel_bits, num_plane,
				vdev->active_fmt.fmt.pix_mp.width,
				BW_MB_s);
			break;
		case MTKCAM_IPI_RAW_YUVO_2:
		case MTKCAM_IPI_RAW_YUVO_4:
		case MTKCAM_IPI_RAW_YUVO_5:
			qos_port_id = engine_id * raw_qos_port_num + yuvo_r2;
			if (ipi_video_id == MTKCAM_IPI_RAW_YUVO_2)
				vdev = &pipe->vdev_nodes[MTK_RAW_YUVO_2_OUT - MTK_RAW_SINK_NUM];
			else if (ipi_video_id == MTKCAM_IPI_RAW_YUVO_4)
				vdev = &pipe->vdev_nodes[MTK_RAW_YUVO_4_OUT - MTK_RAW_SINK_NUM];
			else if (ipi_video_id == MTKCAM_IPI_RAW_YUVO_5)
				vdev = &pipe->vdev_nodes[MTK_RAW_YUVO_5_OUT - MTK_RAW_SINK_NUM];
			ipi_fmt = mtk_cam_get_img_fmt(vdev->active_fmt.fmt.pix_mp.pixelformat);
			pixel_bits = mtk_cam_get_pixel_bits(ipi_fmt);
			num_plane = mtk_cam_get_plane_num(ipi_fmt);
			BW_MB_s = vdev->active_fmt.fmt.pix_mp.width * fps *
						(vblank + height) * pixel_bits * num_plane / 8;
			dvfs_info->qos_bw_avg[qos_port_id] += BW_MB_s;
			dev_info(cam->dev, "[%16s] qos_idx:%2d ipifmt/bits/plane/w : %2d/%2d/%d/%5d BW(B/s):%lu\n",
				raw_mmqos->port[qos_port_id % raw_qos_port_num],
				qos_port_id, ipi_fmt,
				pixel_bits, num_plane,
				vdev->active_fmt.fmt.pix_mp.width,
				BW_MB_s);
			break;
		case MTKCAM_IPI_RAW_RZH1N2TO_1:
		case MTKCAM_IPI_RAW_RZH1N2TO_2:
		case MTKCAM_IPI_RAW_RZH1N2TO_3:
			qos_port_id = engine_id * raw_qos_port_num + rzh1n2to_r1;
			if (ipi_video_id == MTKCAM_IPI_RAW_RZH1N2TO_1)
				vdev = &pipe->vdev_nodes[MTK_RAW_RZH1N2TO_1_OUT - MTK_RAW_SINK_NUM];
			else if (ipi_video_id == MTKCAM_IPI_RAW_RZH1N2TO_2)
				vdev = &pipe->vdev_nodes[MTK_RAW_RZH1N2TO_2_OUT - MTK_RAW_SINK_NUM];
			else if (ipi_video_id == MTKCAM_IPI_RAW_RZH1N2TO_3)
				vdev = &pipe->vdev_nodes[MTK_RAW_RZH1N2TO_3_OUT - MTK_RAW_SINK_NUM];
			ipi_fmt = mtk_cam_get_img_fmt(vdev->active_fmt.fmt.pix_mp.pixelformat);
			pixel_bits = mtk_cam_get_pixel_bits(ipi_fmt);
			num_plane = mtk_cam_get_plane_num(ipi_fmt);
			BW_MB_s = vdev->active_fmt.fmt.pix_mp.width * fps *
						(vblank + height) * pixel_bits * num_plane / 8;
			dvfs_info->qos_bw_avg[qos_port_id] += BW_MB_s;
			dev_info(cam->dev, "[%16s] qos_idx:%2d ipifmt/bits/plane/w : %2d/%2d/%d/%5d BW(B/s):%lu\n",
				raw_mmqos->port[qos_port_id % raw_qos_port_num],
				qos_port_id, ipi_fmt,
				pixel_bits, num_plane,
				vdev->active_fmt.fmt.pix_mp.width,
				BW_MB_s);
			break;
		case MTKCAM_IPI_RAW_DRZS4NO_1:
		case MTKCAM_IPI_RAW_DRZS4NO_2:
		case MTKCAM_IPI_RAW_DRZS4NO_3:
			qos_port_id = engine_id * raw_qos_port_num + drzs4no_r1;
			if (ipi_video_id == MTKCAM_IPI_RAW_DRZS4NO_1)
				vdev = &pipe->vdev_nodes[MTK_RAW_DRZS4NO_1_OUT - MTK_RAW_SINK_NUM];
			else if (ipi_video_id == MTKCAM_IPI_RAW_DRZS4NO_2)
				vdev = &pipe->vdev_nodes[MTK_RAW_DRZS4NO_2_OUT - MTK_RAW_SINK_NUM];
			else if (ipi_video_id == MTKCAM_IPI_RAW_DRZS4NO_3)
				vdev = &pipe->vdev_nodes[MTK_RAW_DRZS4NO_3_OUT - MTK_RAW_SINK_NUM];
			ipi_fmt = mtk_cam_get_img_fmt(vdev->active_fmt.fmt.pix_mp.pixelformat);
			pixel_bits = mtk_cam_get_pixel_bits(ipi_fmt);
			num_plane = mtk_cam_get_plane_num(ipi_fmt);
			BW_MB_s = vdev->active_fmt.fmt.pix_mp.width * fps *
						(vblank + height) * pixel_bits * num_plane / 8;
			dvfs_info->qos_bw_avg[qos_port_id] += BW_MB_s;
			dev_info(cam->dev, "[%16s] qos_idx:%2d ipifmt/bits/plane/w : %2d/%2d/%d/%5d BW(B/s):%lu\n",
				raw_mmqos->port[qos_port_id % raw_qos_port_num],
				qos_port_id, ipi_fmt,
				pixel_bits, num_plane,
				vdev->active_fmt.fmt.pix_mp.width,
				BW_MB_s);
			break;
		case MTKCAM_IPI_RAW_META_STATS_CFG:
			/* cq_r1 = main+sub cq descriptor size */
			qos_port_id = engine_id * raw_qos_port_num + cqi_r1;
			BW_MB_s = CQ_BUF_SIZE * fps / 10;
			dvfs_info->qos_bw_avg[qos_port_id] += BW_MB_s;
			dev_info(cam->dev, "[%16s] qos_idx:%2d BW(B/s):%lu\n",
				raw_mmqos->port[qos_port_id % raw_qos_port_num],
				qos_port_id, BW_MB_s);
			/* cq_r2 = main+sub cq virtual address size */
			qos_port_id = engine_id * raw_qos_port_num + cqi_r2;
			BW_MB_s = CQ_BUF_SIZE * fps * 9 / 10;
			dvfs_info->qos_bw_avg[qos_port_id] += BW_MB_s;
			dev_info(cam->dev, "[%16s] qos_idx:%2d BW(B/s):%lu\n",
				raw_mmqos->port[qos_port_id % raw_qos_port_num],
				qos_port_id, BW_MB_s);
			/* lsci_r1 = lsci */
			// lsci = MTK_CAM_QOS_LSCI_TABLE_MAX_SIZE
			qos_port_id = engine_id * raw_qos_port_num + lsci_r1;
			BW_MB_s = MTK_CAM_QOS_LSCI_TABLE_MAX_SIZE * fps;
			dvfs_info->qos_bw_avg[qos_port_id] += BW_MB_s;
			dev_info(cam->dev, "[%16s] qos_idx:%2d BW(B/s):%lu\n",
				raw_mmqos->port[qos_port_id % raw_qos_port_num],
				qos_port_id, BW_MB_s);
			/* fho_r1 = aaho + fho + pdo */
			// aaho = MTK_CAM_UAPI_AAHO_HIST_SIZE
			// fho = almost zero
			// pdo = implement later
			qos_port_id = engine_id * raw_qos_port_num + fho_r1;
			BW_MB_s = MTK_CAM_UAPI_AAHO_HIST_SIZE * fps;
			dvfs_info->qos_bw_avg[qos_port_id] += BW_MB_s;
			dev_info(cam->dev, "[%16s] qos_idx:%2d BW(B/s):%lu\n",
				raw_mmqos->port[qos_port_id % raw_qos_port_num],
				qos_port_id, BW_MB_s);
			/* aao_r1 = aao + afo */
			// aao = MTK_CAM_UAPI_AAO_MAX_BUF_SIZE (twin = /2)
			// afo = MTK_CAM_UAPI_AFO_MAX_BUF_SIZE (twin = /2)
			qos_port_id = engine_id * raw_qos_port_num + aao_r1;
			BW_MB_s = (MTK_CAM_UAPI_AAO_MAX_BUF_SIZE + MTK_CAM_UAPI_AFO_MAX_BUF_SIZE)
							* fps;
			dvfs_info->qos_bw_avg[qos_port_id] += BW_MB_s;
			dev_info(cam->dev, "[%16s] qos_idx:%2d BW(B/s):%lu\n",
				raw_mmqos->port[qos_port_id % raw_qos_port_num],
				qos_port_id, BW_MB_s);
			/* tsfso_r1 = tsfso + tsfso + ltmso */
			// tsfso x 2 = MTK_CAM_UAPI_TSFSO_SIZE * 2
			// ltmso = MTK_CAM_UAPI_LTMSO_SIZE
			qos_port_id = engine_id * raw_qos_port_num + tsfso_r1;
			BW_MB_s = (MTK_CAM_UAPI_TSFSO_SIZE * 2 + MTK_CAM_UAPI_LTMSO_SIZE) * fps;
			dvfs_info->qos_bw_avg[qos_port_id] += BW_MB_s;
			dev_info(cam->dev, "[%16s] qos_idx:%2d BW(B/s):%lu\n",
				raw_mmqos->port[qos_port_id % raw_qos_port_num],
				qos_port_id, BW_MB_s);
			/* flko_r1 = flko + ufeo + bpco */
			// flko = MTK_CAM_UAPI_FLK_BLK_SIZE * MTK_CAM_UAPI_FLK_MAX_STAT_BLK_NUM
			//						* sensor height
			// ufeo = implement later
			// bpco = implement later
			qos_port_id = engine_id * raw_qos_port_num + flko_r1;
			BW_MB_s = MTK_CAM_UAPI_FLK_BLK_SIZE *  MTK_CAM_UAPI_FLK_MAX_STAT_BLK_NUM
							* height * fps;
			dvfs_info->qos_bw_avg[qos_port_id] += BW_MB_s;
			dev_info(cam->dev, "[%16s] qos_idx:%2d BW(B/s):%lu\n",
				raw_mmqos->port[qos_port_id % raw_qos_port_num],
				qos_port_id, BW_MB_s);
			break;
		case MTKCAM_IPI_RAW_META_STATS_0:
			break;
		case MTKCAM_IPI_RAW_META_STATS_1:
			break;
		case MTKCAM_IPI_RAW_META_STATS_2:
			break;
		default:
			break;
		}
	}
	if (mtk_cam_is_stagger(ctx)) {
		qos_port_id = engine_id * raw_qos_port_num + rawi_r2;
		vdev = &pipe->vdev_nodes[MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM];
		ipi_fmt = mtk_cam_get_img_fmt(vdev->active_fmt.fmt.pix_mp.pixelformat);
		pixel_bits = mtk_cam_get_pixel_bits(ipi_fmt);
		num_plane = mtk_cam_get_plane_num(ipi_fmt);
		BW_MB_s = vdev->active_fmt.fmt.pix_mp.width * fps *
					(vblank + height) * pixel_bits * num_plane / 8;
		dvfs_info->qos_bw_avg[qos_port_id] += BW_MB_s;
		dev_info(cam->dev, "[%16s] qos_idx:%2d ipifmt/bits/plane/w : %2d/%2d/%d/%5d BW(B/s):%lu\n",
			raw_mmqos->port[qos_port_id % raw_qos_port_num], qos_port_id, ipi_fmt,
			pixel_bits, num_plane, vdev->active_fmt.fmt.pix_mp.width, BW_MB_s);
		qos_port_id = engine_id * raw_qos_port_num + rawi_r3;
		dvfs_info->qos_bw_avg[qos_port_id] += BW_MB_s;
		dev_info(cam->dev, "[%16s] qos_idx:%2d ipifmt/bits/plane/w : %2d/%2d/%d/%5d BW(B/s):%lu\n",
			raw_mmqos->port[qos_port_id % raw_qos_port_num], qos_port_id, ipi_fmt,
			pixel_bits, num_plane, vdev->active_fmt.fmt.pix_mp.width, BW_MB_s);
		qos_port_id = engine_id * raw_qos_port_num + rawi_r5;
		dvfs_info->qos_bw_avg[qos_port_id] += BW_MB_s;
		dev_info(cam->dev, "[%16s] qos_idx:%2d ipifmt/bits/plane/w : %2d/%2d/%d/%5d BW(B/s):%lu\n",
			raw_mmqos->port[qos_port_id % raw_qos_port_num], qos_port_id, ipi_fmt,
			pixel_bits, num_plane, vdev->active_fmt.fmt.pix_mp.width, BW_MB_s);
		qos_port_id = sv_imgo;
		dvfs_info->sv_qos_bw_avg[qos_port_id] += BW_MB_s;
		dev_info(cam->dev, "[%16s] qos_idx:%2d ipifmt/bits/plane/w : %2d/%2d/%d/%5d BW(B/s):%lu\n",
			sv_qos[0].port[qos_port_id % sv_qos_port_num], qos_port_id, ipi_fmt,
			pixel_bits, num_plane, vdev->active_fmt.fmt.pix_mp.width, BW_MB_s);
		if (mtk_cam_is_3_exposure(ctx)) {
			qos_port_id = sv_qos_port_num + sv_imgo;
			dvfs_info->sv_qos_bw_avg[qos_port_id] += BW_MB_s;
			dev_info(cam->dev, "[%16s] qos_idx:%2d ipifmt/bits/plane/w : %2d/%2d/%d/%5d BW(B/s):%lu\n",
				sv_qos[1].port[qos_port_id % sv_qos_port_num], qos_port_id, ipi_fmt,
				pixel_bits, num_plane, vdev->active_fmt.fmt.pix_mp.width, BW_MB_s);
		}
	}
	for (i = 0; i < ctx->used_sv_num; i++) {
		qos_port_id = ((ctx->sv_pipe[i]->id - MTKCAM_SUBDEV_CAMSV_START)
						* sv_qos_port_num)
						+ sv_imgo;
		sv_mmqos = sv_qos + ctx->sv_pipe[i]->id - MTKCAM_SUBDEV_CAMSV_START;
		vdev = &ctx->sv_pipe[i]->vdev_nodes[MTK_CAMSV_MAIN_STREAM_OUT - MTK_CAMSV_SINK_NUM];
		ipi_fmt = mtk_cam_get_img_fmt(vdev->active_fmt.fmt.pix_mp.pixelformat);
		pixel_bits = mtk_cam_get_pixel_bits(ipi_fmt);
		num_plane = mtk_cam_get_plane_num(ipi_fmt);
		BW_MB_s = vdev->active_fmt.fmt.pix_mp.width * fps *
					(vblank + height) * pixel_bits * num_plane / 8;
		dvfs_info->sv_qos_bw_avg[qos_port_id] += BW_MB_s;
		dev_info(cam->dev, "[%16s] qos_idx:%2d ipifmt/bits/plane/w : %2d/%2d/%d/%5d BW(B/s):%lu\n",
			sv_mmqos->port[qos_port_id % sv_qos_port_num], qos_port_id, ipi_fmt,
			pixel_bits, num_plane, vdev->active_fmt.fmt.pix_mp.width, BW_MB_s);
	}
	for (i = 0; i < MTK_CAM_RAW_PORT_NUM; i++) {
		if (dvfs_info->qos_bw_avg[i] != 0) {
			dev_info(cam->dev, "[%s] port idx/name:%2d/%16s BW(kB/s):%lu\n",
				__func__, i, raw_mmqos->port[i % raw_qos_port_num],
				BW_B2KB_WITH_RATIO(dvfs_info->qos_bw_avg[i]));
			mtk_icc_set_bw(dvfs_info->qos_req[i],
				kBps_to_icc(BW_B2KB_WITH_RATIO(dvfs_info->qos_bw_avg[i])), 0);
		}
	}
	for (i = 0; i < MTK_CAM_SV_PORT_NUM; i++) {
		if (dvfs_info->sv_qos_bw_avg[i] != 0) {
			dev_info(cam->dev, "[%s] port idx:%2d BW(kB/s):%lu\n",
				__func__, i,
				BW_B2KB_WITH_RATIO(dvfs_info->sv_qos_bw_avg[i]));
			mtk_icc_set_bw(dvfs_info->sv_qos_req[i],
				kBps_to_icc(BW_B2KB_WITH_RATIO(dvfs_info->sv_qos_bw_avg[i])), 0);
		}
	}
}

void mtk_cam_qos_init(struct mtk_cam_device *cam)
{
	struct mtk_camsys_dvfs *dvfs_info = &cam->camsys_ctrl.dvfs_info;
	struct raw_mmqos *raw_mmqos;
	struct sv_mmqos *sv_mmqos;
	struct mtk_raw_device *raw_dev;
	struct mtk_yuv_device *yuv_dev;
	struct mtk_camsv_device *sv_dev;
	struct device *engineraw_dev;
	struct device *engineyuv_dev;
	struct device *enginesv_dev;
	int i, port_id, engine_id;

	for (i = 0; i < MTK_CAM_RAW_PORT_NUM; i++) {
		dvfs_info->qos_req[i] = 0;
		dvfs_info->qos_bw_avg[i] = 0;
	}
	for (i = 0; i < MTK_CAM_SV_PORT_NUM; i++) {
		dvfs_info->sv_qos_req[i] = 0;
		dvfs_info->sv_qos_bw_avg[i] = 0;
	}
	i = 0;
	for (engine_id = RAW_A; engine_id <= RAW_B; engine_id++) {
		raw_mmqos = raw_qos + engine_id;
		engineraw_dev = cam->raw.devs[engine_id];
		engineyuv_dev = cam->raw.yuvs[engine_id];
		for (port_id = 0; port_id < raw_qos_port_num; port_id++) {
			if (port_id < yuvo_r1) {
				raw_dev = dev_get_drvdata(engineraw_dev);
				dvfs_info->qos_req[i] =
					of_mtk_icc_get(raw_dev->dev, raw_mmqos->port[port_id]);
			} else {
				yuv_dev = dev_get_drvdata(engineyuv_dev);
				dvfs_info->qos_req[i] =
					of_mtk_icc_get(yuv_dev->dev, raw_mmqos->port[port_id]);
			}
			i++;
		}
		dev_info(raw_dev->dev, "[%s] raw_engine_id=%d, port_num=%d\n",
			 __func__, engine_id, i);
	}
	i = 0;
	for (engine_id = CAMSV_0; engine_id <= CAMSV_5; engine_id++) {
		sv_mmqos = sv_qos + engine_id;
		enginesv_dev = cam->sv.devs[engine_id];
		for (port_id = 0; port_id < sv_qos_port_num; port_id++) {
			sv_dev = dev_get_drvdata(enginesv_dev);
			dvfs_info->sv_qos_req[i] =
				of_mtk_icc_get(sv_dev->dev, sv_mmqos->port[port_id]);
			i++;
		}
		dev_info(sv_dev->dev, "[%s] sv_engine_id=%d, port_num=%d\n",
			 __func__, engine_id, i);
	}
}

void mtk_cam_qos_bw_reset(struct mtk_cam_device *cam)
{
	struct mtk_camsys_dvfs *dvfs_info = &cam->camsys_ctrl.dvfs_info;
	int i;

	for (i = 0; i < MTK_CAM_RAW_PORT_NUM; i++) {
		dvfs_info->qos_bw_avg[i] = 0;
		mtk_icc_set_bw(dvfs_info->qos_req[i], 0, 0);
	}
	for (i = 0; i < MTK_CAM_SV_PORT_NUM; i++) {
		dvfs_info->sv_qos_bw_avg[i] = 0;
		mtk_icc_set_bw(dvfs_info->sv_qos_req[i], 0, 0);
	}
	dev_info(cam->dev, "[%s]\n", __func__);
}

