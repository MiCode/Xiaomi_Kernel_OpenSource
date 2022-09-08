/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Dennis YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */

#include <linux/component.h>
#include <linux/dma-mapping.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/math64.h>
#include <soc/mediatek/smi.h>

#include "mtk-mml-buf.h"
#include "mtk-mml-color.h"
#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"
#include "mtk-mml-dle-adaptor.h"

#include "tile_driver.h"
#include "mtk-mml-tile.h"
#include "tile_mdp_func.h"
#include "mtk-mml-sys.h"
#include "mtk-mml-mmp.h"

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
#define VIDO_DITHER_CON			0x058
#define VIDO_OFST_ADDR_V		0x068
#define VIDO_STRIDE_V			0x06c
#define VIDO_EOL_SEL			0x070
#define VIDO_DMA_PREULTRA		0x074
#define VIDO_IN_SIZE			0x078
#define VIDO_ROT_EN			0x07c
#define VIDO_FIFO_TEST			0x080
#define VIDO_MAT_CTRL			0x084
#define VIDO_CG_NEW_CTRL		0x088
#define VIDO_SHADOW_CTRL		0x08c
#define VIDO_DEBUG			0x0d0
#define VIDO_ARB_SW_CTL			0x0d4
#define VIDO_PVRIC			0x0d8
#define VIDO_SCAN_10BIT			0x0dc
#define VIDO_PENDING_ZERO		0x0e0
#define VIDO_PVRIC_SETTING		0x0e4
#define VIDO_CRC_CTRL			0x0e8
#define VIDO_CRC_VALUE			0x0ec
#define VIDO_COMPRESSION_VALUE		0x0f0
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
#define VIDO_BASE_ADDR_HIGH		0xf34
#define VIDO_BASE_ADDR_HIGH_C		0xf38
#define VIDO_BASE_ADDR_HIGH_V		0xf3c
#define VIDO_OFST_ADDR_HIGH		0xf40
#define VIDO_OFST_ADDR_HIGH_C		0xf44
#define VIDO_OFST_ADDR_HIGH_V		0xf48

#define WROT_MIN_BUF_LINE_NUM		16

/* register mask */
#define VIDO_INT_MASK			0x00000007

/* Inline rot offsets */
#define DISPSYS_SHADOW_CTRL		0x010
#define INLINEROT_OVLSEL		0x030
#define INLINEROT_HEIGHT0		0x034
#define INLINEROT_HEIGHT1		0x038
#define INLINEROT_WDONE			0x03C

/* SMI offset */
#define SMI_LARB_NON_SEC_CON		0x380

#define MML_WROT_RACING_MIN		64

/* debug option to change sram write height */
int mml_racing_h = MML_WROT_RACING_MIN;
module_param(mml_racing_h, int, 0644);

int mml_racing_rdone;
module_param(mml_racing_rdone, int, 0644);

/* ceil_m and floor_m helper function */
static u32 ceil_m(u64 n, u64 d)
{
	u32 reminder = do_div((n),(d));

	return n + (reminder != 0);
}

static u32 floor_m(u64 n, u64 d)
{
	do_div(n, d);
	return n;
}

enum wrot_label {
	WROT_LABEL_ADDR = 0,
	WROT_LABEL_ADDR_HIGH,
	WROT_LABEL_ADDR_C,
	WROT_LABEL_ADDR_HIGH_C,
	WROT_LABEL_ADDR_V,
	WROT_LABEL_ADDR_HIGH_V,
	WROT_LABEL_TOTAL
};

static s32 wrot_write_ofst(struct cmdq_pkt *pkt,
			   dma_addr_t addr, dma_addr_t addr_high, u64 value)
{
	s32 ret;

	ret = cmdq_pkt_write(pkt, NULL, addr, value & GENMASK_ULL(31, 0), U32_MAX);
	if (ret)
		return ret;
	ret = cmdq_pkt_write(pkt, NULL, addr_high, value >> 32, U32_MAX);
	return ret;
}

static s32 wrot_write_addr(struct cmdq_pkt *pkt,
			   dma_addr_t addr, dma_addr_t addr_high, u64 value,
			   struct mml_task_reuse *reuse,
			   struct mml_pipe_cache *cache,
			   u16 *label_idx)
{
	s32 ret;

	ret = mml_write(pkt, addr, value & GENMASK_ULL(31, 0), U32_MAX,
			reuse, cache, label_idx);
	if (ret)
		return ret;
	ret = mml_write(pkt, addr_high, value >> 32, U32_MAX,
			reuse, cache, label_idx + 1);
	return ret;
}

static void wrot_update_addr(struct mml_task_reuse *reuse,
			     u16 label, u16 label_high, u64 value)
{
	mml_update(reuse, label, value & GENMASK_ULL(31, 0));
	mml_update(reuse, label_high, value >> 32);
}

struct wrot_data {
	u32 fifo;
	u32 tile_width;
	u32 sram_size;
	u8 rb_swap;	/* version for rb channel swap behavior */
};

static const struct wrot_data mml_wrot_data = {
	.fifo = 256,
	.tile_width = 512,
	.sram_size = 512 * 1024,
	.rb_swap = 1,
};

struct mml_comp_wrot {
	struct mtk_ddp_comp ddp_comp;
	struct mml_comp comp;
	const struct wrot_data *data;
	bool ddp_bound;

	u16 event_eof;		/* wrot frame done */
	u16 event_bufa;		/* notify pipe0 that pipe1 ready buf a */
	u16 event_bufb;		/* notify pipe0 that pipe1 ready buf b */
	u16 event_buf_next;	/* notify pipe1 that pipe0 ready new round */
	int idx;

	struct device *dev;	/* for dmabuf to iova */
	/* smi register to config sram/dram mode */
	phys_addr_t smi_larb_con;
	/* inline rotate base addr */
	phys_addr_t irot_base[MML_PIPE_CNT];
	void __iomem *irot_va[MML_PIPE_CNT];

	u32 sram_size;
	u32 sram_cnt;
	u64 sram_pa;
	struct mutex sram_mutex;
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
	bool en_x_crop;
	bool en_y_crop;
	struct mml_rect out_crop;

	/* following data calculate in init and use in tile command */
	u8 mat_en;
	u8 mat_sel;
	u32 dither_con;
	/* bits per pixel y */
	u32 bbp_y;
	/* bits per pixel uv */
	u32 bbp_uv;
	/* hor right shift uv */
	u32 hor_sh_uv;
	/* vert right shift uv */
	u32 ver_sh_uv;
	/* VIDO_FILT_TYPE_V Chroma down sample filter type */
	u32 filt_v;

	/* calculate in frame, use in each tile calc */
	u32 fifo_max_sz;
	u32 max_line_cnt;

	u32 pixel_acc;		/* pixel accumulation */
	u32 datasize;		/* qos data size in bytes */

	struct {
		bool eol:1;	/* tile is end of current line */
		u8 sram:1;	/* sram ping pong idx of this tile */
	} wdone[256];
	u8 sram_side;		/* write to left/right ovl */

	/* array of indices to one of entry in cache entry list,
	 * use in reuse command
	 */
	u16 labels[WROT_LABEL_TOTAL];

	u32 wdone_cnt;
};

static inline struct wrot_frame_data *wrot_frm_data(struct mml_comp_config *ccfg)
{
	return ccfg->data;
}

static inline struct mml_comp_wrot *comp_to_wrot(struct mml_comp *comp)
{
	return container_of(comp, struct mml_comp_wrot, comp);
}

struct wrot_ofst_addr {
	u64 y;	/* wrot_y_ofst_adr for VIDO_OFST_ADDR */
	u64 c;	/* wrot_c_ofst_adr for VIDO_OFST_ADDR_C */
	u64 v;	/* wrot_v_ofst_adr for VIDO_OFST_ADDR_V */
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

static void wrot_config_left(struct mml_frame_dest *dest,
			     struct wrot_frame_data *wrot_frm)
{
	wrot_frm->en_x_crop = true;
	wrot_frm->out_crop.left = 0;
	wrot_frm->out_crop.width = wrot_frm->out_w >> 1;

	if (MML_FMT_10BIT_PACKED(dest->data.format) &&
	    wrot_frm->out_crop.width & 3) {
		wrot_frm->out_crop.width = (wrot_frm->out_crop.width & ~3) + 4;
		if (is_change_wx(dest->rotate, dest->flip))
			wrot_frm->out_crop.width = wrot_frm->out_w -
						   wrot_frm->out_crop.width;
	} else if (wrot_frm->out_crop.width & 1) {
		wrot_frm->out_crop.width++;
	}
}

static void wrot_config_right(struct mml_frame_dest *dest,
			      struct wrot_frame_data *wrot_frm)
{
	wrot_frm->en_x_crop = true;
	wrot_frm->out_crop.left = wrot_frm->out_w >> 1;

