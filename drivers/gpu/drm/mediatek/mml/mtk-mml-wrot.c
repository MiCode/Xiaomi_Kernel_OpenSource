/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Dennis YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/dma-mapping.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/math64.h>

#include "mtk-mml-buf.h"
#include "mtk-mml-color.h"
#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"
#include "tile_driver.h"
#include "tile_mdp_reg.h"

#ifdef CONFIG_MTK_SMI_EXT
#include "smi_public.h"
#endif

/* WROT register offset */
#define VIDO_CTRL			0x000
#define VIDO_DMA_PERF			0x004
#define VIDO_MAIN_BUF_SIZE		0x008
#define VIDO_SOFT_RST			0x010
#define VIDO_SOFT_RST_STAT		0x014
#define VIDO_INT_EN			0x018
#define VIDO_INT			0x01c
#define VIDO_CROP_OFST			0x020
#define VIDO_TAR_SIZE			0x024
#define VIDO_FRAME_SIZE			0x028
#define VIDO_OFST_ADDR			0x02c
#define VIDO_STRIDE			0x030
#define VIDO_BKGD			0x034
#define VIDO_OFST_ADDR_C		0x038
#define VIDO_STRIDE_C			0x03c
#define VIDO_ISSUE_REQ_TH		0x040
#define VIDO_GROUP_REQ_TH		0x044
#define VIDO_CTRL_2			0x048
#define VIDO_IN_LINE_ROT		0x050
#define VIDO_DITHER			0x054
#define VIDO_OFST_ADDR_V		0x068
#define VIDO_STRIDE_V			0x06c
#define VIDO_RSV_1			0x070
#define VIDO_DMA_PREULTRA		0x074
#define VIDO_IN_SIZE			0x078
#define VIDO_ROT_EN			0x07c
#define VIDO_FIFO_TEST			0x080
#define VIDO_MAT_CTRL			0x084
#define VIDO_DEBUG			0x0d0
#define VIDO_ARB_SW_CTL			0x0d4
#define VIDO_PVRIC			0x0d8
#define VIDO_SCAN_10BIT			0x0dc
#define VIDO_PENDING_ZERO		0x0e0
#define VIDO_PVRIC_SETTING		0x0e4
#define VIDO_CRC_CTRL			0x0e8
#define VIDO_CRC_VALUE			0x0ec
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
#define VIDO_INT_MASK			0x00000007

/* ceil_m and floor_m helper macro */
//#define ceil_m(n, d) ((u32)(((float)(n) / (d)) + ((n) % (d) != 0)))
//#define floor_m(n, d) ((u32)((float)(n) / (d)))
u32 ceil_m(u64 n, u64 d) {
	u32 reminder = do_div((n),(d));
	return n + (reminder != 0);
}
u32 floor_m(u64 n, u64 d) {
	do_div(n, d);
	return n;
}

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

static const struct wrot_data mt6893_wrot_data = {
	.fifo = 256,
	.tile_width = 512
};

struct mml_comp_wrot {
	struct mml_comp comp;
	const struct wrot_data *data;
	struct device *dev;	/* for dmabuf to iova */

	u8 gpr_poll;
	u16 event_poll;
	u16 event_eof;	/* wrot frame done */
};

/* meta data for each different frame config */
struct wrot_frame_data {
	/* 0 or 1 for 1st or 2nd out port */
	u8 out_idx;

	/* width and height before rotate */
	u32 out_w;
	u32 out_h;
	struct mml_rect compose;
	u32 y_stride;
	u32 uv_stride;
	u64 iova[MML_MAX_PLANES];
	u32 plane_offset[MML_MAX_PLANES];

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

	/* array of indices to one of entry in cache entry list,
	 * use in reuse command
	 */
	u16 labels[WROT_LABEL_TOTAL];
};

#define wrot_frm_data(i)	((struct wrot_frame_data *)(i->data))

static inline struct mml_comp_wrot *comp_to_wrot(struct mml_comp *comp)
{
	return container_of(comp, struct mml_comp_wrot, comp);
}

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
		{	/* 0 */
			{ 1 /* [1 2 1] */, 0 /* drop  */, 0, 2 },
			{ 2 /* [1 2 1] */, 0 /* drop  */, 1, 2 }, /* flip */
		}, {	/* 90 */
			{ 0 /* drop    */, 4 /* [1 1] */, 2, 1 },
			{ 0 /* drop    */, 3 /* [1 1] */, 2, 0 }, /* flip */
		}, {	/* 180 */
			{ 2 /* [1 2 1] */, 0 /* drop  */, 1, 2 },
			{ 1 /* [1 2 1] */, 0 /* drop  */, 0, 2 }, /* flip */
		}, {	/* 270 */
			{ 0 /* drop    */, 3 /* [1 1] */, 2, 0 },
			{ 0 /* drop    */, 4 /* [1 1] */, 2, 1 }, /* flip */
		},
	}, {	/* YUV420 */
		{	/* 0 */
			{ 1 /* [1 2 1] */, 3 /* [1 1] */, 0, 0 },
			{ 2 /* [1 2 1] */, 3 /* [1 1] */, 1, 0 }, /* flip */
		}, {	/* 90 */
			{ 1 /* [1 2 1] */, 4 /* [1 1] */, 0, 1 },
			{ 1 /* [1 2 1] */, 3 /* [1 1] */, 0, 0 }, /* flip */
		}, {	/* 180 */
			{ 2 /* [1 2 1] */, 4 /* [1 1] */, 1, 1 },
			{ 1 /* [1 2 1] */, 4 /* [1 1] */, 0, 1 }, /* flip */
		}, {	/* 270 */
			{ 2 /* [1 2 1] */, 3 /* [1 1] */, 1, 0 },
			{ 2 /* [1 2 1] */, 4 /* [1 1] */, 1, 1 }, /* flip */
		},
	}
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
	wrot_frm->out_crop_w = wrot_frm->out_w >> 1;

	if (MML_FMT_10BIT_PACKED(dest->data.format) &&
	    wrot_frm->out_crop_w & 3) {
		wrot_frm->out_crop_w = (wrot_frm->out_crop_w & ~3) + 4;
		if (is_change_wx(dest->rotate, dest->flip))
			wrot_frm->out_crop_w = wrot_frm->out_w -
					       wrot_frm->out_crop_w;

	} else if (wrot_frm->out_crop_w & 1) {
		wrot_frm->out_crop_w++;
	}
}

