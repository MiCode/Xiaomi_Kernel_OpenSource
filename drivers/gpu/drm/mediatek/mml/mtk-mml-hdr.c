/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/component.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <mtk_drm_ddp_comp.h>

#include "mtk-mml-color.h"
#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"
#include "mtk-mml-dle-adaptor.h"
#include "mtk-mml-pq-core.h"

#include "mtk-mml-driver.h"
#include "tile_driver.h"
#include "mtk-mml-tile.h"
#include "tile_mdp_func.h"

#undef pr_fmt
#define pr_fmt(fmt) "[mml_pq_hdr]" fmt


#define HDR_TOP					(0x00000000)
#define HDR_RELAY				(0x00000004)
#define HDR_INTERR				(0x00000008)
#define HDR_INTSTA				(0x0000000C)
#define HDR_ENGSTA				(0x00000010)
#define HDR_SIZE_0				(0x00000014)
#define HDR_SIZE_1				(0x00000018)
#define HDR_SIZE_2				(0x0000001C)
#define HDR_HIST_CTRL_0			(0x00000020)
#define HDR_HIST_CTRL_1			(0x00000024)
#define HDR_HIST_CTRL_2			(0x00000028)
#define HDR_DEMO_CTRL_0			(0x0000002C)
#define HDR_DEMO_CTRL_1			(0x00000030)
#define HDR_DEMO_CTRL_2			(0x00000034)
#define HDR_3x3_COEF_0			(0x00000038)
#define HDR_3x3_COEF_1			(0x0000003C)
#define HDR_3x3_COEF_2			(0x00000040)
#define HDR_3x3_COEF_3			(0x00000044)
#define HDR_3x3_COEF_4			(0x00000048)
#define HDR_3x3_COEF_5			(0x0000004C)
#define HDR_3x3_COEF_6			(0x00000050)
#define HDR_3x3_COEF_7			(0x00000054)
#define HDR_3x3_COEF_8			(0x00000058)
#define HDR_3x3_COEF_9			(0x0000005C)
#define HDR_3x3_COEF_10			(0x00000060)
#define HDR_3x3_COEF_11			(0x00000064)
#define HDR_3x3_COEF_12			(0x00000068)
#define HDR_3x3_COEF_13			(0x0000006C)
#define HDR_3x3_COEF_14			(0x00000070)
#define HDR_3x3_COEF_15			(0x00000074)
#define HDR_TONE_MAP_P01		(0x00000078)
#define HDR_TONE_MAP_P02		(0x0000007C)
#define HDR_TONE_MAP_P03		(0x00000080)
#define HDR_TONE_MAP_P04		(0x00000084)
#define HDR_TONE_MAP_P05		(0x00000088)
#define HDR_TONE_MAP_P06		(0x0000008C)
#define HDR_TONE_MAP_P07		(0x00000090)
#define HDR_TONE_MAP_P08		(0x00000094)
#define HDR_TONE_MAP_S00		(0x00000098)
#define HDR_TONE_MAP_S01		(0x0000009C)
#define HDR_TONE_MAP_S02		(0x000000A0)
#define HDR_TONE_MAP_S03		(0x000000A4)
#define HDR_TONE_MAP_S04		(0x000000A8)
#define HDR_TONE_MAP_S05		(0x000000AC)
#define HDR_TONE_MAP_S06		(0x000000B0)
#define HDR_TONE_MAP_S07		(0x000000B4)
#define HDR_TONE_MAP_S08		(0x000000B8)
#define HDR_TONE_MAP_S09		(0x000000BC)
#define HDR_TONE_MAP_S10		(0x000000C0)
#define HDR_TONE_MAP_S11		(0x000000C4)
#define HDR_TONE_MAP_S12		(0x000000C8)
#define HDR_TONE_MAP_S13		(0x000000CC)
#define HDR_TONE_MAP_S14		(0x000000D0)
#define HDR_TONE_MAP_S15		(0x000000D4)
#define HDR_B_CHANNEL_NR		(0x000000D8)
#define HDR_HIST_ADDR			(0x000000DC)
#define HDR_HIST_DATA			(0x000000E0)
#define HDR_A_LUMINANCE			(0x000000E4)
#define HDR_GAIN_TABLE_0		(0x000000E8)
#define HDR_GAIN_TABLE_1		(0x000000EC)
#define HDR_GAIN_TABLE_2		(0x000000F0)
#define HDR_LBOX_DET_1			(0x000000F8)
#define HDR_LBOX_DET_2			(0x000000FC)
#define HDR_LBOX_DET_3			(0x00000100)
#define HDR_LBOX_DET_4			(0x00000104)
#define HDR_LBOX_DET_5			(0x00000108)
#define HDR_CURSOR_CTRL			(0x0000010C)
#define HDR_CURSOR_POS			(0x00000110)
#define HDR_CURSOR_COLOR		(0x00000114)
#define HDR_TILE_POS			(0x00000118)
#define HDR_CURSOR_BUF0			(0x0000011C)
#define HDR_CURSOR_BUF1			(0x00000120)
#define HDR_CURSOR_BUF2			(0x00000124)
#define HDR_R2Y_00				(0x00000128)
#define HDR_R2Y_01				(0x0000012C)
#define HDR_R2Y_02				(0x00000130)
#define HDR_R2Y_03				(0x00000134)
#define HDR_R2Y_04				(0x00000138)
#define HDR_R2Y_05				(0x0000013C)
#define HDR_R2Y_06				(0x00000140)
#define HDR_R2Y_07				(0x00000144)
#define HDR_R2Y_08				(0x00000148)
#define HDR_R2Y_09				(0x0000014C)
#define HDR_Y2R_00				(0x00000150)
#define HDR_Y2R_01				(0x00000154)
#define HDR_Y2R_02				(0x0000015C)
#define HDR_Y2R_03				(0x00000160)
#define HDR_Y2R_04				(0x00000164)
#define HDR_Y2R_05				(0x00000168)
#define HDR_Y2R_06				(0x0000016C)
#define HDR_Y2R_07				(0x00000170)
#define HDR_Y2R_08				(0x00000174)
#define HDR_Y2R_09				(0x00000178)
#define HDR_Y2R_10				(0x0000017C)
#define HDR_PROG_EOTF_0			(0x00000180)
#define HDR_PROG_EOTF_1			(0x00000184)
#define HDR_PROG_EOTF_2			(0x00000188)
#define HDR_PROG_EOTF_3			(0x0000018C)
#define HDR_PROG_EOTF_4			(0x00000190)
#define HDR_PROG_EOTF_5			(0x00000194)
#define HDR_EOTF_TABLE_0		(0x0000019C)
#define HDR_EOTF_TABLE_1		(0x000001A0)
#define HDR_EOTF_TABLE_2		(0x000001A4)
#define HDR_OETF_TABLE_0		(0x000001A8)
#define HDR_OETF_TABLE_1		(0x000001AC)
#define TONE_MAP_TOP			(0x000001B0)
#define HDR_EOTF_ACCURACY_0		(0x000001B4)
#define HDR_EOTF_ACCURACY_1		(0x000001B8)
#define HDR_EOTF_ACCURACY_2		(0x000001BC)
#define HDR_L_MIX_0				(0x000001C0)
#define HDR_L_MIX_1				(0x000001C4)
#define HDR_L_MIX_2				(0x000001C8)
#define HDR_Y_GAIN_IDX_0		(0x000001CC)
#define HDR_Y_GAIN_IDX_1		(0x000001D0)
#define HDR_DUMMY0				(0x000001D4)
#define HDR_DUMMY1				(0x000001D8)
#define HDR_DUMMY2				(0x000001DC)
#define HDR_HLG_SG				(0x000001E0)

