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

#define HDR_TOP			0x000
#define HDR_RELAY		0x004
#define HDR_INTERR		0x008
#define HDR_INTSTA		0x00c
#define HDR_ENGSTA		0x010
#define HDR_SIZE_0		0x014
#define HDR_SIZE_1		0x018
#define HDR_SIZE_2		0x01c
#define HDR_HIST_CTRL_0		0x020
#define HDR_HIST_CTRL_1		0x024
#define HDR_HIST_CTRL_2		0x028
#define HDR_DEMO_CTRL_0		0x02c
#define HDR_DEMO_CTRL_1		0x030
#define HDR_DEMO_CTRL_2		0x034
#define HDR_3x3_COEF_00		0x038
#define HDR_3x3_COEF_01		0x03c
#define HDR_3x3_COEF_02		0x040
#define HDR_3x3_COEF_03		0x044
#define HDR_3x3_COEF_04		0x048
#define HDR_3x3_COEF_05		0x04c
#define HDR_3x3_COEF_06		0x050
#define HDR_3x3_COEF_07		0x054
#define HDR_3x3_COEF_08		0x058
#define HDR_3x3_COEF_09		0x05c
#define HDR_3x3_COEF_10		0x060
#define HDR_3x3_COEF_11		0x064
#define HDR_3x3_COEF_12		0x068
#define HDR_3x3_COEF_13		0x06c
#define HDR_3x3_COEF_14		0x070
#define HDR_3x3_COEF_15		0x074
#define HDR_TONE_MAP_P01	0x078
#define HDR_TONE_MAP_P02	0x07c
#define HDR_TONE_MAP_P03	0x080
#define HDR_TONE_MAP_P04	0x084
#define HDR_TONE_MAP_P05	0x088
#define HDR_TONE_MAP_P06	0x08c
#define HDR_TONE_MAP_P07	0x090
#define HDR_TONE_MAP_P08	0x094
#define HDR_TONE_MAP_S00	0x098
#define HDR_TONE_MAP_S01	0x09c
#define HDR_TONE_MAP_S02	0x0a0
#define HDR_TONE_MAP_S03	0x0a4
#define HDR_TONE_MAP_S04	0x0a8
#define HDR_TONE_MAP_S05	0x0ac
#define HDR_TONE_MAP_S06	0x0b0
#define HDR_TONE_MAP_S07	0x0b4
#define HDR_TONE_MAP_S08	0x0b8
#define HDR_TONE_MAP_S09	0x0bc
#define HDR_TONE_MAP_S10	0x0c0
#define HDR_TONE_MAP_S11	0x0c4
#define HDR_TONE_MAP_S12	0x0c8
#define HDR_TONE_MAP_S13	0x0cc
#define HDR_TONE_MAP_S14	0x0d0
#define HDR_TONE_MAP_S15	0x0d4
#define HDR_B_CHANNEL_NR	0x0d8
#define HDR_HIST_ADDR		0x0dc
#define HDR_HIST_DATA		0x0e0
#define HDR_A_LUMINANCE		0x0e4
#define HDR_GAIN_TABLE_0	0x0e8
#define HDR_GAIN_TABLE_1	0x0ec
#define HDR_GAIN_TABLE_2	0x0f0
#define HDR_LBOX_DET_1		0x0f8
#define HDR_LBOX_DET_2		0x0fc
#define HDR_LBOX_DET_3		0x100
#define HDR_LBOX_DET_4		0x104
#define HDR_LBOX_DET_5		0x108
#define HDR_CURSOR_CTRL		0x10c
#define HDR_CURSOR_POS		0x110
#define HDR_CURSOR_COLOR	0x114
#define HDR_TILE_POS		0x118
#define HDR_CURSOR_BUF0		0x11c
#define HDR_CURSOR_BUF1		0x120
#define HDR_CURSOR_BUF2		0x124
#define HDR_R2Y_00		0x128
#define HDR_R2Y_01		0x12c
#define HDR_R2Y_02		0x130
#define HDR_R2Y_03		0x134
#define HDR_R2Y_04		0x138
#define HDR_R2Y_05		0x13c
#define HDR_R2Y_06		0x140
#define HDR_R2Y_07		0x144
#define HDR_R2Y_08		0x148
#define HDR_R2Y_09		0x14c
#define HDR_Y2R_00		0x150
#define HDR_Y2R_01		0x154
#define HDR_Y2R_02		0x15c
#define HDR_Y2R_03		0x160
#define HDR_Y2R_04		0x164
#define HDR_Y2R_05		0x168
#define HDR_Y2R_06		0x16c
#define HDR_Y2R_07		0x170
#define HDR_Y2R_08		0x174
#define HDR_Y2R_09		0x178
#define HDR_Y2R_10		0x17c
#define HDR_PROG_EOTF_0		0x180
#define HDR_PROG_EOTF_1		0x184
#define HDR_PROG_EOTF_2		0x188
#define HDR_PROG_EOTF_3		0x18c
#define HDR_PROG_EOTF_4		0x190
#define HDR_PROG_EOTF_5		0x194
#define HDR_EOTF_TABLE_0	0x19c
#define HDR_EOTF_TABLE_1	0x1a0
#define HDR_EOTF_TABLE_2	0x1a4
#define HDR_OETF_TABLE_0	0x1a8
#define HDR_OETF_TABLE_1	0x1ac
#define TONE_MAP_TOP		0x1b0
#define HDR_EOTF_ACCURACY_0	0x1b4
#define HDR_EOTF_ACCURACY_1	0x1b8
#define HDR_EOTF_ACCURACY_2	0x1bc
#define HDR_L_MIX_0		0x1c0
#define HDR_L_MIX_1		0x1c4
#define HDR_L_MIX_2		0x1c8
#define HDR_Y_GAIN_IDX_0	0x1cc
#define HDR_Y_GAIN_IDX_1	0x1d0
#define HDR_DUMMY0		0x1d4
#define HDR_DUMMY1		0x1d8
#define HDR_DUMMY2		0x1dc
#define HDR_HLG_SG		0x1e0

