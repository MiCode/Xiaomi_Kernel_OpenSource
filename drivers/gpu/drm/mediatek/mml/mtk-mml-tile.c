/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>

#include "mtk-mml-core.h"
#include "mtk-mml-tile.h"

#include "tile_driver.h"
#include "tile_param.h"
#include "tile_mdp_reg.h"

struct mml_tile_output fixed_tile_output;

static const struct mml_topology_path **get_topology_path(
	struct mml_task *task)
{
	return task->config->path;
}

static s32 calc_tile_in_region(struct mml_frame_config *cfg,
			   struct mml_tile_region *region,
			   TILE_FUNC_BLOCK_STRUCT *ptr_func)
{
	region->xs = ptr_func->in_pos_xs;
	region->xe = ptr_func->in_pos_xe;
	region->ys = ptr_func->in_pos_ys;
	region->ye = ptr_func->in_pos_ye;
	return 0;
}

static s32 calc_tile_out_region(struct mml_frame_config *cfg,
			    struct mml_tile_region *region,
			    TILE_FUNC_BLOCK_STRUCT *ptr_func)
{
	region->xs = ptr_func->out_pos_xs;
	region->xe = ptr_func->out_pos_xe;
	region->ys = ptr_func->out_pos_ys;
	region->ye = ptr_func->out_pos_ye;
	return 0;
}

static s32 calc_tile_luma(struct mml_frame_config *cfg,
			 struct mml_tile_offset *offset,
			 TILE_FUNC_BLOCK_STRUCT *ptr_func)
{
	offset->x = ptr_func->bias_x;
	offset->x_sub = ptr_func->offset_x;
	offset->y = ptr_func->bias_y;
	offset->y_sub = ptr_func->offset_y;
	return 0;
}

static s32 calc_tile_chroma(struct mml_frame_config *cfg,
			   struct mml_tile_offset *offset,
			   TILE_FUNC_BLOCK_STRUCT *ptr_func)
{
	offset->x = ptr_func->bias_x_c;
	offset->x_sub = ptr_func->offset_x_c;
	offset->y = ptr_func->bias_y_c;
	offset->y_sub = ptr_func->offset_y_c;
	return 0;
}

static s32 get_tile_engine(struct mml_path_node *engine,
			   struct mml_frame_config *cfg,
			   struct mml_tile_engine *tile_eng,
			   TILE_FUNC_BLOCK_STRUCT *ptr_func)
{
	struct mml_tile_region *in
		= kzalloc(sizeof(struct mml_tile_region), GFP_KERNEL);
	struct mml_tile_region *out
		= kzalloc(sizeof(struct mml_tile_region), GFP_KERNEL);
	struct mml_tile_offset *luma
		= kzalloc(sizeof(struct mml_tile_offset), GFP_KERNEL);
	struct mml_tile_offset *chroma
		= kzalloc(sizeof(struct mml_tile_offset), GFP_KERNEL);

	calc_tile_in_region(cfg, in, ptr_func);
	calc_tile_out_region(cfg, out, ptr_func);
	calc_tile_luma(cfg, luma, ptr_func);
	calc_tile_chroma(cfg, chroma, ptr_func);

	tile_eng->comp_id = engine->id;
	tile_eng->in = *in;
	tile_eng->out = *out;
	tile_eng->luma = *luma;
	tile_eng->chroma = *chroma;
	return 0;
}

static s32 calc_tile_config(struct mml_task *task,
			    const struct mml_topology_path *path,
			    struct mml_tile_config *tile,
			    u32 tile_idx,
			    TILE_PARAM_STRUCT *tile_param)
{
	struct mml_frame_config *cfg = task->config;
	u8 eng_cnt = path->tile_engine_cnt;
	u8 i = 0;
	FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param =
		tile_param->ptr_tile_func_param;
	TILE_FUNC_BLOCK_STRUCT *ptr_func;

	for (i = 0; i < eng_cnt; i++) {
		struct mml_path_node e = path->nodes[path->tile_engines[i]];
		ptr_func = &ptr_tile_func_param->func_list[i];
		get_tile_engine(&e, cfg, &tile->tile_engines[i], ptr_func);
	}

	tile->tile_no = tile_idx;
	tile->h_tile_no = tile_idx;
	tile->v_tile_no = tile_idx;
	tile->engine_cnt = eng_cnt;
	return 0;
}

