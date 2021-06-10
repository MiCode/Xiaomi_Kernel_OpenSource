/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chris-YC Chen <chris-yc.chen@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk-cmdq.h>

#include "mtk-mml-color.h"
#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"

#include "smi_public.h"

/* mtk iommu */
#ifdef CONFIG_MTK_IOMMU_V2
#include "mtk_iommu_ext.h"
#include "mach/pseudo_m4u.h"
#endif


#define RDMA_EN				0x000
#define RDMA_UFBDC_DCM_EN		0x004
#define RDMA_RESET			0x008
#define RDMA_INTERRUPT_ENABLE		0x010
#define RDMA_INTERRUPT_STATUS		0x018
#define RDMA_CON			0x020
#define RDMA_GMCIF_CON			0x028
#define RDMA_SRC_CON			0x030
#define RDMA_COMP_CON			0x038
#define RDMA_PVRIC_CRYUVAL_0		0x040
#define RDMA_PVRIC_CRYUVAL_1		0x048
#define RDMA_PVRIC_CRCH0123VAL_0	0x050
#define RDMA_PVRIC_CRCH0123VAL_1	0x058
#define RDMA_MF_BKGD_SIZE_IN_BYTE	0x060
#define RDMA_MF_BKGD_SIZE_IN_PXL	0x068
#define RDMA_MF_SRC_SIZE		0x070
#define RDMA_MF_CLIP_SIZE		0x078
#define RDMA_MF_OFFSET_1		0x080
#define RDMA_MF_PAR			0x088
#define RDMA_SF_BKGD_SIZE_IN_BYTE	0x090
#define RDMA_MF_BKGD_H_SIZE_IN_PXL	0x098
#define RDMA_SF_PAR			0x0b8
#define RDMA_MB_DEPTH			0x0c0
#define RDMA_MB_BASE			0x0c8
#define RDMA_MB_CON			0x0d0
#define RDMA_SB_DEPTH			0x0d8
#define RDMA_SB_BASE			0x0e0
#define RDMA_SB_CON			0x0e8
#define RDMA_VC1_RANGE			0x0f0
#define RDMA_SRC_END_0			0x100
#define RDMA_SRC_END_1			0x108
#define RDMA_SRC_END_2			0x110
#define RDMA_SRC_OFFSET_0		0x118
#define RDMA_SRC_OFFSET_1		0x120
#define RDMA_SRC_OFFSET_2		0x128
#define RDMA_SRC_OFFSET_W_0		0x130
#define RDMA_SRC_OFFSET_W_1		0x138
#define RDMA_SRC_OFFSET_W_2		0x140
#define RDMA_SRC_OFFSET_WP		0x148
#define RDMA_SRC_OFFSET_HP		0x150
#define RDMA_TRANSFORM_0		0x200
#define RDMA_DMABUF_CON_0		0x240
#define RDMA_ULTRA_TH_HIGH_CON_0	0x248
#define RDMA_ULTRA_TH_LOW_CON_0		0x250
#define RDMA_DMABUF_CON_1		0x258
#define RDMA_ULTRA_TH_HIGH_CON_1	0x260
#define RDMA_ULTRA_TH_LOW_CON_1		0x268
#define RDMA_DMABUF_CON_2		0x270
#define RDMA_ULTRA_TH_HIGH_CON_2	0x278
#define RDMA_ULTRA_TH_LOW_CON_2		0x280
#define RDMA_DMABUF_CON_3		0x288
#define RDMA_ULTRA_TH_HIGH_CON_3	0x290
#define RDMA_ULTRA_TH_LOW_CON_3		0x298
#define RDMA_DITHER_CON			0x2a0
#define RDMA_RESV_DUMMY_0		0x2a8
#define RDMA_UNCOMP_MON			0x2c0
#define RDMA_COMP_MON			0x2c8
#define RDMA_CHKS_EXTR			0x300
#define RDMA_CHKS_INTW			0x308
#define RDMA_CHKS_INTR			0x310
#define RDMA_CHKS_ROTO			0x318
#define RDMA_CHKS_SRIY			0x320
#define RDMA_CHKS_SRIU			0x328
#define RDMA_CHKS_SRIV			0x330
#define RDMA_CHKS_SROY			0x338
#define RDMA_CHKS_SROU			0x340
#define RDMA_CHKS_SROV			0x348
#define RDMA_CHKS_VUPI			0x350
#define RDMA_CHKS_VUPO			0x358
#define RDMA_DEBUG_CON			0x380
#define RDMA_MON_STA_0			0x400
#define RDMA_MON_STA_1			0x408
#define RDMA_MON_STA_2			0x410
#define RDMA_MON_STA_3			0x418
#define RDMA_MON_STA_4			0x420
#define RDMA_MON_STA_5			0x428
#define RDMA_MON_STA_6			0x430
#define RDMA_MON_STA_7			0x438
#define RDMA_MON_STA_8			0x440
#define RDMA_MON_STA_9			0x448
#define RDMA_MON_STA_10			0x450
#define RDMA_MON_STA_11			0x458
#define RDMA_MON_STA_12			0x460
#define RDMA_MON_STA_13			0x468
#define RDMA_MON_STA_14			0x470
#define RDMA_MON_STA_15			0x478
#define RDMA_MON_STA_16			0x480
#define RDMA_MON_STA_17			0x488
#define RDMA_MON_STA_18			0x490
#define RDMA_MON_STA_19			0x498
#define RDMA_MON_STA_20			0x4a0
#define RDMA_MON_STA_21			0x4a8
#define RDMA_MON_STA_22			0x4b0
#define RDMA_MON_STA_23			0x4b8
#define RDMA_MON_STA_24			0x4c0
#define RDMA_MON_STA_25			0x4c8
#define RDMA_MON_STA_26			0x4d0
#define RDMA_MON_STA_27			0x4d8
#define RDMA_MON_STA_28			0x4e0
#define RDMA_SRC_BASE_0			0xf00
#define RDMA_SRC_BASE_1			0xf08
#define RDMA_SRC_BASE_2			0xf10
#define RDMA_UFO_DEC_LENGTH_BASE_Y	0xf20
#define RDMA_UFO_DEC_LENGTH_BASE_C	0xf28