#define HDR_WAIT_TIMEOUT_MS (50)
#define HDR_REG_NUM (70)
#define HDR_HIST_CNT (57)

struct hdr_data {
	u32 min_tile_width;
	u16 cpr[MML_PIPE_CNT];
	u16 gpr[MML_PIPE_CNT];
	bool vcp_readback;
};

static const struct hdr_data mt6893_hdr_data = {
	.min_tile_width = 16,
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.vcp_readback = false,
};

static const struct hdr_data mt6983_hdr_data = {
	.min_tile_width = 16,
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.vcp_readback = false,
};

static const struct hdr_data mt6879_hdr_data = {
	.min_tile_width = 16,
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.vcp_readback = false,
};

static const struct hdr_data mt6895_hdr_data = {
	.min_tile_width = 16,
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.vcp_readback = true,
};




struct mml_comp_hdr {
	struct mtk_ddp_comp ddp_comp;
	struct mml_comp comp;
	const struct hdr_data *data;
	bool ddp_bound;
};

/* meta data for each different frame config */
struct hdr_frame_data {
	u32 out_hist_xs;
	u32 cut_pos_x;
	u32 begin_offset;
	u32 condi_offset;
	struct cmdq_poll_reuse polling_reuse;
	u16 labels[HDR_CURVE_NUM + HDR_REG_NUM + CMDQ_GPR_UPDATE];
	bool is_hdr_need_readback;
};

static inline struct hdr_frame_data *hdr_frm_data(struct mml_comp_config *ccfg)
{
	return ccfg->data;
}

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
	return 0;
}

static s32 hdr_buf_prepare(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	s32 ret = 0;

	mml_pq_msg("%s pipe_id[%d] engine_id[%d]", __func__,
		   ccfg->pipe, comp->id);
	mml_pq_trace_ex_begin("%s", __func__);
	if (dest->pq_config.en_hdr)
		ret = mml_pq_set_comp_config(task);
	mml_pq_trace_ex_end();

	return ret;
}