static void wrot_config_pipe1(struct mml_frame_dest *dest,
			      struct wrot_frame_data *wrot_frm)
{
	wrot_frm->out_x_off = wrot_frm->out_w >> 1;

	if (MML_FMT_10BIT_PACKED(dest->data.format) &&
	    wrot_frm->out_x_off & 3) {
		wrot_frm->out_x_off = (wrot_frm->out_x_off & ~3) + 4;
		if (is_change_wx(dest->rotate, dest->flip))
			wrot_frm->out_x_off = wrot_frm->out_w -
					      wrot_frm->out_x_off;

	} else if (wrot_frm->out_x_off & 1) {
		wrot_frm->out_x_off++;
	}

	wrot_frm->out_crop_w = wrot_frm->out_w - wrot_frm->out_x_off;
}

static s32 wrot_config_write(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct wrot_frame_data *wrot_frm;
	struct mml_frame_dest *dest;
	u8 i;

	/* initialize component frame data for current frame config */
	wrot_frm = kzalloc(sizeof(*wrot_frm), GFP_KERNEL);
	ccfg->data = wrot_frm;

	/* cache out index for easy use */
	wrot_frm->out_idx = ccfg->node->out_idx;

	/* select output port struct */
	dest = &cfg->info.dest[wrot_frm->out_idx];

	wrot_frm->compose = dest->compose;
	wrot_frm->y_stride = dest->data.y_stride;
	wrot_frm->uv_stride = dest->data.uv_stride;
	if (dest->rotate == MML_ROT_0 || dest->rotate == MML_ROT_180) {
		wrot_frm->out_w = dest->data.width;
		wrot_frm->out_h = dest->data.height;
	} else {
		wrot_frm->out_w = dest->data.height;
		wrot_frm->out_h = dest->data.width;
		swap(wrot_frm->compose.width, wrot_frm->compose.height);
	}

	/* make sure uv stride data */
	if (MML_FMT_PLANE(dest->data.format) > 1 && !wrot_frm->uv_stride)
		wrot_frm->uv_stride = mml_color_get_min_uv_stride(
			dest->data.format, dest->data.width);

	/* plane offset for later use */
	for (i = 0; i < task->buf.dest[wrot_frm->out_idx].cnt; i++)
		wrot_frm->plane_offset[i] = dest->data.plane_offset[i];

	if (cfg->dual) {
		if (ccfg->pipe == 0)
			wrot_config_pipe0(dest, wrot_frm);
		else
			wrot_config_pipe1(dest, wrot_frm);
	}

	return 0;
}

static s32 wrot_buf_map(struct mml_comp *comp, struct mml_task *task,
			const struct mml_path_node *node)
{
	struct mml_comp_wrot *wrot = comp_to_wrot(comp);
	struct mml_file_buf *dest_buf = &task->buf.dest[node->out_idx];
	s32 ret;

	/* get iova */
	ret = mml_buf_iova_get(wrot->dev, dest_buf);
	if (ret < 0)
		mml_err("%s iova fail %d", __func__, ret);

	mml_msg("%s comp %u iova %#11llx (%u) %#11llx (%u) %#11llx (%u)",
		__func__, comp->id,
		task->buf.dest[node->out_idx].dma[0].iova,
		task->buf.dest[node->out_idx].size[0],
		task->buf.dest[node->out_idx].dma[1].iova,
		task->buf.dest[node->out_idx].size[1],
		task->buf.dest[node->out_idx].dma[2].iova,
		task->buf.dest[node->out_idx].size[2]);

	return ret;
}

static s32 wrot_buf_prepare(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg)
{
	struct wrot_frame_data *wrot_frm = wrot_frm_data(ccfg);
	struct mml_file_buf *dest_buf = &task->buf.dest[wrot_frm->out_idx];
	u32 i;

	for (i = 0; i < dest_buf->cnt; i++)
		wrot_frm->iova[i] = dest_buf->dma[i].iova;

	return 0;
}