	if (MML_FMT_10BIT_PACKED(dest->data.format) &&
	    wrot_frm->out_crop.left & 3) {
		wrot_frm->out_crop.left = (wrot_frm->out_crop.left & ~3) + 4;
		if (is_change_wx(dest->rotate, dest->flip))
			wrot_frm->out_crop.left = wrot_frm->out_w -
						  wrot_frm->out_crop.left;
	} else if (wrot_frm->out_crop.left & 1) {
		wrot_frm->out_crop.left++;
	}

	wrot_frm->out_crop.width = wrot_frm->out_w - wrot_frm->out_crop.left;
}

static void wrot_config_top(struct mml_frame_dest *dest,
			    struct wrot_frame_data *wrot_frm)
{
	wrot_frm->en_y_crop = true;
	wrot_frm->out_crop.top = 0;
	wrot_frm->out_crop.height = wrot_frm->out_h >> 1;
	if (wrot_frm->out_crop.height & 0x1)
		wrot_frm->out_crop.height++;
}

static void wrot_config_bottom(struct mml_frame_dest *dest,
			       struct wrot_frame_data *wrot_frm)
{
	wrot_frm->en_y_crop = true;
	wrot_frm->out_crop.top = wrot_frm->out_h >> 1;
	if (wrot_frm->out_crop.top & 0x1)
		wrot_frm->out_crop.top++;
	wrot_frm->out_crop.height = wrot_frm->out_h - wrot_frm->out_crop.top;
}

static void wrot_config_pipe0(struct mml_frame_config *cfg,
			      struct mml_frame_dest *dest,
			      struct wrot_frame_data *wrot_frm)
{
	if (cfg->info.mode == MML_MODE_RACING) {
		if ((dest->rotate == MML_ROT_90 && !dest->flip) ||
		    (dest->rotate == MML_ROT_270 && dest->flip))
			wrot_config_bottom(dest, wrot_frm);
		else if ((dest->rotate == MML_ROT_90 && dest->flip) ||
			 (dest->rotate == MML_ROT_270 && !dest->flip))
			wrot_config_top(dest, wrot_frm);
		else if ((dest->rotate == MML_ROT_0 && !dest->flip) ||
			 (dest->rotate == MML_ROT_180 && dest->flip))
			wrot_config_left(dest, wrot_frm);
		else if ((dest->rotate == MML_ROT_0 && dest->flip) ||
			 (dest->rotate == MML_ROT_180 && !dest->flip))
			wrot_config_right(dest, wrot_frm);
	} else {
		wrot_config_left(dest, wrot_frm);
	}
}

static void wrot_config_pipe1(struct mml_frame_config *cfg,
			      struct mml_frame_dest *dest,
			      struct wrot_frame_data *wrot_frm)
{
	if (cfg->info.mode == MML_MODE_RACING) {
		if ((dest->rotate == MML_ROT_90 && !dest->flip) ||
		    (dest->rotate == MML_ROT_270 && dest->flip))
			wrot_config_top(dest, wrot_frm);
		else if ((dest->rotate == MML_ROT_90 && dest->flip) ||
			 (dest->rotate == MML_ROT_270 && !dest->flip))
			wrot_config_bottom(dest, wrot_frm);
		else if ((dest->rotate == MML_ROT_0 && !dest->flip) ||
			 (dest->rotate == MML_ROT_180 && dest->flip))
			wrot_config_right(dest, wrot_frm);
		else if ((dest->rotate == MML_ROT_0 && dest->flip) ||
			 (dest->rotate == MML_ROT_180 && !dest->flip))
			wrot_config_left(dest, wrot_frm);
	} else {
		wrot_config_right(dest, wrot_frm);
	}
}

static s32 wrot_prepare(struct mml_comp *comp, struct mml_task *task,
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
			wrot_config_pipe0(cfg, dest, wrot_frm);
		else
			wrot_config_pipe1(cfg, dest, wrot_frm);

		if (cfg->info.mode == MML_MODE_RACING) {
			if (dest->rotate == MML_ROT_0)
				wrot_frm->sram_side = ccfg->pipe;
			else if (dest->rotate == MML_ROT_90)
				wrot_frm->sram_side = !ccfg->pipe;
			else if (dest->rotate == MML_ROT_180)
				wrot_frm->sram_side = !ccfg->pipe;
			else
				wrot_frm->sram_side = ccfg->pipe;

			if (dest->flip)
				wrot_frm->sram_side = !wrot_frm->sram_side;
		}
	}

	return 0;
}

static s32 wrot_buf_map(struct mml_comp *comp, struct mml_task *task,
			const struct mml_path_node *node)
{
	struct mml_comp_wrot *wrot = comp_to_wrot(comp);
	struct mml_file_buf *dest_buf = &task->buf.dest[node->out_idx];
	s32 ret = 0;

	mml_trace_ex_begin("%s", __func__);

	if (task->config->info.mode == MML_MODE_RACING) {
	} else {
		/* get iova */
		ret = mml_buf_iova_get(wrot->dev, dest_buf);
		if (ret < 0)
			mml_err("%s iova fail %d", __func__, ret);

		mml_mmp(buf_map, MMPROFILE_FLAG_PULSE,
			((u64)task->job.jobid << 16) | comp->id,
			(unsigned long)dest_buf->dma[0].iova);
	}

	mml_trace_ex_end();

	mml_msg("%s comp %u iova %#11llx (%u) %#11llx (%u) %#11llx (%u)",
		__func__, comp->id,
		dest_buf->dma[0].iova,
		dest_buf->size[0],
		dest_buf->dma[1].iova,
		dest_buf->size[1],
		dest_buf->dma[2].iova,
		dest_buf->size[2]);

	return ret;
}

static s32 wrot_buf_prepare(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg)
{
	struct mml_comp_wrot *wrot = comp_to_wrot(comp);
	struct wrot_frame_data *wrot_frm = wrot_frm_data(ccfg);
	struct mml_file_buf *dest_buf = &task->buf.dest[wrot_frm->out_idx];
	u32 i;

	if (task->config->info.mode == MML_MODE_RACING) {
		/* assign sram pa directly */
		mutex_lock(&wrot->sram_mutex);
		if (!wrot->sram_cnt)
			wrot->sram_pa = (u64)mml_sram_get(task->config->mml);
		wrot->sram_cnt++;
		mutex_unlock(&wrot->sram_mutex);
		wrot_frm->iova[0] = wrot->sram_pa;
	} else {
		for (i = 0; i < dest_buf->cnt; i++)
			wrot_frm->iova[i] = dest_buf->dma[i].iova;
	}
	return 0;
}

static void wrot_buf_unprepare(struct mml_comp *comp, struct mml_task *task,
			       struct mml_comp_config *ccfg)
{
	struct mml_comp_wrot *wrot = comp_to_wrot(comp);

	if (task->config->info.mode == MML_MODE_RACING) {
		mutex_lock(&wrot->sram_mutex);
		wrot->sram_cnt--;
		if (wrot->sram_cnt == 0)
			mml_sram_put(task->config->mml);
		mutex_unlock(&wrot->sram_mutex);
	}
}

static s32 wrot_tile_prepare(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg,
			     struct tile_func_block *func,
			     union mml_tile_data *data)
{
	struct wrot_frame_data *wrot_frm = wrot_frm_data(ccfg);
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_dest *dest = &cfg->info.dest[wrot_frm->out_idx];
	struct mml_comp_wrot *wrot = comp_to_wrot(comp);

	data->wrot.dest_fmt = dest->data.format;
	data->wrot.rotate = dest->rotate;
	data->wrot.flip = dest->flip;
	data->wrot.alpharot = cfg->alpharot;
	data->wrot.racing = cfg->info.mode == MML_MODE_RACING;
	data->wrot.racing_h = max(mml_racing_h, MML_WROT_RACING_MIN);

	/* reuse wrot_frm data which processed with rotate and dual */
	data->wrot.enable_x_crop = wrot_frm->en_x_crop;
	data->wrot.enable_y_crop = wrot_frm->en_y_crop;
	data->wrot.crop = wrot_frm->out_crop;
	func->full_size_x_in = wrot_frm->out_w;
	func->full_size_y_in = wrot_frm->out_h;
	func->full_size_x_out = wrot_frm->out_w;
	func->full_size_y_out = wrot_frm->out_h;

	data->wrot.max_width = wrot->data->tile_width;
	/* WROT support crop capability */
	func->type = TILE_TYPE_WDMA | TILE_TYPE_CROP_EN;
	func->init_func = tile_wrot_init;
	func->for_func = tile_wrot_for;
	func->back_func = tile_wrot_back;
	func->data = data;
	func->enable_flag = true;

	return 0;
}

static const struct mml_comp_tile_ops wrot_tile_ops = {
	.prepare = wrot_tile_prepare,
};

static u32 wrot_get_label_count(struct mml_comp *comp, struct mml_task *task,
				struct mml_comp_config *ccfg)
{
	return WROT_LABEL_TOTAL;
}