static s32 hdr_tile_prepare(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg,
			    struct tile_func_block *func,
			    union mml_tile_data *data)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_data *src = &cfg->info.src;
	struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct mml_comp_hdr *hdr = comp_to_hdr(comp);

	data->hdr.relay_mode = dest->pq_config.en_hdr ? false : true;
	data->hdr.min_width = hdr->data->min_tile_width;
	func->init_func = tile_hdr_init;
	func->for_func = tile_hdr_for;
	func->data = data;

	func->enable_flag = dest->pq_config.en_hdr;

	if ((cfg->info.dest_cnt == 1 ||
	     !memcmp(&cfg->info.dest[0].crop,
		     &cfg->info.dest[1].crop,
		     sizeof(struct mml_crop))) &&
	    (dest->crop.r.width != src->width ||
	    dest->crop.r.height != src->height)) {
		u32 in_crop_w, in_crop_h;

		in_crop_w = dest->crop.r.width;
		in_crop_h = dest->crop.r.height;
		if (in_crop_w + dest->crop.r.left > src->width)
			in_crop_w = src->width - dest->crop.r.left;
		if (in_crop_h + dest->crop.r.top > src->height)
			in_crop_h = src->height - dest->crop.r.top;
		func->full_size_x_in = in_crop_w;
		func->full_size_y_in = in_crop_h;
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
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];

	mml_pq_msg("%s pipe_id[%d] engine_id[%d] en_hdr[%d]", __func__,
		ccfg->pipe, comp->id, dest->pq_config.en_hdr);

	if (!dest->pq_config.en_hdr)
		return 0;

	return HDR_CURVE_NUM + HDR_REG_NUM + CMDQ_GPR_UPDATE;
}

static void hdr_init(struct cmdq_pkt *pkt, const phys_addr_t base_pa)
{
	/* Enable engine and shadow */
	cmdq_pkt_write(pkt, NULL, base_pa + HDR_TOP, 0x100001, 0x00308001);
}

static void hdr_relay(struct cmdq_pkt *pkt, const phys_addr_t base_pa,
		      u32 relay)
{
	cmdq_pkt_write(pkt, NULL, base_pa + HDR_RELAY, relay, U32_MAX);
}

static s32 hdr_config_init(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	mml_pq_msg("%s pipe_id[%d] engine_id[%d]", __func__, ccfg->pipe, comp->id);

	hdr_init(task->pkts[ccfg->pipe], comp->base_pa);

	return 0;
}

static struct mml_pq_comp_config_result *get_hdr_comp_config_result(
	struct mml_task *task)
{
	return task->pq_task->comp_config.result;
}

