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

#define AAL_WAIT_TIMEOUT_MS	(50)
#define AAL_POLL_SLEEP_TIME_US	(10)
#define AAL_MAX_POLL_TIME_US	(1000)
#define MAX_SRAM_BUF_NUM (2)

#define AAL_SRAM_STATUS_BIT	BIT(17)
#define AAL_HIST_MAX_SUM (300)
#define REG_NOT_SUPPORT 0xfff
#define BLK_WIDTH_DEFAULT (120)
#define BLK_HEIGH_DEFAULT (135)

/* min of label count for aal curve
 *	(AAL_CURVE_NUM * 7 / CMDQ_NUM_CMD(CMDQ_CMD_BUFFER_SIZE) + 1)
 */
#define AAL_LABEL_CNT		10

enum mml_aal_reg_index {
	AAL_EN,
	AAL_INTSTA,
	AAL_STATUS,
	AAL_CFG,
	AAL_INPUT_COUNT,
	AAL_OUTPUT_COUNT,
	AAL_SIZE,
	AAL_OUTPUT_SIZE,
	AAL_OUTPUT_OFFSET,
	AAL_SRAM_STATUS,
	AAL_SRAM_RW_IF_0,
	AAL_SRAM_RW_IF_1,
	AAL_SRAM_RW_IF_2,
	AAL_SRAM_RW_IF_3,
	AAL_SHADOW_CTRL,
	AAL_TILE_02,
	AAL_DRE_BLOCK_INFO_07,
	AAL_CFG_MAIN,
	AAL_WIN_X_MAIN,
	AAL_WIN_Y_MAIN,
	AAL_DRE_BLOCK_INFO_00,
	AAL_TILE_00,
	AAL_TILE_01,
	AAL_DUAL_PIPE_00,
	AAL_DUAL_PIPE_01,
	AAL_DUAL_PIPE_02,
	AAL_DUAL_PIPE_03,
	AAL_DUAL_PIPE_04,
	AAL_DUAL_PIPE_05,
	AAL_DUAL_PIPE_06,
	AAL_DUAL_PIPE_07,
	AAL_DRE_ROI_00,
	AAL_DRE_ROI_01,
	AAL_DUAL_PIPE_08,
	AAL_DUAL_PIPE_09,
	AAL_DUAL_PIPE_10,
	AAL_DUAL_PIPE_11,
	AAL_DUAL_PIPE_12,
	AAL_DUAL_PIPE_13,
	AAL_DUAL_PIPE_14,
	AAL_DUAL_PIPE_15,
	AAL_BILATERAL_STATUS_00,
	AAL_BILATERAL_STATUS_CTRL,
	AAL_REG_MAX_COUNT
};

static const u16 aal_reg_table_mt6983[AAL_REG_MAX_COUNT] = {
	[AAL_EN] = 0x000,
	[AAL_INTSTA] = 0x00c,
	[AAL_STATUS] = 0x010,
	[AAL_CFG] = 0x020,
	[AAL_INPUT_COUNT] = 0x024,
	[AAL_OUTPUT_COUNT] = 0x028,
	[AAL_SIZE] = 0x030,
	[AAL_OUTPUT_SIZE] = 0x034,
	[AAL_OUTPUT_OFFSET] = 0x038,
	[AAL_SRAM_STATUS] = 0x0c8,
	[AAL_SRAM_RW_IF_0] = 0x0cc,
	[AAL_SRAM_RW_IF_1] = 0x0d0,
	[AAL_SRAM_RW_IF_2] = 0x0d4,
	[AAL_SRAM_RW_IF_3] = 0x0d8,
	[AAL_SHADOW_CTRL] = 0x0f0,
	[AAL_TILE_02] = 0x0f4,
	[AAL_DRE_BLOCK_INFO_07] = 0x0f8,
	[AAL_CFG_MAIN] = 0x200,
	[AAL_WIN_X_MAIN] = 0x460,
	[AAL_WIN_Y_MAIN] = 0x464,
	[AAL_DRE_BLOCK_INFO_00] = 0x468,
	[AAL_TILE_00] = 0x4ec,
	[AAL_TILE_01] = 0x4f0,
	[AAL_DUAL_PIPE_00] = 0x500,
	[AAL_DUAL_PIPE_01] = 0x504,
	[AAL_DUAL_PIPE_02] = 0x508,
	[AAL_DUAL_PIPE_03] = 0x50c,
	[AAL_DUAL_PIPE_04] = 0x510,
	[AAL_DUAL_PIPE_05] = 0x514,
	[AAL_DUAL_PIPE_06] = 0x518,
	[AAL_DUAL_PIPE_07] = 0x51c,
	[AAL_DRE_ROI_00] = 0x520,
	[AAL_DRE_ROI_01] = 0x524,
	[AAL_DUAL_PIPE_08] = 0x544,
	[AAL_DUAL_PIPE_09] = 0x548,
	[AAL_DUAL_PIPE_10] = 0x54c,
	[AAL_DUAL_PIPE_11] = 0x550,
	[AAL_DUAL_PIPE_12] = 0x554,
	[AAL_DUAL_PIPE_13] = 0x558,
	[AAL_DUAL_PIPE_14] = 0x55c,
	[AAL_DUAL_PIPE_15] = 0x560,
	[AAL_BILATERAL_STATUS_00] = REG_NOT_SUPPORT,
	[AAL_BILATERAL_STATUS_CTRL] = REG_NOT_SUPPORT
};

