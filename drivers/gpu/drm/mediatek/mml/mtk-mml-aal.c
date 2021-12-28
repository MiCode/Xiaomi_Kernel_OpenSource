/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/component.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <mtk_drm_ddp_comp.h>

#include "mtk-mml-color.h"
#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"
#include "mtk-mml-dle-adaptor.h"
#include "mtk-mml-pq-core.h"

#include "tile_driver.h"
#include "mtk-mml-tile.h"
#include "tile_mdp_func.h"

#undef pr_fmt
#define pr_fmt(fmt) "[mml_pq_aal]" fmt

#define AAL_EN				0x000
#define AAL_RESET			0x004
#define AAL_INTEN			0x008
#define AAL_INTSTA			0x00c
#define AAL_STATUS			0x010
#define AAL_CFG				0x020
#define AAL_INPUT_COUNT			0x024
#define AAL_OUTPUT_COUNT		0x028
#define AAL_CHKSUM			0x02c
#define AAL_SIZE			0x030
#define AAL_OUTPUT_SIZE			0x034
#define AAL_OUTPUT_OFFSET		0x038
#define AAL_DUMMY_REG			0x0c0
#define AAL_SRAM_CFG			0x0c4
#define AAL_SRAM_STATUS			0x0c8
#define AAL_SRAM_RW_IF_0		0x0cc
#define AAL_SRAM_RW_IF_1		0x0d0
#define AAL_SRAM_RW_IF_2		0x0d4
#define AAL_SRAM_RW_IF_3		0x0d8
#define AAL_SHADOW_CTRL			0x0f0
#define AAL_TILE_02			0x0f4
#define AAL_DRE_BLOCK_INFO_07		0x0f8
#define AAL_ATPG			0x0fc
#define AAL_DREI_PAT_GEN_SET		0x100
#define AAL_DREI_PAT_GEN_COLOR0		0x108
#define AAL_DREI_PAT_GEN_COLOR1		0x10c
#define AAL_DREO_PAT_GEN_SET		0x130
#define AAL_DREO_PAT_GEN_COLOR0		0x138
#define AAL_DREO_PAT_GEN_COLOR1		0x13c
#define AAL_DREO_PAT_GEN_POS		0x144
#define AAL_DREO_PAT_GEN_CURSOR_RB0	0x148
#define AAL_DREO_PAT_GEN_CURSOR_RB1	0x14c
#define AAL_CABCO_PAT_GEN_SET		0x160
#define AAL_CABCO_PAT_GEN_FRM_SIZE	0x164
#define AAL_CABCO_PAT_GEN_COLOR0	0x168
#define AAL_CABCO_PAT_GEN_COLOR1	0x16c
#define AAL_CABCO_PAT_GEN_COLOR2	0x170
#define AAL_CABCO_PAT_GEN_POS		0x174
#define AAL_CABCO_PAT_GEN_CURSOR_RB0	0x178
#define AAL_CABCO_PAT_GEN_CURSOR_RB1	0x17c
#define AAL_CABCO_PAT_GEN_RAMP		0x180
#define AAL_CABCO_PAT_GEN_TILE_POS	0x184
#define AAL_CABCO_PAT_GEN_TILE_OV	0x188
#define AAL_CFG_MAIN			0x200
#define AAL_MAX_HIST_CONFIG_00		0x204
#define AAL_DRE_FLT_FORCE_00		0x358
#define AAL_DRE_FLT_FORCE_01		0x35c
#define AAL_DRE_FLT_FORCE_02		0x360
#define AAL_DRE_FLT_FORCE_03		0x364
#define AAL_DRE_FLT_FORCE_04		0x368
#define AAL_DRE_FLT_FORCE_05		0x36c
#define AAL_DRE_FLT_FORCE_06		0x370
#define AAL_DRE_FLT_FORCE_07		0x374
#define AAL_DRE_FLT_FORCE_08		0x378
#define AAL_DRE_FLT_FORCE_09		0x37c
#define AAL_DRE_FLT_FORCE_10		0x380
#define AAL_DRE_FLT_FORCE_11		0x384
#define AAL_DRE_MAPPING_00		0x3b4
#define AAL_DBG_CFG_MAIN		0x45c
#define AAL_WIN_X_MAIN			0x460
#define AAL_WIN_Y_MAIN			0x464
#define AAL_DRE_BLOCK_INFO_00		0x468
#define AAL_DRE_BLOCK_INFO_01		0x46c
#define AAL_DRE_BLOCK_INFO_02		0x470
#define AAL_DRE_BLOCK_INFO_03		0x474
#define AAL_DRE_BLOCK_INFO_04		0x478
#define AAL_DRE_CHROMA_HIST_00		0x480
#define AAL_DRE_CHROMA_HIST_01		0x484
#define AAL_DRE_ALPHA_BLEND_00		0x488
#define AAL_DRE_BITPLUS_00		0x48c
#define AAL_DRE_BITPLUS_01		0x490
#define AAL_DRE_BITPLUS_02		0x494
#define AAL_DRE_BITPLUS_03		0x498
#define AAL_DRE_BITPLUS_04		0x49c
#define AAL_DRE_BLOCK_INFO_05		0x4b4
#define AAL_DRE_BLOCK_INFO_06		0x4b8
#define AAL_Y2R_00			0x4bc
#define AAL_Y2R_01			0x4c0
#define AAL_Y2R_02			0x4c4
#define AAL_Y2R_03			0x4c8
#define AAL_Y2R_04			0x4cc
#define AAL_Y2R_05			0x4d0
#define AAL_R2Y_00			0x4d4
#define AAL_R2Y_01			0x4d8
#define AAL_R2Y_02			0x4dc
#define AAL_R2Y_03			0x4e0
#define AAL_R2Y_04			0x4e4
#define AAL_R2Y_05			0x4e8
#define AAL_TILE_00			0x4ec
#define AAL_TILE_01			0x4f0
#define AAL_DUAL_PIPE_00		0x500
#define AAL_DUAL_PIPE_01		0x504
#define AAL_DUAL_PIPE_02		0x508
#define AAL_DUAL_PIPE_03		0x50c
#define AAL_DUAL_PIPE_04		0x510
#define AAL_DUAL_PIPE_05		0x514
#define AAL_DUAL_PIPE_06		0x518
#define AAL_DUAL_PIPE_07		0x51c
#define AAL_DRE_ROI_00			0x520
#define AAL_DRE_ROI_01			0x524
#define AAL_DRE_CHROMA_HIST2_00		0x528
#define AAL_DRE_CHROMA_HIST2_01		0x52c
#define AAL_DRE_CHROMA_HIST3_00		0x530
#define AAL_DRE_CHROMA_HIST3_01		0x534
#define AAL_DRE_FLATLINE_DIR		0x538
#define AAL_DRE_BILATERAL		0x53c
#define AAL_DRE_DISP_OUT		0x540
#define AAL_DUAL_PIPE_08		0x544
#define AAL_DUAL_PIPE_09		0x548
#define AAL_DUAL_PIPE_10		0x54c
#define AAL_DUAL_PIPE_11		0x550
#define AAL_DUAL_PIPE_12		0x554
#define AAL_DUAL_PIPE_13		0x558
#define AAL_DUAL_PIPE_14		0x55c
#define AAL_DUAL_PIPE_15		0x560
#define AAL_DRE_BILATERAL_BLENDING	0x564

