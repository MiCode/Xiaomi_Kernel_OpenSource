/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chris-YC Chen <chris-yc.chen@mediatek.com>
 */
#include "tile_driver.h"
#include "tile_utility.h"

/* resizer common forward & backward */
static ISP_TILE_MESSAGE_ENUM tile_back_src_acc(TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg);
static ISP_TILE_MESSAGE_ENUM tile_back_8tp(TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg);
static ISP_TILE_MESSAGE_ENUM tile_back_6tp(TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg);
static ISP_TILE_MESSAGE_ENUM tile_back_4tp(TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg);
static ISP_TILE_MESSAGE_ENUM tile_back_5tp(TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg);
static ISP_TILE_MESSAGE_ENUM tile_back_2tp(TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg);
static ISP_TILE_MESSAGE_ENUM tile_back_mdp_6tp(TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg);
static ISP_TILE_MESSAGE_ENUM tile_back_cub_acc(TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg);
static ISP_TILE_MESSAGE_ENUM tile_back_mdp_cub_acc(TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg);
static ISP_TILE_MESSAGE_ENUM tile_for_src_acc(TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg);
static ISP_TILE_MESSAGE_ENUM tile_for_8tp(TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg);
static ISP_TILE_MESSAGE_ENUM tile_for_6tp(TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg);
static ISP_TILE_MESSAGE_ENUM tile_for_4tp(TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg);
static ISP_TILE_MESSAGE_ENUM tile_for_5tp(TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg);
static ISP_TILE_MESSAGE_ENUM tile_for_2tp(TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg);
static ISP_TILE_MESSAGE_ENUM tile_for_mdp_6tp(TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg);
static ISP_TILE_MESSAGE_ENUM tile_for_cub_acc(TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg);
static ISP_TILE_MESSAGE_ENUM tile_for_mdp_cub_acc(TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg);
static ISP_TILE_MESSAGE_ENUM tile_for_resizer_init(TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg,
                                              TILE_FUNC_BLOCK_STRUCT *ptr_func);
static ISP_TILE_MESSAGE_ENUM tile_for_resizer_update_result(TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg,
                                              TILE_FUNC_BLOCK_STRUCT *ptr_func,
											  TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg);
static ISP_TILE_MESSAGE_ENUM tile_for_switch_resizer(TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg);
static ISP_TILE_MESSAGE_ENUM tile_back_switch_resizer(TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg);
static ISP_TILE_MESSAGE_ENUM tile_back_resizer_init(TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg,
                                      TILE_FUNC_BLOCK_STRUCT *ptr_func);
static ISP_TILE_MESSAGE_ENUM tile_back_resizer_update_result(TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg,
                                              TILE_FUNC_BLOCK_STRUCT *ptr_func);
/* resizer verify */
static ISP_TILE_MESSAGE_ENUM tile_for_resizer_verify(TILE_FUNC_BLOCK_STRUCT *ptr_func,
												TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg,
												TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg);
static ISP_TILE_MESSAGE_ENUM tile_for_offset_cal(TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_back_arg);

ISP_TILE_MESSAGE_ENUM tile_back_comp_resizer(TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg,
                                               TILE_FUNC_BLOCK_STRUCT *ptr_func)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    result = tile_back_resizer_init(ptr_back_arg, ptr_func);
    if (ISP_MESSAGE_TILE_OK == result)
    {
        result = tile_back_switch_resizer(ptr_back_arg);
    }
    if (ISP_MESSAGE_TILE_OK == result)
    {
        result = tile_back_resizer_update_result(ptr_back_arg, ptr_func);
    }
    return result;
}

