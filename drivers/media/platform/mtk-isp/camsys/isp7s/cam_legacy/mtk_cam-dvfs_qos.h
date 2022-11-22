/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_CAM_DVFS_QOS_H
#define __MTK_CAM_DVFS_QOS_H

#include <linux/clk.h>

struct device;
struct regulator;
struct icc_path;
struct mtk_cam_device;
struct mtk_cam_ctx;
struct mtk_cam_dvfs_tbl;

#define DVFS_QOS_READY
#define ISP_CLK_LEVEL_CNT 10
#define MTK_CAM_RAW_PORT_NUM 57
#define MTK_CAM_SV_PORT_NUM 12
#define MTK_CAM_MRAW_PORT_NUM 12
#define MAX_CAM_OPP_STEP 10

struct mtk_camsys_dvfs {
	struct device *dev;
	unsigned int clklv_num;
	unsigned int clklv[ISP_CLK_LEVEL_CNT];
	unsigned int voltlv[ISP_CLK_LEVEL_CNT];
	unsigned int clklv_idx;
	unsigned int clklv_target;
	atomic_t fixed_clklv;
	struct clk *mmdvfs_clk;
	unsigned long updated_raw_dmas[RAW_NUM];
	struct icc_path *qos_req[MTK_CAM_RAW_PORT_NUM];
	unsigned long qos_bw_avg[MTK_CAM_RAW_PORT_NUM];
	unsigned long qos_bw_peak[MTK_CAM_RAW_PORT_NUM];
	struct icc_path *sv_qos_req[MTK_CAM_SV_PORT_NUM];
	unsigned long sv_qos_bw_avg[MTK_CAM_SV_PORT_NUM];
	unsigned long sv_qos_bw_peak[MTK_CAM_SV_PORT_NUM];
	struct icc_path *mraw_qos_req[MTK_CAM_MRAW_PORT_NUM];
	unsigned long mraw_qos_bw_avg[MTK_CAM_MRAW_PORT_NUM];
	unsigned long mraw_qos_bw_peak[MTK_CAM_MRAW_PORT_NUM];
};

int mtk_cam_dvfs_get_clkidx(struct mtk_cam_device *cam, u64 freq_cur, bool debug);
void mtk_cam_dvfs_init(struct mtk_cam_device *cam);
void mtk_cam_dvfs_uninit(struct mtk_cam_device *cam);
void mtk_cam_dvfs_update_clk(struct mtk_cam_device *cam, bool force_update);
void mtk_cam_dvfs_force_clk(struct mtk_cam_device *cam, bool enable);
void mtk_cam_dvfs_tbl_init(struct mtk_cam_dvfs_tbl *tbl, int opp_num);
void mtk_cam_dvfs_tbl_add_opp(struct mtk_cam_dvfs_tbl *tbl, int opp);
void mtk_cam_dvfs_tbl_del_opp(struct mtk_cam_dvfs_tbl *tbl, int opp);
int mtk_cam_dvfs_tbl_get_opp(struct mtk_cam_dvfs_tbl *tbl);
void mtk_cam_qos_init(struct mtk_cam_device *cam);
void mtk_cam_qos_bw_reset(struct mtk_cam_ctx *ctx);
void mtk_cam_qos_sv_bw_reset(struct mtk_cam_ctx *ctx);
void mtk_cam_qos_bw_calc(struct mtk_cam_ctx *ctx,
	struct mtk_cam_request_stream_data *s_data, bool force);
void mtk_cam_qos_sv_bw_calc(struct mtk_cam_ctx *ctx,
	struct mtk_cam_request_stream_data *s_data, bool force);
void mtk_cam_qos_mraw_bw_calc(struct mtk_cam_ctx *ctx,
	struct mtk_cam_request_stream_data *s_data, bool force);

#endif