#define AAL_WAIT_TIMEOUT_MS	(50)

#define AAL_POLL_SLEEP_TIME_US	(10)
#define AAL_MAX_POLL_TIME_US	(1000)
#define MAX_SRAM_BUF_NUM (2)

#define AAL_SRAM_STATUS_BIT	BIT(17)

struct aal_data {
	u32 min_tile_width;
	u32 tile_width;
	u32 min_hist_width;
	bool vcp_readback;
	u16 gpr[MML_PIPE_CNT];
	u16 cpr[MML_PIPE_CNT];
};

static const struct aal_data mt6893_aal_data = {
	.min_tile_width = 50,
	.tile_width = 560,
	.min_hist_width = 128,
	.vcp_readback = false,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
};

static const struct aal_data mt6983_aal_data = {
	.min_tile_width = 50,
	.tile_width = 1652,
	.min_hist_width = 128,
	.vcp_readback = false,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
};

static const struct aal_data mt6879_aal_data = {
	.min_tile_width = 50,
	.tile_width = 1376,
	.min_hist_width = 128,
	.vcp_readback = false,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
};

static const struct aal_data mt6895_aal0_data = {
	.min_tile_width = 50,
	.tile_width = 1300,
	.min_hist_width = 128,
	.vcp_readback = true,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
};

static const struct aal_data mt6895_aal1_data = {
	.min_tile_width = 50,
	.tile_width = 852,
	.min_hist_width = 128,
	.vcp_readback = true,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
};

struct mml_comp_aal {
	struct mtk_ddp_comp ddp_comp;
	struct mml_comp comp;
	const struct aal_data *data;
	bool ddp_bound;

	u32 sram_curve_start;
	u32 sram_hist_start;
};

/* meta data for each different frame config */
struct aal_frame_data {
	u32 out_hist_xs;
	u32 dre_blk_width;
	u32 dre_blk_height;
	u32 begin_offset;
	u32 condi_offset;
	u32 cut_pos_x;
	struct cmdq_poll_reuse polling_reuse;
	u16 labels[AAL_CURVE_NUM + CMDQ_GPR_UPDATE];
	bool is_aal_need_readback;
};

static inline struct aal_frame_data *aal_frm_data(struct mml_comp_config *ccfg)
{
	return ccfg->data;
}

static inline struct mml_comp_aal *comp_to_aal(struct mml_comp *comp)
{
	return container_of(comp, struct mml_comp_aal, comp);
}

static s32 aal_prepare(struct mml_comp *comp, struct mml_task *task,
		       struct mml_comp_config *ccfg)
{
	struct aal_frame_data *aal_frm;

	aal_frm = kzalloc(sizeof(*aal_frm), GFP_KERNEL);
	ccfg->data = aal_frm;
	return 0;
}

static s32 aal_buf_prepare(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	s32 ret = 0;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s engine_id[%d] en_dre[%d]", __func__, comp->id,
		   dest->pq_config.en_dre);

	if (dest->pq_config.en_dre)
		ret = mml_pq_set_comp_config(task);

	mml_pq_trace_ex_end();
	return ret;
}

static s32 aal_tile_prepare(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg,
			    struct tile_func_block *func,
			    union mml_tile_data *data)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_data *src = &cfg->info.src;
	struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct mml_comp_aal *aal = comp_to_aal(comp);

	mml_pq_trace_ex_begin("%s", __func__);
	data->aal.max_width = aal->data->tile_width;
	data->aal.min_hist_width = aal->data->min_hist_width;
	data->aal.min_width = aal->data->min_tile_width;
	func->init_func = tile_aal_init;
	func->for_func = tile_aal_for;
	func->data = data;

	func->enable_flag = dest->pq_config.en_dre;

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

	mml_pq_msg("%s engine_id[%d]", __func__, comp->id);
	mml_pq_trace_ex_end();
	return 0;
}

static const struct mml_comp_tile_ops aal_tile_ops = {
	.prepare = aal_tile_prepare,
};

static u32 aal_get_label_count(struct mml_comp *comp, struct mml_task *task,
			       struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];

	mml_pq_msg("%s engine_id[%d] en_dre[%d]", __func__, comp->id,
			dest->pq_config.en_dre);

	if (!dest->pq_config.en_dre)
		return 0;

	return AAL_CURVE_NUM + CMDQ_GPR_UPDATE;
}

static void aal_init(struct cmdq_pkt *pkt, const phys_addr_t base_pa)
{
	cmdq_pkt_write(pkt, NULL, base_pa + AAL_EN, 0x1, U32_MAX);
	/* Enable shadow */
	cmdq_pkt_write(pkt, NULL, base_pa + AAL_SHADOW_CTRL, 0x2, U32_MAX);
}

static void aal_relay(struct cmdq_pkt *pkt, const phys_addr_t base_pa,
		      u32 relay)
{
	cmdq_pkt_write(pkt, NULL, base_pa + AAL_CFG, relay, 0x00000001);
}

static s32 aal_config_init(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	aal_init(task->pkts[ccfg->pipe], comp->base_pa);
	return 0;
}

static struct mml_pq_comp_config_result *get_aal_comp_config_result(
	struct mml_task *task)
{
	return task->pq_task->comp_config.result;
}

