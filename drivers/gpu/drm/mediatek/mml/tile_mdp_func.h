/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __TILE_MDP_FUNC_H__
#define __TILE_MDP_FUNC_H__

/* prototype init */
enum isp_tile_message tile_rdma_init(struct tile_func_block *ptr_func,
				     struct tile_reg_map *ptr_tile_reg_map);
enum isp_tile_message tile_hdr_init(struct tile_func_block *ptr_func,
				    struct tile_reg_map *ptr_tile_reg_map);
enum isp_tile_message tile_aal_init(struct tile_func_block *ptr_func,
				    struct tile_reg_map *ptr_tile_reg_map);
enum isp_tile_message tile_prz_init(struct tile_func_block *ptr_func,
				    struct tile_reg_map *ptr_tile_reg_map);
enum isp_tile_message tile_tdshp_init(struct tile_func_block *ptr_func,
				      struct tile_reg_map *ptr_tile_reg_map);
enum isp_tile_message tile_wrot_init(struct tile_func_block *ptr_func,
				     struct tile_reg_map *ptr_tile_reg_map);
/* prototype for */
enum isp_tile_message tile_rdma_for(struct tile_func_block *ptr_func,
				    struct tile_reg_map *ptr_tile_reg_map);
enum isp_tile_message tile_crop_for(struct tile_func_block *ptr_func,
				    struct tile_reg_map *ptr_tile_reg_map);
enum isp_tile_message tile_aal_for(struct tile_func_block *ptr_func,
				   struct tile_reg_map *ptr_tile_reg_map);
enum isp_tile_message tile_prz_for(struct tile_func_block *ptr_func,
				   struct tile_reg_map *ptr_tile_reg_map);
enum isp_tile_message tile_wrot_for(struct tile_func_block *ptr_func,
				    struct tile_reg_map *ptr_tile_reg_map);
/* prototype back */
enum isp_tile_message tile_rdma_back(struct tile_func_block *ptr_func,
				     struct tile_reg_map *ptr_tile_reg_map);
enum isp_tile_message tile_prz_back(struct tile_func_block *ptr_func,
				    struct tile_reg_map *ptr_tile_reg_map);
enum isp_tile_message tile_wrot_back(struct tile_func_block *ptr_func,
				     struct tile_reg_map *ptr_tile_reg_map);
enum isp_tile_message tile_dlo_back(struct tile_func_block *ptr_func,
				    struct tile_reg_map *ptr_tile_reg_map);

#endif