static s32 hdr_config_frame(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	struct hdr_frame_data *hdr_frm = hdr_frm_data(ccfg);
	struct mml_frame_data *src = &cfg->info.src;
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	const phys_addr_t base_pa = comp->base_pa;
	struct mml_pq_comp_config_result *result;
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];
	s32 ret;

	mml_pq_msg("%s pipe_id[%d] engine_id[%d] en_hdr[%d]", __func__,
		ccfg->pipe, comp->id, dest->pq_config.en_hdr);

	if (MML_FMT_10BIT(src->format) || MML_FMT_10BIT(dest->data.format))
		cmdq_pkt_write(pkt, NULL, base_pa + HDR_TOP,
			3 << 28, 0x30000000);
	else
		cmdq_pkt_write(pkt, NULL, base_pa + HDR_TOP,
			1 << 28, 0x30000000);

	if (!dest->pq_config.en_hdr) {
		/* relay mode */
		hdr_relay(pkt, base_pa, 0x1);
		return 0;
	}
	hdr_relay(pkt, base_pa, 0x0);

	ret = mml_pq_get_comp_config_result(task, HDR_WAIT_TIMEOUT_MS);
	if (!ret) {
		result = get_hdr_comp_config_result(task);
		if (result) {
			s32 i;
			s32 curve_idx = 0;
			struct mml_pq_reg *regs = result->hdr_regs;
			u32 *curve = result->hdr_curve;

			/* TODO: use different regs */
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
				mml_write(pkt, base_pa + HDR_GAIN_TABLE_2, curve[curve_idx + 1],
					U32_MAX, reuse, cache, &hdr_frm->labels[i + 1]);
				i += 2;
				curve_idx += 2;
			}
			cmdq_pkt_write(pkt, NULL, base_pa + HDR_HIST_ADDR,
					1 << 8, 1 << 8);
			cmdq_pkt_write(pkt, NULL, base_pa + HDR_HIST_CTRL_2,
					1 << 31, 1 << 31);
			cmdq_pkt_write(pkt, NULL, base_pa + HDR_GAIN_TABLE_0,
					1 << 11, 1 << 11);
			mml_pq_msg("%s is_hdr_need_readback[%d]", __func__,
				   result->is_hdr_need_readback);
			hdr_frm->is_hdr_need_readback = result->is_hdr_need_readback;
		} else {
			mml_pq_err("%s: not get result from user lib", __func__);
		}
	} else {
		mml_pq_comp_config_clear(task);
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

	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);
	u16 tile_cnt = cfg->tile_output[ccfg->pipe]->tile_cnt;
	u32 hdr_input_w;
	u32 hdr_input_h;
	u32 hdr_crop_xs;
	u32 hdr_crop_xe;
	u32 hdr_crop_ys;
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
	hdr_crop_ys = tile->out.ys - tile->in.ys;
	hdr_crop_ye = tile->out.ye - tile->in.ys;

	cmdq_pkt_write(pkt, NULL, base_pa + HDR_TILE_POS,
		(tile->out.ys << 16) + tile->out.xs, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + HDR_SIZE_0,
		(hdr_input_h << 16) + hdr_input_w, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + HDR_SIZE_1,
		(hdr_crop_xe << 16) + hdr_crop_xs, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + HDR_SIZE_2,
		(hdr_crop_ye << 16) + hdr_crop_ys, U32_MAX);

	mml_pq_msg("%s %d: [input] [xs, xe] = [%d, %d], [ys, ye] = [%d, %d]",
			__func__, idx, tile->in.xs, tile->in.xe, tile->in.ys, tile->in.ye);
	mml_pq_msg("%s %d: [output] [xs, xe] = [%d, %d], [ys, ye] = [%d, %d]",
			__func__, idx, tile->out.xs, tile->out.xe, tile->out.ys,
			tile->out.ye);

	if (!dest->pq_config.en_hdr)
		return 0;

	if (!idx) {
		if (task->config->dual)
			hdr_frm->cut_pos_x = (dest->crop.r.width/2) + dest->crop.r.left;
		else
			hdr_frm->cut_pos_x = dest->crop.r.left+dest->crop.r.width;
		if (ccfg->pipe)
			hdr_frm->out_hist_xs = hdr_frm->cut_pos_x;
	}

	hdr_hist_left_start =
		(tile->out.xs > hdr_frm->out_hist_xs) ? tile->out.xs : hdr_frm->out_hist_xs;
	hdr_hist_begin_x = hdr_hist_left_start - tile->in.xs;

	if (task->config->dual && !ccfg->pipe && (idx + 1 >= tile_cnt))
		hdr_hist_end_x = hdr_frm->cut_pos_x - tile->in.xs - 1;
	else
		hdr_hist_end_x = tile->out.xe - tile->in.xs;

	cmdq_pkt_write(pkt, NULL, base_pa + HDR_HIST_CTRL_0, hdr_hist_begin_x, 0x0000ffff);
	cmdq_pkt_write(pkt, NULL, base_pa + HDR_HIST_CTRL_1, hdr_hist_end_x, 0x0000ffff);

	if (tile_cnt == 1) {
		if (task->config->dual && ccfg->pipe)
			hdr_frm->out_hist_xs = tile->in.xs + hdr_hist_end_x + 1;
		else
			hdr_frm->out_hist_xs = tile->out.xe + 1;
		hdr_first_tile = 1;
		hdr_last_tile = 1;
	} else if (!idx) {
		if (task->config->dual && ccfg->pipe)
			hdr_frm->out_hist_xs = tile->in.xs + hdr_hist_end_x + 1;
		else
			hdr_frm->out_hist_xs = tile->out.xe + 1;
		hdr_first_tile = 1;
		hdr_last_tile = 0;
	} else if (idx + 1 >= tile_cnt) {
		hdr_frm->out_hist_xs = 0;
		hdr_first_tile = 0;
		hdr_last_tile = 1;
	} else {
		if (task->config->dual && ccfg->pipe)
			hdr_frm->out_hist_xs = tile->in.xs + hdr_hist_end_x + 1;
		else
			hdr_frm->out_hist_xs = tile->out.xe + 1;
		hdr_first_tile = 0;
		hdr_last_tile = 0;
	}

	cmdq_pkt_write(pkt, NULL, base_pa + HDR_TOP,
		      (hdr_first_tile << 5) | (hdr_last_tile << 6), 0x00000060);
	cmdq_pkt_write(pkt, NULL, base_pa + HDR_HIST_ADDR, (hdr_first_tile << 9), 0x00000200);

	mml_pq_msg("%s %d: hdr_hist_begin_x[%d] hdr_hist_end_x[%d] out_hist_xs[%d]",
		__func__, idx, hdr_hist_begin_x, hdr_hist_end_x,
		hdr_frm->out_hist_xs);
	mml_pq_msg("%s %d: hdr_first_tile[%d] hdr_last_tile[%d] out_hist_xs[%d] cut_pos_x[%d]",
		__func__, idx, hdr_first_tile, hdr_last_tile,
		hdr_frm->out_hist_xs, hdr_frm->cut_pos_x);

	return 0;
}

