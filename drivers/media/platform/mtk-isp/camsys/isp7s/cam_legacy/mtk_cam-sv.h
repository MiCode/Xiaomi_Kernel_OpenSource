/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_SV_H
#define __MTK_CAM_SV_H

#include <linux/kfifo.h>
#include <linux/suspend.h>

#include "mtk_cam-video.h"

#define CAMSV_HW_NUM 6
#define MAX_SV_HW_TAGS 8
#define MAX_SV_HW_GROUPS 4
#define MAX_SV_PIPELINE_NUN 16
#define CAMSV_IRQ_NUM 4

#define SV_IMG_MAX_WIDTH		8192
#define SV_IMG_MAX_HEIGHT		6144
#define SV_IMG_MIN_WIDTH		0
#define SV_IMG_MIN_HEIGHT		0

/* refer to ipi hw path definition */
enum mtkcam_sv_hw_path_control {
	MTKCAM_SV_SPECIAL_SCENARIO_ADDITIONAL_RAW = 24,
	MTKCAM_SV_SPECIAL_SCENARIO_EXT_ISP,
	MTKCAM_SV_SPECIAL_SCENARIO_DISPLAY_IC
};

#define CAMSV_EXT_META_0_WIDTH 1024
#define CAMSV_EXT_META_0_HEIGHT 1024
#define CAMSV_EXT_META_1_WIDTH 1024
#define CAMSV_EXT_META_1_HEIGHT 1024
#define CAMSV_EXT_META_2_WIDTH 1024
#define CAMSV_EXT_META_2_HEIGHT 1024

#define MTK_CAMSV_SUPPORTED_SPECIAL_HW_SCENARIO	(\
			(1 << MTKCAM_SV_SPECIAL_SCENARIO_ADDITIONAL_RAW) |\
			(1 << MTKCAM_SV_SPECIAL_SCENARIO_EXT_ISP) |\
			(1 << MTKCAM_SV_SPECIAL_SCENARIO_DISPLAY_IC) |\
			(1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_STAGGER)) |\
			(1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_DC_STAGGER)) |\
			(1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER)) |\
			(1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_OFFLINE)))

#define CAMSV_WRITE_BITS(RegAddr, RegName, FieldName, FieldValue) do {\
	union RegName reg;\
	\
	reg.Raw = readl_relaxed(RegAddr);\
	reg.Bits.FieldName = FieldValue;\
	writel_relaxed(reg.Raw, RegAddr);\
} while (0)

#define CAMSV_WRITE_REG(RegAddr, RegValue) ({\
	writel_relaxed(RegValue, RegAddr);\
})

#define CAMSV_READ_BITS(RegAddr, RegName, FieldName) ({\
	union RegName reg;\
	\
	reg.Raw = readl_relaxed(RegAddr);\
	reg.Bits.FieldName;\
})

#define CAMSV_READ_REG(RegAddr) ({\
	unsigned int var;\
	\
	var = readl_relaxed(RegAddr);\
	var;\
})

struct mtk_cam_ctx;
struct mtk_cam_request;
struct mtk_cam_request_stream_data;

enum camsv_function_id {
	DISPLAY_IC = (1 << 0)
};

enum camsv_module_id {
	CAMSV_START = 0,
	CAMSV_0 = CAMSV_START,
	CAMSV_1 = 1,
	CAMSV_2 = 2,
	CAMSV_3 = 3,
	CAMSV_4 = 4,
	CAMSV_5 = 5,
	CAMSV_END
};

enum camsv_db_load_src {
	SV_DB_SRC_SUB_P1_DONE = 0,
	SV_DB_SRC_SOF         = 1,
	SV_DB_SRC_SUB_SOF     = 2,
};