#define HDR_WAIT_TIMEOUT_MS (50)
#define HDR_REG_NUM (70)

struct hdr_data {
	u32 min_tile_width;
};

static const struct hdr_data hdr10_hdr_data = {
	.min_tile_width = 16,
};

struct mml_comp_hdr {
	struct mtk_ddp_comp ddp_comp;
	struct mml_comp comp;
	const struct hdr_data *data;
	bool ddp_bound;
};

/* meta data for each different frame config */
struct hdr_frame_data {
	u8 out_idx;
	u32 out_hist_xs;
	u16 labels[HDR_CURVE_NUM+HDR_REG_NUM];
	bool is_hdr_need_readback;
};

#define hdr_frm_data(i)	((struct hdr_frame_data *)(i->data))

static inline struct mml_comp_hdr *comp_to_hdr(struct mml_comp *comp)
{
	return container_of(comp, struct mml_comp_hdr, comp);
}

static s32 hdr_prepare(struct mml_comp *comp, struct mml_task *task,
		       struct mml_comp_config *ccfg)
{
	struct hdr_frame_data *hdr_frm;

	hdr_frm = kzalloc(sizeof(*hdr_frm), GFP_KERNEL);
	ccfg->data = hdr_frm;
	/* cache out index for easy use */
	hdr_frm->out_idx = ccfg->node->out_idx;

