/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __TILE_MDP_FUNC_H__
#define __TILE_MDP_FUNC_H__

/* prototype init */
ISP_TILE_MESSAGE_ENUM tile_rdma_init(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);
ISP_TILE_MESSAGE_ENUM tile_hdr_init(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);
ISP_TILE_MESSAGE_ENUM tile_aal_init(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);
ISP_TILE_MESSAGE_ENUM tile_prz_init(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);
ISP_TILE_MESSAGE_ENUM tile_tdshp_init(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);
ISP_TILE_MESSAGE_ENUM tile_wrot_init(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);
/* prototype for */
ISP_TILE_MESSAGE_ENUM tile_rdma_for(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);
ISP_TILE_MESSAGE_ENUM tile_hdr_for(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);
ISP_TILE_MESSAGE_ENUM tile_aal_for(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);
ISP_TILE_MESSAGE_ENUM tile_prz_for(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);
ISP_TILE_MESSAGE_ENUM tile_wrot_for(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);
/* prototype back */
ISP_TILE_MESSAGE_ENUM tile_rdma_back(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);
ISP_TILE_MESSAGE_ENUM tile_prz_back(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);
ISP_TILE_MESSAGE_ENUM tile_wrot_back(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);

#endif