static s32 aal_config_frame(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg)
{
	struct mml_comp_aal *aal = comp_to_aal(comp);
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	struct aal_frame_data *aal_frm = aal_frm_data(ccfg);
	struct mml_frame_data *src = &cfg->info.src;
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	const phys_addr_t base_pa = comp->base_pa;

	struct mml_pq_comp_config_result *result;
	struct mml_pq_aal_config_param *tile_config_param;
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];
	u32 addr = aal->sram_curve_start;
	u32 gpr = aal->data->gpr[ccfg->pipe];
	u32 cpr = aal->data->cpr[ccfg->pipe];
	s32 ret = 0;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s engine_id[%d] en_dre[%d]", __func__, comp->id, dest->pq_config.en_dre);
	if (!dest->pq_config.en_dre || dest->crop.r.width < aal->data->min_tile_width) {
		/* relay mode */
		aal_relay(pkt, base_pa, 0x1);
		goto exit;
	}
	aal_relay(pkt, base_pa, 0x0);

	mml_pq_msg("%s sram_start_addr[%d] cmdq_cpr[%d]", __func__, addr, cpr);

	if (MML_FMT_10BIT(src->format) || MML_FMT_10BIT(dest->data.format))
		cmdq_pkt_write(pkt, NULL, base_pa + AAL_CFG_MAIN,
			0, 0x00000080);
	else
		cmdq_pkt_write(pkt, NULL, base_pa + AAL_CFG_MAIN,
			1 << 7, 0x00000080);

	ret = mml_pq_get_comp_config_result(task, AAL_WAIT_TIMEOUT_MS);
	if (!ret) {
		result = get_aal_comp_config_result(task);
		if (result) {
			s32 i;
			struct mml_pq_reg *regs = result->aal_regs;
			u32 *curve = result->aal_curve;

			/* TODO: use different regs */
			mml_pq_msg("%s:config aal regs, count: %d", __func__, result->aal_reg_cnt);
			for (i = 0; i < result->aal_reg_cnt; i++) {
				cmdq_pkt_write(pkt, NULL, base_pa + regs[i].offset,
					regs[i].value, regs[i].mask);
				mml_pq_msg("[aal][config][%x] = %#x mask(%#x)",
					regs[i].offset, regs[i].value, regs[i].mask);
			}
			for (i = 0; i < AAL_CURVE_NUM; i++) {
				cmdq_pkt_write(pkt, NULL, base_pa + AAL_SRAM_RW_IF_0, addr,
					U32_MAX);
				cmdq_pkt_poll(pkt, NULL, (0x1 << 16),
					base_pa + AAL_SRAM_STATUS, (0x1 << 16),
					gpr);
				mml_write(pkt, base_pa + AAL_SRAM_RW_IF_1, curve[i],
					U32_MAX, reuse, cache, &aal_frm->labels[i]);
				addr += 4;
			}

			mml_pq_msg("%s is_aal_need_readback[%d] base_pa[%llx]", __func__,
					result->is_aal_need_readback, base_pa);

			aal_frm->is_aal_need_readback = result->is_aal_need_readback;

			tile_config_param = &(result->aal_param[ccfg->node->out_idx]);
			aal_frm->dre_blk_width = tile_config_param->dre_blk_width;
			aal_frm->dre_blk_height = tile_config_param->dre_blk_height;
			mml_pq_msg("%s: success dre_blk_width[%d], dre_blk_height[%d]",
				__func__, tile_config_param->dre_blk_width,
				tile_config_param->dre_blk_height);
		} else {
			mml_pq_err("%s: not get result from user lib", __func__);
		}
	} else {
		mml_pq_comp_config_clear(task);
		mml_pq_err("get aal param timeout: %d in %dms",
			ret, AAL_WAIT_TIMEOUT_MS);
	}

exit:
	mml_pq_trace_ex_end();
	return ret;
}

