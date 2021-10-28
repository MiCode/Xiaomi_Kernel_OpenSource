/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __TILE_PARAM_H__
#define __TILE_PARAM_H__

#include "mtk-mml-core.h"

/* only refer by tile core, tile driver, & ut entry file only */
typedef struct tile_param {
	struct tile_reg_map *ptr_tile_reg_map;
	struct func_description *ptr_tile_func_param;
} TILE_PARAM_STRUCT;

/* prototype dp interface */
enum isp_tile_message tile_convert_func(struct tile_reg_map *ptr_tile_reg_map,
	struct func_description *ptr_tile_func_param,
	const struct mml_topology_path *path);
enum isp_tile_message tile_init_config(struct tile_reg_map *ptr_tile_reg_map,
	struct func_description *ptr_tile_func_param);
enum isp_tile_message tile_frame_mode_init(
	struct tile_reg_map *ptr_tile_reg_map,
	struct func_description *ptr_tile_func_param);
enum isp_tile_message tile_frame_mode_close(
	struct tile_reg_map *ptr_tile_reg_map,
	struct func_description *ptr_tile_func_param);
enum isp_tile_message tile_mode_init(struct tile_reg_map *ptr_tile_reg_map,
	struct func_description *ptr_tile_func_param);
enum isp_tile_message tile_mode_close(struct tile_reg_map *ptr_tile_reg_map,
	struct func_description *ptr_tile_func_param);
enum isp_tile_message tile_proc_main_single(
	struct tile_reg_map *ptr_tile_reg_map,
	struct func_description *ptr_tile_func_param,
	int tile_no, bool *stop_flag);

#endif
