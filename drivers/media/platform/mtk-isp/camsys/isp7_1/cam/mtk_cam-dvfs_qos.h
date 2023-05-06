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

#define ISP_CLK_LEVEL_CNT 10
#define MTK_CAM_RAW_PORT_NUM 72
#define MTK_CAM_SV_PORT_NUM 16
#define MTK_CAM_MRAW_PORT_NUM 16
#define MAX_CAM_OPP_STEP 10

struct mtk_camsys_dvfs {
	struct device *dev;
	struct regulator *reg_vmm;
	unsigned int clklv_num;
	unsigned int clklv[ISP_CLK_LEVEL_CNT];
	unsigned int voltlv[ISP_CLK_LEVEL_CNT];
	unsigned int clklv_idx;
	unsigned int clklv_target;
	struct clk *mux;
	struct clk *clk_src[MAX_CAM_OPP_STEP];
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

void mtk_cam_dvfs_init(struct mtk_cam_device *cam);
void mtk_cam_dvfs_uninit(struct mtk_cam_device *cam);
void mtk_cam_dvfs_update_clk(struct mtk_cam_device *cam);

void mtk_cam_qos_init(struct mtk_cam_device *cam);
void mtk_cam_qos_bw_reset(struct mtk_cam_ctx *ctx, unsigned int enabled_sv);
void mtk_cam_qos_bw_calc(struct mtk_cam_ctx *ctx, unsigned long raw_dmas, bool force);
#endif