static s32 aal_config_tile(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;
	struct aal_frame_data *aal_frm = aal_frm_data(ccfg);
	struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];

	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);
	u32 src_frame_width = cfg->info.src.width;
	u32 src_frame_height = cfg->info.src.height;
	u16 tile_cnt = cfg->tile_output[ccfg->pipe]->tile_cnt;

	u32 aal_input_w;
	u32 aal_input_h;
	u32 aal_output_w;
	u32 aal_output_h;
	u32 aal_crop_x_offset;
	u32 aal_crop_y_offset;
	u32 dre_blk_width = 0;
	u32 dre_blk_height = 0;

	u32 act_win_x_start = 0, act_win_x_end = 0;
	u32 act_win_y_start = 0, act_win_y_end = 0;
	u32 win_x_start = 0, win_x_end = 0;
	u32 blk_num_x_start = 0, blk_num_x_end = 0;
	u32 blk_num_y_start = 0, blk_num_y_end = 0;
	u32 blk_cnt_x_start = 0, blk_cnt_x_end = 0;
	u32 blk_cnt_y_start = 0, blk_cnt_y_end = 0;
	u32 roi_x_start = 0, roi_x_end = 0;
	u32 roi_y_start = 0, roi_y_end = 0;
	u32 win_y_start = 0, win_y_end = 0;

	u32 aal_hist_left_start = 0;
	u32 tile_pxl_x_start = 0, tile_pxl_x_end = 0;
	u32 tile_pxl_y_start = 0, tile_pxl_y_end = 0;
	u32 last_tile_x_flag = 0, last_tile_y_flag = 0;
	u32 save_first_blk_col_flag = 0;
	u32 save_last_blk_col_flag = 0;
	s32 ret = 0;

	mml_pq_trace_ex_begin("%s", __func__);
	aal_input_w = tile->in.xe - tile->in.xs + 1;
	aal_input_h = tile->in.ye - tile->in.ys + 1;
	aal_output_w = tile->out.xe - tile->out.xs + 1;
	aal_output_h = tile->out.ye - tile->out.ys + 1;
	aal_crop_x_offset = tile->out.xs - tile->in.xs;
	aal_crop_y_offset = tile->out.ys - tile->in.ys;

	cmdq_pkt_write(pkt, NULL, base_pa + AAL_SIZE,
		(aal_input_w << 16) + aal_input_h, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + AAL_OUTPUT_OFFSET,
		(aal_crop_x_offset << 16) + aal_crop_y_offset, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + AAL_OUTPUT_SIZE,
		(aal_output_w << 16) + aal_output_h, U32_MAX);

	if (!dest->pq_config.en_dre)
		goto exit;

	if (!idx) {
		if (task->config->dual)
			aal_frm->cut_pos_x = dest->crop.r.width / 2;
		else
			aal_frm->cut_pos_x = dest->crop.r.width;
		if (ccfg->pipe)
			aal_frm->out_hist_xs = aal_frm->cut_pos_x;
	}

	dre_blk_width = aal_frm->dre_blk_width;
	dre_blk_height = aal_frm->dre_blk_height;

	aal_hist_left_start =
		(tile->out.xs > aal_frm->out_hist_xs) ? tile->out.xs : aal_frm->out_hist_xs;

	mml_pq_msg("%s jobid[%d] engine_id[%d] idx[%d] pipe[%d] pkt[%08x]",
		__func__, task->job.jobid, comp->id, idx, ccfg->pipe, pkt);

	mml_pq_msg("%s %d: %d: %d: [input] [xs, xe] = [%d, %d], [ys, ye] = [%d, %d]",
		__func__, task->job.jobid, comp->id, idx, tile->in.xs,
		tile->in.xe, tile->in.ys,
		tile->in.ye);
	mml_pq_msg("%s %d: %d: %d: [output] [xs, xe] = [%d, %d], [ys, ye] = [%d, %d]",
		__func__, task->job.jobid, comp->id, idx,
		tile->out.xs, tile->out.xe, tile->out.ys,
		tile->out.ye);
	mml_pq_msg("%s %d: %d: %d: [aal_crop_offset] [x, y] = [%d, %d], aal_hist_left_start[%d]",
		__func__, task->job.jobid, comp->id, idx,
		aal_crop_x_offset, aal_crop_y_offset,
		aal_hist_left_start);

	act_win_x_start = aal_hist_left_start - tile->in.xs;
	if (task->config->dual && !ccfg->pipe && (idx + 1 >= tile_cnt))
		act_win_x_end = aal_frm->cut_pos_x - tile->in.xs - 1;
	else
		act_win_x_end = tile->out.xe - tile->in.xs;
	tile_pxl_x_start = tile->in.xs;
	tile_pxl_x_end = tile->in.xe;

	last_tile_x_flag = (tile->in.xe+1 >= src_frame_width) ? 1:0;

	mml_pq_msg("%s %d: %d: %d: [tile_pxl] [xs, xe] = [%d, %d]",
		__func__, task->job.jobid, comp->id, idx, tile_pxl_x_start, tile_pxl_x_end);

	act_win_y_start = 0;
	act_win_y_end = tile->in.ye - tile->in.ys;
	tile_pxl_y_start = 0;
	tile_pxl_y_end = tile->in.ye - tile->in.ys;

	last_tile_y_flag = (tile_pxl_y_end+1 >= src_frame_height) ? 1:0;
	roi_x_start = 0;
	roi_x_end = tile->in.xe - tile->in.xs;
	roi_y_start = 0;
	roi_y_end = tile->in.ye - tile->in.ys;

	blk_num_x_start = (tile_pxl_x_start / dre_blk_width);
	blk_num_x_end = (tile_pxl_x_end / dre_blk_width);
	blk_cnt_x_start = tile_pxl_x_start - (blk_num_x_start * dre_blk_width);
	blk_cnt_x_end = tile_pxl_x_end - (blk_num_x_end * dre_blk_width);
	blk_num_y_start = (tile_pxl_y_start / dre_blk_height);
	blk_num_y_end = (tile_pxl_y_end / dre_blk_height);
	blk_cnt_y_start = tile_pxl_y_start - (blk_num_y_start * dre_blk_height);
	blk_cnt_y_end = tile_pxl_y_end - (blk_num_y_end * dre_blk_height);

	if (!idx) {
		if (task->config->dual && ccfg->pipe)
			aal_frm->out_hist_xs = tile->in.xs + act_win_x_end + 1;
		else
			aal_frm->out_hist_xs = tile->out.xe + 1;
		save_first_blk_col_flag = (ccfg->pipe) ? 1 : 0;
		save_last_blk_col_flag = 0;
	} else if (idx + 1 >= tile_cnt) {
		aal_frm->out_hist_xs = 0;
		save_first_blk_col_flag = 0;
		save_last_blk_col_flag = (ccfg->pipe) ? 0 : 1;
	} else {
		if (task->config->dual && ccfg->pipe)
			aal_frm->out_hist_xs = tile->in.xs + act_win_x_end + 1;
		else
			aal_frm->out_hist_xs = tile->out.xe + 1;
		save_first_blk_col_flag = 0;
		save_last_blk_col_flag = 0;
	}

	win_x_start = 0;
	win_x_end = tile_pxl_x_end - tile_pxl_x_start + 1;
	win_y_start = 0;
	win_y_end = tile_pxl_y_end - tile_pxl_y_start + 1;

	mml_pq_msg("%s %d: %d: %d:[act_win][xs, xe] = [%d, %d], [ys, ye] = [%d, %d]",
			__func__, task->job.jobid, comp->id, idx, act_win_x_start, act_win_x_end,
			act_win_y_start, act_win_y_end);
	mml_pq_msg("%s %d: %d: %d:[blk_num][xs, xe] = [%d, %d], [ys, ye] = [%d, %d]",
			__func__, task->job.jobid, comp->id, idx, blk_num_x_start, blk_num_x_end,
			blk_num_y_start, blk_num_y_end);
	mml_pq_msg("%s %d: %d: %d:[blk_cnt][xs, xe] = [%d, %d], [ys, ye] = [%d, %d]",
			__func__, task->job.jobid, comp->id, idx, blk_cnt_x_start, blk_cnt_x_end,
			blk_cnt_y_start, blk_cnt_y_end);
	mml_pq_msg("%s %d: %d: %d:[roi][xs, xe] = [%d, %d], [ys, ye] = [%d, %d]",
			__func__,  task->job.jobid, comp->id, idx, roi_x_start,
			roi_x_end, roi_y_start, roi_y_end);
	mml_pq_msg("%s %d: %d: %d:[win][xs, xe] = [%d, %d], [ys, ye] = [%d, %d]",
			__func__, task->job.jobid, comp->id, idx, win_x_start,
			win_x_end, win_y_start, win_y_end);
	mml_pq_msg("%s %d: %d: %d:save_first_blk_col_flag[%d], save_last_blk_col_flag[%d]",
			__func__, task->job.jobid, comp->id, idx,
			save_first_blk_col_flag,
			save_last_blk_col_flag);
	mml_pq_msg("%s %d: %d: %d:last_tile_x_flag[%d], last_tile_y_flag[%d]",
			__func__, task->job.jobid, comp->id, idx,
			last_tile_x_flag, last_tile_y_flag);

	cmdq_pkt_write(pkt, NULL, base_pa + AAL_DRE_BLOCK_INFO_00,
		(act_win_x_end << 16) | act_win_x_start, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + AAL_DRE_BLOCK_INFO_07, (act_win_y_end << 16) |
		act_win_y_start, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + AAL_TILE_00, (save_first_blk_col_flag << 23) |
		(save_last_blk_col_flag << 22) | (last_tile_x_flag << 21) |
		(last_tile_y_flag << 20) | (blk_num_x_end << 15) |
		(blk_num_x_start << 10) | (blk_num_y_end << 5) |
		blk_num_y_start, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + AAL_TILE_01, (blk_cnt_x_end << 16) |
		blk_cnt_x_start, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + AAL_TILE_02, (blk_cnt_y_end << 16) |
		blk_cnt_y_start, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + AAL_DRE_ROI_00, (roi_x_end << 16) |
		roi_x_start, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + AAL_DRE_ROI_01, (roi_y_end << 16) |
		roi_y_start, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + AAL_WIN_X_MAIN, (win_x_end << 16) |
		win_x_start, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + AAL_WIN_Y_MAIN, (win_y_end << 16) |
		win_y_start, U32_MAX);