static const u16 aal_reg_table_mt6985[AAL_REG_MAX_COUNT] = {
	[AAL_EN] = 0x000,
	[AAL_INTSTA] = 0x00c,
	[AAL_STATUS] = 0x010,
	[AAL_CFG] = 0x020,
	[AAL_INPUT_COUNT] = 0x024,
	[AAL_OUTPUT_COUNT] = 0x028,
	[AAL_SIZE] = 0x030,
	[AAL_OUTPUT_SIZE] = 0x034,
	[AAL_OUTPUT_OFFSET] = 0x038,
	[AAL_SRAM_STATUS] = 0x0c8,
	[AAL_SRAM_RW_IF_0] = 0x0cc,
	[AAL_SRAM_RW_IF_1] = 0x0d0,
	[AAL_SRAM_RW_IF_2] = 0x0d4,
	[AAL_SRAM_RW_IF_3] = 0x0d8,
	[AAL_SHADOW_CTRL] = 0x0f0,
	[AAL_TILE_02] = 0x0f4,
	[AAL_DRE_BLOCK_INFO_07] = 0x0f8,
	[AAL_CFG_MAIN] = 0x200,
	[AAL_WIN_X_MAIN] = 0x460,
	[AAL_WIN_Y_MAIN] = 0x464,
	[AAL_DRE_BLOCK_INFO_00] = 0x468,
	[AAL_TILE_00] = 0x4ec,
	[AAL_TILE_01] = 0x4f0,
	[AAL_DUAL_PIPE_00] = 0x500,
	[AAL_DUAL_PIPE_01] = 0x504,
	[AAL_DUAL_PIPE_02] = 0x508,
	[AAL_DUAL_PIPE_03] = 0x50c,
	[AAL_DUAL_PIPE_04] = 0x510,
	[AAL_DUAL_PIPE_05] = 0x514,
	[AAL_DUAL_PIPE_06] = 0x518,
	[AAL_DUAL_PIPE_07] = 0x51c,
	[AAL_DRE_ROI_00] = 0x520,
	[AAL_DRE_ROI_01] = 0x524,
	[AAL_DUAL_PIPE_08] = 0x544,
	[AAL_DUAL_PIPE_09] = 0x548,
	[AAL_DUAL_PIPE_10] = 0x54c,
	[AAL_DUAL_PIPE_11] = 0x550,
	[AAL_DUAL_PIPE_12] = 0x554,
	[AAL_DUAL_PIPE_13] = 0x558,
	[AAL_DUAL_PIPE_14] = 0x55c,
	[AAL_DUAL_PIPE_15] = 0x560,
	[AAL_BILATERAL_STATUS_00] = 0x588,
	[AAL_BILATERAL_STATUS_CTRL] = 0x5b8
};

struct aal_data {
	u32 min_tile_width;
	u32 tile_width;
	u32 min_hist_width;
	bool vcp_readback;
	u16 gpr[MML_PIPE_CNT];
	u16 cpr[MML_PIPE_CNT];
	const u16 *reg_table;
	bool crop;
};

static const struct aal_data mt6893_aal_data = {
	.min_tile_width = 50,
	.tile_width = 560,
	.min_hist_width = 128,
	.vcp_readback = false,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.reg_table = aal_reg_table_mt6983,
	.crop = true,
};

static const struct aal_data mt6983_aal_data = {
	.min_tile_width = 50,
	.tile_width = 1652,
	.min_hist_width = 128,
	.vcp_readback = false,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.reg_table = aal_reg_table_mt6983,
	.crop = true,
};

static const struct aal_data mt6879_aal_data = {
	.min_tile_width = 50,
	.tile_width = 1376,
	.min_hist_width = 128,
	.vcp_readback = false,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.reg_table = aal_reg_table_mt6983,
	.crop = true,
};

static const struct aal_data mt6895_aal0_data = {
	.min_tile_width = 50,
	.tile_width = 1300,
	.min_hist_width = 128,
	.vcp_readback = true,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.reg_table = aal_reg_table_mt6983,
	.crop = true,
};

static const struct aal_data mt6895_aal1_data = {
	.min_tile_width = 50,
	.tile_width = 852,
	.min_hist_width = 128,
	.vcp_readback = true,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.reg_table = aal_reg_table_mt6983,
	.crop = true,
};

static const struct aal_data mt6985_aal_data = {
	.min_tile_width = 50,
	.tile_width = 1690,
	.min_hist_width = 128,
	.vcp_readback = false,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.reg_table = aal_reg_table_mt6985,
	.crop = false,
};

static const struct aal_data mt6886_aal_data = {
	.min_tile_width = 50,
	.tile_width = 1300,
	.min_hist_width = 128,
	.vcp_readback = false,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.reg_table = aal_reg_table_mt6983,
	.crop = true,
};