static s32 wrot_tile_prepare(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg,
			     void *ptr_func, void *tile_data)
{
	TILE_FUNC_BLOCK_STRUCT *func = (TILE_FUNC_BLOCK_STRUCT*)ptr_func;
	struct mml_tile_data *data = (struct mml_tile_data*)tile_data;
	struct wrot_frame_data *wrot_frm = wrot_frm_data(ccfg);
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_dest *dest = &cfg->info.dest[wrot_frm->out_idx];
	struct mml_comp_wrot *wrot = comp_to_wrot(comp);

	data->wrot_data.dest_fmt = dest->data.format;
	data->wrot_data.rotate = dest->rotate;
	data->wrot_data.flip = dest->flip;
	data->wrot_data.alpharot = cfg->alpharot;
	data->wrot_data.enable_crop = cfg->dual? true: false;

	/* reuse wrot_frm data which processed with rotate and dual */
	data->wrot_data.crop_left = wrot_frm->out_x_off;
	data->wrot_data.crop_width = wrot_frm->out_crop_w;
	func->full_size_x_in = wrot_frm->out_w;
	func->full_size_y_in = wrot_frm->out_h;
	func->full_size_x_out = wrot_frm->out_w;
	func->full_size_y_out = wrot_frm->out_h;

	data->wrot_data.max_width = wrot->data->tile_width;
	func->func_data = (struct TILE_FUNC_DATA_STRUCT*)(&data->wrot_data);
	func->enable_flag = true;

	return 0;
}

static const struct mml_comp_tile_ops wrot_tile_ops = {
	.prepare = wrot_tile_prepare,
};


static u32 wrot_get_label_count(struct mml_comp *comp, struct mml_task *task)
{
	return WROT_LABEL_TOTAL;
}

