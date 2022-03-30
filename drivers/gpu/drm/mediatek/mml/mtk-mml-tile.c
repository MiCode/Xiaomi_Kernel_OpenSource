// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/vmalloc.h>

#include "mtk-mml-core.h"
#include "mtk-mml-tile.h"
#include "tile_driver.h"

static const struct mml_topology_path *get_topology_path(
	struct mml_task *task, u32 pipe)
{
	return task->config->path[pipe];
}

static const struct mml_path_node *get_tile_node(
	const struct mml_topology_path *path, u32 eng_idx)
{
	return &path->nodes[path->tile_engines[eng_idx]];
}

static void set_tile_in_region(struct mml_tile_region *region,
			       struct tile_func_block *func)
{
	region->xs = func->in_pos_xs;
	region->xe = func->in_pos_xe;
	region->ys = func->in_pos_ys;
	region->ye = func->in_pos_ye;
}

static void set_tile_out_region(struct mml_tile_region *region,
				struct tile_func_block *func)
{
	region->xs = func->out_pos_xs;
	region->xe = func->out_pos_xe;
	region->ys = func->out_pos_ys;
	region->ye = func->out_pos_ye;
}

static void set_tile_luma(struct mml_tile_offset *offset,
			  struct tile_func_block *func)
{
	offset->x = func->bias_x;
	offset->x_sub = func->offset_x;
	offset->y = func->bias_y;
	offset->y_sub = func->offset_y;
}

static void set_tile_chroma(struct mml_tile_offset *offset,
			    struct tile_func_block *func)
{
	offset->x = func->bias_x_c;
	offset->x_sub = func->offset_x_c;
	offset->y = func->bias_y_c;
	offset->y_sub = func->offset_y_c;
}

static void set_tile_engine(const struct mml_path_node *engine,
			    struct mml_tile_engine *tile_eng,
			    struct tile_func_block *func)
{
	tile_eng->comp_id = engine->id;
	set_tile_in_region(&tile_eng->in, func);
	set_tile_out_region(&tile_eng->out, func);
	set_tile_luma(&tile_eng->luma, func);
	set_tile_chroma(&tile_eng->chroma, func);
}

static void set_tile_config(const struct mml_topology_path *path,
			    struct mml_tile_config *tile,
			    u32 tile_idx,
			    struct func_description *tile_func)
{
	u32 eng_cnt = path->tile_engine_cnt;
	u32 i;

	for (i = 0; i < eng_cnt; i++) {
		const struct mml_path_node *e = get_tile_node(path, i);
		struct tile_func_block *func = tile_func->func_list[i];

		set_tile_engine(e, &tile->tile_engines[i], func);
	}

	tile->tile_no = tile_idx;
	tile->engine_cnt = eng_cnt;
}

#define has_tile_op(_comp, op) \
	(_comp->tile_ops && _comp->tile_ops->op)