enum rdma_label {
	RDMA_LABEL_BASE_0 = 0,
	RDMA_LABEL_BASE_1,
	RDMA_LABEL_BASE_2,
	RDMA_LABEL_BASE_END_0,
	RDMA_LABEL_BASE_END_1,
	RDMA_LABEL_BASE_END_2,
	RDMA_LABEL_UFO_DEC_BASE_Y,
	RDMA_LABEL_UFO_DEC_BASE_C,
	RDMA_LABEL_TOTAL
};

struct rdma_data {
	u32 tile_width;
};

static const struct rdma_data mt6893_rdma_data = {
	.tile_width = 640
};

struct mml_rdma {
	struct mml_comp comp;
	const struct rdma_data *data;

	u8 gpr_poll;
	u16 event_poll;
	u16 event_sof;
	u16 event_eof;

	/* mtk iommu */
	u8 larb;
	u32 m4u_port;
};

struct rdma_frame_data {
	u8 enable_ufo;
	u8 hw_fmt;
	u8 swap;
	u8 blk;
	u8 field;
	u8 blk_10bit;
	u8 blk_tile;
	u8 color_tran;
	u8 matrix_sel;
	u32 bits_per_pixel_y;
	u32 bits_per_pixel_uv;
	u32 hor_shift_uv;
	u32 ver_shift_uv;
	u32 vdo_blk_shift_w;
	u32 vdo_blk_height;
	u32 vdo_blk_shift_h;

	u16 label_array_idx[RDMA_LABEL_TOTAL];
};

#define rdma_frm_data(i)	((struct rdma_frame_data *)(i->data))

static s32 rdma_prepare(struct mml_comp *comp, struct mml_task *task,
			struct mml_comp_config *ccfg)
{
	ccfg->data = kzalloc(sizeof(struct rdma_frame_data), GFP_KERNEL);
	return 0;
}

static u32 rdma_get_label_count(struct mml_comp *comp, struct mml_task *task)
{
	return RDMA_LABEL_TOTAL;
}

static s32 rdma_init(struct mml_comp *comp, struct mml_task *task,
		     struct mml_comp_config *ccfg)
{
	struct mml_rdma *rdma = container_of(comp, struct mml_rdma, comp);
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;

	/* Reset engine */
	cmdq_pkt_wfe(pkt, rdma->event_poll);
	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_RESET, 0x00000001, 0x00000001);
	cmdq_pkt_poll(pkt, NULL, 0x00000100, base_pa + RDMA_MON_STA_1,
		      0x00000100, rdma->gpr_poll);
	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_RESET, 0x00000000, 0x00000001);
	cmdq_pkt_set_event(pkt, rdma->event_poll);
	return 0;
}

static void rdma_color_fmt(struct mml_frame_config *cfg,
			   struct rdma_frame_data *rdma_frm)
{
	u32 fmt = cfg->info.src.format;
	u16 profile_in = cfg->info.src.profile;

	rdma_frm->color_tran = 0;
	rdma_frm->matrix_sel = 0;

	rdma_frm->enable_ufo = MML_FMT_UFO(fmt);
	rdma_frm->hw_fmt = MML_FMT_HW_FORMAT(fmt);
	rdma_frm->swap = MML_FMT_SWAP(fmt);
	rdma_frm->blk = MML_FMT_BLOCK(fmt);
	rdma_frm->field = MML_FMT_INTERLACED(fmt);
	rdma_frm->blk_10bit = MML_FMT_10BIT_PACKED(fmt);
	rdma_frm->blk_tile = MML_FMT_10BIT_TILE(fmt);