enum camsv_int_en {
	SV_INT_EN_VS1_INT_EN               = (1L<<0),
	SV_INT_EN_TG_INT1_EN               = (1L<<1),
	SV_INT_EN_TG_INT2_EN               = (1L<<2),
	SV_INT_EN_EXPDON1_INT_EN           = (1L<<3),
	SV_INT_EN_TG_ERR_INT_EN            = (1L<<4),
	SV_INT_EN_TG_GBERR_INT_EN          = (1L<<5),
	SV_INT_EN_TG_SOF_INT_EN            = (1L<<6),
	SV_INT_EN_TG_WAIT_INT_EN           = (1L<<7),
	SV_INT_EN_TG_DROP_INT_EN           = (1L<<8),
	SV_INT_EN_VS_INT_ORG_EN            = (1L<<9),
	SV_INT_EN_DB_LOAD_ERR_EN           = (1L<<10),
	SV_INT_EN_PASS1_DON_INT_EN         = (1L<<11),
	SV_INT_EN_SW_PASS1_DON_INT_EN      = (1L<<12),
	SV_INT_EN_SUB_PASS1_DON_INT_EN     = (1L<<13),
	SV_INT_EN_UFEO_OVERR_INT_EN        = (1L<<15),
	SV_INT_EN_DMA_ERR_INT_EN           = (1L<<16),
	SV_INT_EN_IMGO_OVERR_INT_EN        = (1L<<17),
	SV_INT_EN_UFEO_DROP_INT_EN         = (1L<<18),
	SV_INT_EN_IMGO_DROP_INT_EN         = (1L<<19),
	SV_INT_EN_IMGO_DONE_INT_EN         = (1L<<20),
	SV_INT_EN_UFEO_DONE_INT_EN         = (1L<<21),
	SV_INT_EN_TG_INT3_EN               = (1L<<22),
	SV_INT_EN_TG_INT4_EN               = (1L<<23),
	SV_INT_EN_INT_WCLR_EN              = (1L<<31),
};

enum camsv_tg_fmt {
	SV_TG_FMT_RAW8      = 0,
	SV_TG_FMT_RAW10     = 1,
	SV_TG_FMT_RAW12     = 2,
	SV_TG_FMT_YUV422    = 3,
	SV_TG_FMT_RAW14     = 4,
	SV_TG_FMT_RSV1      = 5,
	SV_TG_FMT_RSV2      = 6,
	SV_TG_FMT_JPG       = 7,
};

enum camsv_tg_swap {
	TG_SW_UYVY = 0,
	TG_SW_YUYV = 1,
	TG_SW_VYUY = 2,
	TG_SW_YVYU = 3,
};

/* enum for pads of camsv pipeline */
enum {
	MTK_CAMSV_SINK_BEGIN = 0,
	MTK_CAMSV_SINK = MTK_CAMSV_SINK_BEGIN,
	MTK_CAMSV_SINK_NUM,

	MTK_CAMSV_SOURCE_BEGIN = MTK_CAMSV_SINK_NUM,
	MTK_CAMSV_MAIN_STREAM_OUT = MTK_CAMSV_SOURCE_BEGIN,
	MTK_CAMSV_EXT_STREAM_OUT,
	MTK_CAMSV_PIPELINE_PADS_NUM,
};

enum camsv_tag_idx {
	SVTAG_START = 0,
	SVTAG_IMG_START = SVTAG_START,
	SVTAG_0 = SVTAG_IMG_START,
	SVTAG_1,
	SVTAG_2,
	SVTAG_3,
	SVTAG_IMG_END,
	SVTAG_META_START = SVTAG_IMG_END,
	SVTAG_4 = SVTAG_META_START,
	SVTAG_5,
	SVTAG_6,
	SVTAG_7,
	SVTAG_META_END,
	SVTAG_END = SVTAG_META_END,
};

#define MTK_CAMSV_TOTAL_NODES (MTK_CAMSV_PIPELINE_PADS_NUM - MTK_CAMSV_SINK_NUM)

struct mtk_camsv_pad_config {
	struct v4l2_mbus_framefmt mbus_fmt;
};

struct mtk_camsv_pipeline {
	unsigned int id;
	struct v4l2_subdev subdev;
	struct media_pad pads[MTK_CAMSV_PIPELINE_PADS_NUM];
	struct mtk_cam_video_device vdev_nodes[MTK_CAMSV_TOTAL_NODES];
	struct mtk_camsv *sv;
	struct mtk_camsv_pad_config cfg[MTK_CAMSV_PIPELINE_PADS_NUM];