	return 0;
}

static s32 hdr_buf_prepare(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	struct hdr_frame_data *hdr_frm = hdr_frm_data(ccfg);
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_dest *dest = &cfg->info.dest[hdr_frm->out_idx];
	s32 ret = 0;

	mml_pq_msg("%s pipe_id[%d] engine_id[%d]", __func__,
		ccfg->pipe, comp->id);
	if (dest->pq_config.en_hdr)
		ret = mml_pq_set_comp_config(task);

	return ret;
}


static s32 hdr_tile_prepare(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg,
			    struct tile_func_block *func,
			    union mml_tile_data *data)
{
	struct hdr_frame_data *hdr_frm = hdr_frm_data(ccfg);
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_data *src = &cfg->info.src;
	struct mml_frame_dest *dest = &cfg->info.dest[hdr_frm->out_idx];
	struct mml_comp_hdr *hdr = comp_to_hdr(comp);

	data->hdr_data.relay_mode = dest->pq_config.en_hdr ? false : true;
	data->hdr_data.min_width = hdr->data->min_tile_width;
	func->init_func_ptr = tile_hdr_init;
	func->for_func_ptr = tile_hdr_for;
	func->func_data = data;

	func->enable_flag = dest->pq_config.en_hdr;

	if (cfg->info.dest_cnt == 1 &&
	    (dest->crop.r.width != src->width ||
	    dest->crop.r.height != src->height)) {
		u32 in_crop_w, in_crop_h;

		in_crop_w = dest->crop.r.width;
		in_crop_h = dest->crop.r.height;
		if (in_crop_w + dest->crop.r.left > src->width)
			in_crop_w = src->width - dest->crop.r.left;
		if (in_crop_h + dest->crop.r.top > src->height)
			in_crop_h = src->height - dest->crop.r.top;
		func->full_size_x_in = in_crop_w + dest->crop.r.left;
		func->full_size_y_in = in_crop_h + dest->crop.r.top;
	} else {
 		func->full_size_x_in = src->width;
		func->full_size_y_in = src->height;
	}
	func->full_size_x_out = func->full_size_x_in;
	func->full_size_y_out = func->full_size_y_in;

	return 0;
}

static const struct mml_comp_tile_ops hdr_tile_ops = {
	.prepare = hdr_tile_prepare,
};

static u32 hdr_get_label_count(struct mml_comp *comp, struct mml_task *task,
			struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct hdr_frame_data *hdr_frm = hdr_frm_data(ccfg);
	const struct mml_frame_dest *dest = &cfg->info.dest[hdr_frm->out_idx];

	mml_pq_msg("%s pipe_id[%d] engine_id[%d] en_hdr[%d]", __func__,
		ccfg->pipe, comp->id, dest->pq_config.en_hdr);

	if (!dest->pq_config.en_hdr)
		return 0;

	return HDR_CURVE_NUM+HDR_REG_NUM;
}