	switch (fmt) {
	case MML_FMT_GREY:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		break;
	case MML_FMT_RGB565:
	case MML_FMT_BGR565:
		rdma_frm->bits_per_pixel_y = 16;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		rdma_frm->color_tran = 1;
		break;
	case MML_FMT_RGB888:
	case MML_FMT_BGR888:
		rdma_frm->bits_per_pixel_y = 24;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		rdma_frm->color_tran = 1;
		break;
	case MML_FMT_RGBA8888:
	case MML_FMT_BGRA8888:
	case MML_FMT_ARGB8888:
	case MML_FMT_ABGR8888:
	case MML_FMT_RGBA1010102:
	case MML_FMT_BGRA1010102:
	case MML_FMT_RGBA8888_AFBC:
	case MML_FMT_BGRA8888_AFBC:
	case MML_FMT_RGBA1010102_AFBC:
	case MML_FMT_BGRA1010102_AFBC:
		rdma_frm->bits_per_pixel_y = 32;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		rdma_frm->color_tran = 1;
		break;
	case MML_FMT_UYVY:
	case MML_FMT_VYUY:
	case MML_FMT_YUYV:
	case MML_FMT_YVYU:
		rdma_frm->bits_per_pixel_y = 16;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		break;
	case MML_FMT_I420:
	case MML_FMT_YV12:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 8;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 1;
		break;
	case MML_FMT_I422:
	case MML_FMT_YV16:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 8;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 0;
		break;
	case MML_FMT_I444:
	case MML_FMT_YV24:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 8;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		break;
	case MML_FMT_NV12:
	case MML_FMT_NV21:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 16;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 1;
		break;
	case MML_FMT_BLK_UFO:
	case MML_FMT_BLK_UFO_AUO:
	case MML_FMT_BLK:
		rdma_frm->vdo_blk_shift_w = 4;
		rdma_frm->vdo_blk_height = 32;
		rdma_frm->vdo_blk_shift_h = 5;
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 16;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 1;
		break;
	case MML_FMT_NV16:
	case MML_FMT_NV61:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 16;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 0;
		break;
	case MML_FMT_NV24:
	case MML_FMT_NV42:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 16;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		break;
	case MML_FMT_NV12_10L:
	case MML_FMT_NV21_10L:
		rdma_frm->bits_per_pixel_y = 16;
		rdma_frm->bits_per_pixel_uv = 32;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 1;
		break;
	case MML_FMT_YUV4441010102:
		rdma_frm->bits_per_pixel_y = 32;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		break;
	case MML_FMT_NV12_10P:
	case MML_FMT_NV21_10P:
		rdma_frm->bits_per_pixel_y = 10;
		rdma_frm->bits_per_pixel_uv = 20;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 1;
		break;
	case MML_FMT_BLK_10H:
	case MML_FMT_BLK_10V:
	case MML_FMT_BLK_10HJ:
	case MML_FMT_BLK_10VJ:
	case MML_FMT_BLK_UFO_10H:
	case MML_FMT_BLK_UFO_10V:
	case MML_FMT_BLK_UFO_10HJ:
	case MML_FMT_BLK_UFO_10VJ:
		rdma_frm->vdo_blk_shift_w = 4;
		rdma_frm->vdo_blk_height = 32;
		rdma_frm->vdo_blk_shift_h = 5;
		rdma_frm->bits_per_pixel_y = 10;
		rdma_frm->bits_per_pixel_uv = 20;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 1;
		break;
	default:
		mml_err("[rdma] not support format %x", fmt);
		break;
	}

	if (profile_in == MML_YCBCR_PROFILE_BT2020 ||
	    profile_in == MML_YCBCR_PROFILE_FULL_BT709 ||
	    profile_in == MML_YCBCR_PROFILE_FULL_BT2020)
		profile_in = MML_YCBCR_PROFILE_BT709;

	if (rdma_frm->color_tran == 1) {
		if (profile_in == MML_YCBCR_PROFILE_BT601)
			rdma_frm->matrix_sel = 2;
		else if (profile_in == MML_YCBCR_PROFILE_BT709)
			rdma_frm->matrix_sel = 3;
		else if (profile_in == MML_YCBCR_PROFILE_FULL_BT601)
			rdma_frm->matrix_sel = 0;
		else
			mml_err("[rdma] unknown color conversion %x",
				profile_in);
	}
}

static void add_label(struct mml_pipe_cache *cache, struct cmdq_pkt *pkt,
		      u16 *label_array, u32 label, u32 value)
{
	if (cache->label_idx >= cache->label_cnt) {
		mml_err("[rdma] out of label cnt idx %u count %u label %u",
			cache->label_idx, cache->label_cnt, label);
		return;
	}

	label_array[label] = cache->label_idx;
	cache->labels[cache->label_idx].offset = pkt->cmd_buf_size;
	cache->labels[cache->label_idx].val = value;
	cache->label_idx++;
}

static void update_label(struct mml_pipe_cache *cache,
			 struct rdma_frame_data *rdma_frm,
			 u32 label, u32 value)
{
	cache->labels[rdma_frm->label_array_idx[label]].val = value;
}

s32 check_setting(struct mml_file_buf *src_buf, struct mml_frame_data *src)
{
	s32 plane = MML_FMT_PLANE(src->format);
	/* block format error check */
	if (MML_FMT_BLOCK(src->format)) {
		if ((src->width & 0x0f) || (src->height & 0x1f)) {
			mml_err("invalid block format setting, width %u, height %u",
				src->width, src->height);
			mml_err("width should be 16 align, height should be 32 align");
			return -1;
		}

		/* 16-byte error */
		/* secure handle will not check */
		if (!src->secure && ((src_buf->iova[0] & 0x0f) ||
		    (plane > 1 && (src_buf->iova[1] & 0x0f)) ||
		    (plane > 2 && (src_buf->iova[2] & 0x0f)))) {
			mml_err("invalid block format setting, buffer %llu %llu %llu",
				src_buf->iova[0], src_buf->iova[1], src_buf->iova[2]);
			mml_err("buffer should be 16 align");
			return -1;
		}
		/* 128-byte warning for performance */
		if (!src->secure && ((src_buf->iova[0] & 0x7f) ||
		    (plane > 1 && (src_buf->iova[1] & 0x7f)) ||
		    (plane > 2 && (src_buf->iova[2] & 0x7f)))) {
			mml_log("warning: block format setting, buffer %llu %llu %llu\n",
				src_buf->iova[0], src_buf->iova[1], src_buf->iova[2]);
		}
	}

	if ((MML_FMT_PLANE(src->format) > 1) && src->uv_stride <= 0) {
		src->uv_stride = mml_color_get_min_uv_stride(src->format,
			src->width);
	}
	if ((plane == 1 && !src_buf->iova[0]) ||
	    (plane == 2 && (!src_buf->iova[1] || !src_buf->iova[0])) ||
	    (plane == 3 && (!src_buf->iova[2] || !src_buf->iova[1] || !src_buf->iova[0]))) {
		mml_err("configFrameRead buffer plane number error, color format = %#x, plane = %d",
			src->format, plane);
		return -1;
	}

	if (MML_FMT_H_SUBSAMPLE(src->format))
		src->width &= ~1;

	if (MML_FMT_V_SUBSAMPLE(src->format))
		src->height &= ~1;

	return 0;
}

