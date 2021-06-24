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

static s32 get_src_width(struct mml_frame_config *cfg)
{
	return cfg->info.src.width;
}

static s32 get_src_height(struct mml_frame_config *cfg)
{
	return cfg->info.src.height;
}

static s32 get_dst_width(struct mml_frame_config *cfg, u8 dst_id)
{
	return cfg->info.dest[dst_id].data.width;
}

static s32 get_dst_height(struct mml_frame_config *cfg, u8 dst_id)
{
	return cfg->info.dest[dst_id].data.height;
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

void dual_pipe_wrot_mt6893(u8 pipe_idx,
			   struct mml_frame_dest *dest,
			   struct wrot_tile_data *wrot_data)
{
	struct mml_frame_data *data = &dest->data;

	if (pipe_idx == 0) {
		wrot_data->crop_left = 0;
		wrot_data->crop_width = data->width >> 1;

		if (dest->rotate == MML_ROT_0 || dest->rotate == MML_ROT_180)
			wrot_data->crop_width = data->width >> 1;
		else
			wrot_data->crop_width = data->height >> 1;

		if (MML_FMT_10BIT_PACKED(data->format) &&
			(wrot_data->crop_width & 3)) {
			wrot_data->crop_width =
				(wrot_data->crop_width & ~3) + 4;
			if ((dest->rotate == MML_ROT_0 && dest->flip) ||
				(dest->rotate == MML_ROT_180 && !dest->flip) ||
				dest->rotate == MML_ROT_270)
				wrot_data->crop_width =
					data->width - wrot_data->crop_width;
		} else if (wrot_data->crop_width & 1)
			wrot_data->crop_width++;
	} else {


		if (dest->rotate == MML_ROT_0 || dest->rotate == MML_ROT_180)
			wrot_data->crop_left = data->width >> 1;
		else
			wrot_data->crop_left = data->height >> 1;

		if (MML_FMT_10BIT_PACKED(data->format) &&
			(wrot_data->crop_left & 3)) {
			wrot_data->crop_left =
				(wrot_data->crop_left & ~3) + 4;
			if ((dest->rotate == MML_ROT_0 && dest->flip) ||
				(dest->rotate == MML_ROT_180 && !dest->flip) ||
				dest->rotate == MML_ROT_270)
				wrot_data->crop_left =
					data->width - wrot_data->crop_left;
		} else if (wrot_data->crop_left & 1)
			wrot_data->crop_left++;
		if (dest->rotate == MML_ROT_0 || dest->rotate == MML_ROT_180)
			wrot_data->crop_width =
				data->width - wrot_data->crop_left;
		else
			wrot_data->crop_width =
				data->height - wrot_data->crop_left;
	}
}

void prepare_tile_mt6893(struct mml_task *task,
			 const struct mml_topology_path *path,
			 u8 pipe_idx,
			 struct TILE_PARAM_STRUCT *p_tile_param,
			 struct rdma_tile_data *rdma_data,
			 struct hdr_tile_data *hdr_data,
			 struct aal_tile_data *aal_data,
			 struct rsz_tile_data *rsz_data,
			 struct tdshp_tile_data *tdshp_data,
			 struct wrot_tile_data *wrot_data)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_data *src = &cfg->info.src;
	struct mml_frame_dest *dest = &cfg->info.dest[0]; //index need to fix
	u8 eng_cnt = path->tile_engine_cnt;
	u8 i = 0;
	u32 in_crop_w, in_crop_h;
	bool enable_rdma_crop = false;

	FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param =
		p_tile_param->ptr_tile_func_param;
	TILE_FUNC_BLOCK_STRUCT  *ptr_func;

	/* move to rdma prepare */
	if (cfg->info.dest_cnt == 1 &&
	    (dest->crop.r.width != get_src_width(cfg) ||
	     dest->crop.r.height != get_src_height(cfg)))
		enable_rdma_crop = true;

	for (i = 0; i < eng_cnt; i++) {
		struct mml_path_node e = path->nodes[path->tile_engines[i]];
		ptr_func = &ptr_tile_func_param->func_list[i];

		/* data */
		switch (e.id) {
			case 7:
			case 8:
				/* move to rdma prepare */
				rdma_data->src_fmt = src->format;
				rdma_data->blk_shift_w =
					MML_FMT_BLOCK(src->format)? 4: 0;
				rdma_data->blk_shift_h =
					MML_FMT_BLOCK(src->format)? 5: 0;
				if (cfg->info.dest_cnt == 1) {
					rdma_data->crop.left =
						dest->crop.r.left;
					rdma_data->crop.top =
						dest->crop.r.top;
					rdma_data->crop.width =
						dest->crop.r.width;
					rdma_data->crop.height =
						dest->crop.r.height;
				} else {
					rdma_data->crop.left = 0;
					rdma_data->crop.top = 0;
					rdma_data->crop.width = src->width;
					rdma_data->crop.height = src->height;
				}
				rdma_data->alpharot = cfg->alpharot;
				rdma_data->max_width = 640;
				ptr_func->func_data =
					(struct TILE_FUNC_DATA_STRUCT*)
					rdma_data;
				break;
			case 13:
			case 14:
				/* move to hdr prepare */
				hdr_data->relay_mode = dest->pq_config.en_hdr?
					false: true; //index need to fix
				hdr_data->min_width = 16;
				ptr_func->func_data =
					(struct TILE_FUNC_DATA_STRUCT*)hdr_data;
				break;
			case 17:
			case 18:
				/* move to aal prepare */
				aal_data->max_width = 560;
				aal_data->hist_min_width = 128;
				aal_data->min_width = 50;
				ptr_func->func_data =
					(struct TILE_FUNC_DATA_STRUCT*)aal_data;
				break;
			case 19:
			case 20:
				/* move to rsz prepare */
				rsz_data->max_width = 544;
				/* now relay mode no need */
				ptr_func->func_data =
					(struct TILE_FUNC_DATA_STRUCT*)rsz_data;
				break;
			case 21:
			case 22:
				/* move to tdshp prepare */
				tdshp_data->enable_hfg =
					dest->pq_config.hfg_en? true: false;
				tdshp_data->max_width = 528;
				tdshp_data->hfg_min_width = 9;
				ptr_func->func_data =
					(struct TILE_FUNC_DATA_STRUCT*)
					tdshp_data;
				break;
			case 25:
			case 26:
			case 27:
			case 28:
				/* move to wrot prepare */
				wrot_data->dest_fmt = dest->data.format;
				wrot_data->rotate = dest->rotate;
				wrot_data->flip = dest->flip;
				wrot_data->alpharot = cfg->alpharot;
				wrot_data->enable_crop = cfg->dual?
					true: false;
				if (cfg->dual) {
					dual_pipe_wrot_mt6893(pipe_idx,
						dest, wrot_data);
				} else {
					wrot_data->crop_left = 0;
					wrot_data->crop_width = 0;
				}
				wrot_data->max_width = 512;
				wrot_data->max_fifo = 256;
				ptr_func->func_data =
					(struct TILE_FUNC_DATA_STRUCT*)
					wrot_data;
				break;
			default:
				break;
		}

		/* bypass engine */
		switch (e.id) {
			/* move to component prepare; rdma, fg, pq_sout, color,
			 * tcc, wrot always true, hdr, aal, rsz, tdshp by
			 * pq_config
			 */
			case 13:
			case 14:
			case 17:
			case 18:
			case 19:
			case 20:
			case 21:
			case 22:
				ptr_func->enable_flag = false;
				break;
			default:
				ptr_func->enable_flag = true;
				break;
		}

		/* in/out size */
		switch (e.id) {
			case 7:
			case 8:
				/* move to rdma prepare */
				ptr_func->full_size_x_in = get_src_width(cfg);
				ptr_func->full_size_y_in = get_src_height(cfg);

				in_crop_w = dest->crop.r.width;
				in_crop_h = dest->crop.r.height;
				if (in_crop_w + dest->crop.r.left >
				    get_src_width(cfg))
					in_crop_w = get_src_width(cfg) -
						dest->crop.r.left;
				if (in_crop_h + dest->crop.r.top >
				    get_src_height(cfg))
					in_crop_h = get_src_height(cfg) -
						dest->crop.r.top;

				if (enable_rdma_crop) {
					if (cfg->alpharot) {
						ptr_func->full_size_x_out =
							in_crop_w;
						ptr_func->full_size_y_out =
							in_crop_h;
					} else {
						ptr_func->full_size_x_out =
							in_crop_w +
							dest->crop.r.left;
						ptr_func->full_size_y_out =
							in_crop_h +
							dest->crop.r.top;
					}
				}
				break;
			case 9:
			case 10:
			case 11:
			case 12:
			case 13:
			case 14:
			case 17:
			case 18:
				/* move to fg, hdr, aal prepare */
				if (enable_rdma_crop) {
					in_crop_w = dest->crop.r.width;
					in_crop_h = dest->crop.r.height;
					if (in_crop_w + dest->crop.r.left >
					    get_src_width(cfg))
						in_crop_w = get_src_width(cfg) -
							dest->crop.r.left;
					if (in_crop_h + dest->crop.r.top >
					    get_src_height(cfg))
						in_crop_h =
							get_src_height(cfg) -
							dest->crop.r.top;
					ptr_func->full_size_x_in =
						in_crop_w + dest->crop.r.left;
					ptr_func->full_size_y_in =
						in_crop_h + dest->crop.r.top;
				} else {
					ptr_func->full_size_x_in =
						get_src_width(cfg);
					ptr_func->full_size_y_in =
						get_src_height(cfg);

				}
				ptr_func->full_size_x_out =
					ptr_func->full_size_x_in;
				ptr_func->full_size_y_out =
					ptr_func->full_size_y_in;
				break;
			case 19:
			case 20:
				/* move to rsz prepare */
				in_crop_w = dest->crop.r.width;
				in_crop_h = dest->crop.r.height;
				if (in_crop_w + dest->crop.r.left >
				    get_src_width(cfg))
					in_crop_w = get_src_width(cfg) -
						dest->crop.r.left;
				if (in_crop_h + dest->crop.r.top >
				    get_src_height(cfg))
					in_crop_h =
						get_src_height(cfg) -
						dest->crop.r.top;
				if (enable_rdma_crop) {
					ptr_func->full_size_x_in =
						in_crop_w + dest->crop.r.left;
					ptr_func->full_size_y_in =
						in_crop_h + dest->crop.r.top;
				} else {
					ptr_func->full_size_x_in =
						get_src_width(cfg);
					ptr_func->full_size_y_in =
						get_src_height(cfg);
				}
				if (dest->rotate == MML_ROT_90 ||
					dest->rotate == MML_ROT_270) {
					ptr_func->full_size_x_out =
						get_dst_height(cfg, 0); //index need to fix
					ptr_func->full_size_y_out =
						get_dst_width(cfg, 0); //index need to fix
				} else {
					ptr_func->full_size_x_out =
						get_dst_width(cfg, 0); //index need to fix
					ptr_func->full_size_y_out =
						get_dst_height(cfg, 0); //index need to fix
				}
				break;
			case 15:
			case 16:
			case 21:
			case 22:
			case 23:
			case 24:
			case 25:
			case 26:
			case 27:
			case 28:
				/* move to tdshp, color, tcc, wrot prepare */
				if (dest->rotate == MML_ROT_90 ||
					dest->rotate == MML_ROT_270) {
					ptr_func->full_size_x_in =
						get_dst_height(cfg, 0); //index need to fix
					ptr_func->full_size_y_in =
						get_dst_width(cfg, 0); //index need to fix
					ptr_func->full_size_x_out =
						get_dst_height(cfg, 0); //index need to fix
					ptr_func->full_size_y_out =
						get_dst_width(cfg, 0); //index need to fix
				} else {
					ptr_func->full_size_x_in =
						get_dst_width(cfg, 0); //index need to fix
					ptr_func->full_size_y_in =
						get_dst_height(cfg, 0); //index need to fix
					ptr_func->full_size_x_out =
						get_dst_width(cfg, 0); //index need to fix
					ptr_func->full_size_y_out =
						get_dst_height(cfg, 0); //index need to fix
				}
				break;
			default:
				mml_err("no this engine %d", e.id);
				break;
		}
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

	struct rdma_tile_data *rdma_data;
	struct hdr_tile_data *hdr_data;
	struct aal_tile_data *aal_data;
	struct rsz_tile_data *rsz_data;
	struct tdshp_tile_data *tdshp_data;
	struct wrot_tile_data *wrot_data;

	mml_msg("%s task %p pipe %hhu", __func__, task, pipe_idx);

	rdma_data = kzalloc(sizeof(struct rdma_tile_data), GFP_KERNEL);
	hdr_data = kzalloc(sizeof(struct hdr_tile_data), GFP_KERNEL);
	aal_data = kzalloc(sizeof(struct aal_tile_data), GFP_KERNEL);
	rsz_data = kzalloc(sizeof(struct rsz_tile_data), GFP_KERNEL);
	tdshp_data = kzalloc(sizeof(struct tdshp_tile_data), GFP_KERNEL);
	wrot_data = kzalloc(sizeof(struct wrot_tile_data), GFP_KERNEL);

	memset(rdma_data, 0, sizeof(struct rdma_tile_data));
	memset(hdr_data, 0, sizeof(struct hdr_tile_data));
	memset(aal_data, 0, sizeof(struct aal_tile_data));
	memset(rsz_data, 0, sizeof(struct rsz_tile_data));
	memset(tdshp_data, 0, sizeof(struct tdshp_tile_data));
	memset(wrot_data, 0, sizeof(struct wrot_tile_data));

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
	prepare_tile_mt6893(task, paths[pipe_idx], pipe_idx, tile_param,
		rdma_data, hdr_data, aal_data, rsz_data, tdshp_data, wrot_data);

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

	kfree(rdma_data);
	kfree(hdr_data);
	kfree(rsz_data);
	kfree(tdshp_data);
	kfree(wrot_data);
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

