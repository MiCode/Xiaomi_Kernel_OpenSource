/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __TILE_DRIVER_H__
#define __TILE_DRIVER_H__

#include "mtk-mml-tile.h"

#define tile_driver_printf(fmt, ...) mml_err("[%s][%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)

#define MAX_TILE_HEIGHT_HW (65536)
#define MAX_TILE_PREV_NO (10)
#define MAX_TILE_BRANCH_NO (6)
#define MIN_TILE_FUNC_NO (2)
#define MAX_TILE_FUNC_NO MML_MAX_PATH_NODES /* smaller or equal to (PREVIOUS_BLK_NO_OF_START-1) */
#define MAX_INPUT_TILE_FUNC_NO (32)
#define MAX_FORWARD_FUNC_CAL_LOOP_NO (16 * MAX_TILE_FUNC_NO)
#define MAX_TILE_FUNC_EN_NO (192)
#define MAX_TILE_FUNC_NAME_SIZE (32)
#define MAX_TILE_TOT_NO (1200)

#define TILE_MOD(num, denom) (((denom) == 1) ? 0 : (((denom) == 2) ? ((num) & 0x1) : (((denom) == 4) ? \
	((num) & 0x3) : (((denom) == 8) ? ((num) & 0x7) : ((num) % (denom))))))
#define TILE_INT_DIV(num, denom) (((denom) == 1) ? (num) : (((denom) == 2) ? ((unsigned int)(num) >> 0x1) : \
	(((denom) == 4) ? ((unsigned int)(num) >> 0x2) : (((denom) == 8) ? ((unsigned int)(num) >> 0x3) : ((num) / (denom))))))

#define TILE_ORDER_Y_FIRST (0x1)
#define TILE_ORDER_RIGHT_TO_LEFT (0x2)
#define TILE_ORDER_BOTTOM_TO_TOP (0x4)

#define PREVIOUS_BLK_NO_OF_START (0xFF)

/* MAX TILE WIDTH & HEIGHT */
#define MAX_SIZE (65536)

/* TILE HORIZONTAL BUFFER */
#define MAX_TILE_BACKUP_HORZ_NO (24)

/* Tile edge */
#define TILE_EDGE_BOTTOM_MASK (0x8)
#define TILE_EDGE_TOP_MASK (0x4)
#define TILE_EDGE_RIGHT_MASK (0x2)
#define TILE_EDGE_LEFT_MASK (0x1)
#define TILE_EDGE_HORZ_MASK (TILE_EDGE_RIGHT_MASK + TILE_EDGE_LEFT_MASK)

#define LAST_MODULE_ID_OF_START (0x0fffffff)

typedef enum TILE_RUN_MODE_ENUM {
	TILE_RUN_MODE_SUB_OUT = 0x1,
	TILE_RUN_MODE_SUB_IN = TILE_RUN_MODE_SUB_OUT + 0x2,
	TILE_RUN_MODE_MAIN = TILE_RUN_MODE_SUB_IN + 0x4
} TILE_RUN_MODE_ENUM;

