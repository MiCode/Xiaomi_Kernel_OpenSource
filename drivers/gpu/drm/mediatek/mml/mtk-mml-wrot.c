/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/ioport.h>
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


/* WROT register offset */
#define VIDO_CTRL			0x00
#define VIDO_DMA_PERF			0x04
#define VIDO_MAIN_BUF_SIZE		0x08
#define VIDO_SOFT_RST			0x10
#define VIDO_SOFT_RST_STAT		0x14
#define VIDO_INT_EN			0x18
#define VIDO_INT			0x1c
#define VIDO_CROP_OFST			0x20
#define VIDO_TAR_SIZE			0x24
#define VIDO_FRAME_SIZE			0x28
#define VIDO_OFST_ADDR			0x2c
#define VIDO_STRIDE			0x30
#define VIDO_BKGD			0x34
#define VIDO_OFST_ADDR_C		0x38
#define VIDO_STRIDE_C			0x3c
#define VIDO_ISSUE_REQ_TH		0x40
#define VIDO_GROUP_REQ_TH		0x44
#define VIDO_CTRL_2			0x48
#define VIDO_IN_LINE_ROT		0x50
#define VIDO_DITHER			0x54
#define VIDO_OFST_ADDR_V		0x68
#define VIDO_STRIDE_V			0x6c
#define VIDO_RSV_1			0x70
#define VIDO_DMA_PREULTRA		0x74
#define VIDO_IN_SIZE			0x78
#define VIDO_ROT_EN			0x7c
#define VIDO_FIFO_TEST			0x80
#define VIDO_MAT_CTRL			0x84
#define VIDO_DEBUG			0xd0
#define VIDO_ARB_SW_CTL			0xd4
#define VIDO_PVRIC			0xd8
#define VIDO_SCAN_10BIT			0xdc
#define VIDO_PENDING_ZERO		0xe0
#define VIDO_PVRIC_SETTING		0xe4
#define VIDO_CRC_CTRL			0xe8
#define VIDO_CRC_VALUE			0xec
#define VIDO_BASE_ADDR			0xf00
#define VIDO_BASE_ADDR_C		0xf04
#define VIDO_BASE_ADDR_V		0xf08
#define VIDO_PVRIC_FMT			0xf0c
#define VIDO_FBC_FBDC_CR_CH0123_VAL0	0xf10
#define VIDO_FBC_FBDC_CR_CH0123_VAL1	0xf14
#define VIDO_FBC_FBDC_CR_Y_VAL0		0xf18
#define VIDO_FBC_FBDC_CR_UV_VAL0	0xf1c
#define VIDO_FBC_FBDC_CR_Y_VAL1		0xf20
#define VIDO_FBC_FBDC_CR_UV_VAL1	0xf24
#define VIDO_AFBC_VERSION		0xf28
#define VIDO_AFBC_YUVTRANS		0xf2c
#define VIDO_BUS_CTRL			0xf30

/* register mask */
#define VIDO_INT_MASK			0x07

/* ceil and floor helper macro */
#define ceil(n, d) ((u32)(((float)(n) / (d)) + ((n) % (d) != 0)))
#define floor(n, d) ((u32)((float)(n) / (d)))

enum wrot_label {
	WROT_LABEL_ADDR = 0,
	WROT_LABEL_ADDR_C,
	WROT_LABEL_ADDR_V,
	WROT_LABEL_TOTAL
};

struct wrot_data {
	u32 fifo;
	u32 tile_width;
};

struct wrot_data mt6893_wrot_data = {
	.fifo = 256,
	.tile_width = 512
};

struct mml_wrot {
	struct mml_comp comp;
	struct wrot_data *data;

	u8 gpr_poll;
	u16 event_poll;
	u16 event_eof;	/* wrot frame done */

	/* mtk iommu */
	u8 larb;
	u32 m4u_port;
};

/* meta data for each different frame config */
struct wrot_frame_data {
	/* 0 or 1 for 1st or 2nd out port */
	u8 out_idx;

	/* width and height before rotate */
	u32 out_w;
	u32 out_h;
	struct mml_rect compose;

	/* calculate in prepare and use as tile input */
	u32 out_x_off;
	u32 out_crop_w;

	/* following data calculate in init and use in tile command */
	u8 mat_en;
	u8 mat_sel;
	/* bits per pixel y */
	u32 bbp_y;
	/* bits per pixel uv */
	u32 bbp_uv;
	/* hor right shift uv */
	u32 hor_sh_uv;
	/* vert right shift uv */
	u32 ver_sh_uv;

	/* calculate in frame, use in each tile calc */
	u32 fifo_max_sz;
	u32 max_line_cnt;

	/* use in reuse command */
	u16 label_array_idx[WROT_LABEL_TOTAL];
};

#define wrot_frm_data(i)	((struct wrot_frame_data *)(i->data))

struct wrot_ofst_addr {
	u32 y;	/* wrot_y_ofst_adr for VIDO_OFST_ADDR */
	u32 c;	/* wrot_c_ofst_adr for VIDO_OFST_ADDR_C */
	u32 v;	/* wrot_v_ofst_adr for VIDO_OFST_ADDR_V */
};

/* different wrot setting between each tile */
struct wrot_setting {
	/* parameters for calculation */
	u32 tar_xsize;
	/* result settings */
	u32 main_blk_width;
	u32 main_buf_line_num;
};

struct check_buf_param {
	u32 y_buf_size;
	u32 uv_buf_size;
	u32 y_buf_check;
	u32 uv_buf_check;
	u32 y_buf_width;
	u32 y_buf_usage;
	u32 uv_blk_width;
	u32 uv_blk_line;
	u32 uv_buf_width;
	u32 uv_buf_usage;
};

/* filt_h, filt_v, uv_xsel, uv_ysel */
static const u32 uv_table[2][4][2][4] = {
	{	/* YUV422 */
		{    /* 0 */
			{ 1 /* [1 2 1] */, 0 /* drop  */, 0, 2 },
			{ 2 /* [1 2 1] */, 0 /* drop  */, 1, 2 }, /* flip */
		}, { /* 90 */
			{ 0 /* drop    */, 4 /* [1 1] */, 2, 1 },
			{ 0 /* drop    */, 3 /* [1 1] */, 2, 0 }, /* flip */
		}, { /* 180 */
			{ 2 /* [1 2 1] */, 0 /* drop  */, 1, 2 },
			{ 1 /* [1 2 1] */, 0 /* drop  */, 0, 2 }, /* flip */
		}, { /* 270 */
			{ 0 /* drop    */, 3 /* [1 1] */, 2, 0 },
			{ 0 /* drop    */, 4 /* [1 1] */, 2, 1 }, /* flip */
		},
	}, { /* YUV420 */
		{    /* 0 */
			{ 1 /* [1 2 1] */, 3 /* [1 1] */, 0, 0 },
			{ 2 /* [1 2 1] */, 3 /* [1 1] */, 1, 0 }, /* flip */
		}, { /* 90 */
			{ 1 /* [1 2 1] */, 4 /* [1 1] */, 0, 1 },
			{ 1 /* [1 2 1] */, 3 /* [1 1] */, 0, 0 }, /* flip */
		}, { /* 180 */
			{ 2 /* [1 2 1] */, 4 /* [1 1] */, 1, 1 },
			{ 1 /* [1 2 1] */, 4 /* [1 1] */, 0, 1 }, /* flip */
		}, { /* 270 */
			{ 2 /* [1 2 1] */, 3 /* [1 1] */, 1, 0 },
			{ 2 /* [1 2 1] */, 4 /* [1 1] */, 1, 1 }, /* flip */
		},
	}
};