static void hdr_start_config(struct cmdq_pkt *pkt, const phys_addr_t base_pa,
			     bool is_start)
{
	if (is_start) {
		/* Enable engine and shadow */
		cmdq_pkt_write(pkt, NULL, base_pa + HDR_TOP, 0x100001, 0x00308001);
	} else {
		/* Disable engine */
		cmdq_pkt_write(pkt, NULL, base_pa + HDR_TOP, 0x0, 0x00000001);
	}
}

static s32 hdr_init(struct mml_comp *comp, struct mml_task *task,
		    struct mml_comp_config *ccfg)
{
	mml_pq_msg("%s pipe_id[%d] engine_id[%d]", __func__, ccfg->pipe, comp->id);

	hdr_start_config(task->pkts[ccfg->pipe], comp->base_pa, true);
	return 0;
}

static struct mml_pq_comp_config_result *get_hdr_comp_config_result(
	struct mml_task *task)
{
	struct mml_pq_sub_task *sub_task = &task->pq_task->comp_config;

	return (struct mml_pq_comp_config_result *)sub_task->result;
}

static s32 hdr_config_frame(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	struct hdr_frame_data *hdr_frm = hdr_frm_data(ccfg);
	struct mml_frame_data *src = &cfg->info.src;
	const struct mml_frame_dest *dest = &cfg->info.dest[hdr_frm->out_idx];
	const phys_addr_t base_pa = comp->base_pa;
	struct mml_pq_comp_config_result *result = NULL;
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];
	s32 ret = 0;

	mml_pq_msg("%s pipe_id[%d] engine_id[%d] 12345 en_hdr[%d]", __func__,
		ccfg->pipe, comp->id, dest->pq_config.en_hdr);

	if (MML_FMT_10BIT(src->format) ||
	    MML_FMT_10BIT(dest->data.format))
		cmdq_pkt_write(pkt, NULL, base_pa + HDR_TOP,
			3 << 28, 0x30000000);
	else
		cmdq_pkt_write(pkt, NULL, base_pa + HDR_TOP,
			1 << 28, 0x30000000);

	if (!dest->pq_config.en_hdr) {
		/* relay mode */
		cmdq_pkt_write(pkt, NULL, base_pa + HDR_RELAY, 0x1, U32_MAX);
		return ret;
	}

	cmdq_pkt_write(pkt, NULL, base_pa + HDR_RELAY, 0x0, U32_MAX);

	ret = mml_pq_get_comp_config_result(task, HDR_WAIT_TIMEOUT_MS);
	if (!ret) {
		result = get_hdr_comp_config_result(task);
		if (result) {
			s32 i;
			s32 curve_idx = 0;
			struct mml_pq_reg *regs = result->hdr_regs;
			u32 *curve = result->hdr_curve;
			//TODO: use different regs
			mml_pq_msg("%s:config hdr regs, count: %d", __func__, result->hdr_reg_cnt);
			for (i = 0; i < result->hdr_reg_cnt; i++) {
				mml_write(pkt, base_pa + regs[i].offset, regs[i].value,
					regs[i].mask, reuse, cache,
					&hdr_frm->labels[i]);
				mml_pq_msg("[hdr][config][%x] = %#x mask(%#x)",
					regs[i].offset, regs[i].value, regs[i].mask);
			}
			while (i < HDR_CURVE_NUM + result->hdr_reg_cnt) {
				mml_write(pkt, base_pa + HDR_GAIN_TABLE_1, curve[curve_idx],
					U32_MAX, reuse, cache, &hdr_frm->labels[i]);
				mml_write(pkt, base_pa + HDR_GAIN_TABLE_2, curve[curve_idx+1],
					U32_MAX, reuse, cache, &hdr_frm->labels[i+1]);
				i = i+2;
				curve_idx = curve_idx+2;
			}
			cmdq_pkt_write(pkt, NULL, base_pa + HDR_GAIN_TABLE_0,
					1 << 11, 1 << 11);
			mml_pq_msg("%s is_hdr_need_readback[%d]", __func__,
				result->is_hdr_need_readback);
			hdr_frm->is_hdr_need_readback = result->is_hdr_need_readback;
		} else {
			mml_pq_err("%s: not get result from user lib", __func__);
		}
	} else {
		mml_pq_err("get hdr param timeout: %d in %dms",
			ret, HDR_WAIT_TIMEOUT_MS);
	}

	return 0;
}