static void hdr_readback_vcp(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg)
{
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct mml_comp_hdr *hdr = comp_to_hdr(comp);
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	u8 pipe = ccfg->pipe;
	struct hdr_frame_data *hdr_frm = hdr_frm_data(ccfg);


	u32 gpr = hdr->data->gpr[ccfg->pipe];
	u32 engine = CMDQ_VCP_ENG_MML_HDR0 + pipe;

	mml_pq_rb_msg("%s hdr_hist[%p] pipe[%d]", __func__,
		task->pq_task->hdr_hist[pipe], pipe);

	if (!(task->pq_task->hdr_hist[pipe])) {
		task->pq_task->hdr_hist[pipe] =
			kzalloc(sizeof(struct mml_pq_readback_buffer), GFP_KERNEL);

		if (unlikely(!task->pq_task->hdr_hist[pipe])) {
			mml_pq_err("%s not enough mem for hdr_hist", __func__);
			return;
		}

		task->pq_task->hdr_hist[pipe]->va =
			cmdq_get_vcp_buf(engine, &(task->pq_task->hdr_hist[pipe]->pa));

		mml_pq_rb_msg("%s hdr_hist[%p] pipe[%d] hdr_hist_va[%p] hdr_hist_pa[%llx]",
			__func__, task->pq_task->hdr_hist[pipe], pipe,
			task->pq_task->hdr_hist[pipe]->va,
			task->pq_task->hdr_hist[pipe]->pa);
	}

	mml_pq_get_vcp_buf_offset(task, MML_PQ_HDR0+pipe, task->pq_task->hdr_hist[pipe]);

	cmdq_vcp_enable(true);

	cmdq_pkt_readback(pkt, engine, task->pq_task->hdr_hist[pipe]->va_offset,
		HDR_HIST_NUM, gpr,
		&(reuse->labels[reuse->label_idx]),
		&(hdr_frm->polling_reuse));

	hdr_frm->labels[HDR_CURVE_NUM + HDR_REG_NUM] = reuse->label_idx;
	reuse->labels[reuse->label_idx].val = task->pq_task->hdr_hist[pipe]->va_offset;
	reuse->label_idx++;

}

