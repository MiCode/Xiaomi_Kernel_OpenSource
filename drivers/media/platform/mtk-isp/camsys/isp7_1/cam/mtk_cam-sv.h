/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_SV_H
#define __MTK_CAM_SV_H

#include <linux/kfifo.h>
#include <linux/suspend.h>

#include "mtk_cam-video.h"

#define PDAF_READY 1
#define CAMSV_PIPELINE_NUM 16

#define SV_IMG_MAX_WIDTH		8192
#define SV_IMG_MAX_HEIGHT		6144
#define SV_IMG_MIN_WIDTH		0
#define SV_IMG_MIN_HEIGHT		0

/* camsv hardware capability definition: */
/* [15:0]: hardware scenario             */
/* [19:16]: group number                 */
/* [23:20]: exposure order               */
#define CAMSV_GROUP_SHIFT 16
#define CAMSV_EXP_ORDER_SHIFT 20
#define CAMSV_GROUP_AMOUNT 4

enum mtkcam_sv_hw_path_control {
	MTKCAM_SV_SPECIAL_SCENARIO_ADDITIONAL_RAW = MTKCAM_IPI_HW_PATH_OFFLINE_M2M + 2,
};

#define MTK_CAMSV_SUPPORTED_SPECIAL_HW_SCENARIO	(\
			(1 << MTKCAM_SV_SPECIAL_SCENARIO_ADDITIONAL_RAW) |\
			(1 << MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER) |\
			(1 << MTKCAM_IPI_HW_PATH_OFFLINE_SRT_DCIF_STAGGER) |\
			(1 << MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER) |\
			(1 << MTKCAM_IPI_HW_PATH_OFFLINE_M2M))
#define MTK_CAMSV_SUPPORTED_STAGGER_SCENARIO	(\
			(1 << MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER) |\
			(1 << MTKCAM_IPI_HW_PATH_OFFLINE_SRT_DCIF_STAGGER) |\
			(1 << MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER))

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

#define CAMSV_MAX_PIPE_USED 4

struct mtk_cam_ctx;
struct mtk_cam_request;
struct mtk_cam_request_stream_data;

enum camsv_module_id {
	CAMSV_START = 0,
	CAMSV_0 = CAMSV_START,
	CAMSV_1 = 1,
	CAMSV_2 = 2,
	CAMSV_3 = 3,
	CAMSV_4 = 4,
	CAMSV_5 = 5,
	CAMSV_6 = 6,
	CAMSV_7 = 7,
	CAMSV_8 = 8,
	CAMSV_9 = 9,
	CAMSV_10 = 10,
	CAMSV_11 = 11,
	CAMSV_12 = 12,
	CAMSV_13 = 13,
	CAMSV_14 = 14,
	CAMSV_15 = 15,
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
	MTK_CAMSV_PIPELINE_PADS_NUM,
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

	/* cached settings */
	unsigned int enabled_sv;
	unsigned int enabled_dmas;

	/* seninf pad index */
	u32 seninf_padidx;

	/* special hardware scenario */
	unsigned int hw_cap;
	unsigned int cammux_id;
	unsigned int hw_scen;
	unsigned int master_pipe_id;
	unsigned int exp_order;
	unsigned int is_occupied;
};

struct mtk_camsv_device {
	struct device *dev;
	struct mtk_cam_device *cam;
	unsigned int id;
	int irq;
	void __iomem *base;
	void __iomem *base_inner;
	unsigned int num_clks;
	struct clk **clks;
	struct mtk_camsv_pipeline *pipeline;
	unsigned int hw_cap;
	unsigned int cammux_id;

	int fifo_size;
	void *msg_buffer;
	struct kfifo msg_fifo;
	atomic_t is_fifo_overflow;

	unsigned int sof_count;
	unsigned int frame_wait_to_process;
	struct notifier_block notifier_blk;
	unsigned int is_enqueued;
};