/* error enum */
#define ERROR_MESSAGE_DATA(n, CMD) \
	CMD(n, ISP_MESSAGE_TILE_OK)\
	CMD(n, ISP_MESSAGE_OVER_MAX_MASK_WORD_NO_ERROR)\
	CMD(n, ISP_MESSAGE_OVER_MAX_TILE_WORD_NO_ERROR)\
	CMD(n, ISP_MESSAGE_OVER_MAX_TILE_TOT_NO_ERROR)\
	CMD(n, ISP_MESSAGE_UNDER_MIN_TILE_FUNC_NO_ERROR)\
	CMD(n, ISP_MESSAGE_DUPLICATED_SUPPORT_FUNC_ERROR)\
	CMD(n, ISP_MESSAGE_OVER_MAX_BRANCH_NO_ERROR)\
	CMD(n, ISP_MESSAGE_OVER_MAX_INPUT_TILE_FUNC_NO_ERROR)\
	CMD(n, ISP_MESSAGE_TILE_FUNC_CANNOT_FIND_LAST_FUNC_ERROR)\
	CMD(n, ISP_MESSAGE_SCHEDULING_BACKWARD_ERROR)\
	CMD(n, ISP_MESSAGE_SCHEDULING_FORWARD_ERROR)\
	CMD(n, ISP_MESSAGE_IN_CONST_X_ERROR)\
	CMD(n, ISP_MESSAGE_IN_CONST_Y_ERROR)\
	CMD(n, ISP_MESSAGE_OUT_CONST_X_ERROR)\
	CMD(n, ISP_MESSAGE_OUT_CONST_Y_ERROR)\
	CMD(n, ISP_MESSAGE_INIT_INCORRECT_X_INPUT_SIZE_POS_ERROR)\
	CMD(n, ISP_MESSAGE_INIT_INCORRECT_Y_INPUT_SIZE_POS_ERROR)\
	CMD(n, ISP_MESSAGE_INIT_INCORRECT_X_OUTPUT_SIZE_POS_ERROR)\
	CMD(n, ISP_MESSAGE_INIT_INCORRECT_Y_OUTPUT_SIZE_POS_ERROR)\
	CMD(n, ISP_MESSAGE_TILE_X_DIR_NOT_END_TOGETHER_ERROR)\
	CMD(n, ISP_MESSAGE_TILE_Y_DIR_NOT_END_TOGETHER_ERROR)\
	CMD(n, ISP_MESSAGE_INCORRECT_XE_INPUT_POS_REDUCED_BY_TILE_SIZE_ERROR)\
	CMD(n, ISP_MESSAGE_INCORRECT_YE_INPUT_POS_REDUCED_BY_TILE_SIZE_ERROR)\
	CMD(n, ISP_MESSAGE_FORWARD_FUNC_CAL_LOOP_COUNT_OVER_MAX_ERROR)\
	CMD(n, ISP_MESSAGE_TILE_LOSS_OVER_TILE_HEIGHT_ERROR)\
	CMD(n, ISP_MESSAGE_TILE_LOSS_OVER_TILE_WIDTH_ERROR)\
	CMD(n, ISP_MESSAGE_TP6_FOR_INVALID_OUT_XYS_XYE_ERROR)\
	CMD(n, ISP_MESSAGE_TP4_FOR_INVALID_OUT_XYS_XYE_ERROR)\
	CMD(n, ISP_MESSAGE_SRC_ACC_FOR_INVALID_OUT_XYS_XYE_ERROR)\
	CMD(n, ISP_MESSAGE_CUB_ACC_FOR_INVALID_OUT_XYS_XYE_ERROR)\
	CMD(n, ISP_MESSAGE_RECURSIVE_FOUND_ERROR)\
	CMD(n, ISP_MESSAGE_CHECK_IN_CONFIG_ALIGN_XS_POS_ERROR)\
	CMD(n, ISP_MESSAGE_CHECK_IN_CONFIG_ALIGN_XE_POS_ERROR)\
	CMD(n, ISP_MESSAGE_CHECK_IN_CONFIG_ALIGN_YS_POS_ERROR)\
	CMD(n, ISP_MESSAGE_CHECK_IN_CONFIG_ALIGN_YE_POS_ERROR)\
	CMD(n, ISP_MESSAGE_XSIZE_NOT_DIV_BY_IN_CONST_X_ERROR)\
	CMD(n, ISP_MESSAGE_YSIZE_NOT_DIV_BY_IN_CONST_Y_ERROR)\
	CMD(n, ISP_MESSAGE_XSIZE_NOT_DIV_BY_OUT_CONST_X_ERROR)\
	CMD(n, ISP_MESSAGE_YSIZE_NOT_DIV_BY_OUT_CONST_Y_ERROR)\
	CMD(n, ISP_MESSAGE_TILE_FORWARD_OUT_OVER_TILE_WIDTH_ERROR)\
	CMD(n, ISP_MESSAGE_TILE_FORWARD_OUT_OVER_TILE_HEIGHT_ERROR)\
	CMD(n, ISP_MESSAGE_TILE_BACKWARD_IN_OVER_TILE_WIDTH_ERROR)\
	CMD(n, ISP_MESSAGE_TILE_BACKWARD_IN_OVER_TILE_HEIGHT_ERROR)\
	CMD(n, ISP_MESSAGE_FORWARD_CHECK_TOP_EDGE_ERROR)\
	CMD(n, ISP_MESSAGE_FORWARD_CHECK_BOTTOM_EDGE_ERROR)\
	CMD(n, ISP_MESSAGE_FORWARD_CHECK_LEFT_EDGE_ERROR)\
	CMD(n, ISP_MESSAGE_FORWARD_CHECK_RIGHT_EDGE_ERROR)\
	CMD(n, ISP_MESSAGE_BACKWARD_CHECK_TOP_EDGE_ERROR)\
	CMD(n, ISP_MESSAGE_BACKWARD_CHECK_BOTTOM_EDGE_ERROR)\
	CMD(n, ISP_MESSAGE_BACKWARD_CHECK_LEFT_EDGE_ERROR)\
	CMD(n, ISP_MESSAGE_BACKWARD_CHECK_RIGHT_EDGE_ERROR)\
	CMD(n, ISP_MESSAGE_CHECK_OUT_CONFIG_ALIGN_XS_POS_ERROR)\
	CMD(n, ISP_MESSAGE_CHECK_OUT_CONFIG_ALIGN_XE_POS_ERROR)\
	CMD(n, ISP_MESSAGE_CHECK_OUT_CONFIG_ALIGN_YS_POS_ERROR)\
	CMD(n, ISP_MESSAGE_CHECK_OUT_CONFIG_ALIGN_YE_POS_ERROR)\
	CMD(n, ISP_MESSAGE_DISABLE_FUNC_X_SIZE_CHECK_ERROR)\
	CMD(n, ISP_MESSAGE_DISABLE_FUNC_Y_SIZE_CHECK_ERROR)\
	CMD(n, ISP_MESSAGE_OUTPUT_DISABLE_INPUT_FUNC_CHECK_ERROR)\
	CMD(n, ISP_MESSAGE_RESIZER_SRC_ACC_SCALING_UP_ERROR)\
	CMD(n, ISP_MESSAGE_RESIZER_CUBIC_ACC_SCALING_UP_ERROR)\
	CMD(n, ISP_MESSAGE_INCORRECT_END_FUNC_TYPE_ERROR)\
	CMD(n, ISP_MESSAGE_INCORRECT_START_FUNC_TYPE_ERROR)\
	/* tdr sort check */\
	CMD(n, ISP_MESSAGE_INCORRECT_ORDER_CONFIG_ERROR)\
	/* multi-input flow error check */\
	CMD(n, ISP_MESSAGE_TWO_MAIN_PREV_ERROR)\
	CMD(n, ISP_MESSAGE_TWO_START_PREV_ERROR)\
	CMD(n, ISP_MESSAGE_NO_MAIN_OUTPUT_ERROR)\
	CMD(n, ISP_MESSAGE_DIFF_PREV_CONFIG_ERROR)\
	CMD(n, ISP_MESSAGE_DIFF_NEXT_CONFIG_ERROR)\
	CMD(n, ISP_MESSAGE_TWO_MAIN_START_ERROR)\
	CMD(n, ISP_MESSAGE_DIFF_PREV_FORWARD_ERROR)\
	CMD(n, ISP_MESSAGE_FOR_BACK_COMP_X_ERROR)\
	CMD(n, ISP_MESSAGE_FOR_BACK_COMP_Y_ERROR)\
	CMD(n, ISP_MESSAGE_BROKEN_SUB_PATH_ERROR)\
	CMD(n, ISP_MESSAGE_MIX_SUB_IN_OUT_PATH_ERROR)\
	CMD(n, ISP_MESSAGE_INVALID_SUB_IN_CONFIG_ERROR)\
	CMD(n, ISP_MESSAGE_INVALID_SUB_OUT_CONFIG_ERROR)\
	CMD(n, ISP_MESSAGE_UNKNOWN_RUN_MODE_ERROR)\
	/* diff view check */\
	CMD(n, ISP_MESSAGE_DIFF_VIEW_BRANCH_MERGE_ERROR)\
	CMD(n, ISP_MESSAGE_DIFF_VIEW_INPUT_ERROR)\
	CMD(n, ISP_MESSAGE_DIFF_VIEW_OUTPUT_ERROR)\
	CMD(n, ISP_MESSAGE_FRAME_MODE_NOT_END_ERROR)\
	CMD(n, ISP_MESSAGE_DIFF_VIEW_TILE_WIDTH_ERROR)\
	CMD(n, ISP_MESSAGE_DIFF_VIEW_TILE_HEIGHT_ERROR)\
	/* min size constraints */\
	CMD(n, ISP_MESSAGE_UNDER_MIN_XSIZE_ERROR)\
	CMD(n, ISP_MESSAGE_UNDER_MIN_YSIZE_ERROR)\
	/* MDP ERROR MESSAGE DATA */\
	MDP_ERROR_MESSAGE_ENUM(n, CMD)\
	/* final count, can not be changed */\
	CMD(n, ISP_MESSAGE_TILE_MAX_NO)