static s32 wrot_init(struct mml_comp *comp, struct mml_task *task,
		     struct mml_comp_config *ccfg)
{
	struct mml_comp_wrot *wrot = comp_to_wrot(comp);
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
			mml_err("[wrot] unknown profile conversion %x",
				profile_in);
	} else {
		if (profile_in == MML_YCBCR_PROFILE_JPEG &&
		    profile_out == MML_YCBCR_PROFILE_BT601) {
			wrot_frm->mat_en = 1;
			wrot_frm->mat_sel = 8;
		} else if (profile_in == MML_YCBCR_PROFILE_JPEG &&
			   profile_out == MML_YCBCR_PROFILE_BT709) {
			wrot_frm->mat_en = 1;
			wrot_frm->mat_sel = 9;
		} else if (profile_in == MML_YCBCR_PROFILE_BT601 &&
			   profile_out == MML_YCBCR_PROFILE_JPEG) {
			wrot_frm->mat_en = 1;
			wrot_frm->mat_sel = 10;
		} else if (profile_in == MML_YCBCR_PROFILE_BT709 &&
			   profile_out == MML_YCBCR_PROFILE_JPEG) {
			wrot_frm->mat_en = 1;
			wrot_frm->mat_sel = 11;
		} else if (profile_in == MML_YCBCR_PROFILE_BT709 &&
			   profile_out == MML_YCBCR_PROFILE_BT601) {
			wrot_frm->mat_en = 1;
			wrot_frm->mat_sel = 12;
		} else if (profile_in == MML_YCBCR_PROFILE_BT601 &&
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

static void wrot_calc_hw_buf_setting(struct mml_comp_wrot *wrot,
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

static s32 wrot_config_frame(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg)
{
	struct mml_comp_wrot *wrot = comp_to_wrot(comp);
	struct mml_frame_config *cfg = task->config;
	struct wrot_frame_data *wrot_frm = wrot_frm_data(ccfg);
	struct mml_frame_dest *dest = &cfg->info.dest[wrot_frm->out_idx];
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];

	const phys_addr_t base_pa = comp->base_pa;
	const u32 src_fmt = cfg->info.src.format;
	const u32 dest_fmt = dest->data.format;
	const u16 rotate = dest->rotate;
	const u8 flip = dest->flip ? 1 : 0;
	const u32 h_subsample = MML_FMT_H_SUBSAMPLE(dest_fmt);
	const u32 v_subsample = MML_FMT_V_SUBSAMPLE(dest_fmt);
	const u8 plane = MML_FMT_PLANE(dest_fmt);
	const u32 crop_en = 1;		/* always enable crop */

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
		swap(wrot_frm->iova[1], wrot_frm->iova[2]);
		swap(wrot_frm->plane_offset[1], wrot_frm->plane_offset[2]);
	}

	calc_plane_offset(wrot_frm->compose.left, wrot_frm->compose.top,
			  wrot_frm->y_stride, wrot_frm->uv_stride,
			  wrot_frm->bbp_y, wrot_frm->bbp_uv,
			  wrot_frm->hor_sh_uv, wrot_frm->ver_sh_uv,
			  wrot_frm->plane_offset);

	if (dest->data.secure) {
		/* TODO: for secure case setup plane offset and reg */
	}

	/* DMA_SUPPORT_AFBC */
	if (MML_FMT_COMPRESS(dest_fmt)) {
		u32 block_x, addr_c, addr_v, addr;
		u32 frame_size;

		/* Write frame base address */
		calc_afbc_block(wrot_frm->bbp_y,
				wrot_frm->y_stride, dest->data.vert_stride,
				wrot_frm->iova, wrot_frm->plane_offset,
				&block_x, &addr_c, &addr_v, &addr);

		/* Write frame base address */
		mml_write(pkt, base_pa + VIDO_BASE_ADDR, addr, U32_MAX,
			cache, &wrot_frm->labels[WROT_LABEL_ADDR]);
		mml_write(pkt, base_pa + VIDO_BASE_ADDR_C, addr_c, U32_MAX,
			cache, &wrot_frm->labels[WROT_LABEL_ADDR_C]);
		mml_write(pkt, base_pa + VIDO_BASE_ADDR_V, addr_v, U32_MAX,
			cache, &wrot_frm->labels[WROT_LABEL_ADDR_V]);

		if (dest->rotate == MML_ROT_0 || dest->rotate == MML_ROT_180)
			frame_size = ((((wrot_frm->out_h + 31) >>
					 5) << 5) << 16) +
					 ((block_x << 5) << 0);
		else
			frame_size = ((((wrot_frm->out_w + 31) >>
					 5) << 5) << 16) +
					 ((block_x << 5) << 0);
		cmdq_pkt_write(pkt, NULL, base_pa + VIDO_FRAME_SIZE,
			       frame_size, U32_MAX);

		cmdq_pkt_write(pkt, NULL, base_pa + VIDO_AFBC_YUVTRANS,
			       MML_FMT_IS_ARGB(dest_fmt), 0x1);
	} else {
		u32 iova[3] = {
			wrot_frm->iova[0] + wrot_frm->plane_offset[0],
			wrot_frm->iova[1] + wrot_frm->plane_offset[1],
			wrot_frm->iova[2] + wrot_frm->plane_offset[2],
		};

		mml_msg("%s base %#x+%u %#x+%u %#x+%u",
			__func__,
			iova[0], wrot_frm->plane_offset[0],
			iova[1], wrot_frm->plane_offset[1],
			iova[2], wrot_frm->plane_offset[2]);

		/* Write frame base address */
		mml_write(pkt, base_pa + VIDO_BASE_ADDR, iova[0], U32_MAX,
			cache, &wrot_frm->labels[WROT_LABEL_ADDR]);
		mml_write(pkt, base_pa + VIDO_BASE_ADDR_C, iova[1], U32_MAX,
			cache, &wrot_frm->labels[WROT_LABEL_ADDR_C]);
		mml_write(pkt, base_pa + VIDO_BASE_ADDR_V, iova[2], U32_MAX,
			cache, &wrot_frm->labels[WROT_LABEL_ADDR_V]);
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

	if (MML_FMT_COMPRESS(dest_fmt)) {
		pvric = pvric | BIT(0);
		if (MML_FMT_10BIT(dest_fmt))
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
			       preultra, U32_MAX);

	/* Write frame Y stride */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_STRIDE, wrot_frm->y_stride,
		       U32_MAX);

	/* Write frame UV stride */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_STRIDE_C,
		       wrot_frm->uv_stride, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_STRIDE_V,
		       wrot_frm->uv_stride, U32_MAX);

	/* Write matrix control */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_MAT_CTRL,
		       (wrot_frm->mat_sel << 4) +
		       (wrot_frm->mat_en << 0), 0x000000f3);

	/* Set the fixed ALPHA as 0xff */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_DITHER, 0xff000000,
		       0xff000000);

	/* Set VIDO_EOL_SEL */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_RSV_1, 0x80000000,
		       0x80000000);

	/* Set VIDO_FIFO_TEST */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_FIFO_TEST, wrot->data->fifo,
		       U32_MAX);

	/* Filter Enable */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_MAIN_BUF_SIZE,
		       (filt_v << 4) + (filt_h << 0), 0x00000077);

	/* turn off WROT dma dcm */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_ROT_EN,
		       (0x1 << 23) + (0x1 << 20), 0x00900000);

	return 0;
}

static void wrot_tile_calc_comp(const struct mml_frame_dest *dest,
				const struct wrot_frame_data *wrot_frm,
				const struct mml_tile_engine *tile,
				struct wrot_ofst_addr *ofst)
{
	/* Following data retrieve from tile calc result */
	const u32 out_xs = tile->out.xs;