static void hdr_readback_cmdq(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg)
{
	struct mml_comp_hdr *hdr = comp_to_hdr(comp);
	struct hdr_frame_data *hdr_frm = hdr_frm_data(ccfg);
	const phys_addr_t base_pa = comp->base_pa;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct mml_frame_config *cfg = task->config;
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];

	u8 pipe = ccfg->pipe;
	dma_addr_t begin_pa = 0;
	dma_addr_t pa = 0;
	u32 *condi_inst = NULL;
	struct cmdq_operand lop, rop;

	const u16 idx_counter = CMDQ_THR_SPR_IDX1;
	const u16 idx_val = CMDQ_THR_SPR_IDX2;
	const u16 idx_out = hdr->data->cpr[ccfg->pipe];
	const u16 idx_out64 = CMDQ_CPR_TO_CPR64(idx_out);

	mml_pq_get_readback_buffer(task, pipe, &(task->pq_task->hdr_hist[pipe]));

	if (unlikely(!task->pq_task->hdr_hist[pipe])) {
		mml_pq_err("%s job_id[%d] hdr_hist is null", __func__,
			task->job.jobid);
		return;
	}

	pa = task->pq_task->hdr_hist[pipe]->pa;

	/* readback to this pa */
	mml_assign(pkt, idx_out, (u32)pa,
		reuse, cache, &hdr_frm->labels[HDR_CURVE_NUM + HDR_REG_NUM]);
	mml_assign(pkt, idx_out + 1, (u32)(pa >> 32),
		reuse, cache, &hdr_frm->labels[HDR_CURVE_NUM + HDR_REG_NUM + 1]);

	/* counter init to 0 */
	cmdq_pkt_assign_command(pkt, idx_counter, 0);

	/* loop again here */
	hdr_frm->begin_offset = pkt->cmd_buf_size;
	begin_pa = cmdq_pkt_get_pa_by_offset(pkt, hdr_frm->begin_offset);

	/* read to value cpr */
	cmdq_pkt_read_addr(pkt, base_pa + HDR_HIST_DATA, idx_val);
	/* write value spr to dst cpr */
	cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);

	/* jump forward end if match, if spr1 >= 57 - 1	*/
	cmdq_pkt_assign_command(pkt, CMDQ_THR_SPR_IDX0, 0);
	hdr_frm->condi_offset = pkt->cmd_buf_size - CMDQ_INST_SIZE;

	/* inc counter */
	lop.reg = true;
	lop.idx = idx_counter;
	rop.reg = false;
	rop.value = 1;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_counter, &lop, &rop);
	/* inc outut pa */
	lop.reg = true;
	lop.idx = idx_out;
	rop.reg = false;
	rop.value = 4;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_out, &lop, &rop);


	lop.reg = true;
	lop.idx = idx_counter;
	rop.reg = false;
	rop.value =  HDR_HIST_CNT - 1;
	cmdq_pkt_cond_jump_abs(pkt, CMDQ_THR_SPR_IDX0, &lop, &rop,
		CMDQ_LESS_THAN_AND_EQUAL);
	condi_inst = (u32 *)cmdq_pkt_get_va_by_offset(pkt, hdr_frm->condi_offset);
	if (unlikely(!condi_inst))
		mml_pq_err("%s wrong offset %u\n", __func__, hdr_frm->condi_offset);

	*condi_inst = (u32)CMDQ_REG_SHIFT_ADDR(begin_pa);

	/* read to value cpr */
	cmdq_pkt_read_addr(pkt, base_pa + HDR_LBOX_DET_4, idx_val);
	/* write value spr to dst cpr */
	cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);

	mml_pq_rb_msg("%s end job_id[%d] engine_id[%d] va[%p] pa[%08x] pkt[%p]",
		__func__, task->job.jobid, comp->id, task->pq_task->hdr_hist[pipe]->va,
		task->pq_task->hdr_hist[pipe]->pa, pkt);

	mml_pq_rb_msg("%s end job_id[%d] condi:offset[%u] inst[%p], begin:offset[%u] pa[%08x]",
		__func__, task->job.jobid, hdr_frm->condi_offset, condi_inst,
		hdr_frm->begin_offset, begin_pa);
}


static s32 hdr_config_post(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	struct mml_comp_hdr *hdr = comp_to_hdr(comp);
	struct mml_frame_dest *dest = &task->config->info.dest[ccfg->node->out_idx];
	bool vcp = hdr->data->vcp_readback;

	mml_pq_msg("%s vcp[%d] en_hdr[%d]", __func__,
		vcp, dest->pq_config.en_hdr);

	if (!dest->pq_config.en_hdr)
		goto exit;

	if (vcp)
		hdr_readback_vcp(comp, task, ccfg);
	else
		hdr_readback_cmdq(comp, task, ccfg);

exit:
	return 0;

}

static s32 hdr_reconfig_frame(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct hdr_frame_data *hdr_frm = hdr_frm_data(ccfg);
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct mml_pq_comp_config_result *result;
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	s32 ret;

	mml_pq_msg("%s pipe_id[%d] engine_id[%d] en_hdr[%d]", __func__,
		ccfg->pipe, comp->id, dest->pq_config.en_hdr);
	if (!dest->pq_config.en_hdr)
		return 0;

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
				mml_update(reuse, hdr_frm->labels[i + 1],
					   curve[curve_idx + 1]);
				i += 2;
				curve_idx += 2;
			}
			mml_pq_msg("%s is_hdr_need_readback[%d]", __func__,
				result->is_hdr_need_readback);
			hdr_frm->is_hdr_need_readback = result->is_hdr_need_readback;
		} else {
			mml_pq_err("%s: not get result from user lib", __func__);
		}
	} else {
		mml_pq_comp_config_clear(task);
		mml_pq_err("get hdr param timeout: %d in %dms",
			ret, HDR_WAIT_TIMEOUT_MS);
	}
	return 0;
}

