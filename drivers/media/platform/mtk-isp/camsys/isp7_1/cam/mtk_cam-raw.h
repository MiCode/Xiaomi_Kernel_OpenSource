/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_RAW_H
#define __MTK_CAM_RAW_H

#include <linux/kfifo.h>
#include <media/v4l2-subdev.h>
#include "mtk_cam-video.h"
#include "mtk_camera-v4l2-controls.h"

struct mtk_cam_request_stream_data;

#define RAW_PIPELINE_NUM 3
#define SCQ_DEADLINE_MS  15 // ~1/2 frame length
#define SCQ_DEFAULT_CLK_RATE 208 // default 208MHz
#define USINGSCQ 1
#define MTK_CAMSYS_RES_STEP_NUM	8

/* FIXME: dynamic config image max/min w/h */
#define IMG_MAX_WIDTH		12000
#define IMG_MAX_HEIGHT		9000
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
	RAW_C = 2,
	RAW_NUM = 3,
};
#define PREISP
/* feature mask to categorize all raw functions */
#define MTK_CAM_FEATURE_HDR_MASK		0x0000000F
#define MTK_CAM_FEATURE_SUBSAMPLE_MASK		0x000000F0
#define MTK_CAM_FEATURE_OFFLINE_M2M_MASK	0x00000100
#define MTK_CAM_FEATURE_PURE_OFFLINE_M2M_MASK	0x00000200
#define MTK_CAM_FEATURE_TIMESHARE_MASK		0x00001000
#define MTK_CAM_FEATURE_HDR_MEMORY_SAVING_MASK		0x00002000
#define MTK_CAM_FEATURE_EXT_ISP_MASK	0x0000C000
/* flags common to features */
#define MTK_CAM_FEATURE_SEAMLESS_SWITCH_MASK	BIT(31)


enum raw_function_id {
	/* hdr */
	STAGGER_2_EXPOSURE_LE_SE	= (1 << 0),
	STAGGER_2_EXPOSURE_SE_LE	= (2 << 0),
	STAGGER_3_EXPOSURE_LE_NE_SE	= (3 << 0),
	STAGGER_3_EXPOSURE_SE_NE_LE	= (4 << 0),
	MSTREAM_NE_SE			= (5 << 0),
	MSTREAM_SE_NE			= (6 << 0),
	/* high fps (subsample) */
	HIGHFPS_2_SUBSAMPLE		= (1 << 4),
	HIGHFPS_4_SUBSAMPLE		= (2 << 4),
	HIGHFPS_8_SUBSAMPLE		= (3 << 4),
	HIGHFPS_16_SUBSAMPLE		= (4 << 4),
	HIGHFPS_32_SUBSAMPLE		= (5 << 4),
	OFFLINE_M2M			= (1 << 8),
	PURE_OFFLINE_M2M		= (1 << 9),
	TIMESHARE_1_GROUP		= (1 << 12),
	HDR_MEMORY_SAVING		= (1 << 13),
	EXT_ISP_CUS_1			= (1 << 14),
	EXT_ISP_CUS_2			= (2 << 14),
	EXT_ISP_CUS_3			= (3 << 14),
	WITH_W_CHANNEL			= (1 << 16),
	RAW_FUNCTION_END		= 0xF0000000,
};

enum hdr_scenario_id {
	STAGGER_ON_THE_FLY	= (1 << 0),
	STAGGER_OFFLINE		= (1 << 1),
	STAGGER_DCIF		= (1 << 2),
	STAGGER_M2M		= (1 << 3),
	MSTREAM			= (1 << 4),
	MSTREAM_M2M		= (1 << 5),
};

enum hardware_mode_id {
	HW_MODE_DEFAULT			= 0,
	HW_MODE_ON_THE_FLY		= 1,
	HW_MODE_DIRECT_COUPLED	= 2,
	HW_MODE_OFFLINE			= 3,
	HW_MODE_M2M				= 4,
};

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
	MTK_RAW_MAIN_STREAM_SV_1_OUT,
	MTK_RAW_MAIN_STREAM_SV_2_OUT,
	MTK_RAW_META_OUT_BEGIN,
	MTK_RAW_META_OUT_0 = MTK_RAW_META_OUT_BEGIN,
	MTK_RAW_META_OUT_1,
	MTK_RAW_META_OUT_2,
	MTK_RAW_META_SV_OUT_0,
	MTK_RAW_META_SV_OUT_1,
	MTK_RAW_META_SV_OUT_2,
	MTK_RAW_PIPELINE_PADS_NUM,
};