/* error enum */
#define MDP_ERROR_MESSAGE_ENUM(n, CMD) \
	/* PRZ check */\
	CMD(n, MDP_MESSAGE_RESIZER_SCALING_ERROR)\
	/* TDSHP check */\
	CMD(n, MDP_MESSAGE_TDSHP_BACK_LT_FORWARD)\
	/* WROT check */\
	CMD(n, MDP_MESSAGE_WROT_INVALID_FORMAT)\
	/* General status */\
	CMD(n, MDP_MESSAGE_NULL_DATA)\
	CMD(n, MDP_MESSAGE_INVALID_STATE)\
	CMD(n, MDP_MESSAGE_UNKNOWN_ERROR)

#define ISP_ENUM_DECLARE(n, a) a,
#define ISP_ENUM_STRING_CASE(n, a) \
	case (a): \
		(n) = #a; \
		break;

/* error enum */
typedef enum isp_tile_message {
	ISP_TILE_MESSAGE_UNKNOWN = 0,
	ERROR_MESSAGE_DATA(n, ISP_ENUM_DECLARE)
} ISP_TILE_MESSAGE_ENUM;

#define GET_ERROR_NAME(name, err) \
	switch (err) { \
	case ISP_TILE_MESSAGE_UNKNOWN: \
		(name) = "ISP_MESSAGE_UNKNOWN"; \
		break; \
	ERROR_MESSAGE_DATA(name, ISP_ENUM_STRING_CASE) \
	default: \
		(name) = ""; \
	}