exit:
	mml_pq_trace_ex_end();
	return ret;
}

static void aal_readback_cmdq(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg)
{
	struct mml_comp_aal *aal = comp_to_aal(comp);
	struct aal_frame_data *aal_frm = aal_frm_data(ccfg);

	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct mml_frame_config *cfg = task->config;
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];

	const phys_addr_t base_pa = comp->base_pa;
	u32 addr = aal->sram_hist_start;
	u32 dre30_hist_sram_start = aal->sram_hist_start;
	s32 i = 0;
	u8 pipe = ccfg->pipe;
	dma_addr_t begin_pa = 0;
	u32 *condi_inst = NULL;
	dma_addr_t pa = 0;
	struct cmdq_operand lop, rop;

	const u16 idx_addr = CMDQ_THR_SPR_IDX1;
	const u16 idx_val = CMDQ_THR_SPR_IDX2;
	const u16 poll_gpr = aal->data->gpr[ccfg->pipe];
	const u16 idx_out = aal->data->cpr[ccfg->pipe];
	const u16 idx_out64 = CMDQ_CPR_TO_CPR64(idx_out);

	mml_pq_msg("%s start engine_id[%d] addr[%d]", __func__, comp->id,  addr);

	mml_pq_get_readback_buffer(task, pipe, &(task->pq_task->aal_hist[pipe]));

	if (unlikely(!task->pq_task->aal_hist[pipe])) {
		mml_pq_err("%s job_id[%d] aal_hist is null", __func__,
			task->job.jobid);
		return;
	}

	pa = task->pq_task->aal_hist[pipe]->pa;

	/* init sprs
	 * spr1 = AAL_SRAM_START
	 * cpr64 = out_pa
	 */
	cmdq_pkt_assign_command(pkt, idx_addr, dre30_hist_sram_start);

	mml_assign(pkt, idx_out, (u32)pa,
		reuse, cache, &aal_frm->labels[AAL_CURVE_NUM]);
	mml_assign(pkt, idx_out + 1, (u32)(pa >> 32),
		reuse, cache, &aal_frm->labels[AAL_CURVE_NUM + 1]);

	/* loop again here */
	aal_frm->begin_offset = pkt->cmd_buf_size;
	begin_pa = cmdq_pkt_get_pa_by_offset(pkt, aal_frm->begin_offset);

	/* config aal sram addr and poll */
	cmdq_pkt_write_reg_addr(pkt, base_pa + AAL_SRAM_RW_IF_2,
		idx_addr, U32_MAX);
	/* use gpr low as poll gpr */
	cmdq_pkt_poll_addr(pkt, AAL_SRAM_STATUS_BIT,
		base_pa + AAL_SRAM_STATUS,
		AAL_SRAM_STATUS_BIT, poll_gpr);
	/* read to value spr */
	cmdq_pkt_read_addr(pkt, base_pa + AAL_SRAM_RW_IF_3, idx_val);
	/* write value spr to dst cpr64 */
	cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);

	/* jump forward end if sram is last one, if spr1 >= 4096 + 4 * 767 */

	cmdq_pkt_assign_command(pkt, CMDQ_THR_SPR_IDX0, begin_pa);
	aal_frm->condi_offset = pkt->cmd_buf_size - CMDQ_INST_SIZE;

	/* inc src addr */
	lop.reg = true;
	lop.idx = idx_addr;
	rop.reg = false;
	rop.value = 4;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_addr, &lop, &rop);
	/* inc outut pa */
	lop.reg = true;
	lop.idx = idx_out;
	rop.reg = false;
	rop.value = 4;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_out, &lop, &rop);

	lop.reg = true;
	lop.idx = idx_addr;
	rop.reg = false;
	rop.value = dre30_hist_sram_start + 4 * (AAL_HIST_NUM - 1);
	cmdq_pkt_cond_jump_abs(pkt, CMDQ_THR_SPR_IDX0, &lop, &rop,
		CMDQ_LESS_THAN_AND_EQUAL);

	condi_inst = (u32 *)cmdq_pkt_get_va_by_offset(pkt, aal_frm->condi_offset);
	if (unlikely(!condi_inst))
		mml_pq_err("%s wrong offset %u\n", __func__, aal_frm->condi_offset);

	*condi_inst = (u32)CMDQ_REG_SHIFT_ADDR(begin_pa);

	for (i = 0; i < 8; i++) {
		cmdq_pkt_read_addr(pkt, base_pa + AAL_DUAL_PIPE_00 + i * 4, idx_val);
		cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);

		lop.reg = true;
		lop.idx = idx_out;
		rop.reg = false;
		rop.value = 4;
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_out, &lop, &rop);
	}

	for (i = 0; i < 8; i++) {
		cmdq_pkt_read_addr(pkt, base_pa + AAL_DUAL_PIPE_08 + i * 4, idx_val);
		cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);

		lop.reg = true;
		lop.idx = idx_out;
		rop.reg = false;
		rop.value = 4;
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_out, &lop, &rop);
	}

	mml_pq_rb_msg("%s end job_id[%d] engine_id[%d] va[%p] pa[%llx] pkt[%p]",
		__func__, task->job.jobid, comp->id, task->pq_task->aal_hist[pipe]->va,
		task->pq_task->aal_hist[pipe]->pa, pkt);

	mml_pq_rb_msg("%s end job_id[%d] condi:offset[%u] inst[%p], begin:offset[%u] pa[%llx]",
		__func__, task->job.jobid, aal_frm->condi_offset, condi_inst,
		aal_frm->begin_offset, begin_pa);
}