/* max(pdi_table1, pdi_table2, ...) */
#define RAW_STATS_CFG_VARIOUS_SIZE ALIGN(0x7500, SZ_1K)

#define MTK_RAW_TOTAL_NODES (MTK_RAW_PIPELINE_PADS_NUM - MTK_RAW_SINK_NUM)

#define MTK_RAW_CTRL_VALUE	0x0F
#define MTK_RAW_CTRL_UPDATE	0x10

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

struct mtk_raw_pde_config {
	struct mtk_cam_pde_info pde_info[CAM_CTRL_NUM];
};

struct mtk_cam_resource_config {
	struct v4l2_subdev *seninf;
	struct mutex resource_lock;
	struct v4l2_fract interval;
	s64 pixel_rate;
	u32 bin_limit;
	u32 frz_limit;
	u32 hwn_limit_max;
	u32 hwn_limit_min;
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
	u32 tgo_pxl_mode_before_raw;
	u32 raw_path;
	/* sink fmt adjusted according resource used*/
	struct v4l2_mbus_framefmt sink_fmt;
	u32 enable_hsf_raw;
	u32 hw_mode;
};

/* exposure for m-stream */
struct mtk_cam_shutter_gain {
	__u32 shutter;
	__u32 gain;
};

/* multiple exposure for m-stream(2 exposures) */
struct mtk_cam_mstream_exposure {
	struct mtk_cam_shutter_gain exposure[2];
	unsigned int valid;
	int req_id;
};

struct mtk_raw_pad_config {
	struct v4l2_mbus_framefmt mbus_fmt;
	struct v4l2_rect crop;
};

/*
 * struct mtk_raw_pipeline - sub dev to use raws.
 *
 * @feature_pending: keep the user value of S_CTRL V4L2_CID_MTK_CAM_FEATURE.
 *		     It it safe save to be used in mtk_cam_vidioc_s_fmt,
 *		     mtk_cam_vb2_queue_setup and mtk_cam_vb2_buf_queue
 *		     But considering that we can't when the user calls S_CTRL,
 *		     please use mtk_cam_request_stream_data's
 *		     feature.raw_feature field
 *		     to avoid the CTRL value change tming issue.
 * @feature_active: The active feature during streaming. It can't be changed
 *		    during streaming and can only be used after streaming on.
 *
 */
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
	s64 feature_pending;
	s64 feature_active;
	int dynamic_exposure_num_max;
	bool enqueued_tg_flash_req; /* need a better way to collect the request */
	struct mtk_cam_tg_flash_config tg_flash_config;
	/* TODO: merge or integrate with mtk_cam_resource_config */
	struct mtk_cam_resource user_res;
	struct mtk_cam_resource_config res_config;
	int sensor_mode_update;
	s64 sync_id;
	/* mstream */
	struct mtk_cam_mstream_exposure mstream_exposure;
	/* pde module */
	struct mtk_raw_pde_config pde_config;
	s64 hw_mode;
	s64 hw_mode_pending;
	/* Frame sync */
	int fs_config;
};

struct mtk_raw_device {
	struct device *dev;
	struct mtk_cam_device *cam;
	unsigned int id;
	int irq;
	void __iomem *base;
	void __iomem *base_inner;
	void __iomem *yuv_base;
	void __iomem *yuv_base_inner;
	unsigned int num_clks;
	struct clk **clks;
#ifdef CONFIG_PM_SLEEP
	struct notifier_block pm_notifier;
#endif

	int		fifo_size;
	void		*msg_buffer;
	struct kfifo	msg_fifo;
	atomic_t	is_fifo_overflow;

	struct mtk_raw_pipeline *pipeline;
	bool is_slave;

	u64 sof_count;
	u64 vsync_count;
	u64 last_sof_time_ns;

	/* for subsample, sensor-control */
	bool sub_sensor_ctrl_en;
	int set_sensor_idx;
	int cur_vsync_idx;

	u8 time_shared_busy;
	u8 time_shared_busy_ctx_id;
	atomic_t vf_en;
	u32 stagger_en;
	int overrun_debug_dump_cnt;
	int default_printk_cnt;

	/* larb */
	struct platform_device *larb_pdev;
};

struct mtk_yuv_device {
	struct device *dev;
	unsigned int id;
	void __iomem *base;
	void __iomem *base_inner;
	unsigned int num_clks;
	struct clk **clks;
#ifdef CONFIG_PM_SLEEP
	struct notifier_block pm_notifier;
#endif
	struct platform_device *larb_pdev;
};