/* a, b, c, d, e reserved
 * data type
 * register name of current c model
 * reserved
 * value mask
 * array bracket []
 * S: c model variables, U: unmasked variable, M: masked variable
 * be careful with init, must items to reset by TILE_MODULE_CHECK macro
 * output_disable = false function to reset by tile_init_config()
 */
#define TILE_FUNC_BLOCK_LUT(CMD, a, b, c, d, e) \
    CMD(a, b, c, d, e, int, func_num, , ,, S, ,)\
    CMD(a, b, c, d, e, char, func_name, ,, [MAX_TILE_FUNC_NAME_SIZE], S, ,)\
    CMD(a, b, c, d, e, TILE_RUN_MODE_ENUM, run_mode, , ,, S, ,)\
    CMD(a, b, c, d, e, bool, enable_flag, , ,, S, ,)\
    CMD(a, b, c, d, e, bool, output_disable_flag, , ,, S, ,)\
    CMD(a, b, c, d, e, unsigned char, tdr_edge, , TILE_EDGE_HORZ_MASK, , M, ,)/* to reset */\
    CMD(a, b, c, d, e, unsigned char, tot_branch_num, , ,, S, ,)/* to reset */\
    CMD(a, b, c, d, e, unsigned char, next_blk_num, ,, [MAX_TILE_BRANCH_NO], S, ,)\
    CMD(a, b, c, d, e, unsigned char, tot_prev_num, , ,, S, ,)/* to reset */\
    CMD(a, b, c, d, e, unsigned char, prev_blk_num, ,, [MAX_TILE_PREV_NO], S, ,)\
    CMD(a, b, c, d, e, bool, tdr_h_disable_flag, , ,, U, ,)/* diff view cal with backup, to reset */\
    CMD(a, b, c, d, e, bool, h_end_flag, , ,, U, ,)/* backup */\
    CMD(a, b, c, d, e, bool, crop_h_end_flag, , ,, S, ,)\
    CMD(a, b, c, d, e, int, in_pos_xs, , ,, U, ,)\
    CMD(a, b, c, d, e, int, in_pos_xe, , ,, U, ,)\
    CMD(a, b, c, d, e, int, full_size_x_in, , ,, S, ,)\
    CMD(a, b, c, d, e, int, in_tile_width, , ,, S, ,)\
    CMD(a, b, c, d, e, int, in_tile_width_max, , ,, S, ,)/* backward boundary */\
    CMD(a, b, c, d, e, int, in_tile_width_max_str, , ,, S, ,)\
    CMD(a, b, c, d, e, int, in_tile_width_max_end, , ,, S, ,)\
    CMD(a, b, c, d, e, int, in_tile_width_loss, , ,, S, ,)/* backward boundary for ufd smt */\
    CMD(a, b, c, d, e, int, in_max_width, , ,, S, ,)\
    CMD(a, b, c, d, e, int, in_min_width, , ,, S, ,)\
    CMD(a, b, c, d, e, int, in_log_width, , ,, S, ,)\
    CMD(a, b, c, d, e, int, out_pos_xs, , ,, U, ,)\
    CMD(a, b, c, d, e, int, out_pos_xe, , ,, U, ,)\
    CMD(a, b, c, d, e, int, full_size_x_out, , ,, S, ,)/* to reset */\
    CMD(a, b, c, d, e, int, out_tile_width, , ,, S, ,)\
    CMD(a, b, c, d, e, int, out_tile_width_max, , ,, S, ,)/* backward boundary */\
    CMD(a, b, c, d, e, int, out_tile_width_max_str, , ,, S, ,)\
    CMD(a, b, c, d, e, int, out_tile_width_max_end, , ,, S, ,)\
    CMD(a, b, c, d, e, int, out_tile_width_loss, , ,, S, ,)/* backward boundary for ufd smt */\
    CMD(a, b, c, d, e, int, out_max_width, , ,, S, ,)\
    CMD(a, b, c, d, e, int, out_log_width, , ,, S, ,)\
    CMD(a, b, c, d, e, bool, max_h_edge_flag, , ,, S, ,)\
    CMD(a, b, c, d, e, unsigned char, in_const_x, , ,, S, ,)\
    CMD(a, b, c, d, e, unsigned char, out_const_x, , ,, S, ,)\
    CMD(a, b, c, d, e, bool, tdr_v_disable_flag, , ,, S, ,)/* diff view cal, to reset */\
    CMD(a, b, c, d, e, bool, v_end_flag, , ,, S, ,)\
    CMD(a, b, c, d, e, bool, crop_v_end_flag, , ,, S, ,)\
    CMD(a, b, c, d, e, int, in_pos_ys, , ,, S, ,)\
    CMD(a, b, c, d, e, int, in_pos_ye, , ,, S, ,)\
    CMD(a, b, c, d, e, int, full_size_y_in, , ,, S, ,)/* to reset */\
    CMD(a, b, c, d, e, int, in_tile_height, , ,, S, ,)\
    CMD(a, b, c, d, e, int, in_tile_height_max, , ,, S, ,)/* backward boundary */\
    CMD(a, b, c, d, e, int, in_tile_height_max_str, , ,, S, ,)\
    CMD(a, b, c, d, e, int, in_tile_height_max_end, , ,, S, ,)\
    CMD(a, b, c, d, e, int, in_max_height, , ,, S, ,)\
    CMD(a, b, c, d, e, int, in_min_height, , ,, S, ,)\
    CMD(a, b, c, d, e, int, in_log_height, , ,, S, ,)\
    CMD(a, b, c, d, e, int, out_pos_ys, , ,, S, ,)\
    CMD(a, b, c, d, e, int, out_pos_ye, , ,, S, ,)\
    CMD(a, b, c, d, e, int, full_size_y_out, , ,, S, ,)\
    CMD(a, b, c, d, e, int, out_tile_height, , ,, S, ,)\
    CMD(a, b, c, d, e, int, out_tile_height_max, , ,, S, ,)/* backward boundary */\
    CMD(a, b, c, d, e, int, out_tile_height_max_str, , ,, S, ,)\
    CMD(a, b, c, d, e, int, out_tile_height_max_end, , ,, S, ,)\
    CMD(a, b, c, d, e, int, out_max_height, , ,, S, ,)\
    CMD(a, b, c, d, e, int, out_log_height, , ,, S, ,)\
    CMD(a, b, c, d, e, bool, max_v_edge_flag, , ,, S, ,)\
    CMD(a, b, c, d, e, unsigned char, in_const_y, , ,, S, ,)\
    CMD(a, b, c, d, e, unsigned char, out_const_y, , ,, S, ,)\
    CMD(a, b, c, d, e, int, min_in_pos_xs, , ,, S, ,)\
    CMD(a, b, c, d, e, int, max_in_pos_xe, , ,, S, ,)\
    CMD(a, b, c, d, e, int, min_out_pos_xs, , ,, S, ,)\
    CMD(a, b, c, d, e, int, max_out_pos_xe, , ,, S, ,)\
    CMD(a, b, c, d, e, int, min_in_crop_xs, , ,, S, ,)/* backward boundary for ufd smt */\
    CMD(a, b, c, d, e, int, max_in_crop_xe, , ,, S, ,)/* backward boundary for ufd smt */\
    CMD(a, b, c, d, e, int, min_out_crop_xs, , ,, S, ,)/* backward boundary for ufd smt */\
    CMD(a, b, c, d, e, int, max_out_crop_xe, , ,, S, ,)/* backward boundary for ufd smt */\
    CMD(a, b, c, d, e, int, min_in_pos_ys, , ,, S, ,)\
    CMD(a, b, c, d, e, int, max_in_pos_ye, , ,, S, ,)\
    CMD(a, b, c, d, e, int, min_out_pos_ys, , ,, S, ,)\
    CMD(a, b, c, d, e, int, max_out_pos_ye, , ,, S, ,)\
    CMD(a, b, c, d, e, int, valid_h_no, , ,, S, ,)/* diff view cal, to reset */\
    CMD(a, b, c, d, e, int, valid_v_no, , ,, S, ,)/* diff view cal, to reset */\
    CMD(a, b, c, d, e, int, last_valid_tile_no, , ,, S, ,)/* diff view cal, to reset */\
    CMD(a, b, c, d, e, int, last_valid_v_no, , ,, S, ,)/* diff view cal, to reset */\
    CMD(a, b, c, d, e, int, bias_x, , ,, U, ,)\
    CMD(a, b, c, d, e, int, offset_x, , ,, U, ,)\
    CMD(a, b, c, d, e, int, bias_x_c, , ,, U, ,)\
    CMD(a, b, c, d, e, int, offset_x_c, , ,, U, ,)\
    CMD(a, b, c, d, e, int, bias_y, , ,, S, ,)\
    CMD(a, b, c, d, e, int, offset_y, , ,, S, ,)\
    CMD(a, b, c, d, e, int, bias_y_c, , ,, S, ,)\
    CMD(a, b, c, d, e, int, offset_y_c, , ,, S, ,)\
    CMD(a, b, c, d, e, unsigned char, l_tile_loss, , ,, S, ,)\
    CMD(a, b, c, d, e, unsigned char, r_tile_loss, , ,, S, ,)\
    CMD(a, b, c, d, e, unsigned char, t_tile_loss, , ,, S, ,)\
    CMD(a, b, c, d, e, unsigned char, b_tile_loss, , ,, S, ,)\
    CMD(a, b, c, d, e, int, crop_bias_x, , ,, S, ,)\
    CMD(a, b, c, d, e, int, crop_offset_x, , ,, S, ,)\
    CMD(a, b, c, d, e, int, crop_bias_y, , ,, S, ,)\
    CMD(a, b, c, d, e, int, crop_offset_y, , ,, S, ,)\
    CMD(a, b, c, d, e, int, backward_input_xs_pos, , ,, S, ,)\
    CMD(a, b, c, d, e, int, backward_input_xe_pos, , ,, S, ,)\
    CMD(a, b, c, d, e, int, backward_output_xs_pos, , ,, S, ,)\
    CMD(a, b, c, d, e, int, backward_output_xe_pos, , ,, S, ,)\
    CMD(a, b, c, d, e, int, backward_input_ys_pos, , ,, S, ,)\
    CMD(a, b, c, d, e, int, backward_input_ye_pos, , ,, S, ,)\
    CMD(a, b, c, d, e, int, backward_output_ys_pos, , ,, S, ,)\
    CMD(a, b, c, d, e, int, backward_output_ye_pos, , ,, S, ,)\
    CMD(a, b, c, d, e, int, last_input_xs_pos, , ,, S, ,)\
    CMD(a, b, c, d, e, int, last_input_xe_pos, , ,, S, ,)\
    CMD(a, b, c, d, e, int, last_output_xs_pos, , ,, S, ,)\
    CMD(a, b, c, d, e, int, last_output_xe_pos, , ,, S, ,)\
    CMD(a, b, c, d, e, int, last_input_ys_pos, , ,, S, ,)\
    CMD(a, b, c, d, e, int, last_input_ye_pos, , ,, S, ,)\
    CMD(a, b, c, d, e, int, last_output_ys_pos, , ,, S, ,)\
    CMD(a, b, c, d, e, int, last_output_ye_pos, , ,, S, ,)\
    CMD(a, b, c, d, e, unsigned char, type, , ,, S, ,)\
    CMD(a, b, c, d, e, unsigned int, in_stream_order, , ,, S, ,)\
    CMD(a, b, c, d, e, unsigned int, out_stream_order, , ,, S, ,)\
    CMD(a, b, c, d, e, unsigned int, in_cal_order, , ,, S, ,)\
    CMD(a, b, c, d, e, unsigned int, out_cal_order, , ,, S, ,)\
    CMD(a, b, c, d, e, unsigned int, in_dump_order, , ,, S, ,)\
    CMD(a, b, c, d, e, unsigned int, out_dump_order, , ,, S, ,)\
    CMD(a, b, c, d, e, int, min_tile_in_pos_xs, , ,, S, ,)/* diff view cal */\
    CMD(a, b, c, d, e, int, min_tile_in_pos_xe, , ,, S, ,)/* diff view cal */\
    CMD(a, b, c, d, e, int, min_tile_out_pos_xs, , ,, S, ,)/* diff view cal */\
    CMD(a, b, c, d, e, int, min_tile_out_pos_xe, , ,, S, ,)/* diff view cal */\
    CMD(a, b, c, d, e, int, min_tile_crop_in_pos_xs, , ,, S, ,)/* diff view cal */\
    CMD(a, b, c, d, e, int, min_tile_crop_in_pos_xe, , ,, S, ,)/* diff view cal */\
    CMD(a, b, c, d, e, int, min_tile_crop_out_pos_xs, , ,, S, ,)/* diff view cal */\
    CMD(a, b, c, d, e, int, min_tile_crop_out_pos_xe, , ,, S, ,)/* diff view cal */\
    CMD(a, b, c, d, e, int, min_tile_in_pos_ys, , ,, S, ,)/* diff view cal */\
    CMD(a, b, c, d, e, int, min_tile_in_pos_ye, , ,, S, ,)/* diff view cal */\
    CMD(a, b, c, d, e, int, min_tile_out_pos_ys, , ,, S, ,)/* diff view cal */\
    CMD(a, b, c, d, e, int, min_tile_out_pos_ye, , ,, S, ,)/* diff view cal */\
    CMD(a, b, c, d, e, bool, min_cal_tdr_h_disable_flag, , ,, S, ,)/* diff view cal */\
    CMD(a, b, c, d, e, bool, min_cal_h_end_flag, , ,, S, ,)/* diff view log */\
    CMD(a, b, c, d, e, bool, min_cal_max_h_edge_flag, , ,, S, ,)/* diff view log */\
    CMD(a, b, c, d, e, bool, min_cal_tdr_v_disable_flag, , ,, S, ,)\
    CMD(a, b, c, d, e, bool, min_cal_v_end_flag, , ,, S, ,)/* diff view log */\
    CMD(a, b, c, d, e, bool, min_cal_max_v_edge_flag, , ,, S, ,)/* diff view log */\
    CMD(a, b, c, d, e, bool, direct_h_end_flag, , ,, S, ,)/* DL interface */\
    CMD(a, b, c, d, e, bool, direct_v_end_flag, , ,, S, ,)/* DL interface */\
    CMD(a, b, c, d, e, int, direct_out_pos_xs, , ,, S, ,)/* DL interface */\
    CMD(a, b, c, d, e, int, direct_out_pos_xe, , ,, S, ,)/* DL interface */\
    CMD(a, b, c, d, e, int, direct_out_pos_ys, , ,, S, ,)/* DL interface */\
    CMD(a, b, c, d, e, int, direct_out_pos_ye, , ,, S, ,)/* DL interface */\
    CMD(a, b, c, d, e, bool, backward_tdr_h_disable_flag, , ,, U, ,)/* diff view tdr used with backup */\
    CMD(a, b, c, d, e, bool, backward_tdr_v_disable_flag, , ,, S, ,)\
    CMD(a, b, c, d, e, bool, backward_h_end_flag, , ,, U, ,)/* diff view tdr used with backup */\
    CMD(a, b, c, d, e, bool, backward_v_end_flag, , ,, S, ,)\
    CMD(a, b, c, d, e, int, in_tile_width_backup, , ,, S, ,)\
    CMD(a, b, c, d, e, int, out_tile_width_backup, , ,, S, ,)\
    CMD(a, b, c, d, e, int, in_tile_height_backup, , ,, S, ,)\
    CMD(a, b, c, d, e, int, out_tile_height_backup, , ,, S, ,)\
    CMD(a, b, c, d, e, int, min_last_input_xs_pos, , ,, S, ,)\
    CMD(a, b, c, d, e, int, max_last_input_xe_pos, , ,, S, ,)\
    CMD(a, b, c, d, e, int, max_last_input_ye_pos, , ,, S, ,)\
    CMD(a, b, c, d, e, int, last_func_num, ,, [MAX_TILE_PREV_NO], S, ,)\
    CMD(a, b, c, d, e, int, next_func_num, ,, [MAX_TILE_BRANCH_NO], S, ,)\
    CMD(a, b, c, d, e, TILE_HORZ_BACKUP_BUFFER, horz_para, ,, [MAX_TILE_BACKUP_HORZ_NO], S, ,)