s32 wrot_query_write(struct mml_frame_config *cfg, u8 out)
{
	struct mml_frame_dest *dest = &cfg->info.dest[out];

	if (dest->rotate == MML_ROT_90 || dest->rotate == MML_ROT_270) {
		if (MML_FMT_H_SUBSAMPLE(dest->data.format)) {
			dest->data.width &= ~1; /* WROT HW constraint */
			dest->data.height &= ~1;
		} else if (MML_FMT_V_SUBSAMPLE(dest->data.format)) {
			dest->data.width &= ~1;
		}

	        if (MML_FMT_PLANE(dest->data.format) > 1 &&
		    dest->data.uv_stride <= 0) {
			dest->data.uv_stride =
				mml_color_get_min_uv_stride(dest->data.format,
					dest->data.height);
		}
	} else {
		if (MML_FMT_H_SUBSAMPLE(dest->data.format))
			dest->data.width &= ~1;

	        if (MML_FMT_V_SUBSAMPLE(dest->data.format))
			dest->data.height &= ~1;

	        if ((MML_FMT_PLANE(dest->data.format) > 1) &&
		    dest->data.uv_stride <= 0) {
			dest->data.uv_stride =
				mml_color_get_min_uv_stride(dest->data.format,
					dest->data.width);
	        }
	}

	return 0;
}

static u32 wrot_get_out_w(struct mml_comp_config *ccfg)
{
	struct wrot_frame_data *wrot_frm = wrot_frm_data(ccfg);

	return wrot_frm->out_w;
}

static u32 wrot_get_out_h(struct mml_comp_config *ccfg)
{
	struct wrot_frame_data *wrot_frm = wrot_frm_data(ccfg);

	return wrot_frm->out_h;
}

static const struct mml_comp_tile_ops wrot_tile_ops = {
	.get_out_w = wrot_get_out_w,
	.get_out_h = wrot_get_out_h,
};

static bool is_change_wx(u16 r, bool f)
{
	return ((r == MML_ROT_0 && f) ||
		(r == MML_ROT_180 && !f) ||
		r == MML_ROT_270);
}

static void wrot_config_pipe0(struct mml_frame_dest *dest,
			      struct wrot_frame_data *wrot_frm)
{
	wrot_frm->out_x_off = 0;
	wrot_frm->out_crop_w = wrot_frm->out_h >> 1;

	if (MML_FMT_10BIT_PACKED(dest->data.format) &&
	    wrot_frm->out_crop_w & 3) {
		wrot_frm->out_crop_w = (wrot_frm->out_crop_w & ~3) + 4;
		if (is_change_wx(dest->rotate, dest->flip))
			wrot_frm->out_crop_w = wrot_frm->out_h -
					       wrot_frm->out_crop_w;

	} else if (wrot_frm->out_crop_w & 1) {
		wrot_frm->out_crop_w++;
	}
}

static void wrot_config_pipe1(struct mml_frame_dest *dest,
			      struct wrot_frame_data *wrot_frm)
{
	wrot_frm->out_x_off = wrot_frm->out_h >> 1;

	if (MML_FMT_10BIT_PACKED(dest->data.format) &&
	    wrot_frm->out_x_off & 3) {
		wrot_frm->out_x_off = (wrot_frm->out_x_off & ~3) + 4;
		if (is_change_wx(dest->rotate, dest->flip))
			wrot_frm->out_x_off = wrot_frm->out_h -
					      wrot_frm->out_x_off;

	} else if (wrot_frm->out_x_off & 1) {
		wrot_frm->out_x_off++;
	}

	wrot_frm->out_crop_w = wrot_frm->out_h - wrot_frm->out_x_off;
}

static s32 wrot_config_write(struct mml_comp *comp,
			     struct mml_task *task,
			     struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	const struct mml_topology_path *path = cfg->path[ccfg->pipe];
	struct wrot_frame_data *wrot_frm;
	struct mml_frame_dest *dest;
	u8 i;

	/* initialize component frame data for current frame config */
	wrot_frm = kzalloc(sizeof(*wrot_frm), GFP_KERNEL);
	ccfg->data = wrot_frm;
	for (i = 0; i < MML_MAX_OUTPUTS; i++) {
		if (comp->id == path->out_engine_ids[i]) {
			wrot_frm->out_idx = i;
			break;
		}
	}

	/* select output port struct */
	dest = &cfg->info.dest[wrot_frm->out_idx];

	wrot_frm->compose = dest->compose;
	if (dest->rotate == MML_ROT_0 || dest->rotate == MML_ROT_180) {
		wrot_frm->out_w = dest->data.width;
		wrot_frm->out_h = dest->data.height;
	} else {
		wrot_frm->out_w = dest->data.height;
		wrot_frm->out_h = dest->data.width;
		swap(wrot_frm->compose.width, wrot_frm->compose.height);
	}

	if (cfg->dual) {
		if (ccfg->pipe == 0)
			wrot_config_pipe0(dest, wrot_frm);
		else
			wrot_config_pipe1(dest, wrot_frm);
	}

	return 0;
}

static u32 wrot_get_label_count(struct mml_comp *comp, struct mml_task *task)
{
	return WROT_LABEL_TOTAL;
}

static s32 wrot_init(struct mml_comp *comp, struct mml_task *task,
		     struct mml_comp_config *ccfg)
{
	struct mml_wrot *wrot = container_of(comp, struct mml_wrot, comp);
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;

	/* Reset engine */
	cmdq_pkt_wfe(pkt, wrot->event_poll);
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_SOFT_RST, 0x1, 0x00000001);
	cmdq_pkt_poll(pkt, NULL, 1, base_pa + VIDO_SOFT_RST_STAT, 0x00000001,
		      wrot->gpr_poll);
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_SOFT_RST, 0, 0x00000001);
	cmdq_pkt_poll(pkt, NULL, 0, base_pa + VIDO_SOFT_RST_STAT, 0x00000001,
		      wrot->gpr_poll);
	cmdq_pkt_set_event(pkt, wrot->event_poll);

	return 0;
}

static void wrot_color_fmt(struct mml_frame_config *cfg,
			   struct wrot_frame_data *wrot_frm)
{
	u32 fmt = cfg->info.dest[wrot_frm->out_idx].data.format;
	u16 profile_in = cfg->info.src.profile;
	u16 profile_out = cfg->info.dest[wrot_frm->out_idx].data.profile;

	/* TODO: following mapColorFormat */

	wrot_frm->mat_en = 0;
	wrot_frm->mat_sel = 0;
	wrot_frm->bbp_y = MML_FMT_BITS_PER_PIXEL(fmt);