static s32 hdr_config_repost(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	struct mml_comp_hdr *hdr = comp_to_hdr(comp);
	struct hdr_frame_data *hdr_frm = hdr_frm_data(ccfg);
	struct mml_frame_dest *dest = &task->config->info.dest[ccfg->node->out_idx];
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	bool vcp = hdr->data->vcp_readback;
	dma_addr_t begin_pa;
	u32 *condi_inst;
	u8 pipe = ccfg->pipe;
	u32 engine = CMDQ_VCP_ENG_MML_HDR0 + pipe;

	if (!dest->pq_config.en_hdr)
		goto exit;

	if (vcp) {
		if (!(task->pq_task->hdr_hist[pipe])) {
			task->pq_task->hdr_hist[pipe] =
				kzalloc(sizeof(struct mml_pq_readback_buffer), GFP_KERNEL);

			if (unlikely(!task->pq_task->hdr_hist[pipe])) {
				mml_pq_err("%s not enough mem for hdr_hist", __func__);
				goto comp_config_put;
			}

			task->pq_task->hdr_hist[pipe]->va =
				cmdq_get_vcp_buf(engine, &(task->pq_task->hdr_hist[pipe]->pa));
		}
		cmdq_vcp_enable(true);
		mml_pq_get_vcp_buf_offset(task, MML_PQ_HDR0 + pipe,
			task->pq_task->hdr_hist[pipe]);

		reuse->labels[hdr_frm->labels[HDR_CURVE_NUM + HDR_REG_NUM]].val =
			cmdq_pkt_vcp_reuse_val(engine,
			task->pq_task->hdr_hist[pipe]->va_offset,
			HDR_HIST_NUM);
		cmdq_pkt_reuse_poll(pkt, &(hdr_frm->polling_reuse));

	} else {
		mml_pq_get_readback_buffer(task, pipe, &(task->pq_task->hdr_hist[pipe]));

		if (unlikely(!task->pq_task->hdr_hist[pipe])) {
			mml_pq_err("%s job_id[%d] hdr_hist is null", __func__,
				task->job.jobid);
			goto comp_config_put;
		}

		mml_update(reuse, hdr_frm->labels[HDR_CURVE_NUM+HDR_REG_NUM],
			(u32)task->pq_task->hdr_hist[pipe]->pa);
		mml_update(reuse, hdr_frm->labels[HDR_CURVE_NUM+HDR_REG_NUM+1],
			(u32)(task->pq_task->hdr_hist[pipe]->pa >> 32));

		begin_pa = cmdq_pkt_get_pa_by_offset(pkt, hdr_frm->begin_offset);
		condi_inst = (u32 *)cmdq_pkt_get_va_by_offset(pkt, hdr_frm->condi_offset);
		if (unlikely(!condi_inst))
			mml_pq_err("%s wrong offset %u\n", __func__, hdr_frm->condi_offset);

		*condi_inst = (u32)CMDQ_REG_SHIFT_ADDR(begin_pa);

		mml_pq_rb_msg("%s end job_id[%d] engine_id[%d] va[%p] pa[%08x] pkt[%p] ",
			__func__, task->job.jobid, comp->id, task->pq_task->hdr_hist[pipe]->va,
			task->pq_task->hdr_hist[pipe]->pa, pkt);

		mml_pq_rb_msg("%s end job_id[%d]condi:offset[%u]inst[%p],begin:offset[%u]pa[%08x]",
			__func__, task->job.jobid, hdr_frm->condi_offset, condi_inst,
			hdr_frm->begin_offset, begin_pa);
	}

comp_config_put:
	mml_pq_put_comp_config_result(task);
exit:
	return 0;
}

static const struct mml_comp_config_ops hdr_cfg_ops = {
	.prepare = hdr_prepare,
	.buf_prepare = hdr_buf_prepare,
	.get_label_count = hdr_get_label_count,
	.init = hdr_config_init,
	.frame = hdr_config_frame,
	.tile = hdr_config_tile,
	.post = hdr_config_post,
	.reframe = hdr_reconfig_frame,
	.repost = hdr_config_repost,
};

