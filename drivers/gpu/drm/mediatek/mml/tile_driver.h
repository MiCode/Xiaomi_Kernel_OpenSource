/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __TILE_DRIVER_H__
#define __TILE_DRIVER_H__

#include "tile_mdp_reg.h"/* must */
#include <dt-bindings/mml/mml-mt6893.h>

#include "mtk-mml-core.h"
#define tile_driver_printf(fmt, ...) mml_err("[%s][%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)

#define MAX_TILE_HEIGHT_HW (65536)
#define MAX_TILE_PREV_NO (10)
#define MAX_TILE_BRANCH_NO (6)
#define MIN_TILE_FUNC_NO (2)
#define MAX_TILE_FUNC_NO (128) /* smaller or equal to (PREVIOUS_BLK_NO_OF_START-1) */
#define MAX_INPUT_TILE_FUNC_NO (32)
#define MAX_FORWARD_FUNC_CAL_LOOP_NO (16 * MAX_TILE_FUNC_NO)
#define MAX_TILE_FUNC_EN_NO (192)
#define MAX_TILE_FUNC_NAME_SIZE (32)
#define MAX_TILE_TOT_NO (1200)

#define TILE_MOD(num, denom) (((denom) == 1) ? 0 : (((denom) == 2) ? ((num) & 0x1) : (((denom) == 4) ? \
	((num) & 0x3) : (((denom) == 8) ? ((num) & 0x7) : ((num) % (denom))))))