static void calc_ufo(struct mml_file_buf *src_buf, struct mml_frame_data *src,
		     u32 *ufo_dec_length_y, u32 *ufo_dec_length_c,
		     u32 *u4pic_size_bs, u32 *u4pic_size_y_bs)
{
	u32 u4pic_size_y = src->width * src->height;
	u32 u4ufo_len_size_y =
		((((u4pic_size_y + 255) >> 8) + 63 + (16*8)) >> 6) << 6;
	u32 u4pic_size_c_bs;

	if (MML_FMT_10BIT_PACKED(src->format)) {
		if (MML_FMT_10BIT_JUMP(src->format)) {
			*u4pic_size_y_bs =
				(((u4pic_size_y * 5 >> 2) + 511) >> 9) << 9;
			*u4pic_size_bs =
				((*u4pic_size_y_bs + u4ufo_len_size_y + 4095) >>
				12) << 12;
			u4pic_size_c_bs =
				(((u4pic_size_y * 5 >> 3) + 63) >> 6) << 6;
		} else {
			*u4pic_size_y_bs =
				(((u4pic_size_y * 5 >> 2) + 4095) >> 12) << 12;
			u4pic_size_c_bs = u4pic_size_y * 5 >> 3;
			*u4pic_size_bs =
				((*u4pic_size_y_bs + u4pic_size_c_bs + 511) >>
				9) << 9;
		}
	} else {
		if (MML_FMT_AUO(src->format)) {
			u4ufo_len_size_y = u4ufo_len_size_y << 1;
			*u4pic_size_y_bs = ((u4pic_size_y + 511) >> 9) << 9;
			*u4pic_size_bs =
				((*u4pic_size_y_bs + u4ufo_len_size_y + 4095) >>
				12) << 12;
			u4pic_size_c_bs =
				(((u4pic_size_y >> 1) + 63) >> 6) << 6;
		} else {
			*u4pic_size_y_bs = ((u4pic_size_y + 4095) >> 12) << 12;
			u4pic_size_c_bs = u4pic_size_y >> 1;
			*u4pic_size_bs =
				((*u4pic_size_y_bs + u4pic_size_c_bs + 511) >>
				9) << 9;
		}
	}

	if (MML_FMT_10BIT_JUMP(src->format) || MML_FMT_AUO(src->format)) {
		/* Y YL C CL*/
		*ufo_dec_length_y = src_buf->iova[0] + src->plane_offset[0] +
				   *u4pic_size_y_bs;
		*ufo_dec_length_c = src_buf->iova[1] + src->plane_offset[1] +
				   u4pic_size_c_bs;
	} else {
		/* Y C YL CL */
		*ufo_dec_length_y = src_buf->iova[0] + src->plane_offset[0] +
				   *u4pic_size_bs;
		*ufo_dec_length_c = src_buf->iova[0] + src->plane_offset[0] +
				   *u4pic_size_bs + u4ufo_len_size_y;
	}
}