static void hdr_task_done_readback(struct mml_comp *comp, struct mml_task *task,
					 struct mml_comp_config *ccfg)
{
	struct mml_comp_hdr *hdr = comp_to_hdr(comp);
	struct mml_frame_config *cfg = task->config;
	struct hdr_frame_data *hdr_frm = hdr_frm_data(ccfg);
	struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	u8 pipe = ccfg->pipe;
	u32 engine = MML_PQ_HDR0 + pipe;
	bool vcp = hdr->data->vcp_readback;
	u32 offset = 0;


	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s is_hdr_need_readback[%d] id[%d] en_hdr[%d]", __func__,
			hdr_frm->is_hdr_need_readback, comp->id, dest->pq_config.en_hdr);

	if (!dest->pq_config.en_hdr || !task->pq_task->hdr_hist[pipe])
		goto exit;

	if (!dest->pq_config.en_hdr) {
		s32 i;
		u32 *phist = kmalloc(HDR_HIST_NUM*sizeof(u32), GFP_KERNEL);
		void __iomem *base = comp->base;

		for (i = 0; i < HDR_HIST_NUM; i++) {
			if (i == 57) {
				phist[i] = readl(base + HDR_LBOX_DET_4);
				continue;
			}
			phist[i] = readl(base + HDR_HIST_DATA);
		}
		mml_pq_hdr_readback(task, ccfg->pipe, phist);
	}

	offset = vcp ? task->pq_task->hdr_hist[pipe]->va_offset : 0;

	mml_pq_msg("%s job_id[%d] id[%d] pipe[%d] en_hdr[%d] va[%p] pa[%08x]",
		__func__, task->job.jobid, comp->id, ccfg->pipe,
		dest->pq_config.en_hdr, task->pq_task->hdr_hist[pipe]->va,
		task->pq_task->hdr_hist[pipe]->pa);

	mml_pq_rb_msg("%s job_id[%d] hist[0~4]={%08x, %08x, %08x, %08x, %08x} hist[57]=[%08x]",
		__func__, task->job.jobid,
		task->pq_task->hdr_hist[pipe]->va[offset/4+0],
		task->pq_task->hdr_hist[pipe]->va[offset/4+1],
		task->pq_task->hdr_hist[pipe]->va[offset/4+2],
		task->pq_task->hdr_hist[pipe]->va[offset/4+3],
		task->pq_task->hdr_hist[pipe]->va[offset/4+4],
		task->pq_task->hdr_hist[pipe]->va[offset/4+57]);

	if (hdr_frm->is_hdr_need_readback) {
		mml_pq_hdr_readback(task, ccfg->pipe,
			&(task->pq_task->hdr_hist[pipe]->va[offset/4]));
		if (mml_pq_rb_msg) {
			u32 i = 0, sum = 0;
			u32 expect_value = 0;
			u32 letter_up = 0, letter_down = 0, letter_height = 0;

			for (i = 0; i < HDR_HIST_CNT; i++)
				sum = sum + task->pq_task->hdr_hist[pipe]->va[offset/4+i];

			letter_up = ((task->pq_task->hdr_hist[pipe]->va[57] &
				0x0000FFFF) >> 0);
			letter_down = ((task->pq_task->hdr_hist[pipe]->va[57] &
				0xFFFF0000) >> 16);
			letter_height = letter_down - letter_up;
			if (task->config->dual) {
				if (pipe)
					expect_value = (letter_height+1) *
						(task->config->info.dest[0].crop.r.width -
						hdr_frm->cut_pos_x) * 8;
				else
					expect_value = (letter_height+1) * hdr_frm->cut_pos_x * 8;
			} else
				expect_value =
					(letter_height+1) *
					task->config->info.dest[0].crop.r.width * 8;
			mml_pq_rb_msg("%s sum[%u] expect_value[%u] job_id[%d] pipe[%d]", __func__,
				sum, expect_value, task->job.jobid, pipe);
			mml_pq_rb_msg("%s job_id[%d] pipe[%d] letter_height[%d] down[%d] up[%d]",
				__func__, task->job.jobid, pipe, letter_height, letter_down,
				letter_up);
		}
	}
	if (vcp) {
		mml_pq_put_vcp_buf_offset(task, engine, task->pq_task->hdr_hist[pipe]);
		cmdq_vcp_enable(false);
	} else
		mml_pq_put_readback_buffer(task, pipe, task->pq_task->hdr_hist[pipe]);
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

static const struct mtk_ddp_comp_funcs ddp_comp_funcs = {
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
		.data = &mt6983_hdr_data,
	},
	{
		.compatible = "mediatek,mt6893-mml_hdr",
		.data = &mt6893_hdr_data,
	},
	{
		.compatible = "mediatek,mt6879-mml_hdr",
		.data = &mt6879_hdr_data,
	},
	{
		.compatible = "mediatek,mt6895-mml_hdr",
		.data = &mt6895_hdr_data,
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