ISP_TILE_MESSAGE_ENUM tile_for_comp_resizer(TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg,
                                              TILE_FUNC_BLOCK_STRUCT *ptr_func,
											  TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    result = tile_for_resizer_init(ptr_for_arg, ptr_func);
    if (ISP_MESSAGE_TILE_OK == result)
    {
        result = tile_for_switch_resizer(ptr_for_arg);
    }
    if (ISP_MESSAGE_TILE_OK == result)
    {
        result = tile_for_resizer_update_result(ptr_for_arg, ptr_func, ptr_back_arg);
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_back_resizer_init(TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg,
                                      TILE_FUNC_BLOCK_STRUCT *ptr_func)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    if (CAM_DIR_X == ptr_back_arg->dir_mode)/* x dir */
    {
        if (ptr_func->in_const_x == 1)
        {
            ptr_back_arg->align_flag = CAM_UV_444_FLAG;/* YUV444 */
        }
        else
        {
            ptr_back_arg->align_flag = CAM_UV_422_FLAG;/* YUV422 */
        }
		/* check cal order, input */
		if ((1 == ptr_func->out_const_x) && (CAM_UV_422_FLAG == ptr_back_arg->uv_flag))
        {
            if (ptr_func->in_cal_order & TILE_ORDER_RIGHT_TO_LEFT)
            {
                /* align odd xe */
                if (ptr_func->out_pos_xs & 0x1)
                {
                    ptr_func->out_pos_xs += 1;
                }
            }
            else
            {
                if (ptr_func->out_pos_xe + 1 < ptr_func->full_size_x_out)
                {
                    /* align odd xe */
                    if ((ptr_func->out_pos_xe + 1) & 0x1)
                    {
                        ptr_func->out_pos_xe -= 1;
                    }
                }
            }
        }
        ptr_back_arg->out_pos_start = ptr_func->out_pos_xs;
        ptr_back_arg->out_pos_end = ptr_func->out_pos_xe;
        ptr_back_arg->max_in_pos_end = ptr_func->full_size_x_in - 1;
        ptr_back_arg->max_out_pos_end = ptr_func->full_size_x_out - 1;
        /* check cal order, input */
        ptr_back_arg->bias = ptr_func->crop_bias_x;
        ptr_back_arg->offset = ptr_func->crop_offset_x;
		if (0 == ptr_back_arg->config_bits)
		{
			ptr_back_arg->config_bits = REZ_OFFSET_SHIFT_FACTOR;
		}
    }
    else if (CAM_DIR_Y == ptr_back_arg->dir_mode)/* y dir */
    {
        if (ptr_func->in_const_y == 1)
        {
            ptr_back_arg->align_flag = CAM_UV_444_FLAG;/* YUV444 */
        }
        else
        {
            ptr_back_arg->align_flag = CAM_UV_422_FLAG;/* YUV422 */
        }
		if ((1 == ptr_func->out_const_y) && (CAM_UV_422_FLAG == ptr_back_arg->uv_flag))
        {
			if (ptr_func->out_pos_ye + 1 < ptr_func->full_size_y_out)
			{
				/* align odd ye */
				if ((ptr_func->out_pos_ye + 1) & 0x1)
				{
					ptr_func->out_pos_ye -= 1;
				}
            }
        }
        ptr_back_arg->out_pos_start = ptr_func->out_pos_ys;
        ptr_back_arg->out_pos_end = ptr_func->out_pos_ye;
        ptr_back_arg->max_in_pos_end = ptr_func->full_size_y_in - 1;
        ptr_back_arg->max_out_pos_end = ptr_func->full_size_y_out - 1;
        /* check cal order, input */
        ptr_back_arg->bias = ptr_func->crop_bias_y;
        ptr_back_arg->offset = ptr_func->crop_offset_y;
		if (0 == ptr_back_arg->config_bits)
		{
			ptr_back_arg->config_bits = REZ_OFFSET_SHIFT_FACTOR;
		}
    }
    else
    {
        result = ISP_MESSAGE_UNKNOWN_RESIZER_DIR_MODE_ERROR;
        tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_back_resizer_update_result(TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg,
                                              TILE_FUNC_BLOCK_STRUCT *ptr_func)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    if (CAM_DIR_X == ptr_back_arg->dir_mode)/* x dir */
    {
        ptr_func->in_pos_xs = ptr_back_arg->in_pos_start;
        ptr_func->in_pos_xe = ptr_back_arg->in_pos_end;
		if ((ptr_func->in_const_x > 1) || (CAM_UV_422_FLAG == ptr_back_arg->uv_flag))
        {
			int in_const_x = (ptr_func->in_const_x > 1)?ptr_func->in_const_x:2;
			/* check cal order, input */
            if (ptr_func->in_cal_order & TILE_ORDER_RIGHT_TO_LEFT)
            {
			    int val_e = TILE_MOD(ptr_func->in_pos_xe + 1, in_const_x);
			    int val_s = TILE_MOD(ptr_func->in_pos_xs, in_const_x);
			    if (0 != val_e)
			    {
				    ptr_func->in_pos_xe += in_const_x - val_e;/* increase xe */
			    }
				if (ptr_func->in_pos_xe + 1 >= ptr_func->full_size_x_in)
                {
                    ptr_func->in_pos_xe = ptr_func->full_size_x_in - 1;
				}
                /* check tile width & even xs */
                if ((ptr_func->in_pos_xs + ptr_func->in_tile_width > val_s + ptr_func->in_pos_xe + 1) || (0 == ptr_func->in_tile_width))
                {
					if (0 != val_s)
					{
						ptr_func->in_pos_xs -= val_s;/* decrease xs */
					}
                }
                else/* tile width clipping */
                {
                    ptr_func->in_pos_xs =  ptr_func->in_pos_xe + 1 - ptr_func->in_tile_width;
					val_s = TILE_MOD(ptr_func->in_pos_xs, in_const_x);
					if (0 != val_s)
					{
						ptr_func->in_pos_xs += in_const_x - val_s;/* increaes xs */
					}
                }
            }
            else
            {
			    int val_e = TILE_MOD(ptr_func->in_pos_xe + 1, in_const_x);
			    int val_s = TILE_MOD(ptr_func->in_pos_xs, in_const_x);                
			    if (0 != val_s)
			    {
				    ptr_func->in_pos_xs -= val_s;/* decreae xs */
			    }
                /* check tile width & extend odd xe */
				if ((ptr_func->in_pos_xs + ptr_func->in_tile_width + val_e > ptr_func->in_pos_xe + (val_e?in_const_x:0) + 1) || (0 == ptr_func->in_tile_width))
                {
					if (0 != val_e)
					{
						ptr_func->in_pos_xe += in_const_x - val_e;/* increase xe */
					}
					if (ptr_func->in_pos_xe + 1 >= ptr_func->full_size_x_in)
					{
						ptr_func->in_pos_xe = ptr_func->full_size_x_in - 1;
					}
                }
                else/* tile width clipping, always even */
                {
                    ptr_func->in_pos_xe =  ptr_func->in_pos_xs - 1 + ptr_func->in_tile_width;
					val_e = TILE_MOD(ptr_func->in_pos_xe + 1, in_const_x);
					if (0 != val_e)
					{
						ptr_func->in_pos_xe -= val_e;/* decrease xe */
					}
                }
            }
        }
    }
    else if (CAM_DIR_Y == ptr_back_arg->dir_mode)/* y dir */
    {
        ptr_func->in_pos_ys = ptr_back_arg->in_pos_start;
        ptr_func->in_pos_ye = ptr_back_arg->in_pos_end;
		if ((ptr_func->in_const_y > 1) || (CAM_UV_422_FLAG == ptr_back_arg->uv_flag))
        {
			int in_const_y = (ptr_func->in_const_y > 1)?ptr_func->in_const_y:2;
			int val_e = TILE_MOD(ptr_func->in_pos_ye + 1, in_const_y);
			int val_s = TILE_MOD(ptr_func->in_pos_ys, in_const_y);                
			if (0 != val_s)
			{
				ptr_func->in_pos_ys -= val_s;/* decrease xs */
			}
			/* check tile width & extend odd xe */
			if ((ptr_func->in_pos_ys + ptr_func->in_tile_height + val_e > ptr_func->in_pos_ye + (val_e?in_const_y:0) + 1) || (0 == ptr_func->in_tile_height))
			{
				if (0 != val_e)
				{
					ptr_func->in_pos_ye += in_const_y - val_e;/* increase xe */
				}
				if (ptr_func->in_pos_ye + 1 >= ptr_func->full_size_y_in)
				{
					ptr_func->in_pos_ye = ptr_func->full_size_y_in - 1;
				}
			}
			else/* tile width clipping, always even */
			{
				ptr_func->in_pos_ye =  ptr_func->in_pos_ys - 1 + ptr_func->in_tile_height;
				val_e = TILE_MOD(ptr_func->in_pos_ye + 1, in_const_y);
				if (0 != val_e)
				{
					ptr_func->in_pos_ye -= val_e;/* decreae xe */
				}
			}
		}
    }
    else
    {
        result = ISP_MESSAGE_UNKNOWN_RESIZER_DIR_MODE_ERROR;
        tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_back_switch_resizer(TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    switch (ptr_back_arg->mode)
    {
        case TILE_RESIZER_MODE_8_TAPES:
            result = tile_back_8tp(ptr_back_arg);
            break;
        case TILE_RESIZER_MODE_SRC_ACC:
            result = tile_back_src_acc(ptr_back_arg);
            break;
        case TILE_RESIZER_MODE_CUBIC_ACC:
            result = tile_back_cub_acc(ptr_back_arg);
            break;
        case TILE_RESIZER_MODE_4_TAPES:
            result = tile_back_4tp(ptr_back_arg);
            break;
        case TILE_RESIZER_MODE_5_TAPES:
            result = tile_back_5tp(ptr_back_arg);
            break;
        case TILE_RESIZER_MODE_6_TAPES:
            result = tile_back_6tp(ptr_back_arg);
            break;
        case TILE_RESIZER_MODE_2_TAPES:
            result = tile_back_2tp(ptr_back_arg);
            break;
        case TILE_RESIZER_MODE_MDP_6_TAPES:
            result = tile_back_mdp_6tp(ptr_back_arg);
            break;
        case TILE_RESIZER_MODE_MDP_CUBIC_ACC:
            result = tile_back_mdp_cub_acc(ptr_back_arg);
            break;
        default:
            result = ISP_MESSAGE_NOT_SUPPORT_RESIZER_MODE_ERROR;
            tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
            break;
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_for_resizer_init(TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg,
                                              TILE_FUNC_BLOCK_STRUCT *ptr_func)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    if (CAM_DIR_X == ptr_for_arg->dir_mode)/* x dir */
    {
        if (1 == ptr_func->out_const_x)
        {
            ptr_for_arg->align_flag = CAM_UV_444_FLAG;/* YUV444 */
        }
        else
        {
            ptr_for_arg->align_flag = CAM_UV_422_FLAG;/* YUV422 */
        }
        ptr_for_arg->in_pos_start = ptr_func->in_pos_xs;
        ptr_for_arg->in_pos_end = ptr_func->in_pos_xe;
        ptr_for_arg->max_in_pos_end = ptr_func->full_size_x_in - 1;
        ptr_for_arg->max_out_pos_end = ptr_func->full_size_x_out - 1;
        /* check cal order, input */
        ptr_for_arg->bias = ptr_func->crop_bias_x;
        ptr_for_arg->offset = ptr_func->crop_offset_x;
		if (0 == ptr_for_arg->config_bits)
		{
			ptr_for_arg->config_bits = REZ_OFFSET_SHIFT_FACTOR;
		}
    }
    else if (CAM_DIR_Y == ptr_for_arg->dir_mode)/* y dir */
    {
        if (ptr_func->out_const_y == 1)
        {
            ptr_for_arg->align_flag = CAM_UV_444_FLAG;/* YUV444 */
        }
        else
        {
            ptr_for_arg->align_flag = CAM_UV_422_FLAG;/* YUV422 */
        }
        ptr_for_arg->in_pos_start = ptr_func->in_pos_ys;
        ptr_for_arg->in_pos_end = ptr_func->in_pos_ye;
        ptr_for_arg->max_in_pos_end = ptr_func->full_size_y_in - 1;
        ptr_for_arg->max_out_pos_end = ptr_func->full_size_y_out - 1;
        /* check cal order, input */
        ptr_for_arg->bias = ptr_func->crop_bias_y;
        ptr_for_arg->offset = ptr_func->crop_offset_y;
		if (0 == ptr_for_arg->config_bits)
		{
			ptr_for_arg->config_bits = REZ_OFFSET_SHIFT_FACTOR;
		}
    }
    else
    {
        result = ISP_MESSAGE_UNKNOWN_RESIZER_DIR_MODE_ERROR;
        tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_for_resizer_update_result(TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg,
                                              TILE_FUNC_BLOCK_STRUCT *ptr_func,
											  TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    if (CAM_DIR_X == ptr_for_arg->dir_mode)/* x dir */
    {
		ptr_func->out_pos_xs = ptr_for_arg->out_pos_start;
		ptr_func->out_pos_xe = ptr_for_arg->out_pos_end;
		/* check cal order, output */
		if (ptr_func->out_cal_order & TILE_ORDER_RIGHT_TO_LEFT)
		{
			int out_const_x = (ptr_func->out_const_x > 1)?ptr_func->out_const_x:((CAM_UV_422_FLAG == ptr_for_arg->uv_flag)?2:1);
			if (TILE_RUN_MODE_SUB_OUT == ptr_func->run_mode)
			{
				/* check alignment, keep odd */
				if (out_const_x > 1)
				{
					int val_e = TILE_MOD(ptr_func->out_pos_xe + 1, out_const_x);
					if (0 != val_e)
					{
						ptr_func->out_pos_xe -= val_e;/* decrease xe */
					}
				}
				/* check tile size */
				if (ptr_func->out_pos_xs + ptr_func->out_tile_width < ptr_func->out_pos_xe + 1)
				{
					ptr_func->out_pos_xs = ptr_func->out_pos_xe + 1 - ptr_func->out_tile_width;
				}
				/* check alignment, keep even */
				if (out_const_x > 1)
				{
					int val_s = TILE_MOD(ptr_func->out_pos_xs, out_const_x);
					if (0 != val_s)
					{
						ptr_func->out_pos_xs += out_const_x - val_s;/* increase xs */
					}
				}
			}
			else
			{
				if (ptr_func->backward_output_xe_pos > ptr_func->out_pos_xe)
				{
					result = ISP_MESSAGE_BACKWARD_START_LESS_THAN_FORWARD_ERROR;
					tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
					return result;
				}
				else
				{
					/* crop disabled w/o edge sync */
					if ((ptr_func->out_pos_xe < ptr_for_arg->max_out_pos_end) || (ptr_func->type & TILE_TYPE_CROP_EN))
					{
						ptr_func->out_pos_xe = ptr_func->backward_output_xe_pos;
					}
					else
					{
						/* check alignment, keep odd */
						if (out_const_x > 1)
						{
							int val_e = TILE_MOD(ptr_func->out_pos_xe + 1, out_const_x);
							if (0 != val_e)
							{
								ptr_func->out_pos_xe -= val_e;/* decrease xe */
							}
						}
					}
				}
				/* resizer support crop by xs */
				if (ptr_func->out_pos_xs > ptr_func->backward_output_xs_pos)
				{
					/* check alignment, keep even */
					if (out_const_x > 1)
					{
						int val_s = TILE_MOD(ptr_func->out_pos_xs, out_const_x);
						if (0 != val_s)
						{
							ptr_func->out_pos_xs += out_const_x - val_s;/* increase xs */
						}
					}
				}
				else
				{
					/* crop disabled w/o edge sync */
					if (ptr_func->out_pos_xs || (ptr_func->type & TILE_TYPE_CROP_EN))
					{
						ptr_func->out_pos_xs = ptr_func->backward_output_xs_pos;
					}
					else
					{
						/* sync edge */
						ptr_func->out_pos_xs = 0;
					}
				}
			}
		}
        else
		{
			int out_const_x = (ptr_func->out_const_x > 1)?ptr_func->out_const_x:((CAM_UV_422_FLAG == ptr_for_arg->uv_flag)?2:1);
			if (TILE_RUN_MODE_SUB_OUT == ptr_func->run_mode)
			{
				/* check alignment, keep even */
				if (out_const_x > 1)
				{
					int val_s = TILE_MOD(ptr_func->out_pos_xs, out_const_x);
					if (0 != val_s)
					{
						ptr_func->out_pos_xs += out_const_x - val_s;/* increase xs */
					}
				}
				/* check tile size */
				if (ptr_func->out_pos_xs + ptr_func->out_tile_width < ptr_func->out_pos_xe + 1)
				{
					ptr_func->out_pos_xe = ptr_func->out_pos_xs - 1 + ptr_func->out_tile_width;
				}
				/* check alignment, keep odd */
				if (out_const_x > 1)
				{
					int val_e = TILE_MOD(ptr_func->out_pos_xe + 1, out_const_x);
					if (0 != val_e)
					{
						ptr_func->out_pos_xe -= val_e;/* decrease xe */
					}
				}
			}
			else
			{
				if (ptr_func->backward_output_xs_pos < ptr_func->out_pos_xs)
				{
					result = ISP_MESSAGE_BACKWARD_START_LESS_THAN_FORWARD_ERROR;
					tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
					return result;
				}
				else
				{
					/* crop disabled w/o edge sync */
					if (ptr_func->out_pos_xs || (ptr_func->type & TILE_TYPE_CROP_EN))
					{
						ptr_func->out_pos_xs = ptr_func->backward_output_xs_pos;
					}
					else
					{
						/* check alignment, keep even */
						if (out_const_x > 1)
						{
							int val_s = TILE_MOD(ptr_func->out_pos_xs, out_const_x);
							if (0 != val_s)
							{
								ptr_func->out_pos_xs += out_const_x - val_s;/* increase xs */
							}
						}
					}
				}
				/* resizer support crop by xe */
				if (ptr_func->out_pos_xe < ptr_func->backward_output_xe_pos)
				{
					/* check alignment, keep odd */
					if (out_const_x > 1)
					{
						int val_e = TILE_MOD(ptr_func->out_pos_xe + 1, out_const_x);
						if (0 != val_e)
						{
							ptr_func->out_pos_xe -= val_e;/* decreae xe */
						}
					}
				}
				else
				{
					/* crop disabled w/o edge sync */
					if ((ptr_func->out_pos_xe < ptr_for_arg->max_out_pos_end) || (ptr_func->type & TILE_TYPE_CROP_EN))
					{
						ptr_func->out_pos_xe = ptr_func->backward_output_xe_pos;
					}
				}
			}
		}
        /* update for tile_for_offset_cal() */
        ptr_for_arg->offset_cal_start = ptr_func->out_pos_xs;
        /* cal forward offset */
        result = tile_for_offset_cal(ptr_for_arg);
        /* update forward offset */
        ptr_func->bias_x = ptr_for_arg->in_bias;
        ptr_func->offset_x = ptr_for_arg->in_offset;
        ptr_func->bias_x_c = ptr_for_arg->in_bias_c;
        ptr_func->offset_x_c = ptr_for_arg->in_offset_c;
        if (ISP_MESSAGE_TILE_OK == result)
        {
            result = tile_for_resizer_verify(ptr_func, ptr_for_arg, ptr_back_arg);
        }
        if (ISP_MESSAGE_TILE_OK != result)
        {
            return result;
        }
    }
    else if (CAM_DIR_Y == ptr_for_arg->dir_mode)/* y dir */
    {
		ptr_func->out_pos_ys = ptr_for_arg->out_pos_start;
		ptr_func->out_pos_ye = ptr_for_arg->out_pos_end;
		/* check cal order, not support */
		if (TILE_RUN_MODE_SUB_OUT == ptr_func->run_mode)
		{
			int out_const_y = (ptr_func->out_const_y > 1)?ptr_func->out_const_y:((CAM_UV_422_FLAG == ptr_for_arg->uv_flag)?2:1);
			/* check alignment, keep even */
			if (out_const_y > 1)
			{
				int val_s = TILE_MOD(ptr_func->out_pos_ys, out_const_y);
				if (0 != val_s)
				{
					ptr_func->out_pos_ys += out_const_y - val_s;/* increase xs */
				}
			}
			/* check tile size */
			if (ptr_func->out_pos_ys + ptr_func->out_tile_height < ptr_func->out_pos_ye + 1)
			{
				ptr_func->out_pos_ye = ptr_func->out_pos_ys - 1 + ptr_func->out_tile_height;
			}
			/* check alignment, keep even */
			if (out_const_y > 1)
			{
				int val_e = TILE_MOD(ptr_func->out_pos_ye + 1, out_const_y);
				if (0 != val_e)
				{
					ptr_func->out_pos_ye -= val_e;/* decrease xe */
				}
			}
		}
		else
		{
			int out_const_y = (ptr_func->out_const_y > 1)?ptr_func->out_const_y:((CAM_UV_422_FLAG == ptr_for_arg->uv_flag)?2:1);
			if (ptr_func->backward_output_ys_pos < ptr_func->out_pos_ys)
			{
				result = ISP_MESSAGE_BACKWARD_START_LESS_THAN_FORWARD_ERROR;
				tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
				return result;
			}
			else
			{
				/* crop disabled w/o edge sync */
				if (ptr_for_arg->out_pos_start || (ptr_func->type & TILE_TYPE_CROP_EN))
				{
					/* update vertical result */
					ptr_func->out_pos_ys = ptr_func->backward_output_ys_pos;
				}
			}
			/* resizer support crop by ye */
			if (ptr_for_arg->out_pos_end < ptr_func->backward_output_ye_pos)
			{
				/* check alignment, keep odd */
				if (out_const_y > 1)
				{
					int val_e = TILE_MOD(ptr_func->out_pos_ye + 1, out_const_y);
					if (0 != val_e)
					{
						ptr_func->out_pos_ye -= val_e;/* decreae ye */
					}
				}
			}
			else
			{
				/* crop disabled w/o edge sync */
				if ((ptr_for_arg->out_pos_end < ptr_for_arg->max_out_pos_end) || (ptr_func->type & TILE_TYPE_CROP_EN))
				{
					ptr_func->out_pos_ye = ptr_func->backward_output_ye_pos;
				}
				else
				{
					/* sync edge */
					ptr_func->out_pos_ye = ptr_for_arg->out_pos_end;
				}
			}
		}
        /* update for tile_for_offset_cal() */
        ptr_for_arg->offset_cal_start = ptr_func->out_pos_ys;
        /* cal forward offset */
        result = tile_for_offset_cal(ptr_for_arg);
        /* update forward offset */
        ptr_func->bias_y = ptr_for_arg->in_bias;
        ptr_func->offset_y = ptr_for_arg->in_offset;
        ptr_func->bias_y_c = ptr_for_arg->in_bias_c;
        ptr_func->offset_y_c = ptr_for_arg->in_offset_c;
        if (ISP_MESSAGE_TILE_OK == result)
        {
            result = tile_for_resizer_verify(ptr_func, ptr_for_arg, ptr_back_arg);
        }
    }
    else
    {
        result = ISP_MESSAGE_UNKNOWN_RESIZER_DIR_MODE_ERROR;
        tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_for_switch_resizer(TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    switch (ptr_for_arg->mode)
    {
        case TILE_RESIZER_MODE_8_TAPES:
            result = tile_for_8tp(ptr_for_arg);
            break;
        case TILE_RESIZER_MODE_SRC_ACC:
            result = tile_for_src_acc(ptr_for_arg);
            break;
        case TILE_RESIZER_MODE_CUBIC_ACC:
            result = tile_for_cub_acc(ptr_for_arg);
            break;
        case TILE_RESIZER_MODE_4_TAPES:
            result = tile_for_4tp(ptr_for_arg);
            break;
        case TILE_RESIZER_MODE_5_TAPES:
            result = tile_for_5tp(ptr_for_arg);
            break;
        case TILE_RESIZER_MODE_6_TAPES:
            result = tile_for_6tp(ptr_for_arg);
            break;
        case TILE_RESIZER_MODE_2_TAPES:
            result = tile_for_2tp(ptr_for_arg);
            break;
        case TILE_RESIZER_MODE_MDP_6_TAPES:
            result = tile_for_mdp_6tp(ptr_for_arg);
            break;
        case TILE_RESIZER_MODE_MDP_CUBIC_ACC:
            result = tile_for_mdp_cub_acc(ptr_for_arg);
            break;
        default:
            result = ISP_MESSAGE_NOT_SUPPORT_RESIZER_MODE_ERROR;
            tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
            break;
    }
    return result;
}


static ISP_TILE_MESSAGE_ENUM tile_for_offset_cal(TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    int prec = (int)(1 << ptr_for_arg->prec_bits);
    switch (ptr_for_arg->mode)
    {
        case TILE_RESIZER_MODE_8_TAPES:
        case TILE_RESIZER_MODE_6_TAPES:
        case TILE_RESIZER_MODE_5_TAPES:
        case TILE_RESIZER_MODE_4_TAPES:
        case TILE_RESIZER_MODE_2_TAPES:
        case TILE_RESIZER_MODE_MDP_6_TAPES:
        {
            int coeff_temp = ptr_for_arg->coeff_step;
            int offset = ((unsigned long long)ptr_for_arg->offset*prec)>>ptr_for_arg->config_bits;
            /* cal bias & offset by offset_cal_start */
            long long sub_temp = (long long)ptr_for_arg->offset_cal_start*coeff_temp +
                (long long)ptr_for_arg->bias*prec + offset - (long long)ptr_for_arg->in_pos_start*prec;
			if (sub_temp >= 0)
			{
				ptr_for_arg->in_bias = (int)((unsigned long long)sub_temp>>ptr_for_arg->prec_bits);
				ptr_for_arg->in_offset = (int)(sub_temp - (long long)prec*ptr_for_arg->in_bias);
				if (CAM_UV_444_FLAG == ptr_for_arg->uv_flag)/* YUV444 */
				{
					ptr_for_arg->in_bias_c = ptr_for_arg->in_bias;
				}
				else
				{
					ptr_for_arg->in_bias_c = (unsigned int)ptr_for_arg->in_bias>>1;
				}
				if ((CAM_UV_444_FLAG == ptr_for_arg->uv_flag) || (0 == (ptr_for_arg->in_bias & 0x1)))/* YUV444 */
				{
					ptr_for_arg->in_offset_c = ptr_for_arg->in_offset;
				}
				else
				{
					ptr_for_arg->in_offset_c = ptr_for_arg->in_offset + prec;
				}
			}
			else/* negative offset for MDP 6 taps */
			{
				ptr_for_arg->in_bias = -1;
				sub_temp += (long long)prec;
				if (sub_temp < 0)
				{
					ptr_for_arg->in_offset = 0;
				}
				else
				{
					ptr_for_arg->in_offset = (int)sub_temp;
				}
				if (CAM_UV_444_FLAG == ptr_for_arg->uv_flag)/* YUV444 */
				{
					ptr_for_arg->in_bias_c = ptr_for_arg->in_bias;
				}
				else
				{
					ptr_for_arg->in_bias_c = 0;
				}
				if ((CAM_UV_444_FLAG == ptr_for_arg->uv_flag) || (0 == (ptr_for_arg->in_bias & 0x1)))/* YUV444 */
				{
					ptr_for_arg->in_offset_c = ptr_for_arg->in_offset;
				}
				else
				{
					ptr_for_arg->in_offset_c = ptr_for_arg->in_offset + prec;
				}
			}
            break;
        }
        case TILE_RESIZER_MODE_SRC_ACC:
        case TILE_RESIZER_MODE_CUBIC_ACC:
        case TILE_RESIZER_MODE_MDP_CUBIC_ACC:
        {
            int coeff_temp = ptr_for_arg->coeff_step;
            int offset = ((unsigned long long)ptr_for_arg->offset*coeff_temp)>>ptr_for_arg->config_bits;
            /* cal bias & offset by offset_cal_start */
            long long sub_temp = (long long)ptr_for_arg->offset_cal_start*prec +
                (long long)ptr_for_arg->bias*coeff_temp + offset - (long long)ptr_for_arg->in_pos_start*coeff_temp;
            ptr_for_arg->in_bias = (int)((unsigned long long)sub_temp>>ptr_for_arg->prec_bits);
			ptr_for_arg->in_offset = (int)(sub_temp - (long long)prec*ptr_for_arg->in_bias);
            ptr_for_arg->in_bias_c = ptr_for_arg->in_bias;
            ptr_for_arg->in_offset_c = ptr_for_arg->in_offset;
            break;
        }
        default:
        {
            result = ISP_MESSAGE_NOT_SUPPORT_RESIZER_MODE_ERROR;
            tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
            return result;
        }
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_for_resizer_verify(TILE_FUNC_BLOCK_STRUCT *ptr_func,
                                                TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg,
												TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    /* verify backward & forward consistence */
    ptr_back_arg->mode = ptr_for_arg->mode;
    ptr_back_arg->out_pos_start = ptr_for_arg->out_pos_start;
    ptr_back_arg->out_pos_end = ptr_for_arg->out_pos_end;
    ptr_back_arg->bias = ptr_for_arg->bias;
    ptr_back_arg->offset = ptr_for_arg->offset;
    ptr_back_arg->prec_bits = ptr_for_arg->prec_bits;
    ptr_back_arg->config_bits = ptr_for_arg->config_bits;
	ptr_back_arg->uv_flag = ptr_for_arg->uv_flag;
    /* horizontal */
    if (CAM_DIR_X == ptr_for_arg->dir_mode)/* x dir */
    {
        if (ptr_func->in_const_x == 1)
        {
            ptr_back_arg->align_flag = CAM_UV_444_FLAG;/* YUV444 */
        }
        else
        {
            ptr_back_arg->align_flag = CAM_UV_422_FLAG;/* YUV422 */
        }
    }
    /* vertical */
    else if (CAM_DIR_Y == ptr_for_arg->dir_mode)/* y dir */
    {
        if (ptr_func->in_const_y == 1)
        {
            ptr_back_arg->align_flag = CAM_UV_444_FLAG;/* YUV444 */
        }
        else
        {
            ptr_back_arg->align_flag = CAM_UV_422_FLAG;/* YUV422 */
        }
    }
    else
    {
        result = ISP_MESSAGE_UNKNOWN_RESIZER_DIR_MODE_ERROR;
        tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
        return result;
    }
    ptr_back_arg->dir_mode = ptr_for_arg->dir_mode;
    ptr_back_arg->max_in_pos_end =  ptr_for_arg->max_in_pos_end;
    ptr_back_arg->max_out_pos_end = ptr_for_arg->max_out_pos_end;
    ptr_back_arg->coeff_step = ptr_for_arg->coeff_step;
    result = tile_back_switch_resizer(ptr_back_arg);
    if (ISP_MESSAGE_TILE_OK == result)
    {
        if (ptr_back_arg->in_pos_start < ptr_for_arg->in_pos_start)
        {
            if (CAM_DIR_X == ptr_for_arg->dir_mode)/* x dir */
            {   
                result = ISP_MESSAGE_VERIFY_BACKWARD_XS_LESS_THAN_FORWARD_ERROR;
                tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
            }
            else if (CAM_DIR_Y == ptr_for_arg->dir_mode) /* y dir */
            {
                result = ISP_MESSAGE_VERIFY_BACKWARD_YS_LESS_THAN_FORWARD_ERROR;
                tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
            }
            else
            {
                result = ISP_MESSAGE_UNKNOWN_RESIZER_DIR_MODE_ERROR;
                tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
            }
            return result;
        }
        if (ptr_for_arg->in_pos_end < ptr_back_arg->in_pos_end)
        {
            if (CAM_DIR_X == ptr_for_arg->dir_mode)/* x dir */
            {   
                result = ISP_MESSAGE_VERIFY_FORWARD_XE_LESS_THAN_BACKWARD_ERROR;
                tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
            }
            else if (CAM_DIR_Y == ptr_for_arg->dir_mode)/* y dir */
            {
                result = ISP_MESSAGE_VERIFY_FORWARD_YE_LESS_THAN_BACKWARD_ERROR;
                tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
            }
            else
            {
                result = ISP_MESSAGE_UNKNOWN_RESIZER_DIR_MODE_ERROR;
                tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
            }
            return result;
        }
    }
    /* check pos correct */
    if ((ptr_for_arg->in_pos_start != ptr_back_arg->in_pos_start) ||
        (ptr_for_arg->in_pos_end != ptr_back_arg->in_pos_end))
    {
        int temp_start = ptr_for_arg->out_pos_start;
        int temp_end = ptr_for_arg->out_pos_end;
        ptr_for_arg->in_pos_start = ptr_back_arg->in_pos_start;
        ptr_for_arg->in_pos_end = ptr_back_arg->in_pos_end;
        result = tile_for_switch_resizer(ptr_for_arg);
        if (ISP_MESSAGE_TILE_OK == result)
        {
            if (ptr_for_arg->out_pos_start != temp_start)
            {
                ptr_for_arg->out_pos_start = temp_start;
                switch (ptr_back_arg->mode)
                {
                    case TILE_RESIZER_MODE_8_TAPES:
                    case TILE_RESIZER_MODE_6_TAPES:
                    case TILE_RESIZER_MODE_5_TAPES:
                    case TILE_RESIZER_MODE_4_TAPES:
                    case TILE_RESIZER_MODE_2_TAPES:
                    case TILE_RESIZER_MODE_MDP_6_TAPES:
                        if (CAM_DIR_X == ptr_for_arg->dir_mode)/* x dir */
                        {   
                            result = ISP_MESSAGE_VERIFY_4_8_TAPES_XS_OUT_INCONSISTENCE_ERROR;
                            tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                        }
                        else if (CAM_DIR_Y == ptr_for_arg->dir_mode)/* y dir */
                        {
                            result = ISP_MESSAGE_VERIFY_4_8_TAPES_YS_OUT_INCONSISTENCE_ERROR;
                            tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                        }
                        else
                        {
                            result = ISP_MESSAGE_UNKNOWN_RESIZER_DIR_MODE_ERROR;
                            tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                        }
                        break;
                    case TILE_RESIZER_MODE_SRC_ACC:
                        if (CAM_DIR_X == ptr_for_arg->dir_mode)/* x dir */
                        {   
                            result = ISP_MESSAGE_VERIFY_SRC_ACC_XS_OUT_INCONSISTENCE_ERROR;
                            tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                        }
                        else if (CAM_DIR_Y == ptr_for_arg->dir_mode)/* y dir */
                        {
                            result = ISP_MESSAGE_VERIFY_SRC_ACC_YS_OUT_INCONSISTENCE_ERROR;
                            tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                        }
                        else
                        {
                            result = ISP_MESSAGE_UNKNOWN_RESIZER_DIR_MODE_ERROR;
                            tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                        }
                        break;
                    case TILE_RESIZER_MODE_CUBIC_ACC:
                    case TILE_RESIZER_MODE_MDP_CUBIC_ACC:
                        if (CAM_DIR_X == ptr_for_arg->dir_mode)/* x dir */
                        {   
                            result = ISP_MESSAGE_VERIFY_CUBIC_ACC_XS_OUT_INCONSISTENCE_ERROR;
                            tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                        }
                        else if (CAM_DIR_Y == ptr_for_arg->dir_mode)/* y dir */
                        {
                            result = ISP_MESSAGE_VERIFY_CUBIC_ACC_YS_OUT_INCONSISTENCE_ERROR;
                            tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                        }
                        else
                        {
                            result = ISP_MESSAGE_UNKNOWN_RESIZER_DIR_MODE_ERROR;
                            tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                        }
                        break;
                    default:
                        result = ISP_MESSAGE_NOT_SUPPORT_RESIZER_MODE_ERROR;
                        tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                        break;
                }
            }
            if (ISP_MESSAGE_TILE_OK == result)
            {
                if (ptr_for_arg->out_pos_end != temp_end)
                {   
                    ptr_for_arg->out_pos_end = temp_end;
                    switch (ptr_back_arg->mode)
                    {
                        case TILE_RESIZER_MODE_8_TAPES:
                        case TILE_RESIZER_MODE_6_TAPES:
                        case TILE_RESIZER_MODE_5_TAPES:
                        case TILE_RESIZER_MODE_4_TAPES:
                        case TILE_RESIZER_MODE_2_TAPES:
						case TILE_RESIZER_MODE_MDP_6_TAPES:
                            if (CAM_DIR_X == ptr_for_arg->dir_mode)/* x dir */
                            {   
                                result = ISP_MESSAGE_VERIFY_4_8_TAPES_XE_OUT_INCONSISTENCE_ERROR;
                                tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                            }
                            else if (CAM_DIR_Y == ptr_for_arg->dir_mode)/* y dir */
                            {
                                result = ISP_MESSAGE_VERIFY_4_8_TAPES_YE_OUT_INCONSISTENCE_ERROR;
                                tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                            }
                            else
                            {
                                result = ISP_MESSAGE_UNKNOWN_RESIZER_DIR_MODE_ERROR;
                                tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                            }
                            break;
                        case TILE_RESIZER_MODE_SRC_ACC:
                            if (CAM_DIR_X == ptr_for_arg->dir_mode)/* x dir */
                            {   
                                result = ISP_MESSAGE_VERIFY_SRC_ACC_XE_OUT_INCONSISTENCE_ERROR;
                                tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                            }
                            else if (CAM_DIR_Y == ptr_for_arg->dir_mode)/* y dir */
                            {
                                result = ISP_MESSAGE_VERIFY_SRC_ACC_YE_OUT_INCONSISTENCE_ERROR;
                                tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                            }
                            else
                            {
                                result = ISP_MESSAGE_UNKNOWN_RESIZER_DIR_MODE_ERROR;
                                tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                            }
                            break;
                        case TILE_RESIZER_MODE_CUBIC_ACC:
                        case TILE_RESIZER_MODE_MDP_CUBIC_ACC:
                            if (CAM_DIR_X == ptr_for_arg->dir_mode)/* x dir */
                            {   
                                result = ISP_MESSAGE_VERIFY_CUBIC_ACC_XE_OUT_INCONSISTENCE_ERROR;
                                tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                            }
                            else if (CAM_DIR_Y == ptr_for_arg->dir_mode)/* y dir */
                            {
                                result = ISP_MESSAGE_VERIFY_CUBIC_ACC_YE_OUT_INCONSISTENCE_ERROR;
                                tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                            }
                            else
                            {
                                result = ISP_MESSAGE_UNKNOWN_RESIZER_DIR_MODE_ERROR;
                                tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                            }
                            break;
                        default:
                            result = ISP_MESSAGE_NOT_SUPPORT_RESIZER_MODE_ERROR;
                            tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                            break;
                    }
                }
            }
        }
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_for_8tp(TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    int prec = (int)(1 << ptr_for_arg->prec_bits);
    int coeff_temp = ptr_for_arg->coeff_step;
    int offset = ((unsigned long long)ptr_for_arg->offset*prec)>>ptr_for_arg->config_bits;
    long long start_temp;
    long long end_temp;
    int uv_loss = (CAM_UV_422_FLAG == ptr_for_arg->uv_flag)?2:0;
    /* cal pos */
    if (0 == ptr_for_arg->in_pos_start)
    {
        ptr_for_arg->out_pos_start = 0;
    }
    else
    {
        int n;
        start_temp = (long long)(ptr_for_arg->in_pos_start + 3)*prec - (long long)ptr_for_arg->bias*prec - offset;
        n = (int)(start_temp/coeff_temp);
        if (((long long)n*coeff_temp) < start_temp)/* ceiling be careful with value smaller than zero */
        {
            n = n + 1;
        }
        if (n < 0)
        {
            n = 0;
        }
        if (((n & 0x1) == 0) || (CAM_UV_444_FLAG == ptr_for_arg->align_flag))/* YUV444 */
        {
            ptr_for_arg->out_pos_start = n;
        }
        else
        {
            ptr_for_arg->out_pos_start = n + 1;
        }
    }
    if (ptr_for_arg->in_pos_end == ptr_for_arg->max_in_pos_end)
    {
        ptr_for_arg->out_pos_end = ptr_for_arg->max_out_pos_end;
    }
    else
    {
        int n;
        end_temp = (long long)(ptr_for_arg->in_pos_end - 3 - uv_loss)*prec -  (long long)ptr_for_arg->bias*prec - offset;
        n = (int)(end_temp/coeff_temp);
        if (((long long)n*coeff_temp) == end_temp)
        {
            n = n - 1;
        }
        if (((n & 0x1) == 1) || (CAM_UV_444_FLAG == ptr_for_arg->align_flag))/* YUV444 */
        {
            ptr_for_arg->out_pos_end = n;
        }
        else
        {
            ptr_for_arg->out_pos_end = n - 1;
        }
    }
    if (ptr_for_arg->out_pos_start > ptr_for_arg->out_pos_end)
    {
        result = ISP_MESSAGE_TP8_FOR_INVALID_OUT_XYS_XYE_ERROR;
        tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
        return result;
    }
    if (ptr_for_arg->out_pos_end > ptr_for_arg->max_out_pos_end)
    {
        ptr_for_arg->out_pos_end = ptr_for_arg->max_out_pos_end;
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_for_6tp(TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    int prec = (int)(1 << ptr_for_arg->prec_bits);
    int coeff_temp = ptr_for_arg->coeff_step;
	int offset = ((unsigned long long)ptr_for_arg->offset*prec)>>ptr_for_arg->config_bits;
    long long start_temp;
    long long end_temp;
    int uv_loss = (CAM_UV_422_FLAG == ptr_for_arg->uv_flag)?2:0;
    /* cal pos */
    if (0 == ptr_for_arg->in_pos_start)
    {
        ptr_for_arg->out_pos_start = 0;
    }
    else
    {
        int n;
        start_temp = (long long)(ptr_for_arg->in_pos_start + 2)*prec - (long long)ptr_for_arg->bias*prec - offset;
        n = (int)(start_temp/coeff_temp);
        if (((long long)n*coeff_temp) < start_temp)/* ceiling be careful with value smaller than zero */
        {
            n = n + 1;
        }
        if (n < 0)
        {
            n = 0;
        }
        if (((n & 0x1) == 0) || (CAM_UV_444_FLAG == ptr_for_arg->align_flag))/* YUV444 */
        {
            ptr_for_arg->out_pos_start = n;
        }
        else
        {
            ptr_for_arg->out_pos_start = n + 1;
        }
    }
    if (ptr_for_arg->in_pos_end == ptr_for_arg->max_in_pos_end)
    {
        ptr_for_arg->out_pos_end = ptr_for_arg->max_out_pos_end;
    }
    else
    {
        int n;
        end_temp = (long long)(ptr_for_arg->in_pos_end - 2 - uv_loss)*prec -  (long long)ptr_for_arg->bias*prec - offset;
        n = (int)(end_temp/coeff_temp);
        if (((long long)n*coeff_temp) == end_temp)
        {
            n = n - 1;
        }
        if (((n & 0x1) == 1) || (CAM_UV_444_FLAG == ptr_for_arg->align_flag))/* YUV444 */
        {
            ptr_for_arg->out_pos_end = n;
        }
        else
        {
            ptr_for_arg->out_pos_end = n - 1;
        }
    }
    if (ptr_for_arg->out_pos_start > ptr_for_arg->out_pos_end)
    {
        result = ISP_MESSAGE_TP6_FOR_INVALID_OUT_XYS_XYE_ERROR;
        tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
        return result;
    }
    if (ptr_for_arg->out_pos_end > ptr_for_arg->max_out_pos_end)
    {
        ptr_for_arg->out_pos_end = ptr_for_arg->max_out_pos_end;
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_for_4tp(TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    int prec = (int)(1 << ptr_for_arg->prec_bits);
    int coeff_temp = ptr_for_arg->coeff_step;
    int offset = ((unsigned long long)ptr_for_arg->offset*prec)>>ptr_for_arg->config_bits;
    long long start_temp;
    long long end_temp;
    int uv_loss = (CAM_UV_422_FLAG == ptr_for_arg->uv_flag)?2:0;
    /* cal pos */
    if (0 == ptr_for_arg->in_pos_start)
    {
        ptr_for_arg->out_pos_start = 0;
    }
    else
    {
        int n;
        start_temp = (long long)(ptr_for_arg->in_pos_start + 1)*prec - (long long)ptr_for_arg->bias*prec - offset;
        n = (int)(start_temp/coeff_temp);
        if (((long long)n*coeff_temp) < start_temp)/* ceiling be careful with value smaller than zero */
        {
            n = n + 1;
        }
        if (n < 0)
        {
            n = 0;
        }
        if (((n & 0x1) == 0) || (CAM_UV_444_FLAG == ptr_for_arg->align_flag))/* YUV444 */
        {
            ptr_for_arg->out_pos_start = n;
        }
        else
        {
            ptr_for_arg->out_pos_start = n + 1;
        }
    }
    if (ptr_for_arg->in_pos_end == ptr_for_arg->max_in_pos_end)
    {
        ptr_for_arg->out_pos_end = ptr_for_arg->max_out_pos_end;
    }
    else
    {
        int n;
        end_temp = (long long)(ptr_for_arg->in_pos_end - 1 - uv_loss)*prec -  (long long)ptr_for_arg->bias*prec - offset;
        n = (int)(end_temp/coeff_temp);
        if (((long long)n*coeff_temp) == end_temp)
        {
            n = n - 1;
        }
        if (((n & 0x1) == 1) || (CAM_UV_444_FLAG == ptr_for_arg->align_flag))/* YUV444 */
        {
            ptr_for_arg->out_pos_end = n;
        }
        else
        {
            ptr_for_arg->out_pos_end = n - 1;
        }
    }
    if (ptr_for_arg->out_pos_start > ptr_for_arg->out_pos_end)
    {
        result = ISP_MESSAGE_TP4_FOR_INVALID_OUT_XYS_XYE_ERROR;
        tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
        return result;
    }
    if (ptr_for_arg->out_pos_end > ptr_for_arg->max_out_pos_end)
    {
        ptr_for_arg->out_pos_end = ptr_for_arg->max_out_pos_end;
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_for_5tp(TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    int prec = (int)(1 << ptr_for_arg->prec_bits);
    int coeff_temp = ptr_for_arg->coeff_step;
    int offset = ((unsigned long long)ptr_for_arg->offset*prec)>>ptr_for_arg->config_bits;
    long long start_temp;
    long long end_temp;
    int uv_loss = (CAM_UV_422_FLAG == ptr_for_arg->uv_flag)?2:0;
    /* cal pos */
    if (0 == ptr_for_arg->in_pos_start)
    {
        ptr_for_arg->out_pos_start = 0;
    }
    else
    {
        int n;
        start_temp = (long long)(ptr_for_arg->in_pos_start + 2)*prec - (long long)ptr_for_arg->bias*prec - offset;
        n = (int)(start_temp/coeff_temp);
        if (((long long)n*coeff_temp) < start_temp)/* ceiling be careful with value smaller than zero */
        {
            n = n + 1;
        }
        if (n < 0)
        {
            n = 0;
        }
        if (((n & 0x1) == 0) || (CAM_UV_444_FLAG == ptr_for_arg->align_flag))/* YUV444 */
        {
            ptr_for_arg->out_pos_start = n;
        }
        else
        {
            ptr_for_arg->out_pos_start = n + 1;
        }
    }
    if (ptr_for_arg->in_pos_end == ptr_for_arg->max_in_pos_end)
    {
        ptr_for_arg->out_pos_end = ptr_for_arg->max_out_pos_end;
    }
    else
    {
        int n;
        end_temp = (long long)(ptr_for_arg->in_pos_end - 1 - uv_loss)*prec -  (long long)ptr_for_arg->bias*prec - offset;
        n = (int)(end_temp/coeff_temp);
        if (((long long)n*coeff_temp) == end_temp)
        {
            n = n - 1;
        }
        if (((n & 0x1) == 1) || (CAM_UV_444_FLAG == ptr_for_arg->align_flag))/* YUV444 */
        {
            ptr_for_arg->out_pos_end = n;
        }
        else
        {
            ptr_for_arg->out_pos_end = n - 1;
        }
    }
    if (ptr_for_arg->out_pos_start > ptr_for_arg->out_pos_end)
    {
        result = ISP_MESSAGE_TP4_FOR_INVALID_OUT_XYS_XYE_ERROR;
        tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
        return result;
    }
    if (ptr_for_arg->out_pos_end > ptr_for_arg->max_out_pos_end)
    {
        ptr_for_arg->out_pos_end = ptr_for_arg->max_out_pos_end;
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_for_2tp(TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    int prec = (int)(1 << ptr_for_arg->prec_bits);
    int coeff_temp = ptr_for_arg->coeff_step;
    int offset = ((unsigned long long)ptr_for_arg->offset*prec)>>ptr_for_arg->config_bits;
    long long start_temp;
    long long end_temp;
    int uv_loss = (CAM_UV_422_FLAG == ptr_for_arg->uv_flag)?2:0;
    /* cal pos */
    if (0 == ptr_for_arg->in_pos_start)
    {
        ptr_for_arg->out_pos_start = 0;
    }
    else
    {
        int n;
        start_temp = (long long)ptr_for_arg->in_pos_start*prec - (long long)ptr_for_arg->bias*prec - offset;
        n = (int)(start_temp/coeff_temp);
        if (((long long)n*coeff_temp) < start_temp)/* ceiling be careful with value smaller than zero */
        {
            n = n + 1;
        }
        if (n < 0)
        {
            n = 0;
        }
        if (((n & 0x1) == 0) || (CAM_UV_444_FLAG == ptr_for_arg->align_flag))/* YUV444 */
        {
            ptr_for_arg->out_pos_start = n;
        }
        else
        {
            ptr_for_arg->out_pos_start = n + 1;
        }
    }
    if (ptr_for_arg->in_pos_end == ptr_for_arg->max_in_pos_end)
    {
        ptr_for_arg->out_pos_end = ptr_for_arg->max_out_pos_end;
    }
    else
    {
        int n;
        end_temp = (long long)(ptr_for_arg->in_pos_end - uv_loss)*prec - (long long)ptr_for_arg->bias*prec - offset;
        n = (int)(end_temp/coeff_temp);
        if (((long long)n*coeff_temp) == end_temp)
        {
            n = n - 1;
        }
        if (((n & 0x1) == 1) || (CAM_UV_444_FLAG == ptr_for_arg->align_flag))/* YUV444 */
        {
            ptr_for_arg->out_pos_end = n;
        }
        else
        {
            ptr_for_arg->out_pos_end = n - 1;
        }
    }
    if (ptr_for_arg->out_pos_start > ptr_for_arg->out_pos_end)
    {
        result = ISP_MESSAGE_TP2_FOR_INVALID_OUT_XYS_XYE_ERROR;
        tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
        return result;
    }
    if (ptr_for_arg->out_pos_end > ptr_for_arg->max_out_pos_end)
    {
        ptr_for_arg->out_pos_end = ptr_for_arg->max_out_pos_end;
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_for_mdp_6tp(TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    int prec = (int)(1 << ptr_for_arg->prec_bits);
    int coeff_temp = ptr_for_arg->coeff_step;
    int offset = ((unsigned long long)ptr_for_arg->offset*prec)>>ptr_for_arg->config_bits;
    long long start_temp;
    long long end_temp;
    int uv_loss = (CAM_UV_422_FLAG == ptr_for_arg->uv_flag)?2:0;
    /* cal pos */
    if (0 == ptr_for_arg->in_pos_start)
    {
        ptr_for_arg->out_pos_start = 0;
    }
    else
    {
        int n;
        start_temp = (long long)(ptr_for_arg->in_pos_start + 3)*prec - (long long)ptr_for_arg->bias*prec - offset;
        n = (int)(start_temp/coeff_temp);
        if (((long long)n*coeff_temp) < start_temp)/* ceiling be careful with value smaller than zero */
        {
            n = n + 1;
        }
        if (n < 0)
        {
            n = 0;
        }
        if (((n & 0x1) == 0) || (CAM_UV_444_FLAG == ptr_for_arg->align_flag))/* YUV444 */
        {
            ptr_for_arg->out_pos_start = n;
        }
        else
        {
            ptr_for_arg->out_pos_start = n + 1;
        }
    }
    if (ptr_for_arg->in_pos_end == ptr_for_arg->max_in_pos_end)
    {
        ptr_for_arg->out_pos_end = ptr_for_arg->max_out_pos_end;
    }
    else
    {
        int n;
        end_temp = (long long)(ptr_for_arg->in_pos_end - 2 - uv_loss)*prec -  (long long)ptr_for_arg->bias*prec - offset;
        n = (int)(end_temp/coeff_temp);
        if (((long long)n*coeff_temp) == end_temp)
        {
            n = n - 1;
        }
        if (((n & 0x1) == 1) || (CAM_UV_444_FLAG == ptr_for_arg->align_flag))/* YUV444 */
        {
            ptr_for_arg->out_pos_end = n;
        }
        else
        {
            ptr_for_arg->out_pos_end = n - 1;
        }
    }
    if (ptr_for_arg->out_pos_start > ptr_for_arg->out_pos_end)
    {
        result = ISP_MESSAGE_TP6_FOR_INVALID_OUT_XYS_XYE_ERROR;
        tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
        return result;
    }
    if (ptr_for_arg->out_pos_end > ptr_for_arg->max_out_pos_end)
    {
        ptr_for_arg->out_pos_end = ptr_for_arg->max_out_pos_end;
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_for_cub_acc(TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    int prec = (int)(1 << ptr_for_arg->prec_bits);
    long long start_temp;
    long long end_temp;
    int coeff_temp = ptr_for_arg->coeff_step;
    int offset = ((unsigned long long)ptr_for_arg->offset*coeff_temp)>>ptr_for_arg->config_bits;
    if (coeff_temp > prec)
    {
        result = ISP_MESSAGE_RESIZER_CUBIC_ACC_SCALING_UP_ERROR;
        tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
        return result;
    }
    /* cal pos */
    if (0 == ptr_for_arg->in_pos_start)
    {
        ptr_for_arg->out_pos_start = 0;
    }
    else
    {
        int n;
        start_temp = (long long)ptr_for_arg->in_pos_start*coeff_temp -
            (long long)ptr_for_arg->bias*coeff_temp - offset;
        n = (int)((unsigned long long)start_temp>>ptr_for_arg->prec_bits);
        if (((long long)n*prec) < start_temp)/* ceiling be careful with value smaller than zero */
        {
            n = n + 1;
        }
        n = n + 2;
        if (n < 0)
        {
            n = 0;
        }
        if ((0 == (n & 0x1)) || (CAM_UV_444_FLAG == ptr_for_arg->align_flag))/* YUV444 */
        {
            ptr_for_arg->out_pos_start = n;
        }
        else
        {
            ptr_for_arg->out_pos_start = n + 1;
        }
    }
    if (ptr_for_arg->in_pos_end == ptr_for_arg->max_in_pos_end)
    {
        ptr_for_arg->out_pos_end = ptr_for_arg->max_out_pos_end;
    }
    else
    {
        int n;
        end_temp = (long long)ptr_for_arg->in_pos_end*coeff_temp -
            (long long)ptr_for_arg->bias*coeff_temp - offset;
        n = (int)((unsigned long long)end_temp>>ptr_for_arg->prec_bits) - 2;
        if ((1 == (n & 0x1)) || (CAM_UV_444_FLAG == ptr_for_arg->align_flag))/* YUV444 */
        {
            ptr_for_arg->out_pos_end = n;
        }
        else
        {
            ptr_for_arg->out_pos_end = n - 1;
        }
    }
    if (ptr_for_arg->out_pos_start > ptr_for_arg->out_pos_end)
    {
        result = ISP_MESSAGE_CUB_ACC_FOR_INVALID_OUT_XYS_XYE_ERROR;
        tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
        return result;
    }
    if (ptr_for_arg->out_pos_end > ptr_for_arg->max_out_pos_end)
    {
        ptr_for_arg->out_pos_end = ptr_for_arg->max_out_pos_end;
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_for_mdp_cub_acc(TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    int prec = (int)(1 << ptr_for_arg->prec_bits);
    long long start_temp;
    long long end_temp;
    int coeff_temp = ptr_for_arg->coeff_step;
    int offset = ((unsigned long long)ptr_for_arg->offset*coeff_temp)>>ptr_for_arg->config_bits;
    if (coeff_temp > prec)
    {
        result = ISP_MESSAGE_RESIZER_CUBIC_ACC_SCALING_UP_ERROR;
        tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
        return result;
    }
    /* cal pos */
    if (0 == ptr_for_arg->in_pos_start)
    {
        ptr_for_arg->out_pos_start = 0;
    }
    else
    {
        int n;
        start_temp = (long long)ptr_for_arg->in_pos_start*coeff_temp -
            (long long)ptr_for_arg->bias*coeff_temp - offset;
        n = (int)((unsigned long long)start_temp>>ptr_for_arg->prec_bits);
        if (((long long)n*prec) < start_temp)/* ceiling be careful with value smaller than zero */
        {
            n = n + 1;
        }
        n = n + 3;
        if (n < 0)
        {
            n = 0;
        }
        if ((0 == (n & 0x1)) || (CAM_UV_444_FLAG == ptr_for_arg->align_flag))/* YUV444 */
        {
            ptr_for_arg->out_pos_start = n;
        }
        else
        {
            ptr_for_arg->out_pos_start = n + 1;
        }
    }
    if (ptr_for_arg->in_pos_end == ptr_for_arg->max_in_pos_end)
    {
        ptr_for_arg->out_pos_end = ptr_for_arg->max_out_pos_end;
    }
    else
    {
        int n;
        end_temp = (long long)ptr_for_arg->in_pos_end*coeff_temp -
            (long long)ptr_for_arg->bias*coeff_temp - offset;
        n = (int)((unsigned long long)end_temp>>ptr_for_arg->prec_bits) - 3;
        if ((1 == (n & 0x1)) || (CAM_UV_444_FLAG == ptr_for_arg->align_flag))/* YUV444 */
        {
            ptr_for_arg->out_pos_end = n;
        }
        else
        {
            ptr_for_arg->out_pos_end = n - 1;
        }
    }
    if (ptr_for_arg->out_pos_start > ptr_for_arg->out_pos_end)
    {
        result = ISP_MESSAGE_CUB_ACC_FOR_INVALID_OUT_XYS_XYE_ERROR;
        tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
        return result;
    }
    if (ptr_for_arg->out_pos_end > ptr_for_arg->max_out_pos_end)
    {
        ptr_for_arg->out_pos_end = ptr_for_arg->max_out_pos_end;
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_for_src_acc(TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    int prec = (int)(1 << ptr_for_arg->prec_bits);
    long long start_temp;
    long long end_temp;
    int coeff_temp = ptr_for_arg->coeff_step;
    int offset = ((unsigned long long)ptr_for_arg->offset*coeff_temp)>>ptr_for_arg->config_bits;
    if (coeff_temp > prec)
    {
        result = ISP_MESSAGE_RESIZER_SRC_ACC_SCALING_UP_ERROR;
        tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
        return result;
    }
    /* cal pos */
    if (0 == ptr_for_arg->in_pos_start)
    {
        ptr_for_arg->out_pos_start = 0;
    }
    else
    {
        int n;
        start_temp = (long long)ptr_for_arg->in_pos_start*coeff_temp - 
            (((unsigned int)coeff_temp)>>1) - (long long)ptr_for_arg->bias*coeff_temp - offset;
        n = (int)((unsigned long long)(start_temp*2 + prec)>>(ptr_for_arg->prec_bits + 1));
        if (((long long)n*2*prec) < (start_temp*2 + prec))/* ceiling be careful with value smaller than zero */
        {
            n = n + 1;
        }
        if (n < 0)
        {
            n = 0;
        }
        if (((n & 0x1) == 0) || (CAM_UV_444_FLAG == ptr_for_arg->align_flag))/* YUV444 */
        {
            ptr_for_arg->out_pos_start = n;
        }
        else
        {
            ptr_for_arg->out_pos_start = n + 1;
        }
    }
    if (ptr_for_arg->in_pos_end == ptr_for_arg->max_in_pos_end)
    {
        ptr_for_arg->out_pos_end = ptr_for_arg->max_out_pos_end;
    }
    else
    {
        int n;
        end_temp = (long long)ptr_for_arg->in_pos_end*coeff_temp +
            (((unsigned int)coeff_temp)>>1) - (long long)ptr_for_arg->bias*coeff_temp - offset;
        n = (int)((unsigned long long)(end_temp*2 - prec)>>(ptr_for_arg->prec_bits + 1));
        if (((n & 0x1) == 1) || (CAM_UV_444_FLAG == ptr_for_arg->align_flag))/* YUV444 */
        {
            ptr_for_arg->out_pos_end = n;
        }
        else
        {
            ptr_for_arg->out_pos_end = n - 1;
        }
    }
    if (ptr_for_arg->out_pos_start > ptr_for_arg->out_pos_end)
    {
        result = ISP_MESSAGE_SRC_ACC_FOR_INVALID_OUT_XYS_XYE_ERROR;
        tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
        return result;
    }
    if (ptr_for_arg->out_pos_end > ptr_for_arg->max_out_pos_end)
    {
        ptr_for_arg->out_pos_end = ptr_for_arg->max_out_pos_end;
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_back_8tp(TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg)
{
    int prec = (int)(1 << ptr_back_arg->prec_bits);
    int coeff_temp = ptr_back_arg->coeff_step;
    int offset = ((unsigned long long)ptr_back_arg->offset*prec)>>ptr_back_arg->config_bits;
    long long start_temp = (long long)ptr_back_arg->out_pos_start*coeff_temp +
         (long long)ptr_back_arg->bias*prec + offset;
    long long end_temp;
    int uv_loss = (CAM_UV_422_FLAG == ptr_back_arg->uv_flag)?2:0;
    if (start_temp < (long long)(3*prec))
    {
        ptr_back_arg->in_pos_start = 0;
    }
    else
    {
        int n;
        n = (int)((unsigned long long)start_temp>>ptr_back_arg->prec_bits) - 3;
		if (n > ptr_back_arg->max_in_pos_end)
		{
			n = ptr_back_arg->max_in_pos_end;
		}
        if ((0 == (n & 0x1)) || (CAM_UV_444_FLAG == ptr_back_arg->align_flag))/* YUV444 */
        {
            ptr_back_arg->in_pos_start = n ;
        }
        else/* must be even */
        {
            n = n - 1;
            ptr_back_arg->in_pos_start = n;
        }
    }
    end_temp = (long long)ptr_back_arg->out_pos_end*coeff_temp +
        (long long)ptr_back_arg->bias*prec + offset;
    if ((end_temp + (4 + uv_loss)*prec) > (long long)ptr_back_arg->max_in_pos_end*prec)
    {
        ptr_back_arg->in_pos_end = ptr_back_arg->max_in_pos_end;
    }
    else
    {
        int n;
        /* due to ceiling in forward */
        n = (int)((unsigned long long)(end_temp + (4 + uv_loss)*prec)>>ptr_back_arg->prec_bits);
        if ((1 == (n & 0x1)) || (CAM_UV_444_FLAG == ptr_back_arg->align_flag))/* YUV444 */
        {
            ptr_back_arg->in_pos_end = n;
        }
        else
        {
            ptr_back_arg->in_pos_end = n + 1;
        }
    }
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_back_6tp(TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg)
{
    int prec = (int)(1 << ptr_back_arg->prec_bits);
    int coeff_temp = ptr_back_arg->coeff_step;
    int offset = ((unsigned long long)ptr_back_arg->offset*prec)>>ptr_back_arg->config_bits;
    long long start_temp = (long long)ptr_back_arg->out_pos_start*coeff_temp +
        (long long)ptr_back_arg->bias*prec + offset;
    long long end_temp;
    int uv_loss = (CAM_UV_422_FLAG == ptr_back_arg->uv_flag)?2:0;
    /* cal pos */
    if (start_temp < (long long)(2*prec))
    {
        ptr_back_arg->in_pos_start = 0;
    }
    else
    {
        int n;
        n = (int)((unsigned long long)start_temp>>ptr_back_arg->prec_bits) - 2;
		if (n > ptr_back_arg->max_in_pos_end)
		{
			n = ptr_back_arg->max_in_pos_end;
		}
        if ((0 == (n & 0x1)) || (CAM_UV_444_FLAG == ptr_back_arg->align_flag))/* YUV444 */
        {
            ptr_back_arg->in_pos_start = n ;
        }
        else/* must be even */
        {
            n = n - 1;
            ptr_back_arg->in_pos_start = n;
        }
    }
    end_temp = (long long)ptr_back_arg->out_pos_end*coeff_temp + (long long)ptr_back_arg->bias*prec + offset;
    if ((end_temp + (3 + uv_loss)*prec) > (long long)ptr_back_arg->max_in_pos_end*prec)
    {
        ptr_back_arg->in_pos_end = ptr_back_arg->max_in_pos_end;
    }
    else
    {
        int n;
        /* due to ceiling in forward */
        n = (int)((unsigned long long)(end_temp + (3 + uv_loss)*prec)>>ptr_back_arg->prec_bits);
        if ((1 == (n & 0x1)) || (CAM_UV_444_FLAG == ptr_back_arg->align_flag))/* YUV444 */
        {
            ptr_back_arg->in_pos_end = n;
        }
        else
        {
            ptr_back_arg->in_pos_end = n + 1;
        }
    }
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_back_4tp(TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg)
{
    int prec = (int)(1 << ptr_back_arg->prec_bits);
    int coeff_temp = ptr_back_arg->coeff_step;
    int offset = ((unsigned long long)ptr_back_arg->offset*prec)>>ptr_back_arg->config_bits;
    long long start_temp = (long long)ptr_back_arg->out_pos_start*coeff_temp +
        (long long)ptr_back_arg->bias*prec + offset;
    long long end_temp;
    int uv_loss = (CAM_UV_422_FLAG == ptr_back_arg->uv_flag)?2:0;
    if (start_temp < (long long)prec)
    {
        ptr_back_arg->in_pos_start = 0;
    }
    else
    {
        int n;
        n = (int)((unsigned long long)start_temp>>ptr_back_arg->prec_bits) - 1;
		if (n > ptr_back_arg->max_in_pos_end)
		{
			n = ptr_back_arg->max_in_pos_end;
		}
        if ((0 == (n & 0x1)) || (CAM_UV_444_FLAG == ptr_back_arg->align_flag))/* YUV444 */
        {
            ptr_back_arg->in_pos_start = n ;
        }
        else/* must be even */
        {
            n = n - 1;
            ptr_back_arg->in_pos_start = n;
        }
    }
    end_temp = (long long)ptr_back_arg->out_pos_end*coeff_temp + (long long)ptr_back_arg->bias*prec + offset;
    if ((end_temp + (2 + uv_loss)*prec) > (long long)ptr_back_arg->max_in_pos_end*prec)
    {
        ptr_back_arg->in_pos_end = ptr_back_arg->max_in_pos_end;
    }
    else
    {
        int n;
        /* due to ceiling in forward */
        n = (int)((unsigned long long)(end_temp + (2 + uv_loss)*prec)>>ptr_back_arg->prec_bits);
        if ((1 == (n & 0x1)) || (CAM_UV_444_FLAG == ptr_back_arg->align_flag))/* YUV444 */
        {
            ptr_back_arg->in_pos_end = n;
        }
        else
        {
            ptr_back_arg->in_pos_end = n + 1;
        }
    }
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_back_5tp(TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg)
{
    int prec = (int)(1 << ptr_back_arg->prec_bits);
    int coeff_temp = ptr_back_arg->coeff_step;
    int offset = ((unsigned long long)ptr_back_arg->offset*prec)>>ptr_back_arg->config_bits;
    long long start_temp = (long long)ptr_back_arg->out_pos_start*coeff_temp +
        (long long)ptr_back_arg->bias*prec + offset;
    long long end_temp;
    int uv_loss = (CAM_UV_422_FLAG == ptr_back_arg->uv_flag)?2:0;
    if (start_temp < (long long)prec)
    {
        ptr_back_arg->in_pos_start = 0;
    }
    else
    {
        int n;
        n = (int)((unsigned long long)start_temp>>ptr_back_arg->prec_bits) - 2;
		if (n > ptr_back_arg->max_in_pos_end)
		{
			n = ptr_back_arg->max_in_pos_end;
		}
        if ((0 == (n & 0x1)) || (CAM_UV_444_FLAG == ptr_back_arg->align_flag))/* YUV444 */
        {
            ptr_back_arg->in_pos_start = n ;
        }
        else/* must be even */
        {
            n = n - 1;
            ptr_back_arg->in_pos_start = n;
        }
    }
    end_temp = (long long)ptr_back_arg->out_pos_end*coeff_temp + (long long)ptr_back_arg->bias*prec + offset;
    if ((end_temp + (2 + uv_loss)*prec) > (long long)ptr_back_arg->max_in_pos_end*prec)
    {
        ptr_back_arg->in_pos_end = ptr_back_arg->max_in_pos_end;
    }
    else
    {
        int n;
        /* due to ceiling in forward */
        n = (int)((unsigned long long)(end_temp + (2 + uv_loss)*prec)>>ptr_back_arg->prec_bits);
        if ((1 == (n & 0x1)) || (CAM_UV_444_FLAG == ptr_back_arg->align_flag))/* YUV444 */
        {
            ptr_back_arg->in_pos_end = n;
        }
        else
        {
            ptr_back_arg->in_pos_end = n + 1;
        }
    }
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_back_2tp(TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg)
{
    int prec = (int)(1 << ptr_back_arg->prec_bits);
    int coeff_temp = ptr_back_arg->coeff_step;
    int offset = ((unsigned long long)ptr_back_arg->offset*prec)>>ptr_back_arg->config_bits;
    long long start_temp = (long long)ptr_back_arg->out_pos_start*coeff_temp +
        (long long)ptr_back_arg->bias*prec + offset;
    long long end_temp;
    int uv_loss = (CAM_UV_422_FLAG == ptr_back_arg->uv_flag)?2:0;
    if (start_temp < 0)
    {
        ptr_back_arg->in_pos_start = 0;
    }
    else
    {
        int n;
        n = (int)((unsigned long long)start_temp>>ptr_back_arg->prec_bits);
		if (n > ptr_back_arg->max_in_pos_end)
		{
			n = ptr_back_arg->max_in_pos_end;
		}
        if ((0 == (n & 0x1)) || (CAM_UV_444_FLAG == ptr_back_arg->align_flag))/* YUV444 */
        {
            ptr_back_arg->in_pos_start = n ;
        }
        else/* must be even */
        {
            n = n - 1;
            ptr_back_arg->in_pos_start = n;
        }
    }
    end_temp = (long long)ptr_back_arg->out_pos_end*coeff_temp + (long long)ptr_back_arg->bias*prec + offset;
    if ((end_temp + (1 + uv_loss)*prec) > (long long)ptr_back_arg->max_in_pos_end*prec)
    {
        ptr_back_arg->in_pos_end = ptr_back_arg->max_in_pos_end;
    }
    else
    {
        int n;
        /* due to ceiling in forward */
        n = (int)((unsigned long long)(end_temp + (1 + uv_loss)*prec)>>ptr_back_arg->prec_bits);
        if ((1 == (n & 0x1)) || (CAM_UV_444_FLAG == ptr_back_arg->align_flag))/* YUV444 */
        {
            ptr_back_arg->in_pos_end = n;
        }
        else
        {
            ptr_back_arg->in_pos_end = n + 1;
        }
    }
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_back_mdp_6tp(TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg)
{
    int prec = (int)(1 << ptr_back_arg->prec_bits);
    int coeff_temp = ptr_back_arg->coeff_step;
    int offset = ((unsigned long long)ptr_back_arg->offset*prec)>>ptr_back_arg->config_bits;
    long long start_temp = (long long)ptr_back_arg->out_pos_start*coeff_temp +
        (long long)ptr_back_arg->bias*prec + offset;
    long long end_temp;
    int uv_loss = (CAM_UV_422_FLAG == ptr_back_arg->uv_flag)?2:0;
    /* cal pos */
    if (start_temp < (long long)(3*prec))
    {
        ptr_back_arg->in_pos_start = 0;
    }
    else
    {
        int n;
        n = (int)((unsigned long long)start_temp>>ptr_back_arg->prec_bits) - 3;
		if (n > ptr_back_arg->max_in_pos_end)
		{
			n = ptr_back_arg->max_in_pos_end;
		}
        if ((0 == (n & 0x1)) || (CAM_UV_444_FLAG == ptr_back_arg->align_flag))/* YUV444 */
        {
            ptr_back_arg->in_pos_start = n ;
        }
        else/* must be even */
        {
            n = n - 1;
            ptr_back_arg->in_pos_start = n;
        }
    }
    end_temp = (long long)ptr_back_arg->out_pos_end*coeff_temp +
        (long long)ptr_back_arg->bias*prec + offset;
    if ((end_temp + (3 + uv_loss)*prec) > (long long)ptr_back_arg->max_in_pos_end*prec)
    {
        ptr_back_arg->in_pos_end = ptr_back_arg->max_in_pos_end;
    }
    else
    {
        int n;
        /* due to ceiling in forward */
        n = (int)((unsigned long long)(end_temp + (3 + uv_loss)*prec)>>ptr_back_arg->prec_bits);
        if ((1 == (n & 0x1)) || (CAM_UV_444_FLAG == ptr_back_arg->align_flag))/* YUV444 */
        {
            ptr_back_arg->in_pos_end = n;
        }
        else
        {
            ptr_back_arg->in_pos_end = n + 1;
        }
    }
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_back_src_acc(TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    int prec = (int)(1 << ptr_back_arg->prec_bits);
    long long start_temp;
    long long end_temp;
    int coeff_temp = ptr_back_arg->coeff_step;
    int offset = ((unsigned long long)ptr_back_arg->offset*coeff_temp)>>ptr_back_arg->config_bits;
    ptr_back_arg->coeff_step = coeff_temp;
    if (coeff_temp > prec)
    {
        result = ISP_MESSAGE_RESIZER_SRC_ACC_SCALING_UP_ERROR;
        tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
        return result;
    }
    start_temp = (long long)ptr_back_arg->out_pos_start*prec +
        (long long)ptr_back_arg->bias*coeff_temp + offset;
    if (start_temp*2 + coeff_temp <= (long long)prec)
    {
        ptr_back_arg->in_pos_start = 0;
    }
    else
    {
        int n;
        n = (int)((start_temp*2 + coeff_temp - prec)/(2*coeff_temp));
        if ((0 == (n & 0x1)) || (CAM_UV_444_FLAG == ptr_back_arg->align_flag))/* YUV444 */
        {
            ptr_back_arg->in_pos_start = n ;
        }
        else/* must be even */
        {
            n = n - 1;
            ptr_back_arg->in_pos_start = n;
        }
    }
    end_temp = (long long)ptr_back_arg->out_pos_end*prec +
        (long long)ptr_back_arg->bias*coeff_temp + offset;
    if ((end_temp*2 + coeff_temp + prec) >= ((long long)2*coeff_temp*ptr_back_arg->max_in_pos_end))
    {
        ptr_back_arg->in_pos_end = ptr_back_arg->max_in_pos_end;
    }
    else
    {
        int n = (int)((end_temp*2 + coeff_temp + prec)/(2*coeff_temp));
        if (((long long)n*2*coeff_temp) == (end_temp*2 + coeff_temp + prec))
        {
            n = n - 1;
        }
        if ((1 == (n & 0x1)) || (CAM_UV_444_FLAG == ptr_back_arg->align_flag))/* YUV444 */
        {
            ptr_back_arg->in_pos_end = n;
        }
        else
        {
            ptr_back_arg->in_pos_end = n + 1;
        }
    }
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_back_cub_acc(TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    int prec = (int)(1 << ptr_back_arg->prec_bits);
    long long start_temp;
    long long end_temp;
    int coeff_temp = ptr_back_arg->coeff_step;
    int offset = ((unsigned long long)ptr_back_arg->offset*coeff_temp)>>ptr_back_arg->config_bits;
    if (coeff_temp > prec)
    {
        result = ISP_MESSAGE_RESIZER_CUBIC_ACC_SCALING_UP_ERROR;
        tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
        return result;
    }
    start_temp = (long long)ptr_back_arg->out_pos_start*prec +
        (long long)ptr_back_arg->bias*coeff_temp + offset;
    if (start_temp < (long long)(2*prec))
    {
        ptr_back_arg->in_pos_start = 0;
    }
    else
    {
        int n;      
        n = (int)((start_temp - 2*prec)/coeff_temp);
		if (n > ptr_back_arg->max_in_pos_end)
		{
			n = ptr_back_arg->max_in_pos_end;
		}
        if ((0 == (n & 0x1)) || (CAM_UV_444_FLAG == ptr_back_arg->align_flag))/* YUV444 */
        {
            ptr_back_arg->in_pos_start = n;
        }
        else/* not even */
        {
            n = n - 1;
            ptr_back_arg->in_pos_start = n; 
        }
    }
    end_temp = (long long)ptr_back_arg->out_pos_end*prec +
        (long long)ptr_back_arg->bias*coeff_temp + offset;
    if ((end_temp  + coeff_temp + 2*prec) >= (long long)coeff_temp*ptr_back_arg->max_in_pos_end)
    {
        ptr_back_arg->in_pos_end = ptr_back_arg->max_in_pos_end;
    }
    else
    {
        int n = (int)((end_temp + coeff_temp + 2*prec)/coeff_temp);
        if (((long long)n*coeff_temp) == (end_temp + coeff_temp + 2*prec))
        {
            n = n - 1;
        }
        if ((1 == (n & 0x1)) || (CAM_UV_444_FLAG == ptr_back_arg->align_flag))/* YUV444 */
        {
            ptr_back_arg->in_pos_end = n;
        }
        else/* not odd */
        {
            ptr_back_arg->in_pos_end = n + 1;
        }
    }
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_back_mdp_cub_acc(TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    int prec = (int)(1 << ptr_back_arg->prec_bits);
    long long start_temp;
    long long end_temp;
    int coeff_temp = ptr_back_arg->coeff_step;
    int offset = ((unsigned long long)ptr_back_arg->offset*coeff_temp)>>ptr_back_arg->config_bits;
    if (coeff_temp > prec)
    {
        result = ISP_MESSAGE_RESIZER_CUBIC_ACC_SCALING_UP_ERROR;
        tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
        return result;
    }
    start_temp = (long long)ptr_back_arg->out_pos_start*prec +
        (long long)ptr_back_arg->bias*coeff_temp + offset;
    if (start_temp < (long long)(3*prec))
    {
        ptr_back_arg->in_pos_start = 0;
    }
    else
    {
        int n;      
        n = (int)((start_temp - 3*prec)/coeff_temp);
		if (n > ptr_back_arg->max_in_pos_end)
		{
			n = ptr_back_arg->max_in_pos_end;
		}
        if ((0 == (n & 0x1)) || (CAM_UV_444_FLAG == ptr_back_arg->align_flag))/* YUV444 */
        {
            ptr_back_arg->in_pos_start = n;
        }
        else/* not even */
        {
            n = n - 1;
            ptr_back_arg->in_pos_start = n; 
        }
    }
    end_temp = (long long)ptr_back_arg->out_pos_end*prec +
        (long long)ptr_back_arg->bias*coeff_temp + offset;
    if ((end_temp  + coeff_temp + 3*prec) >= (long long)coeff_temp*ptr_back_arg->max_in_pos_end)
    {
        ptr_back_arg->in_pos_end = ptr_back_arg->max_in_pos_end;
    }
    else
    {
        int n = (int)((end_temp + coeff_temp + 3*prec)/coeff_temp);
        if (((long long)n*coeff_temp) == (end_temp + coeff_temp + 3*prec))
        {
            n = n - 1;
        }
        if ((1 == (n & 0x1)) || (CAM_UV_444_FLAG == ptr_back_arg->align_flag))/* YUV444 */
        {
            ptr_back_arg->in_pos_end = n;
        }
        else/* not odd */
        {
            ptr_back_arg->in_pos_end = n + 1;
        }
    }
    return ISP_MESSAGE_TILE_OK;
}

const char *tile_print_error_message(ISP_TILE_MESSAGE_ENUM n)
{
    GET_ERROR_NAME(n);
}