static void aal_readback_vcp(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg)
{
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct mml_comp_aal *aal = comp_to_aal(comp);
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	u8 pipe = ccfg->pipe;
	struct aal_frame_data *aal_frm = aal_frm_data(ccfg);


	u32 gpr = aal->data->gpr[ccfg->pipe];
	u32 engine = CMDQ_VCP_ENG_MML_AAL0 + pipe;

	mml_pq_rb_msg("%s aal_hist[%p] pipe[%d]", __func__,
		task->pq_task->aal_hist[pipe], pipe);

	if (!(task->pq_task->aal_hist[pipe])) {
		task->pq_task->aal_hist[pipe] =
			kzalloc(sizeof(struct mml_pq_readback_buffer), GFP_KERNEL);

		if (unlikely(!task->pq_task->aal_hist[pipe])) {
			mml_pq_err("%s not enough mem for aal_hist", __func__);
			return;
		}

		task->pq_task->aal_hist[pipe]->va =
			cmdq_get_vcp_buf(engine, &(task->pq_task->aal_hist[pipe]->pa));
	}

	mml_pq_get_vcp_buf_offset(task, MML_PQ_AAL0+pipe, task->pq_task->aal_hist[pipe]);

	cmdq_vcp_enable(true);

	cmdq_pkt_readback(pkt, engine, task->pq_task->aal_hist[pipe]->va_offset,
		 AAL_HIST_NUM+AAL_DUAL_INFO_NUM, gpr,
		&(reuse->labels[reuse->label_idx]),
		&(aal_frm->polling_reuse));

	aal_frm->labels[AAL_CURVE_NUM] = reuse->label_idx;
	reuse->labels[reuse->label_idx].val = task->pq_task->aal_hist[pipe]->va_offset;
	reuse->label_idx++;

	mml_pq_rb_msg("%s end job_id[%d] engine_id[%d] va[%p] pa[%llx] pkt[%p] offset[%d]",
			__func__, task->job.jobid, comp->id, task->pq_task->aal_hist[pipe]->va,
			task->pq_task->aal_hist[pipe]->pa, pkt,
			task->pq_task->aal_hist[pipe]->va_offset);
}

static s32 aal_config_post(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	struct mml_frame_dest *dest = &task->config->info.dest[ccfg->node->out_idx];
	struct mml_comp_aal *aal = comp_to_aal(comp);
	bool vcp = aal->data->vcp_readback;

	mml_pq_msg("%s start engine_id[%d] en_dre[%d]", __func__, comp->id,
			dest->pq_config.en_dre);

	if (!dest->pq_config.en_dre)
		goto exit;

	if (vcp)
		aal_readback_vcp(comp, task, ccfg);
	else
		aal_readback_cmdq(comp, task, ccfg);

	mml_pq_put_comp_config_result(task);

exit:
	return 0;
}

static s32 aal_reconfig_frame(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg)
{
	struct aal_frame_data *aal_frm = aal_frm_data(ccfg);
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct mml_pq_comp_config_result *result;
	s32 ret = 0;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s engine_id[%d] en_dre[%d]", __func__, comp->id,
			dest->pq_config.en_dre);
	if (!dest->pq_config.en_dre)
		goto exit;

	ret = mml_pq_get_comp_config_result(task, AAL_WAIT_TIMEOUT_MS);
	if (!ret) {
		result = get_aal_comp_config_result(task);
		if (result) {
			s32 i;
			u32 *curve = result->aal_curve;

			for (i = 0; i < AAL_CURVE_NUM; i++)
				mml_update(reuse, aal_frm->labels[i], curve[i]);

			mml_pq_msg("%s is_aal_need_readback[%d]", __func__,
					result->is_aal_need_readback);
			aal_frm->is_aal_need_readback = result->is_aal_need_readback;
		} else {
			mml_pq_err("%s: not get result from user lib", __func__);
		}
	} else {
		mml_pq_comp_config_clear(task);
		mml_pq_err("get aal param timeout: %d in %dms",
			ret, AAL_WAIT_TIMEOUT_MS);
	}

exit:
	mml_pq_trace_ex_end();
	return ret;
}

static s32 aal_config_repost(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	struct aal_frame_data *aal_frm = aal_frm_data(ccfg);
	struct mml_frame_dest *dest = &task->config->info.dest[ccfg->node->out_idx];
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	u8 pipe = ccfg->pipe;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	dma_addr_t begin_pa = 0;
	u32 *condi_inst = NULL;
	struct mml_comp_aal *aal = comp_to_aal(comp);
	bool vcp = aal->data->vcp_readback;
	u32 engine = CMDQ_VCP_ENG_MML_AAL0 + pipe;

	mml_pq_msg("%s engine_id[%d] en_dre[%d]", __func__, comp->id,
			dest->pq_config.en_dre);

	if (!dest->pq_config.en_dre)
		goto exit;

	if (vcp) {
		if (!(task->pq_task->aal_hist[pipe])) {
			task->pq_task->aal_hist[pipe] =
				kzalloc(sizeof(struct mml_pq_readback_buffer), GFP_KERNEL);

			if (unlikely(!task->pq_task->aal_hist[pipe])) {
				mml_pq_err("%s not enough mem for aal_hist", __func__);
				goto comp_config_put;
			}
			mml_pq_rb_msg("%s aal_hist[%p] pipe[%d]", __func__,
				task->pq_task->aal_hist[pipe], pipe);

			task->pq_task->aal_hist[pipe]->va =
				cmdq_get_vcp_buf(engine, &(task->pq_task->aal_hist[pipe]->pa));
		}

		cmdq_vcp_enable(true);
		mml_pq_get_vcp_buf_offset(task, MML_PQ_AAL0+pipe,
			task->pq_task->aal_hist[pipe]);

		reuse->labels[aal_frm->labels[AAL_CURVE_NUM]].val =
			cmdq_pkt_vcp_reuse_val(engine,
			task->pq_task->aal_hist[pipe]->va_offset,
			AAL_HIST_NUM+AAL_DUAL_INFO_NUM);

		cmdq_pkt_reuse_poll(pkt, &(aal_frm->polling_reuse));

		mml_pq_rb_msg("%s end job_id[%d] engine_id[%d] va[%p] pa[%llx] pkt[%p] offset[%d]",
			__func__, task->job.jobid, comp->id, task->pq_task->aal_hist[pipe]->va,
			task->pq_task->aal_hist[pipe]->pa, pkt,
			task->pq_task->aal_hist[pipe]->va_offset);

	} else {
		mml_pq_get_readback_buffer(task, pipe, &(task->pq_task->aal_hist[pipe]));

		if (unlikely(!task->pq_task->aal_hist[pipe])) {
			mml_pq_err("%s job_id[%d] aal_hist is null", __func__,
				task->job.jobid);
			goto comp_config_put;
		}

		mml_update(reuse, aal_frm->labels[AAL_CURVE_NUM],
			(u32)task->pq_task->aal_hist[pipe]->pa);
		mml_update(reuse, aal_frm->labels[AAL_CURVE_NUM+1],
			(u32)(task->pq_task->aal_hist[pipe]->pa >> 32));

		begin_pa = cmdq_pkt_get_pa_by_offset(pkt, aal_frm->begin_offset);
		condi_inst = (u32 *)cmdq_pkt_get_va_by_offset(pkt, aal_frm->condi_offset);
		if (unlikely(!condi_inst))
			mml_pq_err("%s wrong offset %u\n", __func__, aal_frm->condi_offset);

		*condi_inst = (u32)CMDQ_REG_SHIFT_ADDR(begin_pa);

		mml_pq_rb_msg("%s end job_id[%d] engine_id[%d] va[%p] pa[%llx] pkt[%p]",
			__func__, task->job.jobid, comp->id, task->pq_task->aal_hist[pipe]->va,
			task->pq_task->aal_hist[pipe]->pa, pkt);

		mml_pq_rb_msg("%s end job_id[%d]condi:offset[%u]inst[%p],begin:offset[%u]pa[%llx]",
			__func__, task->job.jobid, aal_frm->condi_offset, condi_inst,
			aal_frm->begin_offset, begin_pa);
	}

comp_config_put:
	mml_pq_put_comp_config_result(task);
exit:
	return 0;

}