static s32 hdr_config_tile(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;
	struct hdr_frame_data *hdr_frm = hdr_frm_data(ccfg);

	const struct mml_frame_dest *dest = &cfg->info.dest[hdr_frm->out_idx];
	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);
	u16 tile_cnt = cfg->tile_output[ccfg->pipe]->tile_cnt;
	u32 hdr_input_w;
	u32 hdr_input_h;
	u32 hdr_crop_xs;
	u32 hdr_crop_xe;
	u32 hdr_crop_ye;
	u32 hdr_hist_left_start = 0;
	u32 hdr_hist_begin_x = 0;
	u32 hdr_hist_end_x = 0;
	u32 hdr_first_tile = 0;
	u32 hdr_last_tile = 0;

	mml_pq_msg("%s pipe_id[%d] engine_id[%d] en_hdr[%d]", __func__,
		ccfg->pipe, comp->id, dest->pq_config.en_hdr);

	hdr_input_w = tile->in.xe - tile->in.xs + 1;
	hdr_input_h = tile->in.ye - tile->in.ys + 1;
	hdr_crop_xs = tile->out.xs - tile->in.xs;
	hdr_crop_xe = tile->out.xe - tile->in.xs;
	hdr_crop_ye = tile->in.ye - tile->in.ys;

	cmdq_pkt_write(pkt, NULL, base_pa + HDR_TILE_POS,
		(tile->out.ys << 16) + tile->out.xs, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + HDR_SIZE_0,
		(hdr_input_h << 16) + hdr_input_w, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + HDR_SIZE_1,
		(hdr_crop_xe << 16) + hdr_crop_xs, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + HDR_SIZE_2,
		hdr_crop_ye << 16, U32_MAX);

	mml_pq_msg("%s %d: [input] [xs, xe] = [%d, %d], [ys, ye] = [%d, %d]",
			__func__, idx, tile->in.xs, tile->in.xe, tile->in.ys, tile->in.ye);
	mml_pq_msg("%s %d: [output] [xs, xe] = [%d, %d], [ys, ye] = [%d, %d]",
			__func__, idx, tile->out.xs, tile->out.xe, tile->out.ys,
			tile->out.ye);

	if (!dest->pq_config.en_hdr)
		return 0;

	hdr_hist_left_start =
		(tile->out.xs > hdr_frm->out_hist_xs) ? tile->out.xs : hdr_frm->out_hist_xs;
	hdr_hist_begin_x = hdr_hist_left_start - tile->in.xs;
	hdr_hist_end_x = tile->out.xe - tile->in.xs;

	cmdq_pkt_write(pkt, NULL, base_pa + HDR_HIST_CTRL_0, hdr_hist_begin_x, 0x00003FFF);
	cmdq_pkt_write(pkt, NULL, base_pa + HDR_HIST_CTRL_1, hdr_hist_end_x, 0x00003FFF);

	if (!idx) {
		hdr_frm->out_hist_xs = tile->out.xe+1;
		hdr_first_tile = 1;
		hdr_last_tile = 0;
	} else if (idx+1 >= tile_cnt) {
		hdr_frm->out_hist_xs = 0;
		hdr_first_tile = 0;
		hdr_last_tile = 1;
	} else {
		hdr_frm->out_hist_xs = tile->out.xe+1;
		hdr_first_tile = 0;
		hdr_last_tile = 0;
	}

	cmdq_pkt_write(pkt, NULL, base_pa + HDR_TOP, (hdr_first_tile << 5) | (hdr_last_tile << 6),
		0x00000060);
	cmdq_pkt_write(pkt, NULL, base_pa + HDR_HIST_ADDR, (1 << 9), 0x00000200);

	mml_pq_msg("%s %d: hdr_hist_begin_x[%d] hdr_hist_end_x[%d] out_hist_xs[%d]",
		__func__, idx, hdr_hist_begin_x, hdr_hist_end_x,
		hdr_frm->out_hist_xs);
	mml_pq_msg("%s %d: hdr_first_tile[%d] hdr_last_tile[%d] out_hist_xs[%d]",
		__func__, idx, hdr_first_tile, hdr_last_tile,
		hdr_frm->out_hist_xs);

	return 0;
}