	switch (fmt) {
	case MML_FMT_GREY:
		/* Y only */
		wrot_frm->bbp_uv = 0;
		wrot_frm->hor_sh_uv = 0;
		wrot_frm->ver_sh_uv = 0;
		break;
	case MML_FMT_RGB565:
	case MML_FMT_BGR565:
	case MML_FMT_RGB888:
	case MML_FMT_BGR888:
	case MML_FMT_RGBA8888:
	case MML_FMT_BGRA8888:
	case MML_FMT_ARGB8888:
	case MML_FMT_ABGR8888:
	/* HW_SUPPORT_10BIT_PATH */
	case MML_FMT_RGBA1010102:
	case MML_FMT_BGRA1010102:
	case MML_FMT_ARGB1010102:
	case MML_FMT_ABGR1010102:
	/* DMA_SUPPORT_AFBC */
	case MML_FMT_RGBA8888_AFBC:
	case MML_FMT_BGRA8888_AFBC:
	case MML_FMT_RGBA1010102_AFBC:
	case MML_FMT_BGRA1010102_AFBC:
		wrot_frm->bbp_uv = 0;
		wrot_frm->hor_sh_uv = 0;
		wrot_frm->ver_sh_uv = 0;
		wrot_frm->mat_en = 1;
		break;
	case MML_FMT_UYVY:
	case MML_FMT_VYUY:
	case MML_FMT_YUYV:
	case MML_FMT_YVYU:
		/* YUV422, 1 plane */
		wrot_frm->bbp_uv = 0;
		wrot_frm->hor_sh_uv = 0;
		wrot_frm->ver_sh_uv = 0;
		break;
	case MML_FMT_I420:
	case MML_FMT_YV12:
		/* YUV420, 3 plane */
		wrot_frm->bbp_uv = 8;
		wrot_frm->hor_sh_uv = 1;
		wrot_frm->ver_sh_uv = 1;
		break;
	case MML_FMT_I422:
	case MML_FMT_YV16:
		/* YUV422, 3 plane */
		wrot_frm->bbp_uv = 8;
		wrot_frm->hor_sh_uv = 1;
		wrot_frm->ver_sh_uv = 0;
		break;
	case MML_FMT_I444:
	case MML_FMT_YV24:
		/* YUV444, 3 plane */
		wrot_frm->bbp_uv = 8;
		wrot_frm->hor_sh_uv = 0;
		wrot_frm->ver_sh_uv = 0;
		break;
	case MML_FMT_NV12:
	case MML_FMT_NV21:
		/* YUV420, 2 plane */
		wrot_frm->bbp_uv = 16;
		wrot_frm->hor_sh_uv = 1;
		wrot_frm->ver_sh_uv = 1;
		break;
	case MML_FMT_NV16:
	case MML_FMT_NV61:
		/* YUV422, 2 plane */
		wrot_frm->bbp_uv = 16;
		wrot_frm->hor_sh_uv = 1;
		wrot_frm->ver_sh_uv = 0;
		break;
	/* HW_SUPPORT_10BIT_PATH */
	case MML_FMT_NV12_10L:
	case MML_FMT_NV21_10L:
		/* P010 YUV420, 2 plane 10bit */
		wrot_frm->bbp_uv = 32;
		wrot_frm->hor_sh_uv = 1;
		wrot_frm->ver_sh_uv = 1;
		break;
	case MML_FMT_NV12_10P:
	case MML_FMT_NV21_10P:
		/* MTK packet YUV420, 2 plane 10bit */
		wrot_frm->bbp_uv = 20;
		wrot_frm->hor_sh_uv = 1;
		wrot_frm->ver_sh_uv = 1;
		break;
	default:
		mml_err("[wrot] not support format %x", fmt);
		break;
	}

	/*
	 * 4'b0000: RGB to JPEG
	 * 4'b0010: RGB to BT601
	 * 4'b0011: RGB to BT709
	 * 4'b0100: JPEG to RGB
	 * 4'b0110: BT601 to RGB
	 * 4'b0111: BT709 to RGB
	 * 4'b1000: JPEG to BT601
	 * 4'b1001: JPEG to BT709
	 * 4'b1010: BT601 to JPEG
	 * 4'b1011: BT709 to JPEG
	 * 4'b1100: BT709 to BT601
	 * 4'b1101: BT601 to BT709
	 */
	if (profile_in == MML_YCBCR_PROFILE_BT2020 ||
	    profile_in == MML_YCBCR_PROFILE_FULL_BT709 ||
	    profile_in == MML_YCBCR_PROFILE_FULL_BT2020)
		profile_in = MML_YCBCR_PROFILE_BT709;

	if (wrot_frm->mat_en == 1) {
		if (profile_in == MML_YCBCR_PROFILE_BT601)
			wrot_frm->mat_sel = 6;
		else if (profile_in == MML_YCBCR_PROFILE_BT709)
			wrot_frm->mat_sel = 7;
		else if (profile_in == MML_YCBCR_PROFILE_JPEG)
			wrot_frm->mat_sel = 4;
		else
			mml_err("unknown profile conversion %x", profile_in);
	} else {
		if (profile_in == MML_YCBCR_PROFILE_JPEG &&
		    profile_out == MML_YCBCR_PROFILE_BT601) {
			wrot_frm->mat_en = 1;
			wrot_frm->mat_sel = 8;
		}
		else if (profile_in == MML_YCBCR_PROFILE_JPEG &&
			 profile_out == MML_YCBCR_PROFILE_BT709) {
			wrot_frm->mat_en = 1;
			wrot_frm->mat_sel = 9;
		}
		else if (profile_in == MML_YCBCR_PROFILE_BT601 &&
			 profile_out == MML_YCBCR_PROFILE_JPEG) {
			wrot_frm->mat_en = 1;
			wrot_frm->mat_sel = 10;
		}
		else if (profile_in == MML_YCBCR_PROFILE_BT709 &&
			 profile_out == MML_YCBCR_PROFILE_JPEG) {
			wrot_frm->mat_en = 1;
			wrot_frm->mat_sel = 11;
		}
		else if (profile_in == MML_YCBCR_PROFILE_BT709 &&
			 profile_out == MML_YCBCR_PROFILE_BT601) {
			wrot_frm->mat_en = 1;
			wrot_frm->mat_sel = 12;
		}
		else if (profile_in == MML_YCBCR_PROFILE_BT601 &&
			 profile_out == MML_YCBCR_PROFILE_BT709) {
			wrot_frm->mat_en = 1;
			wrot_frm->mat_sel = 13;
		}
	}
}

static void calc_plane_offset(u32 left, u32 top,
			      u32 y_stride, u32 uv_stride,
			      u32 bbp_y, u32 bbp_uv,
			      u32 hor_sh_uv, u32 ver_sh_uv,
			      u32 *offset)
{
	if (!left && !top)
		return;

	offset[0] += (left * bbp_y >> 3) + (y_stride * top);

	offset[1] += (left >> hor_sh_uv) * (bbp_uv >> 3) +
		     (top >> ver_sh_uv) * uv_stride;

	offset[2] += (left >> hor_sh_uv) * (bbp_uv >> 3) +
		     (top >> hor_sh_uv) * uv_stride;
}

static void add_label(struct mml_pipe_cache *cache, struct cmdq_pkt *pkt,
		      u16 *label_array, u32 label, u32 value)
{
	if (cache->label_idx >= cache->label_cnt) {
		mml_err("[wrot] out of label cnt idx %u count %u label %u",
			cache->label_idx, cache->label_cnt, label);
		return;
	}

	label_array[label] = cache->label_idx;
	cache->labels[cache->label_idx].offset = pkt->cmd_buf_size;
	cache->labels[cache->label_idx].val = value;
	cache->label_idx++;
}

static void update_label(struct mml_pipe_cache *cache,
			 struct wrot_frame_data *wrot_frm,
			 u32 label, u32 value)
{
	cache->labels[wrot_frm->label_array_idx[label]].val = value;
}

static void calc_afbc_block(u32 bits_per_pixel, u32 y_stride, u32 vert_stride,
		     u64 *iova, u32 *offset,
		     u32 *block_x, u32 *addr_c, u32 *addr_v, u32 *addr)
{
	u32 block_y, header_sz;

	*block_x = ((y_stride << 3) / bits_per_pixel + 31) >> 5;
	block_y = (vert_stride + 7) >> 3;
	header_sz = ((((*block_x * block_y) << 4) + 1023) >> 10) << 10;

	*addr_c = (u32)iova[0] + offset[0];
	*addr_v = (u32)iova[2] + offset[2];
	*addr = *addr_c + header_sz;
}

static void wrot_calc_hw_buf_setting(struct mml_wrot *wrot,
				     struct mml_frame_config *cfg,
				     struct mml_frame_dest *dest,
				     struct wrot_frame_data *wrot_frm)
{
	const u32 h_sub = MML_FMT_H_SUBSAMPLE(dest->data.format);
	const u32 v_sub = MML_FMT_V_SUBSAMPLE(dest->data.format);