	/* seninf pad index */
	u32 seninf_padidx;

	unsigned int req_vfmt_update;
	unsigned int req_pfmt_update;
	struct v4l2_subdev_format req_pad_fmt[MTK_CAMSV_PIPELINE_PADS_NUM];

	/* display ic */
	u32 feature_pending;
};

struct mtk_camsv_tag_info {
	struct mtk_camsv_pipeline *sv_pipe;
	u8 seninf_padidx;
	unsigned int hw_scen;
	unsigned int tag_order;
	/* camsv todo: move stride to frame param */
	unsigned int stride;
	struct mtkcam_ipi_input_param cfg_in_param;
	struct v4l2_format img_fmt;
	atomic_t is_config_done;
	atomic_t is_stream_on;
};

struct mtk_camsv_device {
	struct device *dev;
	struct mtk_cam_device *cam;
	unsigned int id;
	int irq[CAMSV_IRQ_NUM];
	void __iomem *base;
	void __iomem *base_inner;
	void __iomem *base_dma;
	void __iomem *base_inner_dma;
	void __iomem *base_scq;
	void __iomem *base_inner_scq;
	unsigned int num_clks;
	struct clk **clks;
	unsigned int cammux_id;
	unsigned int used_tag_cnt;
	unsigned int streaming_cnt;
	unsigned int enabled_tags;
	unsigned int ctx_stream_id;
	struct mtk_camsv_tag_info tag_info[MAX_SV_HW_TAGS];
	unsigned int cfg_group_info[MAX_SV_HW_GROUPS];
	unsigned int active_group_info[MAX_SV_HW_GROUPS];
	unsigned int first_tag;
	unsigned int last_tag;

	int fifo_size;
	void *msg_buffer;
	struct kfifo msg_fifo;
	atomic_t is_fifo_overflow;

	unsigned int sof_count;
	/* for preisp - for sof counter sync.*/
	int tg_cnt;
	u64 last_sof_time_ns;
	unsigned int frame_wait_to_process;
	struct notifier_block notifier_blk;
	u64 sof_timestamp;

	atomic_t is_first_frame;
};

struct mtk_camsv {
	struct device *cam_dev;
	struct device *devs[CAMSV_HW_NUM];
	struct mtk_camsv_pipeline pipelines[MAX_SV_PIPELINE_NUN];
};

struct mtk_camsv_reconfig_info {
	u32 grab_pxl;
	u32 grab_lin;
	u32 fmt_sel;
	u32 pak;
	u32 imgo_xsize;
	u32 imgo_ysize;
	u32 imgo_stride;
};

static inline bool mtk_camsv_is_yuv_format(unsigned int fmt)
{
	bool ret = false;

	switch (fmt) {
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
		ret = true;
		break;
	default:
		ret = false;
		break;
	}

	return ret;
}

struct mtk_larb;
int mtk_camsv_register_entities(
	struct mtk_camsv *sv, struct v4l2_device *v4l2_dev);
void mtk_camsv_unregister_entities(struct mtk_camsv *sv);
int mtk_camsv_call_pending_set_fmt(struct v4l2_subdev *sd,
				 struct v4l2_subdev_format *fmt);
void sv_reset(struct mtk_camsv_device *dev);
int mtk_cam_call_sv_pipeline_config(
	struct mtk_cam_ctx *ctx,
	struct mtk_camsv_tag_info *arr_tag,
	unsigned int tag_idx,
	unsigned int seninf_padidx,
	unsigned int hw_scen,
	unsigned int tag_order,
	unsigned int pixelmode,
	unsigned int sub_ratio,
	struct mtk_camsv_pipeline *pipeline,
	struct v4l2_mbus_framefmt *mf,
	struct v4l2_format *img_fmt);