static const struct mml_comp_config_ops aal_cfg_ops = {
	.prepare = aal_prepare,
	.buf_prepare = aal_buf_prepare,
	.get_label_count = aal_get_label_count,
	.init = aal_config_init,
	.frame = aal_config_frame,
	.tile = aal_config_tile,
	.post = aal_config_post,
	.reframe = aal_reconfig_frame,
	.repost = aal_config_repost,
};

static inline bool aal_reg_poll(struct mml_comp *comp, u32 addr, u32 value, u32 mask)
{
	bool return_value = false;
	u32 reg_value = 0;
	u32 polling_time = 0;
	void __iomem *base = comp->base;

	do {
		reg_value = readl(base + addr);

		if ((reg_value & mask) == value) {
			return_value = true;
			break;
		}

		udelay(AAL_POLL_SLEEP_TIME_US);
		polling_time += AAL_POLL_SLEEP_TIME_US;
	} while (polling_time < AAL_MAX_POLL_TIME_US);

	return return_value;
}

static void aal_task_done_readback(struct mml_comp *comp, struct mml_task *task,
					 struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct aal_frame_data *aal_frm = aal_frm_data(ccfg);
	struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct mml_comp_aal *aal = comp_to_aal(comp);
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	u8 pipe = ccfg->pipe;
	bool vcp = aal->data->vcp_readback;
	u32 engine = MML_PQ_AAL0 + pipe;
	u32 offset = 0;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s is_aal_need_readback[%d] id[%d] en_dre[%d]", __func__,
			aal_frm->is_aal_need_readback, comp->id, dest->pq_config.en_dre);

	mml_pq_msg("%s job_id[%d] pkt %p size %zu", __func__, task->job.jobid,
		pkt, pkt->cmd_buf_size);
	if (!dest->pq_config.en_dre || !task->pq_task->aal_hist[pipe])
		goto exit;

	offset = vcp ? task->pq_task->aal_hist[pipe]->va_offset : 0;

	mml_pq_rb_msg("%s job_id[%d] id[%d] pipe[%d] en_dre[%d] va[%p] pa[%llx] offset[%d]",
		__func__, task->job.jobid, comp->id, ccfg->pipe,
		dest->pq_config.en_dre, task->pq_task->aal_hist[pipe]->va,
		task->pq_task->aal_hist[pipe]->pa,
		task->pq_task->aal_hist[pipe]->va_offset);


	if (!pipe) {
		mml_pq_rb_msg("%s job_id[%d] hist[0~4]={%08x, %08x, %08x, %08x, %08x}",
			__func__, task->job.jobid,
			task->pq_task->aal_hist[pipe]->va[offset/4+0],
			task->pq_task->aal_hist[pipe]->va[offset/4+1],
			task->pq_task->aal_hist[pipe]->va[offset/4+2],
			task->pq_task->aal_hist[pipe]->va[offset/4+3],
			task->pq_task->aal_hist[pipe]->va[offset/4+4]);

		mml_pq_rb_msg("%s job_id[%d] hist[10~14]={%08x, %08x, %08x, %08x, %08}",
			__func__, task->job.jobid,
			task->pq_task->aal_hist[pipe]->va[offset/4+10],
			task->pq_task->aal_hist[pipe]->va[offset/4+11],
			task->pq_task->aal_hist[pipe]->va[offset/4+12],
			task->pq_task->aal_hist[pipe]->va[offset/4+13],
			task->pq_task->aal_hist[pipe]->va[offset/4+14]);
	} else {
		mml_pq_rb_msg("%s job_id[%d] hist[600~604]={%08x, %08x, %08x, %08x, %08x",
			 __func__, task->job.jobid,
			task->pq_task->aal_hist[pipe]->va[offset/4+600],
			task->pq_task->aal_hist[pipe]->va[offset/4+601],
			task->pq_task->aal_hist[pipe]->va[offset/4+602],
			task->pq_task->aal_hist[pipe]->va[offset/4+603],
			task->pq_task->aal_hist[pipe]->va[offset/4+604]);

		mml_pq_rb_msg("%s job_id[%d] hist[610~614]={%08x, %08x, %08x, %08x, %08x",
			__func__, task->job.jobid,
			task->pq_task->aal_hist[pipe]->va[offset/4+610],
			task->pq_task->aal_hist[pipe]->va[offset/4+611],
			task->pq_task->aal_hist[pipe]->va[offset/4+612],
			task->pq_task->aal_hist[pipe]->va[offset/4+613],
			task->pq_task->aal_hist[pipe]->va[offset/4+614]);
	}


	/*remain code for ping-pong in the feature*/
	if (!dest->pq_config.en_dre) {
		u32 addr = aal->sram_hist_start;
		void __iomem *base = comp->base;
		s32 i;
		s32 dual_info_start = AAL_HIST_NUM;
		u32 *phist = kmalloc((AAL_HIST_NUM+AAL_DUAL_INFO_NUM)*sizeof(u32),
			GFP_KERNEL);

		for (i = 0; i < AAL_HIST_NUM; i++) {
			if (aal_reg_poll(comp, AAL_INTSTA, (0x1 << 1), (0x1 << 1))) {
				do {
					writel(addr, base + AAL_SRAM_RW_IF_2);

					if (aal_reg_poll(comp, AAL_SRAM_STATUS,
							(0x1 << 17), (0x1 << 17)) != true) {
						mml_pq_log("%s idx[%d]", __func__, i);
						break;
					}
					phist[i] = readl(base + AAL_SRAM_RW_IF_3);
				} while (0);
				addr = addr + 4;
			}
		}
		if (task->config->dual) {
			phist[dual_info_start++] = readl(base + AAL_DUAL_PIPE_00);
			phist[dual_info_start++] = readl(base + AAL_DUAL_PIPE_01);
			phist[dual_info_start++] = readl(base + AAL_DUAL_PIPE_02);
			phist[dual_info_start++] = readl(base + AAL_DUAL_PIPE_03);
			phist[dual_info_start++] = readl(base + AAL_DUAL_PIPE_04);
			phist[dual_info_start++] = readl(base + AAL_DUAL_PIPE_05);
			phist[dual_info_start++] = readl(base + AAL_DUAL_PIPE_06);
			phist[dual_info_start++] = readl(base + AAL_DUAL_PIPE_07);
			phist[dual_info_start++] = readl(base + AAL_DUAL_PIPE_08);
			phist[dual_info_start++] = readl(base + AAL_DUAL_PIPE_09);
			phist[dual_info_start++] = readl(base + AAL_DUAL_PIPE_10);
			phist[dual_info_start++] = readl(base + AAL_DUAL_PIPE_11);
			phist[dual_info_start++] = readl(base + AAL_DUAL_PIPE_12);
			phist[dual_info_start++] = readl(base + AAL_DUAL_PIPE_13);
			phist[dual_info_start++] = readl(base + AAL_DUAL_PIPE_14);
			phist[dual_info_start++] = readl(base + AAL_DUAL_PIPE_15);
		}
		mml_pq_aal_readback(task, ccfg->pipe, phist);
	}


	if (aal_frm->is_aal_need_readback)
		mml_pq_aal_readback(task, ccfg->pipe,
			&(task->pq_task->aal_hist[pipe]->va[offset/4]));

	if (vcp) {
		mml_pq_put_vcp_buf_offset(task, engine, task->pq_task->aal_hist[pipe]);
		cmdq_vcp_enable(false);
	} else
		mml_pq_put_readback_buffer(task, pipe, task->pq_task->aal_hist[pipe]);
