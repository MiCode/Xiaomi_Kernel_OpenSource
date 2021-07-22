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

static const struct mml_topology_path **get_topology_path(
	struct mml_task *task)
{
	return task->config->path;
}

static void get_tile_in_region(struct mml_frame_config *cfg,
			       struct mml_tile_region *region,
			       struct tile_func_block *ptr_func)
{
	region->xs = ptr_func->in_pos_xs;
	region->xe = ptr_func->in_pos_xe;
	region->ys = ptr_func->in_pos_ys;
	region->ye = ptr_func->in_pos_ye;
}

static void get_tile_out_region(struct mml_frame_config *cfg,
				struct mml_tile_region *region,
				struct tile_func_block *ptr_func)
{
	region->xs = ptr_func->out_pos_xs;
	region->xe = ptr_func->out_pos_xe;
	region->ys = ptr_func->out_pos_ys;
	region->ye = ptr_func->out_pos_ye;
}

static void get_tile_luma(struct mml_frame_config *cfg,
			  struct mml_tile_offset *offset,
			  struct tile_func_block *ptr_func)
{
	offset->x = ptr_func->bias_x;
	offset->x_sub = ptr_func->offset_x;
	offset->y = ptr_func->bias_y;
	offset->y_sub = ptr_func->offset_y;
}

static void get_tile_chroma(struct mml_frame_config *cfg,
			    struct mml_tile_offset *offset,
			    struct tile_func_block *ptr_func)
{
	offset->x = ptr_func->bias_x_c;
	offset->x_sub = ptr_func->offset_x_c;
	offset->y = ptr_func->bias_y_c;
	offset->y_sub = ptr_func->offset_y_c;
}

static void get_tile_engine(struct mml_path_node *engine,
			    struct mml_frame_config *cfg,
			    struct mml_tile_engine *tile_eng,
			    struct tile_func_block *ptr_func)
{
	tile_eng->comp_id = engine->id;
	get_tile_in_region(cfg, &tile_eng->in, ptr_func);
	get_tile_out_region(cfg, &tile_eng->out, ptr_func);
	get_tile_luma(cfg, &tile_eng->luma, ptr_func);
	get_tile_chroma(cfg, &tile_eng->chroma, ptr_func);
}

static s32 calc_tile_config(struct mml_task *task,
			    const struct mml_topology_path *path,
			    struct mml_tile_config *tile,
			    u32 tile_idx,
			    struct tile_param *tile_param)
{
	struct mml_frame_config *cfg = task->config;
	u8 eng_cnt = path->tile_engine_cnt;
	u8 i = 0;
	struct func_description *ptr_tile_func_param =
		tile_param->ptr_tile_func_param;
	struct tile_func_block *ptr_func;

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

static s32 prepare_tile(struct mml_task *task,
			const struct mml_topology_path *path, u32 pipe_idx,
			struct tile_param *tile_param,
			union mml_tile_data *tile_datas)
{
	struct mml_pipe_cache *cache = &task->config->cache[pipe_idx];
	u32 eng_cnt = path->tile_engine_cnt;
	struct func_description *ptr_tile_func_param =
		tile_param->ptr_tile_func_param;
	u32 i;

	for (i = 0; i < eng_cnt; i++) {
		struct mml_comp *comp = path->nodes[path->tile_engines[i]].comp;
		struct tile_func_block *ptr_func;

		ptr_func = &ptr_tile_func_param->func_list[i];
		if (comp->id != ptr_func->func_num) {
			mml_err("[tile]mismatched tile_func(%d) and comp(%d) at [%d]",
				ptr_func->func_num, comp->id, i);
			return -EINVAL;
		}
		call_tile_op(comp, prepare, task,
			&cache->cfg[path->tile_engines[i]],
			ptr_func, &tile_datas[i]);
	}
	return 0;
}

enum mml_tile_state {
	TILE_CALC,
	TILE_DONE
};

#define MAX_TILE_NUM 8

s32 calc_tile(struct mml_task *task, u32 pipe_idx)
{
	struct mml_tile_output *output;
	struct mml_tile_config *tiles;
	const struct mml_topology_path **paths = get_topology_path(task);
	u32 tile_cnt = 0;
	struct tile_param *tile_param;
	struct tile_reg_map *tile_reg_map;
	struct func_description *tile_func;
	enum isp_tile_message result = ISP_MESSAGE_TILE_OK;
	bool stop = false;
	enum mml_tile_state tile_state = TILE_CALC;
	union mml_tile_data *tile_datas;
	u32 eng_cnt = paths[pipe_idx]->tile_engine_cnt;

	mml_msg("%s task %p pipe %u", __func__, task, pipe_idx);

	tile_datas = kcalloc(eng_cnt, sizeof(*tile_datas), GFP_KERNEL);
	output = kzalloc(sizeof(*output), GFP_KERNEL);
	tiles = kcalloc(MAX_TILE_NUM, sizeof(*tiles), GFP_KERNEL);

	/* todo: vmalloc when driver init */
	tile_param = kzalloc(sizeof(*tile_param), GFP_KERNEL);
	tile_reg_map = kzalloc(sizeof(*tile_reg_map), GFP_KERNEL);
	tile_func = kzalloc(sizeof(*tile_func), GFP_KERNEL);

	tile_param->ptr_tile_reg_map = tile_reg_map;
	tile_param->ptr_tile_func_param = tile_func;

	/* frame calculate */
	result = tile_convert_func(tile_reg_map, tile_func, paths[pipe_idx]);

	/* comp prepare initTileCalc to get each engine's in/out size */
	prepare_tile(task, paths[pipe_idx], pipe_idx, tile_param, tile_datas);

	result = tile_init_config(tile_reg_map, tile_func);
	if (ISP_MESSAGE_TILE_OK == result)
		result = tile_frame_mode_init(tile_reg_map, tile_func);

	result = tile_proc_main_single(tile_reg_map, tile_func, 0, &stop);

	result = tile_frame_mode_close(tile_reg_map, tile_func);

	result = tile_mode_init(tile_reg_map, tile_func);

	/* initialize stop for tile calculate */
	stop = false;
	/* tile calculate */
	while (tile_state != TILE_DONE) {
		result = tile_proc_main_single(tile_reg_map, tile_func,
			tile_cnt, &stop);

		/* get tile result retrieveTileParam */
		calc_tile_config(task, paths[pipe_idx], &tiles[tile_cnt],
			tile_cnt, tile_param);

		tile_cnt++;
		if (stop) {
			result = tile_mode_close(tile_reg_map, tile_func);
			tile_state = TILE_DONE;
		}
	}
	output->tiles = tiles;
	output->tile_cnt = tile_cnt;

	/* put tile_output to task */
	task->config->tile_output[pipe_idx] = output;

	kfree(tile_datas);
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