static void wrot_color_fmt(struct mml_frame_config *cfg,
			   struct wrot_frame_data *wrot_frm)
{
	u32 fmt = cfg->info.dest[wrot_frm->out_idx].data.format;
	u16 profile_in = cfg->info.src.profile;
	u16 profile_out = cfg->info.dest[wrot_frm->out_idx].data.profile;

	wrot_frm->mat_en = 0;
	wrot_frm->mat_sel = 15;
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

	/* Enable dither */
	if (MML_FMT_10BIT(cfg->info.src.format) && !MML_FMT_10BIT(fmt)) {
		wrot_frm->mat_en = 1;
		wrot_frm->dither_con = (0x1 << 10) +
			 (0x0 << 8) +
			 (0x0 << 4) +
			 (0x1 << 2) +
			 (0x1 << 1) +
			 (0x1 << 0);
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
			    u32 *block_x, u64 *addr_c, u64 *addr_v, u64 *addr)
{
	u32 block_y;
	u64 header_sz;

	*block_x = ((y_stride << 3) / bits_per_pixel + 31) >> 5;
	block_y = (vert_stride + 7) >> 3;
	header_sz = ((((*block_x * block_y) << 4) + 1023) >> 10) << 10;

	*addr_c = iova[0] + offset[0];
	*addr_v = iova[2] + offset[2];
	*addr = *addr_c + header_sz;
}

static void wrot_calc_hw_buf_setting(const struct mml_comp_wrot *wrot,
				     const struct mml_frame_config *cfg,
				     const struct mml_frame_dest *dest,
				     struct wrot_frame_data *wrot_frm)
{
	const u32 dest_fmt = dest->data.format;

	if (MML_FMT_YUV422(dest_fmt)) {
		if (MML_FMT_PLANE(dest_fmt) == 1) {
			wrot_frm->fifo_max_sz = wrot->data->tile_width * 32;
			wrot_frm->max_line_cnt = 32;
		} else {
			wrot_frm->fifo_max_sz = wrot->data->tile_width * 48;
			wrot_frm->max_line_cnt = 48;
		}
	} else if (MML_FMT_YUV420(dest_fmt)) {
		wrot_frm->fifo_max_sz = wrot->data->tile_width * 64;
		wrot_frm->max_line_cnt = 64;
	} else if (dest_fmt == MML_FMT_GREY) {
		wrot_frm->fifo_max_sz = wrot->data->tile_width * 64;
		wrot_frm->max_line_cnt = 64;
	} else if (MML_FMT_IS_RGB(dest_fmt)) {
		if (cfg->alpharot) {
			wrot_frm->fifo_max_sz = wrot->data->tile_width * 16;
			wrot_frm->max_line_cnt = 16;
		} else {
			wrot_frm->fifo_max_sz = wrot->data->tile_width * 32;
			wrot_frm->max_line_cnt = 32;
		}
	} else {
		mml_err("%s fail set fifo max size, max line count for %#x",
			__func__, dest_fmt);
	}
}

static void wrot_config_addr(const struct mml_frame_dest *dest,
			     const u32 dest_fmt,
			     const phys_addr_t base_pa,
			     struct wrot_frame_data *wrot_frm,
			     struct cmdq_pkt *pkt,
			     struct mml_task_reuse *reuse,
			     struct mml_pipe_cache *cache)
{
	u64 addr_c, addr_v, addr;

	if (MML_FMT_COMPRESS(dest_fmt)) {
		/* wrot afbc output case */
		u32 block_x;
		u32 frame_size;

		/* Write frame base address */
		calc_afbc_block(wrot_frm->bbp_y,
				wrot_frm->y_stride, dest->data.vert_stride,
				wrot_frm->iova, wrot_frm->plane_offset,
				&block_x, &addr_c, &addr_v, &addr);

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
		addr = wrot_frm->iova[0] + wrot_frm->plane_offset[0];
		addr_c = wrot_frm->iova[1] + wrot_frm->plane_offset[1];
		addr_v = wrot_frm->iova[2] + wrot_frm->plane_offset[2];

		mml_msg("%s base %#llx+%u %#llx+%u %#llx+%u",
			__func__,
			addr, wrot_frm->plane_offset[0],
			addr_c, wrot_frm->plane_offset[1],
			addr_v, wrot_frm->plane_offset[2]);
	}

	if (!mml_slt) {
		/* Write frame base address */
		wrot_write_addr(pkt,
				base_pa + VIDO_BASE_ADDR,
				base_pa + VIDO_BASE_ADDR_HIGH, addr,
				reuse, cache, &wrot_frm->labels[WROT_LABEL_ADDR]);
		wrot_write_addr(pkt,
				base_pa + VIDO_BASE_ADDR_C,
				base_pa + VIDO_BASE_ADDR_HIGH_C, addr_c,
				reuse, cache, &wrot_frm->labels[WROT_LABEL_ADDR_C]);
		wrot_write_addr(pkt,
				base_pa + VIDO_BASE_ADDR_V,
				base_pa + VIDO_BASE_ADDR_HIGH_V, addr_v,
				reuse, cache, &wrot_frm->labels[WROT_LABEL_ADDR_V]);
	}
}

static void wrot_config_ready(struct mml_comp_wrot *wrot,
	struct mml_frame_config *cfg,
	struct wrot_frame_data *wrot_frm, u32 pipe, struct cmdq_pkt *pkt,
	bool enable)
{
	const struct mml_topology_path *path = cfg->path[pipe];
	phys_addr_t sel = path->mmlsys->base_pa +
		mml_sys_get_reg_ready_sel(path->mmlsys);
	u32 shift, mask;

	if (wrot->idx == 0)
		shift = 0;
	else if (wrot->idx == 1)
		shift = 3;
	else
		return;
	mask = cfg->dual ? (0x7 << shift) | GENMASK(31, 6) : U32_MAX;

	if (mml_racing_rdone) {
		/* debug mode, make rdone to wrot tie 1 */
		cmdq_pkt_write(pkt, NULL, sel, 0x24, U32_MAX);
		return;
	}

	if (!enable) {
		cmdq_pkt_write(pkt, NULL, sel, 0, mask);
	} else if (cfg->disp_dual) {
		/* 1:2 or 2:2 monitor both disp0 and disp1, mdpsys merge ready */
		cmdq_pkt_write(pkt, NULL, sel, 2 << shift, mask);
	} else {
		/* 1:1 or 2:1 monitor only disp0 */
		cmdq_pkt_write(pkt, NULL, sel, 0, mask);
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
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];

	const phys_addr_t base_pa = comp->base_pa;
	const u32 src_fmt = cfg->info.src.format;
	const u32 dest_fmt = dest->data.format;
	const u16 rotate = dest->rotate;
	const u8 flip = dest->flip ? 1 : 0;
	const u32 h_subsample = MML_FMT_H_SUBSAMPLE(dest_fmt);
	const u32 v_subsample = MML_FMT_V_SUBSAMPLE(dest_fmt);
	const u8 plane = MML_FMT_PLANE(dest_fmt);
	const u32 preultra_en = 1;	/* always enable wrot pre-ultra */
	const u32 crop_en = 1;		/* always enable crop */
	const u32 hw_fmt = MML_FMT_HW_FORMAT(dest_fmt);

	u32 out_swap = MML_FMT_SWAP(dest_fmt);
	u32 uv_xsel, uv_ysel;
	u32 preultra;
	u32 scan_10bit = 0, bit_num = 0, pending_zero = 0, pvric = 0;

	wrot_color_fmt(cfg, wrot_frm);

	/* calculate for later config tile use */
	wrot_calc_hw_buf_setting(wrot, cfg, dest, wrot_frm);

	if (cfg->alpharot) {
		wrot_frm->mat_en = 0;

		if (wrot->data->rb_swap == 1) {
			if (!MML_FMT_COMPRESS(src_fmt) && !MML_FMT_10BIT(src_fmt))
				out_swap ^= MML_FMT_SWAP(src_fmt);
			else if (MML_FMT_COMPRESS(src_fmt) && !MML_FMT_10BIT(src_fmt))
				out_swap =
					(MML_FMT_SWAP(src_fmt) == MML_FMT_SWAP(dest_fmt)) ? 1 : 0;
			else if (MML_FMT_COMPRESS(src_fmt) && MML_FMT_10BIT(src_fmt))
				out_swap = out_swap ? 0 : 1;
		}
	}

	mml_msg("use config %p wrot %p", cfg, wrot);

	/* Enable engine */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_ROT_EN, 0x01, 0x00000001);