#define has_tile_op(_comp, op) \
	(_comp->tile_ops && _comp->tile_ops->op)

#define call_tile_op(_comp, op, ...) \
	(has_tile_op(_comp, op) ? \
		_comp->tile_ops->op(_comp, ##__VA_ARGS__) : 0)

void prepare_tile(struct mml_task *task,
		  const struct mml_topology_path *path,
		  u8 pipe_idx,
		  struct TILE_PARAM_STRUCT *p_tile_param,
		  struct mml_tile_data *tile_data)
{
	struct mml_pipe_cache *cache = &task->config->cache[pipe_idx];
	u8 eng_cnt = path->tile_engine_cnt;
	FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param =
		p_tile_param->ptr_tile_func_param;
	TILE_FUNC_BLOCK_STRUCT  *ptr_func;
	u8 i = 0;

	for (i = 0; i < eng_cnt; i++) {
		struct mml_path_node e = path->nodes[path->tile_engines[i]];
		struct mml_comp *comp = e.comp;

		ptr_func = &ptr_tile_func_param->func_list[i];

		call_tile_op(comp, prepare, task,
			&cache->cfg[path->tile_engines[i]],
			(void*)ptr_func, (void*)tile_data);
	}
}

enum MML_TILE_STATE {
	TILE_CALC,
	TILE_DONE
};

#define MAX_TILE_NUM 8

s32 calc_tile(struct mml_task *task, u8 pipe_idx)
{
	struct mml_tile_output *output;
	struct mml_tile_config *tiles;
	const struct mml_topology_path **paths = get_topology_path(task);
	u32 tile_cnt = 0;
	TILE_PARAM_STRUCT *tile_param;
	TILE_REG_MAP_STRUCT *tile_reg_map;
	FUNC_DESCRIPTION_STRUCT *tile_func;
	ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
	bool stop = false;
	char *tile_info_file = NULL;
	enum MML_TILE_STATE tile_state = TILE_CALC;
	struct mml_tile_data *tile_data;

	mml_msg("%s task %p pipe %hhu", __func__, task, pipe_idx);

	tile_data = kzalloc(sizeof(struct mml_tile_data), GFP_KERNEL);

	memset(tile_data, 0, sizeof(struct mml_tile_data));

	mml_msg("%s task %p pipe %hhu", __func__, task, pipe_idx);

	output = kzalloc(sizeof(struct mml_tile_output), GFP_KERNEL);
	tiles = kcalloc(MAX_TILE_NUM, sizeof(struct mml_tile_config),
			GFP_KERNEL);

	/* todo: vmalloc when driver init */
	tile_param = kzalloc(sizeof(TILE_PARAM_STRUCT), GFP_KERNEL);
	tile_reg_map = kzalloc(sizeof(TILE_REG_MAP_STRUCT), GFP_KERNEL);
	tile_func = kzalloc(sizeof(FUNC_DESCRIPTION_STRUCT), GFP_KERNEL);

	memset(tile_param, 0, sizeof(TILE_PARAM_STRUCT));
	memset(tile_reg_map, 0, sizeof(TILE_REG_MAP_STRUCT));
	memset(tile_func, 0, sizeof(FUNC_DESCRIPTION_STRUCT));

	tile_param->ptr_tile_reg_map = tile_reg_map;
	tile_param->ptr_tile_func_param = tile_func;
	tile_reg_map->LAST_IRQ = 1;

	/* frame calculate */
	result = tile_convert_func(tile_param->ptr_tile_reg_map,
		tile_param->ptr_tile_func_param, paths[pipe_idx]);

	/* comp prepare initTileCalc to get each engine's in/out size */
	prepare_tile(task, paths[pipe_idx], pipe_idx, tile_param, tile_data);

	result = tile_init_config(tile_param->ptr_tile_reg_map,
		tile_param->ptr_tile_func_param);
	if (ISP_MESSAGE_TILE_OK == result)
		result = tile_frame_mode_init(tile_param->ptr_tile_reg_map,
		tile_param->ptr_tile_func_param);

	tile_proc_main_single(tile_param->ptr_tile_reg_map,
		tile_param->ptr_tile_func_param, 0, &stop, tile_info_file);

	result = tile_frame_mode_close(tile_param->ptr_tile_reg_map,
		tile_param->ptr_tile_func_param);

	result = tile_mode_init(tile_param->ptr_tile_reg_map,
		tile_param->ptr_tile_func_param);

	/* initialize stop for tile calculate */
	stop = false;
	/* tile calculate */
	while (tile_state != TILE_DONE) {
		tile_proc_main_single(tile_param->ptr_tile_reg_map,
			tile_param->ptr_tile_func_param, tile_cnt, &stop,
			tile_info_file);

		/* get tile result retrieveTileParam */
		calc_tile_config(task, paths[pipe_idx], &tiles[tile_cnt],
			tile_cnt, tile_param);

		tile_cnt++;
		if (stop) {
			result = tile_mode_close(tile_param->ptr_tile_reg_map,
				tile_param->ptr_tile_func_param);
			tile_state = TILE_DONE;
		}
	}
	output->tiles = tiles;
	output->tile_cnt = tile_cnt;

	/* put tile_output to task */
	task->config->tile_output[pipe_idx] = output;

	kfree(tile_data);
	kfree(tile_param);
	kfree(tile_reg_map);
	kfree(tile_func);
	return 0;
}

static struct mml_tile_output *get_tile_output(
	struct mml_task *task, u8 pipe_idx)
{
	return task->config->tile_output[pipe_idx];
}

void destroy_tile_output(struct mml_tile_output *output)
{
	if (!output)
		return;

	kfree(output->tiles);
	kfree(output);

	return;
}

static __attribute__((unused)) s32 destroy_all(struct mml_task *task)
{
	bool dual = task->config->dual;
	u8 pipe_cnt = 0;
	u8 pipe_idx = 0;

	if (dual) {
		pipe_cnt = 2;
	} else {
		pipe_cnt = 1;
	}

	for (pipe_idx = 0; pipe_idx < pipe_cnt; pipe_idx++) {
		destroy_tile_output(task->config->tile_output[pipe_idx]);
	}
	return 0;
}

static void dump_tile_region(char *prefix, struct mml_tile_region *region)
{
	mml_log("\t\t\t\t%s: %u, %u, %u, %u",
		prefix, region->xs, region->xe, region->ys, region->ye);
}

static void dump_tile_bias_offset(char *prefix,
				  struct mml_tile_offset *offset)
{
	mml_log("\t\t\t\t%s: %u, %u, %u, %u",
		prefix, offset->x, offset->x_sub, offset->y, offset->y_sub);
}

static void dump_tile_engine(struct mml_tile_engine *engine)
{
	u32 id = engine->comp_id;

	mml_log("\t\t\tcomp_id %u", id);
	dump_tile_region("in ", &engine->in);
	dump_tile_region("out", &engine->out);
	dump_tile_bias_offset("luma", &engine->luma);
	dump_tile_bias_offset("chro", &engine->chroma);
}

static void dump_tile_config(struct mml_tile_config *tile)
{
	u32 tile_no = tile->tile_no;
	u32 h_no = tile->h_tile_no;
	u32 v_no = tile->v_tile_no;
	u8 engine_cnt = tile->engine_cnt;
	u8 i = 0;

	mml_log("\t\ttile_no %u, h_no %u, v_no %u, eng_cnt %u",
		tile_no, h_no, v_no, engine_cnt);
	for (i = 0; i < engine_cnt; i++) {
		dump_tile_engine(&tile->tile_engines[i]);
	}
}

void dump_tile_output(struct mml_task *task, u8 pipe_idx)
{
	struct mml_tile_output *output = get_tile_output(task, pipe_idx);
	u8 tile_cnt = 0;
	u8 i = 0;

	tile_cnt = output->tile_cnt;
	mml_log("tile_cnt %u", tile_cnt);
	for (i = 0; i < tile_cnt; i++) {
		dump_tile_config(&output->tiles[i]);
	}

	return;
}