/* AE information */
struct mtk_ae_debug_data {
	u64 OBC_R1_Sum[4];
	u64 OBC_R2_Sum[4];
	u64 OBC_R3_Sum[4];
	u64 AA_Sum[4];
	u64 LTM_Sum[4];
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

struct mtk_raw_stagger_select {
	int hw_mode;
	int enabled_raw;
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

int mtk_cam_raw_select(struct mtk_cam_ctx *ctx,
		       struct mtkcam_ipi_input_param *cfg_in_param);

int mtk_cam_get_subsample_ratio(int raw_feature);

void subsample_enable(struct mtk_raw_device *dev);

void stagger_enable(struct mtk_raw_device *dev);

void stagger_disable(struct mtk_raw_device *dev);

void dbload_force(struct mtk_raw_device *dev);

void toggle_db(struct mtk_raw_device *dev);

void enable_tg_db(struct mtk_raw_device *dev, int en);

void initialize(struct mtk_raw_device *dev, int is_slave);

void stream_on(struct mtk_raw_device *dev, int on);

void immediate_stream_off(struct mtk_raw_device *dev);

void apply_cq(struct mtk_raw_device *dev,
	      int initial, dma_addr_t cq_addr,
	      unsigned int cq_size, unsigned int cq_offset,
	      unsigned int sub_cq_size, unsigned int sub_cq_offset);

void trigger_rawi(struct mtk_raw_device *dev, struct mtk_cam_ctx *ctx,
		signed int hw_scene);

void reset(struct mtk_raw_device *dev);

void dump_aa_info(struct mtk_cam_ctx *ctx,
				 struct mtk_ae_debug_data *ae_info);

int mtk_raw_call_pending_set_fmt(struct v4l2_subdev *sd,
				 struct v4l2_subdev_format *fmt);

struct v4l2_subdev *mtk_cam_find_sensor(struct mtk_cam_ctx *ctx,
					struct media_entity *entity);

void mtk_cam_update_sensor(struct mtk_cam_ctx *ctx,
			   struct v4l2_subdev *sensor);

struct mtk_raw_pipeline*
mtk_cam_get_link_enabled_raw(struct v4l2_subdev *seninf);

bool
mtk_raw_sel_get_res(struct v4l2_subdev *sd, struct v4l2_subdev_selection *sel,
		    struct mtk_cam_resource *res);
bool
mtk_raw_fmt_get_res(struct v4l2_subdev *sd, struct v4l2_subdev_format *fmt,
		    struct mtk_cam_resource *res);

unsigned int mtk_raw_get_hdr_scen_id(struct mtk_cam_ctx *ctx);

struct v4l2_mbus_framefmt*
mtk_raw_pipeline_get_fmt(struct mtk_raw_pipeline *pipe,
			 struct v4l2_subdev_pad_config *cfg,
			 int padid, int which);
struct v4l2_rect*
mtk_raw_pipeline_get_selection(struct mtk_raw_pipeline *pipe,
			       struct v4l2_subdev_pad_config *cfg,
			       int pad, int which);
int
mtk_cam_raw_try_res_ctrl(struct mtk_raw_pipeline *pipeline,
			 struct mtk_cam_resource *res_user,
			 struct mtk_cam_resource_config *res_cfg,
			 struct v4l2_mbus_framefmt *sink_fmt,
			 char *dbg_str, bool log);
int
mtk_cam_res_copy_fmt_from_user(struct mtk_raw_pipeline *pipeline,
			       struct mtk_cam_resource *res_user,
			       struct v4l2_mbus_framefmt *dest);

int
mtk_cam_res_copy_fmt_to_user(struct mtk_raw_pipeline *pipeline,
			     struct mtk_cam_resource *res_user,
			     struct v4l2_mbus_framefmt *src);

bool mtk_raw_resource_calc(struct mtk_cam_device *cam,
			   struct mtk_cam_resource_config *res,
			   s64 pixel_rate, int res_plan, int fps,
			   int in_w, int in_h, int *out_w, int *out_h);

#ifdef CAMSYS_TF_DUMP_71_1
int
mtk_cam_translation_fault_callback(int port,
				dma_addr_t mva,
				void *data);
#endif

extern struct platform_driver mtk_cam_raw_driver;
extern struct platform_driver mtk_cam_yuv_driver;

static inline u32 dmaaddr_lsb(dma_addr_t addr)
{
	return addr & (BIT_MASK(32) - 1UL);
}

static inline u32 dmaaddr_msb(dma_addr_t addr)
{
	return addr >> 32;
}

#endif /*__MTK_CAM_RAW_H*/