struct mml_comp_aal {
	struct mtk_ddp_comp ddp_comp;
	struct mml_comp comp;
	const struct aal_data *data;
	bool ddp_bound;

	u32 sram_curve_start;
	u32 sram_hist_start;
};

enum aal_label_index {
	AAL_REUSE_LABEL = 0,
	AAL_POLLGPR_0 = AAL_LABEL_CNT,
	AAL_POLLGPR_1,
	AAL_LABEL_TOTAL
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
	u16 labels[AAL_LABEL_TOTAL];
	struct mml_reuse_array reuse_curve;
	struct mml_reuse_offset offs_curve[AAL_LABEL_CNT];
	bool is_aal_need_readback;
	bool is_clarity_need_readback;
	bool config_success;
	bool relay_mode;
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
	aal_frm->reuse_curve.offs = aal_frm->offs_curve;
	aal_frm->reuse_curve.offs_size = ARRAY_SIZE(aal_frm->offs_curve);
	return 0;
}

static s32 aal_buf_prepare(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct mml_comp_aal *aal = comp_to_aal(comp);
	struct aal_frame_data *aal_frm = aal_frm_data(ccfg);
	s32 ret = 0;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s engine_id[%d] en_dre[%d]", __func__, comp->id,
		   dest->pq_config.en_dre);
	aal_frm->relay_mode = (!(dest->pq_config.en_dre) ||
		dest->crop.r.width < aal->data->min_tile_width);

	if (!(aal_frm->relay_mode))
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
	func->in_tile_width = aal->data->tile_width;
	func->out_tile_width = aal->data->tile_width;
	func->for_func = tile_aal_for;
	func->in_tile_height  = 65535;
	func->out_tile_height = 65535;
	if (aal->data->crop) {
		func->l_tile_loss     = 8;
		func->r_tile_loss     = 8;
	} else {
		func->l_tile_loss     = 0;
		func->r_tile_loss     = 0;
	}

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

	func->in_min_width = max(min(aal->data->min_hist_width,
				     (u32)func->full_size_x_in),
				 aal->data->min_tile_width);

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

	return AAL_LABEL_TOTAL;
}

static void aal_init(struct mml_comp *comp, struct cmdq_pkt *pkt, const phys_addr_t base_pa)
{
	struct mml_comp_aal *aal = comp_to_aal(comp);

	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_EN], 0x1, U32_MAX);
	/* Enable shadow */
	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_SHADOW_CTRL], 0x2, U32_MAX);
}

static void aal_relay(struct mml_comp *comp, struct cmdq_pkt *pkt, const phys_addr_t base_pa,
		      u32 relay)
{
	struct mml_comp_aal *aal = comp_to_aal(comp);

	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_CFG], relay, 0x00000001);
}