static s32 hdr_config_post(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	struct hdr_frame_data *hdr_frm = hdr_frm_data(ccfg);
	struct mml_frame_dest *dest = &task->config->info.dest[hdr_frm->out_idx];

	if (dest->pq_config.en_hdr)
		mml_pq_put_comp_config_result(task);
	return 0;
}

static s32 hdr_reconfig_frame(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct hdr_frame_data *hdr_frm = hdr_frm_data(ccfg);
	const struct mml_frame_dest *dest = &cfg->info.dest[hdr_frm->out_idx];
	struct mml_pq_comp_config_result *result = NULL;
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	s32 ret = 0;

	mml_pq_msg("%s pipe_id[%d] engine_id[%d] en_hdr[%d]", __func__,
		ccfg->pipe, comp->id, dest->pq_config.en_hdr);

	if (!dest->pq_config.en_hdr)
		return ret;

	ret = mml_pq_get_comp_config_result(task, HDR_WAIT_TIMEOUT_MS);
	if (!ret) {
		result = get_hdr_comp_config_result(task);
		if (result) {
			s32 i = 0;
			s32 curve_idx = 0;
			struct mml_pq_reg *regs = result->hdr_regs;
			u32 *curve = result->hdr_curve;

			for (i = 0; i < result->hdr_reg_cnt; i++)
				mml_update(reuse, hdr_frm->labels[i], regs[i].value);

			while (i < HDR_CURVE_NUM + result->hdr_reg_cnt) {
				mml_update(reuse, hdr_frm->labels[i], curve[curve_idx]);
				mml_update(reuse, hdr_frm->labels[i+1], curve[curve_idx+1]);
				i = i + 2;
				curve_idx = curve_idx + 2;
			}
			mml_pq_msg("%s is_hdr_need_readback[%d]", __func__,
				result->is_hdr_need_readback);
			hdr_frm->is_hdr_need_readback = result->is_hdr_need_readback;
		} else {
			mml_pq_err("%s: not get result from user lib", __func__);
		}
	} else {
		mml_pq_err("get aal param timeout: %d in %dms",
			ret, HDR_WAIT_TIMEOUT_MS);
	}
	return 0;
}

static const struct mml_comp_config_ops hdr_cfg_ops = {
	.prepare = hdr_prepare,
	.buf_prepare = hdr_buf_prepare,
	.get_label_count = hdr_get_label_count,
	.init = hdr_init,
	.frame = hdr_config_frame,
	.tile = hdr_config_tile,
	.post = hdr_config_post,
	.reframe = hdr_reconfig_frame,
	.repost = hdr_config_post,
};

static void hdr_task_done_readback(struct mml_comp *comp, struct mml_task *task,
					 struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct hdr_frame_data *hdr_frm = hdr_frm_data(ccfg);
	struct mml_frame_dest *dest = &cfg->info.dest[hdr_frm->out_idx];


	mml_pq_trace_ex_begin("%s", __func__);
	mml_msg("%s is_hdr_need_readback[%d] id[%d] en_hdr[%d]", __func__,
			hdr_frm->is_hdr_need_readback, comp->id, dest->pq_config.en_hdr);

	if (!dest->pq_config.en_hdr)
		goto exit;

	if (hdr_frm->is_hdr_need_readback) {
		void __iomem *base = comp->base;
		s32 i;
		u32 *phist = kmalloc(HDR_HIST_NUM*sizeof(u32), GFP_KERNEL);

		for (i = 0; i < HDR_HIST_NUM; i++) {
			if (i == 57) {
				phist[i] = readl(base + HDR_LBOX_DET_4);
				continue;
			}
			phist[i] = readl(base + HDR_HIST_DATA);
		}
		mml_pq_hdr_readback(task, ccfg->pipe, phist);
	}

exit:
	mml_pq_trace_ex_end();
}