#define call_tile_op(_comp, op, ...) \
	(has_tile_op(_comp, op) ? \
		_comp->tile_ops->op(_comp, ##__VA_ARGS__) : 0)

static s32 prepare_tile(struct mml_task *task,
			const struct mml_topology_path *path, u32 pipe,
			struct func_description *tile_func,
			union mml_tile_data *tile_datas)
{
	struct mml_pipe_cache *cache = &task->config->cache[pipe];
	u32 eng_cnt = path->tile_engine_cnt;
	u32 i;
	s32 ret;

	for (i = 0; i < eng_cnt; i++) {
		struct mml_comp *comp = get_tile_node(path, i)->comp;
		struct tile_func_block *func = tile_func->func_list[i];

		if (unlikely(comp->id != func->func_num)) {
			mml_err("[tile]mismatched tile_func(%d) and comp(%d) at [%d]",
				func->func_num, comp->id, i);
			return -EINVAL;
		}
		ret = call_tile_op(comp, prepare, task,
				   &cache->cfg[path->tile_engines[i]],
				   func, &tile_datas[i]);
		if (ret) {
			mml_err("[tile]comp(%d) prepare fail %d", comp->id, ret);
			return ret;
		}
	}
	return 0;
}

#define MAX_DECOUPLE_TILE_NUM 8
#define MAX_RACING_90_270_TILE_NUM 64
#define MAX_RACING_0_180_TILE_NUM 128
#define MAX_TILE_NUM 128 /* must be max value of above cases */

static s32 tile_message_to_errno(enum isp_tile_message result)
{
	switch (result) {
	case ISP_MESSAGE_TILE_OK:
		return 0;
	case ISP_MESSAGE_OVER_MAX_BRANCH_NO_ERROR:
	case ISP_MESSAGE_TILE_FUNC_CANNOT_FIND_LAST_FUNC_ERROR:
	case ISP_MESSAGE_SCHEDULING_BACKWARD_ERROR:
	case ISP_MESSAGE_SCHEDULING_FORWARD_ERROR:
	case MDP_MESSAGE_WROT_INVALID_FORMAT:
	case MDP_MESSAGE_NULL_DATA:
	case MDP_MESSAGE_INVALID_STATE:
		return -EINVAL;
	case ISP_MESSAGE_IN_CONST_X_ERROR:
	case ISP_MESSAGE_IN_CONST_Y_ERROR:
	case ISP_MESSAGE_OUT_CONST_X_ERROR:
	case ISP_MESSAGE_OUT_CONST_Y_ERROR:
	case ISP_MESSAGE_INIT_INCORRECT_X_INPUT_SIZE_POS_ERROR:
	case ISP_MESSAGE_INIT_INCORRECT_Y_INPUT_SIZE_POS_ERROR:
	case ISP_MESSAGE_INIT_INCORRECT_X_OUTPUT_SIZE_POS_ERROR:
	case ISP_MESSAGE_INIT_INCORRECT_Y_OUTPUT_SIZE_POS_ERROR:
	case ISP_MESSAGE_DISABLE_FUNC_X_SIZE_CHECK_ERROR:
	case ISP_MESSAGE_DISABLE_FUNC_Y_SIZE_CHECK_ERROR:
		return -EDOM;
	case ISP_MESSAGE_TILE_LOSS_OVER_TILE_HEIGHT_ERROR:
	case ISP_MESSAGE_TILE_LOSS_OVER_TILE_WIDTH_ERROR:
	case ISP_MESSAGE_CHECK_IN_CONFIG_ALIGN_XS_POS_ERROR:
	case ISP_MESSAGE_CHECK_IN_CONFIG_ALIGN_XE_POS_ERROR:
	case ISP_MESSAGE_CHECK_IN_CONFIG_ALIGN_YS_POS_ERROR:
	case ISP_MESSAGE_CHECK_IN_CONFIG_ALIGN_YE_POS_ERROR:
	case ISP_MESSAGE_XSIZE_NOT_DIV_BY_IN_CONST_X_ERROR:
	case ISP_MESSAGE_YSIZE_NOT_DIV_BY_IN_CONST_Y_ERROR:
	case ISP_MESSAGE_XSIZE_NOT_DIV_BY_OUT_CONST_X_ERROR:
	case ISP_MESSAGE_YSIZE_NOT_DIV_BY_OUT_CONST_Y_ERROR:
	case ISP_MESSAGE_TILE_FORWARD_OUT_OVER_TILE_WIDTH_ERROR:
	case ISP_MESSAGE_TILE_FORWARD_OUT_OVER_TILE_HEIGHT_ERROR:
	case ISP_MESSAGE_TILE_BACKWARD_IN_OVER_TILE_WIDTH_ERROR:
	case ISP_MESSAGE_TILE_BACKWARD_IN_OVER_TILE_HEIGHT_ERROR:
	case ISP_MESSAGE_FORWARD_CHECK_TOP_EDGE_ERROR:
	case ISP_MESSAGE_FORWARD_CHECK_BOTTOM_EDGE_ERROR:
	case ISP_MESSAGE_FORWARD_CHECK_LEFT_EDGE_ERROR:
	case ISP_MESSAGE_FORWARD_CHECK_RIGHT_EDGE_ERROR:
	case ISP_MESSAGE_BACKWARD_CHECK_TOP_EDGE_ERROR:
	case ISP_MESSAGE_BACKWARD_CHECK_BOTTOM_EDGE_ERROR:
	case ISP_MESSAGE_BACKWARD_CHECK_LEFT_EDGE_ERROR:
	case ISP_MESSAGE_BACKWARD_CHECK_RIGHT_EDGE_ERROR:
	case ISP_MESSAGE_CHECK_OUT_CONFIG_ALIGN_XS_POS_ERROR:
	case ISP_MESSAGE_CHECK_OUT_CONFIG_ALIGN_XE_POS_ERROR:
	case ISP_MESSAGE_CHECK_OUT_CONFIG_ALIGN_YS_POS_ERROR:
	case ISP_MESSAGE_CHECK_OUT_CONFIG_ALIGN_YE_POS_ERROR:
	case MDP_MESSAGE_BACKWARD_START_LESS_THAN_FORWARD:
	case MDP_MESSAGE_RESIZER_SCALING_ERROR:
		return -ERANGE;
	default:
		return -EINVAL;
	}
}

struct tile_ctx {
	/* output */
	struct mml_tile_output *output;
	/* working */
	union mml_tile_data *tile_datas;
	struct tile_reg_map *tile_reg_map;
	struct func_description *tile_func;
};

static void destroy_tile_working(struct tile_ctx *ctx)
{
	/* free working but keep output */
	kfree(ctx->tile_datas);
	kfree(ctx->tile_reg_map);
	kfree(ctx->tile_func);
}

static void dump_tile_working(struct tile_ctx *ctx)
{
	u32 i;
	struct tile_reg_map *tile_reg_map = ctx->tile_reg_map;
	struct func_description *tile_func = ctx->tile_func;
	struct tile_func_block *func;

	if (!tile_func)
		return;
	mml_log(
		"run_mode:%d, first_frame:%d, first_pass:%d, tile_no(h:%d v:%d) used:%d",
		tile_reg_map->run_mode,
		tile_reg_map->first_frame,
		tile_reg_map->first_pass,
		tile_reg_map->curr_horizontal_tile_no,
		tile_reg_map->curr_vertical_tile_no,
		tile_reg_map->used_tile_no);
	mml_log(
		"func_num: full_in     , full_out    , mima_out(xs, xe, ys, ye), in_pos  (xs, xe, ys, ye), out_pos (xs, xe, ys, ye), back_in (xs, xe, ys, ye), back_out(xs, xe, ys, ye), edge, disable");
	for (i = 0; i < tile_func->used_func_no; i++) {
		func = tile_func->func_list[i];
		mml_log(
			" [%5d]: (%4d, %4d), (%4d, %4d), (%4d, %4d, %4d, %4d), (%4d, %4d, %4d, %4d), (%4d, %4d, %4d, %4d), (%4d, %4d, %4d, %4d), (%4d, %4d, %4d, %4d), 0x%02x,  (%1d, %1d)",
			func->func_num,
			func->full_size_x_in, func->full_size_y_in,
			func->full_size_x_out, func->full_size_y_out,
			func->min_out_pos_xs, func->max_out_pos_xe,
			func->min_out_pos_ys, func->max_out_pos_ye,
			func->in_pos_xs, func->in_pos_xe,
			func->in_pos_ys, func->in_pos_ye,
			func->out_pos_xs, func->out_pos_xe,
			func->out_pos_ys, func->out_pos_ye,
			func->backward_input_xs_pos, func->backward_input_xe_pos,
			func->backward_input_ys_pos, func->backward_input_ye_pos,
			func->backward_output_xs_pos, func->backward_output_xe_pos,
			func->backward_output_ys_pos, func->backward_output_ye_pos,
			func->tdr_edge,
			func->tdr_h_disable_flag, func->tdr_v_disable_flag);
	}
}

static s32 create_tile_ctx(struct tile_ctx *ctx, u32 eng_cnt, size_t tile_max,
			   struct mml_tile_cache *tile_cache)
{
	u32 i;

	if (!tile_cache->ready) {
		if (ARRAY_SIZE(tile_cache->func_list) !=
			ARRAY_SIZE(ctx->tile_func->func_list))
			mml_err("%s tile func list count not match %u and %u",
				__func__,
				(u32)ARRAY_SIZE(tile_cache->func_list),
				(u32)ARRAY_SIZE(ctx->tile_func->func_list));
		for (i = 0; i < ARRAY_SIZE(tile_cache->func_list); i++) {
			if (tile_cache->func_list[i])
				continue;
			tile_cache->func_list[i] = kmalloc(
				sizeof(struct tile_func_block), GFP_KERNEL);
			if (!tile_cache->func_list[i])
				return -ENOMEM;
		}

		if (!tile_cache->tiles) {
			tile_cache->tiles = vmalloc(
				MAX_TILE_NUM * sizeof(*ctx->output->tiles));
			if (!tile_cache->tiles)
				return -ENOMEM;
		}

		tile_cache->ready = true;
	}

	ctx->output = kzalloc(sizeof(*ctx->output), GFP_KERNEL);
	if (!ctx->output)
		return -ENOMEM;
	ctx->tile_datas = kcalloc(eng_cnt, sizeof(*ctx->tile_datas), GFP_KERNEL);
	if (!ctx->tile_datas)
		return -ENOMEM;
	/* todo: vmalloc when driver init */
	ctx->tile_reg_map = kzalloc(sizeof(*ctx->tile_reg_map), GFP_KERNEL);
	if (!ctx->tile_reg_map)
		return -ENOMEM;
	ctx->tile_func = kzalloc(sizeof(*ctx->tile_func), GFP_KERNEL);
	if (!ctx->tile_func)
		return -ENOMEM;
	for (i = 0; i < ARRAY_SIZE(tile_cache->func_list); i++)
		ctx->tile_func->func_list[i] = tile_cache->func_list[i];
	ctx->output->tiles = tile_cache->tiles;
	memset(ctx->output->tiles, 0, tile_max * sizeof(*ctx->output->tiles));
	return 0;
}

static s32 calc_frame(struct mml_task *task, u32 pipe,
	const struct mml_topology_path *path, struct tile_ctx *ctx)
{
	struct tile_reg_map *tile_reg_map = ctx->tile_reg_map;
	struct func_description *tile_func = ctx->tile_func;
	enum isp_tile_message result;
	bool stop = false;
	s32 ret;

	/* frame calculate */
	result = tile_convert_func(tile_reg_map, tile_func, path);
	if (result != ISP_MESSAGE_TILE_OK)
		goto err_tile;

	/* comp prepare initTileCalc to get each engine's in/out size */
	ret = prepare_tile(task, path, pipe, tile_func, ctx->tile_datas);
	if (ret)
		goto err_exit;

	result = tile_init_config(tile_reg_map, tile_func);
	if (result != ISP_MESSAGE_TILE_OK)
		goto err_tile;

	result = tile_frame_mode_init(tile_reg_map, tile_func);
	if (result != ISP_MESSAGE_TILE_OK)
		goto err_tile;

	result = tile_proc_main_single(tile_reg_map, tile_func, 0, &stop);
	if (result != ISP_MESSAGE_TILE_OK)
		goto err_tile;

	result = tile_frame_mode_close(tile_reg_map, tile_func);
	if (result != ISP_MESSAGE_TILE_OK)
		goto err_tile;

	return 0;

err_tile:
	ret = tile_message_to_errno(result);
	mml_err("%s fail message %d errno %d", __func__, result, ret);
err_exit:
	return ret;
}

static s32 calc_tile_loop(const struct mml_topology_path *path,
	struct tile_ctx *ctx)
{
	struct tile_reg_map *tile_reg_map = ctx->tile_reg_map;
	struct func_description *tile_func = ctx->tile_func;
	struct mml_tile_config *tiles = ctx->output->tiles;
	u32 tile_cnt = 0;
	enum isp_tile_message result;
	bool stop;
	s32 ret;

	/* tile calculate */
	result = tile_mode_init(tile_reg_map, tile_func);
	if (result != ISP_MESSAGE_TILE_OK)
		goto err_tile;

	/* initialize stop for tile calculate */
	stop = false;
	while (!stop) {
		tiles[tile_cnt].h_tile_no = tile_reg_map->curr_horizontal_tile_no;
		tiles[tile_cnt].v_tile_no = tile_reg_map->curr_vertical_tile_no;

		result = tile_proc_main_single(tile_reg_map, tile_func,
					       tile_cnt, &stop);
		if (result != ISP_MESSAGE_TILE_OK)
			goto err_tile;

		/* tile result from param */
		set_tile_config(path, &tiles[tile_cnt], tile_cnt, tile_func);
		tile_cnt++;
	}

	result = tile_mode_close(tile_reg_map, tile_func);
	if (result != ISP_MESSAGE_TILE_OK)
		goto err_tile;

	ctx->output->tile_cnt = tile_cnt;
	ctx->output->h_tile_cnt = tiles[tile_cnt - 1].h_tile_no + 1;
	ctx->output->v_tile_cnt = tiles[tile_cnt - 1].v_tile_no + 1;
	return 0;

err_tile:
	ret = tile_message_to_errno(result);
	mml_err("%s fail message %d errno %d", __func__, result, ret);
	return ret;
}

static s32 calc_frame_mode(const struct mml_topology_path *path,
	struct tile_ctx *ctx)
{
	struct mml_tile_output *output = ctx->output;
	struct mml_tile_config *tiles = output->tiles;
	struct tile_func_block *func = ctx->tile_func->func_list[0];

	/* tile result from param once */
	set_tile_config(path, &tiles[0], 0, ctx->tile_func);

	output->tile_cnt = 1;
	output->h_tile_cnt = 1;
	output->v_tile_cnt = 1;
	output->src_crop.left = func->in_pos_xs;
	output->src_crop.top = func->in_pos_ys;
	output->src_crop.width = func->in_pos_xe - func->in_pos_xs + 1;
	output->src_crop.height = func->in_pos_ye - func->in_pos_ys + 1;
	return 0;
}

s32 calc_tile(struct mml_task *task, u32 pipe, struct mml_tile_cache *tile_cache)
{
	size_t tile_max;
	const struct mml_frame_dest *dest = &task->config->info.dest[0];
	const struct mml_topology_path *path = get_topology_path(task, pipe);
	struct tile_ctx ctx = {0};
	u32 eng_cnt = path->tile_engine_cnt;
	s32 ret = 0;

	mml_msg("%s task %p pipe %u", __func__, task, pipe);

	if (task->config->info.mode == MML_MODE_RACING) {
		if (task->config->info.dest_cnt > 1)
			mml_err("Racing mode but output count > 1");

		if (dest->rotate == MML_ROT_90 || dest->rotate == MML_ROT_270)
			tile_max = MAX_RACING_90_270_TILE_NUM;
		else
			tile_max = MAX_RACING_0_180_TILE_NUM;
	} else if (task->config->framemode) {
		tile_max = 1;
	} else {
		tile_max = MAX_DECOUPLE_TILE_NUM;
	}

	ret = create_tile_ctx(&ctx, eng_cnt, tile_max, tile_cache);
	if (ret) {
		mml_err("no memory to create tile context");
		goto free_output;
	}

	ret = calc_frame(task, pipe, path, &ctx);
	if (ret)
		goto err_tile;

	if (task->config->framemode)
		ret = calc_frame_mode(path, &ctx);
	else
		ret = calc_tile_loop(path, &ctx);
	if (ret)
		goto err_tile;

	/* put tile_output to task */
	task->config->tile_output[pipe] = ctx.output;
	goto free_working;

err_tile:
	dump_tile_working(&ctx);
	dump_tile_output(ctx.output);
free_output:
	mml_err("%s free output %d", __func__, ret);
	destroy_tile_output(ctx.output);
free_working:
	destroy_tile_working(&ctx);
	return ret;
}

void destroy_tile_output(struct mml_tile_output *output)
{
	kfree(output);
}

static void dump_tile_region(char *prefix, struct mml_tile_region *region)
{
	mml_log("\t\t\t%s: x:(%4u, %4u), y:(%4u, %4u)",
		prefix, region->xs, region->xe, region->ys, region->ye);
}

static void dump_tile_offset(char *prefix, struct mml_tile_offset *offset)
{
	mml_log("\t\t\t%s: x:(%4u, %4u), y:(%4u, %4u)",
		prefix, offset->x, offset->x_sub, offset->y, offset->y_sub);
}

static void dump_tile_engine(struct mml_tile_engine *engine)
{
	static const struct mml_tile_offset zero = {0};

	mml_log("\t\tcomp %u", engine->comp_id);
	if (memcmp(&engine->in, &engine->out, sizeof(engine->in))) {
		dump_tile_region("  in", &engine->in);
		dump_tile_region(" out", &engine->out);
	} else {
		dump_tile_region("size", &engine->in);
	}
	if (memcmp(&engine->luma, &zero, sizeof(engine->luma)))
		dump_tile_offset("luma", &engine->luma);
	if (memcmp(&engine->luma, &engine->chroma, sizeof(engine->luma)))
		dump_tile_offset("chro", &engine->chroma);
}

static void dump_tile_config(struct mml_tile_config *tile)
{
	u32 engine_cnt = tile->engine_cnt;
	u32 i;

	mml_log("\ttile_no:%u (h:%u v:%u), eng_cnt:%u, eol:%d",
		tile->tile_no, tile->h_tile_no, tile->v_tile_no, engine_cnt,
		tile->eol);
	for (i = 0; i < engine_cnt; i++)
		dump_tile_engine(&tile->tile_engines[i]);
}

void dump_tile_output(struct mml_tile_output *output)
{
	u32 tile_cnt = output->tile_cnt;
	u32 i;

	mml_log("tile_cnt:%u (h:%u v:%u)",
		tile_cnt, output->h_tile_cnt, output->v_tile_cnt);
	for (i = 0; i < tile_cnt; i++)
		dump_tile_config(&output->tiles[i]);
}