exit:
	mml_pq_trace_ex_end();
}

static const struct mml_comp_hw_ops aal_hw_ops = {
	.clk_enable = &mml_comp_clk_enable,
	.clk_disable = &mml_comp_clk_disable,
	.task_done = aal_task_done_readback,
};

static void aal_debug_dump(struct mml_comp *comp)
{
	void __iomem *base = comp->base;
	u32 value[9];
	u32 shadow_ctrl;

	mml_err("aal component %u dump:", comp->id);

	/* Enable shadow read working */
	shadow_ctrl = readl(base + AAL_SHADOW_CTRL);
	shadow_ctrl |= 0x4;
	writel(shadow_ctrl, base + AAL_SHADOW_CTRL);

	value[0] = readl(base + AAL_INTSTA);
	value[1] = readl(base + AAL_STATUS);
	value[2] = readl(base + AAL_INPUT_COUNT);
	value[3] = readl(base + AAL_OUTPUT_COUNT);
	value[4] = readl(base + AAL_SIZE);
	value[5] = readl(base + AAL_OUTPUT_SIZE);
	value[6] = readl(base + AAL_OUTPUT_OFFSET);
	value[7] = readl(base + AAL_TILE_00);
	value[8] = readl(base + AAL_TILE_01);

	mml_err("AAL_INTSTA %#010x AAL_STATUS %#010x AAL_INPUT_COUNT %#010x AAL_OUTPUT_COUNT %#010x",
		value[0], value[1], value[2], value[3]);
	mml_err("AAL_SIZE %#010x AAL_OUTPUT_SIZE %#010x AAL_OUTPUT_OFFSET %#010x",
		value[4], value[5], value[6]);
	mml_err("AAL_TILE_00 %#010x AAL_TILE_01 %#010x",
		value[7], value[8]);
}

static const struct mml_comp_debug_ops aal_debug_ops = {
	.dump = &aal_debug_dump,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_aal *aal = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	s32 ret;

	if (!drm_dev) {
		ret = mml_register_comp(master, &aal->comp);
		if (ret)
			dev_err(dev, "Failed to register mml component %s: %d\n",
				dev->of_node->full_name, ret);
	} else {
		ret = mml_ddp_comp_register(drm_dev, &aal->ddp_comp);
		if (ret)
			dev_err(dev, "Failed to register ddp component %s: %d\n",
				dev->of_node->full_name, ret);
		else
			aal->ddp_bound = true;
	}
	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_aal *aal = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	if (!drm_dev) {
		mml_unregister_comp(master, &aal->comp);
	} else {
		mml_ddp_comp_unregister(drm_dev, &aal->ddp_comp);
		aal->ddp_bound = false;
	}
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static const struct mtk_ddp_comp_funcs ddp_comp_funcs = {
};

static struct mml_comp_aal *dbg_probed_components[4];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_comp_aal *priv;
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

	if (of_property_read_u32(dev->of_node, "sram_curve_base", &priv->sram_curve_start))
		dev_err(dev, "read curve base fail\n");

	if (of_property_read_u32(dev->of_node, "sram_his_base", &priv->sram_hist_start))
		dev_err(dev, "read his base fail\n");

	/* assign ops */
	priv->comp.tile_ops = &aal_tile_ops;
	priv->comp.config_ops = &aal_cfg_ops;
	priv->comp.hw_ops = &aal_hw_ops;
	priv->comp.debug_ops = &aal_debug_ops;

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

const struct of_device_id mml_aal_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6983-mml_aal",
		.data = &mt6983_aal_data,
	},
	{
		.compatible = "mediatek,mt6893-mml_aal",
		.data = &mt6893_aal_data
	},
	{
		.compatible = "mediatek,mt6879-mml_aal",
		.data = &mt6879_aal_data
	},
	{
		.compatible = "mediatek,mt6895-mml_aal0",
		.data = &mt6895_aal0_data
	},
	{
		.compatible = "mediatek,mt6895-mml_aal1",
		.data = &mt6895_aal1_data
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_aal_driver_dt_match);

struct platform_driver mml_aal_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-aal",
		.owner = THIS_MODULE,
		.of_match_table = mml_aal_driver_dt_match,
	},
};

//module_platform_driver(mml_aal_driver);

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
module_param_cb(aal_debug, &dbg_param_ops, NULL, 0644);
MODULE_PARM_DESC(aal_debug, "mml aal debug case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML AAL driver");
MODULE_LICENSE("GPL v2");