int mtk_cam_sv_pipeline_config(struct mtk_camsv_device *camsv_dev);
int mtk_cam_sv_cq_config(struct mtk_camsv_device *camsv_dev);
int mtk_cam_sv_cq_enable(struct mtk_camsv_device *camsv_dev);
int mtk_cam_sv_cq_disable(struct mtk_camsv_device *camsv_dev);
struct mtk_camsv_device *mtk_cam_get_used_sv_dev(struct mtk_cam_ctx *ctx);
int mtk_cam_get_sv_cammux_id(struct mtk_camsv_device *camsv_dev, int tag_idx);
int mtk_cam_sv_dev_pertag_stream_on(
	struct mtk_cam_ctx *ctx, unsigned int tag_idx,
	unsigned int streaming);
int mtk_cam_sv_dev_stream_on(
	struct mtk_cam_ctx *ctx, unsigned int streaming);
unsigned int mtk_cam_sv_format_sel(unsigned int pixel_fmt);
unsigned int mtk_cam_sv_pak_sel(unsigned int tg_fmt,
	unsigned int pixel_mode);
unsigned int mtk_cam_sv_xsize_cal(
	struct mtkcam_ipi_input_param *cfg_in_param);
int mtk_cam_sv_central_common_config(struct mtk_camsv_device *dev,
	unsigned int sub_ratio);
int mtk_cam_sv_dmao_common_config(struct mtk_camsv_device *dev);
int mtk_cam_sv_central_pertag_config(struct mtk_camsv_device *dev, unsigned int tag_idx);
int mtk_cam_sv_dmao_pertag_config(
	struct mtk_camsv_device *dev, unsigned int tag_idx);
int mtk_cam_sv_fbc_pertag_config(
	struct mtk_camsv_device *dev, unsigned int tag_idx);
int mtk_cam_sv_fbc_pertag_enable(
	struct mtk_camsv_device *dev, unsigned int tag_idx);
int mtk_cam_sv_print_fbc_status(struct mtk_camsv_device *dev);
int mtk_cam_sv_toggle_tg_db(struct mtk_camsv_device *dev);
int mtk_cam_sv_toggle_db(struct mtk_camsv_device *dev);
int mtk_cam_sv_central_common_enable(struct mtk_camsv_device *dev);
int mtk_cam_sv_fbc_disable(struct mtk_camsv_device *dev, unsigned int tag_idx);
int mtk_cam_sv_enquehwbuf(struct mtk_camsv_device *dev,
	dma_addr_t ba, unsigned int seq_no, unsigned int tag);
int mtk_cam_sv_apply_all_buffers(struct mtk_cam_ctx *ctx);
unsigned int mtk_cam_get_sv_tag_index(struct mtk_cam_ctx *ctx,
		unsigned int pipe_id);
int mtk_cam_sv_dev_pertag_write_rcnt(
	struct mtk_camsv_device *camsv_dev,
	unsigned int tag_idx);
void mtk_cam_sv_vf_reset(struct mtk_cam_ctx *ctx,
	struct mtk_camsv_device *dev);
bool mtk_cam_sv_is_zero_fbc_cnt(struct mtk_cam_ctx *ctx, unsigned int pipe_id);
void mtk_cam_sv_check_fbc_cnt(struct mtk_camsv_device *camsv_dev,
	unsigned int tag_idx);
int mtk_cam_sv_frame_no_inner(struct mtk_camsv_device *dev);
void apply_camsv_cq(struct mtk_camsv_device *dev,
	      dma_addr_t cq_addr, unsigned int cq_size, unsigned int cq_offset,
	      int initial);
int mtk_cam_sv_update_feature(struct mtk_cam_video_device *node);
int mtk_cam_sv_update_image_size(struct mtk_cam_video_device *node, struct v4l2_format *f);
bool mtk_cam_is_display_ic(struct mtk_cam_ctx *ctx);

#ifdef CAMSYS_TF_DUMP_7S
int mtk_camsv_translation_fault_callback(int port, dma_addr_t mva, void *data);
#endif

extern struct platform_driver mtk_cam_sv_driver;

#endif
