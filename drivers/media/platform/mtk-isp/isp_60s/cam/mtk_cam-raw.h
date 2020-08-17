/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_RAW_H
#define __MTK_CAM_RAW_H

#include "mtk_cam-video.h"

#define RAW_PIPELINE_NUM 3
#define SCQ_DEADLINE_MS  15 // ~1/2 frame length
#define SCQ_DEFAULT_CLK_RATE 208 // default 208MHz
#define USINGSCQ 1
#define MTK_CAMSYS_RES_STEP_NUM	7

/* FIXME: dynamic config image max/min w/h */
#define IMG_MAX_WIDTH		5376
#define IMG_MAX_HEIGHT		4032
#define IMG_MIN_WIDTH		80
#define IMG_MIN_HEIGHT		60

enum raw_module_id {
	RAW_A = 0,
	RAW_B = 1,
	RAW_C = 2,
};

/* enum for pads of raw pipeline */
enum {
	MTK_RAW_SINK_BEGIN = 0,
	MTK_RAW_SINK = MTK_RAW_SINK_BEGIN,
	MTK_RAW_SINK_NUM,
	MTK_RAW_META_IN = MTK_RAW_SINK_NUM,
	MTK_RAW_RAWI_2_IN,
	MTK_RAW_UFDI_2_IN,
	MTK_RAW_RAWI_3_IN,
	MTK_RAW_SOURCE_BEGIN,
	MTK_RAW_MAIN_STREAM_OUT = MTK_RAW_SOURCE_BEGIN,
	MTK_RAW_UFEO_OUT,
	MTK_RAW_PACKED_BIN_OUT,
	MTK_RAW_UFGO_OUT,
	MTK_RAW_YUVO_OUT,
	MTK_RAW_YUVBO_OUT,
	MTK_RAW_YUVCO_OUT,
	MTK_RAW_RSSO_1_OUT,
	MTK_RAW_RSSO_2_OUT,
	MTK_RAW_CRZO_1_OUT,
	MTK_RAW_CRZO_2_OUT,
	MTK_RAW_CRZBO_OUT,
	MTK_RAW_META_OUT_BEGIN,
	MTK_RAW_META_OUT_0 = MTK_RAW_META_OUT_BEGIN,
	MTK_RAW_META_OUT_1,
	MTK_RAW_META_OUT_2,
	MTK_RAW_PIPELINE_PADS_NUM,
};

#define MTK_RAW_TOTAL_NODES (MTK_RAW_PIPELINE_PADS_NUM - MTK_RAW_SINK_NUM)

struct mtk_cam_dev;
struct mtk_cam_ctx;

/**
 * TODO raw_hw_ops
 * struct raw_device;
 * struct raw_hw_ops {
 *	TODO: may add arguments to select raw
 *	int (*get)(struct raw_device *raw, struct mtk_cam_ctx *ctx);
 *	void (*put)(struct raw_device *raw, struct mtk_cam_ctx *ctx);
 *
 *	int (*enable_clk)(struct raw_device *raw, struct mtk_cam_ctx *ctx,
 *			  raw_resource res);
 *	int (*disable_clk)(struct raw_device *raw, struct mtk_cam_ctx *ctx,
 *			   raw_resource res);
 *
 *	void (*reset)(struct raw_device *raw, u8 flag);
 *	irqreturn_t (*isr)(int irq, void *rawdev);
 * };
 */

/**
 * TODO: raw_isr_ops
 * struct raw_isr_ops {
 *	void (*apply)(struct raw_device *raw, enum raw_module_id module_id);
 *	void (*sof)(struct raw_device *raw, enum raw_module_id module_id);
 *	void (*frame_done)(struct raw_device *raw);
 * };
 */
struct mtk_cam_resource_config {
	struct v4l2_subdev *seninf;
	struct mutex resource_lock;
	s64 pixel_rate;
	u32 bin_limit;
	u32 frz_limit;
	u32 hwn_limit;
	u32 res_plan;
	u32 res_strategy[MTK_CAMSYS_RES_STEP_NUM];
	u32 clk_target;
	u32 raw_num_used;
	u32 bin_enable;
	u32 frz_enable;
	u32 frz_ratio;
};

struct mtk_raw_pad_config {
	struct v4l2_mbus_framefmt mbus_fmt;
};

struct mtk_raw_pipeline {
	unsigned int id;
	struct v4l2_subdev subdev;
	struct media_pad pads[MTK_RAW_PIPELINE_PADS_NUM];
	struct mtk_cam_video_device vdev_nodes[MTK_RAW_TOTAL_NODES];
	struct mtk_raw *raw;
	struct mtk_raw_pad_config cfg[MTK_RAW_PIPELINE_PADS_NUM];
	/* cached settings */
	unsigned int enabled_raw;
	unsigned int enabled_dmas;
	/* resource controls */
	struct v4l2_ctrl_handler ctrl_handler;
	struct mtk_cam_resource_config res_config;
};

struct mtk_raw_device {
	struct device *dev;
	struct mtk_cam_device *cam;
	struct device *larb;
	unsigned int id;
	void __iomem *base;
	void __iomem *base_inner;
	unsigned int num_clks;
	struct clk_bulk_data *clks;
	struct mtk_raw_pipeline *pipeline;
	spinlock_t spinlock_irq;
	unsigned int sof_count;
};

/*
 * struct mtk_raw - the raw information
 *
 * //FIXME for raw info comments
 *
 */
struct mtk_raw {
	struct device *cam_dev;
	struct device *devs[RAW_PIPELINE_NUM];
	struct mtk_raw_pipeline pipelines[RAW_PIPELINE_NUM];
};

int mtk_raw_register_entities(struct mtk_raw *raw,
			      struct v4l2_device *v4l2_dev);
void mtk_raw_unregister_entities(struct mtk_raw *raw);
int mtk_cam_raw_select(struct mtk_raw_pipeline *pipe,
		       struct mtkcam_ipi_input_param *cfg_in_param);

void initialize(struct mtk_raw_device *dev);
void stream_on(struct mtk_raw_device *dev, int on);
void apply_cq(struct mtk_raw_device *dev,
	      dma_addr_t cq_addr, unsigned int cq_size, int initial);
void reset(struct mtk_raw_device *dev);

extern struct platform_driver mtk_cam_raw_driver;

#endif /*__MTK_CAM_RAW_H*/
