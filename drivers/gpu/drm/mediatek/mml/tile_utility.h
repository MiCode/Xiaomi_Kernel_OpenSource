/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __TILE_UTILITY_H__
#define __TILE_UTILITY_H__

/* tile utility.h extern */
extern ISP_TILE_MESSAGE_ENUM tile_for_comp_resizer(TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg,
                                              TILE_FUNC_BLOCK_STRUCT *ptr_func,
											  TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg);
extern ISP_TILE_MESSAGE_ENUM tile_back_comp_resizer(TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg,
                                              TILE_FUNC_BLOCK_STRUCT *ptr_func);
/* print message api */
extern const char *tile_print_error_message(ISP_TILE_MESSAGE_ENUM n);
#endif