#define TILE_INT_DIV(num, denom) (((denom) == 1) ? (num) : (((denom) == 2) ? ((unsigned int)(num) >> 0x1) : \
	(((denom) == 4) ? ((unsigned int)(num) >> 0x2) : (((denom) == 8) ? ((unsigned int)(num) >> 0x3) : ((num) / (denom))))))\

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
    CMD(n, ISP_MESSAGE_TILE_OK, ISP_TPIPE_MESSAGE_OK)\
    CMD(n, ISP_MESSAGE_TILE_NULL_PTR_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_UNKNOWN_DRIVER_CONFIGURED_REG_MODE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_DIFFERENT_TILE_CONFIG_NO_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_OVER_MAX_MASK_WORD_NO_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_OVER_MAX_TILE_WORD_NO_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_OVER_MAX_TILE_TOT_NO_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_UNDER_MIN_TILE_FUNC_NO_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_NOT_FOUND_INIT_TILE_PROPERTY_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_NOT_FOUND_ENABLE_TILE_FUNC_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_NOT_FOUND_SUB_RDMA_TDR_FUNC_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_CUT_SUB_OUT_WDMA_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    /* tile dump check */\
    CMD(n, ISP_MESSAGE_INCONSISTENT_TDR_DUMP_MASK_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_DUPLICATED_TDR_DUMP_MASK_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_DUPLICATED_SUPPORT_FUNC_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_DUPLICATED_SUB_RDMA_FUNC_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_OVER_MAX_BRANCH_NO_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_OVER_MAX_INPUT_TILE_FUNC_NO_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TILE_FUNC_CANNOT_FIND_LAST_FUNC_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_SCHEDULING_BACKWARD_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_SCHEDULING_FORWARD_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_IN_CONST_X_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_IN_CONST_Y_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_OUT_CONST_X_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_OUT_CONST_Y_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_NULL_INIT_PTR_FOR_START_FUNC_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_INIT_INCORRECT_X_INPUT_SIZE_POS_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_INIT_INCORRECT_Y_INPUT_SIZE_POS_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_INIT_INCORRECT_X_OUTPUT_SIZE_POS_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_INIT_INCORRECT_Y_OUTPUT_SIZE_POS_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TILE_X_DIR_NOT_END_TOGETHER_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TILE_Y_DIR_NOT_END_TOGETHER_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_INCORRECT_XE_INPUT_POS_REDUCED_BY_TILE_SIZE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_INCORRECT_YE_INPUT_POS_REDUCED_BY_TILE_SIZE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_FORWARD_FUNC_CAL_LOOP_COUNT_OVER_MAX_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TILE_LOSS_OVER_TILE_HEIGHT_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TILE_LOSS_OVER_TILE_WIDTH_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TP6_FOR_INVALID_OUT_XYS_XYE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TP4_FOR_INVALID_OUT_XYS_XYE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_SRC_ACC_FOR_INVALID_OUT_XYS_XYE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_CUB_ACC_FOR_INVALID_OUT_XYS_XYE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_BACKWARD_START_LESS_THAN_FORWARD_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_NOT_SUPPORT_RESIZER_MODE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_RECURSIVE_FOUND_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_CHECK_IN_CONFIG_ALIGN_XS_POS_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_CHECK_IN_CONFIG_ALIGN_XE_POS_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_CHECK_IN_CONFIG_ALIGN_YS_POS_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_CHECK_IN_CONFIG_ALIGN_YE_POS_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_XSIZE_NOT_DIV_BY_IN_CONST_X_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_YSIZE_NOT_DIV_BY_IN_CONST_Y_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_XSIZE_NOT_DIV_BY_OUT_CONST_X_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_YSIZE_NOT_DIV_BY_OUT_CONST_Y_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TILE_FORWARD_OUT_OVER_TILE_WIDTH_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TILE_FORWARD_OUT_OVER_TILE_HEIGHT_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TILE_BACKWARD_IN_OVER_TILE_WIDTH_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TILE_BACKWARD_IN_OVER_TILE_HEIGHT_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_FORWARD_CHECK_TOP_EDGE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_FORWARD_CHECK_BOTTOM_EDGE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_FORWARD_CHECK_LEFT_EDGE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_FORWARD_CHECK_RIGHT_EDGE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_BACKWARD_CHECK_TOP_EDGE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_BACKWARD_CHECK_BOTTOM_EDGE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_BACKWARD_CHECK_LEFT_EDGE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_BACKWARD_CHECK_RIGHT_EDGE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_CHECK_OUT_CONFIG_ALIGN_XS_POS_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_CHECK_OUT_CONFIG_ALIGN_XE_POS_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_CHECK_OUT_CONFIG_ALIGN_YS_POS_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_CHECK_OUT_CONFIG_ALIGN_YE_POS_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_UNKNOWN_RESIZER_DIR_MODE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_DISABLE_FUNC_X_SIZE_CHECK_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_DISABLE_FUNC_Y_SIZE_CHECK_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_OUTPUT_DISABLE_INPUT_FUNC_CHECK_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_RESIZER_SRC_ACC_SCALING_UP_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_RESIZER_CUBIC_ACC_SCALING_UP_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_INCORRECT_END_FUNC_TYPE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_INCORRECT_START_FUNC_TYPE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    /* verification */\
    CMD(n, ISP_MESSAGE_VERIFY_4_8_TAPES_XS_OUT_INCONSISTENCE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_VERIFY_4_8_TAPES_XE_OUT_INCONSISTENCE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_VERIFY_4_8_TAPES_YS_OUT_INCONSISTENCE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_VERIFY_4_8_TAPES_YE_OUT_INCONSISTENCE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_VERIFY_CUBIC_ACC_XS_OUT_INCONSISTENCE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_VERIFY_CUBIC_ACC_XE_OUT_INCONSISTENCE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_VERIFY_CUBIC_ACC_YS_OUT_INCONSISTENCE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_VERIFY_CUBIC_ACC_YE_OUT_INCONSISTENCE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_VERIFY_SRC_ACC_XS_OUT_INCONSISTENCE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_VERIFY_SRC_ACC_XE_OUT_INCONSISTENCE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_VERIFY_SRC_ACC_YS_OUT_INCONSISTENCE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_VERIFY_SRC_ACC_YE_OUT_INCONSISTENCE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_VERIFY_BACKWARD_XS_LESS_THAN_FORWARD_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_VERIFY_FORWARD_XE_LESS_THAN_BACKWARD_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_VERIFY_BACKWARD_YS_LESS_THAN_FORWARD_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_VERIFY_FORWARD_YE_LESS_THAN_BACKWARD_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    /* dump c model hex */\
    CMD(n, ISP_MESSAGE_TILE_CONFIG_EN_FILE_OPEN_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TILE_CONFIG_MAP_FILE_OPEN_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TILE_CONFIG_MAP_SKIP_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    /* tile mode control */\
    CMD(n, ISP_MESSAGE_TILE_MODE_OUTPUT_FILE_COPY_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TILE_MODE_OUTPUT_FILE_CMP_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TILE_MODE_OUTPUT_FILE_DEL_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    /* tile platform */\
    CMD(n, ISP_MESSAGE_TILE_PLATFORM_NULL_INPUT_CONFIG_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TILE_PLATFORM_NULL_WORKING_BUFFER_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TILE_PLATFORM_LESS_WORKING_BUFFER_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TILE_NULL_PTR_COMP_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TILE_REG_MAP_COMP_DIFF_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TILE_NULL_MEM_PTR_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_WORKING_BUFFER_PTR_NON_4_BYTES_ALGIN_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_WORKING_BUFFER_SIZE_NON_4_BYTES_ALGIN_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_INVALID_DIRECT_LINK_REG_FILE_WARNING, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_INVALID_CAL_DUMP_FILE_WARNING, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_DISABLE_C_MODEL_DIRECT_LINK_WARNING, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_ZERO_SL_HRZ_OR_VRZ_COMP_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    /* tdr inverse */\
    CMD(n, ISP_MESSAGE_TDR_INV_TILE_NO_OVER_MAX_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TDR_INV_DIFFERENT_TILE_CONFIG_NO_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TDR_INV_NULL_PTR_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    /* tile ut */\
    CMD(n, ISP_MESSAGE_TILE_UT_FILE_OPEN_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    /* tdr sort check */\
    CMD(n, ISP_MESSAGE_INCORRECT_ORDER_CONFIG_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TDR_SORT_OVER_MAX_H_TILE_NO_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TDR_SORT_NON_4_BYTES_TILE_INFO_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    /* tile driver error check */\
    CMD(n, ISP_MESSAGE_MEM_DUMP_PARSE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
	/* multi-input flow error check */\
    CMD(n, ISP_MESSAGE_TWO_MAIN_PREV_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TWO_START_PREV_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_NO_MAIN_OUTPUT_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_DIFF_PREV_CONFIG_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_DIFF_NEXT_CONFIG_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TWO_MAIN_START_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_DIFF_PREV_FORWARD_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_FOR_BACK_COMP_X_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_FOR_BACK_COMP_Y_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_BROKEN_SUB_PATH_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_MIX_SUB_IN_OUT_PATH_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_INVALID_SUB_IN_CONFIG_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_INVALID_SUB_OUT_CONFIG_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_UNKNOWN_RUN_MODE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    /* C model func id check */\
    CMD(n, ISP_MESSAGE_TILE_CMODEL_OVER_MAX_COUNT_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TILE_CMODEL_FUNC_ID_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TILE_CMODEL_FUNC_CROP_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    /* dpframework check */\
    CMD(n, ISP_MESSAGE_DPFRAMEWORK_UNKNOWN_ENUM_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_DIRECT_LINK_DIFF_TILE_NO_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_DIRECT_LINK_CHECK_RESULT_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TILE_NULL_FILE_PTR_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
	/* diff view check */\
    CMD(n, ISP_MESSAGE_DIFF_VIEW_BRANCH_MERGE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_DIFF_VIEW_INPUT_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_DIFF_VIEW_OUTPUT_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_FRAME_MODE_NOT_END_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_DIFF_VIEW_TDR_SORTING_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TDR_DISABLE_CONFIG_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_UNDER_MIN_TDR_WORD_NO_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TCM_DISABLE_NOT_SUPPORT_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_DIFF_VIEW_TILE_WIDTH_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_DIFF_VIEW_TILE_HEIGHT_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
	/* c model random gen */\
    CMD(n, ISP_MESSAGE_NULL_RAND_GEN_FUNC_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_RAND_GEN_FILE_OPEN_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_TEST_CONFIG_PARSE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_CONFIG_OVER_BUFFER_SIZE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
	/* tile sel mode */\
    CMD(n, ISP_MESSAGE_TDR_DISPATCH_CONFIG_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_INCONSISTENT_TDR_MASK_LSB_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    /* min size constraints */\
    CMD(n, ISP_MESSAGE_UNDER_MIN_XSIZE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    CMD(n, ISP_MESSAGE_UNDER_MIN_YSIZE_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
    /* MDP ERROR MESSAGE DATA */\
    MDP_TILE_ERROR_MESSAGE_ENUM(n, CMD)\
    /* final count, can not be changed */\
    CMD(n, ISP_MESSAGE_TILE_MAX_NO, ISP_TPIPE_MESSAGE_MAX_NO)

#define ISP_ENUM_DECLARE(n, a, b) a,
#define ISP_ENUM_STRING(n, a, b) if ((a) == (n)) return #a;

/* to prevent from directly calling macro */
#define GET_ERROR_NAME(n) \
    if (0 == (n)) \
		return "ISP_MESSAGE_UNKNOWN";\
    ERROR_MESSAGE_DATA(n, ISP_ENUM_STRING)\
    return "";

/* error enum */
typedef enum isp_tile_message {
	ISP_TILE_MESSAGE_UNKNOWN = 0,
	ERROR_MESSAGE_DATA(n, ISP_ENUM_DECLARE)
} ISP_TILE_MESSAGE_ENUM;

/* a, b, c, d, e reserved */
/* data type */
/* register name of current c model */
/* reserved */
/* value mask */
/* array bracket [] */
/* S: c model variables, U: unmasked variable, M: masked variable */
/* be careful with init, must items to reset by TILE_MODULE_CHECK macro */
/* output_disable = false function to reset by tile_init_config() */
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
    CMD(a, b, c, d, e, TILE_HORZ_BACKUP_BUFFER, horz_para, ,, [MAX_TILE_BACKUP_HORZ_NO], S, ,)\

/* register table ( , , tile driver) for tile driver only parameters */
/* a, b, c, d, e reserved */
/* data type */
/* register name of current c model */
/* register name of HW ISP & platform parameters */
/* internal variable name of tile */
/* array bracket [xx] */
/* valid condition by tdr_en to print platform log with must string, default: false */
/* isp_reg.h reg name */
/* isp_reg.h field name */
/* direct-link param 0: must be equal, 1: replaced by MDP, 2: don't care */
#define COMMON_TILE_INTERNAL_REG_LUT(CMD, a, b, c, d, e) \
    /* Internal */\
    CMD(a, b, c, d, e, int, ,, skip_x_cal, , , ,, 2)\
    CMD(a, b, c, d, e, int, ,, skip_y_cal, , , ,, 2)\
    CMD(a, b, c, d, e, int, ,, backup_x_skip_y, , , ,, 2)\
    /* tdr_control_en */\
    CMD(a, b, c, d, e, int, ,, tdr_ctrl_en, , , ,, 1)\
    /* run mode */\
    CMD(a, b, c, d, e, int, ,, run_mode, , , ,, 2)\
	/* frame mode flag */\
    CMD(a, b, c, d, e, int, ,, first_frame, , , ,, 2)/* first frame to run frame mode */\
    /* vertical_tile_no */\
    CMD(a, b, c, d, e, int, ,, curr_vertical_tile_no, , , ,, 2)\
    /* horizontal_tile_no */\
    CMD(a, b, c, d, e, int, ,, horizontal_tile_no, , , ,, 2)\
    /* curr_horizontal_tile_no */\
    CMD(a, b, c, d, e, int, ,, curr_horizontal_tile_no, , , ,, 2)\
    /* used_tile_no */\
    CMD(a, b, c, d, e, int, ,, used_tile_no, , , ,, 2)\
    CMD(a, b, c, d, e, int, ,, valid_tile_no, , , ,, 2)\
    /* tile cal & dump order flag */\
    CMD(a, b, c, d, e, unsigned int, ,, src_stream_order, , , ,, 0)/* keep isp src_stream_order */\
    CMD(a, b, c, d, e, unsigned int, ,, src_cal_order, , , ,, 1)/* copy RDMA in_cal_order */\
    CMD(a, b, c, d, e, unsigned int, ,, src_dump_order, , , ,, 1)/* copy RDMA in_dump_order */\
    /* skip tile mode by c model */\
    CMD(a, b, c, d, e, int, ,, skip_tile_mode, , , ,, 0)\
    /* sub mode */\
    CMD(a, b, c, d, e, int, ,, found_sub_in, , , ,, 2)\
    CMD(a, b, c, d, e, int, ,, found_sub_out, , , ,, 2)\
	/* frame mode flag */\
    CMD(a, b, c, d, e, int, ,, first_pass, , , ,, 2)/* first pass to run min edge & min tile cal */\
    /* first func no */\
    CMD(a, b, c, d, e, int, ,, first_func_en_no, , , ,, 2)\
    /* last func no */\
    CMD(a, b, c, d, e, int, ,, last_func_en_no, , , ,, 2)\
    /* debug mode with invalid offset to enable recursive forward*/\
    CMD(a, b, c, d, e, int, ,, recursive_forward_en, , , ,, 2)\
    /* max input width */\
    CMD(a, b, c, d, e, int, ,, max_input_width, , , ,, 2)\
    /* max input height */\
    CMD(a, b, c, d, e, int, ,, max_input_height, , , ,, 2)\
    /* Tile IRQ */\
    CMD(a, b, c, d, e, int, ,, CTRL_TILE_LOAD_SIZE, , , ,, 2)\
    CMD(a, b, c, d, e, int, ,, WPE_CTRL_TILE_LOAD_SIZE, , , ,, 2)\
    CMD(a, b, c, d, e, int, ,, MFB_CTRL_TILE_LOAD_SIZE, , , ,, 2)\
	CMD(a, b, c, d, e, int, ,, MSS_CTRL_TILE_LOAD_SIZE, , , ,, 2)\

#define REG_CMP_EQ(ptr, reg, val) ((val) == (ptr)->reg)
#define REG_CMP_NOT_EQ(ptr, reg, val) ((val) != (ptr)->reg)
#define REG_CMP_NONZERO(ptr, reg) ((ptr)->reg)
#define REG_CMP_LE(ptr, reg, val) ((ptr)->reg <= (val))
#define REG_CMP_GE(ptr, reg, val) ((ptr)->reg >= (val))
#define REG_CHECK_EN(ptr, reg) (1 == (ptr)->reg)
#define REG_CHECK_DISABLED(ptr, reg) (0 == (ptr)->reg)
#define REG_CMP_AND(ptr, reg, val)  ((val) == ((ptr)->reg & (val)))
#define REG_CMP_NAND(ptr, reg, val)  (!((val) == ((ptr)->reg & (val))))
#define REG_VAL(ptr, reg) ((ptr)->reg)

/* a, b c, d, e reserved */
/* function id */
/* function name */
/* tile type: 0x1 non-fixed func to configure, 0x2 rdma, 0x4 wdma, 0x8 crop_en */
/* tile group, 0: ISP group, 1: CDP group 2: resizer with offset & crop */
/* tile group except for 2 will restrict last end < current end (to ensure WDMA end at same time) */
/* tile loss, l_loss, r_loss, t_loss, b_loss, in_x, int_y, out_x, out_y */
/* init function name, default NULL */
/* forward function name, default NULL */
/* back function name, default NULL */
/* calculate tile reg function name, default NULL */
/* input tile constraint, 0: no check, 1: to clip when enabled */
/* output tile constraint, 0: no check, 1: to clip when enabled */
#define TILE_TYPE_LOSS (0x1)/* post process by c model */
#define TILE_TYPE_RDMA (0x2)
#define TILE_TYPE_WDMA (0x4)
#define TILE_TYPE_CROP_EN (0x8)
#define TILE_TYPE_DONT_CARE_END (0x10) /* used by dpframework & sub_out*/

#define TILE_WRAPPER_DATA_TYPE_DECLARE(a, b, c, d, e, f, g, h, i, j, k, m, n, ...) f g j;
#define TILE_HW_REG_TYPE_DECLARE(a, b, c, d, e, f, g, h, i, j, ...) f i j;

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

/* init tile function */
//a: ptr of current TILE_FUNC_BLOCK_STRUCT
//b: ptr of current TILE_CAL_FUNC_STRUCT
//c: found flag
//d: reserved
//e: reserved
//f: reserved
//g: func no
//h: func name
//t1~t10: tile property
//p1~p4: fun ptr
#define INIT_TILE_FUNC(a, b, c, d, e, f, mx, my, g, h, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, p1, p2, p3, p4, r1, r2, m1, m2) \
	if (false == (b)) {\
		if ((g) == (a)->func_num) {\
			enum isp_tile_message (*init_func_ptr)(struct tile_func_block *ptr_func, struct tile_reg_map *ptr_tile_reg_map) = p1;\
			enum isp_tile_message (*for_func_ptr)(struct tile_func_block *ptr_func, struct tile_reg_map *ptr_tile_reg_map) = p2;\
			enum isp_tile_message (*back_func_ptr)(struct tile_func_block *ptr_func, struct tile_reg_map *ptr_tile_reg_map) = p3;\
			(a)->run_mode = TILE_RUN_MODE_MAIN;\
			(a)->type = (t1);\
			(a)->l_tile_loss = (t3);\
			(a)->r_tile_loss = (t4);\
			(a)->t_tile_loss = (t5);\
			(a)->b_tile_loss = (t6);\
			(a)->in_const_x = (t7);\
			(a)->in_const_y = (t8);\
			(a)->out_const_x = (t9);\
			(a)->out_const_y = (t10);\
			(a)->in_min_width = (m1);\
			(a)->in_min_height = (m2);\
			(a)->init_func_ptr = init_func_ptr;\
			(a)->for_func_ptr = for_func_ptr;\
			(a)->back_func_ptr = back_func_ptr;\
			if (r1) {\
				if (e) {\
					(a)->in_tile_width = (e);\
				} else {\
					if (r1 == 1) {\
						tile_driver_printf("Error [%s] wrong initial in tile width = 1, recover to %d\r\n", #h, f);\
						(a)->in_tile_width = (f);\
					} else {\
						(a)->in_tile_width = (r1);\
					} \
				} \
				(a)->in_tile_height = MAX_TILE_HEIGHT_HW;\
			} else {\
				(a)->in_tile_width = 0;\
				(a)->in_tile_height = 0;\
			} \
			(a)->in_max_width = (mx);\
			(a)->in_max_height = (my);\
			if (r2) {\
				if (e) {\
					(a)->out_tile_width = (e);\
				} else {\
					if (r2 == 1) {\
						tile_driver_printf("Error [%s] wrong initial out tile width = 1, recover to %d\r\n", #h, f);\
						(a)->out_tile_width = (f);\
					} else {\
						(a)->out_tile_width = (r2);\
					} \
				} \
				(a)->out_tile_height = MAX_TILE_HEIGHT_HW;\
			} else {\
				(a)->out_tile_width = 0;\
				(a)->out_tile_height = 0;\
			} \
			(a)->out_max_width = (mx);\
			(a)->out_max_height = (my);\
			(a)->in_log_width = 0;\
			(a)->in_log_height = 0;\
			(a)->out_log_width = 0;\
			(a)->out_log_height = 0;\
	    (b) = true;\
	} \
    } \

#define TILE_COPY_SRC_ORDER(a, b) \
    (a)->in_stream_order = (b)->src_stream_order;\
    (a)->in_cal_order = (b)->src_cal_order;\
    (a)->in_dump_order = (b)->src_dump_order;\
    (a)->out_stream_order = (b)->src_stream_order;\
    (a)->out_cal_order = (b)->src_cal_order;\
    (a)->out_dump_order = (b)->src_dump_order;\

#define TILE_COPY_PRE_ORDER(a, b) \
    (a)->in_stream_order = (b)->out_stream_order;\
    (a)->in_cal_order = (b)->out_cal_order;\
    (a)->in_dump_order = (b)->out_dump_order;\
    (a)->out_stream_order = (b)->out_stream_order;\
    (a)->out_cal_order = (b)->out_cal_order;\
    (a)->out_dump_order = (b)->out_dump_order;\

#define TILE_CHECK_RESULT(result) \
	{\
		if (ISP_MESSAGE_TILE_OK != (result)) {\
			return result;\
		} \
	} \

typedef struct TILE_HORZ_BACKUP_BUFFER {
    TILE_FUNC_BLOCK_LUT(TILE_WRAPPER_HORZ_PARA_DECLARE, , , , ,)
} TILE_HORZ_BACKUP_BUFFER;

/* tile reg & variable */
typedef struct tile_reg_map {
	/* COMMON */
	COMMON_TILE_INTERNAL_REG_LUT(TILE_HW_REG_TYPE_DECLARE, , , , ,)
} TILE_REG_MAP_STRUCT;

/* self reference type */
typedef struct tile_func_block {
	TILE_FUNC_BLOCK_LUT(TILE_WRAPPER_DATA_TYPE_DECLARE, , , , ,)
	enum isp_tile_message (*init_func_ptr)(struct tile_func_block *ptr_func, struct tile_reg_map *ptr_tile_reg_map);
	enum isp_tile_message (*for_func_ptr)(struct tile_func_block *ptr_func, struct tile_reg_map *ptr_tile_reg_map);
	enum isp_tile_message (*back_func_ptr)(struct tile_func_block *ptr_func, struct tile_reg_map *ptr_tile_reg_map);
	union mml_tile_data *func_data;
} TILE_FUNC_BLOCK_STRUCT;

typedef struct tile_func_enable {
	int func_num;
	bool enable_flag;
	bool output_disable_flag;
} TILE_FUNC_ENABLE_STRUCT;

/* tile function interface to be compatiable with new c model */
typedef struct func_description {
	unsigned char used_func_no;
	unsigned char used_en_func_no;
	unsigned int valid_flag[(MAX_TILE_FUNC_NO + 31) / 32];
	unsigned int for_recursive_count;
	unsigned char scheduling_forward_order[MAX_TILE_FUNC_NO];
	unsigned char scheduling_backward_order[MAX_TILE_FUNC_NO];
	struct tile_func_block func_list[MAX_TILE_FUNC_NO];
	struct tile_func_enable func_en_list[MAX_TILE_FUNC_EN_NO];
} FUNC_DESCRIPTION_STRUCT;

#endif