static s32 rdma_config_frame(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg)
{
	struct mml_rdma *rdma = container_of(comp, struct mml_rdma, comp);
	struct mml_frame_config *cfg = task->config;
	struct rdma_frame_data *rdma_frm = rdma_frm_data(ccfg);
	struct mml_file_buf *src_buf = &task->buf.src;
	struct mml_frame_data *src = &cfg->info.src;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];

	const phys_addr_t base_pa = comp->base_pa;
	u8 simple_mode = 1;
	u8 filterMode;
	u8 loose = 0;
	u8 bit_number = 0;
	u8 alpharot = 0;
	u8 auo = 0;
	u8 jump = 0;
	u8 afbc = 0;
	u8 afbc_y2r = 0;
	u8 hyfbc = 0;
	u8 ufbdc = 0;
	u32 write_mask = 0;
	u8 output_10bit = 0;
	u32 iova[3];
	u32 iova_end[3];
	u32 ufo_dec_length_y = 0;
	u32 ufo_dec_length_c = 0;
	u32 u4pic_size_bs = 0;
	u32 u4pic_size_y_bs = 0;

	if (cfg->alpharot)
		alpharot = 1;

	mml_msg("use config %p rdma %p", cfg, rdma);

	rdma_color_fmt(cfg, rdma_frm);

	if (MML_FMT_V_SUBSAMPLE(src->format) &&
	    !MML_FMT_V_SUBSAMPLE(cfg->info.dest[0].data.format) &&
	    !MML_FMT_BLOCK(src->format))
		/* 420 to 422 interpolation solution */
		filterMode = 2;
	else
		/* config.enRDMACrop ? 3 : 2 */
		/* RSZ uses YUV422, RDMA could use V filter unless cropping */
		filterMode = 3;

	if (cfg->alpharot) {
		rdma_frm->color_tran = 0;
	}

	if (rdma_frm->blk_10bit) {
		if (MML_FMT_IS_YUV(src->format)) {
			rdma_frm->matrix_sel = 0xf;
			rdma_frm->color_tran = 1;
			cmdq_pkt_write(pkt, NULL, base_pa + RDMA_RESV_DUMMY_0,
				       0x7, 0x00000007);
		}
	}
	else {
		cmdq_pkt_write(pkt, NULL, base_pa + RDMA_RESV_DUMMY_0,
			       0x0, 0x00000007);
	}
	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_GMCIF_CON, (1 <<  0) +
							    (7 <<  4) +
							    (1 << 16),
							    0x00030071);
	if (MML_FMT_IS_ARGB(src->format) &&
	    cfg->info.dest[0].pq_config.hdr_en && !cfg->info.dest[0].pq_config.aal_en &&
	    !cfg->info.dest[0].pq_config.tdshp_en)
		rdma_frm->color_tran = 0;

	if (MML_FMT_10BIT_LOOSE(src->format))
		loose = 1;
	if (MML_FMT_10BIT(src->format))
		bit_number = 1;

	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_SRC_CON,
		       (rdma_frm->hw_fmt <<  0) +
		       (filterMode << 9) +
		       (loose << 11) +
		       (rdma_frm->field << 12) +
		       (rdma_frm->swap << 14) +
		       (rdma_frm->blk << 15) +
		       (bit_number << 18) +
		       (rdma_frm->blk_tile << 23) +
		       (0 << 24) +
		       (alpharot << 25),
		       0x038cfe0f);

	write_mask |= 0xb0000000|0x0603000;
	if (rdma_frm->blk_10bit)
		jump = MML_FMT_10BIT_JUMP(src->format);
	else
		auo = MML_FMT_AUO(src->format);

	if (MML_FMT_COMPRESS(src->format)) {
		afbc = 1;
		if (MML_FMT_IS_ARGB(src->format))
			afbc_y2r = 1;
		ufbdc = 1;
		cmdq_pkt_write(pkt, NULL, base_pa + RDMA_MF_BKGD_SIZE_IN_PXL,
			       ((src->width + 31) >> 5) << 5,
			       0x001FFFFF);
		cmdq_pkt_write(pkt, NULL, base_pa + RDMA_MF_BKGD_H_SIZE_IN_PXL,
			       ((src->height + 7) >> 3) << 3,
			       0x001FFFFF);
	}
	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_COMP_CON,
		       (rdma_frm->enable_ufo << 31) +
		       (auo << 29) +
		       (jump << 28) +
		       (afbc << 22) +
		       (afbc_y2r << 21) +
		       (hyfbc << 13) +
		       (ufbdc << 12),
		       write_mask);

	if (src->secure) {
		/* TODO: for secure case setup plane offset and reg */
	}

	if (rdma_frm->enable_ufo) {
		calc_ufo(src_buf, src, &ufo_dec_length_y, &ufo_dec_length_c,
			 &u4pic_size_bs, &u4pic_size_y_bs);

		cmdq_pkt_write(pkt, NULL, base_pa + RDMA_UFO_DEC_LENGTH_BASE_Y,
			       ufo_dec_length_y, U32_MAX);
		add_label(cache, pkt, rdma_frm->label_array_idx,
			  RDMA_LABEL_UFO_DEC_BASE_Y, ufo_dec_length_y);
		cmdq_pkt_write(pkt, NULL, base_pa + RDMA_UFO_DEC_LENGTH_BASE_C,
			       ufo_dec_length_c, U32_MAX);
		add_label(cache, pkt, rdma_frm->label_array_idx,
			  RDMA_LABEL_UFO_DEC_BASE_C, ufo_dec_length_c);

		if (rdma_frm->blk_10bit)
			cmdq_pkt_write(pkt, NULL,
				       base_pa + RDMA_MF_BKGD_SIZE_IN_PXL,
				       (src->y_stride << 2) / 5,
				       0x001fffff);
	}

	if (MML_FMT_10BIT(src->format) ||
	    MML_FMT_10BIT(cfg->info.dest[0].data.format))
		output_10bit = 1;
	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_CON,
		       (rdma_frm->blk << 12) +
		       (output_10bit << 5) +
		       (simple_mode << 4),
		       0x00001130);

	/* Write frame base address */
	if (rdma_frm->enable_ufo) {
		if (MML_FMT_10BIT_JUMP(src->format) || MML_FMT_AUO(src->format)) {
			iova[0] = src_buf->iova[0] + src->plane_offset[0];
			iova[1] = src_buf->iova[0] + src->plane_offset[0] +
				  u4pic_size_bs;
			iova[2] = src_buf->iova[2] + src->plane_offset[2];
		} else {
			iova[0] = src_buf->iova[0] + src->plane_offset[0];
			iova[1] = src_buf->iova[0] + src->plane_offset[0] +
				  u4pic_size_y_bs;
			iova[2] = src_buf->iova[2] + src->plane_offset[2];
		}
	} else {
		iova[0] = src_buf->iova[0] + src->plane_offset[0];
		iova[1] = src_buf->iova[1] + src->plane_offset[1];
		iova[2] = src_buf->iova[2] + src->plane_offset[2];
	}
	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_SRC_BASE_0,
		       iova[0], U32_MAX);
	add_label(cache, pkt, rdma_frm->label_array_idx,
		  RDMA_LABEL_BASE_0, iova[0]);

	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_SRC_BASE_1,
		       iova[1], U32_MAX);
	add_label(cache, pkt, rdma_frm->label_array_idx,
		  RDMA_LABEL_BASE_1, iova[1]);

	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_SRC_BASE_2,
		       iova[2], U32_MAX);
	add_label(cache, pkt, rdma_frm->label_array_idx,
		  RDMA_LABEL_BASE_2, iova[2]);

	iova_end[0] = iova[0] + src_buf->size[0];
	iova_end[1] = iova[1] + src_buf->size[1];
	iova_end[2] = iova[2] + src_buf->size[2];
	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_SRC_END_0,
		       iova_end[0], U32_MAX);
	add_label(cache, pkt, rdma_frm->label_array_idx,
		  RDMA_LABEL_BASE_END_0, iova_end[0]);

	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_SRC_END_1,
		       iova_end[1], U32_MAX);
	add_label(cache, pkt, rdma_frm->label_array_idx,
		  RDMA_LABEL_BASE_END_1, iova_end[1]);

	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_SRC_END_2,
		       iova_end[2], U32_MAX);
	add_label(cache, pkt, rdma_frm->label_array_idx,
		  RDMA_LABEL_BASE_END_2, iova_end[2]);

	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_MF_BKGD_SIZE_IN_BYTE,
		       src->y_stride, 0x001FFFFF);
	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_SF_BKGD_SIZE_IN_BYTE,
		       src->uv_stride, 0x001FFFFF);

	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_TRANSFORM_0,
		       (rdma_frm->matrix_sel << 23) +
		       (rdma_frm->color_tran << 16), 0x0F810000);

	return 0;
}

