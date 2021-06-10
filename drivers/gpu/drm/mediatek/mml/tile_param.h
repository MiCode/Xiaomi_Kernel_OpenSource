/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __TILE_PARAM_H__
#define __TILE_PARAM_H__

#include "mtk-mml-core.h"

/* only refer by tile core, tile driver, & ut entry file only */
typedef struct TILE_PARAM_STRUCT
{
    TILE_REG_MAP_STRUCT *ptr_tile_reg_map;
    FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param;
    DIRECT_LINK_DUMP_STRUCT *ptr_direct_link_dump_param;
}TILE_PARAM_STRUCT;

/* prototype dp interface */
extern ISP_TILE_MESSAGE_ENUM tile_convert_func(TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
					       FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param,
					       const struct mml_topology_path *path);
extern ISP_TILE_MESSAGE_ENUM tile_init_config(TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
					      FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param);
extern ISP_TILE_MESSAGE_ENUM tile_frame_mode_close(TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
			FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param);
extern ISP_TILE_MESSAGE_ENUM tile_mode_init(TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
					    FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param);
extern ISP_TILE_MESSAGE_ENUM tile_mode_close(TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
					     FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param);
extern ISP_TILE_MESSAGE_ENUM tile_frame_mode_init(TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
						  FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param);

extern ISP_TILE_MESSAGE_ENUM tile_proc_main_single(TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
                    FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param,
                    int tile_no, bool *stop_flag, const char *fpt_log);
extern ISP_TILE_MESSAGE_ENUM tile_fprint_reg_map(TILE_REG_MAP_STRUCT *ptr_tile_reg_map, const char *filename,
                            const char *filename_d, const char *filename_wpe, const char *filename_wpe_d, const char *filename_eaf);
#endif