struct mtk_camsv {
	struct device *cam_dev;
	struct device *devs[CAMSV_PIPELINE_NUM];
	struct mtk_camsv_pipeline pipelines[CAMSV_PIPELINE_NUM];
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

struct mtk_camsv_frame_params {
	u8 is_reconfig;
	struct mtk_camsv_reconfig_info cfg_info;
	struct mtkcam_ipi_img_output img_out;
};

struct mtk_larb;
int mtk_camsv_setup_dependencies(struct mtk_camsv *sv, struct mtk_larb *larb);
int mtk_camsv_register_entities(
	struct mtk_camsv *sv, struct v4l2_device *v4l2_dev);
void mtk_camsv_unregister_entities(struct mtk_camsv *sv);
int mtk_camsv_call_pending_set_fmt(struct v4l2_subdev *sd,
				 struct v4l2_subdev_format *fmt);
int mtk_cam_sv_select(
	struct mtk_camsv_pipeline *pipe,
	struct mtkcam_ipi_input_param *cfg_in_param);
void sv_reset(struct mtk_camsv_device *dev);
int mtk_cam_sv_pipeline_config(
	struct mtk_cam_ctx *ctx, unsigned int idx,
	struct mtkcam_ipi_input_param *cfg_in_param);
struct device *mtk_cam_find_sv_dev(
	struct mtk_cam_device *cam, unsigned int sv_mask);
int mtk_cam_sv_dev_config(
	struct mtk_cam_ctx *ctx, unsigned int idx, unsigned int hw_scen,
	unsigned int exp_order);
int mtk_cam_sv_dev_stream_on(
	struct mtk_cam_ctx *ctx, unsigned int idx,
	unsigned int streaming, unsigned int hw_scen);
unsigned int mtk_cam_sv_format_sel(unsigned int pixel_fmt);
unsigned int mtk_cam_sv_pak_sel(unsigned int tg_fmt,
	unsigned int pixel_mode);
unsigned int mtk_cam_sv_xsize_cal(
	struct mtkcam_ipi_input_param *cfg_in_param);
int mtk_cam_sv_tg_config(
	struct mtk_camsv_device *dev, struct mtkcam_ipi_input_param *cfg_in_param);
int mtk_cam_sv_top_config(
	struct mtk_camsv_device *dev, struct mtkcam_ipi_input_param *cfg_in_param);
int mtk_cam_sv_dmao_config(struct mtk_camsv_device *dev,
	struct mtkcam_ipi_input_param *cfg_in_param, int hw_scen, int raw_imgo_stride);
int mtk_cam_sv_fbc_config(
	struct mtk_camsv_device *dev, struct mtkcam_ipi_input_param *cfg_in_param);

int mtk_cam_sv_tg_enable(
	struct mtk_camsv_device *dev, struct mtkcam_ipi_input_param *cfg_in_param);
int mtk_cam_sv_toggle_tg_db(struct mtk_camsv_device *dev);
int mtk_cam_sv_toggle_db(struct mtk_camsv_device *dev);
int mtk_cam_sv_top_enable(struct mtk_camsv_device *dev);
int mtk_cam_sv_dmao_enable(
	struct mtk_camsv_device *dev, struct mtkcam_ipi_input_param *cfg_in_param);
int mtk_cam_sv_fbc_enable(
	struct mtk_camsv_device *dev, struct mtkcam_ipi_input_param *cfg_in_param);

int mtk_cam_sv_tg_disable(struct mtk_camsv_device *dev);
int mtk_cam_sv_top_disable(struct mtk_camsv_device *dev);
int mtk_cam_sv_dmao_disable(struct mtk_camsv_device *dev);
int mtk_cam_sv_fbc_disable(struct mtk_camsv_device *dev);
int mtk_cam_sv_vf_on(struct mtk_camsv_device *dev, unsigned int is_on);
int mtk_cam_sv_is_vf_on(struct mtk_camsv_device *dev);
int mtk_cam_sv_enquehwbuf(struct mtk_camsv_device *dev,
	dma_addr_t ba, unsigned int seq_no);
bool mtk_cam_sv_finish_buf(struct mtk_cam_request_stream_data *s_data);
int mtk_cam_find_sv_dev_index(struct mtk_cam_ctx *ctx, unsigned int idx);
int mtk_cam_sv_update_all_buffer_ts(struct mtk_cam_ctx *ctx, u64 ts_ns);
int mtk_cam_sv_apply_all_buffers(struct mtk_cam_ctx *ctx);
int mtk_cam_sv_apply_next_buffer(struct mtk_cam_ctx *ctx, unsigned int pipe_id, u64 ts_ns);
int mtk_cam_sv_rgbw_apply_next_buffer(
	struct mtk_cam_request_stream_data *s_data);
int mtk_cam_sv_apply_switch_buffers(struct mtk_cam_ctx *ctx);
int mtk_cam_sv_write_rcnt(struct mtk_cam_ctx *ctx, unsigned int pipe_id);
int mtk_cam_sv_cal_cfg_info(struct mtk_cam_ctx *ctx,
	const struct v4l2_format *img_fmt, struct mtk_camsv_frame_params *params);
int mtk_cam_sv_setup_cfg_info(struct mtk_camsv_device *dev,
	struct mtk_cam_request_stream_data *s_data);
#ifdef CAMSYS_TF_DUMP_71_1
int mtk_camsv_translation_fault_callback(int port, dma_addr_t mva, void *data);
#endif

extern struct platform_driver mtk_cam_sv_driver;

#endif