/* register table for tile driver only parameters
 * data type
 * internal variable name of tile
 * array bracket [xx]
 * direct-link param 0: must be equal, 1: replaced by MDP, 2: don't care
 */
#define COMMON_TILE_INTERNAL_REG_LUT(CMD) \
	/* Internal */\
	CMD(int, skip_x_cal, , 2)\
	CMD(int, skip_y_cal, , 2)\
	CMD(int, backup_x_skip_y, , 2)\
	/* tdr_control_en */\
	CMD(int, tdr_ctrl_en, , 1)\
	/* run mode */\
	CMD(int, run_mode, , 2)\
	/* frame mode flag */\
	CMD(int, first_frame, , 2)/* first frame to run frame mode */\
	/* vertical_tile_no */\
	CMD(int, curr_vertical_tile_no, , 2)\
	/* horizontal_tile_no */\
	CMD(int, horizontal_tile_no, , 2)\
	/* curr_horizontal_tile_no */\
	CMD(int, curr_horizontal_tile_no, , 2)\
	/* used_tile_no */\
	CMD(int, used_tile_no, , 2)\
	CMD(int, valid_tile_no, , 2)\
	/* tile cal & dump order flag */\
	CMD(unsigned int, src_stream_order, , 0)/* keep isp src_stream_order */\
	CMD(unsigned int, src_cal_order, , 1)/* copy RDMA in_cal_order */\
	CMD(unsigned int, src_dump_order, , 1)/* copy RDMA in_dump_order */\
	/* sub mode */\
	CMD(int, found_sub_in, , 2)\
	CMD(int, found_sub_out, , 2)\
	/* frame mode flag */\
	CMD(int, first_pass, , 2)/* first pass to run min edge & min tile cal */\
	/* first func no */\
	CMD(int, first_func_en_no, , 2)\
	/* last func no */\
	CMD(int, last_func_en_no, , 2)\
	/* debug mode with invalid offset to enable recursive forward*/\
	CMD(int, recursive_forward_en, , 2)