static struct mml_tile_engine *rdma_get_tile(struct mml_frame_config *cfg,
					     struct mml_comp_config *ccfg,
					     u8 idx)
{
	struct mml_tile_engine *tile_engines =
		cfg->tile_output[ccfg->pipe]->tiles[idx].tile_engines;

	return &tile_engines[ccfg->node_idx];
}

static s32 rdma_config_tile(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg, u8 idx)
{
	struct mml_frame_config *cfg = task->config;
	struct rdma_frame_data *rdma_frm = rdma_frm_data(ccfg);
	struct mml_frame_data *src = &cfg->info.src;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	const phys_addr_t base_pa = comp->base_pa;

	struct mml_tile_engine *tile = rdma_get_tile(cfg,
		ccfg, idx);

	u32 src_offset_0;
	u32 src_offset_1;
	u32 src_offset_2;
	u32 mf_src_w;
	u32 mf_src_h;
	u32 mf_clip_w;
	u32 mf_clip_h;
	u32 mf_offset_w_1;
	u32 mf_offset_h_1;

	/* Following data retrieve from tile calc result */
	u32 in_xs = tile->in.xs;
	const u32 in_xe = tile->in.xe;
	u32 in_ys = tile->in.ys;
	const u32 in_ye = tile->in.ye;
	const u32 out_xs = tile->out.xs;
	const u32 out_xe = tile->out.xe;
	const u32 out_ys = tile->out.ys;
	const u32 out_ye = tile->out.ye;
	const u32 crop_ofst_x = tile->luma.x;
	const u32 crop_ofst_y = tile->luma.y;

	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_EN, 0x1, 0x00000001);

	if (rdma_frm->blk) {
		/* Alignment X left in block boundary */
		in_xs = ((in_xs >> rdma_frm->vdo_blk_shift_w) <<
			rdma_frm->vdo_blk_shift_w);
		/* Alignment Y top  in block boundary */
		in_ys = ((in_ys >> rdma_frm->vdo_blk_shift_h) <<
			rdma_frm->vdo_blk_shift_h);
	}

	if (MML_FMT_COMPRESS(src->format)) {
		cmdq_pkt_write(pkt, NULL, base_pa + RDMA_SRC_OFFSET_WP, in_xs,
			       0xFFFFFFFF);
		cmdq_pkt_write(pkt, NULL, base_pa + RDMA_SRC_OFFSET_HP, in_ys,
			       0xFFFFFFFF);
	}

	if (!rdma_frm->blk) {
		/* Set Y pixel offset */
		src_offset_0  = (in_xs * rdma_frm->bits_per_pixel_y >> 3) +
				in_ys * cfg->info.src.y_stride;

		/* Set U pixel offset */
		src_offset_1  = ((in_xs >> rdma_frm->hor_shift_uv) *
				rdma_frm->bits_per_pixel_uv >> 3) +
				(in_ys >> rdma_frm->ver_shift_uv) *
				cfg->info.src.uv_stride;

		/* Set V pixel offset */
		src_offset_2  = ((in_xs >> rdma_frm->hor_shift_uv) *
				rdma_frm->bits_per_pixel_uv >> 3) +
				(in_ys >> rdma_frm->ver_shift_uv) *
				cfg->info.src.uv_stride;

		/* Set source size */
		mf_src_w = in_xe - in_xs + 1;
		mf_src_h = in_ye - in_ys + 1;

		/* Set target size */
		mf_clip_w = out_xe - out_xs + 1;
		mf_clip_h = out_ye - out_ys + 1;

		/* Set crop offset */
		mf_offset_w_1 = crop_ofst_x;
		mf_offset_h_1 = crop_ofst_y;
	} else {
		/* Set Y pixel offset */
		src_offset_0 = (in_xs *
			       (rdma_frm->vdo_blk_height << rdma_frm->field) *
			       rdma_frm->bits_per_pixel_y >> 3) +
			       (out_ys >> rdma_frm->vdo_blk_shift_h) *
			       cfg->info.src.y_stride;

		/* Set 10bit UFO mode */
		if (MML_FMT_10BIT_PACKED(src->format) && rdma_frm->enable_ufo)
		{
			cmdq_pkt_write(pkt, NULL, base_pa + RDMA_SRC_OFFSET_WP,
				       (src_offset_0 << 2) / 5,
				       0xFFFFFFFF);
		}

		/* Set U pixel offset */
		src_offset_1 = ((in_xs >> rdma_frm->hor_shift_uv) *
			       ((rdma_frm->vdo_blk_height >>
			       rdma_frm->ver_shift_uv) << rdma_frm->field) *
			       rdma_frm->bits_per_pixel_uv >> 3) +
			       (out_ys >> rdma_frm->vdo_blk_shift_h) *
			       cfg->info.src.uv_stride;

		/* Set V pixel offset */
		src_offset_2 = ((in_xs >> rdma_frm->hor_shift_uv) *
			       ((rdma_frm->vdo_blk_height >>
			       rdma_frm->ver_shift_uv) << rdma_frm->field) *
			       rdma_frm->bits_per_pixel_uv >> 3) +
			       (out_ys >> rdma_frm->vdo_blk_shift_h) *
			       cfg->info.src.uv_stride;

		/* Set source size */
		mf_src_w      = in_xe - in_xs + 1;
		mf_src_h      = (in_ye - in_ys + 1) << rdma_frm->field;

		/* Set target size */
		mf_clip_w     = out_xe - out_xs + 1;
		mf_clip_h     = (out_ye - out_ys + 1) << rdma_frm->field;

		/* Set crop offset */
		mf_offset_w_1 = (out_xs - in_xs);
		mf_offset_h_1 = (out_ys  - in_ys) << rdma_frm->field;
	}
	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_SRC_OFFSET_0,
		       src_offset_0,
		       0xFFFFFFFF);

	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_SRC_OFFSET_1,
		       src_offset_1,
		       0xFFFFFFFF);

	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_SRC_OFFSET_2,
		       src_offset_2,
		       0xFFFFFFFF);

	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_MF_SRC_SIZE,
		       (mf_src_h << 16) + mf_src_w, 0x7FFF7FFF);

	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_MF_CLIP_SIZE,
		       (mf_clip_h << 16) + mf_clip_w, 0x7FFF7FFF);


	cmdq_pkt_write(pkt, NULL, base_pa + RDMA_MF_OFFSET_1,
		       (mf_offset_h_1 << 16) + mf_offset_w_1,
		       0x003F001F);
	return 0;
}