	/* Enable shadow */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_SHADOW_CTRL, 0x1, U32_MAX);

	if (h_subsample) {	/* YUV422/420 out */
		wrot_frm->filt_v = MML_FMT_V_SUBSAMPLE(src_fmt) ||
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

	if (task->config->info.mode == MML_MODE_RACING) {
		/* config smi addr to emi (iova) or sram */
		cmdq_pkt_write(pkt, NULL, wrot->smi_larb_con,
			GENMASK(19, 16), GENMASK(19, 16));

		/* config ready signal from disp0 or disp1 */
		wrot_config_ready(wrot, cfg, wrot_frm, ccfg->pipe, pkt, true);

		/* inline rotate case always write to sram pa */
		cmdq_pkt_write(pkt, NULL, base_pa + VIDO_BASE_ADDR,
			wrot->sram_pa, U32_MAX);
		cmdq_pkt_write(pkt, NULL, base_pa + VIDO_BASE_ADDR_HIGH,
			wrot->sram_pa >> 32, U32_MAX);
		cmdq_pkt_write(pkt, NULL, base_pa + VIDO_BASE_ADDR_C,
			wrot->sram_pa, U32_MAX);
		cmdq_pkt_write(pkt, NULL, base_pa + VIDO_BASE_ADDR_HIGH_C,
			wrot->sram_pa >> 32, U32_MAX);
		cmdq_pkt_write(pkt, NULL, base_pa + VIDO_BASE_ADDR_V,
			wrot->sram_pa, U32_MAX);
		cmdq_pkt_write(pkt, NULL, base_pa + VIDO_BASE_ADDR_HIGH_V,
			wrot->sram_pa >> 32, U32_MAX);

		cmdq_pkt_write(pkt, NULL,
			wrot->irot_base[0] + DISPSYS_SHADOW_CTRL, 0x2, U32_MAX);

		if (mml_racing_ut == 2)
			cmdq_pkt_write(pkt, NULL,
				wrot->irot_base[0] + INLINEROT_OVLSEL, 0x22, U32_MAX);
		else if (mml_racing_ut == 3)
			cmdq_pkt_write(pkt, NULL,
				wrot->irot_base[0] + INLINEROT_OVLSEL, 0xc, U32_MAX);

		mml_msg("%s sram pa %#x", __func__, (u32)wrot->sram_pa);
	} else {
		/* normal dram case config wrot iova with reuse */
		wrot_config_addr(dest, dest_fmt, base_pa,
				 wrot_frm, pkt, reuse, cache);
		/* always turn off ready to wrot */
		wrot_config_ready(wrot, cfg, wrot_frm, ccfg->pipe, pkt, false);

		/* and clear inlinerot enable since last frame maybe racing mode */
		cmdq_pkt_write(pkt, NULL, base_pa + VIDO_IN_LINE_ROT, 0, U32_MAX);
	}

	/* Write frame related registers */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_CTRL,
		       (uv_ysel		<< 30) +
		       (uv_xsel		<< 28) +
		       (flip		<< 24) +
		       (dest->rotate	<< 20) +
		       (cfg->alpharot	<< 16) + /* alpha rot */
		       (preultra_en	<< 14) + /* pre-ultra */
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
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_SCAN_10BIT, scan_10bit, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_PENDING_ZERO,
		       pending_zero << 26, 0x04000000);
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_CTRL_2, bit_num,
		       0x00000007);

	if (MML_FMT_COMPRESS(dest_fmt)) {
		pvric |= BIT(0);
		if (MML_FMT_10BIT(dest_fmt))
			pvric |= BIT(1);
	}
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_PVRIC, pvric, U32_MAX);

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
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_DMA_PREULTRA, preultra,
		       U32_MAX);

	/* Write frame Y stride */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_STRIDE, wrot_frm->y_stride,
		       U32_MAX);
	/* Write frame UV stride */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_STRIDE_C, wrot_frm->uv_stride,
		       U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_STRIDE_V, wrot_frm->uv_stride,
		       U32_MAX);

	/* Write matrix control */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_MAT_CTRL,
		       (wrot_frm->mat_sel << 4) +
		       (wrot_frm->mat_en << 0), U32_MAX);

	/* Set the fixed ALPHA as 0xff */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_DITHER, 0xff000000, U32_MAX);

	/* Set VIDO_EOL_SEL */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_EOL_SEL, 0x80000000, 0x80000000);

	/* Set VIDO_FIFO_TEST */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_FIFO_TEST, wrot->data->fifo, U32_MAX);

	/* turn off WROT dma dcm */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_ROT_EN,
		       (0x1 << 23) + (0x1 << 20), 0x00900000);

	/* Enable dither */
	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_DITHER_CON, wrot_frm->dither_con, U32_MAX);

	return 0;
}

static void wrot_tile_calc_comp(const struct mml_frame_dest *dest,
				const struct wrot_frame_data *wrot_frm,
				const struct mml_tile_engine *tile,
				struct wrot_ofst_addr *ofst)
{
	/* Following data retrieve from tile calc result */
	const u64 out_xs = tile->out.xs;
	const char *msg = "";

	if (dest->rotate == MML_ROT_0 && !dest->flip) {
		/* Target Y offset */
		ofst->y = out_xs * 32;

		/*
		 * Target U offset
		 * RGBA: 64, YV12 8-bit: 24, 10-bit: 32
		 */
		ofst->c = DO_COMMON_DIV(ofst->y, 64);

		msg = "No flip and no rotation";
	} else if (dest->rotate == MML_ROT_0 && dest->flip) {
		/* Target Y offset */
		ofst->y = (wrot_frm->out_w - out_xs - 32) * 32;

		/* Target U offset */
		ofst->c = DO_COMMON_DIV(ofst->y, 64);

		msg = "Flip without rotation";
	} else if (dest->rotate == MML_ROT_90 && !dest->flip) {
		/* Target Y offset */
		ofst->y = ((DO_COMMON_DIV(out_xs, 8) + 1) *
			  (wrot_frm->y_stride / 128) - 1) * 1024;

		/* Target U offset */
		ofst->c = DO_COMMON_DIV(ofst->y, 64);

		msg = "Rotate 90 degree only";
	} else if (dest->rotate == MML_ROT_90 && dest->flip) {
		/* Target Y offset */
		ofst->y = DO_COMMON_DIV(out_xs, 8) * (wrot_frm->y_stride / 128) * 1024;

		/* Target U offset */
		ofst->c = DO_COMMON_DIV(ofst->y, 64);

		msg = "Flip and Rotate 90 degree";
	} else if (dest->rotate == MML_ROT_180 && !dest->flip) {
		/* Target Y offset */
		ofst->y = ((((u64)wrot_frm->out_h / 8) - 1) *
			  (wrot_frm->y_stride / 128) +
			  ((wrot_frm->out_w / 32) - DO_COMMON_DIV(out_xs, 32) - 1)) *
			  1024;

		/* Target U offset */
		ofst->c = DO_COMMON_DIV(ofst->y, 64);

		msg = "Rotate 180 degree only";
	} else if (dest->rotate == MML_ROT_180 && dest->flip) {
		/* Target Y offset */
		ofst->y = ((((u64)wrot_frm->out_h / 8) - 1) *
			  (wrot_frm->y_stride / 128) +
			  DO_COMMON_DIV(out_xs, 32)) * 1024;

		/* Target U offset */
		ofst->c = DO_COMMON_DIV(ofst->y, 64);

		msg = "Flip and Rotate 180 degree";
	} else if (dest->rotate == MML_ROT_270 && !dest->flip) {
		/* Target Y offset */
		ofst->y = ((wrot_frm->out_w / 8) - DO_COMMON_DIV(out_xs, 8) - 1) *
			  (wrot_frm->y_stride / 128) * 1024;

		/* Target U offset */
		ofst->c = DO_COMMON_DIV(ofst->y, 64);

		msg = "Rotate 270 degree only";
	} else if (dest->rotate == MML_ROT_270 && dest->flip) {
		/* Target Y offset */
		ofst->y = (((wrot_frm->out_w / 8) - DO_COMMON_DIV(out_xs, 8)) *
			  (wrot_frm->y_stride / 128) - 1) * 1024;

		/* Target U offset */
		ofst->c = DO_COMMON_DIV(ofst->y, 64);

		msg = "Flip and Rotate 270 degree";
	}
	mml_msg("%s %s: offset Y:%#010llx U:%#010llx V:%#010llx",
		__func__, msg, ofst->y, ofst->c, ofst->v);
}

static void wrot_tile_calc(const struct mml_task *task,
			   const struct mml_comp_wrot *wrot,
			   const struct mml_frame_dest *dest,
			   const struct mml_tile_output *tout,
			   const struct mml_tile_engine *tile,
			   const u32 idx,
			   const enum mml_mode mode,
			   struct wrot_frame_data *wrot_frm,
			   struct wrot_ofst_addr *ofst)
{
	/* Following data retrieve from tile calc result */
	u64 out_xs = tile->out.xs;
	u64 out_ys = tile->out.ys;
	u32 out_w = wrot_frm->out_w;
	u32 out_h = wrot_frm->out_h;
	u32 sram_block = 0;		/* buffer block number for sram */
	const char *msg = "";
	bool tile_eol = false;