/* a, b c, d, e reserved
 * function id
 * function name
 * tile type: 0x1 non-fixed func to configure, 0x2 rdma, 0x4 wdma, 0x8 crop_en
 * tile group, 0: ISP group, 1: CDP group 2: resizer with offset & crop
 * tile group except for 2 will restrict last end < current end (to ensure WDMA end at same time)
 * tile loss, l_loss, r_loss, t_loss, b_loss, in_x, int_y, out_x, out_y
 * init function name, default NULL
 * forward function name, default NULL
 * back function name, default NULL
 * calculate tile reg function name, default NULL
 * input tile constraint, 0: no check, 1: to clip when enabled
 * output tile constraint, 0: no check, 1: to clip when enabled
 */
#define TILE_TYPE_LOSS (0x1)/* post process by c model */
#define TILE_TYPE_RDMA (0x2)
#define TILE_TYPE_WDMA (0x4)
#define TILE_TYPE_CROP_EN (0x8)
#define TILE_TYPE_DONT_CARE_END (0x10) /* used by dpframework & sub_out*/

#define TILE_WRAPPER_DATA_TYPE_DECLARE(a, b, c, d, e, f, g, h, i, j, k, m, n, ...) f g j;
#define TILE_HW_REG_TYPE_DECLARE(a, b, c, ...) a b c;