static s32 rdma_wait(struct mml_comp *comp, struct mml_task *task,
		     struct mml_comp_config *ccfg)
{
	struct mml_rdma *rdma = container_of(comp, struct mml_rdma, comp);
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	/* wait rdma frame done */
	cmdq_pkt_wfe(pkt, rdma->event_eof);
	/* Disable engine */
	cmdq_pkt_write(pkt, NULL, comp->base_pa + RDMA_EN, 0x0, 0x00000001);
	return 0;
}

static s32 rdma_reconfig_frame(struct mml_comp *comp, struct mml_task *task,
			       struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
 	struct rdma_frame_data *rdma_frm = rdma_frm_data(ccfg);
	struct mml_file_buf *src_buf = &task->buf.src;
	struct mml_frame_data *src = &cfg->info.src;
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];

	u32 iova[3];
	u32 iova_end[3];
	u32 ufo_dec_length_y = 0;
	u32 ufo_dec_length_c = 0;
	u32 u4pic_size_bs = 0;
	u32 u4pic_size_y_bs = 0;

	if (src->secure) {
		/* TODO: for secure case setup plane offset and reg */
	}

	if (rdma_frm->enable_ufo) {
		calc_ufo(src_buf, src, &ufo_dec_length_y, &ufo_dec_length_c,
			 &u4pic_size_bs, &u4pic_size_y_bs);

		update_label(cache, rdma_frm, RDMA_LABEL_UFO_DEC_BASE_Y,
			     ufo_dec_length_y);
		update_label(cache, rdma_frm, RDMA_LABEL_UFO_DEC_BASE_C,
			     ufo_dec_length_c);
	}

	/* Write frame base address */
	if (rdma_frm->enable_ufo) {
		if (MML_FMT_10BIT_JUMP(src->format) || MML_FMT_AUO(src->format)) {
			iova[0] = src_buf->iova[0] + src->plane_offset[0];
			iova[1] = src_buf->iova[0] + src->plane_offset[0] +
				  u4pic_size_bs;
			iova[2] = src_buf->iova[2] + src->plane_offset[2];
		} else {
			iova[0] = src_buf->iova[0] + src->plane_offset[0];
			iova[1] = src_buf->iova[0] + src->plane_offset[0] +
				  u4pic_size_y_bs;
			iova[2] = src_buf->iova[2] + src->plane_offset[2];
		}
	} else {
		iova[0] = src_buf->iova[0] + src->plane_offset[0];
		iova[1] = src_buf->iova[1] + src->plane_offset[1];
		iova[2] = src_buf->iova[2] + src->plane_offset[2];
	}

	update_label(cache, rdma_frm, RDMA_LABEL_BASE_0, iova[0]);
	update_label(cache, rdma_frm, RDMA_LABEL_BASE_1, iova[1]);
	update_label(cache, rdma_frm, RDMA_LABEL_BASE_2, iova[2]);

	iova_end[0] = iova[0] + src_buf->size[0];
	iova_end[1] = iova[1] + src_buf->size[1];
	iova_end[2] = iova[2] + src_buf->size[2];

	update_label(cache, rdma_frm, RDMA_LABEL_BASE_END_0, iova_end[0]);
	update_label(cache, rdma_frm, RDMA_LABEL_BASE_END_1, iova_end[1]);
	update_label(cache, rdma_frm, RDMA_LABEL_BASE_END_2, iova_end[2]);

	return 0;
}

static const struct mml_comp_config_ops rdma_cfg_ops = {
	.prepare = rdma_prepare,
	.get_label_count = rdma_get_label_count,
	.init = rdma_init,
	.frame = rdma_config_frame,
	.tile = rdma_config_tile,
	.wait = rdma_wait,
	.reframe = rdma_reconfig_frame,
};

static s32 rdma_pw_enable(struct mml_comp *comp)
{
	struct mml_rdma *rdma = container_of(comp, struct mml_rdma, comp);

	/* mtk iommu */
	mml_log("%s larb %hhu", __func__, rdma->larb);
	if (rdma->larb == 2)
		smi_bus_prepare_enable(SMI_LARB2, "MDP");
	else if (rdma->larb == 3)
		smi_bus_prepare_enable(SMI_LARB3, "MDP");
	else
		mml_err("[rdma] %s unknown larb %hhu", __func__, rdma->larb);

	return 0;
}

