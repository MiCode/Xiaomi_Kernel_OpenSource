/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_AOV_H
#define __MTK_CAM_AOV_H

#define MAX_DEST_NUM 4

struct seninf_vc_out_dest {
	u8 mux; // allocated per group
	u8 mux_vr; // allocated per group
	u8 cam; // assigned by cam driver
	u8 tag; // assigned by cam driver
	u8 cam_type; // assigned by cam driver
};

struct seninf_vc {
	u8 vc;
	u8 dt;
	u8 feature;
	u8 out_pad;
	u8 pixel_mode;
	u8 group;
	u8 dest_cnt;
	struct seninf_vc_out_dest dest[MAX_DEST_NUM];
	u8 enable;
	u16 exp_hsize;
	u16 exp_vsize;
	u8 bit_depth;
	u8 dt_remap_to_type;
};

/* camsys supported parameter */
struct mtk_seninf_aov_param {
	struct seninf_vc vc;
	unsigned int sensor_idx;
	unsigned int is_cphy:1;
	int num_data_lanes;
	s64 mipi_pixel_rate;
	s64 width;
	s64 height;
	s64 hblank;
	s64 vblank;
	int fps_n;
	int fps_d;
	s64 customized_pixel_rate;
	int port;
	int portA;
	int portB;
	unsigned int is_4d1c:1;
	int seninfIdx;
	u32 cphy_settle;
	u32 dphy_clk_settle;
	u32 dphy_data_settle;
	u32 dphy_trail;
	u8 legacy_phy;
	u8 not_fixed_trail_settle;
	int cnt;
	int is_test_model;
	int isp_freq;
	int camtg;
	int seninf_dphy_settle_delay_dt;
	int cphy_settle_delay_dt;
	int dphy_settle_delay_dt;
	int settle_delay_ck;
	int hs_trail_parameter;
	u32 dphy_csi2_resync_dmy_cycle;
	u8 not_fixed_dphy_settle;
};

enum AOV_DEINIT_TYPE {
	DEINIT_NORMAL = 0,
	DEINIT_ABNORMAL_USR_FD_KILL,
	DEINIT_ABNORMAL_SCP_STOP,
};

enum AOV_INIT_TYPE {
	INIT_NORMAL = 0,
	INIT_ABNORMAL_SCP_READY,
};

extern int mtk_cam_seninf_s_aov_param(unsigned int sensor_id,
	struct mtk_seninf_aov_param *aov_seninf_param,
	enum AOV_INIT_TYPE aov_seninf_init_type);

extern int mtk_cam_seninf_aov_runtime_suspend(unsigned int sensor_id);

extern int mtk_cam_seninf_aov_runtime_resume(unsigned int sensor_id,
	enum AOV_DEINIT_TYPE aov_seninf_deinit_type);

#endif /* __MTK_CAM_AOV_H */