	if (h_sub && !v_sub) {
		if (MML_FMT_PLANE(dest->data.format) == 1) {
			wrot_frm->fifo_max_sz = wrot->data->tile_width * 32;
			wrot_frm->max_line_cnt = 32;
		} else {
			wrot_frm->fifo_max_sz = wrot->data->tile_width * 48;
			wrot_frm->max_line_cnt = 48;
		}
	} else if (h_sub && v_sub) {
		wrot_frm->fifo_max_sz = wrot->data->tile_width * 64;
		wrot_frm->max_line_cnt = 64;
	} else if (dest->data.format == MML_FMT_GREY) {
		wrot_frm->fifo_max_sz = wrot->data->tile_width * 64;
		wrot_frm->max_line_cnt = 64;
	} else if (MML_FMT_GROUP(dest->data.format) == 0) {
		if (cfg->alpharot) {
			wrot_frm->fifo_max_sz = wrot->data->tile_width * 16;
			wrot_frm->max_line_cnt = 16;
		} else {
			wrot_frm->fifo_max_sz = wrot->data->tile_width * 32;
			wrot_frm->max_line_cnt = 32;
		}
	} else {
		mml_err("%s fail set fifo max size, max line count for %#x",
			__func__, dest->data.format);
	}
}

static s32 wrot_config_frame(struct mml_comp *comp,
			     struct mml_task *task,
			     struct mml_comp_config *ccfg)
{
	struct mml_wrot *wrot = container_of(comp, struct mml_wrot, comp);
	struct mml_frame_config *cfg = task->config;
	struct wrot_frame_data *wrot_frm = wrot_frm_data(ccfg);
	struct mml_file_buf *dest_buf = &task->buf.dest[wrot_frm->out_idx];
	struct mml_frame_dest *dest = &cfg->info.dest[wrot_frm->out_idx];
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];

	const phys_addr_t base_pa = comp->base_pa;
	const u32 src_fmt = cfg->info.src.format;
	const u32 dest_fmt = dest->data.format;
	const u16 rotate = dest->rotate;
	const u8 flip = dest->flip ? 1 : 0;
	const u32 h_subsample = MML_FMT_H_SUBSAMPLE(dest->data.format);
	const u32 v_subsample = MML_FMT_V_SUBSAMPLE(dest_fmt);
	const u8 plane = MML_FMT_PLANE(dest_fmt);
	const u32 crop_en = 1;		/* always enable crop */
	const u8 ext_mat = 0;		/* not use for now */

	u32 hw_fmt = MML_FMT_HW_FORMAT(dest_fmt);
	u32 out_swap = MML_FMT_SWAP(dest_fmt);
	u32 uv_xsel, uv_ysel;
	u32 filt_v = 0, filt_h = 0;
	u32 preultra = 1;		/* always enable wrot pre-ultra */
	u32 scan_10bit = 0, bit_num = 0, pending_zero = 0, pvric = 0;

	wrot_color_fmt(cfg, wrot_frm);

	/* calculate for later config tile use */
	wrot_calc_hw_buf_setting(wrot, cfg, dest, wrot_frm);

	if (cfg->alpharot) {
		wrot_frm->mat_en = 0;

		/* TODO: check if still need this sw workaround */
		if (!MML_FMT_COMPRESS(src_fmt) || MML_FMT_10BIT(src_fmt))
			out_swap ^= MML_FMT_SWAP(src_fmt);
	}

	mml_msg("use config %p wrot %p", cfg, wrot);

	if (h_subsample == 1) {    /* YUV422/420 out */
		filt_v = MML_FMT_V_SUBSAMPLE(src_fmt) ||
			 MML_FMT_GROUP(src_fmt) == 2 ?
			 0 : uv_table[v_subsample][rotate][flip][1];
		uv_xsel = uv_table[v_subsample][rotate][flip][2];
		uv_ysel = uv_table[v_subsample][rotate][flip][3];
	} else if (dest_fmt == MML_FMT_GREY) {
		uv_xsel = 0;
		uv_ysel = 0;
	} else {
		uv_xsel = 2;
		uv_ysel = 2;
	}

	/* Note: check odd size roi_w & roi_h for uv_xsel/uv_ysel */
	if ((wrot_frm->compose.width & 0x1) && uv_xsel == 1)
		uv_xsel = 0;
	if ((wrot_frm->compose.height & 0x1) && uv_ysel == 1)
		uv_ysel = 0;

	/* Note: WROT not support UV swap */
	if (out_swap == 1 && MML_FMT_PLANE(dest_fmt) == 3) {
		swap(dest_buf->f[1], dest_buf->f[2]);
		swap(dest_buf->iova[1], dest_buf->iova[2]);
		swap(dest->data.plane_offset[1], dest->data.plane_offset[2]);
	}

	calc_plane_offset(wrot_frm->compose.left, wrot_frm->compose.top,
			  dest->data.y_stride, dest->data.uv_stride,
			  wrot_frm->bbp_y, wrot_frm->bbp_uv,
			  wrot_frm->hor_sh_uv, wrot_frm->ver_sh_uv,
			  dest->data.plane_offset);

	if (dest->data.secure) {
		/* TODO: for secure case setup plane offset and reg */
	}

	/* DMA_SUPPORT_AFBC */
	if (MML_FMT_COMPRESS(dest_fmt)) {
		u32 block_x, addr_c, addr_v, addr;

		/* Write frame base address */
		calc_afbc_block(wrot_frm->bbp_y,
				dest->data.y_stride,
				dest->data.vert_stride,
				dest_buf->iova,
				dest->data.plane_offset,
				&block_x,
				&addr_c,
				&addr_v,
				&addr);

		/* Write frame base address */
		cmdq_pkt_write(pkt, NULL, base_pa + VIDO_BASE_ADDR,
			       addr, U32_MAX);
		add_label(cache, pkt, wrot_frm->label_array_idx,
			  WROT_LABEL_ADDR, addr);

		cmdq_pkt_write(pkt, NULL, base_pa + VIDO_BASE_ADDR_C,
			       addr_c, U32_MAX);
		add_label(cache, pkt, wrot_frm->label_array_idx,
			  WROT_LABEL_ADDR_C, addr_c);

		cmdq_pkt_write(pkt, NULL, base_pa + VIDO_BASE_ADDR_V,
			       addr_v, U32_MAX);
		add_label(cache, pkt, wrot_frm->label_array_idx,
			  WROT_LABEL_ADDR_V, addr_v);

		/* TODO: cmdq should return label of above 3 write */

		if (dest->rotate == MML_ROT_0 || dest->rotate == MML_ROT_180) {
			u32 out_height = ((((wrot_frm->out_h + 31) >>
					 5) << 5) << 16) +
					 ((block_x << 5) <<  0);

			cmdq_pkt_write(pkt, NULL, base_pa + VIDO_FRAME_SIZE,
				       out_height, U32_MAX);
		} else {
			u32 out_height = ((((wrot_frm->out_w + 31) >>
					 5) << 5) << 16) +
					 ((block_x << 5) <<  0);

			cmdq_pkt_write(pkt, NULL, base_pa + VIDO_FRAME_SIZE,
				       out_height, U32_MAX);
		}

		cmdq_pkt_write(pkt, NULL, base_pa + VIDO_AFBC_YUVTRANS,
			       MML_FMT_IS_ARGB(dest_fmt), 0x1);
	} else {
		u32 iova[3] = {
			dest_buf->iova[0] + dest->data.plane_offset[0],
			dest_buf->iova[1] + dest->data.plane_offset[1],
			dest_buf->iova[2] + dest->data.plane_offset[2],
		};

		/* Write frame base address */
		cmdq_pkt_write(pkt, NULL, base_pa + VIDO_BASE_ADDR,
			       iova[0], U32_MAX);
		add_label(cache, pkt, wrot_frm->label_array_idx,
			  WROT_LABEL_ADDR, iova[0]);

		cmdq_pkt_write(pkt, NULL, base_pa + VIDO_BASE_ADDR_C,
			       iova[1], U32_MAX);
		add_label(cache, pkt, wrot_frm->label_array_idx,
			  WROT_LABEL_ADDR_C, iova[1]);

		cmdq_pkt_write(pkt, NULL, base_pa + VIDO_BASE_ADDR_V,
			       iova[2], U32_MAX);
		add_label(cache, pkt, wrot_frm->label_array_idx,
			  WROT_LABEL_ADDR_V, iova[2]);
	}

	/* Write frame related registers */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_CTRL,
		       (uv_ysel		<< 30) +
		       (uv_xsel		<< 28) +
		       (flip		<< 24) +
		       (dest->rotate	<< 20) +
		       (cfg->alpharot	<< 16) + /* alpha rot */
		       (preultra	<< 14) + /* pre-ultra */
		       (crop_en		<< 12) +
		       (out_swap	<<  8) +
		       (hw_fmt		<<  0), 0xf131510f);

	if (MML_FMT_10BIT_LOOSE(dest_fmt)) {
		scan_10bit = 1;
		bit_num = 1;
	} else if (MML_FMT_10BIT_PACKED(dest_fmt)) {
		if (MML_FMT_IS_ARGB(dest_fmt))
			scan_10bit = 1;
		else
			scan_10bit = 3;
		pending_zero = 1;
		bit_num = 1;
	}

	/* DMA_SUPPORT_AFBC */
	if (MML_FMT_COMPRESS(dest_fmt)) {
		scan_10bit = 0;
		pending_zero = 1;
	}

	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_SCAN_10BIT, scan_10bit,
		       0x0000000f);
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_PENDING_ZERO,
		       pending_zero << 26, 0x04000000);
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_CTRL_2, bit_num,
		       0x00000007);

	if (MML_FMT_COMPRESS(dest->data.format)) {
		pvric = pvric | BIT(0);
		if (MML_FMT_10BIT(dest->data.format))
			pvric = pvric | BIT(1);
	}
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_PVRIC, pvric, 0x3);

	/* set ESL */
	if (plane == 3 || plane == 2 || hw_fmt == 7) {
		/* 3-plane, 2-plane, Y8 */
		preultra = (216 << 12) + (196 << 0);
	} else if(hw_fmt == 0 || hw_fmt == 1) {
		/* RGB */
		preultra = (136 << 12) + (76 << 0);
	} else if (hw_fmt == 2 || hw_fmt == 3) {
		/* ARGB */
		preultra = (96 << 12) + (16 << 0);
	} else if (hw_fmt == 4 || hw_fmt == 5) {
		/* UYVY */
		preultra = (176 << 12) + (136 << 0);
	} else {
		preultra = 0;
	}

	if (preultra)
		cmdq_pkt_write(pkt, NULL, base_pa + VIDO_DMA_PREULTRA,
			       preultra, 0x00ffffff);

	/* Write frame Y stride */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_STRIDE, dest->data.y_stride,
		       0x0000ffff);

	/* Write frame UV stride */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_STRIDE_C,
		       dest->data.uv_stride, 0x0000ffff);
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_STRIDE_V,
		       dest->data.uv_stride, 0x0000ffff);

	/* Write matrix control */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_MAT_CTRL,
		       (wrot_frm->mat_sel << 4) +
		       (ext_mat << 1) +
		       (wrot_frm->mat_en << 0), 0x000000f3);

	/* Set the fixed ALPHA as 0xff */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_DITHER, 0xff000000,
		       0xff000000);

	/* Set VIDO_EOL_SEL */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_RSV_1, 0x80000000,
		       0x80000000);

	/* Set VIDO_FIFO_TEST */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_FIFO_TEST, wrot->data->fifo,
		       0x00000fff);

	/* Filter Enable */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_MAIN_BUF_SIZE,
		       (filt_v << 4) + (filt_h << 0), 0x00000077);

	/* turn off WROT dma dcm */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_ROT_EN,
		       (0x1 << 23) + (0x1 << 20), 0x00900000);

	return 0;
}