#define TILE_WRAPPER_HORZ_PARA_DECLARE(a, b, c, d, e, f, g, h, i, j, k, m, n, ...) TILE_WRAPPER_HORZ_PARA_DECLARE_##k(f, g, j)
#define TILE_WRAPPER_HORZ_PARA_DECLARE_S(f, g, j)
#define TILE_WRAPPER_HORZ_PARA_DECLARE_U(f, g, j) f g j;
#define TILE_WRAPPER_HORZ_PARA_DECLARE_M(f, g, j) TILE_WRAPPER_HORZ_PARA_DECLARE_U(f, g, j)
#define TILE_WRAPPER_HORZ_PARA_BACKUP(a, b, c, d, e, f, g, h, i, j, k, m, n, ...) TILE_WRAPPER_HORZ_PARA_BACKUP_##k(a, b, g);
#define TILE_WRAPPER_HORZ_PARA_BACKUP_S(a, b, g)
#define TILE_WRAPPER_HORZ_PARA_BACKUP_U(a, b, g) (a)->g = (b)->g;
#define TILE_WRAPPER_HORZ_PARA_BACKUP_M(a, b, g)TILE_WRAPPER_HORZ_PARA_BACKUP_U(a, b, g)
#define TILE_WRAPPER_HORZ_PARA_RESTORE(a, b, c, d, e, f, g, h, i, j, k, m, n, ...) TILE_WRAPPER_HORZ_PARA_RESTORE_##k(a, b, g, i);
#define TILE_WRAPPER_HORZ_PARA_RESTORE_S(a, b, g, i)
#define TILE_WRAPPER_HORZ_PARA_RESTORE_U(a, b, g, i) (b)->g = (a)->g;
#define TILE_WRAPPER_HORZ_PARA_RESTORE_M(a, b, g, i) (b)->g = (((b)->g) & (~(i) & 0xF)) | (((a)->g) & (i));