static const struct mml_comp_hw_ops hdr_hw_ops = {
	.clk_enable = &mml_comp_clk_enable,
	.clk_disable = &mml_comp_clk_disable,
	.task_done = hdr_task_done_readback,
};

static void hdr_debug_dump(struct mml_comp *comp)
{
	void __iomem *base = comp->base;
	u32 value[16];
	u32 hdr_top;

	mml_err("hdr component %u dump:", comp->id);

	/* Enable shadow read working */
	hdr_top = readl(base + HDR_TOP);
	hdr_top |= 0x8000;
	writel(hdr_top, base + HDR_TOP);

	value[0] = readl(base + HDR_TOP);
	value[1] = readl(base + HDR_RELAY);
	value[2] = readl(base + HDR_INTSTA);
	value[3] = readl(base + HDR_ENGSTA);
	value[4] = readl(base + HDR_SIZE_0);
	value[5] = readl(base + HDR_SIZE_1);
	value[6] = readl(base + HDR_SIZE_2);
	value[7] = readl(base + HDR_HIST_CTRL_0);
	value[8] = readl(base + HDR_HIST_CTRL_1);
	value[9] = readl(base + HDR_CURSOR_CTRL);
	value[10] = readl(base + HDR_CURSOR_POS);
	value[11] = readl(base + HDR_CURSOR_COLOR);
	value[12] = readl(base + HDR_TILE_POS);
	value[13] = readl(base + HDR_CURSOR_BUF0);
	value[14] = readl(base + HDR_CURSOR_BUF1);
	value[15] = readl(base + HDR_CURSOR_BUF2);

	mml_err("HDR_TOP %#010x HDR_RELAY %#010x HDR_INTSTA %#010x HDR_ENGSTA %#010x",
		value[0], value[1], value[2], value[3]);
	mml_err("HDR_SIZE_0 %#010x HDR_SIZE_1 %#010x HDR_SIZE_2 %#010x",
		value[4], value[5], value[6]);
	mml_err("HDR_HIST_CTRL_0 %#010x HDR_HIST_CTRL_1 %#010x",
		value[7], value[8]);
	mml_err("HDR_CURSOR_CTRL %#010x HDR_CURSOR_POS %#010x HDR_CURSOR_COLOR %#010x",
		value[9], value[10], value[11]);
	mml_err("HDR_TILE_POS %#010x", value[12]);
	mml_err("HDR_CURSOR_BUF0 %#010x HDR_CURSOR_BUF1 %#010x HDR_CURSOR_BUF2 %#010x",
		value[13], value[14], value[15]);
}