static struct mml_tile_engine *wrot_get_tile(struct mml_frame_config *cfg,
					     struct mml_comp_config *ccfg,
					     u8 idx)
{
	struct mml_tile_engine *tile_engines =
		cfg->tile_output[ccfg->pipe]->tiles[idx].tile_engines;

	return &tile_engines[ccfg->node_idx];
}

static void wrot_tile_calc_comp(const struct mml_frame_dest *dest,
				const struct wrot_frame_data *wrot_frm,
				const struct mml_tile_engine *tile,
				struct wrot_ofst_addr *ofst)
{
	/* Following data retrieve from tile calc result */
	const u32 out_xs = tile->out.xs;

	if (dest->rotate == MML_ROT_0 && !dest->flip)
	{
		/* Target Y offset */
		ofst->y = out_xs * 32;

		/*
		 * Target U offset
		 * RGBA: 64, YV12 8-bit: 24, 10-bit: 32
		 */
		ofst->c = ofst->y / 64;

		mml_msg("%s No flip and no rotation: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	}
	else if (dest->rotate == MML_ROT_0 && dest->flip)
	{
		/* Target Y offset */
		ofst->y = (wrot_frm->out_w - out_xs - 32) * 32;

		/* Target U offset */
		ofst->c = ofst->y / 64;

		mml_msg("%s Flip without rotation: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	}
	else if (dest->rotate == MML_ROT_90 && !dest->flip)
	{
		/* Target Y offset */
		ofst->y = ((out_xs / 8) + 1) *
			  ((dest->data.y_stride / 128) - 1) * 1024;

		/* Target U offset */
		ofst->c = ofst->y / 64;

		mml_msg("%s Rotate 90 degree only: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	}
	else if (dest->rotate == MML_ROT_90 && dest->flip)
	{
		/* Target Y offset */
		ofst->y = (out_xs / 8) * (dest->data.y_stride / 128) * 1024;

		/* Target U offset */
		ofst->c = ofst->y / 64;

		mml_msg("%s Flip and Rotate 90 degree: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	}
	else if (dest->rotate == MML_ROT_180 && !dest->flip)
	{
		/* Target Y offset */
		ofst->y = (((wrot_frm->out_h / 8) - 1) *
			  (dest->data.y_stride / 128) +
			  ((wrot_frm->out_w / 32) - (out_xs / 32) - 1)) *
			  1024;

		/* Target U offset */
		ofst->c = ofst->y / 64;

		mml_msg("%s Rotate 180 degree only: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	}
	else if (dest->rotate == MML_ROT_180 && dest->flip)
	{
		/* Target Y offset */
		ofst->y = (((wrot_frm->out_h / 8) - 1) *
			  (dest->data.y_stride / 128) +
			  (out_xs / 32)) * 1024;

		/* Target U offset */
		ofst->c = ofst->y / 64;

		mml_msg("%s Flip and Rotate 180 degree: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	}
	else if (dest->rotate == MML_ROT_270 && !dest->flip)
	{
		/* Target Y offset */
		ofst->y = ((wrot_frm->out_w / 8) - (out_xs / 8) - 1) *
			  (dest->data.y_stride / 128) * 1024;

		/* Target U offset */
		ofst->c = ofst->y / 64;

		mml_msg("%s Rotate 270 degree only: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	}
	else if (dest->rotate == MML_ROT_270 && dest->flip)
	{
		/* Target Y offset */
		ofst->y = ((wrot_frm->out_w / 8) - (out_xs / 8)) *
			  ((dest->data.y_stride / 128) - 1) * 1024;

		/* Target U offset */
		ofst->c = ofst->y / 64;

		mml_msg("%s Flip and Rotate 270 degree: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	}
}

static void wrot_tile_calc(const struct mml_frame_dest *dest,
			   const struct wrot_frame_data *wrot_frm,
			   const struct mml_tile_engine *tile,
			   struct wrot_ofst_addr *ofst)
{
	/* Following data retrieve from tile calc result */
	const u32 out_xs = tile->out.xs;
	const u32 out_ys = tile->out.ys;

	if (dest->rotate == MML_ROT_0 && !dest->flip)
	{
		/* Target Y offset */
		ofst->y = out_ys * dest->data.y_stride +
			  (out_xs * wrot_frm->bbp_y >> 3);

		/* Target U offset */
		ofst->c = (out_ys >> wrot_frm->ver_sh_uv) *
			  dest->data.uv_stride +
			  ((out_xs >> wrot_frm->hor_sh_uv) *
			  wrot_frm->bbp_uv >> 3);

		/* Target V offset */
		ofst->v = (out_ys >> wrot_frm->ver_sh_uv) *
			  dest->data.uv_stride +
			  ((out_xs >> wrot_frm->hor_sh_uv) *
			  wrot_frm->bbp_uv >> 3);

		mml_msg("%s No flip and no rotation: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	}
	else if (dest->rotate == MML_ROT_0 && dest->flip)
	{
		/* Target Y offset */
		ofst->y = out_ys * dest->data.y_stride +
			  ((wrot_frm->out_w - out_xs) *
			  wrot_frm->bbp_y >> 3) - 1;

		/* Target U offset */
		ofst->c = (out_ys >> wrot_frm->ver_sh_uv) *
			  dest->data.uv_stride +
			  (((wrot_frm->out_w - out_xs) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;

		/* Target V offset */
		ofst->v = (out_ys >> wrot_frm->ver_sh_uv) *
			  dest->data.uv_stride +
			  (((wrot_frm->out_w - out_xs) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;

		mml_msg("%s Flip without rotation: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	}
	else if (dest->rotate == MML_ROT_90 && !dest->flip)
	{
		/* Target Y offset */
		ofst->y = out_xs * dest->data.y_stride +
			  ((wrot_frm->out_h - out_ys) *
			  wrot_frm->bbp_y >> 3) - 1;

		/* Target U offset */
		ofst->c = (out_xs >> wrot_frm->ver_sh_uv) *
			  dest->data.uv_stride +
			  (((wrot_frm->out_h - out_ys) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;

		/* Target V offset */
		ofst->v = (out_xs >> wrot_frm->ver_sh_uv) *
			  dest->data.uv_stride +
			  (((wrot_frm->out_h - out_ys) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;

		mml_msg("%s Rotate 90 degree only: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	}
	else if (dest->rotate == MML_ROT_90 && dest->flip)
	{
		/* Target Y offset */
		ofst->y = out_xs * dest->data.y_stride +
			  (out_ys * wrot_frm->bbp_y >> 3);

		/* Target U offset */
		ofst->c = (out_xs >> wrot_frm->ver_sh_uv) *
			  dest->data.uv_stride +
			  ((out_ys >> wrot_frm->hor_sh_uv) *
			  wrot_frm->bbp_uv >> 3);

		/* Target V offset */
		ofst->v = (out_xs >> wrot_frm->ver_sh_uv) *
			  dest->data.uv_stride +
			  ((out_ys >> wrot_frm->hor_sh_uv) *
			  wrot_frm->bbp_uv >> 3);

		mml_msg("%s Flip and Rotate 90 degree: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	}
	else if (dest->rotate == MML_ROT_180 && !dest->flip)
	{
		/* Target Y offset */
		ofst->y = (wrot_frm->out_h - out_ys - 1) *
			  dest->data.y_stride +
			  ((wrot_frm->out_w - out_xs) *
			  wrot_frm->bbp_y >> 3) - 1;

		/* Target U offset */
		ofst->c = ((wrot_frm->out_h - out_ys - 1) >>
			  wrot_frm->ver_sh_uv) * dest->data.uv_stride +
			  (((wrot_frm->out_w - out_xs) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;

		/* Target V offset */
		ofst->v = ((wrot_frm->out_h - out_ys - 1) >>
			  wrot_frm->ver_sh_uv) * dest->data.uv_stride +
			  (((wrot_frm->out_w - out_xs) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;

		mml_msg("%s Rotate 180 degree only: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	}
	else if (dest->rotate == MML_ROT_180 && dest->flip)
	{
		/* Target Y offset */
		ofst->y = (wrot_frm->out_h - out_ys - 1) *
			  dest->data.y_stride +
			  (out_xs * wrot_frm->bbp_y >> 3);

		/* Target U offset */
		ofst->c = ((wrot_frm->out_h - out_ys - 1) >>
			  wrot_frm->ver_sh_uv) * dest->data.uv_stride +
			  ((out_xs >> wrot_frm->hor_sh_uv) *
			  wrot_frm->bbp_uv >> 3);

		/* Target V offset */
		ofst->v = ((wrot_frm->out_h - out_ys - 1) >>
			  wrot_frm->ver_sh_uv) * dest->data.uv_stride +
			  ((out_xs >> wrot_frm->hor_sh_uv) *
			  wrot_frm->bbp_uv >> 3);

		mml_msg("%s Flip and Rotate 180 degree: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	}
	else if (dest->rotate == MML_ROT_270 && !dest->flip)
	{
		/* Target Y offset */
		ofst->y = (wrot_frm->out_w - out_xs - 1) *
			  dest->data.y_stride +
			  (out_ys * wrot_frm->bbp_y >> 3);

		/* Target U offset */
		ofst->c = ((wrot_frm->out_w - out_xs - 1) >>
			  wrot_frm->ver_sh_uv) * dest->data.uv_stride +
			  ((out_ys >> wrot_frm->hor_sh_uv) *
			  wrot_frm->bbp_uv >> 3);

		/* Target V offset */
		ofst->v = ((wrot_frm->out_w - out_xs - 1) >>
			  wrot_frm->ver_sh_uv) * dest->data.uv_stride +
			  ((out_ys >> wrot_frm->hor_sh_uv) *
			  wrot_frm->bbp_uv >> 3);

		mml_msg("%s Rotate 270 degree only: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	}
	else if (dest->rotate == MML_ROT_270 && dest->flip)
	{
		/* Target Y offset */
		ofst->y = (wrot_frm->out_w - out_xs - 1) *
			  dest->data.y_stride +
			  ((wrot_frm->out_h - out_ys) *
			  wrot_frm->bbp_y >> 3) - 1;

		/* Target U offset */
		ofst->c = ((wrot_frm->out_w - out_xs - 1) >>
			  wrot_frm->ver_sh_uv) * dest->data.uv_stride +
			  (((wrot_frm->out_h - out_ys) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;

		/* Target V offset */
		ofst->v = ((wrot_frm->out_w - out_xs - 1) >>
			  wrot_frm->ver_sh_uv) * dest->data.uv_stride +
			  (((wrot_frm->out_h - out_ys) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;

		mml_msg("%s Flip and Rotate 270 degree: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	}
}

static void wrot_check_buf(const struct mml_frame_dest *dest,
			   struct wrot_setting *setting,
			   struct check_buf_param *buf)
{
	/* Checking Y buffer usage
	 * y_buf_width is just larger than main_blk_width
	 */
	buf->y_buf_width = ceil(setting->main_blk_width,
				setting->main_buf_line_num) *
			   setting->main_buf_line_num;
	buf->y_buf_usage = buf->y_buf_width * setting->main_buf_line_num;
	if (buf->y_buf_usage > buf->y_buf_size) {
		setting->main_buf_line_num = setting->main_buf_line_num - 4;
		buf->y_buf_check = 0;
		buf->uv_buf_check = 0;
		return;
	} else {
		buf->y_buf_check = 1;
	}

	/* Checking UV buffer usage */
	if (MML_FMT_H_SUBSAMPLE(dest->data.format) == 0) {
		buf->uv_blk_width = setting->main_blk_width;
		buf->uv_blk_line = setting->main_buf_line_num;
	} else {
		/* MML_FMT_H_SUBSAMPLE(dest->data.format) == 1 */
		if (MML_FMT_V_SUBSAMPLE(dest->data.format) == 0) {
			/* YUV422 */
			if (dest->rotate == MML_ROT_0 ||
			    dest->rotate == MML_ROT_180) {
				buf->uv_blk_width =
					setting->main_blk_width >> 1;
				buf->uv_blk_line = setting->main_buf_line_num;
			} else {
				buf->uv_blk_width = setting->main_blk_width;
				buf->uv_blk_line =
					setting->main_buf_line_num >> 1;
			}
		} else {
			/* MML_FMT_V_SUBSAMPLE(dest->data.format) == 1
			 * YUV420
			 */
			buf->uv_blk_width = setting->main_blk_width >> 1;
			buf->uv_blk_line = setting->main_buf_line_num >> 1;
		}
	}

	buf->uv_buf_width = ceil(buf->uv_blk_width, buf->uv_blk_line) *
			    buf->uv_blk_line;
	buf->uv_buf_usage = buf->uv_buf_width * buf->uv_blk_line;
	if (buf->uv_buf_usage > buf->uv_buf_size) {
		setting->main_buf_line_num = setting->main_buf_line_num - 4;
		buf->y_buf_check = 0;
		buf->uv_buf_check = 0;
	} else {
		buf->uv_buf_check = 1;
	}
}

static void wrot_calc_setting(struct mml_wrot *wrot,
			      const struct mml_frame_dest *dest,
			      const struct wrot_frame_data *wrot_frm,
			      struct wrot_setting *setting)
{
	u32 hw_fmt = MML_FMT_HW_FORMAT(dest->data.format);
	u32 tile_width = wrot->data->tile_width;
	u32 coeff1, coeff2, temp;
	struct check_buf_param buf = {0};

	if (hw_fmt == 9 || hw_fmt == 13) {
		buf.y_buf_size = tile_width * 48;
		buf.uv_buf_size = tile_width / 2 * 48;
	} else if (hw_fmt == 8 || hw_fmt == 12) {
		buf.y_buf_size = tile_width * 64;
		buf.uv_buf_size = tile_width / 2 * 32;
	} else {
		buf.y_buf_size = tile_width * 32;
		buf.uv_buf_size = tile_width * 32;
	}

	/* Default value */
	setting->main_buf_line_num = 0;
	/* Allocate FIFO buffer */
	setting->main_blk_width = setting->tar_xsize;

	coeff1 = floor(wrot_frm->fifo_max_sz, setting->tar_xsize * 2) * 2;
	coeff2 = ceil(setting->tar_xsize, coeff1);
	temp = ceil(setting->tar_xsize, coeff2 * 4) * 4;

	if (temp > setting->tar_xsize)
		setting->main_buf_line_num = ceil(setting->tar_xsize, 4) * 4;
	else
		setting->main_buf_line_num = temp;
	if (setting->main_buf_line_num > wrot_frm->max_line_cnt)
		setting->main_buf_line_num = wrot_frm->max_line_cnt;

	/* check for internal buffer size */
	while (!buf.y_buf_check || !buf.uv_buf_check)
		wrot_check_buf(dest, setting, &buf);
}

static s32 wrot_config_tile(struct mml_comp *comp,
			    struct mml_task *task,
			    struct mml_comp_config *ccfg,
			    u8 idx)
{
	struct mml_wrot *wrot = container_of(comp, struct mml_wrot, comp);
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	/* frame data should not change between each tile */
	const struct wrot_frame_data *wrot_frm = wrot_frm_data(ccfg);
	const struct mml_frame_dest *dest = &cfg->info.dest[wrot_frm->out_idx];
	const phys_addr_t base_pa = comp->base_pa;
	const u32 dest_fmt = dest->data.format;

	struct mml_tile_engine *tile = wrot_get_tile(cfg, ccfg, idx);
	/* Following data retrieve from tile result */
	const u32 in_xs = tile->in.xs;
	const u32 in_xe = tile->in.xe;
	const u32 in_ys = tile->in.ys;
	const u32 in_ye = tile->in.ye;
	const u32 out_xs = tile->out.xs;
	const u32 out_xe = tile->out.xe;
	const u32 out_ys = tile->out.ys;
	const u32 out_ye = tile->out.ye;
	const u32 wrot_crop_ofst_x = tile->luma.x;
	const u32 wrot_crop_ofst_y = tile->luma.y;

	u32 wrot_in_xsize;
	u32 wrot_in_ysize;
	u32 wrot_tar_xsize;
	u32 wrot_tar_ysize;
	struct wrot_ofst_addr ofst = {0};
	struct wrot_setting setting = {0};

	/* Fill the the tile settings */
	if (MML_FMT_COMPRESS(dest_fmt))
		wrot_tile_calc_comp(dest, wrot_frm, tile, &ofst);
	else
		wrot_tile_calc(dest, wrot_frm, tile, &ofst);

	/* Write Y pixel offset */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_OFST_ADDR, ofst.y,
		       0x0FFFFFFF);

	/* Write U pixel offset */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_OFST_ADDR_C, ofst.c,
		       0x0FFFFFFF);

	/* Write V pixel offset */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_OFST_ADDR_V, ofst.v,
		       0x0FFFFFFF);

	/* Write source size */
	wrot_in_xsize = in_xe  - in_xs + 1;
	wrot_in_ysize = in_ye  - in_ys  + 1;
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_IN_SIZE,
		       (wrot_in_ysize << 16) + (wrot_in_xsize <<  0),
		       0xFFFFFFFF);

	/* Write target size */
	wrot_tar_xsize = out_xe - out_xs + 1;
	wrot_tar_ysize = out_ye - out_ys + 1;
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_TAR_SIZE,
		       (wrot_tar_ysize << 16) + (wrot_tar_xsize <<  0),
		       0xFFFFFFFF);

	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_CROP_OFST,
		       (wrot_crop_ofst_y << 16) + (wrot_crop_ofst_x <<  0),
		       0xFFFFFFFF);

	/* set max internal buffer for tile usage,
	 * and check for internal buffer size
	 */
	setting.tar_xsize = wrot_tar_xsize;
	wrot_calc_setting(wrot, dest, wrot_frm, &setting);

	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_MAIN_BUF_SIZE,
		       (setting.main_blk_width << 16) +
		       (setting.main_buf_line_num << 8), 0xFFFF7F00);

	/* Set wrot interrupt bit for debug,
	 * this bit will clear to 0 after wrot done.
	 */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_INT, 0x1, VIDO_INT_MASK);

	/* Enable engine */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_ROT_EN, 0x01, 0x00000001);

	mml_msg("%s min block width: %u min buf line num: %u\n",
		__func__, setting.main_blk_width, setting.main_buf_line_num);

	return 0;
}

static s32 wrot_wait(struct mml_comp *comp,
		     struct mml_task *task,
		     struct mml_comp_config *ccfg)
{
	struct mml_wrot *wrot = container_of(comp, struct mml_wrot, comp);
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	/* wait wrot frame done */
	cmdq_pkt_wfe(pkt, wrot->event_eof);

	/* Disable engine */
	cmdq_pkt_write(pkt, NULL, comp->base_pa + VIDO_ROT_EN, 0, 0x00000001);
	return 0;
}

static s32 wrot_reconfig_frame(struct mml_comp *comp,
			       struct mml_task *task,
			       struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct wrot_frame_data *wrot_frm = wrot_frm_data(ccfg);
	struct mml_file_buf *dest_buf = &task->buf.dest[wrot_frm->out_idx];
	struct mml_frame_dest *dest = &cfg->info.dest[wrot_frm->out_idx];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];

	const u32 dest_fmt = dest->data.format;
	const u32 out_swap = MML_FMT_SWAP(dest_fmt);

	if (out_swap == 1 && MML_FMT_PLANE(dest_fmt) == 3) {
		swap(dest_buf->f[1], dest_buf->f[2]);
		swap(dest_buf->iova[1], dest_buf->iova[2]);
		swap(dest->data.plane_offset[1], dest->data.plane_offset[2]);
	}

	calc_plane_offset(wrot_frm->compose.left, wrot_frm->compose.top,
			  dest->data.y_stride, dest->data.uv_stride,
			  wrot_frm->bbp_y, wrot_frm->bbp_uv,
			  wrot_frm->hor_sh_uv, wrot_frm->ver_sh_uv,
			  dest->data.plane_offset);

	if (dest->data.secure) {
		/* TODO: for secure case setup plane offset and reg */
	}

	/* DMA_SUPPORT_AFBC */
	if (MML_FMT_COMPRESS(dest_fmt)) {
		u32 block_x, addr_c, addr_v, addr;

		/* Write frame base address */
		calc_afbc_block(MML_FMT_BITS_PER_PIXEL(dest_fmt),
				dest->data.y_stride,
				dest->data.vert_stride,
				dest_buf->iova,
				dest->data.plane_offset,
				&block_x,
				&addr_c,
				&addr_v,
				&addr);

		/* update frame base address to list */
		update_label(cache, wrot_frm, WROT_LABEL_ADDR, addr);
		update_label(cache, wrot_frm, WROT_LABEL_ADDR_C, addr_c);
		update_label(cache, wrot_frm, WROT_LABEL_ADDR_V, addr_v);

		/* TODO: cmdq should return label of above 3 write */
	} else {
		u32 iova[3] = {
			dest_buf->iova[0] + dest->data.plane_offset[0],
			dest_buf->iova[1] + dest->data.plane_offset[1],
			dest_buf->iova[2] + dest->data.plane_offset[2],
		};

		/* update frame base address to list */
		update_label(cache, wrot_frm, WROT_LABEL_ADDR, iova[0]);
		update_label(cache, wrot_frm, WROT_LABEL_ADDR_C, iova[1]);
		update_label(cache, wrot_frm, WROT_LABEL_ADDR_V, iova[2]);
	}

	return 0;
}

static const struct mml_comp_config_ops wrot_cfg_ops = {
	.prepare = wrot_config_write,
	.get_label_count = wrot_get_label_count,
	.init = wrot_init,
	.frame = wrot_config_frame,
	.tile = wrot_config_tile,
	.wait = wrot_wait,
	.reframe = wrot_reconfig_frame,
};

static s32 wrot_pw_enable(struct mml_comp *comp)
{
	struct mml_wrot *wrot = container_of(comp, struct mml_wrot, comp);

	/* mtk iommu */
	mml_log("%s larb %hhu", __func__, wrot->larb);
	if (wrot->larb == 2)
		smi_bus_prepare_enable(SMI_LARB2, "MDP");
	else if (wrot->larb == 3)
		smi_bus_prepare_enable(SMI_LARB3, "MDP");
	else
		mml_err("[wrot] %s unknown larb %hhu", __func__, wrot->larb);

	return 0;
}

static s32 wrot_pw_disable(struct mml_comp *comp)
{
	struct mml_wrot *wrot = container_of(comp, struct mml_wrot, comp);

	/* mtk iommu */
	mml_log("%s larb %hhu", __func__, wrot->larb);
	if (wrot->larb == 2)
		smi_bus_disable_unprepare(SMI_LARB2, "MDP");
	else if (wrot->larb == 3)
		smi_bus_disable_unprepare(SMI_LARB3, "MDP");
	else
		mml_err("[wrot] %s unknown larb %hhu", __func__, wrot->larb);

	return 0;
}

static s32 wrot_clk_enable(struct mml_comp *comp)
{
	u8 i;

	/* mtk iommu */
#ifdef CONFIG_MTK_IOMMU_V2
	struct mml_wrot *wrot = container_of(comp, struct mml_wrot, comp);
	struct M4U_PORT_STRUCT port = {
		.ePortID = wrot->m4u_port,
		.Virtuality = 1,
	};

	m4u_config_port(&port);
#endif

	for (i = 0; i < ARRAY_SIZE(comp->clks); i++) {
		if (IS_ERR(comp->clks[i]))
			break;
		clk_prepare_enable(comp->clks[i]);
	}

	return 0;
}

static s32 wrot_clk_disable(struct mml_comp *comp)
{
	u8 i;

	for (i = 0; i < ARRAY_SIZE(comp->clks); i++) {
		if (IS_ERR(comp->clks[i]))
			break;
		clk_disable_unprepare(comp->clks[i]);
	}

	return 0;
}

static const struct mml_comp_hw_ops wrot_hw_ops = {
	.pw_enable = &wrot_pw_enable,
	.pw_disable = &wrot_pw_disable,
	.clk_enable = &wrot_clk_enable,
	.clk_disable = &wrot_clk_disable,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_wrot *wrot = dev_get_drvdata(dev);
	s32 ret;

	ret = mml_register_comp(master, &wrot->comp);
	if (ret)
		dev_err(dev, "Failed to register mml component %s: %d\n",
			dev->of_node->full_name, ret);
	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_wrot *wrot = dev_get_drvdata(dev);

	mml_unregister_comp(master, &wrot->comp);
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static struct mml_wrot *dbg_probed_components[4];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_wrot *priv;
	s32 ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->data = (struct wrot_data*)of_device_get_match_data(dev);

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

	if (of_property_read_u16(dev->of_node, "event_frame_done",
		&priv->event_eof))
		dev_err(dev, "read event poll fail\n");

	/* mtk iommu */
	if (of_property_read_u8(dev->of_node, "larb", &priv->larb))
		dev_err(dev, "config larb fail\n");

	if (of_property_read_u32(dev->of_node, "m4u_port", &priv->m4u_port))
		dev_err(dev, "config m4u port fail\n");

	/* assign ops */
	priv->comp.tile_ops = &wrot_tile_ops;
	priv->comp.config_ops = &wrot_cfg_ops;
	priv->comp.hw_ops = &wrot_hw_ops;

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

const struct of_device_id mtk_mml_wrot_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6893-mml_wrot",
	  .data = &mt6893_wrot_data},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_mml_wrot_driver_dt_match);

struct platform_driver mtk_mml_wrot_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
			.name = "mediatek-mml-wrot",
			.owner = THIS_MODULE,
			.of_match_table = mtk_mml_wrot_driver_dt_match,
		},
};

//module_platform_driver(mtk_mml_wrot_driver);

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
module_param_cb(wrot_ut_case, &up_param_ops, NULL, 0644);
MODULE_PARM_DESC(wrot_ut_case, "mml wrot UT test case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML WROT driver");
MODULE_LICENSE("GPL v2");
