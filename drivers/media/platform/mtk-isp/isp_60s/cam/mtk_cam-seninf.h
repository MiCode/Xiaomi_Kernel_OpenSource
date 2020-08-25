/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2019 MediaTek Inc. */

#ifndef __MTK_CAM_SENINF_H__
#define __MTK_CAM_SENINF_H__

#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fwnode.h>

#include "mtk_cam-seninf-def.h"

#ifdef V4L2_MBUS_CSI2_IS_USER_DEFINED_DATA
#define SENINF_VC_ROUTING
#endif

struct seninf_mux {
	struct list_head list;
	int idx;
};

struct seninf_cam_mux {
	struct list_head list;
	int idx;
};

struct seninf_vc {
	u8 vc;
	u8 dt;
	u8 feature;
	u8 out_pad;
	u8 pixel_mode;
	u8 group;
	u8 mux; // allocated per group
	u8 cam; // assigned by cam driver
	u8 enable;
	u16 exp_hsize;
	u16 exp_vsize;
};

struct seninf_vcinfo {
	struct seninf_vc vc[SENINF_VC_MAXCNT];
	int cnt;
};

struct seninf_dfs {
	struct device *dev;
	struct regulator *reg;
	unsigned long *freqs;
	unsigned long *volts;
	int cnt;
};

struct seninf_core {
	struct device *dev;
	struct clk *clk[CLK_MAXCNT];
	struct seninf_dfs dfs;
	struct list_head list;
	struct list_head list_mux;
	struct seninf_mux mux[SENINF_MUX_NUM];
#ifdef SENINF_DEBUG
	struct list_head list_cam_mux;
	struct seninf_cam_mux cam_mux[SENINF_CAM_MUX_NUM];
#endif
	struct mutex mutex;
	void __iomem *reg_if;
	void __iomem *reg_ana;
	int refcnt;
};

struct seninf_ctx {
	struct v4l2_subdev subdev;
	struct v4l2_async_notifier notifier;
	struct device *dev;
	struct v4l2_ctrl_handler ctrl_handler;
	struct media_pad pads[PAD_MAXCNT];
	struct v4l2_subdev_format fmt[PAD_MAXCNT];
	struct seninf_core *core;
	struct list_head list;

	int port;
	int portNum;
	int portA;
	int portB;
	int num_data_lanes;
	s64 mipi_pixel_rate;
	s64 buffered_pixel_rate;

	unsigned int is_4d1c:1;
	unsigned int is_cphy:1;
	unsigned int is_test_model:1;
#ifdef SENINF_DEBUG
	unsigned int is_test_streamon:1;
#endif

	int seninfIdx;
	int pad2cam[PAD_MAXCNT];

	/* remote sensor */
	struct v4l2_subdev *sensor_sd;
	int sensor_pad_idx;

	/* provided by sensor */
	struct seninf_vcinfo vcinfo;
	int fps_n;
	int fps_d;

	/* dfs */
	int isp_freq;

	void __iomem *reg_ana_csi_rx[CSI_PORT_MAX_NUM];
	void __iomem *reg_ana_dphy_top[CSI_PORT_MAX_NUM];
	void __iomem *reg_ana_cphy_top[CSI_PORT_MAX_NUM];
	void __iomem *reg_if_top;
	void __iomem *reg_if_ctrl[SENINF_NUM];
	void __iomem *reg_if_cam_mux;
	void __iomem *reg_if_tg[SENINF_NUM];
	void __iomem *reg_if_csi2[SENINF_NUM];
	void __iomem *reg_if_mux[SENINF_MUX_NUM];

	/* resources */
	struct list_head list_mux;
	struct list_head list_cam_mux;

	/* flags */
	unsigned int streaming:1;

};

#endif