static const struct mml_comp_debug_ops hdr_debug_ops = {
	.dump = &hdr_debug_dump,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_hdr *hdr = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	s32 ret;

	if (!drm_dev) {
		ret = mml_register_comp(master, &hdr->comp);
		if (ret)
			dev_err(dev, "Failed to register mml component %s: %d\n",
				dev->of_node->full_name, ret);
	} else {
		ret = mml_ddp_comp_register(drm_dev, &hdr->ddp_comp);
		if (ret)
			dev_err(dev, "Failed to register ddp component %s: %d\n",
				dev->of_node->full_name, ret);
		else
			hdr->ddp_bound = true;
	}
	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_hdr *hdr = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	if (!drm_dev) {
		mml_unregister_comp(master, &hdr->comp);
	} else {
		mml_ddp_comp_unregister(drm_dev, &hdr->ddp_comp);
		hdr->ddp_bound = false;
	}
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static inline struct mml_comp_hdr *ddp_comp_to_hdr(struct mtk_ddp_comp *ddp_comp)
{
	return container_of(ddp_comp, struct mml_comp_hdr, ddp_comp);
}

static void hdr_addon_config(struct mtk_ddp_comp *ddp_comp,
			     enum mtk_ddp_comp_id prev,
			     enum mtk_ddp_comp_id next,
			     union mtk_addon_config *addon_config,
			     struct cmdq_pkt *pkt)
{
	const phys_addr_t base_pa = ddp_comp_to_hdr(ddp_comp)->comp.base_pa;

	cmdq_pkt_write(pkt, NULL, base_pa + HDR_RELAY, 0x1, U32_MAX);
}

static void hdr_start(struct mtk_ddp_comp *ddp_comp, struct cmdq_pkt *pkt)
{
	hdr_start_config(pkt, ddp_comp_to_hdr(ddp_comp)->comp.base_pa, true);
}

static void hdr_stop(struct mtk_ddp_comp *ddp_comp, struct cmdq_pkt *pkt)
{
	hdr_start_config(pkt, ddp_comp_to_hdr(ddp_comp)->comp.base_pa, false);
}

static void hdr_ddp_prepare(struct mtk_ddp_comp *ddp_comp)
{
	struct mml_comp *comp = &ddp_comp_to_hdr(ddp_comp)->comp;

	comp->hw_ops->clk_enable(comp);
}

static void hdr_ddp_unprepare(struct mtk_ddp_comp *ddp_comp)
{
	struct mml_comp *comp = &ddp_comp_to_hdr(ddp_comp)->comp;

	comp->hw_ops->clk_disable(comp);
}

static const struct mtk_ddp_comp_funcs ddp_comp_funcs = {
	.addon_config = hdr_addon_config,
	.start = hdr_start,
	.stop = hdr_stop,
	.prepare = hdr_ddp_prepare,
	.unprepare = hdr_ddp_unprepare,
};

static struct mml_comp_hdr *dbg_probed_components[2];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_comp_hdr *priv;
	s32 ret;
	bool add_ddp = true;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->data = of_device_get_match_data(dev);

	ret = mml_comp_init(pdev, &priv->comp);
	if (ret) {
		dev_err(dev, "Failed to init mml component: %d\n", ret);
		return ret;
	}

	/* assign ops */
	priv->comp.tile_ops = &hdr_tile_ops;
	priv->comp.config_ops = &hdr_cfg_ops;
	priv->comp.hw_ops = &hdr_hw_ops;
	priv->comp.debug_ops = &hdr_debug_ops;

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

	return ret;
}

static int remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mml_comp_ops);
	component_del(&pdev->dev, &mml_comp_ops);
	return 0;
}

const struct of_device_id mml_hdr_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6983-mml_hdr",
		.data = &hdr10_hdr_data,
	},
	{
		.compatible = "mediatek,mt6893-mml_hdr",
		.data = &hdr10_hdr_data,
	},
	{
		.compatible = "mediatek,mt6879-mml_hdr",
		.data = &hdr10_hdr_data,
	},
	{
		.compatible = "mediatek,mt6895-mml_hdr",
		.data = &hdr10_hdr_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_hdr_driver_dt_match);

struct platform_driver mml_hdr_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-hdr",
		.owner = THIS_MODULE,
		.of_match_table = mml_hdr_driver_dt_match,
	},
};

//module_platform_driver(mml_hdr_driver);

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
				"  - [%d] mml comp_id: %d.%d @%08x name: %s bound: %d\n", i,
				comp->id, comp->sub_idx, comp->base_pa,
				comp->name ? comp->name : "(null)", comp->bound);
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  -         larb_port: %d @%08x pw: %d clk: %d\n",
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
module_param_cb(hdr_debug, &dbg_param_ops, NULL, 0644);
MODULE_PARM_DESC(hdr_debug, "mml hdr debug case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML HDR driver");
MODULE_LICENSE("GPL v2");
