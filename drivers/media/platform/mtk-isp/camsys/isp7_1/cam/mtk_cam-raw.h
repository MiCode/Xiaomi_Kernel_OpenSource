/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_RAW_H
#define __MTK_CAM_RAW_H

#include <media/v4l2-subdev.h>
#include "mtk_cam-video.h"

#define RAW_PIPELINE_NUM 2
#define SCQ_DEADLINE_MS  15 // ~1/2 frame length
#define SCQ_DEFAULT_CLK_RATE 208 // default 208MHz
#define USINGSCQ 1
#define MTK_CAMSYS_RES_STEP_NUM	8
#define _STAGGER_TRIGGER_CQ_BY_CAMSV_SOF 1
/* FIXME: dynamic config image max/min w/h */
#define IMG_MAX_WIDTH		5376
#define IMG_MAX_HEIGHT		4032
#define IMG_MIN_WIDTH		80
#define IMG_MIN_HEIGHT		60
#define YUV_GROUP1_MAX_WIDTH	8160
#define YUV_GROUP1_MAX_HEIGHT	3896
#define YUV_GROUP2_MAX_WIDTH	3060
#define YUV_GROUP2_MAX_HEIGHT	1145
#define YUV1_MAX_WIDTH		8160
#define YUV1_MAX_HEIGHT		2290
#define YUV2_MAX_WIDTH		3060
#define YUV2_MAX_HEIGHT		1145
#define YUV3_MAX_WIDTH		7794
#define YUV3_MAX_HEIGHT		3896
#define YUV4_MAX_WIDTH		1530
#define YUV4_MAX_HEIGHT		572
#define YUV5_MAX_WIDTH		1530
#define YUV5_MAX_HEIGHT		572
#define DRZS4NO1_MAX_WIDTH	2400
#define DRZS4NO1_MAX_HEIGHT	1080
#define DRZS4NO2_MAX_WIDTH	2400
#define DRZS4NO2_MAX_HEIGHT	1080
#define DRZS4NO3_MAX_WIDTH	576
#define DRZS4NO3_MAX_HEIGHT	432
#define RZH1N2TO1_MAX_WIDTH	1280
#define RZH1N2TO1_MAX_HEIGHT	600
#define RZH1N2TO2_MAX_WIDTH	512
#define RZH1N2TO2_MAX_HEIGHT	512
#define RZH1N2TO3_MAX_WIDTH	1280
#define RZH1N2TO3_MAX_HEIGHT	600

#define IMG_PIX_ALIGN		2

enum raw_module_id {
	RAW_A = 0,
	RAW_B = 1,
};

#define MTK_CAM_FEATURE_STAGGER_MASK	0x0000000F
#define MTK_CAM_FEATURE_SUBSAMPLE_MASK	0x000000F0
#define MTK_CAM_FEATURE_STAGGER_M2M_MASK	0x00000F00
#define MTK_CAM_FEATURE_TIMESHARE_MASK	0x0000F000

enum raw_function_id {
	STAGGER_2_EXPOSURE_LE_SE	= (1 << 0),
	STAGGER_2_EXPOSURE_SE_LE	= (2 << 0),
	STAGGER_3_EXPOSURE_LE_NE_SE	= (3 << 0),
	STAGGER_3_EXPOSURE_SE_NE_LE	= (4 << 0),
	HIGHFPS_2_SUBSAMPLE		= (1 << 4),
	HIGHFPS_4_SUBSAMPLE		= (2 << 4),
	HIGHFPS_8_SUBSAMPLE		= (3 << 4),
	HIGHFPS_16_SUBSAMPLE		= (4 << 4),
	HIGHFPS_32_SUBSAMPLE		= (5 << 4),
	STAGGER_M2M_2_EXPOSURE_LE_SE	= (1 << 8),
	STAGGER_M2M_2_EXPOSURE_SE_LE	= (2 << 8),
	STAGGER_M2M_3_EXPOSURE_LE_NE_SE = (3 << 8),
	STAGGER_M2M_3_EXPOSURE_SE_NE_LE = (4 << 8),
	TIMESHARE_1_GROUP		= (1 << 12),
	RAW_FUNCTION_END,
};

enum hdr_scenario_id {
	STAGGER_ON_THE_FLY	= (1 << 0),
	STAGGER_OFFLINE		= (1 << 1),
	STAGGER_DCIF		= (1 << 2),
	STAGGER_M2M		= (1 << 3),
};

#define RAW_STATS_CFG_SIZE \
	ALIGN(sizeof(struct mtk_cam_uapi_meta_raw_stats_cfg), SZ_1K)

/* meta out max size include 1k meta info and dma buffer size */
#define RAW_STATS_0_SIZE \
	ALIGN(ALIGN(sizeof(struct mtk_cam_uapi_meta_raw_stats_0), SZ_1K) + \
	      MTK_CAM_UAPI_AAO_MAX_BUF_SIZE + MTK_CAM_UAPI_AAHO_MAX_BUF_SIZE + \
	      MTK_CAM_UAPI_LTMSO_SIZE + \
	      MTK_CAM_UAPI_FLK_MAX_BUF_SIZE + \
	      MTK_CAM_UAPI_TSFSO_SIZE * 2 + /* r1 & r2 */ \
	      MTK_CAM_UAPI_TNCSYO_SIZE \
	      /* TODO: TNCSO TNCSBO TNCSHO SIZE */ \
	      , (4 * SZ_1K))