static s32 aal_config_init(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	aal_init(comp, task->pkts[ccfg->pipe], comp->base_pa);
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
	struct mml_pq_reg *regs;
	u32 *curve;
	struct mml_pq_aal_config_param *tile_config_param;
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];
	u32 addr = aal->sram_curve_start;
	u32 gpr = aal->data->gpr[ccfg->pipe];
	u32 cpr = aal->data->cpr[ccfg->pipe];
	s32 ret = 0;
	u32 i;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s engine_id[%d] en_dre[%d]", __func__, comp->id, dest->pq_config.en_dre);
	if (aal_frm->relay_mode) {
		/* relay mode */
		aal_relay(comp, pkt, base_pa, 0x1);
		goto exit;
	}
	aal_relay(comp, pkt, base_pa, 0x0);

	mml_pq_msg("%s sram_start_addr[%d] cmdq_cpr[%d]", __func__, addr, cpr);

	if (MML_FMT_10BIT(src->format) || MML_FMT_10BIT(dest->data.format))
		cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_CFG_MAIN],
			0, 0x00000080);
	else
		cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_CFG_MAIN],
			1 << 7, 0x00000080);

	do {
		ret = mml_pq_get_comp_config_result(task, AAL_WAIT_TIMEOUT_MS);
		if (ret) {
			mml_pq_comp_config_clear(task);
			aal_frm->config_success = false;
			mml_pq_err("get aal param timeout: %d in %dms",
				ret, AAL_WAIT_TIMEOUT_MS);
			ret = -ETIMEDOUT;
			goto exit;
		}

		result = get_aal_comp_config_result(task);
		if (!result) {
			mml_pq_err("%s: not get result from user lib", __func__);
			ret = -EBUSY;
			goto exit;
		}
	} while ((mml_pq_debug_mode & MML_PQ_SET_TEST) && result->is_set_test);

	regs = result->aal_regs;
	curve = result->aal_curve;

	/* TODO: use different regs */
	mml_pq_msg("%s:config aal regs, count: %d", __func__, result->aal_reg_cnt);
	aal_frm->config_success = true;
	for (i = 0; i < result->aal_reg_cnt; i++) {
		cmdq_pkt_write(pkt, NULL, base_pa + regs[i].offset,
			regs[i].value, regs[i].mask);
		mml_pq_msg("[aal][config][%x] = %#x mask(%#x)",
			regs[i].offset, regs[i].value, regs[i].mask);
	}
	for (i = 0; i < AAL_CURVE_NUM; i++, addr += 4) {
		cmdq_pkt_write(pkt, NULL,
			base_pa + aal->data->reg_table[AAL_SRAM_RW_IF_0], addr, U32_MAX);
		cmdq_pkt_poll(pkt, NULL, (0x1 << 16),
			base_pa + aal->data->reg_table[AAL_SRAM_STATUS], (0x1 << 16), gpr);
		mml_write_array(pkt, base_pa + aal->data->reg_table[AAL_SRAM_RW_IF_1], curve[i],
			U32_MAX, reuse, cache, &aal_frm->reuse_curve);
	}

	mml_pq_msg("%s is_aal_need_readback[%d] base_pa[%llx] reuses[%u]",
		__func__, result->is_aal_need_readback, base_pa,
		aal_frm->reuse_curve.idx);

	aal_frm->is_aal_need_readback = result->is_aal_need_readback;
	aal_frm->is_clarity_need_readback = result->is_clarity_need_readback;
	aal_frm->config_success = true;

	tile_config_param = &(result->aal_param[ccfg->node->out_idx]);
	aal_frm->dre_blk_width = tile_config_param->dre_blk_width;
	aal_frm->dre_blk_height = tile_config_param->dre_blk_height;
	mml_pq_msg("%s: success dre_blk_width[%d], dre_blk_height[%d]",
		__func__, tile_config_param->dre_blk_width,
		tile_config_param->dre_blk_height);
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
	struct mml_comp_aal *aal = comp_to_aal(comp);

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
	u32 hist_first_tile = 0, hist_last_tile = 0;
	s32 ret = 0;

	mml_pq_trace_ex_begin("%s", __func__);
	aal_input_w = tile->in.xe - tile->in.xs + 1;
	aal_input_h = tile->in.ye - tile->in.ys + 1;
	aal_output_w = tile->out.xe - tile->out.xs + 1;
	aal_output_h = tile->out.ye - tile->out.ys + 1;
	aal_crop_x_offset = tile->out.xs - tile->in.xs;
	aal_crop_y_offset = tile->out.ys - tile->in.ys;

	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_SIZE],
		(aal_input_w << 16) + aal_input_h, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_OUTPUT_OFFSET],
		(aal_crop_x_offset << 16) + aal_crop_y_offset, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_OUTPUT_SIZE],
		(aal_output_w << 16) + aal_output_h, U32_MAX);

	if (aal_frm->relay_mode)
		goto exit;

	if (!idx) {
		if (task->config->dual)
			aal_frm->cut_pos_x = dest->crop.r.width / 2;
		else
			aal_frm->cut_pos_x = dest->crop.r.width;
		if (ccfg->pipe)
			aal_frm->out_hist_xs = aal_frm->cut_pos_x;
	}

	dre_blk_width =
		(aal_frm->dre_blk_width) ? aal_frm->dre_blk_width : BLK_WIDTH_DEFAULT;
	dre_blk_height =
		(aal_frm->dre_blk_height) ? aal_frm->dre_blk_height : BLK_HEIGH_DEFAULT;

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
		hist_first_tile = 1;
		hist_last_tile = (tile_cnt == 1) ? 1 : 0;
		save_first_blk_col_flag = (ccfg->pipe) ? 1 : 0;
		if (idx + 1 >= tile_cnt)
			save_last_blk_col_flag = (ccfg->pipe) ? 0 : 1;
		else
			save_last_blk_col_flag = 0;
	} else if (idx + 1 >= tile_cnt) {
		aal_frm->out_hist_xs = 0;
		save_first_blk_col_flag = 0;
		save_last_blk_col_flag = (ccfg->pipe) ? 0 : 1;
		hist_first_tile = 0;
		hist_last_tile = 1;
	} else {
		if (task->config->dual && ccfg->pipe)
			aal_frm->out_hist_xs = tile->in.xs + act_win_x_end + 1;
		else
			aal_frm->out_hist_xs = tile->out.xe + 1;
		save_first_blk_col_flag = 0;
		save_last_blk_col_flag = 0;
		hist_first_tile = 0;
		hist_last_tile = 0;
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

	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_DRE_BLOCK_INFO_00],
		(act_win_x_end << 16) | act_win_x_start, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_DRE_BLOCK_INFO_07],
		(act_win_y_end << 16) | act_win_y_start, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_TILE_00],
		(save_first_blk_col_flag << 23) | (save_last_blk_col_flag << 22) |
		(last_tile_x_flag << 21) | (last_tile_y_flag << 20) |
		(blk_num_x_end << 15) | (blk_num_x_start << 10) |
		(blk_num_y_end << 5) | blk_num_y_start, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_TILE_01],
		(blk_cnt_x_end << 16) | blk_cnt_x_start, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_TILE_02],
		(blk_cnt_y_end << 16) | blk_cnt_y_start, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_DRE_ROI_00],
		(roi_x_end << 16) | roi_x_start, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_DRE_ROI_01],
		(roi_y_end << 16) | roi_y_start, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_WIN_X_MAIN],
		(win_x_end << 16) | win_x_start, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_WIN_Y_MAIN],
		(win_y_end << 16) | win_y_start, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_BILATERAL_STATUS_CTRL],
		(hist_last_tile << 2) | (hist_first_tile << 1) | 1, U32_MAX);

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
		reuse, cache, &aal_frm->labels[AAL_POLLGPR_0]);
	mml_assign(pkt, idx_out + 1, (u32)(pa >> 32),
		reuse, cache, &aal_frm->labels[AAL_POLLGPR_1]);

	/* loop again here */
	aal_frm->begin_offset = pkt->cmd_buf_size;
	begin_pa = cmdq_pkt_get_pa_by_offset(pkt, aal_frm->begin_offset);

	/* config aal sram addr and poll */
	cmdq_pkt_write_reg_addr(pkt, base_pa + aal->data->reg_table[AAL_SRAM_RW_IF_2],
		idx_addr, U32_MAX);
	/* use gpr low as poll gpr */
	cmdq_pkt_poll_addr(pkt, AAL_SRAM_STATUS_BIT,
		base_pa + aal->data->reg_table[AAL_SRAM_STATUS],
		AAL_SRAM_STATUS_BIT, poll_gpr);
	/* read to value spr */
	cmdq_pkt_read_addr(pkt, base_pa + aal->data->reg_table[AAL_SRAM_RW_IF_3], idx_val);
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
		cmdq_pkt_read_addr(pkt, base_pa + aal->data->reg_table[AAL_DUAL_PIPE_00] + i * 4,
			idx_val);
		cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);

		lop.reg = true;
		lop.idx = idx_out;
		rop.reg = false;
		rop.value = 4;
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_out, &lop, &rop);
	}

	for (i = 0; i < 8; i++) {
		cmdq_pkt_read_addr(pkt, base_pa + aal->data->reg_table[AAL_DUAL_PIPE_08] + i * 4,
			idx_val);
		cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);

		lop.reg = true;
		lop.idx = idx_out;
		rop.reg = false;
		rop.value = 4;
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_out, &lop, &rop);
	}

	for (i = 0; i < AAL_CLARITY_STATUS_NUM; i++) {
		cmdq_pkt_read_addr(pkt,
			base_pa + aal->data->reg_table[AAL_BILATERAL_STATUS_00] + i * 4,
			idx_val);
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
		&reuse->labels[reuse->label_idx],
		&aal_frm->polling_reuse);

	add_reuse_label(reuse, &aal_frm->labels[AAL_POLLGPR_0],
		task->pq_task->aal_hist[pipe]->va_offset);

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
	struct aal_frame_data *aal_frm = aal_frm_data(ccfg);
	bool vcp = aal->data->vcp_readback;

	mml_pq_msg("%s start engine_id[%d] en_dre[%d]", __func__, comp->id,
			dest->pq_config.en_dre);

	if (aal_frm->relay_mode)
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
	u32 *curve, i, j, idx;
	s32 ret = 0;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s engine_id[%d] en_dre[%d] config_success[%d]", __func__, comp->id,
			dest->pq_config.en_dre, aal_frm->config_success);
	if (aal_frm->relay_mode)
		goto exit;

	do {
		ret = mml_pq_get_comp_config_result(task, AAL_WAIT_TIMEOUT_MS);
		if (ret) {
			mml_pq_comp_config_clear(task);
			mml_pq_err("get aal param timeout: %d in %dms",
				ret, AAL_WAIT_TIMEOUT_MS);
			ret = -ETIMEDOUT;
			goto exit;
		}

		result = get_aal_comp_config_result(task);
		if (!result || !aal_frm->config_success) {
			mml_pq_err("%s: not get result from user lib", __func__);
			ret = -EBUSY;
			goto exit;
		}
	} while ((mml_pq_debug_mode & MML_PQ_SET_TEST) && result->is_set_test);


	curve = result->aal_curve;
	idx = 0;
	for (i = 0; i < aal_frm->reuse_curve.idx; i++)
		for (j = 0; j < aal_frm->reuse_curve.offs[i].cnt; j++, idx++)
			mml_update_array(reuse, &aal_frm->reuse_curve, i, j, curve[idx]);

	mml_pq_msg("%s is_aal_need_readback[%d] is_clarity_need_readback[%d]",
		__func__, result->is_aal_need_readback,
		result->is_clarity_need_readback);
	aal_frm->is_aal_need_readback = result->is_aal_need_readback;
	aal_frm->is_clarity_need_readback = result->is_clarity_need_readback;
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

	if (aal_frm->relay_mode)
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

		mml_update(reuse, aal_frm->labels[AAL_POLLGPR_0],
			cmdq_pkt_vcp_reuse_val(engine,
			task->pq_task->aal_hist[pipe]->va_offset,
			AAL_HIST_NUM + AAL_DUAL_INFO_NUM));

		cmdq_pkt_reuse_poll(pkt, &aal_frm->polling_reuse);

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

		mml_update(reuse, aal_frm->labels[AAL_POLLGPR_0],
			(u32)task->pq_task->aal_hist[pipe]->pa);
		mml_update(reuse, aal_frm->labels[AAL_POLLGPR_1],
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

static bool get_dre_block(u32 *phist, const int block_x, const int block_y,
						  const int dre_blk_x_num)
{
	u32 read_value;
	u32 block_offset = 6 * (block_y * dre_blk_x_num + block_x);
	u32 sum = 0, i = 0;
	u32 aal_hist[AAL_HIST_BIN] = {0};
	u32 error_sum = AAL_HIST_MAX_SUM;


	if (block_x < 0 || block_y < 0) {
		mml_pq_err("Error block num block_y = %d, block_x = %d", block_y, block_x);
		return false;
	}

	do {
		if (block_offset >= AAL_HIST_NUM)
			break;
		read_value = phist[block_offset++];
		aal_hist[0] = read_value & 0xff;
		aal_hist[1] = (read_value>>8) & 0xff;
		aal_hist[2] = (read_value>>16) & 0xff;
		aal_hist[3] = (read_value>>24) & 0xff;

		if (block_offset >= AAL_HIST_NUM)
			break;
		read_value = phist[block_offset++];
		aal_hist[4] = read_value & 0xff;
		aal_hist[5] = (read_value>>8) & 0xff;
		aal_hist[6] = (read_value>>16) & 0xff;
		aal_hist[7] = (read_value>>24) & 0xff;

		if (block_offset >= AAL_HIST_NUM)
			break;
		read_value = phist[block_offset++];
		aal_hist[8] = read_value & 0xff;
		aal_hist[9] = (read_value>>8) & 0xff;
		aal_hist[10] = (read_value>>16) & 0xff;
		aal_hist[11] = (read_value>>24) & 0xff;

		if (block_offset >= AAL_HIST_NUM)
			break;
		read_value = phist[block_offset++];
		aal_hist[12] = read_value & 0xff;
		aal_hist[13] = (read_value>>8) & 0xff;
		aal_hist[14] = (read_value>>16) & 0xff;
		aal_hist[15] = (read_value>>24) & 0xff;

		if (block_offset >= AAL_HIST_NUM)
			break;
		read_value = phist[block_offset++];
		aal_hist[16] = read_value & 0xff;
	} while (0);

	for (i = 0; i < AAL_HIST_BIN; i++)
		sum += aal_hist[i];

	if (sum >= error_sum) {
		mml_pq_err("hist[0-8] = (%d %d %d %d %d %d %d %d %d)",
			aal_hist[0], aal_hist[1], aal_hist[2], aal_hist[3],
			aal_hist[4], aal_hist[5], aal_hist[6], aal_hist[7],
			aal_hist[8]);
		mml_pq_err("hist[9-16] = (%d %d %d %d %d %d %d %d)",
			aal_hist[9], aal_hist[10], aal_hist[11], aal_hist[12],
			aal_hist[13], aal_hist[14], aal_hist[15], aal_hist[16]);
		return false;
	} else
		return true;
}


static bool aal_hist_check(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg, u32 *phist)
{
	u32 blk_x = 0, blk_y = 0;
	u8 pipe = (ccfg) ? ccfg->pipe : 0;
	bool dual = (task) ? task->config->dual : false;
	struct aal_frame_data *aal_frm = aal_frm_data(ccfg);
	u32 crop_width = task->config->info.dest[ccfg->node->out_idx].crop.r.width;
	u32 crop_height = task->config->info.dest[ccfg->node->out_idx].crop.r.height;
	u32 cut_pos_x = aal_frm->cut_pos_x;
	u32 dre_blk_width = aal_frm->dre_blk_width;
	u32 dre_blk_height = aal_frm->dre_blk_height;
	u32 blk_x_start = 0;
	u32 dre_blk_y_num = 0, dre_blk_x_num = 0;

	if (dual) {
		if (pipe == 1)
			dre_blk_x_num = (crop_width - cut_pos_x) / dre_blk_width;
		if (!pipe)
			dre_blk_x_num = cut_pos_x / dre_blk_width;

		blk_x_start = cut_pos_x / dre_blk_width + 1;
	} else {
		dre_blk_x_num =	crop_width / dre_blk_width;
		blk_x_start = 0;
	}

	dre_blk_y_num =	crop_height / dre_blk_height;

	mml_pq_msg("%s pipe[%d] crop_width[%u] crop_height[%u] cut_pos_x[%u]",
		__func__, pipe, crop_width, crop_height, cut_pos_x);
	mml_pq_msg("%s dre_blk_x_num[%u] dre_blk_y_num[%u] blk_x_start[%u]",
		__func__, dre_blk_x_num, dre_blk_y_num, blk_x_start);

	for (blk_y = 0; blk_y < dre_blk_y_num; blk_y++)
		for (blk_x = blk_x_start; blk_x < dre_blk_x_num; blk_x++)
			if (!get_dre_block(phist, blk_x, blk_y, dre_blk_x_num))
				return false;
	return true;
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

	offset = vcp ? task->pq_task->aal_hist[pipe]->va_offset/4 : 0;

	mml_pq_rb_msg("%s job_id[%d] id[%d] pipe[%d] en_dre[%d] va[%p] pa[%llx] offset[%d]",
		__func__, task->job.jobid, comp->id, ccfg->pipe,
		dest->pq_config.en_dre, task->pq_task->aal_hist[pipe]->va,
		task->pq_task->aal_hist[pipe]->pa,
		task->pq_task->aal_hist[pipe]->va_offset);


	if (!pipe) {
		mml_pq_rb_msg("%s job_id[%d] hist[0~4]={%08x, %08x, %08x, %08x, %08x}",
			__func__, task->job.jobid,
			task->pq_task->aal_hist[pipe]->va[offset+0],
			task->pq_task->aal_hist[pipe]->va[offset+1],
			task->pq_task->aal_hist[pipe]->va[offset+2],
			task->pq_task->aal_hist[pipe]->va[offset+3],
			task->pq_task->aal_hist[pipe]->va[offset+4]);

		mml_pq_rb_msg("%s job_id[%d] hist[10~14]={%08x, %08x, %08x, %08x, %08x}",
			__func__, task->job.jobid,
			task->pq_task->aal_hist[pipe]->va[offset+10],
			task->pq_task->aal_hist[pipe]->va[offset+11],
			task->pq_task->aal_hist[pipe]->va[offset+12],
			task->pq_task->aal_hist[pipe]->va[offset+13],
			task->pq_task->aal_hist[pipe]->va[offset+14]);
	} else {
		mml_pq_rb_msg("%s job_id[%d] hist[600~604]={%08x, %08x, %08x, %08x, %08x",
			 __func__, task->job.jobid,
			task->pq_task->aal_hist[pipe]->va[offset+600],
			task->pq_task->aal_hist[pipe]->va[offset+601],
			task->pq_task->aal_hist[pipe]->va[offset+602],
			task->pq_task->aal_hist[pipe]->va[offset+603],
			task->pq_task->aal_hist[pipe]->va[offset+604]);

		mml_pq_rb_msg("%s job_id[%d] hist[610~614]={%08x, %08x, %08x, %08x, %08x",
			__func__, task->job.jobid,
			task->pq_task->aal_hist[pipe]->va[offset+610],
			task->pq_task->aal_hist[pipe]->va[offset+611],
			task->pq_task->aal_hist[pipe]->va[offset+612],
			task->pq_task->aal_hist[pipe]->va[offset+613],
			task->pq_task->aal_hist[pipe]->va[offset+614]);
	}


	/*remain code for ping-pong in the feature*/
	if (!dest->pq_config.en_dre) {
		u32 addr = aal->sram_hist_start;
		void __iomem *base = comp->base;
		s32 i;
		s32 dual_info_start = AAL_HIST_NUM;
		u32 poll_intsta_addr = aal->data->reg_table[AAL_INTSTA];
		u32 poll_sram_status_addr = aal->data->reg_table[AAL_SRAM_STATUS];
		u32 *phist = kmalloc((AAL_HIST_NUM+AAL_DUAL_INFO_NUM)*sizeof(u32),
			GFP_KERNEL);

		for (i = 0; i < AAL_HIST_NUM; i++) {
			if (aal_reg_poll(comp, poll_intsta_addr, (0x1 << 1), (0x1 << 1))) {
				do {
					writel(addr,
						base + aal->data->reg_table[AAL_SRAM_RW_IF_2]);

					if (aal_reg_poll(comp, poll_sram_status_addr,
						(0x1 << 17), (0x1 << 17)) != true) {
						mml_pq_log("%s idx[%d]", __func__, i);
						break;
					}

					phist[i] = readl(base +
						aal->data->reg_table[AAL_SRAM_RW_IF_3]);
					} while (0);
					addr = addr + 4;
			}
		}
		if (task->config->dual) {
			phist[dual_info_start++] =
				readl(base + aal->data->reg_table[AAL_DUAL_PIPE_00]);
			phist[dual_info_start++] =
				readl(base + aal->data->reg_table[AAL_DUAL_PIPE_01]);
			phist[dual_info_start++] =
				readl(base + aal->data->reg_table[AAL_DUAL_PIPE_02]);
			phist[dual_info_start++] =
				readl(base + aal->data->reg_table[AAL_DUAL_PIPE_03]);
			phist[dual_info_start++] =
				readl(base + aal->data->reg_table[AAL_DUAL_PIPE_04]);
			phist[dual_info_start++] =
				readl(base + aal->data->reg_table[AAL_DUAL_PIPE_05]);
			phist[dual_info_start++] =
				readl(base + aal->data->reg_table[AAL_DUAL_PIPE_06]);
			phist[dual_info_start++] =
				readl(base + aal->data->reg_table[AAL_DUAL_PIPE_07]);
			phist[dual_info_start++] =
				readl(base + aal->data->reg_table[AAL_DUAL_PIPE_08]);
			phist[dual_info_start++] =
				readl(base + aal->data->reg_table[AAL_DUAL_PIPE_09]);
			phist[dual_info_start++] =
				readl(base + aal->data->reg_table[AAL_DUAL_PIPE_10]);
			phist[dual_info_start++] =
				readl(base + aal->data->reg_table[AAL_DUAL_PIPE_11]);
			phist[dual_info_start++] =
				readl(base + aal->data->reg_table[AAL_DUAL_PIPE_12]);
			phist[dual_info_start++] =
				readl(base + aal->data->reg_table[AAL_DUAL_PIPE_13]);
			phist[dual_info_start++] =
				readl(base + aal->data->reg_table[AAL_DUAL_PIPE_14]);
			phist[dual_info_start++] =
				readl(base + aal->data->reg_table[AAL_DUAL_PIPE_15]);
		}
		mml_pq_aal_readback(task, ccfg->pipe, phist);
	}

	if (aal_frm->is_aal_need_readback) {
		mml_pq_aal_readback(task, ccfg->pipe,
			&(task->pq_task->aal_hist[pipe]->va[offset]));

		if (mml_pq_debug_mode & MML_PQ_HIST_CHECK) {
			if (!aal_hist_check(comp, task, ccfg,
				&(task->pq_task->aal_hist[pipe]->va[offset]))) {
				mml_pq_err("%s hist error", __func__);
				mml_pq_util_aee("MML_PQ_AAL_Histogram Error",
					"AAL Histogram error need to check jobid:%d",
					task->job.jobid);
			}
		}
	}

	if (aal_frm->is_clarity_need_readback) {

		mml_pq_rb_msg("%s job_id[%d] calrity_hist[0~4]={%08x, %08x, %08x, %08x, %08x",
			__func__, task->job.jobid,
			task->pq_task->aal_hist[pipe]->va[offset+AAL_HIST_NUM+AAL_DUAL_INFO_NUM+0],
			task->pq_task->aal_hist[pipe]->va[offset+AAL_HIST_NUM+AAL_DUAL_INFO_NUM+1],
			task->pq_task->aal_hist[pipe]->va[offset+AAL_HIST_NUM+AAL_DUAL_INFO_NUM+2],
			task->pq_task->aal_hist[pipe]->va[offset+AAL_HIST_NUM+AAL_DUAL_INFO_NUM+3],
			task->pq_task->aal_hist[pipe]->va[offset+AAL_HIST_NUM+AAL_DUAL_INFO_NUM+4]);


		mml_pq_clarity_readback(task, ccfg->pipe,
			&(task->pq_task->aal_hist[pipe]->va[
			offset+AAL_HIST_NUM+AAL_DUAL_INFO_NUM]),
			AAL_CLARITY_HIST_START, AAL_CLARITY_STATUS_NUM);
	}

	if (vcp) {
		mml_pq_put_vcp_buf_offset(task, engine, task->pq_task->aal_hist[pipe]);
		cmdq_vcp_enable(false);
	} else
		mml_pq_put_readback_buffer(task, pipe, &(task->pq_task->aal_hist[pipe]));
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
	struct mml_comp_aal *aal = comp_to_aal(comp);
	void __iomem *base = comp->base;
	u32 value[9];
	u32 shadow_ctrl;

	mml_err("aal component %u dump:", comp->id);

	/* Enable shadow read working */
	shadow_ctrl = readl(base + aal->data->reg_table[AAL_SHADOW_CTRL]);
	shadow_ctrl |= 0x4;
	writel(shadow_ctrl, base + aal->data->reg_table[AAL_SHADOW_CTRL]);

	value[0] = readl(base + aal->data->reg_table[AAL_INTSTA]);
	value[1] = readl(base + aal->data->reg_table[AAL_STATUS]);
	value[2] = readl(base + aal->data->reg_table[AAL_INPUT_COUNT]);
	value[3] = readl(base + aal->data->reg_table[AAL_OUTPUT_COUNT]);
	value[4] = readl(base + aal->data->reg_table[AAL_SIZE]);
	value[5] = readl(base + aal->data->reg_table[AAL_OUTPUT_SIZE]);
	value[6] = readl(base + aal->data->reg_table[AAL_OUTPUT_OFFSET]);
	value[7] = readl(base + aal->data->reg_table[AAL_TILE_00]);
	value[8] = readl(base + aal->data->reg_table[AAL_TILE_01]);

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

	if (of_property_read_u32(dev->of_node, "sram-curve-base", &priv->sram_curve_start))
		dev_err(dev, "read curve base fail\n");

	if (of_property_read_u32(dev->of_node, "sram-his-base", &priv->sram_hist_start))
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
	{
		.compatible = "mediatek,mt6985-mml_aal",
		.data = &mt6985_aal_data
	},
	{
		.compatible = "mediatek,mt6886-mml_aal",
		.data = &mt6886_aal_data
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
module_param_cb(aal_debug, &dbg_param_ops, NULL, 0644);
MODULE_PARM_DESC(aal_debug, "mml aal debug case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML AAL driver");
MODULE_LICENSE("GPL v2");