	if (dest->rotate == MML_ROT_0 && !dest->flip) {
		if (mode == MML_MODE_RACING) {
			sram_block = tout->tiles[idx].v_tile_no & 0x1;
			out_ys = 0;
			tile_eol = tout->h_tile_cnt == (tout->tiles[idx].h_tile_no + 1);
		}
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

		msg = "No flip and no rotation";
	} else if (dest->rotate == MML_ROT_0 && dest->flip) {
		if (mode == MML_MODE_RACING) {
			sram_block = tout->tiles[idx].v_tile_no & 0x1;
			out_ys = 0;
			tile_eol = tout->h_tile_cnt == (tout->tiles[idx].h_tile_no + 1);
		}
		/* Target Y offset */
		ofst->y = out_ys * wrot_frm->y_stride +
			  ((out_w - out_xs) *
			  wrot_frm->bbp_y >> 3) - 1;

		/* Target U offset */
		ofst->c = (out_ys >> wrot_frm->ver_sh_uv) *
			  wrot_frm->uv_stride +
			  (((out_w - out_xs) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;

		/* Target V offset */
		ofst->v = (out_ys >> wrot_frm->ver_sh_uv) *
			  wrot_frm->uv_stride +
			  (((out_w - out_xs) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;

		msg = "Flip without rotation";
	} else if (dest->rotate == MML_ROT_90 && !dest->flip) {
		if (mode == MML_MODE_RACING) {
			sram_block = tout->tiles[idx].h_tile_no & 0x1;
			out_xs = 0;
			tile_eol = tout->v_tile_cnt == (tout->tiles[idx].v_tile_no + 1);
		}
		/* Target Y offset */
		ofst->y = out_xs * wrot_frm->y_stride +
			  ((out_h - out_ys) *
			  wrot_frm->bbp_y >> 3) - 1;

		/* Target U offset */
		ofst->c = (out_xs >> wrot_frm->ver_sh_uv) *
			  wrot_frm->uv_stride +
			  (((out_h - out_ys) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;

		/* Target V offset */
		ofst->v = (out_xs >> wrot_frm->ver_sh_uv) *
			  wrot_frm->uv_stride +
			  (((out_h - out_ys) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;

		msg = "Rotate 90 degree only";
	} else if (dest->rotate == MML_ROT_90 && dest->flip) {
		if (mode == MML_MODE_RACING) {
			sram_block = tout->tiles[idx].h_tile_no & 0x1;
			out_xs = 0;
			tile_eol = tout->v_tile_cnt == (tout->tiles[idx].v_tile_no + 1);
		}
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

		msg = "Flip and Rotate 90 degree";
	} else if (dest->rotate == MML_ROT_180 && !dest->flip) {
		if (mode == MML_MODE_RACING) {
			sram_block = (tout->v_tile_cnt - tout->tiles[idx].v_tile_no - 1) & 0x1;
			out_h = tile->out.ye + 1;
			tile_eol = tout->tiles[idx].h_tile_no == 0;
		}
		/* Target Y offset */
		ofst->y = (out_h - out_ys - 1) *
			  wrot_frm->y_stride +
			  ((out_w - out_xs) *
			  wrot_frm->bbp_y >> 3) - 1;

		/* Target U offset */
		ofst->c = ((out_h - out_ys - 1) >>
			  wrot_frm->ver_sh_uv) * wrot_frm->uv_stride +
			  (((out_w - out_xs) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;

		/* Target V offset */
		ofst->v = ((out_h - out_ys - 1) >>
			  wrot_frm->ver_sh_uv) * wrot_frm->uv_stride +
			  (((out_w - out_xs) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;

		msg = "Rotate 180 degree only";
	} else if (dest->rotate == MML_ROT_180 && dest->flip) {
		if (mode == MML_MODE_RACING) {
			sram_block = (tout->v_tile_cnt - tout->tiles[idx].v_tile_no - 1) & 0x1;
			out_h = tile->out.ye + 1;
			tile_eol = tout->tiles[idx].h_tile_no == 0;
		}
		/* Target Y offset */
		ofst->y = (out_h - out_ys - 1) *
			  wrot_frm->y_stride +
			  (out_xs * wrot_frm->bbp_y >> 3);

		/* Target U offset */
		ofst->c = ((out_h - out_ys - 1) >>
			  wrot_frm->ver_sh_uv) * wrot_frm->uv_stride +
			  ((out_xs >> wrot_frm->hor_sh_uv) *
			  wrot_frm->bbp_uv >> 3);

		/* Target V offset */
		ofst->v = ((out_h - out_ys - 1) >>
			  wrot_frm->ver_sh_uv) * wrot_frm->uv_stride +
			  ((out_xs >> wrot_frm->hor_sh_uv) *
			  wrot_frm->bbp_uv >> 3);

		msg = "Flip and Rotate 180 degree";
	} else if (dest->rotate == MML_ROT_270 && !dest->flip) {
		if (mode == MML_MODE_RACING) {
			sram_block = (tout->h_tile_cnt - tout->tiles[idx].h_tile_no - 1) & 0x1;
			out_w = tile->out.xe + 1;
			tile_eol = tout->tiles[idx].v_tile_no == 0;
		}
		/* Target Y offset */
		ofst->y = (out_w - out_xs - 1) *
			  wrot_frm->y_stride +
			  (out_ys * wrot_frm->bbp_y >> 3);

		/* Target U offset */
		ofst->c = ((out_w - out_xs - 1) >>
			  wrot_frm->ver_sh_uv) * wrot_frm->uv_stride +
			  ((out_ys >> wrot_frm->hor_sh_uv) *
			  wrot_frm->bbp_uv >> 3);

		/* Target V offset */
		ofst->v = ((out_w - out_xs - 1) >>
			  wrot_frm->ver_sh_uv) * wrot_frm->uv_stride +
			  ((out_ys >> wrot_frm->hor_sh_uv) *
			  wrot_frm->bbp_uv >> 3);

		msg = "Rotate 270 degree only";
	} else if (dest->rotate == MML_ROT_270 && dest->flip) {
		if (mode == MML_MODE_RACING) {
			sram_block = (tout->h_tile_cnt - tout->tiles[idx].h_tile_no - 1) & 0x1;
			out_w = tile->out.xe + 1;
			tile_eol = tout->tiles[idx].v_tile_no == 0;
		}
		/* Target Y offset */
		ofst->y = (out_w - out_xs - 1) *
			  wrot_frm->y_stride +
			  ((out_h - out_ys) *
			  wrot_frm->bbp_y >> 3) - 1;

		/* Target U offset */
		ofst->c = ((out_w - out_xs - 1) >>
			  wrot_frm->ver_sh_uv) * wrot_frm->uv_stride +
			  (((out_h - out_ys) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;

		/* Target V offset */
		ofst->v = ((out_w - out_xs - 1) >>
			  wrot_frm->ver_sh_uv) * wrot_frm->uv_stride +
			  (((out_h - out_ys) >>
			  wrot_frm->hor_sh_uv) * wrot_frm->bbp_uv >> 3) - 1;

		msg = "Flip and Rotate 270 degree";
	}

	if (mode == MML_MODE_RACING) {
		ofst->y += wrot->data->sram_size * sram_block;
		ofst->c += wrot->data->sram_size * sram_block;
		ofst->v += wrot->data->sram_size * sram_block;
		wrot_frm->wdone[idx].sram = sram_block;
		wrot_frm->wdone[idx].eol = tile_eol;
		tout->tiles[idx].eol = tile_eol;
	}

	mml_msg("%s %s: offset Y:%#010llx U:%#010llx V:%#010llx h:%hu/%hu v:%hu/%hu (%u%s)",
		__func__, msg, ofst->y, ofst->c, ofst->v,
		tout->tiles[idx].h_tile_no, tout->h_tile_cnt,
		tout->tiles[idx].v_tile_no, tout->v_tile_cnt, sram_block,
		tile_eol ? " eol" : "");
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
	if (!MML_FMT_H_SUBSAMPLE(dest->data.format)) {
		buf->uv_blk_width = setting->main_blk_width;
		buf->uv_blk_line = setting->main_buf_line_num;
	} else {
		if (!MML_FMT_V_SUBSAMPLE(dest->data.format)) {
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
			/* YUV420 */
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
			    struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_comp_wrot *wrot = comp_to_wrot(comp);
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct wrot_frame_data *wrot_frm = wrot_frm_data(ccfg);
	u32 plane;

	/* frame data should not change between each tile */
	const struct mml_frame_dest *dest = &cfg->info.dest[wrot_frm->out_idx];
	const phys_addr_t base_pa = comp->base_pa;
	const u32 dest_fmt = dest->data.format;

	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);
	const struct mml_tile_output *tout = cfg->tile_output[ccfg->pipe];
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
	u32 buf_line_num;

	/* Fill the the tile settings */
	if (MML_FMT_COMPRESS(dest_fmt))
		wrot_tile_calc_comp(dest, wrot_frm, tile, &ofst);
	else
		wrot_tile_calc(task, wrot, dest, tout, tile, idx, cfg->info.mode,
			       wrot_frm, &ofst);

	if (cfg->info.mode == MML_MODE_RACING) {
		/* enable inline rotate and config buffer 0 or 1 */
		cmdq_pkt_write(pkt, NULL, base_pa + VIDO_IN_LINE_ROT,
			(wrot_frm->wdone[idx].sram << 1) | 0x1, U32_MAX);
	}

	/* Write Y pixel offset */
	wrot_write_ofst(pkt,
			base_pa + VIDO_OFST_ADDR,
			base_pa + VIDO_OFST_ADDR_HIGH, ofst.y);
	/* Write U pixel offset */
	wrot_write_ofst(pkt,
			base_pa + VIDO_OFST_ADDR_C,
			base_pa + VIDO_OFST_ADDR_HIGH_C, ofst.c);
	/* Write V pixel offset */
	wrot_write_ofst(pkt,
			base_pa + VIDO_OFST_ADDR_V,
			base_pa + VIDO_OFST_ADDR_HIGH_V, ofst.v);

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
	if (cfg->info.mode == MML_MODE_RACING) {
		/* line number for inline always set 16,
		 * since sram has no latency
		 */
		buf_line_num = WROT_MIN_BUF_LINE_NUM;
	} else {
		/* line number for each tile calculated by format */
		buf_line_num = setting.main_buf_line_num;
	}

	cmdq_pkt_write(pkt, NULL, base_pa + VIDO_MAIN_BUF_SIZE,
		       (setting.main_blk_width << 16) |
		       (buf_line_num << 8) |
		       (wrot_frm->filt_v << 4), U32_MAX);

	/* Set wrot interrupt bit for debug,
	 * this bit will clear to 0 after wrot done.
	 *
	 * cmdq_pkt_write(pkt, NULL, base_pa + VIDO_INT, 0x1, U32_MAX);
	 */

	/* qos accumulate tile pixel */
	wrot_frm->pixel_acc += wrot_tar_xsize * wrot_tar_ysize;

	/* no bandwidth for racing mode since wrot write to sram */
	if (cfg->info.mode != MML_MODE_RACING) {
		/* calculate qos for later use */
		plane = MML_FMT_PLANE(dest->data.format);
		wrot_frm->datasize += mml_color_get_min_y_size(dest->data.format,
			wrot_tar_xsize, wrot_tar_ysize);
		if (plane > 1)
			wrot_frm->datasize += mml_color_get_min_uv_size(dest->data.format,
				wrot_tar_xsize, wrot_tar_ysize);
		if (plane > 2)
			wrot_frm->datasize += mml_color_get_min_uv_size(dest->data.format,
				wrot_tar_xsize, wrot_tar_ysize);
	}

	mml_msg("%s min block width: %u min buf line num: %u",
		__func__, setting.main_blk_width, setting.main_buf_line_num);

	return 0;
}

static inline void mml_ir_done_2to1(struct mml_comp_wrot *wrot, bool disp_dual,
	struct cmdq_pkt *pkt, u32 pipe, u32 sram, u32 irot_h_off, u32 height)
{
	u32 wdone = 1 << sram;

	if (pipe == 0) {
		if (sram == 0)
			cmdq_pkt_wfe(pkt, wrot->event_bufa);
		else
			cmdq_pkt_wfe(pkt, wrot->event_bufb);
		/* for pipe 0, wait pipe 1 and trigger wdone */
		cmdq_pkt_write(pkt, NULL, wrot->irot_base[0] + irot_h_off,
			height, U32_MAX);
		cmdq_pkt_write(pkt, NULL, wrot->irot_base[0] + INLINEROT_WDONE,
			wdone, U32_MAX);
		if (disp_dual) {
			cmdq_pkt_write(pkt, NULL, wrot->irot_base[1] + irot_h_off,
				height, U32_MAX);
			cmdq_pkt_write(pkt, NULL, wrot->irot_base[1] + INLINEROT_WDONE,
				wdone, U32_MAX);
		}
		if (sram == 1)
			cmdq_pkt_set_event(pkt, wrot->event_buf_next);
	} else {
		if (sram == 0) {
			/* notify pipe0 buf a */
			cmdq_pkt_set_event(pkt, wrot->event_bufa);
		} else {
			/* notify pipe0 buf b and wait for loop,
			 * this prevent event set twice or buf race condition
			 */
			cmdq_pkt_set_event(pkt, wrot->event_bufb);
			cmdq_pkt_wfe(pkt, wrot->event_buf_next);
		}
	}
}

static void wrot_config_inlinerot(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_comp_wrot *wrot = comp_to_wrot(comp);
	struct wrot_frame_data *wrot_frm = wrot_frm_data(ccfg);
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_dest *dest = &cfg->info.dest[wrot_frm->out_idx];
	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);
	u32 height;
	u32 irot_h_off;

	if (dest->rotate == MML_ROT_0 || dest->rotate == MML_ROT_180)
		height = tile->out.ye - tile->out.ys + 1;
	else
		height = tile->out.xe - tile->out.xs + 1;

	/* config height for current sram buf
	 * INLINEROT_HEIGHT0 for buf a and
	 * INLINEROT_HEIGHT1 for buf b
	 * so assign reg by sram side
	 */
	irot_h_off = INLINEROT_HEIGHT0 + 4 * wrot_frm->wdone[idx].sram;

	/* config wdone to trigger inlinerot work */
	if (cfg->dual) {
		/* 2 wrot to 1 disp: wait and trigger 1 wdone
		 * 2 wrot to 2 disp: wrot0 and wrot1 sync first
		 */
		mml_ir_done_2to1(wrot, cfg->disp_dual, pkt, ccfg->pipe,
			wrot_frm->wdone[idx].sram, irot_h_off, height);
	} else if (!cfg->dual && cfg->disp_dual) {
		/* 1 wrot to 2 disp: trigger 2 wdone (dual done) */
		cmdq_pkt_write(pkt, NULL, wrot->irot_base[0] + irot_h_off,
			height, U32_MAX);
		cmdq_pkt_write(pkt, NULL,
			wrot->irot_base[0] + INLINEROT_WDONE,
			1 << wrot_frm->wdone[idx].sram, U32_MAX);
		cmdq_pkt_write(pkt, NULL, wrot->irot_base[1] + irot_h_off,
			height, U32_MAX);
		cmdq_pkt_write(pkt, NULL,
			wrot->irot_base[1] + INLINEROT_WDONE,
			1 << wrot_frm->wdone[idx].sram, U32_MAX);
	} else {
		/* 1 wrot to 1 disp: trigger 1 wdone (by pipe)
		 * both case set disp wdone for current pipe
		 */
		cmdq_pkt_write(pkt, NULL,
			wrot->irot_base[wrot_frm->sram_side] + irot_h_off,
			height, U32_MAX);
		cmdq_pkt_write(pkt, NULL,
			wrot->irot_base[wrot_frm->sram_side] + INLINEROT_WDONE,
			1 << wrot_frm->wdone[idx].sram, U32_MAX);
	}

	/* debug, make gce send irq to cmdq and mark mmp pulse */
	if (unlikely(mml_racing_wdone_eoc))
		cmdq_pkt_eoc(pkt, false);
}

static s32 wrot_wait(struct mml_comp *comp, struct mml_task *task,
		     struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_comp_wrot *wrot = comp_to_wrot(comp);
	struct wrot_frame_data *wrot_frm = wrot_frm_data(ccfg);
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	/* wait wrot frame done */
	cmdq_pkt_wfe(pkt, wrot->event_eof);

	if (task->config->info.mode == MML_MODE_RACING && wrot_frm->wdone[idx].eol) {
		wrot_config_inlinerot(comp, task, ccfg, idx);
		wrot_frm->wdone_cnt++;
	}

	return 0;
}

static s32 wrot_post(struct mml_comp *comp, struct mml_task *task,
		     struct mml_comp_config *ccfg)
{
	struct wrot_frame_data *wrot_frm = wrot_frm_data(ccfg);
	struct mml_pipe_cache *cache = &task->config->cache[ccfg->pipe];

	/* accmulate data size and use max pixel */
	cache->total_datasize += wrot_frm->datasize;
	cache->max_pixel = max(cache->max_pixel, wrot_frm->pixel_acc);

	mml_msg("%s task %p pipe %hhu data %u pixel %u eol %u",
		__func__, task, ccfg->pipe, wrot_frm->datasize, wrot_frm->pixel_acc,
		wrot_frm->wdone_cnt);

	if (task->config->info.mode == MML_MODE_RACING) {
		struct mml_comp_wrot *wrot = comp_to_wrot(comp);

		/* clear path sel back to dram */
		cmdq_pkt_write(task->pkts[ccfg->pipe], NULL, wrot->smi_larb_con,
			0, GENMASK(19, 16));
	}

	return 0;
}

static s32 update_frame_addr(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct wrot_frame_data *wrot_frm = wrot_frm_data(ccfg);
	struct mml_frame_dest *dest = &cfg->info.dest[wrot_frm->out_idx];
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];

	const u32 dest_fmt = dest->data.format;
	const u32 out_swap = MML_FMT_SWAP(dest_fmt);
	u64 addr_c, addr_v, addr;

	if (out_swap == 1 && MML_FMT_PLANE(dest_fmt) == 3)
		swap(wrot_frm->iova[1], wrot_frm->iova[2]);

	if (dest->data.secure) {
		/* TODO: for secure case setup plane offset and reg */
	}

	/* DMA_SUPPORT_AFBC */
	if (MML_FMT_COMPRESS(dest_fmt)) {
		u32 block_x;

		/* Write frame base address */
		calc_afbc_block(wrot_frm->bbp_y,
				wrot_frm->y_stride, dest->data.vert_stride,
				wrot_frm->iova, wrot_frm->plane_offset,
				&block_x, &addr_c, &addr_v, &addr);
	} else {
		addr = wrot_frm->iova[0] + wrot_frm->plane_offset[0];
		addr_c = wrot_frm->iova[1] + wrot_frm->plane_offset[1];
		addr_v = wrot_frm->iova[2] + wrot_frm->plane_offset[2];
	}
	/* update frame base address to list */
	wrot_update_addr(reuse,
			 wrot_frm->labels[WROT_LABEL_ADDR],
			 wrot_frm->labels[WROT_LABEL_ADDR_HIGH],
			 addr);
	wrot_update_addr(reuse,
			 wrot_frm->labels[WROT_LABEL_ADDR_C],
			 wrot_frm->labels[WROT_LABEL_ADDR_HIGH_C],
			 addr_c);
	wrot_update_addr(reuse,
			 wrot_frm->labels[WROT_LABEL_ADDR_V],
			 wrot_frm->labels[WROT_LABEL_ADDR_HIGH_V],
			 addr_v);
	return 0;
}

static s32 wrot_reconfig_frame(struct mml_comp *comp, struct mml_task *task,
			       struct mml_comp_config *ccfg)
{
	/* for reconfig case, no need update addr */
	if (task->config->info.mode == MML_MODE_RACING)
		return 0;
	return update_frame_addr(comp, task, ccfg);
}

static const struct mml_comp_config_ops wrot_cfg_ops = {
	.prepare = wrot_prepare,
	.buf_map = wrot_buf_map,
	.buf_prepare = wrot_buf_prepare,
	.buf_unprepare = wrot_buf_unprepare,
	.get_label_count = wrot_get_label_count,
	.frame = wrot_config_frame,
	.tile = wrot_config_tile,
	.wait = wrot_wait,
	.post = wrot_post,
	.reframe = wrot_reconfig_frame,
};

u32 wrot_datasize_get(struct mml_task *task, struct mml_comp_config *ccfg)
{
	struct wrot_frame_data *wrot_frm = wrot_frm_data(ccfg);

	return wrot_frm->datasize;
}

u32 wrot_format_get(struct mml_task *task, struct mml_comp_config *ccfg)
{
	return task->config->info.dest[ccfg->node->out_idx].data.format;
}

static const struct mml_comp_hw_ops wrot_hw_ops = {
	.pw_enable = &mml_comp_pw_enable,
	.pw_disable = &mml_comp_pw_disable,
	.clk_enable = &mml_comp_clk_enable,
	.clk_disable = &mml_comp_clk_disable,
	.qos_datasize_get = &wrot_datasize_get,
	.qos_format_get = &wrot_format_get,
	.qos_set = &mml_comp_qos_set,
	.qos_clear = &mml_comp_qos_clear,
};

static const char *wrot_state(u32 state)
{
	switch (state) {
	case 0x0:
		return "sof";
	case 0x1:
		return "frame done";
	default:
		return "";
	}
}

static void wrot_debug_dump(struct mml_comp *comp)
{
	void __iomem *base = comp->base;
	struct mml_comp_wrot *wrot = comp_to_wrot(comp);
	u32 value[33];
	u32 debug[33];
	u32 dbg_id = 0, state, smi_req;
	u32 shadow_ctrl;
	u32 i;

	mml_err("wrot component %u dump:", comp->id);

	/* Enable shadow read working */
	shadow_ctrl = readl(base + VIDO_SHADOW_CTRL);
	shadow_ctrl |= 0x4;
	writel(shadow_ctrl, base + VIDO_SHADOW_CTRL);

	value[0] = readl(base + VIDO_CTRL);
	value[1] = readl(base + VIDO_DMA_PERF);
	value[2] = readl(base + VIDO_MAIN_BUF_SIZE);
	value[3] = readl(base + VIDO_SOFT_RST);
	value[4] = readl(base + VIDO_SOFT_RST_STAT);
	value[5] = readl(base + VIDO_INT);
	value[6] = readl(base + VIDO_IN_SIZE);
	value[7] = readl(base + VIDO_CROP_OFST);
	value[8] = readl(base + VIDO_TAR_SIZE);
	value[9] = readl(base + VIDO_FRAME_SIZE);
	value[10] = readl(base + VIDO_OFST_ADDR_HIGH);
	value[11] = readl(base + VIDO_OFST_ADDR);
	value[12] = readl(base + VIDO_OFST_ADDR_HIGH_C);
	value[13] = readl(base + VIDO_OFST_ADDR_C);
	value[14] = readl(base + VIDO_OFST_ADDR_HIGH_V);
	value[15] = readl(base + VIDO_OFST_ADDR_V);
	value[16] = readl(base + VIDO_STRIDE);
	value[17] = readl(base + VIDO_STRIDE_C);
	value[18] = readl(base + VIDO_STRIDE_V);
	value[19] = readl(base + VIDO_CTRL_2);
	value[20] = readl(base + VIDO_IN_LINE_ROT);
	value[21] = readl(base + VIDO_EOL_SEL);
	value[22] = readl(base + VIDO_ROT_EN);
	value[23] = readl(base + VIDO_SHADOW_CTRL);
	value[24] = readl(base + VIDO_PVRIC);
	value[25] = readl(base + VIDO_SCAN_10BIT);
	value[26] = readl(base + VIDO_PENDING_ZERO);
	value[27] = readl(base + VIDO_BASE_ADDR_HIGH);
	value[28] = readl(base + VIDO_BASE_ADDR);
	value[29] = readl(base + VIDO_BASE_ADDR_HIGH_C);
	value[30] = readl(base + VIDO_BASE_ADDR_C);
	value[31] = readl(base + VIDO_BASE_ADDR_HIGH_V);
	value[32] = readl(base + VIDO_BASE_ADDR_V);

	/* debug id from 0x0100 ~ 0x2100, count 33 which is debug array size */
	for (i = 0; i < ARRAY_SIZE(debug); i++) {
		dbg_id += 0x100;
		writel(dbg_id, base + VIDO_INT_EN);
		debug[i] = readl(base + VIDO_DEBUG);
	}

	mml_err("VIDO_CTRL %#010x VIDO_DMA_PERF %#010x VIDO_MAIN_BUF_SIZE %#010x",
		value[0], value[1], value[2]);
	mml_err("VIDO_SOFT_RST %#010x VIDO_SOFT_RST_STAT %#010x VIDO_INT %#010x",
		value[3], value[4], value[5]);
	mml_err("VIDO_IN_SIZE %#010x VIDO_CROP_OFST %#010x VIDO_TAR_SIZE %#010x",
		value[6], value[7], value[8]);
	mml_err("VIDO_FRAME_SIZE %#010x",
		value[9]);
	mml_err("VIDO_OFST ADDR_HIGH   %#010x ADDR   %#010x",
		value[10], value[11]);
	mml_err("VIDO_OFST ADDR_HIGH_C %#010x ADDR_C %#010x",
		value[12], value[13]);
	mml_err("VIDO_OFST ADDR_HIGH_V %#010x ADDR_V %#010x",
		value[14], value[15]);
	mml_err("VIDO_STRIDE %#010x C %#010x V %#010x",
		value[16], value[17], value[18]);
	mml_err("VIDO_CTRL_2 %#010x VIDO_IN_LINE_ROT %#010x VIDO_EOL_SEL %#010x",
		value[19], value[20], value[21]);
	mml_err("VIDO_ROT_EN %#010x VIDO_SHADOW_CTRL %#010x",
		value[22], value[23]);
	mml_err("VIDO_PVRIC %#010x VIDO_SCAN_10BIT %#010x VIDO_PENDING_ZERO %#010x",
		value[24], value[25], value[26]);
	mml_err("VIDO_BASE ADDR_HIGH   %#010x ADDR   %#010x",
		value[27], value[28]);
	mml_err("VIDO_BASE ADDR_HIGH_C %#010x ADDR_C %#010x",
		value[29], value[30]);
	mml_err("VIDO_BASE ADDR_HIGH_V %#010x ADDR_V %#010x",
		value[31], value[32]);

	for (i = 0; i < ARRAY_SIZE(debug) / 3; i++) {
		mml_err("VIDO_DEBUG %02X %#010x VIDO_DEBUG %02X %#010x VIDO_DEBUG %02X %#010x",
			i * 3 + 1, debug[i * 3],
			i * 3 + 2, debug[i * 3 + 1],
			i * 3 + 3, debug[i * 3 + 2]);
	}

	/* parse state */
	mml_err("WROT crop_busy:%u req:%u valid:%u",
		(debug[2] >> 1) & 0x1, (debug[2] >> 2) & 0x1,
		(debug[2] >> 3) & 0x1);
	state = debug[2] & 0x1;
	smi_req = (debug[24] >> 30) & 0x1;
	mml_err("WROT state: %#x (%s)", state, wrot_state(state));
	mml_err("WROT x_cnt %u y_cnt %u",
		debug[9] & 0xffff, (debug[9] >> 16) & 0xffff);
	mml_err("WROT smi_req:%u => suggest to ask SMI help:%u", smi_req, smi_req);

	/* inlinerot debug */
	if (wrot->irot_va[0]) {
		value[0] = readl(wrot->irot_va[0] + INLINEROT_OVLSEL);
		mml_err("INLINEROT0 INLINEROT_OVLSEL %#x", value[0]);
	}
	if (wrot->irot_va[1]) {
		value[1] = readl(wrot->irot_va[1] + INLINEROT_OVLSEL);
		mml_err("INLINEROT1 INLINEROT_OVLSEL %#x", value[1]);
	}
}

static void wrot_reset(struct mml_comp *comp, struct mml_frame_config *cfg, u32 pipe)
{
	const struct mml_topology_path *path = cfg->path[pipe];
	struct mml_comp_wrot *wrot = comp_to_wrot(comp);

	if (cfg->info.mode == MML_MODE_RACING) {
		cmdq_clear_event(path->clt->chan, wrot->event_bufa);
		cmdq_clear_event(path->clt->chan, wrot->event_bufb);
		cmdq_clear_event(path->clt->chan, wrot->event_buf_next);
	}
}

static const struct mml_comp_debug_ops wrot_debug_ops = {
	.dump = &wrot_debug_dump,
	.reset = &wrot_reset,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_wrot *wrot = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	s32 ret;

	if (!drm_dev) {
		ret = mml_register_comp(master, &wrot->comp);
		if (ret)
			dev_err(dev, "Failed to register mml component %s: %d\n",
				dev->of_node->full_name, ret);
	} else {
		ret = mml_ddp_comp_register(drm_dev, &wrot->ddp_comp);
		if (ret)
			dev_err(dev, "Failed to register ddp component %s: %d\n",
				dev->of_node->full_name, ret);
		else
			wrot->ddp_bound = true;
	}
	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_wrot *wrot = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	if (!drm_dev) {
		mml_unregister_comp(master, &wrot->comp);
	} else {
		mml_ddp_comp_unregister(drm_dev, &wrot->ddp_comp);
		wrot->ddp_bound = false;
	}
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static const struct mtk_ddp_comp_funcs ddp_comp_funcs = {
};

phys_addr_t mml_get_node_base_pa(struct platform_device *pdev, const char *name,
	u32 idx, void __iomem **base)
{
	struct device *dev = &pdev->dev;
	struct device_node *node;
	struct resource res;
	phys_addr_t base_pa = 0;

	node = of_parse_phandle(dev->of_node, name, idx);
	if (!node)
		goto done;

	if (of_address_to_resource(node, 0, &res))
		goto done;

	base_pa = res.start;
	*base = of_iomap(node, 0);
	mml_log("[wrot]%s%u %pa %p", name, idx, &base_pa, *base);

done:
	if (node)
		of_node_put(node);
	return base_pa;
}

static struct mml_comp_wrot *dbg_probed_components[4];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_comp_wrot *priv;
	s32 ret;
	bool add_ddp = true;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->data = of_device_get_match_data(dev);
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

	/* store smi larb con register for later use */
	priv->smi_larb_con = priv->comp.larb_base +
		SMI_LARB_NON_SEC_CON + priv->comp.larb_port * 4;
	mutex_init(&priv->sram_mutex);

	of_property_read_u16(dev->of_node, "event_frame_done",
			     &priv->event_eof);
	of_property_read_u16(dev->of_node, "event_bufa",
			     &priv->event_bufa);
	of_property_read_u16(dev->of_node, "event_bufb",
			     &priv->event_bufb);
	of_property_read_u16(dev->of_node, "event_buf_next",
			     &priv->event_buf_next);

	/* get index of wrot by alias */
	priv->idx = of_alias_get_id(dev->of_node, "mml_wrot");

	/* parse inline rot node for racing mode */
	priv->irot_base[0] = mml_get_node_base_pa(pdev, "inlinerot", 0, &priv->irot_va[0]);
	priv->irot_base[1] = mml_get_node_base_pa(pdev, "inlinerot", 1, &priv->irot_va[1]);

	/* assign ops */
	priv->comp.tile_ops = &wrot_tile_ops;
	priv->comp.config_ops = &wrot_cfg_ops;
	priv->comp.hw_ops = &wrot_hw_ops;
	priv->comp.debug_ops = &wrot_debug_ops;

	ret = mml_ddp_comp_init(dev, &priv->ddp_comp, &priv->comp,
				&ddp_comp_funcs);
	if (ret) {
		mml_log("failed to init ddp component: %d", ret);
		add_ddp = false;
	}

	dbg_probed_components[dbg_probed_count++] = priv;

	ret = component_add(dev, &mml_comp_ops);
	if (add_ddp)
		ret = component_add(dev, &mml_comp_ops);
	if (ret)
		dev_err(dev, "Failed to add component: %d\n", ret);

	mml_log("wrot%d (%u) smi larb con %#lx event eof %hu sync %hu/%hu/%hu",
		priv->idx, priv->comp.id, priv->smi_larb_con,
		priv->event_eof,
		priv->event_bufa,
		priv->event_bufb,
		priv->event_buf_next);

	return ret;
}

static int remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mml_comp_ops);
	component_del(&pdev->dev, &mml_comp_ops);
	return 0;
}

const struct of_device_id mml_wrot_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6983-mml_wrot",
		.data = &mml_wrot_data,
	},
	{
		.compatible = "mediatek,mt6893-mml_wrot",
		.data = &mml_wrot_data
	},
	{
		.compatible = "mediatek,mt6879-mml_wrot",
		.data = &mml_wrot_data
	},
	{
		.compatible = "mediatek,mt6895-mml_wrot",
		.data = &mml_wrot_data
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_wrot_driver_dt_match);

struct platform_driver mml_wrot_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-wrot",
		.owner = THIS_MODULE,
		.of_match_table = mml_wrot_driver_dt_match,
	},
};

//module_platform_driver(mml_wrot_driver);

static s32 dbg_case;
static s32 dbg_set(const char *val, const struct kernel_param *kp)
{
	s32 result;

	result = kstrtos32(val, 0, &dbg_case);
	mml_log("%s: debug_case=%d", __func__, dbg_case);

	switch (dbg_case) {
	case 0:
		mml_log("use read to dump component status");
		break;
	default:
		mml_err("invalid debug_case: %d", dbg_case);
		break;
	}
	return result;
}

static s32 dbg_get(char *buf, const struct kernel_param *kp)
{
	s32 length = 0;
	u32 i;

	switch (dbg_case) {
	case 0:
		length += snprintf(buf + length, PAGE_SIZE - length,
			"[%d] probed count: %d\n", dbg_case, dbg_probed_count);
		for (i = 0; i < dbg_probed_count; i++) {
			struct mml_comp *comp = &dbg_probed_components[i]->comp;

			length += snprintf(buf + length, PAGE_SIZE - length,
				"  - [%d] mml comp_id: %d.%d @%llx name: %s bound: %d\n", i,
				comp->id, comp->sub_idx, comp->base_pa,
				comp->name ? comp->name : "(null)", comp->bound);
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  -         larb_port: %d @%llx pw: %d clk: %d\n",
				comp->larb_port, comp->larb_base,
				comp->pw_cnt, comp->clk_cnt);
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  -     ddp comp_id: %d bound: %d\n",
				dbg_probed_components[i]->ddp_comp.id,
				dbg_probed_components[i]->ddp_bound);
		}
		break;
	default:
		mml_err("not support read for debug_case: %d", dbg_case);
		break;
	}
	buf[length] = '\0';

	return length;
}

static const struct kernel_param_ops dbg_param_ops = {
	.set = dbg_set,
	.get = dbg_get,
};
module_param_cb(wrot_debug, &dbg_param_ops, NULL, 0644);
MODULE_PARM_DESC(wrot_debug, "mml wrot debug case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML WROT driver");
MODULE_LICENSE("GPL v2");