#define TILE_COPY_SRC_ORDER(a, b) \
    (a)->in_stream_order = (b)->src_stream_order;\
    (a)->in_cal_order = (b)->src_cal_order;\
    (a)->in_dump_order = (b)->src_dump_order;\
    (a)->out_stream_order = (b)->src_stream_order;\
    (a)->out_cal_order = (b)->src_cal_order;\
    (a)->out_dump_order = (b)->src_dump_order;

#define TILE_COPY_PRE_ORDER(a, b) \
    (a)->in_stream_order = (b)->out_stream_order;\
    (a)->in_cal_order = (b)->out_cal_order;\
    (a)->in_dump_order = (b)->out_dump_order;\
    (a)->out_stream_order = (b)->out_stream_order;\
    (a)->out_cal_order = (b)->out_cal_order;\
    (a)->out_dump_order = (b)->out_dump_order;

typedef struct TILE_HORZ_BACKUP_BUFFER {
    TILE_FUNC_BLOCK_LUT(TILE_WRAPPER_HORZ_PARA_DECLARE, , , , ,)
} TILE_HORZ_BACKUP_BUFFER;

/* tile reg & variable */
typedef struct tile_reg_map {
	/* COMMON */
	COMMON_TILE_INTERNAL_REG_LUT(TILE_HW_REG_TYPE_DECLARE)
} TILE_REG_MAP_STRUCT;

/* self reference type */
typedef struct tile_func_block {
	TILE_FUNC_BLOCK_LUT(TILE_WRAPPER_DATA_TYPE_DECLARE, , , , ,)
	enum isp_tile_message (*init_func_ptr)(struct tile_func_block *ptr_func, struct tile_reg_map *ptr_tile_reg_map);
	enum isp_tile_message (*for_func_ptr)(struct tile_func_block *ptr_func, struct tile_reg_map *ptr_tile_reg_map);
	enum isp_tile_message (*back_func_ptr)(struct tile_func_block *ptr_func, struct tile_reg_map *ptr_tile_reg_map);
	union mml_tile_data *func_data;
} TILE_FUNC_BLOCK_STRUCT;

/* tile function interface to be compatiable with new c model */
typedef struct func_description {
	unsigned char used_func_no;
	unsigned int valid_flag[(MAX_TILE_FUNC_NO + 31) / 32];
	unsigned int for_recursive_count;
	unsigned char scheduling_forward_order[MAX_TILE_FUNC_NO];
	unsigned char scheduling_backward_order[MAX_TILE_FUNC_NO];
	struct tile_func_block *func_list[MAX_TILE_FUNC_NO];
} FUNC_DESCRIPTION_STRUCT;

#endif
