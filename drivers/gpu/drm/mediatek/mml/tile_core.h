/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __TILE_CORE_H__
#define __TILE_CORE_H__

extern bool tile_init_mdp_func_property(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
extern ISP_TILE_MESSAGE_ENUM tile_lut_mdp_func_output_disable(int module_no, TILE_FUNC_ENABLE_STRUCT *ptr_func_en,
                            TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
#endif