#define RAW_STATS_1_SIZE \
	ALIGN(ALIGN(sizeof(struct mtk_cam_uapi_meta_raw_stats_1), SZ_1K) + \
	      MTK_CAM_UAPI_AFO_MAX_BUF_SIZE, (4 * SZ_1K))
#define RAW_STATS_2_SIZE \
	ALIGN(ALIGN(sizeof(struct mtk_cam_uapi_meta_raw_stats_2), SZ_1K) + \
	      MTK_CAM_UAPI_ACTSO_SIZE, (4 * SZ_1K))

/* enum for pads of raw pipeline */
enum {
	MTK_RAW_SINK_BEGIN = 0,
	MTK_RAW_SINK = MTK_RAW_SINK_BEGIN,
	MTK_RAW_SINK_NUM,
	MTK_RAW_META_IN = MTK_RAW_SINK_NUM,
	MTK_RAW_RAWI_2_IN,
	MTK_RAW_RAWI_3_IN,
	MTK_RAW_RAWI_4_IN,
	MTK_RAW_SOURCE_BEGIN,
	MTK_RAW_MAIN_STREAM_OUT = MTK_RAW_SOURCE_BEGIN,
	MTK_RAW_YUVO_1_OUT,
	MTK_RAW_YUVO_2_OUT,
	MTK_RAW_YUVO_3_OUT,
	MTK_RAW_YUVO_4_OUT,
	MTK_RAW_YUVO_5_OUT,
	MTK_RAW_DRZS4NO_1_OUT,
	MTK_RAW_DRZS4NO_2_OUT,
	MTK_RAW_DRZS4NO_3_OUT,
	MTK_RAW_RZH1N2TO_1_OUT,
	MTK_RAW_RZH1N2TO_2_OUT,
	MTK_RAW_RZH1N2TO_3_OUT,
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
	struct v4l2_fract interval;
	s64 pixel_rate;
	u32 bin_limit;
	u32 frz_limit;
	u32 hwn_limit;
	s64 hblank;
	s64 vblank;
	s64 sensor_pixel_rate;
	u32 res_plan;
	u32 raw_feature;
	u32 res_strategy[MTK_CAMSYS_RES_STEP_NUM];
	u32 clk_target;
	u32 raw_num_used;
	u32 bin_enable;
	u32 frz_enable;
	u32 frz_ratio;
	u32 tgo_pxl_mode;
	u32 raw_path;
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
	unsigned long enabled_dmas;
	/* resource controls */
	struct v4l2_ctrl_handler ctrl_handler;
	struct mtk_cam_resource_config res_config;
	struct mtk_cam_resource_config try_res_config;
	s64 sync_id;
};

struct mtk_raw_device {
	struct device *dev;
	struct mtk_cam_device *cam;
	unsigned int id;
	void __iomem *base;
	void __iomem *base_inner;
	void __iomem *yuv_base;
	void __iomem *top_base;
	unsigned int num_clks;
	struct clk **clks;
	struct mtk_raw_pipeline *pipeline;
	spinlock_t spinlock_irq;
	u64 sof_count;
	u64 setting_count;
	int write_cnt;
	u64 sof_time;
	u32 vsync_ori_count;
	u8 time_shared_busy;
	u8 time_shared_busy_ctx_id;
	u32 vf_en;
	u32 stagger_en;
};

struct mtk_yuv_device {
	struct device *dev;
	unsigned int id;
	void __iomem *base;
	void __iomem *top_base;
	unsigned int num_clks;
	struct clk **clks;
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
	struct device *yuvs[RAW_PIPELINE_NUM];
	struct mtk_raw_pipeline pipelines[RAW_PIPELINE_NUM];
};

static inline struct mtk_raw_pipeline*
mtk_cam_ctrl_handler_to_raw_pipeline(struct v4l2_ctrl_handler *handler)
{
	return container_of(handler, struct mtk_raw_pipeline, ctrl_handler);
};

int mtk_raw_setup_dependencies(struct mtk_raw *raw);
int mtk_raw_register_entities(struct mtk_raw *raw,
			      struct v4l2_device *v4l2_dev);
void mtk_raw_unregister_entities(struct mtk_raw *raw);
int mtk_cam_raw_select(struct mtk_raw_pipeline *pipe,
		       struct mtkcam_ipi_input_param *cfg_in_param);
bool mtk_raw_dev_is_slave(struct mtk_raw_device *raw_dev);

void write_readcount(struct mtk_raw_device *dev);
int mtk_cam_get_subsample_ratio(int raw_feature);
void subsample_enable(struct mtk_raw_device *dev);
void stagger_enable(struct mtk_raw_device *dev);
void stagger_disable(struct mtk_raw_device *dev);
void toggle_db(struct mtk_raw_device *dev);
void enable_tg_db(struct mtk_raw_device *dev, int en);
void initialize(struct mtk_raw_device *dev);
void stream_on(struct mtk_raw_device *dev, int on);
void apply_cq(struct mtk_raw_device *dev,
	      dma_addr_t cq_addr, unsigned int cq_size, unsigned int cq_offset, int initial,
	      unsigned int sub_cq_size, unsigned int sub_cq_offset);
void trigger_rawi(struct mtk_raw_device *dev);
void reset(struct mtk_raw_device *dev);
int mtk_raw_call_pending_set_fmt(struct v4l2_subdev *sd,
				 struct v4l2_subdev_format *fmt);
int mtk_cam_update_sensor(struct mtk_cam_ctx *ctx,
			  struct media_entity *entity);

extern struct platform_driver mtk_cam_raw_driver;
extern struct platform_driver mtk_cam_yuv_driver;

#endif /*__MTK_CAM_RAW_H*/