static s32 rdma_pw_disable(struct mml_comp *comp)
{
	struct mml_rdma *rdma = container_of(comp, struct mml_rdma, comp);

	/* mtk iommu */
	mml_log("%s larb %hhu", __func__, rdma->larb);
	if (rdma->larb == 2)
		smi_bus_disable_unprepare(SMI_LARB2, "MDP");
	else if (rdma->larb == 3)
		smi_bus_disable_unprepare(SMI_LARB3, "MDP");
	else
		mml_err("[rdma] %s unknown larb %hhu", __func__, rdma->larb);

	return 0;
}

static s32 rdma_clk_enable(struct mml_comp *comp)
{
	/* mtk iommu */
#ifdef CONFIG_MTK_IOMMU_V2
	struct mml_rdma *rdma = container_of(comp, struct mml_rdma, comp);
	struct M4U_PORT_STRUCT port = {
		.ePortID = rdma->m4u_port,
		.Virtuality = 1,
	};

	m4u_config_port(&port);
#endif

	return mml_comp_clk_enable(comp);
}

static const struct mml_comp_hw_ops rdma_hw_ops = {
	.pw_enable = &rdma_pw_enable,
	.pw_disable = &rdma_pw_disable,
	.clk_enable = &rdma_clk_enable,
	.clk_disable = &mml_comp_clk_disable,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_rdma *rdma = dev_get_drvdata(dev);
	s32 ret;

	ret = mml_register_comp(master, &rdma->comp);
	if (ret)
		dev_err(dev, "Failed to register mml component %s: %d\n",
			dev->of_node->full_name, ret);
	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_rdma *rdma = dev_get_drvdata(dev);

	mml_unregister_comp(master, &rdma->comp);
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static struct mml_rdma *dbg_probed_components[2];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_rdma *priv;
	s32 ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->data = (const struct rdma_data *)of_device_get_match_data(dev);

	ret = mml_comp_init(pdev, &priv->comp);
	if (ret) {
		dev_err(dev, "Failed to init mml component: %d\n", ret);
		return ret;
	}

	if (of_property_read_u8(dev->of_node, "gpr_poll", &priv->gpr_poll))
		dev_err(dev, "read gpr poll fail\n");

	if (of_property_read_u16(dev->of_node, "event_poll",
				 &priv->event_poll))
		dev_err(dev, "read event poll fail\n");

	if (of_property_read_u16(dev->of_node, "event_frame_start",
				 &priv->event_sof))
		dev_err(dev, "read sof event poll fail\n");

	if (of_property_read_u16(dev->of_node, "event_frame_done",
				 &priv->event_eof))
		dev_err(dev, "read event poll fail\n");

	/* mtk iommu */
	if (of_property_read_u8(dev->of_node, "larb", &priv->larb))
		dev_err(dev, "config larb fail\n");

	if (of_property_read_u32(dev->of_node, "m4u_port", &priv->m4u_port))
		dev_err(dev, "config m4u port fail\n");

	/* assign ops */
	priv->comp.config_ops = &rdma_cfg_ops;
	priv->comp.hw_ops = &rdma_hw_ops;

	dbg_probed_components[dbg_probed_count++] = priv;

	ret = component_add(dev, &mml_comp_ops);
	if (ret)
		dev_err(dev, "Failed to add component: %d\n", ret);

	return ret;
}

static int remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mml_comp_ops);
	return 0;
}

const struct of_device_id mtk_mml_rdma_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6893-mml_rdma",
	  .data = &mt6893_rdma_data},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_mml_rdma_driver_dt_match);

static struct platform_driver mtk_mml_rdma_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
			.name = "mediatek-mml-rdma",
			.owner = THIS_MODULE,
			.of_match_table = mtk_mml_rdma_driver_dt_match,
		},
};

//module_platform_driver(mtk_mml_rdma_driver);

static s32 ut_case;
static s32 ut_set(const char *val, const struct kernel_param *kp)
{
	s32 result;

	result = sscanf(val, "%d", &ut_case);
	if (result != 1) {
		mml_err("invalid input: %s, result(%d)", val, result);
		return -EINVAL;
	}
	mml_log("%s: case_id=%d", __func__, ut_case);

	switch (ut_case) {
	case 0:
		mml_log("use read to dump current pwm setting");
		break;
	default:
		mml_err("invalid case_id: %d", ut_case);
		break;
	}

	mml_log("%s END", __func__);
	return 0;
}

static s32 ut_get(char *buf, const struct kernel_param *kp)
{
	s32 length = 0;
	u32 i;

	switch (ut_case) {
	case 0:
		length += snprintf(buf + length, PAGE_SIZE - length,
			"[%d] probed count: %d\n", ut_case, dbg_probed_count);
		for(i = 0; i < dbg_probed_count; i++) {
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  - [%d] mml_comp_id: %d\n", i,
				dbg_probed_components[i]->comp.id);
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  -      mml_binded: %d\n",
				dbg_probed_components[i]->comp.bound);
		}
	default:
		mml_err("not support read for case_id: %d", ut_case);
		break;
	}
	buf[length] = '\0';

	return length;
}

static struct kernel_param_ops up_param_ops = {
	.set = ut_set,
	.get = ut_get,
};
module_param_cb(rdma_ut_case, &up_param_ops, NULL, 0644);
MODULE_PARM_DESC(rdma_ut_case, "mml rdma UT test case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML RDMA driver");
MODULE_LICENSE("GPL v2");