	if (dest->rotate == MML_ROT_0 && !dest->flip) {
		/* Target Y offset */
		ofst->y = out_xs * 32;

		/*
		 * Target U offset
		 * RGBA: 64, YV12 8-bit: 24, 10-bit: 32
		 */
		ofst->c = ofst->y / 64;

		mml_msg("%s No flip and no rotation: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	} else if (dest->rotate == MML_ROT_0 && dest->flip) {
		/* Target Y offset */
		ofst->y = (wrot_frm->out_w - out_xs - 32) * 32;

		/* Target U offset */
		ofst->c = ofst->y / 64;

		mml_msg("%s Flip without rotation: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	} else if (dest->rotate == MML_ROT_90 && !dest->flip) {
		/* Target Y offset */
		ofst->y = ((out_xs / 8) + 1) *
			  ((wrot_frm->y_stride / 128) - 1) * 1024;

		/* Target U offset */
		ofst->c = ofst->y / 64;

		mml_msg("%s Rotate 90 degree only: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	} else if (dest->rotate == MML_ROT_90 && dest->flip) {
		/* Target Y offset */
		ofst->y = (out_xs / 8) * (wrot_frm->y_stride / 128) * 1024;

		/* Target U offset */
		ofst->c = ofst->y / 64;

		mml_msg("%s Flip and Rotate 90 degree: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	} else if (dest->rotate == MML_ROT_180 && !dest->flip) {
		/* Target Y offset */
		ofst->y = (((wrot_frm->out_h / 8) - 1) *
			  (wrot_frm->y_stride / 128) +
			  ((wrot_frm->out_w / 32) - (out_xs / 32) - 1)) *
			  1024;

		/* Target U offset */
		ofst->c = ofst->y / 64;

		mml_msg("%s Rotate 180 degree only: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	} else if (dest->rotate == MML_ROT_180 && dest->flip) {
		/* Target Y offset */
		ofst->y = (((wrot_frm->out_h / 8) - 1) *
			  (wrot_frm->y_stride / 128) +
			  (out_xs / 32)) * 1024;

		/* Target U offset */
		ofst->c = ofst->y / 64;

		mml_msg("%s Flip and Rotate 180 degree: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	} else if (dest->rotate == MML_ROT_270 && !dest->flip) {
		/* Target Y offset */
		ofst->y = ((wrot_frm->out_w / 8) - (out_xs / 8) - 1) *
			  (wrot_frm->y_stride / 128) * 1024;

		/* Target U offset */
		ofst->c = ofst->y / 64;

		mml_msg("%s Rotate 270 degree only: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	} else if (dest->rotate == MML_ROT_270 && dest->flip) {
		/* Target Y offset */
		ofst->y = ((wrot_frm->out_w / 8) - (out_xs / 8)) *
			  ((wrot_frm->y_stride / 128) - 1) * 1024;

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

	if (dest->rotate == MML_ROT_0 && !dest->flip) {
		/* Target Y offset */
		ofst->y = out_ys * wrot_frm->y_stride +
			  (out_xs * wrot_frm->bbp_y >> 3);

		/* Target U offset */
		ofst->c = (out_ys >> wrot_frm->ver_sh_uv) *
			  wrot_frm->uv_stride +
			  ((out_xs >> wrot_frm->hor_sh_uv) *
			  wrot_frm->bbp_uv >> 3);

		/* Target V offset */
		ofst->v = (out_ys >> wrot_frm->ver_sh_uv) *
			  wrot_frm->uv_stride +
			  ((out_xs >> wrot_frm->hor_sh_uv) *
			  wrot_frm->bbp_uv >> 3);

		mml_msg("%s No flip and no rotation: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	} else if (dest->rotate == MML_ROT_0 && dest->flip) {
		/* Target Y offset */
		ofst->y = out_ys * wrot_frm->y_stride +
			  ((wrot_frm->out_w - out_xs) *
			  wrot_frm->bbp_y >> 3) - 1;

		/* Target U offset */
		ofst->c = (out_ys >> wrot_frm->ver_sh_uv) *
			  wrot_frm->uv_stride +
			  (((wrot_frm->out_w - out_xs) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;

		/* Target V offset */
		ofst->v = (out_ys >> wrot_frm->ver_sh_uv) *
			  wrot_frm->uv_stride +
			  (((wrot_frm->out_w - out_xs) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;

		mml_msg("%s Flip without rotation: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	} else if (dest->rotate == MML_ROT_90 && !dest->flip) {
		/* Target Y offset */
		ofst->y = out_xs * wrot_frm->y_stride +
			  ((wrot_frm->out_h - out_ys) *
			  wrot_frm->bbp_y >> 3) - 1;

		/* Target U offset */
		ofst->c = (out_xs >> wrot_frm->ver_sh_uv) *
			  wrot_frm->uv_stride +
			  (((wrot_frm->out_h - out_ys) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;

		/* Target V offset */
		ofst->v = (out_xs >> wrot_frm->ver_sh_uv) *
			  wrot_frm->uv_stride +
			  (((wrot_frm->out_h - out_ys) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;

		mml_msg("%s Rotate 90 degree only: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	} else if (dest->rotate == MML_ROT_90 && dest->flip) {
		/* Target Y offset */
		ofst->y = out_xs * wrot_frm->y_stride +
			  (out_ys * wrot_frm->bbp_y >> 3);

		/* Target U offset */
		ofst->c = (out_xs >> wrot_frm->ver_sh_uv) *
			  wrot_frm->uv_stride +
			  ((out_ys >> wrot_frm->hor_sh_uv) *
			  wrot_frm->bbp_uv >> 3);

		/* Target V offset */
		ofst->v = (out_xs >> wrot_frm->ver_sh_uv) *
			  wrot_frm->uv_stride +
			  ((out_ys >> wrot_frm->hor_sh_uv) *
			  wrot_frm->bbp_uv >> 3);

		mml_msg("%s Flip and Rotate 90 degree: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	} else if (dest->rotate == MML_ROT_180 && !dest->flip) {
		/* Target Y offset */
		ofst->y = (wrot_frm->out_h - out_ys - 1) *
			  wrot_frm->y_stride +
			  ((wrot_frm->out_w - out_xs) *
			  wrot_frm->bbp_y >> 3) - 1;

		/* Target U offset */
		ofst->c = ((wrot_frm->out_h - out_ys - 1) >>
			  wrot_frm->ver_sh_uv) * wrot_frm->uv_stride +
			  (((wrot_frm->out_w - out_xs) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;

		/* Target V offset */
		ofst->v = ((wrot_frm->out_h - out_ys - 1) >>
			  wrot_frm->ver_sh_uv) * wrot_frm->uv_stride +
			  (((wrot_frm->out_w - out_xs) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;

		mml_msg("%s Rotate 180 degree only: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	} else if (dest->rotate == MML_ROT_180 && dest->flip) {
		/* Target Y offset */
		ofst->y = (wrot_frm->out_h - out_ys - 1) *
			  wrot_frm->y_stride +
			  (out_xs * wrot_frm->bbp_y >> 3);

		/* Target U offset */
		ofst->c = ((wrot_frm->out_h - out_ys - 1) >>
			  wrot_frm->ver_sh_uv) * wrot_frm->uv_stride +
			  ((out_xs >> wrot_frm->hor_sh_uv) *
			  wrot_frm->bbp_uv >> 3);

		/* Target V offset */
		ofst->v = ((wrot_frm->out_h - out_ys - 1) >>
			  wrot_frm->ver_sh_uv) * wrot_frm->uv_stride +
			  ((out_xs >> wrot_frm->hor_sh_uv) *
			  wrot_frm->bbp_uv >> 3);

		mml_msg("%s Flip and Rotate 180 degree: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	} else if (dest->rotate == MML_ROT_270 && !dest->flip) {
		/* Target Y offset */
		ofst->y = (wrot_frm->out_w - out_xs - 1) *
			  wrot_frm->y_stride +
			  (out_ys * wrot_frm->bbp_y >> 3);

		/* Target U offset */
		ofst->c = ((wrot_frm->out_w - out_xs - 1) >>
			  wrot_frm->ver_sh_uv) * wrot_frm->uv_stride +
			  ((out_ys >> wrot_frm->hor_sh_uv) *
			  wrot_frm->bbp_uv >> 3);

		/* Target V offset */
		ofst->v = ((wrot_frm->out_w - out_xs - 1) >>
			  wrot_frm->ver_sh_uv) * wrot_frm->uv_stride +
			  ((out_ys >> wrot_frm->hor_sh_uv) *
			  wrot_frm->bbp_uv >> 3);

		mml_msg("%s Rotate 270 degree only: offset Y:%#010x U:%#010x V:%#010x",
			__func__, ofst->y, ofst->c, ofst->v);
	} else if (dest->rotate == MML_ROT_270 && dest->flip) {
		/* Target Y offset */
		ofst->y = (wrot_frm->out_w - out_xs - 1) *
			  wrot_frm->y_stride +
			  ((wrot_frm->out_h - out_ys) *
			  wrot_frm->bbp_y >> 3) - 1;

		/* Target U offset */
		ofst->c = ((wrot_frm->out_w - out_xs - 1) >>
			  wrot_frm->ver_sh_uv) * wrot_frm->uv_stride +
			  (((wrot_frm->out_h - out_ys) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;

		/* Target V offset */
		ofst->v = ((wrot_frm->out_w - out_xs - 1) >>
			  wrot_frm->ver_sh_uv) * wrot_frm->uv_stride +
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
	buf->y_buf_width = ceil_m(setting->main_blk_width,
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

	buf->uv_buf_width = ceil_m(buf->uv_blk_width, buf->uv_blk_line) *
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

static void wrot_calc_setting(struct mml_comp_wrot *wrot,
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

	coeff1 = floor_m(wrot_frm->fifo_max_sz, setting->tar_xsize * 2) * 2;
	coeff2 = ceil_m(setting->tar_xsize, coeff1);
	temp = ceil_m(setting->tar_xsize, coeff2 * 4) * 4;

	if (temp > setting->tar_xsize)
		setting->main_buf_line_num = ceil_m(setting->tar_xsize, 4) * 4;
	else
		setting->main_buf_line_num = temp;
	if (setting->main_buf_line_num > wrot_frm->max_line_cnt)
		setting->main_buf_line_num = wrot_frm->max_line_cnt;

	/* check for internal buffer size */
	while (!buf.y_buf_check || !buf.uv_buf_check)
		wrot_check_buf(dest, setting, &buf);
}

static s32 wrot_config_tile(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg, u8 idx)
{
	struct mml_comp_wrot *wrot = comp_to_wrot(comp);
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	/* frame data should not change between each tile */
	const struct wrot_frame_data *wrot_frm = wrot_frm_data(ccfg);
	const struct mml_frame_dest *dest = &cfg->info.dest[wrot_frm->out_idx];
	const phys_addr_t base_pa = comp->base_pa;
	const u32 dest_fmt = dest->data.format;

	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);
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
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_OFST_ADDR, ofst.y, U32_MAX);

	/* Write U pixel offset */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_OFST_ADDR_C, ofst.c, U32_MAX);

	/* Write V pixel offset */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_OFST_ADDR_V, ofst.v, U32_MAX);

	/* Write source size */
	wrot_in_xsize = in_xe  - in_xs + 1;
	wrot_in_ysize = in_ye  - in_ys  + 1;
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_IN_SIZE,
		       (wrot_in_ysize << 16) + (wrot_in_xsize <<  0),
		       U32_MAX);

	/* Write target size */
	wrot_tar_xsize = out_xe - out_xs + 1;
	wrot_tar_ysize = out_ye - out_ys + 1;
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_TAR_SIZE,
		       (wrot_tar_ysize << 16) + (wrot_tar_xsize <<  0),
		       U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_CROP_OFST,
		       (wrot_crop_ofst_y << 16) + (wrot_crop_ofst_x <<  0),
		       U32_MAX);

	/* set max internal buffer for tile usage,
	 * and check for internal buffer size
	 */
	setting.tar_xsize = wrot_tar_xsize;
	wrot_calc_setting(wrot, dest, wrot_frm, &setting);

	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_MAIN_BUF_SIZE,
		       (setting.main_blk_width << 16) +
		       (setting.main_buf_line_num << 8), 0xffff7f00);

	/* Set wrot interrupt bit for debug,
	 * this bit will clear to 0 after wrot done.
	 */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_INT, 0x1, U32_MAX);

	/* Enable engine */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_ROT_EN, 0x01, 0x00000001);

	mml_msg("%s min block width: %u min buf line num: %u\n",
		__func__, setting.main_blk_width, setting.main_buf_line_num);

	return 0;
}

static s32 wrot_wait(struct mml_comp *comp, struct mml_task *task,
		     struct mml_comp_config *ccfg)
{
	struct mml_comp_wrot *wrot = comp_to_wrot(comp);
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	/* wait wrot frame done */
	cmdq_pkt_wfe(pkt, wrot->event_eof);
	/* Disable engine */
	cmdq_pkt_write(pkt, NULL, comp->base_pa + VIDO_ROT_EN, 0, 0x00000001);
	return 0;
}

static s32 wrot_reconfig_frame(struct mml_comp *comp, struct mml_task *task,
			       struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct wrot_frame_data *wrot_frm = wrot_frm_data(ccfg);
	struct mml_frame_dest *dest = &cfg->info.dest[wrot_frm->out_idx];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];

	const u32 dest_fmt = dest->data.format;
	const u32 out_swap = MML_FMT_SWAP(dest_fmt);

	if (out_swap == 1 && MML_FMT_PLANE(dest_fmt) == 3)
		swap(wrot_frm->iova[1], wrot_frm->iova[2]);

	if (dest->data.secure) {
		/* TODO: for secure case setup plane offset and reg */
	}

	/* DMA_SUPPORT_AFBC */
	if (MML_FMT_COMPRESS(dest_fmt)) {
		u32 block_x, addr_c, addr_v, addr;

		/* Write frame base address */
		calc_afbc_block(wrot_frm->bbp_y,
				wrot_frm->y_stride, dest->data.vert_stride,
				wrot_frm->iova, wrot_frm->plane_offset,
				&block_x, &addr_c, &addr_v, &addr);

		/* update frame base address to list */
		mml_update(cache, wrot_frm->labels[WROT_LABEL_ADDR], addr);
		mml_update(cache, wrot_frm->labels[WROT_LABEL_ADDR_C], addr_c);
		mml_update(cache, wrot_frm->labels[WROT_LABEL_ADDR_V], addr_v);
	} else {
		u32 addr = wrot_frm->iova[0] + wrot_frm->plane_offset[0];
		u32 addr_c = wrot_frm->iova[1] + wrot_frm->plane_offset[1];
		u32 addr_v = wrot_frm->iova[2] + wrot_frm->plane_offset[2];

		/* update frame base address to list */
		mml_update(cache, wrot_frm->labels[WROT_LABEL_ADDR], addr);
		mml_update(cache, wrot_frm->labels[WROT_LABEL_ADDR_C], addr_c);
		mml_update(cache, wrot_frm->labels[WROT_LABEL_ADDR_V], addr_v);
	}

	return 0;
}

static const struct mml_comp_config_ops wrot_cfg_ops = {
	.prepare = wrot_config_write,
	.buf_map = wrot_buf_map,
	.buf_prepare = wrot_buf_prepare,
	.get_label_count = wrot_get_label_count,
	.init = wrot_init,
	.frame = wrot_config_frame,
	.tile = wrot_config_tile,
	.wait = wrot_wait,
	.reframe = wrot_reconfig_frame,
};

static const struct mml_comp_hw_ops wrot_hw_ops = {
	.pw_enable = &mml_comp_pw_enable,
	.pw_disable = &mml_comp_pw_disable,
	.clk_enable = &mml_comp_clk_enable,
	.clk_disable = &mml_comp_clk_disable,
};

static void wrot_debug_dump(struct mml_comp *comp)
{
	void __iomem *base = comp->base;
	u32 value[18];
	u32 debug[34];
	u32 dbg_id = 0, i;

	mml_err("wrot component %u dump:", comp->id);

	value[0] = readl(base + VIDO_CTRL);
	value[1] = readl(base + VIDO_DMA_PERF);
	value[2] = readl(base + VIDO_MAIN_BUF_SIZE);
	value[3] = readl(base + VIDO_SOFT_RST);
	value[4] = readl(base + VIDO_SOFT_RST_STAT);
	value[5] = readl(base + VIDO_INT);
	value[6] = readl(base + VIDO_TAR_SIZE);
	value[7] = readl(base + VIDO_FRAME_SIZE);
	value[8] = readl(base + VIDO_OFST_ADDR);
	value[9] = readl(base + VIDO_STRIDE);
	value[10] = readl(base + VIDO_RSV_1);
	value[11] = readl(base + VIDO_IN_SIZE);
	value[12] = readl(base + VIDO_ROT_EN);
	value[13] = readl(base + VIDO_PVRIC);
	value[14] = readl(base + VIDO_PENDING_ZERO);
	value[15] = readl(base + VIDO_BASE_ADDR);
	value[16] = readl(base + VIDO_BASE_ADDR_C);
	value[17] = readl(base + VIDO_BASE_ADDR_V);

	/* debug id from 0x0100 ~ 0x2100, count 34 which is debug array size */
	for (i = 0; i < ARRAY_SIZE(debug); i++) {
		dbg_id += 0x100;
		writel(dbg_id, (volatile void *)base + VIDO_INT_EN);
		debug[i] = readl(base + VIDO_DEBUG);
	}

	mml_err("VIDO_CTRL %#010x VIDO_DMA_PERF %#010x VIDO_MAIN_BUF_SIZE %#010x",
		value[0], value[1], value[2]);
	mml_err("VIDO_SOFT_RST %#010x VIDO_SOFT_RST_STAT %#010x VIDO_INT %#010x",
		value[3], value[4], value[5]);
	mml_err("VIDO_TAR_SIZE %#010x VIDO_FRAME_SIZE %#010x VIDO_OFST_ADDR %#010x",
		value[6], value[7], value[8]);
	mml_err("VIDO_STRIDE %#010x VIDO_RSV_1 %#010x VIDO_IN_SIZE %#010x",
		value[9], value[10], value[11]);
	mml_err("VIDO_ROT_EN %#010x VIDO_PVRIC %#010x VIDO_PENDING_ZERO %#010x",
		value[12], value[13], value[14]);
	mml_err("VIDO_BASE_ADDR %#010x C %#010x V %#010x",
		value[15], value[16], value[17]);

	for (i = 0; i < ARRAY_SIZE(debug) / 3; i++)
		mml_err("ROT_DBUGG_%x %#010x ROT_DBUGG_%x %#010x ROT_DBUGG_%x %#010x",
			i * 3, debug[i*3],
			i * 3 + 1, debug[i*3+1],
			i * 3 + 2, debug[i*3+2]);
	mml_err("ROT_DBUGG_21 %#010x", debug[33]);
}

static const struct mml_comp_debug_ops wrot_debug_ops = {
	.dump = &wrot_debug_dump,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_wrot *wrot = dev_get_drvdata(dev);
	s32 ret;

	ret = mml_register_comp(master, &wrot->comp);
	if (ret)
		dev_err(dev, "Failed to register mml component %s: %d\n",
			dev->of_node->full_name, ret);
	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_wrot *wrot = dev_get_drvdata(dev);

	mml_unregister_comp(master, &wrot->comp);
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static struct mml_comp_wrot *dbg_probed_components[4];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_comp_wrot *priv;
	s32 ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->data = (const struct wrot_data *)of_device_get_match_data(dev);
	priv->dev = dev;

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34));
	if (ret)
		dev_err(dev, "fail to config wrot dma mask %d\n", ret);

	ret = mml_comp_init(pdev, &priv->comp);
	if (ret) {
		dev_err(dev, "Failed to init mml component: %d\n", ret);
		return ret;
	}

	/* init larb for smi and mtcmos */
	ret = mml_comp_init_larb(&priv->comp, dev);
	if (ret) {
		if (ret == -EPROBE_DEFER)
			return ret;
		dev_err(dev, "fail to init component %u larb ret %d",
			priv->comp.id, ret);
	}

	if (of_property_read_u8(dev->of_node, "gpr_poll", &priv->gpr_poll))
		dev_err(dev, "read gpr poll fail\n");

	if (of_property_read_u16(dev->of_node, "event_poll",
		&priv->event_poll))
		dev_err(dev, "read event poll fail\n");

	if (of_property_read_u16(dev->of_node, "event_frame_done",
		&priv->event_eof))
		dev_err(dev, "read event poll fail\n");

	/* assign ops */
	priv->comp.tile_ops = &wrot_tile_ops;
	priv->comp.config_ops = &wrot_cfg_ops;
	priv->comp.hw_ops = &wrot_hw_ops;
	priv->comp.debug_ops = &wrot_debug_ops;

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
	{
		.compatible = "mediatek,mt6893-mml_wrot",
		.data = &mt6893_wrot_data
	},
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
				"  -      mml_bound: %d\n",
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
