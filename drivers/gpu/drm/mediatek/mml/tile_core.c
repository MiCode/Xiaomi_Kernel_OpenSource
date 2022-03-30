/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chris-YC Chen <chris-yc.chen@mediatek.com>
 */
#include "tile_driver.h"

/* lut function */
static ISP_TILE_MESSAGE_ENUM tile_init_func_run(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_for_func_run(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_back_func_run(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_schedule_backward(FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param);
static ISP_TILE_MESSAGE_ENUM tile_backward_comp(TILE_REG_MAP_STRUCT *ptr_tile_reg_map, FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param);
static ISP_TILE_MESSAGE_ENUM tile_backward_comp_min(TILE_REG_MAP_STRUCT *ptr_tile_reg_map, FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param);
static ISP_TILE_MESSAGE_ENUM tile_backward_comp_min_tile(TILE_REG_MAP_STRUCT *ptr_tile_reg_map, FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param);
static ISP_TILE_MESSAGE_ENUM tile_schedule_forward(FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param);
static ISP_TILE_MESSAGE_ENUM tile_forward_comp(TILE_REG_MAP_STRUCT *ptr_tile_reg_map, FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param);
static ISP_TILE_MESSAGE_ENUM tile_forward_by_func(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_forward_by_func_pre_x(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_forward_by_func_pre_x_inv(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_forward_by_func_pre_y(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_forward_by_func_post_x(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_forward_by_func_post_x_inv(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_forward_by_func_post_y(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_forward_comp_no_back(TILE_REG_MAP_STRUCT *ptr_tile_reg_map, FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param);
static ISP_TILE_MESSAGE_ENUM tile_forward_by_func_no_back(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_forward_by_func_no_back_pre_x(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_forward_by_func_no_back_pre_y(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_forward_by_func_no_back_post_x(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_forward_by_func_no_back_post_y(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_backward_by_func_pre_x(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_backward_by_func_pre_y(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_backward_by_func(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_backward_by_func_post_x(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_backward_by_func_post_x_inv(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_backward_by_func_post_y(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_backward_output_config(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
													FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param);
static ISP_TILE_MESSAGE_ENUM tile_backward_output_config_skip(TILE_REG_MAP_STRUCT *ptr_tile_reg_map, FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param);
static ISP_TILE_MESSAGE_ENUM tile_backward_output_config_x(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
													FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param);
static ISP_TILE_MESSAGE_ENUM tile_backward_output_config_x_inv(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
														  FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param);
static ISP_TILE_MESSAGE_ENUM tile_backward_output_config_y(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
														  FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param);
static ISP_TILE_MESSAGE_ENUM tile_backward_output_config_min_tile(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
															 FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param);
static ISP_TILE_MESSAGE_ENUM tile_backward_output_config_x_min_tile(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
															 FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param);
static ISP_TILE_MESSAGE_ENUM tile_backward_output_config_x_inv_min_tile(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
															 FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param);
static ISP_TILE_MESSAGE_ENUM tile_backward_output_config_y_min_tile(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
															 FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param);
static ISP_TILE_MESSAGE_ENUM tile_forward_input_config(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
												  FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param);
static ISP_TILE_MESSAGE_ENUM tile_backward_input_check(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_forward_output_check(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_forward_recusive_check(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
													FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param, bool *ptr_restart_flag);
static ISP_TILE_MESSAGE_ENUM tile_check_input_config(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_check_output_config(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_check_output_config_x(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_check_output_config_x_inv(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_check_output_config_y(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_update_last_x_y(TILE_REG_MAP_STRUCT *ptr_tile_reg_map, FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param,
											 bool x_end_flag, bool y_end_flag);
static ISP_TILE_MESSAGE_ENUM tile_compare_forward_back(TILE_REG_MAP_STRUCT *ptr_tile_reg_map, FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param);
static ISP_TILE_MESSAGE_ENUM tile_init_by_prev(TILE_FUNC_BLOCK_STRUCT *ptr_func, FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param);
/* prototype lib */
static int tile_cal_lcm(int a, int b);
/* diff view */
static ISP_TILE_MESSAGE_ENUM tile_check_min_tile(TILE_REG_MAP_STRUCT *ptr_tile_reg_map, FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param);
static ISP_TILE_MESSAGE_ENUM tile_check_valid_output(TILE_REG_MAP_STRUCT *ptr_tile_reg_map, FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param);
static ISP_TILE_MESSAGE_ENUM tile_init_tdr_ctrl_flag(TILE_REG_MAP_STRUCT *ptr_tile_reg_map, FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param);
static ISP_TILE_MESSAGE_ENUM tile_backward_min_tile_backup_input(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_backward_min_tile_init(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_backward_min_tile_restore(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map);
/* check end position with end flag after computation of all tiles are done */
static ISP_TILE_MESSAGE_ENUM tile_check_x_end_pos_with_flag(TILE_REG_MAP_STRUCT *ptr_tile_reg_map, FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param,
													   bool *ptr_x_end_flag, int curr_tile_no);
static ISP_TILE_MESSAGE_ENUM tile_check_y_end_pos_with_flag(TILE_REG_MAP_STRUCT *ptr_tile_reg_map, FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param,
													   bool *ptr_y_end_flag);

const char *tile_print_error_message(ISP_TILE_MESSAGE_ENUM err)
{
	const char *name;

	GET_ERROR_NAME(name, err);
	return name;
}

static ISP_TILE_MESSAGE_ENUM tile_init_func_run(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
	return ptr_func->init_func ?
		ptr_func->init_func(ptr_func, ptr_tile_reg_map) :
		ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_for_func_run(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
	return ptr_func->for_func ?
		ptr_func->for_func(ptr_func, ptr_tile_reg_map) :
		ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_back_func_run(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
	return ptr_func->back_func ?
		ptr_func->back_func(ptr_func, ptr_tile_reg_map) :
		ISP_MESSAGE_TILE_OK;
}

/* core api */
ISP_TILE_MESSAGE_ENUM tile_convert_func(TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
	FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param,
	const struct mml_topology_path *path)
{
	ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
	int module_no, start_count;
	int i;

	/* reset ptr_tile_func_param */
	ptr_tile_func_param->for_recursive_count = 0;

	module_no = path->tile_engine_cnt;
	for (i = 0; i < module_no; i++) {
		const struct mml_path_node *node = &path->nodes[path->tile_engines[i]];
		struct tile_func_block *ptr_func = ptr_tile_func_param->func_list[i];

		memset(ptr_func, 0x0, sizeof(*ptr_func));

		ptr_func->func_num = node->id;
		snprintf(ptr_func->func_name, sizeof(ptr_func->func_name),
			 "%d %s", node->id, node->comp->name);
		ptr_func->run_mode = TILE_RUN_MODE_MAIN;
		ptr_func->enable_flag = true;

		ptr_func->tot_prev_num = 1;
		if (node->prev)
			ptr_func->last_func_num[0] = node->prev->id;
		else
			ptr_func->last_func_num[0] = LAST_MODULE_ID_OF_START;

		ptr_func->in_const_x = 1;
		ptr_func->in_const_y = 1;
		ptr_func->out_const_x = 1;
		ptr_func->out_const_y = 1;
	}
	ptr_tile_func_param->used_func_no = module_no;

	start_count = 0;
	/* check valid module no */
	if (module_no < MIN_TILE_FUNC_NO) {
		result = ISP_MESSAGE_UNDER_MIN_TILE_FUNC_NO_ERROR;
		tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
		return result;
	}
	/* connect modules with error check */
	for (i = 0; i < module_no; i++) {
		TILE_FUNC_BLOCK_STRUCT *ptr_func = ptr_tile_func_param->func_list[i];
		int j;
		int tot_prev_num = ptr_func->tot_prev_num;

		for (j = 0; j < tot_prev_num; j++) {
			int last_func_num = ptr_func->last_func_num[j];

			if (LAST_MODULE_ID_OF_START == last_func_num) {
				/* skip start module */
				ptr_func->prev_blk_num[j] = PREVIOUS_BLK_NO_OF_START;
				start_count++;
				/* valid input no */
				if (start_count > MAX_INPUT_TILE_FUNC_NO) {
					result = ISP_MESSAGE_OVER_MAX_INPUT_TILE_FUNC_NO_ERROR;
					tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
					return result;
				}
				if (j > 0) {
					result = ISP_MESSAGE_TWO_START_PREV_ERROR;
					tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
					return result;
				}
			} else {
				int k;
				bool found_flag = false;

				/* check duplicated last func */
				for (k = j+1; k < tot_prev_num; k++) {
					if (last_func_num == ptr_func->last_func_num[k]) {
						tile_driver_printf("Found duplicated func id: %d, %s, duplicated last func: %d, idx: %d, %d\r\n",
							ptr_func->func_num, ptr_func->func_name, last_func_num, j, k);\
						result = ISP_MESSAGE_DUPLICATED_SUPPORT_FUNC_ERROR;
						tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
						return result;
					}
				}
				for (k = 0; k < module_no; k++) {
					if (i != k) { /* skip self */
						TILE_FUNC_BLOCK_STRUCT *ptr_target =
							ptr_tile_func_param->func_list[k];

						/* find last */
						if (last_func_num == ptr_target->func_num) {
							found_flag = true;
							ptr_func->prev_blk_num[j] = k;
							/* valid branch no */
							if (ptr_target->tot_branch_num < MAX_TILE_BRANCH_NO) {
								ptr_target->next_blk_num[ptr_target->tot_branch_num] = i;
								ptr_target->next_func_num[ptr_target->tot_branch_num] = ptr_func->func_num;
								ptr_target->tot_branch_num++;
							} else {
								/* over max buffer size */
								ptr_target->tot_branch_num++;
								result = ISP_MESSAGE_OVER_MAX_BRANCH_NO_ERROR;
								tile_driver_printf("Error [%s][%s] %s\r\n", ptr_func->func_name, ptr_target->func_name, tile_print_error_message(result));
								return result;
							}
							break;
						}
					}
				}
				if (false == found_flag) {
					tile_driver_printf("Cannot find func: %d, %s, prev ip no: %d, no lut func: %d\r\n", ptr_func->func_num, ptr_func->func_name, j, last_func_num);
					result = ISP_MESSAGE_TILE_FUNC_CANNOT_FIND_LAST_FUNC_ERROR;
					tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
					return result;
				}
			}
		}
	}
	return result;
}

ISP_TILE_MESSAGE_ENUM tile_proc_main_single(TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
	FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param,
	int tile_no, bool *stop_flag)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    int max_loop_count = MAX_TILE_TOT_NO;
    /* update loop count */
    if (tile_no >= max_loop_count)
    {
        result = ISP_MESSAGE_OVER_MAX_TILE_TOT_NO_ERROR;
        tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
    }
    /* to run single tile */
    if (ISP_MESSAGE_TILE_OK == result)
	{
		result = tile_backward_output_config_skip(ptr_tile_reg_map, ptr_tile_func_param);
	}
	/* diff view support */
    if (ISP_MESSAGE_TILE_OK == result)
    {
		result = tile_init_tdr_ctrl_flag(ptr_tile_reg_map, ptr_tile_func_param);
		if (ISP_MESSAGE_TILE_OK == result)
		{
			result = tile_backward_comp_min(ptr_tile_reg_map, ptr_tile_func_param);
		}
		if (ISP_MESSAGE_TILE_OK == result)
		{
			/* min tile check backward */
			result = tile_check_min_tile(ptr_tile_reg_map, ptr_tile_func_param);
		}
	}
    if (ISP_MESSAGE_TILE_OK == result)
    {
		result = tile_backward_comp(ptr_tile_reg_map, ptr_tile_func_param);
	}
	if (ISP_MESSAGE_TILE_OK == result)
	{
		result = tile_forward_comp(ptr_tile_reg_map, ptr_tile_func_param);
	}
    if (ISP_MESSAGE_TILE_OK == result)
    {
		result = tile_check_valid_output(ptr_tile_reg_map, ptr_tile_func_param);
	}
	/* diff view support */
	if (ISP_MESSAGE_TILE_OK == result)
	{
		/* min tile check forward */
		result = tile_check_min_tile(ptr_tile_reg_map, ptr_tile_func_param);
	}
    /* multi-input cal */
    if (ISP_MESSAGE_TILE_OK == result)
    {
        /* run sub in found */
        if (ptr_tile_reg_map->found_sub_in)
        {
            /* config sub in mode */
            ptr_tile_reg_map->run_mode = TILE_RUN_MODE_SUB_IN;
			if (ISP_MESSAGE_TILE_OK == result)
			{
				result = tile_backward_comp(ptr_tile_reg_map, ptr_tile_func_param);
			}
            if (ISP_MESSAGE_TILE_OK == result)
            {
                result = tile_forward_comp(ptr_tile_reg_map, ptr_tile_func_param);
            }
            if (ISP_MESSAGE_TILE_OK == result)
            {
                result = tile_compare_forward_back(ptr_tile_reg_map, ptr_tile_func_param);
            }
            /* restore mode */
            ptr_tile_reg_map->run_mode = TILE_RUN_MODE_MAIN;
        }
    }
    if (ISP_MESSAGE_TILE_OK == result)
    {
        /* run sub out found */
        if (ptr_tile_reg_map->found_sub_out)
        {
            /* config sub in mode */
            ptr_tile_reg_map->run_mode = TILE_RUN_MODE_SUB_OUT;
            result = tile_forward_comp_no_back(ptr_tile_reg_map, ptr_tile_func_param);
            /* restore mode */
            ptr_tile_reg_map->run_mode = TILE_RUN_MODE_MAIN;
        }
    }
    if (ISP_MESSAGE_TILE_OK == result)
    {
        /* check tile end & update tile no */
		bool x_end_flag = ptr_tile_func_param->func_list[
			ptr_tile_reg_map->first_func_en_no]->h_end_flag;
		bool y_end_flag = ptr_tile_func_param->func_list[
			ptr_tile_reg_map->first_func_en_no]->v_end_flag;
        result = tile_check_x_end_pos_with_flag(ptr_tile_reg_map, ptr_tile_func_param, &x_end_flag, tile_no);
        if (ISP_MESSAGE_TILE_OK == result)
        {
            result = tile_check_y_end_pos_with_flag(ptr_tile_reg_map, ptr_tile_func_param, &y_end_flag);
        }
        /* record tile coordinate */
        if (ISP_MESSAGE_TILE_OK == result)
        {
            /* to backup func property before curr_horizontal_tile_no increase */
			result = tile_update_last_x_y(ptr_tile_reg_map, ptr_tile_func_param, x_end_flag, y_end_flag);
			ptr_tile_reg_map->used_tile_no++;
            if (ISP_MESSAGE_TILE_OK == result)
            {
				/* end loop found */
				if ((true == y_end_flag) && (true == x_end_flag))
				{
					*stop_flag = true;
					/* set default valid_tile_no */
					ptr_tile_reg_map->valid_tile_no = ptr_tile_reg_map->used_tile_no;
				}
				else
				{
					if (ptr_tile_reg_map->first_frame)
					{
						result = ISP_MESSAGE_FRAME_MODE_NOT_END_ERROR;
						tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
					}
					if (ISP_MESSAGE_TILE_OK == result)
					{
						if (tile_no + 1 >= max_loop_count)
						{
							result = ISP_MESSAGE_OVER_MAX_TILE_TOT_NO_ERROR;
							tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
						}
					}
				}
			}
		}
	}
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_init_by_prev(TILE_FUNC_BLOCK_STRUCT *ptr_func, FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    int j;
    bool found_prev = false;
    for (j=0;j<ptr_func->tot_prev_num;j++)
    {
		TILE_FUNC_BLOCK_STRUCT *ptr_prev =
			ptr_tile_func_param->func_list[ptr_func->prev_blk_num[j]];
        /* update only for main path */
        if (false == ptr_prev->output_disable_flag)
        {
            if (found_prev)
            {
                if ((ptr_func->full_size_x_in != ptr_prev->full_size_x_out) ||
                    (ptr_func->full_size_y_in != ptr_prev->full_size_y_out)||
                    (ptr_func->in_stream_order != ptr_prev->out_stream_order)||
                    (ptr_func->in_cal_order != ptr_prev->out_cal_order)||
                    (ptr_func->in_dump_order != ptr_prev->out_dump_order))
                {
                    result = ISP_MESSAGE_DIFF_PREV_CONFIG_ERROR;
                    tile_driver_printf("Error [%s][%s] %s\r\n", ptr_func->func_name, ptr_prev->func_name, tile_print_error_message(result));
                    return result;
                }
            }
            else
            {
                /* skip call init function */
                /* force init size for disable func */
                ptr_func->in_pos_xs = 0;
                ptr_func->in_pos_ys = 0;
                ptr_func->in_pos_xe = ptr_prev->full_size_x_out - 1;
                ptr_func->in_pos_ye = ptr_prev->full_size_y_out - 1;
                ptr_func->full_size_x_in = ptr_prev->full_size_x_out;
                ptr_func->full_size_y_in = ptr_prev->full_size_y_out;
                /* check cal order, must init for disabled function too */
                TILE_COPY_PRE_ORDER(ptr_func, ptr_prev);
                found_prev = true;
            }
        }
    }

    return result;
}

ISP_TILE_MESSAGE_ENUM tile_init_config(TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
										 FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    int module_no = ptr_tile_func_param->used_func_no;
    /* update scheduling backward order */
    result = tile_schedule_backward(ptr_tile_func_param);
    /* update scheduling forward order */
    if (ISP_MESSAGE_TILE_OK == result)
    {
        result = tile_schedule_forward(ptr_tile_func_param);
    }
    /* check enable & output disable by backward */
    if (ISP_MESSAGE_TILE_OK == result)
    {
		int i;
        bool input_enable_flag = false;
        bool output_enable_flag = false;
        for (i=0;i<module_no;i++)
        {
            unsigned char module_order = ptr_tile_func_param->scheduling_backward_order[i];
				TILE_FUNC_BLOCK_STRUCT *ptr_func =
					ptr_tile_func_param->func_list[module_order];
            if (false == ptr_func->output_disable_flag)
            {
                /* trace output disable */
                if (0 == ptr_func->tot_branch_num)/* end func */
                {
					if (ptr_func->type & TILE_TYPE_WDMA)
					{
						if (false == ptr_func->enable_flag)
						{
							ptr_func->output_disable_flag = true;
						}
						else
						{
							/* init crop param */
							ptr_func->crop_bias_x = 0;
							ptr_func->crop_offset_x = 0;
							ptr_func->crop_bias_y = 0;
							ptr_func->crop_offset_y = 0;
						}
					}
					else if (PREVIOUS_BLK_NO_OF_START != ptr_func->prev_blk_num[0])
					{
						ptr_func->output_disable_flag = true;
                    }
                }
                else/* not end func */
                {
                    int j;
                    for (j=0;j<ptr_func->tot_branch_num;j++)
                    {
					TILE_FUNC_BLOCK_STRUCT *ptr_next =
						ptr_tile_func_param->func_list[
						ptr_func->next_blk_num[j]];
                        /* find out branch enabled output */
                        if (false == ptr_next->output_disable_flag)
                        {
                            ptr_func->output_disable_flag = false;
                            break;
                        }
                        else
                        {
                            ptr_func->output_disable_flag = true;
                        }
                    }
					if (false == ptr_func->output_disable_flag)
					{
						if (PREVIOUS_BLK_NO_OF_START == ptr_func->prev_blk_num[0])
						{
							if (false == input_enable_flag)
							{
								/* start func disabled */
								if (ptr_func->enable_flag)
								{
									input_enable_flag = true;
								}
							}
						}
						else
						{
							if (0 == ptr_func->tot_prev_num)
							{
								result = ISP_MESSAGE_TILE_FUNC_CANNOT_FIND_LAST_FUNC_ERROR;
								tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
								return result;
							}
						}
					}
                }
            }
        }
        if (false == input_enable_flag)
        {
            result = ISP_MESSAGE_OUTPUT_DISABLE_INPUT_FUNC_CHECK_ERROR;
            tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
            return result;
        }
        /* check RDMA disabled again to disable path by forward */
		for (i=0;i<module_no;i++)
		{
			unsigned char module_order = ptr_tile_func_param->scheduling_forward_order[i];
					TILE_FUNC_BLOCK_STRUCT *ptr_func =
						ptr_tile_func_param->func_list[module_order];
			if (false == ptr_func->output_disable_flag)
			{
				/* start func disabled */
				if (PREVIOUS_BLK_NO_OF_START == ptr_func->prev_blk_num[0])
				{
					if (false == ptr_func->enable_flag)
					{
						ptr_func->output_disable_flag = true;
					}
				}
				else
				{
					int j;
					bool input_enable_count = false;
					for (j=0;j<ptr_func->tot_prev_num;j++)
					{
						TILE_FUNC_BLOCK_STRUCT *ptr_prev =
							ptr_tile_func_param->func_list[
							ptr_func->prev_blk_num[j]];
						/* find input enabled */
						if (false == ptr_prev->output_disable_flag)
						{
							input_enable_count =  true;
							/* early stop */
							break;
						}
					}
					if (false == input_enable_count)
					{
						ptr_func->output_disable_flag = true;
					}
				}
				if (false == output_enable_flag)
				{
					if (0 == ptr_func->tot_branch_num)/* end func */
					{
						if ((false == ptr_func->output_disable_flag) && ptr_func->enable_flag)
						{
							output_enable_flag = true;
						}
					}
				}
			}
		}
		if (false == output_enable_flag)
		{
			result = ISP_MESSAGE_OUTPUT_DISABLE_INPUT_FUNC_CHECK_ERROR;
			tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
			return result;
		}
    }
    if (ISP_MESSAGE_TILE_OK == result)
    {
        int i;
        /* init full size in & out at same time */
        for (i=0;i<module_no;i++)
        {
            if (ISP_MESSAGE_TILE_OK == result)
            {
                unsigned char module_order = ptr_tile_func_param->scheduling_forward_order[i];
				TILE_FUNC_BLOCK_STRUCT *ptr_func =
					ptr_tile_func_param->func_list[module_order];
                if (ptr_func->output_disable_flag)
                {
                    if (ISP_MESSAGE_TILE_OK == result)
                    {
						if (ptr_func->enable_flag)
						{
							/* start func */
							if (PREVIOUS_BLK_NO_OF_START == ptr_func->prev_blk_num[0])
							{
								if (0 == (TILE_TYPE_RDMA & ptr_func->type))
								{
									result = ISP_MESSAGE_INCORRECT_START_FUNC_TYPE_ERROR;
									tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
									return result;
								}
							}
							else if (0 == ptr_func->tot_branch_num)
							{
								/* end func */
								if (0 == (TILE_TYPE_WDMA & ptr_func->type))
								{
									result = ISP_MESSAGE_INCORRECT_END_FUNC_TYPE_ERROR;
									tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
									return result;
								}
							}
						}
					}
				}
                else/* skip output disable func in following check */
                {
                    /* cal input & output size */
                    if (ISP_MESSAGE_TILE_OK == result)
                    {
                        /* reset ptr_func property here */
						ptr_func->valid_v_no = 0;
						ptr_func->valid_h_no = 0;
						ptr_func->last_valid_tile_no = 0;
						ptr_func->last_valid_v_no = 0;
						ptr_func->tdr_h_disable_flag = false;
						ptr_func->tdr_v_disable_flag = false;
						/* set size & init disable func */
                        if (false == ptr_func->enable_flag)
                        {
                            /* clear tile size */\
                            ptr_func->in_tile_width = 0;
                            ptr_func->in_max_width = 0;
                            ptr_func->in_tile_height = 0;
                            ptr_func->in_max_height = 0;
                            ptr_func->out_tile_width = 0;
                            ptr_func->out_max_width = 0;
                            ptr_func->out_tile_height = 0;
                            ptr_func->out_max_height = 0;
                            ptr_func->in_const_x = 1;
                            ptr_func->in_const_y = 1;
                            ptr_func->out_const_x = 1;
                            ptr_func->out_const_y = 1;
							/* mask TILE_TYPE_CROP_EN type */
							ptr_func->type &= ~TILE_TYPE_CROP_EN;
							result = tile_init_by_prev(ptr_func, ptr_tile_func_param);
                            if (ISP_MESSAGE_TILE_OK != result)
							{
								return result;
							}
                        }
                        else
                        {
                            /* set size for enabled func */
                            if (PREVIOUS_BLK_NO_OF_START == ptr_func->prev_blk_num[0])/* start func */
                            {
                                /* check call order, copy src in to first func, before init */
                                TILE_COPY_SRC_ORDER(ptr_func, ptr_tile_reg_map);
                                /* run init func ptr for start func */
                                result = tile_init_func_run(ptr_func, ptr_tile_reg_map);
                                /* set input x y size & pos of start func */
                                if(ISP_MESSAGE_TILE_OK == result)
                                {
                                    ptr_func->in_pos_xs = 0;
                                    ptr_func->in_pos_ys = 0;
                                    ptr_func->in_pos_xe = ptr_func->full_size_x_in - 1;
                                    ptr_func->in_pos_ye = ptr_func->full_size_y_in - 1;
                                }
                                else
                                {
                                    return result;
                                }
                                if (0 == (TILE_TYPE_RDMA & ptr_func->type))
                                {
                                    result = ISP_MESSAGE_INCORRECT_START_FUNC_TYPE_ERROR;
                                    tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                                    return result;
                                }
                                /* check call order, hw constraint */
                                if (ptr_func->in_stream_order & TILE_ORDER_RIGHT_TO_LEFT)
                                {
                                    result = ISP_MESSAGE_INCORRECT_ORDER_CONFIG_ERROR;
                                    tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                                    return result;
                                }
                            }
                            else/* intermediate or end func */
                            {
                                result = tile_init_by_prev(ptr_func, ptr_tile_func_param);
                                if (ISP_MESSAGE_TILE_OK != result)
								{
									return result;
								}
                                /* run init func ptr for intermediate or end func */
                                result = tile_init_func_run(ptr_func, ptr_tile_reg_map);
                                if(ISP_MESSAGE_TILE_OK == result)
                                {
                                    if (0 == ptr_func->tot_branch_num)
                                    {
										if (0 == (TILE_TYPE_WDMA & ptr_func->type))
                                        {
                                            result = ISP_MESSAGE_INCORRECT_END_FUNC_TYPE_ERROR;
                                            tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                                            return result;
                                        }
                                    }
                                }
                                else
                                {
                                    return result;
                                }
                            }
                            if(ISP_MESSAGE_TILE_OK == result)
                            {
                                /* check call order, x flip check, xor, in & out */
                                if ((ptr_func->in_stream_order & TILE_ORDER_BOTTOM_TO_TOP) !=
                                    (ptr_func->out_stream_order & TILE_ORDER_BOTTOM_TO_TOP))
                                {
                                    result = ISP_MESSAGE_INCORRECT_ORDER_CONFIG_ERROR;
                                    tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                                    return result;
                                }
                                /* check call order, x flip check, xor, in & out */
                                if ((ptr_func->in_stream_order & TILE_ORDER_RIGHT_TO_LEFT) ||
                                    (ptr_func->out_stream_order & TILE_ORDER_RIGHT_TO_LEFT))
                                {
                                    result = ISP_MESSAGE_INCORRECT_ORDER_CONFIG_ERROR;
                                    tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                                    return result;
                                }
                                /* check call order, in & out check, commom */
                                if ((ptr_func->in_cal_order & TILE_ORDER_Y_FIRST) ||
                                    (ptr_func->out_cal_order & TILE_ORDER_Y_FIRST))
                                {
                                    result = ISP_MESSAGE_INCORRECT_ORDER_CONFIG_ERROR;
                                    tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                                    return result;
                                }
                                /* check call order, hw constraint */
                                if ((ptr_func->in_cal_order & TILE_ORDER_BOTTOM_TO_TOP) ||
									(ptr_func->out_cal_order & TILE_ORDER_BOTTOM_TO_TOP))
                                {
                                    result = ISP_MESSAGE_INCORRECT_ORDER_CONFIG_ERROR;
                                    tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                                    return result;
                                }
                                /* check call order, x flip check, xor, in & out */
                                if ((ptr_func->in_cal_order & TILE_ORDER_RIGHT_TO_LEFT) !=
                                    (ptr_func->out_cal_order & TILE_ORDER_RIGHT_TO_LEFT))
                                {
                                    result = ISP_MESSAGE_INCORRECT_ORDER_CONFIG_ERROR;
                                    tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                                    return result;
                                }
                                /* check call order, x flip check, xor, in & out */
                                if ((ptr_func->in_dump_order & TILE_ORDER_RIGHT_TO_LEFT) !=
                                    (ptr_func->out_dump_order & TILE_ORDER_RIGHT_TO_LEFT))
                                {
                                    result = ISP_MESSAGE_INCORRECT_ORDER_CONFIG_ERROR;
                                    tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                                    return result;
                                }
                                /* check call order, x flip check, xor, in & out */
                                if ((ptr_func->in_dump_order & TILE_ORDER_BOTTOM_TO_TOP) !=
                                    (ptr_func->out_dump_order & TILE_ORDER_BOTTOM_TO_TOP))
                                {
                                    result = ISP_MESSAGE_INCORRECT_ORDER_CONFIG_ERROR;
                                    tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                                    return result;
                                }
                                /* check call order, x flip check, xor, in & out */
                                if ((ptr_func->in_dump_order & TILE_ORDER_Y_FIRST) !=
                                    (ptr_func->out_dump_order & TILE_ORDER_Y_FIRST))
                                {
                                    result = ISP_MESSAGE_INCORRECT_ORDER_CONFIG_ERROR;
                                    tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                                    return result;
                                }
                            }
                        }
                    }
                    /* check input x y size & pos */
                    /* TBD - input check can be removed */
                    if (ISP_MESSAGE_TILE_OK == result)
                    {
                        /* backup for next error check */
                        ptr_func->last_input_xe_pos = ptr_func->in_pos_xe;
                        ptr_func->last_input_ye_pos = ptr_func->in_pos_ye;
                        ptr_func->last_input_xs_pos = ptr_func->in_pos_xs;
                        ptr_func->last_input_ys_pos = ptr_func->in_pos_ys;
                        result = tile_check_input_config(ptr_func, ptr_tile_reg_map);
                    }
                    if (ISP_MESSAGE_TILE_OK == result)
                    {
			/* set output size */
			if (!ptr_func->init_func
			    || !ptr_func->enable_flag) {
				/* copy for null init func */
				ptr_func->out_pos_xs = ptr_func->in_pos_xs;
				ptr_func->out_pos_ys = ptr_func->in_pos_ys;
				ptr_func->out_pos_xe = ptr_func->in_pos_xe;
				ptr_func->out_pos_ye = ptr_func->in_pos_ye;
				ptr_func->full_size_x_out = ptr_func->full_size_x_in;
				ptr_func->full_size_y_out = ptr_func->full_size_y_in;
                        }
                        else
                        {
                            /* check output size initialized by init func ptr */
                            if ((ptr_func->full_size_x_out > 0) && (ptr_func->full_size_y_out > 0))
                            {
                                /* init with desired output size & skip forward */
                                ptr_func->out_pos_xs = 0;
                                ptr_func->out_pos_ys = 0;
                                ptr_func->out_pos_xe = ptr_func->full_size_x_out - 1;
                                ptr_func->out_pos_ye = ptr_func->full_size_y_out - 1;
                            }
                            else if ((ptr_func->full_size_y_out > 0) && (ptr_func->full_size_x_out <= 0))
                            {
                                /* error with incorrect x output size */
                                result = ISP_MESSAGE_INIT_INCORRECT_X_OUTPUT_SIZE_POS_ERROR;
                                tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                                return result;
                            }
                            else if ((ptr_func->full_size_x_out <= 0) && (ptr_func->full_size_y_out > 0))
                            {
                                /* error with incorrect y output size */
                                result = ISP_MESSAGE_INIT_INCORRECT_Y_OUTPUT_SIZE_POS_ERROR;
                                tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                                return result;
                            }
                            else
                            {
                                /* copy for non-initialized output size by init func */
                                ptr_func->out_pos_xs = ptr_func->in_pos_xs;
                                ptr_func->out_pos_ys = ptr_func->in_pos_ys;
                                ptr_func->out_pos_xe = ptr_func->in_pos_xe;
                                ptr_func->out_pos_ye = ptr_func->in_pos_ye;
                                ptr_func->full_size_x_out = ptr_func->full_size_x_in;
                                ptr_func->full_size_y_out = ptr_func->full_size_y_in;
                            }
                        }
                    }
                    if (ISP_MESSAGE_TILE_OK == result)
                    {
                        /* backup for next error check */
                        /* TBD start - output check can be removed */
                        ptr_func->last_output_xe_pos = ptr_func->out_pos_xe;
                        ptr_func->last_output_ye_pos = ptr_func->out_pos_ye;
                        ptr_func->last_output_xs_pos = ptr_func->out_pos_xs;
                        ptr_func->last_output_ys_pos = ptr_func->out_pos_ys;
                        /* TBD end - output check can be removed */
                        ptr_func->min_out_pos_xs = 0;
                        ptr_func->max_out_pos_xe = ptr_func->full_size_x_out - 1;
                        ptr_func->min_out_pos_ys = 0;
                        ptr_func->max_out_pos_ye = ptr_func->full_size_y_out - 1;
                        /* check output x y size & pos */
                        /* TBD start - output check can be removed */
                        result = tile_check_output_config(ptr_func, ptr_tile_reg_map);
                        /* TBD end - output check can be removed */
                    }
                    if (ISP_MESSAGE_TILE_OK == result)
                    {
                        /* check size of disable func */
                        if (false == ptr_func->enable_flag)
                        {
                            if (ptr_func->full_size_x_in != ptr_func->full_size_x_out)
                            {
                                result = ISP_MESSAGE_DISABLE_FUNC_X_SIZE_CHECK_ERROR;
                                tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                                return result;
                            }
                            if (ptr_func->full_size_y_in != ptr_func->full_size_y_out)
                            {
                                result = ISP_MESSAGE_DISABLE_FUNC_Y_SIZE_CHECK_ERROR;
                                tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                                return result;
                            }
                        }
                    }
                    /* check alignment once */
                    if (ISP_MESSAGE_TILE_OK == result)
                    {
                        if (ptr_func->out_const_x <= 0)
                        {
                            result = ISP_MESSAGE_OUT_CONST_X_ERROR;
                            tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                            return result;
                        }
                        if (ptr_func->out_const_y <= 0)
                        {
                            result = ISP_MESSAGE_OUT_CONST_Y_ERROR;
                            tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                            return result;
                        }
                        if (ptr_func->in_const_x <= 0)
                        {
                            result = ISP_MESSAGE_IN_CONST_X_ERROR;
                            tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                            return result;
                        }
                        if (ptr_func->in_const_y <= 0)
                        {
                            result = ISP_MESSAGE_IN_CONST_Y_ERROR;
                            tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                            return result;
                        }
                    }
                }
            }
            else
            {
                return result;
            }
        }
    }
    /* find out run mode with main, sub in, sub out */
    if (ISP_MESSAGE_TILE_OK == result)
    {
        int i;
        bool found_input_count = false;
        bool found_output_en = false;
        /* search main path */
        for (i=0;i<module_no;i++)
        {
            unsigned char module_order = ptr_tile_func_param->scheduling_forward_order[i];
			TILE_FUNC_BLOCK_STRUCT *ptr_func =
				ptr_tile_func_param->func_list[module_order];
            /* check run_mode */
            /* skip output disable func in following check */
            if (false == ptr_func->output_disable_flag)
            {
                /* skip end func with sub out mode */
                if (TILE_RUN_MODE_MAIN == ptr_func->run_mode)
                {
                    if (PREVIOUS_BLK_NO_OF_START == ptr_func->prev_blk_num[0])
                    {
                        if (found_input_count)
                        {
                            result = ISP_MESSAGE_TWO_MAIN_START_ERROR;
                            tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                            return result;
                        }
						ptr_tile_reg_map->first_func_en_no = module_order;
                        found_input_count = true;
                    }
                    else
                    {
                        int j;
                        bool found_main_input = false;
                        for (j=0;j<ptr_func->tot_prev_num;j++)
                        {
				TILE_FUNC_BLOCK_STRUCT *ptr_prev =
					ptr_tile_func_param->func_list[ptr_func->prev_blk_num[j]];
                            if (false == ptr_prev->output_disable_flag)
                            {
                                if (TILE_RUN_MODE_MAIN == ptr_prev->run_mode)
                                {
                                    if (found_main_input)
                                    {
                                        /* more main input error */
                                        result = ISP_MESSAGE_TWO_MAIN_PREV_ERROR;
                                        tile_driver_printf("Error [%s][%s] %s\r\n", ptr_func->func_name, ptr_prev->func_name, tile_print_error_message(result));
                                        return result;
                                    }
                                    if (ptr_func->enable_flag)
                                    {
										if (false == found_output_en)
										{
											/* only happen with enable */
											if (0 == ptr_func->tot_branch_num)
											{
												found_output_en = true;
												ptr_tile_reg_map->last_func_en_no = module_order;
											}
										}
                                    }
                                    found_main_input = true;
                                }
                            }
                        }
                        /* found sub in to update */
                        if (false == found_main_input)
                        {
                            if (0 == ptr_func->tot_branch_num)
                            {
								/* check SUB_OUT from SUB_IN only */
								result = ISP_MESSAGE_MIX_SUB_IN_OUT_PATH_ERROR;
								tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
								return result;
                            }
                            else
                            {
								ptr_func->run_mode = TILE_RUN_MODE_SUB_IN;
                            }
                        }
                    }
                }
                else if (TILE_RUN_MODE_SUB_IN == ptr_func->run_mode)
				{
					/* multi-out support */
					if (PREVIOUS_BLK_NO_OF_START == ptr_func->prev_blk_num[0])
					{
		                ptr_tile_reg_map->found_sub_in = true;
					}
					else
					{
						/* invalid SUB_IN */
						result = ISP_MESSAGE_INVALID_SUB_IN_CONFIG_ERROR;
						tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
						return result;
					}
                }
				else if (TILE_RUN_MODE_SUB_OUT == ptr_func->run_mode)
				{
					if (ptr_func->tot_branch_num)
					{
						/* invalid SUB_OUT */
						result = ISP_MESSAGE_INVALID_SUB_OUT_CONFIG_ERROR;
						tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
						return result;
					}
					else
					{
						/* check tdr enable to disable SUB_OUT, diff view support */
						if (ptr_tile_reg_map->tdr_ctrl_en)
						{
							ptr_func->run_mode = TILE_RUN_MODE_MAIN;
						}
						else
						{
							/* disable strict end check */
							ptr_func->type |= TILE_TYPE_DONT_CARE_END;
							ptr_tile_reg_map->found_sub_out = true;
						}
					}
				}
				else
				{
					/* invalid run mode */
					result = ISP_MESSAGE_UNKNOWN_RUN_MODE_ERROR;
					tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
					return result;
				}
            }
        }
        if (false == found_output_en)
        {
            result = ISP_MESSAGE_NO_MAIN_OUTPUT_ERROR;
            tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
            return result;
        }
		/* search sub out path */
		if (ptr_tile_reg_map->found_sub_out)
		{
			for (i=0;i<module_no;i++)
			{
				unsigned char module_order = ptr_tile_func_param->scheduling_backward_order[i];
				TILE_FUNC_BLOCK_STRUCT *ptr_func =
					ptr_tile_func_param->func_list[module_order];
				/* check run_mode */
				/* skip output disable func in following check */
				if (false == ptr_func->output_disable_flag)
				{
					if (ptr_func->tot_branch_num)
					{
						int j;
						for (j=0;j<ptr_func->tot_branch_num;j++)
						{
							TILE_FUNC_BLOCK_STRUCT *ptr_next =
								ptr_tile_func_param->func_list[
								ptr_func->next_blk_num[j]];
							if ((TILE_RUN_MODE_SUB_OUT != ptr_next->run_mode) && (false == ptr_next->output_disable_flag))
							{
								break;
							}
						}
						if (j == ptr_func->tot_branch_num)
						{
							/* multi-in support */
							ptr_func->run_mode = TILE_RUN_MODE_SUB_OUT;
						}
					}
				}
			}
        }
    }
    /* lcm in & out alignment by forward */
    if (ISP_MESSAGE_TILE_OK == result)
    {
		int i;
        for (i=0;i<module_no;i++)
        {
            unsigned char module_order = ptr_tile_func_param->scheduling_forward_order[i];
				TILE_FUNC_BLOCK_STRUCT *ptr_func =
					ptr_tile_func_param->func_list[module_order];
            /* skip output disable func in following check */
            if (false == ptr_func->output_disable_flag)
            {
				/* start & end functions full size alignment has been checked before */
                if (0 < ptr_func->tot_branch_num)/* not end func */
                {
					int out_const_x = ptr_func->out_const_x;
					int out_const_y = ptr_func->out_const_y;
					int out_tile_width = ptr_func->out_tile_width;
					int out_tile_height = ptr_func->out_tile_height;
					int out_max_width = ptr_func->out_max_width;
					int out_max_height = ptr_func->out_max_height;
					int j;
                    for (j=0;j<ptr_func->tot_branch_num;j++)
                    {
				TILE_FUNC_BLOCK_STRUCT *ptr_next =
					ptr_tile_func_param->func_list[ptr_func->next_blk_num[j]];
                        /* skip output disable func in following check */
                        if (false == ptr_next->output_disable_flag)
                        {
                            if (ptr_next->in_const_x > 1)
                            {
                                out_const_x = tile_cal_lcm(out_const_x, ptr_next->in_const_x);
                            }
                            if (ptr_next->in_const_y > 1)
                            {
                                out_const_y = tile_cal_lcm(out_const_y, ptr_next->in_const_y);
                            }
                            /* find min tile out width */
                            if (out_tile_width)
                            {
                                if (ptr_next->in_tile_width)
                                {
                                    if (out_tile_width > ptr_next->in_tile_width)
                                    {
                                        out_tile_width = ptr_next->in_tile_width;
                                    }
                                }
                            }
                            else
                            {
                                if (ptr_next->in_tile_width)
                                {
                                    out_tile_width = ptr_next->in_tile_width;
                                }
                            }
                            /* find min out width */
                            if (out_max_width)
                            {
                                if (ptr_next->in_max_width)
                                {
                                    if (out_max_width > ptr_next->in_max_width)
                                    {
                                        out_max_width = ptr_next->in_max_width;
                                    }
                                }
                            }
                            else
                            {
                                if (ptr_next->in_max_width)
                                {
                                    out_max_width = ptr_next->in_max_width;
                                }
                            }
                            /* find min tile out height */
                            if (out_tile_height)
                            {
                                if (ptr_next->in_tile_height)
                                {
                                    if (out_tile_height > ptr_next->in_tile_height)
                                    {
                                        out_tile_height = ptr_next->in_tile_height;
                                    }
                                }
                            }
                            else
                            {
                                if (ptr_next->in_tile_height)
                                {
                                    out_tile_height = ptr_next->in_tile_height;
                                }
                            }
                            /* find min out height */
                            if (out_max_height)
                            {
                                if (ptr_next->in_max_height)
                                {
                                    if (out_max_height > ptr_next->in_max_height)
                                    {
                                        out_max_height = ptr_next->in_max_height;
                                    }
                                }
                            }
                            else
                            {
                                if (ptr_next->in_max_height)
                                {
                                    out_max_height = ptr_next->in_max_height;
                                }
                            }
                        }
                    }
                    ptr_func->out_const_x = out_const_x;
                    ptr_func->out_const_y = out_const_y;
                    ptr_func->out_tile_width = out_tile_width;
                    ptr_func->out_max_width = out_max_width;
                    ptr_func->out_tile_height = out_tile_height;
                    ptr_func->out_max_height = out_max_height;
                    for (j=0;j<ptr_func->tot_branch_num;j++)
                    {
				TILE_FUNC_BLOCK_STRUCT *ptr_next =
					ptr_tile_func_param->func_list[ptr_func->next_blk_num[j]];
                        /* skip output disable func in following update */
                        if (false == ptr_next->output_disable_flag)
                        {
                            ptr_next->in_const_x = out_const_x;
                            ptr_next->in_const_y = out_const_y;
                            ptr_next->in_tile_width = out_tile_width;
                            ptr_next->in_max_width = out_max_width;
                            ptr_next->in_tile_height = out_tile_height;
                            ptr_next->in_max_height = out_max_height;
                        }
                    }
					/* check in/out alignment of full size */
					if (out_const_y > 1)
					{
						if (TILE_MOD(ptr_func->full_size_y_out, out_const_y))
						{
							result = ISP_MESSAGE_YSIZE_NOT_DIV_BY_OUT_CONST_Y_ERROR;
							tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
							return result;
						}
					}
					if (out_const_x > 1)
					{
						if (TILE_MOD(ptr_func->full_size_x_out, out_const_x))
						{
							result = ISP_MESSAGE_XSIZE_NOT_DIV_BY_OUT_CONST_X_ERROR;
							tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
							return result;
						}
					}
                }
            }
        }
    }
    /* lcm in & out alignment by backward */
    if (ISP_MESSAGE_TILE_OK == result)
    {
        if (ptr_tile_reg_map->found_sub_in)
        {
			int i;
            /* clear valid flag */
            for (i=0;i<module_no;i++)
            {
                unsigned char module_order = ptr_tile_func_param->scheduling_backward_order[i];
				TILE_FUNC_BLOCK_STRUCT *ptr_func =
					ptr_tile_func_param->func_list[module_order];
                /* skip output disable func in following check */
                if (false == ptr_func->output_disable_flag)
                {
                    /* only check sub in merge function */
					if (1 < ptr_func->tot_prev_num)
                    {
						int in_const_x = ptr_func->in_const_x;
						int in_const_y = ptr_func->in_const_y;
						int in_tile_width = ptr_func->in_tile_width;
						int in_max_width = ptr_func->in_max_width;
						int in_tile_height = ptr_func->in_tile_height;
						int in_max_height = ptr_func->in_max_height;
						int j;
                        for (j=0;j<ptr_func->tot_prev_num;j++)
                        {
				TILE_FUNC_BLOCK_STRUCT *ptr_prev =
					ptr_tile_func_param->func_list[ptr_func->prev_blk_num[j]];
                            /* skip output disable func in following check */
                            if (false == ptr_prev->output_disable_flag)
                            {
                                if (ptr_prev->out_const_x > 1)
                                {
                                    in_const_x = tile_cal_lcm(in_const_x, ptr_prev->out_const_x);
                                }
                                if (ptr_prev->out_const_y > 1)
                                {
                                    in_const_y = tile_cal_lcm(in_const_y, ptr_prev->out_const_y);
                                }
                                /* find min tile out width */
                                if (in_tile_width)
                                {
                                    if (ptr_prev->out_tile_width)
                                    {
                                        if (in_tile_width > ptr_prev->out_tile_width)
                                        {
                                            in_tile_width = ptr_prev->out_tile_width;
                                        }
                                    }
                                }
                                else
                                {
                                    if (ptr_prev->out_tile_width)
                                    {
                                        in_tile_width = ptr_prev->out_tile_width;
                                    }
                                }
                                /* find min out width */
                                if (in_max_width)
                                {
                                    if (ptr_prev->out_max_width)
                                    {
                                        if (in_max_width > ptr_prev->out_max_width)
                                        {
                                            in_max_width = ptr_prev->out_max_width;
                                        }
                                    }
                                }
                                else
                                {
                                    if (ptr_prev->out_max_width)
                                    {
                                        in_max_width = ptr_prev->out_max_width;
                                    }
                                }
                                /* find min tile out height */
                                if (in_tile_height)
                                {
                                    if (ptr_prev->out_tile_height)
                                    {
                                        if (in_tile_height > ptr_prev->out_tile_height)
                                        {
                                            in_tile_height = ptr_prev->out_tile_height;
                                        }
                                    }
                                }
                                else
                                {
                                    if (ptr_prev->out_tile_height)
                                    {
                                        in_tile_height = ptr_prev->out_tile_height;
                                    }
                                }
                                /* find min out height */
                                if (in_max_height)
                                {
                                    if (ptr_prev->out_max_height)
                                    {
                                        if (in_max_height > ptr_prev->out_max_height)
                                        {
                                            in_max_height = ptr_prev->out_max_height;
                                        }
                                    }
                                }
                                else
                                {
                                    if (ptr_prev->out_max_height)
                                    {
                                        in_max_height = ptr_prev->out_max_height;
                                    }
                                }
                            }
                        }
                        ptr_func->in_const_x = in_const_x;
                        ptr_func->in_const_y = in_const_y;
                        ptr_func->in_tile_width = in_tile_width;
                        ptr_func->in_max_width = in_max_width;
                        ptr_func->in_tile_height = in_tile_height;
                        ptr_func->in_max_height = in_max_height;
                        for (j=0;j<ptr_func->tot_prev_num;j++)
                        {
				TILE_FUNC_BLOCK_STRUCT *ptr_prev =
					ptr_tile_func_param->func_list[ptr_func->prev_blk_num[j]];
                            /* skip output disable func in following update */
                            if (false == ptr_prev->output_disable_flag)
                            {
                                ptr_prev->out_const_x = in_const_x;
                                ptr_prev->out_const_y = in_const_y;
                                ptr_prev->out_tile_width = in_tile_width;
                                ptr_prev->out_max_width = in_max_width;
                                ptr_prev->out_tile_height = in_tile_height;
                                ptr_prev->out_max_height = in_max_height;
                            }
                        }
						/* check in/out alignment of full size */
						if (in_const_y > 1)
						{
							if (TILE_MOD(ptr_func->full_size_y_in, in_const_y))
							{
								result = ISP_MESSAGE_YSIZE_NOT_DIV_BY_IN_CONST_Y_ERROR;
								tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
								return result;
							}
						}
						if (in_const_x > 1)
						{
							if (TILE_MOD(ptr_func->full_size_x_in, in_const_x))
							{
								result = ISP_MESSAGE_XSIZE_NOT_DIV_BY_IN_CONST_X_ERROR;
								tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
								return result;
							}
						}
                    }
                }
            }
        }
    }
    if (ISP_MESSAGE_TILE_OK == result)
    {
        int i;
        /* clear valid flag */
		memset(ptr_tile_func_param->valid_flag, 0x0,
			4 * ((unsigned int)(module_no + 31) >> 5));
        for (i=0;i<module_no;i++)
        {
            unsigned char module_order = ptr_tile_func_param->scheduling_backward_order[i];
			TILE_FUNC_BLOCK_STRUCT *ptr_func =
				ptr_tile_func_param->func_list[module_order];
            unsigned int *ptr_valid = &ptr_tile_func_param->valid_flag[module_order>>5];
            /* check broken path w/o main connected */
            if ((TILE_RUN_MODE_MAIN != ptr_func->run_mode) && (false == ptr_func->output_disable_flag))
            {
                if (ptr_func->tot_branch_num)
                {
                    int j;
                    for (j=0;j<ptr_func->tot_branch_num;j++)
                    {
                        int k = ptr_func->next_blk_num[j];
                        if (false == (ptr_tile_func_param->valid_flag[k>>5] & (1<<(k & 0x1F))))/* break if invalid */
                        {
                            break;
                        }
                    }
                    if (ptr_func->tot_branch_num == j)
                    {
                        *ptr_valid |= 1<<(module_order & 0x1F);
                    }
                }
                else
                {
                    *ptr_valid |= 1<<(module_order & 0x1F);
                }
            }
            if (PREVIOUS_BLK_NO_OF_START == ptr_func->prev_blk_num[0])
            {
                if (*ptr_valid & (1<<(module_order & 0x1F)))/* fail if valid found */
                {
                    result = ISP_MESSAGE_BROKEN_SUB_PATH_ERROR;
                    tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
                    return result;
                }
            }
		}
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_backward_comp(TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
										   FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
	int i;
	int tile_reg_map_run_mode = ptr_tile_reg_map->run_mode;
	/* run normal tile */
	if (ptr_tile_reg_map->backup_x_skip_y)
	{
		return ISP_MESSAGE_TILE_OK;
	}
	/* scheduling backward order */
	for (i=0;i<ptr_tile_func_param->used_func_no;i++)
	{
		/* run normal tile */
		if (ISP_MESSAGE_TILE_OK == result)
		{
			unsigned char module_order = ptr_tile_func_param->scheduling_backward_order[i];
			TILE_FUNC_BLOCK_STRUCT *ptr_func =
				ptr_tile_func_param->func_list[module_order];
			/* skip output disable func */
			if (ptr_func->output_disable_flag)
			{
				continue;
			}
			/* skip diff run mode */
			if (tile_reg_map_run_mode != ptr_func->run_mode)
			{
				continue;
			}
			/* backward comp by func */
			result = tile_backward_output_config(ptr_func, ptr_tile_reg_map, ptr_tile_func_param);
			if (ISP_MESSAGE_TILE_OK == result)
			{
				result = tile_backward_by_func(ptr_func, ptr_tile_reg_map);
			}
			if (ISP_MESSAGE_TILE_OK == result)
			{
				/* check input smaller than tile size */
				result = tile_backward_input_check(ptr_func, ptr_tile_reg_map);
			}
		}
		else
		{
			return result;
		}
	}
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_backward_comp_min(TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
											   FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    if (ptr_tile_reg_map->backup_x_skip_y)
    {
		return ISP_MESSAGE_TILE_OK;
	}
	/* init first_pass enable */
	if (false == ptr_tile_reg_map->first_frame)
	{
		/* run min tile cal */
		if (ptr_tile_reg_map->tdr_ctrl_en)
		{
			ptr_tile_reg_map->first_pass = 1;
			result =  tile_backward_comp_min_tile(ptr_tile_reg_map, ptr_tile_func_param);
			ptr_tile_reg_map->first_pass = 0;
		}
	}
	/* clear first_pass enable */
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_backward_comp_min_tile(TILE_REG_MAP_STRUCT *ptr_tile_reg_map, FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
	/* run min tile */
	int i;
	int tile_reg_map_run_mode = ptr_tile_reg_map->run_mode;
	/* scheduling backward order */
	for (i=0;i<ptr_tile_func_param->used_func_no;i++)
	{
		if (ISP_MESSAGE_TILE_OK == result)
		{
			unsigned char module_order = ptr_tile_func_param->scheduling_backward_order[i];
			TILE_FUNC_BLOCK_STRUCT *ptr_func =
				ptr_tile_func_param->func_list[module_order];
			/* skip output disable func */
			if (ptr_func->output_disable_flag)
			{
				continue;
			}
			if (tile_reg_map_run_mode != ptr_func->run_mode)
			{
				continue;
			}
			result = tile_backward_min_tile_init(ptr_func, ptr_tile_reg_map);
			/* backward comp by func */
			if (ISP_MESSAGE_TILE_OK == result)
			{
				result = tile_backward_output_config_min_tile(ptr_func, ptr_tile_reg_map, ptr_tile_func_param);
			}
			/* cal min tile backward */
			if (ISP_MESSAGE_TILE_OK == result)
			{
				result = tile_backward_by_func(ptr_func, ptr_tile_reg_map);
			}
			if (ISP_MESSAGE_TILE_OK == result)
			{
				/* check input smaller than tile size */
				result = tile_backward_input_check(ptr_func, ptr_tile_reg_map);
			}
			if (ISP_MESSAGE_TILE_OK == result)
			{
				result = tile_backward_min_tile_backup_input(ptr_func, ptr_tile_reg_map);
			}
			if (ISP_MESSAGE_TILE_OK == result)
			{
				result = tile_backward_min_tile_restore(ptr_func, ptr_tile_reg_map);
			}
		}
		else
		{
			return result;
		}
	}
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_schedule_backward(FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param)
{
	ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
	int module_no = ptr_tile_func_param->used_func_no;
	int i;

	/* clear valid flag */
	memset(ptr_tile_func_param->valid_flag, 0x0,
		4 * ((unsigned int)(module_no + 31) >> 5));
    /* scheduling backward */
    for (i=0;i<module_no;i++)
    {
        int j;
        bool found_flag = false;
        unsigned char *ptr_order = (unsigned char *)&ptr_tile_func_param->scheduling_backward_order[i];
        for (j=0;j<module_no;j++)
        {
			unsigned int *ptr_valid = &ptr_tile_func_param->valid_flag[(unsigned int)j>>5];
            if (false == (*ptr_valid & (1<<(j & 0x1F))))/* skip valid */
            {
			TILE_FUNC_BLOCK_STRUCT *ptr_func = ptr_tile_func_param->func_list[j];
				/* non-branch to set valid if next valid */
                if (1 == ptr_func->tot_branch_num)
                {
                    if (ptr_tile_func_param->valid_flag[ptr_func->next_blk_num[0]>>5] & (1<<(ptr_func->next_blk_num[0] & 0x1F)))
                    {
                        *ptr_order = j;
                        *ptr_valid |= 1<<(j & 0x1F);
                        found_flag = true;
                        break;
                    }
                }
                else if (0 == ptr_func->tot_branch_num)/* non-branch to set valid if next end */
                {
                    *ptr_order = j;
                    *ptr_valid |= 1<<(j & 0x1F);
                    found_flag = true;
                    break;
                }
                else/* non-branch to set valid if all branches valid */
                {
                    int k;
                    for (k=0;k<ptr_func->tot_branch_num;k++)
                    {
                        /* stop when invalid found */
                        if (false == (ptr_tile_func_param->valid_flag[ptr_func->next_blk_num[k]>>5] &
							(1<<(ptr_func->next_blk_num[k] & 0x1F))))
                        {
                            break;
                        }
                    }
                    if (k == ptr_func->tot_branch_num)/* set valid if all valid */
                    {
                        *ptr_order = j;
                        *ptr_valid |= 1<<(j & 0x1F);
                        found_flag = true;
                        break;
                    }
                }
            }
        }
        if (false == found_flag)
        {
            result = ISP_MESSAGE_SCHEDULING_BACKWARD_ERROR;
            tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
            return result;
        }
    }
    return result;
}

ISP_TILE_MESSAGE_ENUM tile_mode_init(TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
									   FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param)
{
    /* reset necessary variables only */
    ptr_tile_reg_map->skip_x_cal = false;
    ptr_tile_reg_map->skip_y_cal = false;
	ptr_tile_reg_map->backup_x_skip_y = false;
    ptr_tile_reg_map->used_tile_no = 0;
    ptr_tile_reg_map->horizontal_tile_no = 0;
    ptr_tile_reg_map->curr_horizontal_tile_no = 0;
    ptr_tile_reg_map->curr_vertical_tile_no = 0;
    ptr_tile_reg_map->run_mode = TILE_RUN_MODE_MAIN;
    if (false == ptr_tile_reg_map->first_frame)
    {
        if (ptr_tile_reg_map->found_sub_out)
        {
            int i;
            /* set all sub out to normal mode to prevent too small size in end tile of sub out */
            for (i=0;i<ptr_tile_func_param->used_func_no;i++)
            {
				TILE_FUNC_BLOCK_STRUCT *ptr_func =
					ptr_tile_func_param->func_list[i];
                if (false  == ptr_func->output_disable_flag)
                {
                    if (TILE_RUN_MODE_SUB_OUT == ptr_func->run_mode)
                    {
                        ptr_func->type &= ~TILE_TYPE_DONT_CARE_END;
						ptr_func->run_mode = TILE_RUN_MODE_MAIN;
                    }
                }
            }
            ptr_tile_reg_map->found_sub_out = false;
        }
    }
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_check_valid_output(TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
												FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
	int i;
	bool found_output_count_x = false;
	bool found_output_count_y = false;
	int tile_reg_map_run_mode = ptr_tile_reg_map->run_mode;
	int tile_reg_map_skip_x_cal = ptr_tile_reg_map->skip_x_cal;
	int tile_reg_map_skip_y_cal = ptr_tile_reg_map->skip_y_cal;
	if (ptr_tile_reg_map->backup_x_skip_y)
	{
		return ISP_MESSAGE_TILE_OK;
	}
	if (ptr_tile_reg_map->first_frame)
	{
		return ISP_MESSAGE_TILE_OK;
	}
    for (i=0;i<ptr_tile_func_param->used_func_no;i++)
    {
		/* faster stop by backward order */
		unsigned char module_order = ptr_tile_func_param->scheduling_backward_order[i];
		TILE_FUNC_BLOCK_STRUCT *ptr_func = ptr_tile_func_param->func_list[module_order];
		if (ptr_func->output_disable_flag)
		{
			continue;
		}
		if (false == ptr_func->enable_flag)
		{
			continue;
		}
		if (tile_reg_map_run_mode != ptr_func->run_mode)
		{
			continue;
		}
		if (false == tile_reg_map_skip_x_cal)
		{
			if (false == found_output_count_x)
			{
				/* end func */
				if (0 == ptr_func->tot_branch_num)
				{
					if (false == ptr_func->tdr_h_disable_flag)
					{
						if (ptr_func->valid_h_no)
						{
							/* not direct link */
							if (0x0 == (ptr_func->type & TILE_TYPE_DONT_CARE_END))
							{
								if (ptr_func->out_cal_order & TILE_ORDER_RIGHT_TO_LEFT)
								{
									if (((0x0 == (ptr_func->type & TILE_TYPE_CROP_EN)) ||
										(ptr_func->out_pos_xe + 1 == ptr_func->last_output_xs_pos)) &&
										(ptr_func->out_pos_xs < ptr_func->last_output_xs_pos))
									{
										found_output_count_x = true;
									}
								}
								else
								{
									if (((0x0 == (ptr_func->type & TILE_TYPE_CROP_EN)) ||
										(ptr_func->out_pos_xs == ptr_func->last_output_xe_pos + 1)) &&
										(ptr_func->out_pos_xe > ptr_func->last_output_xe_pos))
									{
										found_output_count_x = true;
									}
								}
							}
							else
							{
								found_output_count_x = true;
							}
						}
						else
						{
							found_output_count_x = true;
						}
					}
				}
			}
		}
		if (false == tile_reg_map_skip_y_cal)
		{
			if (false == found_output_count_y)
			{
				/* end func */
				if (0 == ptr_func->tot_branch_num)
				{
					if (false == ptr_func->tdr_v_disable_flag)
					{
						if (ptr_func->valid_v_no)
						{
							/* not direct link */
							if (0x0 == (ptr_func->type & TILE_TYPE_DONT_CARE_END))
							{
								if (((0x0 == (ptr_func->type & TILE_TYPE_CROP_EN)) ||
									(ptr_func->out_pos_ys == ptr_func->last_output_ye_pos + 1)) &&
									(ptr_func->out_pos_ye > ptr_func->last_output_ye_pos))
								{
									found_output_count_y = true;
								}
							}
							else
							{
								found_output_count_y = true;
							}
						}
						else
						{
							found_output_count_y = true;
						}
					}
				}
			}
		}
		if ((found_output_count_x || tile_reg_map_skip_x_cal) && (found_output_count_y || tile_reg_map_skip_y_cal))
		{
			return result;/* early return OK */
		}
    }
	if (false == tile_reg_map_skip_x_cal)
	{
		if (false == found_output_count_x)
		{
			result = ISP_MESSAGE_DIFF_VIEW_OUTPUT_ERROR;
			tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
			return result;
		}
	}
	if (false == tile_reg_map_skip_y_cal)
	{
		if (false == found_output_count_y)
		{
			result = ISP_MESSAGE_DIFF_VIEW_OUTPUT_ERROR;
			tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
			return result;
		}
	}
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_check_min_tile(TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
											FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
	int i;
	bool found_output_count_x = false;
	bool found_output_count_y = false;
	int tile_reg_map_run_mode = ptr_tile_reg_map->run_mode;
	int tile_reg_map_skip_x_cal = ptr_tile_reg_map->skip_x_cal;
	int tile_reg_map_skip_y_cal = ptr_tile_reg_map->skip_y_cal;
	if (false == ptr_tile_reg_map->tdr_ctrl_en)
	{
		return ISP_MESSAGE_TILE_OK;
	}
	if (ptr_tile_reg_map->backup_x_skip_y)
	{
		return ISP_MESSAGE_TILE_OK;
	}
	if (ptr_tile_reg_map->first_frame)
	{
		return ISP_MESSAGE_TILE_OK;
	}
    for (i=0;i<ptr_tile_func_param->used_func_no;i++)
    {
		/* must check by forward order */
		unsigned char module_order = ptr_tile_func_param->scheduling_forward_order[i];
		TILE_FUNC_BLOCK_STRUCT *ptr_func = ptr_tile_func_param->func_list[module_order];
		if (ptr_func->output_disable_flag)
		{
			continue;
		}
		if (false == ptr_func->enable_flag)
		{
			continue;
		}
		if (tile_reg_map_run_mode != ptr_func->run_mode)
		{
			continue;
		}
		if (false == tile_reg_map_skip_x_cal)
		{
			if (false == found_output_count_x)
			{
				/* check valid input */
				if (PREVIOUS_BLK_NO_OF_START == ptr_func->prev_blk_num[0])
				{
					if (ptr_func->tdr_h_disable_flag)
					{
						result = ISP_MESSAGE_DIFF_VIEW_INPUT_ERROR;
						tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
						return result;
					}
				}
				/* check valid output */
				if (0 == ptr_func->tot_branch_num)
				{
					if (false == ptr_func->tdr_h_disable_flag)
					{
						found_output_count_x = true;
					}
				}
			}
		}
		if (false == tile_reg_map_skip_y_cal)
		{
			if (false == found_output_count_y)
			{
				/* check valid input */
				if (PREVIOUS_BLK_NO_OF_START == ptr_func->prev_blk_num[0])
				{
					if (ptr_func->tdr_v_disable_flag)
					{
						result = ISP_MESSAGE_DIFF_VIEW_INPUT_ERROR;
						tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
						return result;
					}
				}
				/* check valid output */
				if (0 == ptr_func->tot_branch_num)
				{
					if (false == ptr_func->tdr_v_disable_flag)
					{
						found_output_count_y = true;
					}
				}
			}
		}
		if ((found_output_count_x || tile_reg_map_skip_x_cal) && (found_output_count_y || tile_reg_map_skip_y_cal))
		{
			return result;/* early return OK */
		}
    }
	if (false == tile_reg_map_skip_x_cal)
	{
		if (false == found_output_count_x)
		{
			result = ISP_MESSAGE_DIFF_VIEW_OUTPUT_ERROR;
			tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
			return result;
		}
	}
	if (false == tile_reg_map_skip_y_cal)
	{
		if (false == found_output_count_y)
		{
			result = ISP_MESSAGE_DIFF_VIEW_OUTPUT_ERROR;
			tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
			return result;
		}
	}
    return ISP_MESSAGE_TILE_OK;
}

ISP_TILE_MESSAGE_ENUM tile_mode_close(TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
									   FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param)
{
    return ISP_MESSAGE_TILE_OK;
}

ISP_TILE_MESSAGE_ENUM tile_frame_mode_init(TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
											 FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param)
{
    /* init must parameters */
    ISP_TILE_MESSAGE_ENUM result;
    ptr_tile_reg_map->first_frame = 1;

    result = tile_mode_init(ptr_tile_reg_map, ptr_tile_func_param);
    if (ISP_MESSAGE_TILE_OK == result)
    {
        int i;
        for (i=0;i<ptr_tile_func_param->used_func_no;i++)
        {
		TILE_FUNC_BLOCK_STRUCT *ptr_func = ptr_tile_func_param->func_list[i];
			if (false == ptr_func->output_disable_flag)
			{
				ptr_func->min_in_pos_xs = MAX_SIZE;/* init value */
				ptr_func->max_in_pos_xe = 0;
				ptr_func->min_in_pos_ys = MAX_SIZE;/* init value */
				ptr_func->max_in_pos_ye = 0;
				ptr_func->in_tile_width_backup = ptr_func->in_tile_width;
				ptr_func->in_tile_height_backup = ptr_func->in_tile_height;
				ptr_func->out_tile_width_backup = ptr_func->out_tile_width;
				ptr_func->out_tile_height_backup = ptr_func->out_tile_height;
				ptr_func->in_tile_width = 0;
				ptr_func->in_tile_height = 0;
				ptr_func->out_tile_width = 0;
				ptr_func->out_tile_height = 0;
				ptr_func->min_tile_in_pos_xs = MAX_SIZE;/* init value */
				ptr_func->min_tile_in_pos_xe = 0;
				ptr_func->min_tile_in_pos_ys = MAX_SIZE;/* init value */
				ptr_func->min_tile_in_pos_ye =  0;
			}
        }
    }
    return result;
}

ISP_TILE_MESSAGE_ENUM tile_frame_mode_close(TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
			FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param)
{
    int i;
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    for (i=0;i<ptr_tile_func_param->used_func_no;i++)
    {
		TILE_FUNC_BLOCK_STRUCT *ptr_func = ptr_tile_func_param->func_list[i];
		if (false == ptr_func->output_disable_flag)
		{
			ptr_func->in_tile_width = ptr_func->in_tile_width_backup;
			ptr_func->in_tile_height = ptr_func->in_tile_height_backup;
			ptr_func->out_tile_width = ptr_func->out_tile_width_backup;
			ptr_func->out_tile_height = ptr_func->out_tile_height_backup;
			/* update min & max pos only frame tdr is not skipped to prevent from error of min tile cal*/
			if (TILE_RUN_MODE_SUB_OUT == ptr_func->run_mode)
			{
				ptr_func->min_in_pos_xs = ptr_func->in_pos_xs;
				ptr_func->max_in_pos_xe = ptr_func->in_pos_xe;
				ptr_func->min_in_pos_ys =  ptr_func->in_pos_ys;
				ptr_func->max_in_pos_ye = ptr_func->in_pos_ye;
			}
			else
			{
				ptr_func->min_in_pos_xs = ptr_func->backward_input_xs_pos;
				ptr_func->max_in_pos_xe = ptr_func->backward_input_xe_pos;
				ptr_func->min_in_pos_ys =  ptr_func->backward_input_ys_pos;
				ptr_func->max_in_pos_ye = ptr_func->backward_input_ye_pos;
			}
			ptr_func->min_out_pos_xs = ptr_func->out_pos_xs;
			ptr_func->max_out_pos_xe = ptr_func->out_pos_xe;
			ptr_func->min_out_pos_ys = ptr_func->out_pos_ys;
			ptr_func->max_out_pos_ye = ptr_func->out_pos_ye;
		}
    }
    /* restore must parameters */
    if (ISP_MESSAGE_TILE_OK == result)
    {
        result = tile_mode_close(ptr_tile_reg_map, ptr_tile_func_param);
    }
    ptr_tile_reg_map->first_frame = 0;
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_check_input_config(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    if (false == ptr_tile_reg_map->skip_x_cal)
	{
		if (false == ptr_func->tdr_h_disable_flag)
		{
			int in_const_x = ptr_func->in_const_x;
			/* check input x size & pos */
			if ((ptr_func->full_size_x_in <= 0) ||
				(ptr_func->in_pos_xs < 0) ||
				(ptr_func->in_pos_xe >= ptr_func->full_size_x_in) ||
				(ptr_func->in_pos_xs > ptr_func->in_pos_xe))
			{
				result = ISP_MESSAGE_INIT_INCORRECT_X_INPUT_SIZE_POS_ERROR;
				tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
				return result;
			}
			if (ISP_MESSAGE_TILE_OK == result)
			{
				/* skip start time check */
				if (ptr_func->valid_h_no)
				{
					/* check cal order, input */
					if (ptr_func->in_cal_order & TILE_ORDER_RIGHT_TO_LEFT)
					{
						if (ptr_func->in_pos_xe > ptr_func->last_input_xe_pos)
						{
							result = ISP_MESSAGE_TILE_LOSS_OVER_TILE_WIDTH_ERROR;
							tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
							return result;
						}
					}
					else
					{
						if (ptr_func->in_pos_xs < ptr_func->last_input_xs_pos)
						{
							result = ISP_MESSAGE_TILE_LOSS_OVER_TILE_WIDTH_ERROR;
							tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
							return result;
						}
					}
				}
			}
			if (ISP_MESSAGE_TILE_OK == result)
			{
				/* check mis-algin xe & ye compensated by over tile size in backward */
				if (in_const_x > 1)
				{
					if (TILE_MOD(ptr_func->in_pos_xe + 1, in_const_x))
					{
						result = ISP_MESSAGE_CHECK_IN_CONFIG_ALIGN_XE_POS_ERROR;
						tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
						return result;
					}
					if (TILE_MOD(ptr_func->in_pos_xs, in_const_x))
					{
						result = ISP_MESSAGE_CHECK_IN_CONFIG_ALIGN_XS_POS_ERROR;
						tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
						return result;
					}
				}
			}
		}
    }
    if (false == ptr_tile_reg_map->skip_y_cal)
	{
		if (false == ptr_func->tdr_v_disable_flag)
		{
			int in_const_y = ptr_func->in_const_y;
			/* check input y size & pos */
			if ((ptr_func->full_size_y_in <= 0) ||
				(ptr_func->in_pos_ys < 0) ||
				(ptr_func->in_pos_ye >= ptr_func->full_size_y_in) ||
				(ptr_func->in_pos_ys > ptr_func->in_pos_ye))
			{
				result = ISP_MESSAGE_INIT_INCORRECT_Y_INPUT_SIZE_POS_ERROR;
				tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
				return result;
			}
			if (ISP_MESSAGE_TILE_OK == result)
			{
				if (ptr_func->valid_v_no)/* skip start time check */
				{
					/* check cal order, not support */
					if (ptr_func->in_pos_ys < ptr_func->last_input_ys_pos)
					{
						result = ISP_MESSAGE_TILE_LOSS_OVER_TILE_HEIGHT_ERROR;
						tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
						return result;
					}
				}
			}
			if (ISP_MESSAGE_TILE_OK == result)
			{
				/* check mis-algin xe & ye compensated by over tile size in backward */
				if (in_const_y > 1)
				{
					if (TILE_MOD(ptr_func->in_pos_ye + 1, in_const_y))
					{
						result = ISP_MESSAGE_CHECK_IN_CONFIG_ALIGN_YE_POS_ERROR;
						tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
						return result;
					}
					if (TILE_MOD(ptr_func->in_pos_ys, in_const_y))
					{
						result = ISP_MESSAGE_CHECK_IN_CONFIG_ALIGN_YS_POS_ERROR;
						tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
						return result;
					}
				}
			}
		}
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_check_output_config_x(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    if (false == ptr_tile_reg_map->skip_x_cal)
    {
		if (false == ptr_func->tdr_h_disable_flag)
		{
			/* check output x size & pos */
			if ((ptr_func->min_out_pos_xs < 0) || (ptr_func->max_out_pos_xe < 0) ||
				(ptr_func->min_out_pos_xs > ptr_func->max_out_pos_xe) ||
				(ptr_func->out_pos_xs < ptr_func->min_out_pos_xs) ||
				(ptr_func->out_pos_xe > ptr_func->max_out_pos_xe) ||
				(ptr_func->out_pos_xs > ptr_func->out_pos_xe))
			{
				result = ISP_MESSAGE_INIT_INCORRECT_X_OUTPUT_SIZE_POS_ERROR;
				tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
				return result;
			}
			/* check alignment */
			if (ISP_MESSAGE_TILE_OK == result)
			{
				int out_const_x = ptr_func->out_const_x;
				if (1 < out_const_x)
				{
					int val_s = TILE_MOD(ptr_func->out_pos_xs, out_const_x);
					int val_e = TILE_MOD(ptr_func->out_pos_xe + 1, out_const_x);
					if (0 != val_s)
					{
						result = ISP_MESSAGE_CHECK_OUT_CONFIG_ALIGN_XS_POS_ERROR;
						tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
						return result;
					}
					if (0 != val_e)
					{
						result = ISP_MESSAGE_CHECK_OUT_CONFIG_ALIGN_XE_POS_ERROR;
						tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
						return result;
					}
				}
			}
			if (ISP_MESSAGE_TILE_OK == result)
			{
				/* non-end func */
				if ((0 < ptr_func->tot_branch_num) || (ptr_func->type & TILE_TYPE_DONT_CARE_END))
				{
					if (ptr_func->valid_h_no)/* skip start time check */
					{
						if (ptr_func->out_pos_xs < ptr_func->last_output_xs_pos)
						{
							result = ISP_MESSAGE_TILE_LOSS_OVER_TILE_WIDTH_ERROR;
							tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
							return result;
						}
					}
				}
			}
		}
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_check_output_config_x_inv(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    if (false == ptr_tile_reg_map->skip_x_cal)
    {
		if (false == ptr_func->tdr_h_disable_flag)
		{
			/* check output x size & pos */
			if ((ptr_func->min_out_pos_xs < 0) || (ptr_func->max_out_pos_xe < 0) ||
				(ptr_func->min_out_pos_xs > ptr_func->max_out_pos_xe) ||
				(ptr_func->out_pos_xs < ptr_func->min_out_pos_xs) ||
				(ptr_func->out_pos_xe > ptr_func->max_out_pos_xe) ||
				(ptr_func->out_pos_xs > ptr_func->out_pos_xe))
			{
				result = ISP_MESSAGE_INIT_INCORRECT_X_OUTPUT_SIZE_POS_ERROR;
				tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
				return result;
			}
			/* check alignment */
			if (ISP_MESSAGE_TILE_OK == result)
			{
				int out_const_x = ptr_func->out_const_x;
				if (1 < out_const_x)
				{
					int val_s = TILE_MOD(ptr_func->out_pos_xs, out_const_x);
					int val_e = TILE_MOD(ptr_func->out_pos_xe + 1, out_const_x);
					if (0 != val_s)
					{
						result = ISP_MESSAGE_CHECK_OUT_CONFIG_ALIGN_XS_POS_ERROR;
						tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
						return result;
					}
					if (0 != val_e)
					{
						result = ISP_MESSAGE_CHECK_OUT_CONFIG_ALIGN_XE_POS_ERROR;
						tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
						return result;
					}
				}
			}
			if (ISP_MESSAGE_TILE_OK == result)
			{
				/* non-end func */
				if ((0 < ptr_func->tot_branch_num) || (ptr_func->type & TILE_TYPE_DONT_CARE_END))
				{
					if (ptr_func->valid_h_no)/* skip start time check */
					{
						if (ptr_func->out_pos_xe > ptr_func->last_output_xe_pos)
						{
							result = ISP_MESSAGE_TILE_LOSS_OVER_TILE_WIDTH_ERROR;
							tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
							return result;
						}
					}
				}
			}
		}
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_check_output_config_y(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    if (false == ptr_tile_reg_map->skip_y_cal)
    {
        if (false == ptr_func->tdr_v_disable_flag)
		{
			/* check output y size & pos */
			if ((ptr_func->min_out_pos_ys < 0) || (ptr_func->max_out_pos_ye < 0) ||
				(ptr_func->min_out_pos_ys > ptr_func->max_out_pos_ye) ||
				(ptr_func->out_pos_ys < ptr_func->min_out_pos_ys) ||
				(ptr_func->out_pos_ye > ptr_func->max_out_pos_ye) ||
				(ptr_func->out_pos_ys > ptr_func->out_pos_ye))
			{
				result = ISP_MESSAGE_INIT_INCORRECT_Y_OUTPUT_SIZE_POS_ERROR;
				tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
				return result;
			}
			/* check alignment */
			if (ISP_MESSAGE_TILE_OK == result)
			{
				int out_const_y = ptr_func->out_const_y;
				if (1 < out_const_y)
				{
					int val_s = TILE_MOD(ptr_func->out_pos_ys, out_const_y);
					int val_e = TILE_MOD(ptr_func->out_pos_ye + 1, out_const_y);
					if (0 != val_s)
					{
						result = ISP_MESSAGE_CHECK_OUT_CONFIG_ALIGN_YS_POS_ERROR;
						tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
						return result;
					}
					if (0 != val_e)
					{
						result = ISP_MESSAGE_CHECK_OUT_CONFIG_ALIGN_YE_POS_ERROR;
						tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
						return result;
					}
				}
			}
			if (ISP_MESSAGE_TILE_OK == result)
			{
				/* non-end func */
				if ((0 < ptr_func->tot_branch_num) || (ptr_func->type & TILE_TYPE_DONT_CARE_END))
				{
					if (ptr_func->valid_v_no)/* skip start time check */
					{
						/* check cal order, not support */
						if (ptr_func->out_pos_ys < ptr_func->last_output_ys_pos)
						{
							result = ISP_MESSAGE_TILE_LOSS_OVER_TILE_HEIGHT_ERROR;
							tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
							return result;
						}
					}
				}
			}
		}
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_check_output_config(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    if (false == ptr_tile_reg_map->skip_x_cal)
	{
		if (false == ptr_func->tdr_h_disable_flag)
		{
			/* check cal order, output */
			if (ptr_func->out_cal_order & TILE_ORDER_RIGHT_TO_LEFT)
			{
				result = tile_check_output_config_x_inv(ptr_func, ptr_tile_reg_map);
			}
			else
			{
				result = tile_check_output_config_x(ptr_func, ptr_tile_reg_map);
			}
		}
	}
	if (ISP_MESSAGE_TILE_OK == result)
	{
		if (false == ptr_tile_reg_map->skip_y_cal)
		{
			if (false == ptr_func->tdr_v_disable_flag)
			{
				result = tile_check_output_config_y(ptr_func, ptr_tile_reg_map);
			}
		}
	}
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_backward_output_config_skip(TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
														 FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
	/* only check last & non-skipped */
	if (ptr_tile_reg_map->curr_vertical_tile_no == 0)  /* first tile row */
	{
		/* last is end of right but not end tile */
		if (ptr_tile_reg_map->curr_horizontal_tile_no == 0) /* first tile */
		{
			/* reset skip flag */
			ptr_tile_reg_map->skip_x_cal = false;
			ptr_tile_reg_map->skip_y_cal = false;
		}
		else
		{
			/* set skip flag */
			ptr_tile_reg_map->skip_x_cal = false;
			ptr_tile_reg_map->skip_y_cal = true;
		}
		ptr_tile_reg_map->backup_x_skip_y = false;/* x & y not skipped */
	}
	else/* middle row */
	{
		int curr_horizontal_tile_no = ptr_tile_reg_map->curr_horizontal_tile_no;
		/* last is end of right but not end tile */
		if (curr_horizontal_tile_no == 0) /* first tile column, y will need to cal */
		{
			/* set skip flag */
			ptr_tile_reg_map->skip_y_cal = false;
		}
		else
		{
			/* set skip flag */
			ptr_tile_reg_map->skip_y_cal = true;
		}
		if(curr_horizontal_tile_no < MAX_TILE_BACKUP_HORZ_NO)/* y will not need to cal */
		{
			int i;
			ptr_tile_reg_map->skip_x_cal = true;
			for (i=0;i<ptr_tile_func_param->used_func_no;i++)
			{
				TILE_FUNC_BLOCK_STRUCT *ptr_func_backup =
					ptr_tile_func_param->func_list[i];
				if (false == ptr_func_backup->output_disable_flag)
				{
					struct tile_horz_backup *ptr_para =
						&ptr_func_backup->horz_para[
						curr_horizontal_tile_no];

					HORZ_PARA_RESTORE(ptr_para, ptr_func_backup);
				}
			}
		}
		else
		{
			/* not enough buffer to store tile horizontal parameters */
			ptr_tile_reg_map->skip_x_cal = false;
		}
		if (ptr_tile_reg_map->skip_x_cal && ptr_tile_reg_map->skip_y_cal)
		{
			ptr_tile_reg_map->backup_x_skip_y = true;/* x & y all skipped */
		}
		else
		{
			ptr_tile_reg_map->backup_x_skip_y = false;/* x & y not skipped */
		}
	}
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_backward_output_config_x(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
													FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
	if (false == ptr_tile_reg_map->skip_x_cal)
	{
		/* only check last & non-skipped */
		if (false == ptr_func->tdr_h_disable_flag)
		{
			/* tile movement */
			if (0 == ptr_func->tot_branch_num)/* end module only */
			{
				/* not direct link */
				if (0x0 == (ptr_func->type & TILE_TYPE_DONT_CARE_END))
				{
					/* init output size of final module */
					if (ptr_func->valid_h_no == 0)
					{
						/* first tile */
						ptr_func->out_pos_xs = ptr_func->min_out_pos_xs;
						ptr_func->out_pos_xe = ptr_func->out_tile_width?
							(ptr_func->min_out_pos_xs + ptr_func->out_tile_width - 1):ptr_func->max_out_pos_xe;
					}
					else
					{
						/* move right to set output pos */
						if (ptr_func->last_output_xe_pos < ptr_func->max_out_pos_xe)
						{
							ptr_func->out_pos_xs = ptr_func->last_output_xe_pos + 1;
							ptr_func->out_pos_xe = ptr_func->out_tile_width?
								(ptr_func->last_output_xe_pos + ptr_func->out_tile_width):ptr_func->max_out_pos_xe;
						}
						else
						{
							/* keep min output size */
							ptr_func->out_pos_xs = ptr_func->max_out_pos_xe - ptr_func->out_const_x + 1;
							ptr_func->out_pos_xe = ptr_func->max_out_pos_xe;
						}
					}
				    if (1 < ptr_func->out_const_x)
				    {
					    int val_e = TILE_MOD(ptr_func->out_pos_xe + 1, ptr_func->out_const_x);
					    if (0 != val_e)
					    {
						    ptr_func->out_pos_xe -= val_e;/* decreae xe */
					    }
				    }
					/* check size equal to full size */
					if (ptr_func->out_pos_xe >= ptr_func->max_out_pos_xe)
					{
						ptr_func->out_pos_xe = ptr_func->max_out_pos_xe;
						ptr_func->max_h_edge_flag = true;
						ptr_func->h_end_flag = true;
						ptr_func->crop_h_end_flag = true;
					}
					else
					{
						ptr_func->h_end_flag = false;
						ptr_func->crop_h_end_flag = false;
						if (ptr_func->out_tile_width && (ptr_func->out_pos_xs + ptr_func->out_tile_width < ptr_func->max_out_pos_xe + 1))
						{
							ptr_func->max_h_edge_flag = false;
						}
						else
						{
							ptr_func->max_h_edge_flag = true;
						}
					}
				}
				else/* direct link */
				{
					ptr_func->out_pos_xs = ptr_func->direct_out_pos_xs;
					ptr_func->out_pos_xe = ptr_func->direct_out_pos_xe;
					ptr_func->h_end_flag = ptr_func->direct_h_end_flag;
					ptr_func->crop_h_end_flag = ptr_func->direct_h_end_flag;
				}
				/* out_tile_width_max */
				ptr_func->out_tile_width_max = ptr_func->out_tile_width;
				/* out_tile_width_max_str */
				ptr_func->out_tile_width_max_str = ptr_func->out_tile_width;
				/* out_tile_width_max_end */
				ptr_func->out_tile_width_max_end = ptr_func->out_tile_width;
				/* smart tile + ufd */
				ptr_func->max_out_crop_xe = ptr_func->max_out_pos_xe;
				ptr_func->min_tile_crop_out_pos_xe = ptr_func->min_tile_out_pos_xe;
			}
			else if (1 == ptr_func->tot_branch_num)/* check non-branch */
			{
				/* set curr out with next in */
				TILE_FUNC_BLOCK_STRUCT *ptr_next =
					ptr_tile_func_param->func_list[ptr_func->next_blk_num[0]];
				ptr_func->h_end_flag = ptr_next->h_end_flag;
				ptr_func->crop_h_end_flag = ptr_next->crop_h_end_flag;
				/* tdr_h_disable_flag changed during back cal by sub-in */
				ptr_func->tdr_h_disable_flag = ptr_next->tdr_h_disable_flag;
				/* backward h_end_flag */
				if (false == ptr_next->tdr_h_disable_flag)
				{
					ptr_func->out_pos_xs = ptr_next->in_pos_xs;
					ptr_func->out_pos_xe = ptr_next->in_pos_xe;
					ptr_func->max_h_edge_flag = ptr_next->max_h_edge_flag;
					/* out_tile_width_max */
					ptr_func->out_tile_width_max = ptr_next->in_tile_width_max;
					/* out_tile_width_max_str */
					ptr_func->out_tile_width_max_str = ptr_next->in_tile_width_max_str;
					/* out_tile_width_max_end */
					ptr_func->out_tile_width_max_end = ptr_next->in_tile_width_max_end;
					/* smart tile + ufd */
					ptr_func->out_tile_width_loss = ptr_next->in_tile_width_loss;
					ptr_func->max_out_crop_xe = ptr_next->max_in_crop_xe;
					ptr_func->min_tile_crop_out_pos_xe = ptr_next->min_tile_crop_in_pos_xe;
				}
			}
			else/* branch */
			{
				int out_pos_xs = MAX_SIZE;/* select max start pos */
				int out_pos_xe = 0;/* select min end pos */
				int min_tile_out_pos_xs = MAX_SIZE;/* select max start pos */
				int min_tile_out_pos_xe = 0;/* select min end pos */
				bool max_h_edge_flag = true;
				bool h_end_flag = true;
				bool crop_h_end_flag = true;
				bool last_full_range_flag = true;
				int count = 0;
				int i;
				int tile_reg_map_run_mode = ptr_tile_reg_map->run_mode;
				/* out_tile_width_max */
				int out_tile_width_max = ptr_func->out_tile_width;
				/* out_tile_width_max_str */
				int out_tile_width_max_str = ptr_func->out_tile_width;
				/* out_tile_width_max_end */
				int out_tile_width_max_end = ptr_func->out_tile_width;
				int out_tile_width_loss = 0;
				int max_out_crop_xe = 0;
				int min_tile_crop_out_pos_xe = 0;
				/* min xs/ys & min xe/ye sorting for current support tile mode */
				for (i=0;i<ptr_func->tot_branch_num;i++)
				{
					TILE_FUNC_BLOCK_STRUCT *ptr_next =
						ptr_tile_func_param->func_list[
						ptr_func->next_blk_num[i]];
					int temp = ptr_next->max_in_pos_xe;
					/* skip output disabled branch */
					if (ptr_next->output_disable_flag)
					{
						continue;
					}
					if (tile_reg_map_run_mode == (tile_reg_map_run_mode & ptr_next->run_mode))
					{
						h_end_flag &= ptr_next->h_end_flag;
						crop_h_end_flag &= ptr_next->crop_h_end_flag;
						/* tdr_h_disable_flag changed during back cal by sub-in */
						if (ptr_next->tdr_h_disable_flag)
						{
							continue;
						}
						max_h_edge_flag &= ptr_next->max_h_edge_flag;
						/* out_tile_width_max */
						if (out_tile_width_max && ptr_next->in_tile_width_max)
						{
							if (out_tile_width_max > ptr_next->in_tile_width_max)
							{
								out_tile_width_max = ptr_next->in_tile_width_max;
							}
						}
						else if (ptr_next->in_tile_width_max)
						{
							out_tile_width_max = ptr_next->in_tile_width_max;
						}
						/* out_tile_width_max_str */
						if (out_tile_width_max_str && ptr_next->in_tile_width_max_str)
						{
							if (out_tile_width_max_str > ptr_next->in_tile_width_max_str)
							{
								out_tile_width_max_str = ptr_next->in_tile_width_max_str;
							}
						}
						else if (ptr_next->in_tile_width_max_str)
						{
							out_tile_width_max_str = ptr_next->in_tile_width_max_str;
						}
						/* out_tile_width_max_end */
						if (out_tile_width_max_end && ptr_next->in_tile_width_max_end)
						{
							if (out_tile_width_max_end > ptr_next->in_tile_width_max_end)
							{
								out_tile_width_max_end = ptr_next->in_tile_width_max_end;
							}
						}
						else if (ptr_next->in_tile_width_max_end)
						{
							out_tile_width_max_end = ptr_next->in_tile_width_max_end;
						}
						/* smart tile + ufd */
						if (out_tile_width_loss <= ptr_next->in_tile_width_loss)
						{
							/* check equal */
							if (out_tile_width_loss == ptr_next->in_tile_width_loss)
							{
								if (max_out_crop_xe < ptr_next->max_in_crop_xe)
								{
									max_out_crop_xe = ptr_next->max_in_crop_xe;
									min_tile_crop_out_pos_xe = ptr_next->min_tile_crop_in_pos_xe;
								}
							}
							else
							{
								out_tile_width_loss = ptr_next->in_tile_width_loss;
								max_out_crop_xe = ptr_next->max_in_crop_xe;
								min_tile_crop_out_pos_xe = ptr_next->min_tile_crop_in_pos_xe;
							}
						}
						if (TILE_RUN_MODE_MAIN == tile_reg_map_run_mode)
						{
							if ((false == ptr_tile_reg_map->first_pass) && (false == ptr_tile_reg_map->first_frame) &&
								ptr_tile_reg_map->tdr_ctrl_en && (TILE_RUN_MODE_MAIN == ptr_func->run_mode))
							{
								if ((ptr_next->min_tile_in_pos_xs == ptr_next->in_pos_xs) && (ptr_next->min_tile_in_pos_xe == ptr_next->in_pos_xe) && (ptr_next->in_pos_xe < temp))
								{
									if (ptr_next->in_pos_xs < min_tile_out_pos_xs)
									{
										min_tile_out_pos_xs = ptr_next->in_pos_xs;
										min_tile_out_pos_xe = ptr_next->in_pos_xe;
									}
									else if (ptr_next->in_pos_xs == min_tile_out_pos_xs)
									{
										if (ptr_next->in_pos_xe > min_tile_out_pos_xe)
										{
											min_tile_out_pos_xe = ptr_next->in_pos_xe;
										}
									}
								}
								else
								{
									/* select min pos */
									if (out_pos_xs > ptr_next->in_pos_xs)
									{
										out_pos_xs = ptr_next->in_pos_xs;
									}
									if (last_full_range_flag)/* last full range */
									{
										if (ptr_next->in_pos_xe >= temp)/* full range input */
										{
											/* select max xe pos */
											if (out_pos_xe < ptr_next->in_pos_xe)
											{
												out_pos_xe = ptr_next->in_pos_xe;
											}
										}
										else
										{
											/* non-full range input */
											out_pos_xe = ptr_next->in_pos_xe;
											last_full_range_flag = false;
										}
									}
									else
									{
										if (ptr_next->in_pos_xe < temp)/* non-full range input */
										{
											/* select min xe pos */
											if (out_pos_xe > ptr_next->in_pos_xe)
											{
												out_pos_xe = ptr_next->in_pos_xe;
											}
										}
										/* full range input do nothing */
									}
								}
							}
							else
							{
								/* select min pos */
								if (out_pos_xs > ptr_next->in_pos_xs)
								{
									out_pos_xs = ptr_next->in_pos_xs;
								}
								if (last_full_range_flag)/* last full range */
								{
									if (ptr_next->in_pos_xe >= temp)/* full range input */
									{
										/* select max xe pos */
										if (out_pos_xe < ptr_next->in_pos_xe)
										{
											out_pos_xe = ptr_next->in_pos_xe;
										}
									}
									else
									{
										/* non-full range input */
										out_pos_xe = ptr_next->in_pos_xe;
										last_full_range_flag = false;
									}
								}
								else
								{
									if (ptr_next->in_pos_xe < temp)/* non-full range input */
									{
										/* select min xe pos */
										if (out_pos_xe > ptr_next->in_pos_xe)
										{
											out_pos_xe = ptr_next->in_pos_xe;
										}
									}
									/* full range input do nothing */
								}
							}
						}
						else
						{
							/* sub-in with multi-out */
							if (count)
							{
								if ((out_pos_xs != ptr_next->in_pos_xs) || (out_pos_xe != ptr_next->in_pos_xe))
								{
									result = ISP_MESSAGE_DIFF_NEXT_CONFIG_ERROR;
									tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
									return result;
								}
							}
							else
							{
								out_pos_xs = ptr_next->in_pos_xs;
								out_pos_xe = ptr_next->in_pos_xe;
							}
						}
						count++;
					}
				}
				/* update h_end_flag */
				ptr_func->h_end_flag = h_end_flag;
				ptr_func->crop_h_end_flag = crop_h_end_flag;
				if (count)
				{
					if ((false == ptr_tile_reg_map->first_pass) && (false == ptr_tile_reg_map->first_frame) &&
						ptr_tile_reg_map->tdr_ctrl_en && (TILE_RUN_MODE_MAIN == ptr_func->run_mode))
					{
						if (out_pos_xe < out_pos_xs)
						{
							out_pos_xs = min_tile_out_pos_xs;
							out_pos_xe = min_tile_out_pos_xe;
						}
						else if (min_tile_out_pos_xe >= min_tile_out_pos_xs)
						{
							if (out_pos_xs <= min_tile_out_pos_xs)
							{
								if (min_tile_out_pos_xe <= out_pos_xe)
								{
									if (false == h_end_flag)
								{
									out_pos_xe = min_tile_out_pos_xe;
								}
							}
							else
							{
									if (h_end_flag)
									{
										out_pos_xe = min_tile_out_pos_xe;
									}
									else
									{
										if (min_tile_out_pos_xe + out_pos_xs < out_pos_xe + min_tile_out_pos_xs)
										{
											out_pos_xe = min_tile_out_pos_xe;
										}
										else
										{
											if (2*out_pos_xe + 1 > min_tile_out_pos_xe + min_tile_out_pos_xs)
											{
												out_pos_xe = min_tile_out_pos_xe;
											}
										}
									}
								}
							}
							else
							{
								if (min_tile_out_pos_xe <= out_pos_xe)
								{
									if (false == h_end_flag)
									{
								if (min_tile_out_pos_xe + out_pos_xs < out_pos_xe + min_tile_out_pos_xs)
								{
									out_pos_xe = min_tile_out_pos_xe;
								}
									}
								}
								else
								{
									out_pos_xe = min_tile_out_pos_xe;
								}
								out_pos_xs = min_tile_out_pos_xs;
							}
						}
						if (out_pos_xe < ptr_func->min_tile_out_pos_xe)
						{
							out_pos_xe = ptr_func->min_tile_out_pos_xe;
						}
					}
					ptr_func->out_pos_xs = out_pos_xs;
					ptr_func->out_pos_xe = out_pos_xe;
					ptr_func->max_h_edge_flag = max_h_edge_flag;
					/* update tdr_h_disable_flag changed during back cal by sub-in */
					ptr_func->tdr_h_disable_flag = false;
					/* out_tile_width_max */
					ptr_func->out_tile_width_max = out_tile_width_max;
					/* out_tile_width_max_str */
					ptr_func->out_tile_width_max_str = out_tile_width_max_str;
					/* out_tile_width_max_end */
					ptr_func->out_tile_width_max_end = out_tile_width_max_end;
					/* smart tile + ufd */
					ptr_func->out_tile_width_loss = out_tile_width_loss;
					ptr_func->max_out_crop_xe = max_out_crop_xe;
					ptr_func->min_tile_crop_out_pos_xe = min_tile_crop_out_pos_xe;
				}
				else/* check nonzero branch */
				{
					ptr_func->tdr_h_disable_flag = true;
				}
			}
			if (false == ptr_func->tdr_h_disable_flag)
			{
				/* right edge */
				if (ptr_func->out_pos_xe + 1 >= ptr_func->full_size_x_out)
				{
					if (ptr_func->out_tile_width_max_end)
					{
						if (ptr_func->out_tile_width_max_end + ptr_func->out_pos_xs <  ptr_func->full_size_x_out)
						{
							ptr_func->out_pos_xe = ptr_func->full_size_x_out - 1 - ptr_func->out_const_x;
							/* update flag */
							ptr_func->max_h_edge_flag = false;
							ptr_func->crop_h_end_flag = false;
							ptr_func->h_end_flag = false;
						}
					}
				}
				/* left edge */
				if (ptr_func->out_pos_xs <= 0)
				{
					if (ptr_func->out_tile_width_max_str)
					{
						if (ptr_func->out_tile_width_max_str < ptr_func->out_pos_xe + 1)
						{
							int out_const_x = ptr_func->out_const_x;
							ptr_func->in_pos_xe = ptr_func->out_tile_width_max_str - 1;
							/* update flag */
							ptr_func->max_h_edge_flag = false;
							ptr_func->crop_h_end_flag = false;
							ptr_func->h_end_flag = false;
							if (out_const_x > 1)
							{
								/* shift in_pos_xs */
								int val_e = TILE_MOD(ptr_func->out_pos_xe + 1, out_const_x);
								if (val_e)
								{
									ptr_func->out_pos_xe -= val_e;
								}
							}
						}
					}
				}
				/* check over tile size with enable & skip buffer check false */
				if (ptr_func->out_tile_width || ptr_func->out_tile_width_max)
				{
					int out_tile_width = ptr_func->out_tile_width;
					if (out_tile_width)
					{
						if (ptr_func->out_tile_width_max)
						{
							if (ptr_func->out_tile_width > ptr_func->out_tile_width_max)
							{
								out_tile_width = ptr_func->out_tile_width_max;
							}
						}
					}
					else
					{
						out_tile_width = ptr_func->out_tile_width_max;
					}
					if (ptr_func->out_pos_xe + 1 > ptr_func->out_pos_xs + out_tile_width)
					{
						/* update flag */
						ptr_func->max_h_edge_flag = false;
						ptr_func->crop_h_end_flag = false;
					}
					/* tile size constraint check */
					if ((false == ptr_tile_reg_map->first_pass) && (false == ptr_tile_reg_map->first_frame) &&
						ptr_tile_reg_map->tdr_ctrl_en &&
						(TILE_RUN_MODE_MAIN == ptr_func->run_mode) && ptr_func->out_max_width)
					{
						if (ptr_func->out_max_width < out_tile_width)
						{
							if (ptr_func->out_pos_xs + ptr_func->out_max_width < ptr_func->min_tile_out_pos_xe + 1)
							{
								out_tile_width = ptr_func->min_tile_out_pos_xe - ptr_func->out_pos_xs + 1;
							}
							else
							{
								out_tile_width = ptr_func->out_max_width;
							}
						}
					}
					if (ptr_func->out_pos_xe + 1 > ptr_func->out_pos_xs + out_tile_width)
					{
						int out_const_x = ptr_func->out_const_x;
						ptr_func->out_pos_xe = ptr_func->out_pos_xs + out_tile_width - 1;
						/* only update h_end_flag */
						ptr_func->h_end_flag = false;
						if (1 < out_const_x)
						{
							int val_e = TILE_MOD(ptr_func->out_pos_xe + 1, out_const_x);
							if (0 != val_e)
							{
								ptr_func->out_pos_xe -= val_e;/* decreae xe */
							}
						}
					}
				}
				else if (ptr_func->out_max_width)
				{
					if ((false == ptr_tile_reg_map->first_pass) && (false == ptr_tile_reg_map->first_frame) &&
						ptr_tile_reg_map->tdr_ctrl_en &&
						(TILE_RUN_MODE_MAIN == ptr_func->run_mode))
					{
						int out_tile_width = ptr_func->out_max_width;
						if (ptr_func->out_pos_xs + out_tile_width > ptr_func->min_tile_out_pos_xe + 1)
						{
							if (ptr_func->out_pos_xe + 1 > ptr_func->out_pos_xs + out_tile_width)
							{
								int out_const_x = ptr_func->out_const_x;
								ptr_func->out_pos_xe = ptr_func->out_pos_xs + out_tile_width - 1;
								/* only update h_end_flag */
								ptr_func->h_end_flag = false;
								if (1 < out_const_x)
								{
									int val_e = TILE_MOD(ptr_func->out_pos_xe + 1, out_const_x);
									if (0 != val_e)
									{
										ptr_func->out_pos_xe -= val_e;/* decreae xe */
									}
								}
							}
						}
					}
				}
				if (ISP_MESSAGE_TILE_OK == result)
				{
					result = tile_check_output_config_x(ptr_func, ptr_tile_reg_map);
				}
			}
		}
        /* backup direct flag */
	    ptr_func->direct_out_pos_xs = ptr_func->out_pos_xs;
	    ptr_func->direct_out_pos_xe = ptr_func->out_pos_xe;
	    ptr_func->direct_h_end_flag = ptr_func->h_end_flag;
	}
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_backward_output_config_x_inv(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
														  FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
	/* only check last & not skipped */
	if (false == ptr_tile_reg_map->skip_x_cal)
	{
		if (false == ptr_func->tdr_h_disable_flag)
		{
			/* tile movement */
			if (0 == ptr_func->tot_branch_num)/* end module only */
			{
				/* not diect link */
				if (0x0 == (ptr_func->type & TILE_TYPE_DONT_CARE_END))
				{
					/* init output size of final module */
					if (ptr_func->valid_h_no == 0)
					{
						/* first tile */
						ptr_func->out_pos_xs = ptr_func->out_tile_width?
							(ptr_func->max_out_pos_xe - ptr_func->out_tile_width + 1):ptr_func->min_out_pos_xs;
						ptr_func->out_pos_xe = ptr_func->max_out_pos_xe;
					}
					else
					{
						/* move left to set output pos */
						if (ptr_func->last_output_xs_pos > ptr_func->min_out_pos_xs)
						{
							ptr_func->out_pos_xs = ptr_func->out_tile_width?
								(ptr_func->last_output_xs_pos - ptr_func->out_tile_width):ptr_func->min_out_pos_xs;
							ptr_func->out_pos_xe = ptr_func->last_output_xs_pos - 1;
						}
						else
						{
							/* keep min output size */
							ptr_func->out_pos_xs = ptr_func->min_out_pos_xs;
							ptr_func->out_pos_xe = ptr_func->min_out_pos_xs + ptr_func->out_const_x - 1;
						}
					}
				    if (1 < ptr_func->out_const_x)
				    {
					    int val_s = TILE_MOD(ptr_func->out_pos_xs, ptr_func->out_const_x);
					    if (0 != val_s)
					    {
						    ptr_func->out_pos_xs += ptr_func->out_const_x - val_s;/* increae xs */
					    }
				    }
					/* check over max size */
					if (ptr_func->out_pos_xs <= ptr_func->min_out_pos_xs)
					{
						ptr_func->out_pos_xs = ptr_func->min_out_pos_xs;
						ptr_func->max_h_edge_flag = true;
						ptr_func->h_end_flag = true;
						ptr_func->crop_h_end_flag = true;
					}
					else
					{
						ptr_func->h_end_flag = false;
						ptr_func->crop_h_end_flag = false;
						if (ptr_func->out_tile_width && (ptr_func->min_out_pos_xs + ptr_func->out_tile_width < ptr_func->out_pos_xe + 1))
						{
							ptr_func->max_h_edge_flag = false;
						}
						else
						{
							ptr_func->max_h_edge_flag = true;
						}
					}
				}
				else/* direct link */
				{
					ptr_func->out_pos_xs = ptr_func->direct_out_pos_xs;
					ptr_func->out_pos_xe = ptr_func->direct_out_pos_xe;
					ptr_func->h_end_flag = ptr_func->direct_h_end_flag;
					ptr_func->crop_h_end_flag = ptr_func->direct_h_end_flag;
				}
				/* out_tile_width_max */
				ptr_func->out_tile_width_max = ptr_func->out_tile_width;
				/* out_tile_width_max_str */
				ptr_func->out_tile_width_max_str = ptr_func->out_tile_width;
				/* out_tile_width_max_end */
				ptr_func->out_tile_width_max_end = ptr_func->out_tile_width;
				ptr_func->min_out_crop_xs = ptr_func->min_out_pos_xs;
				ptr_func->min_tile_crop_out_pos_xs = ptr_func->min_tile_out_pos_xs;
			}
			else if (1 == ptr_func->tot_branch_num)/* check non-branch */
			{
				/* set curr out with next in */
				TILE_FUNC_BLOCK_STRUCT *ptr_next =
					ptr_tile_func_param->func_list[ptr_func->next_blk_num[0]];
				ptr_func->h_end_flag = ptr_next->h_end_flag;
				ptr_func->crop_h_end_flag = ptr_next->crop_h_end_flag;
				/* update tdr_h_disable_flag for change during back cal by sub-in */
				ptr_func->tdr_h_disable_flag = ptr_next->tdr_h_disable_flag;
				/* backward h_end_flag */
				if (false == ptr_next->tdr_h_disable_flag)
				{
					ptr_func->out_pos_xs = ptr_next->in_pos_xs;
					ptr_func->out_pos_xe = ptr_next->in_pos_xe;
					ptr_func->max_h_edge_flag = ptr_next->max_h_edge_flag;
					/* out_tile_width_max */
					ptr_func->out_tile_width_max = ptr_next->in_tile_width_max;
					/* out_tile_width_max_str */
					ptr_func->out_tile_width_max_str = ptr_next->in_tile_width_max_str;
					/* out_tile_width_max_end */
					ptr_func->out_tile_width_max_end = ptr_next->in_tile_width_max_end;
					/* smart tile + ufd */
					ptr_func->out_tile_width_loss = ptr_next->in_tile_width_loss;
					ptr_func->min_out_crop_xs = ptr_next->min_in_pos_xs;
					ptr_func->min_tile_crop_out_pos_xs = ptr_next->min_tile_crop_in_pos_xs;
				}
			}
			else/* branch */
			{
				int i;
				int out_pos_xs = MAX_SIZE;/* select min start pos */
				int out_pos_xe = 0;/* select max end pos */
				bool max_h_edge_flag = true;
				bool h_end_flag = true;
				bool crop_h_end_flag = true;
				bool last_full_range_flag = true;
				int count = 0;
				int tile_reg_map_run_mode = ptr_tile_reg_map->run_mode;
				/* out_tile_width_max */
				int out_tile_width_max = ptr_func->out_tile_width;
				/* out_tile_width_max_str */
				int out_tile_width_max_str = ptr_func->out_tile_width;
				/* out_tile_width_max_end */
				int out_tile_width_max_end = ptr_func->out_tile_width;
				int out_tile_width_loss = 0;
				int min_out_crop_xs = MAX_SIZE;
				int min_tile_crop_out_pos_xs = MAX_SIZE;
				/* max xs/ys & max xe/ye sorting for current support tile mode */
				for (i=0;i<ptr_func->tot_branch_num;i++)
				{
					TILE_FUNC_BLOCK_STRUCT *ptr_next =
						ptr_tile_func_param->func_list[
						ptr_func->next_blk_num[i]];
					int temp = ptr_next->min_in_pos_xs;
					/* skip output disabled branch */
					if (ptr_next->output_disable_flag)
					{
						continue;
					}
					if (tile_reg_map_run_mode == (tile_reg_map_run_mode & ptr_next->run_mode))
					{
						h_end_flag &= ptr_next->h_end_flag;
						crop_h_end_flag &= ptr_next->crop_h_end_flag;
						/* tdr_h_disable_flag changed during back cal by sub-in */
						if (ptr_next->tdr_h_disable_flag)
						{
							continue;
						}
						max_h_edge_flag &= ptr_next->max_h_edge_flag;
						/* out_tile_width_max */
						if (out_tile_width_max && ptr_next->in_tile_width_max)
						{
							if (out_tile_width_max > ptr_next->in_tile_width_max)
							{
								out_tile_width_max = ptr_next->in_tile_width_max;
							}
						}
						else if (ptr_next->in_tile_width_max)
						{
							out_tile_width_max = ptr_next->in_tile_width_max;
						}
						/* out_tile_width_max_str */
						if (out_tile_width_max_str && ptr_next->in_tile_width_max_str)
						{
							if (out_tile_width_max_str > ptr_next->in_tile_width_max_str)
							{
								out_tile_width_max_str = ptr_next->in_tile_width_max_str;
							}
						}
						else if (ptr_next->in_tile_width_max_str)
						{
							out_tile_width_max_str = ptr_next->in_tile_width_max_str;
						}
						/* out_tile_width_max_end */
						if (out_tile_width_max_end && ptr_next->in_tile_width_max_end)
						{
							if (out_tile_width_max_end > ptr_next->in_tile_width_max_end)
							{
								out_tile_width_max_end = ptr_next->in_tile_width_max_end;
							}
						}
						else if (ptr_next->in_tile_width_max_end)
						{
							out_tile_width_max_end = ptr_next->in_tile_width_max_end;
						}
						/* smart tile + ufd */
						if (out_tile_width_loss <= ptr_next->in_tile_width_loss)
						{
							/* check equal */
							if (out_tile_width_loss == ptr_next->in_tile_width_loss)
							{
								if (min_out_crop_xs > ptr_next->min_in_crop_xs)
								{
									min_out_crop_xs = ptr_next->min_in_crop_xs;
									min_tile_crop_out_pos_xs = ptr_next->min_tile_crop_in_pos_xs;
								}
							}
							else
							{
								out_tile_width_loss = ptr_next->in_tile_width_loss;
								min_out_crop_xs = ptr_next->min_in_crop_xs;
								min_tile_crop_out_pos_xs = ptr_next->min_tile_crop_in_pos_xs;
							}
						}
						if (TILE_RUN_MODE_MAIN == tile_reg_map_run_mode)
						{
							/* select max xe pos */
							if (out_pos_xe < ptr_next->in_pos_xe)
							{
								out_pos_xe = ptr_next->in_pos_xe;
							}
							if (last_full_range_flag)/* last full range */
							{
								if (ptr_next->in_pos_xs <= temp)/* full range input */
								{
									/* select min xs pos */
									if (out_pos_xs > ptr_next->in_pos_xs)
									{
										out_pos_xs = ptr_next->in_pos_xs;
									}
								}
								else
								{
									/* non-full range input */
									out_pos_xs = ptr_next->in_pos_xs;
									last_full_range_flag = false;
								}
							}
							else
							{
								if (ptr_next->in_pos_xs > temp)/* non-full range input */
								{
									/* select max xs pos */
									if (out_pos_xs < ptr_next->in_pos_xs)
									{
										out_pos_xs = ptr_next->in_pos_xs;
									}
								}
								/* full range input do nothing */
							}
						}
						else
						{
							/* sub-in with multi-out */
							if (count)
							{
								if ((out_pos_xs != ptr_next->in_pos_xs) || (out_pos_xe != ptr_next->in_pos_xe))
								{
									result = ISP_MESSAGE_DIFF_NEXT_CONFIG_ERROR;
									tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
									return result;
								}
							}
							else
							{
								out_pos_xs = ptr_next->in_pos_xs;
								out_pos_xe = ptr_next->in_pos_xe;
							}
						}
						count++;
					}
				}
				/* update h_end_flag */
				ptr_func->h_end_flag = h_end_flag;
				ptr_func->crop_h_end_flag = crop_h_end_flag;
				if (count)
				{
					if ((false == ptr_tile_reg_map->first_pass) && (false == ptr_tile_reg_map->first_frame) &&
						ptr_tile_reg_map->tdr_ctrl_en && (TILE_RUN_MODE_MAIN == ptr_func->run_mode))
					{
						if (ptr_func->min_tile_out_pos_xs < out_pos_xs)
						{
							out_pos_xs = ptr_func->min_tile_out_pos_xs;
						}
					}
					ptr_func->out_pos_xs = out_pos_xs;
					ptr_func->out_pos_xe = out_pos_xe;
					ptr_func->max_h_edge_flag = max_h_edge_flag;
					/* update tdr_h_disable_flag changed during back cal by sub-in */
					ptr_func->tdr_h_disable_flag = false;
					/* out_tile_width_max */
					ptr_func->out_tile_width_max = out_tile_width_max;
					/* out_tile_width_max_str */
					ptr_func->out_tile_width_max_str = out_tile_width_max_str;
					/* out_tile_width_max_end */
					ptr_func->out_tile_width_max_end = out_tile_width_max_end;
					/* smart tile + ufd */
					ptr_func->out_tile_width_loss = out_tile_width_loss;
					ptr_func->min_out_crop_xs = min_out_crop_xs;
					ptr_func->min_tile_crop_out_pos_xs = min_tile_crop_out_pos_xs;
				}
				else
				{
					ptr_func->tdr_h_disable_flag = true;
				}
			}
			if (false == ptr_func->tdr_h_disable_flag)
			{
				/* left edge */
				if (ptr_func->out_pos_xs <= 0)
				{
					if (ptr_func->out_tile_width_max_end)
					{
						if (ptr_func->out_tile_width_max_end < ptr_func->out_pos_xe + 1)
						{
							ptr_func->out_pos_xs = ptr_func->out_const_x;
							/* update flag */
							ptr_func->max_h_edge_flag = false;
							ptr_func->crop_h_end_flag = false;
							ptr_func->h_end_flag = false;
						}
					}
				}
				/* right edge */
				if (ptr_func->out_pos_xe + 1 >= ptr_func->full_size_x_out)
				{
					if (ptr_func->out_tile_width_max_str)
					{
						if (ptr_func->out_tile_width_max_str + ptr_func->out_pos_xs <  ptr_func->full_size_x_out)
						{
							int out_const_x = ptr_func->out_const_x;
							ptr_func->out_pos_xs = ptr_func->full_size_x_out - ptr_func->out_tile_width_max_str;
							/* update flag */
							ptr_func->max_h_edge_flag = false;
							ptr_func->crop_h_end_flag = false;
							ptr_func->h_end_flag = false;
							if (out_const_x > 1)
							{
								/* shift in_pos_xs */
								int val_s = TILE_MOD(ptr_func->out_pos_xs, out_const_x);
								if (val_s)
								{
									ptr_func->out_pos_xs += out_const_x - val_s;
								}
							}
						}
					}
				}
				/* check over tile size with enable & skip buffer check false */
				if (ptr_func->out_tile_width || ptr_func->out_tile_width_max)
				{
					int out_tile_width = ptr_func->out_tile_width;
					if (out_tile_width)
					{
						if (ptr_func->out_tile_width_max)
						{
							if (ptr_func->out_tile_width > ptr_func->out_tile_width_max)
							{
								out_tile_width = ptr_func->out_tile_width_max;
							}
						}
					}
					else
					{
						out_tile_width = ptr_func->out_tile_width_max;
					}
					if (ptr_func->out_pos_xe + 1 > ptr_func->out_pos_xs + out_tile_width)
					{
						/* update flag */
						ptr_func->max_h_edge_flag = false;
						ptr_func->crop_h_end_flag = false;
					}
					/* tile size constraint check */
					if ((false == ptr_tile_reg_map->first_pass) && (false == ptr_tile_reg_map->first_frame) &&
						ptr_tile_reg_map->tdr_ctrl_en &&
						(TILE_RUN_MODE_MAIN == ptr_func->run_mode) && ptr_func->out_max_width)
					{
						if (ptr_func->out_max_width < out_tile_width)
						{
							if (ptr_func->out_pos_xe + 1 > ptr_func->min_tile_out_pos_xs + ptr_func->out_max_width)
							{
								out_tile_width = ptr_func->out_pos_xe - ptr_func->min_tile_out_pos_xs + 1;
							}
							else
							{
								out_tile_width = ptr_func->out_max_width;
							}
						}
					}
					if (ptr_func->out_pos_xe + 1 > ptr_func->out_pos_xs + out_tile_width)
					{
						int out_const_x = ptr_func->out_const_x;
						ptr_func->out_pos_xs = ptr_func->out_pos_xe - out_tile_width + 1;
						/* only update h_end_flag */
						ptr_func->h_end_flag = false;
					    if (1 < out_const_x)
					    {
						    int val_s = TILE_MOD(ptr_func->out_pos_xs, out_const_x);
						    if (0 != val_s)
						    {
							    ptr_func->out_pos_xs += out_const_x - val_s;/* increae xs */
						    }
					    }
					}
				}
				else if (ptr_func->out_max_width)
				{
					if ((false == ptr_tile_reg_map->first_pass) && (false == ptr_tile_reg_map->first_frame) &&
						ptr_tile_reg_map->tdr_ctrl_en &&
						(TILE_RUN_MODE_MAIN == ptr_func->run_mode))
					{
						int out_tile_width = ptr_func->out_max_width;
						if (ptr_func->out_pos_xs + out_tile_width > ptr_func->min_tile_out_pos_xe + 1)
						{
							if (ptr_func->out_pos_xe + 1 > ptr_func->out_pos_xs + out_tile_width)
							{
								int out_const_x = ptr_func->out_const_x;
								ptr_func->out_pos_xs = ptr_func->out_pos_xe - out_tile_width + 1;
								/* only update h_end_flag */
								ptr_func->h_end_flag = false;
								if (1 < out_const_x)
								{
									int val_s = TILE_MOD(ptr_func->out_pos_xs, out_const_x);
									if (0 != val_s)
									{
										ptr_func->out_pos_xs += out_const_x - val_s;/* increae xs */
									}
								}
							}
						}
					}
				}
				if (ISP_MESSAGE_TILE_OK == result)
				{
					result = tile_check_output_config_x_inv(ptr_func, ptr_tile_reg_map);
				}
			}
		}
        /* backup direct flag */
	    ptr_func->direct_out_pos_xs = ptr_func->out_pos_xs;
	    ptr_func->direct_out_pos_xe = ptr_func->out_pos_xe;
	    ptr_func->direct_h_end_flag = ptr_func->h_end_flag;
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_backward_output_config_y(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
														  FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
	if (false == ptr_tile_reg_map->skip_y_cal)
	{
		/* only check last & not skipped */
		if (false == ptr_func->tdr_v_disable_flag)
		{
			/* tile movement */
			if (0 == ptr_func->tot_branch_num)/* end module only */
			{
				/* direct link */
				if (0x0 == (ptr_func->type & TILE_TYPE_DONT_CARE_END))
				{
					/* init output size of final module */
					if (ptr_func->valid_v_no == 0)
					{
						/* first tile row */
						ptr_func->out_pos_ys = ptr_func->min_out_pos_ys;
						ptr_func->out_pos_ye = ptr_func->out_tile_height?
							(ptr_func->min_out_pos_ys + ptr_func->out_tile_height - 1):ptr_func->max_out_pos_ye;
					}
					else/* middle row */
					{
						if (ptr_func->last_output_ye_pos < ptr_func->max_out_pos_ye)
						{
							ptr_func->out_pos_ys = ptr_func->last_output_ye_pos + 1;
							ptr_func->out_pos_ye = ptr_func->out_tile_height?
								(ptr_func->last_output_ye_pos + ptr_func->out_tile_height):ptr_func->max_out_pos_ye;
						}
						else
						{
							ptr_func->out_pos_ys = ptr_func->last_output_ye_pos - ptr_func->out_const_y + 1;
							ptr_func->out_pos_ye = ptr_func->last_output_ye_pos;
						}
					}
				    if (1 < ptr_func->out_const_y)
				    {
					    int val_e = TILE_MOD(ptr_func->out_pos_ye + 1, ptr_func->out_const_y);
					    if (0 != val_e)
					    {
						    ptr_func->out_pos_ye -= val_e;/* decreae ye */
					    }
				    }
					/* check size */
					if (ptr_func->out_pos_ye >= ptr_func->max_out_pos_ye)
					{
						ptr_func->out_pos_ye = ptr_func->max_out_pos_ye;
						ptr_func->max_v_edge_flag = true;
						ptr_func->v_end_flag = true;
						ptr_func->crop_v_end_flag = true;
					}
					else
					{
						ptr_func->v_end_flag = false;
						ptr_func->crop_v_end_flag = false;
						if (ptr_func->out_tile_height && (ptr_func->out_pos_ys + ptr_func->out_tile_height < ptr_func->max_out_pos_ye + 1))
						{
							ptr_func->max_v_edge_flag = false;
						}
						else
						{
							ptr_func->max_v_edge_flag = true;
						}
					}
				}
				else/* direct link */
				{
					ptr_func->out_pos_ys = ptr_func->direct_out_pos_ys;
					ptr_func->out_pos_ye = ptr_func->direct_out_pos_ye;
					ptr_func->v_end_flag = ptr_func->direct_v_end_flag;
					ptr_func->crop_v_end_flag = ptr_func->direct_v_end_flag;
				}
				/* out_tile_height_max */
				ptr_func->out_tile_height_max = ptr_func->out_tile_height;
				/* out_tile_height_max_str */
				ptr_func->out_tile_height_max_str = ptr_func->out_tile_height;
				/* out_tile_height_max_end */
				ptr_func->out_tile_height_max_end = ptr_func->out_tile_height;
			}
			else if (1 == ptr_func->tot_branch_num)/* check non-branch */
			{
				/* set curr out with next in */
				TILE_FUNC_BLOCK_STRUCT *ptr_next =
					ptr_tile_func_param->func_list[ptr_func->next_blk_num[0]];
				ptr_func->v_end_flag = ptr_next->v_end_flag;
				ptr_func->crop_v_end_flag = ptr_next->crop_v_end_flag;
				/* update tdr_v_disable_flag for change during back cal by sub-in */
				ptr_func->tdr_v_disable_flag = ptr_next->tdr_v_disable_flag;
				/* backward v_end_flag */
				if (false == ptr_next->tdr_v_disable_flag)
				{
					ptr_func->out_pos_ys = ptr_next->in_pos_ys;
					ptr_func->out_pos_ye = ptr_next->in_pos_ye;
					ptr_func->max_v_edge_flag = ptr_next->max_v_edge_flag;
					/* out_tile_height_max */
					ptr_func->out_tile_height_max = ptr_next->in_tile_height_max;
					/* out_tile_height_max_str */
					ptr_func->out_tile_height_max_str = ptr_next->in_tile_height_max_str;
					/* out_tile_height_max_end */
					ptr_func->out_tile_height_max_end = ptr_next->in_tile_height_max_end;
				}
			}
			else/* branch */
			{
				int i;
				/* diff view check */
				int out_pos_ys = MAX_SIZE;/* select min start pos */
				int out_pos_ye = 0;/* select max end pos */
				bool max_v_edge_flag = true;
				bool v_end_flag = true;
				bool crop_v_end_flag = true;
				bool last_full_range_flag = true;
				int count = 0;
				int tile_reg_map_run_mode = ptr_tile_reg_map->run_mode;
				/* out_tile_height_max */
				int out_tile_height_max = ptr_func->out_tile_height;
				/* out_tile_height_max_str */
				int out_tile_height_max_str = ptr_func->out_tile_height;
				/* out_tile_height_max_end */
				int out_tile_height_max_end = ptr_func->out_tile_height;
				/* min xs/ys & min xe/ye sorting for current support tile mode */
				for (i=0;i<ptr_func->tot_branch_num;i++)
				{
					TILE_FUNC_BLOCK_STRUCT *ptr_next =
						ptr_tile_func_param->func_list[
						ptr_func->next_blk_num[i]];
					int temp = ptr_next->max_in_pos_ye;
					/* skip output disabled branch */
					if (ptr_next->output_disable_flag)
					{
						continue;
					}
					if (tile_reg_map_run_mode == (tile_reg_map_run_mode & ptr_next->run_mode))
					{
						v_end_flag &= ptr_next->v_end_flag;
						crop_v_end_flag &= ptr_next->crop_v_end_flag;
						/* tdr_v_disable_flag changed during back cal by sub-in */
						if (ptr_next->tdr_v_disable_flag)
						{
							continue;
						}
						max_v_edge_flag &= ptr_next->max_v_edge_flag;
						/* out_tile_height_max */
						if (out_tile_height_max && ptr_next->in_tile_height_max)
						{
							if (out_tile_height_max > ptr_next->in_tile_height_max)
							{
								out_tile_height_max = ptr_next->in_tile_height_max;
							}
						}
						else if (ptr_next->in_tile_height_max)
						{
							out_tile_height_max = ptr_next->in_tile_height_max;
						}
						/* out_tile_height_max_str */
						if (out_tile_height_max_str && ptr_next->in_tile_height_max_str)
						{
							if (out_tile_height_max_str > ptr_next->in_tile_height_max_str)
							{
								out_tile_height_max_str = ptr_next->in_tile_height_max_str;
							}
						}
						else if (ptr_next->in_tile_height_max_str)
						{
							out_tile_height_max_str = ptr_next->in_tile_height_max_str;
						}
						/* out_tile_height_max_end */
						if (out_tile_height_max_end && ptr_next->in_tile_height_max_end)
						{
							if (out_tile_height_max_end > ptr_next->in_tile_height_max_end)
							{
								out_tile_height_max_end = ptr_next->in_tile_height_max_end;
							}
						}
						else if (ptr_next->in_tile_height_max_end)
						{
							out_tile_height_max_end = ptr_next->in_tile_height_max_end;
						}
						if (TILE_RUN_MODE_MAIN == tile_reg_map_run_mode)
						{
							/* select min pos */
							if (out_pos_ys > ptr_next->in_pos_ys)
							{
								out_pos_ys = ptr_next->in_pos_ys;
							}
							if (last_full_range_flag)/* last full range */
							{
								if (ptr_next->in_pos_ye >= temp)/* full range input */
								{
									/* select max ye pos */
									if (out_pos_ye < ptr_next->in_pos_ye)
									{
										out_pos_ye = ptr_next->in_pos_ye;
									}
								}
								else
								{
									/* non-full range input */
									out_pos_ye = ptr_next->in_pos_ye;
									last_full_range_flag = false;
								}
							}
							else
							{
								if (ptr_next->in_pos_ye < temp)/* non-full range input */
								{
									/* select min xe pos */
									if (out_pos_ye > ptr_next->in_pos_ye)
									{
										out_pos_ye = ptr_next->in_pos_ye;
									}
								}
								/* full range input do nothing */
							}
						}
						else
						{
							/* sub-in with multi-out */
							if (count)
							{
								if ((out_pos_ys != ptr_next->in_pos_ys) || (out_pos_ye != ptr_next->in_pos_ye))
								{
									result = ISP_MESSAGE_DIFF_NEXT_CONFIG_ERROR;
									tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
									return result;
								}
							}
							else
							{
								out_pos_ys = ptr_next->in_pos_ys;
								out_pos_ye = ptr_next->in_pos_ye;
							}
						}
						count++;
					}
				}
				/* update v_end_flag */
				ptr_func->v_end_flag = v_end_flag;
				ptr_func->crop_v_end_flag = crop_v_end_flag;
				if (count)
				{
					if ((false == ptr_tile_reg_map->first_pass) && (false == ptr_tile_reg_map->first_frame) &&
						ptr_tile_reg_map->tdr_ctrl_en && (TILE_RUN_MODE_MAIN == ptr_func->run_mode))
					{
						if (ptr_func->min_tile_out_pos_ye > out_pos_ye)
						{
							out_pos_ye = ptr_func->min_tile_out_pos_ye;
						}
					}
					ptr_func->out_pos_ys = out_pos_ys;
					ptr_func->out_pos_ye = out_pos_ye;
					ptr_func->max_v_edge_flag = max_v_edge_flag;
					/* update tdr_v_disable_flag changed during back cal by sub-in */
					ptr_func->tdr_v_disable_flag = false;
					/* out_tile_height_max */
					ptr_func->out_tile_height_max = out_tile_height_max;
					/* out_tile_height_max_str */
					ptr_func->out_tile_height_max_str = out_tile_height_max_str;
					/* out_tile_height_max_end */
					ptr_func->out_tile_height_max_end = out_tile_height_max_end;
				}
				else
				{
					ptr_func->tdr_v_disable_flag = true;
				}
			}
			if (false == ptr_func->tdr_v_disable_flag)
			{
				if (ptr_func->out_pos_ye + 1 >= ptr_func->full_size_y_out)
				{
					if (ptr_func->out_tile_height_max_end)
					{
						if (ptr_func->out_tile_height_max_end + ptr_func->out_pos_ys < ptr_func->full_size_y_out)
						{
							ptr_func->out_pos_ye = ptr_func->full_size_y_out - 1 - ptr_func->out_const_y;
							ptr_func->max_v_edge_flag = false;
							ptr_func->crop_v_end_flag = false;
							ptr_func->v_end_flag = false;
						}
					}
				}
				/* top edge */
				if (ptr_func->out_pos_ys <= 0)
				{
					if (ptr_func->out_tile_height_max_str)
					{
						if (ptr_func->out_tile_height_max_str < ptr_func->out_pos_ye + 1)
						{
							int out_const_y = ptr_func->out_const_y;
							ptr_func->out_pos_ye = ptr_func->out_tile_height_max_str - 1;
							ptr_func->max_v_edge_flag = false;
							ptr_func->crop_v_end_flag = false;
							ptr_func->v_end_flag = false;
							if (out_const_y > 1)
							{
								/* shift in_pos_ye */
								int val_e = TILE_MOD(ptr_func->out_pos_ye + 1, out_const_y);
								if (val_e)
								{
									ptr_func->out_pos_ye -= val_e;
								}
							}
						}
					}
				}
				/* check over tile size */
				if (ptr_func->out_tile_height || ptr_func->out_tile_height_max)
				{
					int out_tile_height = ptr_func->out_tile_height;
					if (out_tile_height)
					{
						if (ptr_func->out_tile_height_max)
						{
							if (ptr_func->out_tile_height > ptr_func->out_tile_height_max)
							{
								out_tile_height = ptr_func->out_tile_height_max;
							}
						}
					}
					else
					{
						out_tile_height = ptr_func->out_tile_height_max;
					}
					if (ptr_func->out_pos_ye + 1 > ptr_func->out_pos_ys + out_tile_height)
					{
						ptr_func->max_v_edge_flag = false;
						ptr_func->crop_v_end_flag = false;
					}
					/* tile size constraint check */
					if ((false == ptr_tile_reg_map->first_pass) && (false == ptr_tile_reg_map->first_frame) &&
						ptr_tile_reg_map->tdr_ctrl_en &&
						(TILE_RUN_MODE_MAIN == ptr_func->run_mode) && ptr_func->out_max_height)
					{
						if (ptr_func->out_max_height < out_tile_height)
						{
							if (ptr_func->out_pos_ys + ptr_func->out_max_height < ptr_func->min_tile_out_pos_ye + 1)
							{
								out_tile_height = ptr_func->min_tile_out_pos_ye - ptr_func->out_pos_ys + 1;
							}
							else
							{
								out_tile_height = ptr_func->out_max_height;
							}
						}
					}
					if (ptr_func->out_pos_ye + 1 > ptr_func->out_pos_ys + out_tile_height)
					{
						int out_const_y = ptr_func->out_const_y;
						ptr_func->out_pos_ye = ptr_func->out_pos_ys + out_tile_height - 1;
						/* only update v_end_flag */
						ptr_func->v_end_flag = false;
					    if (1 < out_const_y)
					    {
						    int val_e = TILE_MOD(ptr_func->out_pos_ye + 1, out_const_y);
						    if (0 != val_e)
						    {
							    ptr_func->out_pos_ye -= val_e;/* decreae ye */
						    }
					    }
					}
				}
				else if (ptr_func->out_max_height)
				{
					if ((false == ptr_tile_reg_map->first_pass) && (false == ptr_tile_reg_map->first_frame) &&
						ptr_tile_reg_map->tdr_ctrl_en &&
						(TILE_RUN_MODE_MAIN == ptr_func->run_mode))
					{
						int out_tile_height = ptr_func->out_max_height;
						if (ptr_func->out_pos_ys + ptr_func->out_max_height > ptr_func->min_tile_out_pos_ye + 1)
						{
							if (ptr_func->out_pos_ye + 1 > ptr_func->out_pos_ys + out_tile_height)
							{
								int out_const_y = ptr_func->out_const_y;
								ptr_func->out_pos_ye = ptr_func->out_pos_ys + out_tile_height - 1;
								/* only update v_end_flag */
								ptr_func->v_end_flag = false;
								if (1 < out_const_y)
								{
									int val_e = TILE_MOD(ptr_func->out_pos_ye + 1, out_const_y);
									if (0 != val_e)
									{
										ptr_func->out_pos_ye -= val_e;/* decreae ye */
									}
								}
							}
						}
					}
				}
				if (ISP_MESSAGE_TILE_OK == result)
				{
					result = tile_check_output_config_y(ptr_func, ptr_tile_reg_map);
				}
			}
		}
        /* backup direct flag */
	    ptr_func->direct_out_pos_ys = ptr_func->out_pos_ys;
	    ptr_func->direct_out_pos_ye = ptr_func->out_pos_ye;
	    ptr_func->direct_v_end_flag = ptr_func->v_end_flag;
	}
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_backward_output_config(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
													FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    /* check cal order, output */
    if (ptr_func->out_cal_order & TILE_ORDER_RIGHT_TO_LEFT)
    {
        result = tile_backward_output_config_x_inv(ptr_func, ptr_tile_reg_map, ptr_tile_func_param);
    }
    else
    {
        result = tile_backward_output_config_x(ptr_func, ptr_tile_reg_map, ptr_tile_func_param);
    }
    if (ISP_MESSAGE_TILE_OK == result)
    {
        result = tile_backward_output_config_y(ptr_func, ptr_tile_reg_map, ptr_tile_func_param);
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_backward_min_tile_backup_input(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
	/* backup min tile config */
	if (false == ptr_tile_reg_map->skip_x_cal)
	{
		ptr_func->min_cal_tdr_h_disable_flag = ptr_func->tdr_h_disable_flag;
		ptr_func->min_cal_h_end_flag = ptr_func->h_end_flag;
		if (false == ptr_func->tdr_h_disable_flag)
		{
			ptr_func->min_tile_in_pos_xs = ptr_func->in_pos_xs;
			ptr_func->min_tile_in_pos_xe = ptr_func->in_pos_xe;
			ptr_func->min_tile_out_pos_xs = ptr_func->out_pos_xs;
			ptr_func->min_tile_out_pos_xe = ptr_func->out_pos_xe;
			ptr_func->min_cal_max_h_edge_flag = ptr_func->max_h_edge_flag;
		}
	}
	if (false == ptr_tile_reg_map->skip_y_cal)
	{
		ptr_func->min_cal_tdr_v_disable_flag = ptr_func->tdr_v_disable_flag;
		ptr_func->min_cal_v_end_flag = ptr_func->v_end_flag;
		if (false == ptr_func->tdr_v_disable_flag)
		{
			ptr_func->min_tile_in_pos_ys = ptr_func->in_pos_ys;
			ptr_func->min_tile_in_pos_ye = ptr_func->in_pos_ye;
			ptr_func->min_tile_out_pos_ys = ptr_func->out_pos_ys;
			ptr_func->min_tile_out_pos_ye = ptr_func->out_pos_ye;
			ptr_func->min_cal_max_v_edge_flag = ptr_func->max_v_edge_flag;
		}
	}
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_backward_min_tile_init(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
	/* to set no tile size constraint for correct min tile backward */
	ptr_func->in_tile_width = 0;
	ptr_func->out_tile_width = 0;
	/* to set no tile size constraint for correct min tile backward */
	ptr_func->in_tile_height = 0;
	ptr_func->out_tile_height = 0;
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_backward_min_tile_restore(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
	/* to restore tile size constraint for normal tile backward */
	ptr_func->in_tile_width = ptr_func->in_tile_width_backup;
	ptr_func->out_tile_width = ptr_func->out_tile_width_backup;
	/* to restore tile size constraint for normal tile backward */
	ptr_func->in_tile_height = ptr_func->in_tile_height_backup;
	ptr_func->out_tile_height = ptr_func->out_tile_height_backup;
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_backward_output_config_min_tile(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
															 FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
	/* check cal order, output */
	if (ptr_func->out_cal_order & TILE_ORDER_RIGHT_TO_LEFT)
	{
		result = tile_backward_output_config_x_inv_min_tile(ptr_func, ptr_tile_reg_map, ptr_tile_func_param);
	}
	else
	{
		result = tile_backward_output_config_x_min_tile(ptr_func, ptr_tile_reg_map, ptr_tile_func_param);
	}
	if (ISP_MESSAGE_TILE_OK == result)
	{
		result = tile_backward_output_config_y_min_tile(ptr_func, ptr_tile_reg_map, ptr_tile_func_param);
	}
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_backward_output_config_x_min_tile(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
															 FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
	/* only check last & non-skipped */
	if (false == ptr_tile_reg_map->skip_x_cal)
	{
		/* tile movement */
		if (0 == ptr_func->tot_branch_num)/* end module only */
		{
			/* no direct link */
			if (0x0 == (ptr_func->type & TILE_TYPE_DONT_CARE_END))
			{
				/* init output size of final module */
				if (ptr_func->valid_h_no == 0)
				{
					/* first tile */
					ptr_func->out_pos_xs = ptr_func->min_out_pos_xs;
					ptr_func->out_pos_xe = ptr_func->min_out_pos_xs + ptr_func->out_const_x - 1;
					ptr_func->tdr_h_disable_flag = false;
				}
				else
				{
					/* move right to set output pos */
					if (ptr_func->last_output_xe_pos < ptr_func->max_out_pos_xe)
					{
						ptr_func->out_pos_xs = ptr_func->last_output_xe_pos + 1;
						ptr_func->out_pos_xe =ptr_func->last_output_xe_pos + ptr_func->out_const_x;
						ptr_func->tdr_h_disable_flag = false;
					}
					else
					{
						/* end of tile */
						ptr_func->tdr_h_disable_flag = true;
					}
				}
				/* check size equal to full size */
				if (false == ptr_func->tdr_h_disable_flag)
				{
					/* not updated by back direct-link */
					if (ptr_func->out_pos_xe >= ptr_func->max_out_pos_xe)
					{
						ptr_func->out_pos_xe = ptr_func->max_out_pos_xe;
						ptr_func->max_h_edge_flag = true;
						ptr_func->h_end_flag = true;
					}
					else
					{
						ptr_func->h_end_flag = false;
						if (ptr_func->out_tile_width && (ptr_func->out_pos_xs + ptr_func->out_tile_width < ptr_func->max_out_pos_xe + 1))
						{
							ptr_func->max_h_edge_flag = false;
						}
						else
						{
							ptr_func->max_h_edge_flag = true;
						}
					}
				}
				else
				{
					/* end of tile */
					ptr_func->h_end_flag = true;
				}
			}
			else/* direct link */
			{
				if (false == ptr_func->tdr_h_disable_flag)
				{
					/* copy from min tile config */
					ptr_func->out_pos_xs = ptr_func->min_tile_out_pos_xs;
					ptr_func->out_pos_xe = ptr_func->min_tile_out_pos_xe;
				}
				else
				{
					/* update for backward because of skipping normal back config */
					ptr_func->h_end_flag = ptr_func->direct_h_end_flag;
				}
			}
		}
		else if (1 == ptr_func->tot_branch_num)/* check non-branch */
		{
			/* set curr out with next in */
			TILE_FUNC_BLOCK_STRUCT *ptr_next =
				ptr_tile_func_param->func_list[ptr_func->next_blk_num[0]];
			/* necessary info for backward */
			ptr_func->tdr_h_disable_flag = ptr_next->tdr_h_disable_flag;
			ptr_func->h_end_flag = ptr_next->h_end_flag;
			if (false == ptr_next->tdr_h_disable_flag)
			{
				ptr_func->out_pos_xs = ptr_next->in_pos_xs;
				ptr_func->out_pos_xe = ptr_next->in_pos_xe;
				ptr_func->max_h_edge_flag = ptr_next->max_h_edge_flag;
			}
		}
		else/* branch */
		{
			int i;
			/* diff view check */
			int out_pos_xs = MAX_SIZE;/* select min xs pos */
			int out_pos_xe = 0;/* select max xe pos */
			bool max_h_edge_flag = true;
			bool h_end_flag = true;
			int count = 0;
			int tile_reg_map_run_mode = ptr_tile_reg_map->run_mode;
			/* min xs/ys & min xe/ye sorting for current support tile mode */
			for (i=0;i<ptr_func->tot_branch_num;i++)
			{
				TILE_FUNC_BLOCK_STRUCT *ptr_next =
					ptr_tile_func_param->func_list[ptr_func->next_blk_num[i]];
				/* skip output & tdr disabled branch */
				if (ptr_next->output_disable_flag)
				{
					continue;
				}
				if (tile_reg_map_run_mode == (tile_reg_map_run_mode & ptr_next->run_mode))
				{
					/* update h_end_flag for backward */
					h_end_flag &= ptr_next->h_end_flag;
					/* skip tdr disabled */
					if (ptr_next->tdr_h_disable_flag)
					{
						continue;
					}
					max_h_edge_flag &= ptr_next->max_h_edge_flag;
					/* min tile select min xs pos */
					if (out_pos_xs == ptr_next->in_pos_xs)
					{
						/* min tile select max xe pos */
						if (out_pos_xe < ptr_next->in_pos_xe)
						{
							out_pos_xe = ptr_next->in_pos_xe;
						}
					}
					else if (out_pos_xs > ptr_next->in_pos_xs)
					{
						out_pos_xs = ptr_next->in_pos_xs;
						out_pos_xe = ptr_next->in_pos_xe;
					}
					count++;
				}
			}
			/* update h_end_flag */
			ptr_func->h_end_flag = h_end_flag;
			/* no branches with tdr false */
			if (count)
			{
				ptr_func->out_pos_xs = out_pos_xs;
				ptr_func->out_pos_xe = out_pos_xe;
				ptr_func->max_h_edge_flag = max_h_edge_flag;
				ptr_func->tdr_h_disable_flag = false;
			}
			else
			{
				ptr_func->tdr_h_disable_flag = true;
			}
		}
		if (false == ptr_func->tdr_h_disable_flag)
		{
			result = tile_check_output_config_x(ptr_func, ptr_tile_reg_map);
		}
		/* direct link update */
		ptr_func->min_tile_out_pos_xs = ptr_func->out_pos_xs;
		ptr_func->min_tile_out_pos_xe = ptr_func->out_pos_xe;
		/* update crop_h_end_flag */
		ptr_func->crop_h_end_flag = ptr_func->h_end_flag;
		/* out_tile_width_max */
		ptr_func->out_tile_width_max = 0;
		/* out_tile_width_max_str */
		ptr_func->out_tile_width_max_str = 0;
		/* out_tile_width_max_end */
		ptr_func->out_tile_width_max_end = 0;
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_backward_output_config_x_inv_min_tile(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
															 FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
	/* only check last & not skipped */
	if (false == ptr_tile_reg_map->skip_x_cal)
	{
		/* tile movement */
		if (0 == ptr_func->tot_branch_num)/* end module only */
		{
			/* no direct link */
			if (0x0 == (ptr_func->type & TILE_TYPE_DONT_CARE_END))
			{
				/* init output size of final module */
				if (ptr_func->valid_h_no == 0) /* first tile */
				{
					/* top start */
					ptr_func->out_pos_xs = ptr_func->max_out_pos_xe - ptr_func->out_const_x + 1;
					ptr_func->out_pos_xe = ptr_func->max_out_pos_xe;
					ptr_func->tdr_h_disable_flag = false;
				}
				else
				{
					/* move left to set output pos */
					if (ptr_func->last_output_xs_pos > ptr_func->min_out_pos_xs)
					{
						ptr_func->out_pos_xs = ptr_func->last_output_xs_pos - ptr_func->out_const_x;
						ptr_func->out_pos_xe = ptr_func->last_output_xs_pos - 1;
						ptr_func->tdr_h_disable_flag = false;
					}
					else
					{
						/* end of tile */
						ptr_func->tdr_h_disable_flag = true;
					}
				}
				/* check size equal to full size */
				if (false == ptr_func->tdr_h_disable_flag)
				{
					/* not updated by back direct-link */
					if (ptr_func->out_pos_xs <= ptr_func->min_out_pos_xs)
					{
						ptr_func->out_pos_xs = ptr_func->min_out_pos_xs;
						ptr_func->max_h_edge_flag = true;
						ptr_func->h_end_flag = true;
					}
					else
					{
						ptr_func->h_end_flag = false;
						if (ptr_func->out_tile_width && (ptr_func->min_out_pos_xs + ptr_func->out_tile_width < ptr_func->out_pos_xe + 1))
						{
							ptr_func->max_h_edge_flag = false;
						}
						else
						{
							ptr_func->max_h_edge_flag = true;
						}
					}
				}
				else
				{
					/* end of tile */
					ptr_func->h_end_flag = true;
				}
			}
			else/* direct link */
			{
				if (false == ptr_func->tdr_h_disable_flag)
				{
					/* copy from min tile config */
					ptr_func->out_pos_xs = ptr_func->min_tile_out_pos_xs;
					ptr_func->out_pos_xe = ptr_func->min_tile_out_pos_xe;
				}
				else
				{
					/* update for backward because of skipping normal back config */
					ptr_func->h_end_flag = ptr_func->direct_h_end_flag;
				}
			}
		}
		else if (1 == ptr_func->tot_branch_num)/* check non-branch */
		{
			/* set curr out with next in */
			TILE_FUNC_BLOCK_STRUCT *ptr_next =
				ptr_tile_func_param->func_list[ptr_func->next_blk_num[0]];
			/* update for backward */
			ptr_func->tdr_h_disable_flag = ptr_next->tdr_h_disable_flag;
			ptr_func->h_end_flag = ptr_next->h_end_flag;
			/* backward h_end_flag */
			if (false == ptr_next->tdr_h_disable_flag)
			{
				ptr_func->out_pos_xs = ptr_next->in_pos_xs;
				ptr_func->out_pos_xe = ptr_next->in_pos_xe;
				ptr_func->max_h_edge_flag = ptr_next->max_h_edge_flag;
			}
		}
		else/* branch */
		{
			int i;
			/* diff view check */
			int out_pos_xs = MAX_SIZE;/* select min xs pos */
			int out_pos_xe = 0;/* select max xe pos */
			bool max_h_edge_flag = true;
			bool h_end_flag = true;
			int count = 0;
			int tile_reg_map_run_mode = ptr_tile_reg_map->run_mode;
			/* min xs/ys & min xe/ye sorting for current support tile mode */
			for (i=0;i<ptr_func->tot_branch_num;i++)
			{
				TILE_FUNC_BLOCK_STRUCT *ptr_next =
					ptr_tile_func_param->func_list[ptr_func->next_blk_num[i]];
				/* skip output & tdr disabled branch */
				if (ptr_next->output_disable_flag)
				{
					continue;
				}
				if (tile_reg_map_run_mode == (tile_reg_map_run_mode & ptr_next->run_mode))
				{
					/* update h_end_flag for backward */
					h_end_flag &= ptr_next->h_end_flag;
					/* skip tdr disabled */
					if (ptr_next->tdr_h_disable_flag)
					{
						continue;
					}
					max_h_edge_flag &= ptr_next->max_h_edge_flag;
					/* min tile select max xe pos */
					if (out_pos_xe == ptr_next->in_pos_xe)
					{
						/* min tile select min xs pos */
						if (out_pos_xs > ptr_next->in_pos_xs)
						{
							out_pos_xs = ptr_next->in_pos_xs;
						}
					}
					else if (out_pos_xe < ptr_next->in_pos_xe)
					{
						out_pos_xe = ptr_next->in_pos_xe;
						out_pos_xs = ptr_next->in_pos_xs;
					}
					count++;
				}
			}
			/* update h_end_flag */
			ptr_func->h_end_flag = h_end_flag;
			/* no branches with tdr false */
			if (count)
			{
				ptr_func->out_pos_xs = out_pos_xs;
				ptr_func->out_pos_xe = out_pos_xe;
				ptr_func->max_h_edge_flag = max_h_edge_flag;
				ptr_func->tdr_h_disable_flag = false;
			}
			else
			{
				ptr_func->tdr_h_disable_flag = true;
			}
		}
		if (false == ptr_func->tdr_h_disable_flag)
		{
			result = tile_check_output_config_x_inv(ptr_func, ptr_tile_reg_map);
		}
		/* direct link update */
		ptr_func->min_tile_out_pos_xs = ptr_func->out_pos_xs;
		ptr_func->min_tile_out_pos_xe = ptr_func->out_pos_xe;
		/* update crop_h_end_flag */
		ptr_func->crop_h_end_flag = ptr_func->h_end_flag;
		/* out_tile_width_max */
		ptr_func->out_tile_width_max = 0;
		/* out_tile_width_max_str */
		ptr_func->out_tile_width_max_str = 0;
		/* out_tile_width_max_end */
		ptr_func->out_tile_width_max_end = 0;
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_backward_output_config_y_min_tile(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
															 FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
	/* only check last & not skipped */
	if (false == ptr_tile_reg_map->skip_y_cal)
	{
		/* tile movement */
		if (0 == ptr_func->tot_branch_num)/* end module only */
		{
			if (0x0 == (ptr_func->type & TILE_TYPE_DONT_CARE_END))
			{
				/* init output size of final module */
				if (ptr_func->valid_v_no == 0)
				{
					/* first tile row */
					ptr_func->out_pos_ys = ptr_func->min_out_pos_ys;
					ptr_func->out_pos_ye = ptr_func->min_out_pos_ys + ptr_func->out_const_y - 1;
					ptr_func->tdr_v_disable_flag = false;
				}
				else/* middle row */
				{
					/* next left start to set output pos */
					if (ptr_func->last_output_ye_pos < ptr_func->max_out_pos_ye)
					{
						ptr_func->out_pos_ys = ptr_func->last_output_ye_pos + 1;
						ptr_func->out_pos_ye = ptr_func->last_output_ye_pos + ptr_func->out_const_y;
						ptr_func->tdr_v_disable_flag = false;
					}
					else
					{
						/* last tile is end */
						ptr_func->tdr_v_disable_flag = true;
					}
				}
				/* not updated by back direct-link */
				if (false == ptr_func->tdr_v_disable_flag)
				{
					if (ptr_func->out_pos_ye >= ptr_func->max_out_pos_ye)
					{
						ptr_func->out_pos_ye = ptr_func->max_out_pos_ye;
						ptr_func->max_v_edge_flag = true;
						ptr_func->v_end_flag = true;
					}
					else
					{
						ptr_func->v_end_flag = false;
						if (ptr_func->out_tile_height && (ptr_func->out_pos_ys + ptr_func->out_tile_height < ptr_func->max_out_pos_ye + 1))
						{
							ptr_func->max_v_edge_flag = false;
						}
						else
						{
							ptr_func->max_v_edge_flag = true;
						}
					}
				}
				else
				{
					/* update end flag for normal back */
					ptr_func->v_end_flag = true;
				}
			}
			else
			{
				if (false == ptr_func->tdr_v_disable_flag)
				{
					/* copy from min tile config */
					ptr_func->out_pos_ys = ptr_func->min_tile_out_pos_ys;
					ptr_func->out_pos_ye = ptr_func->min_tile_out_pos_ye;
				}
				else
				{
					/* update for backward because of skipping normal back config */
					ptr_func->v_end_flag = ptr_func->direct_v_end_flag;
				}
			}
		}
		else if (1 == ptr_func->tot_branch_num)/* check non-branch */
		{
			/* set curr out with next in */
			TILE_FUNC_BLOCK_STRUCT *ptr_next =
				ptr_tile_func_param->func_list[ptr_func->next_blk_num[0]];
			/* update for backward */
			ptr_func->tdr_v_disable_flag = ptr_next->tdr_v_disable_flag;
			ptr_func->v_end_flag = ptr_next->v_end_flag;
			if (false == ptr_next->tdr_v_disable_flag)
			{
				ptr_func->out_pos_ys = ptr_next->in_pos_ys;
				ptr_func->out_pos_ye = ptr_next->in_pos_ye;
				ptr_func->max_v_edge_flag = ptr_next->max_v_edge_flag;
			}
		}
		else/* branch */
		{
			int i;
			/* diff view check */
			int out_pos_ys = MAX_SIZE;/* select min start pos */
			int out_pos_ye = 0;/* select max end pos */
			bool max_v_edge_flag = true;
			bool v_end_flag = true;
			int count = 0;
			int tile_reg_map_run_mode = ptr_tile_reg_map->run_mode;
			/* min xs/ys & min xe/ye sorting for current support tile mode */
			for (i=0;i<ptr_func->tot_branch_num;i++)
			{
				TILE_FUNC_BLOCK_STRUCT *ptr_next =
					ptr_tile_func_param->func_list[ptr_func->next_blk_num[i]];
				/* skip output disabled branch */
				if (ptr_next->output_disable_flag)
				{
					continue;
				}
				if (tile_reg_map_run_mode == (tile_reg_map_run_mode & ptr_next->run_mode))
				{
					/* update v_end_flag */
					v_end_flag &= ptr_next->v_end_flag;
					/* skip when tdr disabled */
					if (ptr_next->tdr_v_disable_flag)
					{
						continue;
					}
					max_v_edge_flag &= ptr_next->max_v_edge_flag;
					/* min tile select min ys pos */
					if (out_pos_ys == ptr_next->in_pos_ys)
					{
						/* min tile select max ye pos */
						if (out_pos_ye < ptr_next->in_pos_ye)
						{
							out_pos_ye = ptr_next->in_pos_ye;
						}
					}
					else if (out_pos_ys > ptr_next->in_pos_ys)
					{
						out_pos_ys = ptr_next->in_pos_ys;
						out_pos_ye = ptr_next->in_pos_ye;
					}
					count++;
				}
			}
			/* update v_end_flag */
			ptr_func->v_end_flag = v_end_flag;
			/* no branches with tdr false */
			if (count)
			{
				ptr_func->out_pos_ys = out_pos_ys;
				ptr_func->out_pos_ye = out_pos_ye;
				ptr_func->max_v_edge_flag = max_v_edge_flag;
				ptr_func->tdr_v_disable_flag = false;
			}
			else
			{
				ptr_func->tdr_v_disable_flag = true;
			}
		}
		if (false == ptr_func->tdr_v_disable_flag)
		{
			result = tile_check_output_config_y(ptr_func, ptr_tile_reg_map);
		}
		/* direct link update */
		ptr_func->min_tile_out_pos_ys = ptr_func->out_pos_ys;
		ptr_func->min_tile_out_pos_ye = ptr_func->out_pos_ye;
		/* update crop_v_end_flag */
		ptr_func->crop_v_end_flag = ptr_func->v_end_flag;
		/* out_tile_height_max */
		ptr_func->out_tile_height_max = 0;
		/* out_tile_height_max_str */
		ptr_func->out_tile_height_max_str = 0;
		/* out_tile_height_max_end */
		ptr_func->out_tile_height_max_end = 0;
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_backward_input_check(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    if (false == ptr_tile_reg_map->skip_x_cal)
	{
		if (false == ptr_func->tdr_h_disable_flag)
		{
			/* check resizer input xe & ye by tile size with enable & skip buffer check false */
			if (ptr_func->in_tile_width)
			{
				if (ptr_func->in_pos_xe + 1 > ptr_func->in_pos_xs + ptr_func->in_tile_width)
				{
					result = ISP_MESSAGE_TILE_BACKWARD_IN_OVER_TILE_WIDTH_ERROR;
					tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
					return result;
				}
			}
			/* check tile edge flag by backward */
			if (0 == ptr_func->in_pos_xs)
			{
				if (TILE_EDGE_LEFT_MASK != (ptr_func->tdr_edge & TILE_EDGE_LEFT_MASK))
				{
					result = ISP_MESSAGE_BACKWARD_CHECK_LEFT_EDGE_ERROR;
					tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
					return result;
				}
			}
			else
			{
				if (ptr_func->tdr_edge & TILE_EDGE_LEFT_MASK)
				{
					result = ISP_MESSAGE_BACKWARD_CHECK_LEFT_EDGE_ERROR;
					tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
					return result;
				}
			}
			if (ptr_func->in_pos_xe + 1 >= ptr_func->full_size_x_in)
			{
				if (TILE_EDGE_RIGHT_MASK != (ptr_func->tdr_edge & TILE_EDGE_RIGHT_MASK))
				{
					result = ISP_MESSAGE_BACKWARD_CHECK_RIGHT_EDGE_ERROR;
					tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
					return result;
				}
			}
			else
			{
				if (ptr_func->tdr_edge & TILE_EDGE_RIGHT_MASK)
				{
					result = ISP_MESSAGE_BACKWARD_CHECK_RIGHT_EDGE_ERROR;
					tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
					return result;
				}
			}
		}
		else
		{
			if (TILE_RUN_MODE_MAIN == ptr_func->run_mode)
			{
				if (PREVIOUS_BLK_NO_OF_START == ptr_func->prev_blk_num[0])
				{
					result = ISP_MESSAGE_DIFF_VIEW_TILE_WIDTH_ERROR;
					tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
					return result;
				}
			}
		}
    }
    if (false == ptr_tile_reg_map->skip_y_cal)
	{
		if (false == ptr_func->tdr_v_disable_flag)
		{
			/* check resizer input xe & ye by tile size with enable & skip buffer check false */
			if (ptr_func->in_tile_height)
			{
				if (ptr_func->in_pos_ye + 1 > ptr_func->in_pos_ys + ptr_func->in_tile_height)
				{
					result = ISP_MESSAGE_TILE_BACKWARD_IN_OVER_TILE_HEIGHT_ERROR;
					tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
					return result;
				}
			}
			/* check tile edge flag by backward */
			if (0 == ptr_func->in_pos_ys)
			{
				if (TILE_EDGE_TOP_MASK != (ptr_func->tdr_edge & TILE_EDGE_TOP_MASK))
				{
					result = ISP_MESSAGE_BACKWARD_CHECK_TOP_EDGE_ERROR;
					tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
					return result;
				}
			}
			else
			{
				if (ptr_func->tdr_edge & TILE_EDGE_TOP_MASK)
				{
					result = ISP_MESSAGE_BACKWARD_CHECK_TOP_EDGE_ERROR;
					tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
					return result;
				}
			}
			if (ptr_func->in_pos_ye + 1 >= ptr_func->full_size_y_in)
			{
				if (TILE_EDGE_BOTTOM_MASK != (ptr_func->tdr_edge & TILE_EDGE_BOTTOM_MASK))
				{
					result = ISP_MESSAGE_BACKWARD_CHECK_BOTTOM_EDGE_ERROR;
					tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
					return result;
				}
			}
			else
			{
				if (ptr_func->tdr_edge & TILE_EDGE_BOTTOM_MASK)
				{
					result = ISP_MESSAGE_BACKWARD_CHECK_BOTTOM_EDGE_ERROR;
					tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
					return result;
				}
			}
		}
		else
		{
			if (TILE_RUN_MODE_MAIN == ptr_func->run_mode)
			{
				if (PREVIOUS_BLK_NO_OF_START == ptr_func->prev_blk_num[0])
				{
					result = ISP_MESSAGE_DIFF_VIEW_TILE_HEIGHT_ERROR;
					tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
					return result;
				}
			}
		}
    }
    if (ISP_MESSAGE_TILE_OK == result)
    {
        result = tile_check_input_config(ptr_func, ptr_tile_reg_map);
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_backward_by_func_pre_x(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
    if (false == ptr_tile_reg_map->skip_x_cal)
    {
		if (false == ptr_func->tdr_h_disable_flag)
		{
			int in_const_x = ptr_func->in_const_x;
			/* update output right edge */
			if (ptr_func->out_pos_xe + 1 >= ptr_func->full_size_x_out)
			{
				ptr_func->tdr_edge |= TILE_EDGE_RIGHT_MASK;
				ptr_func->out_pos_xe = ptr_func->full_size_x_out - 1;
			}
			else
			{
				ptr_func->tdr_edge &= ~TILE_EDGE_RIGHT_MASK;
			}
			/* update output left edge */
			if (ptr_func->out_pos_xs <= 0)
			{
				ptr_func->tdr_edge |= TILE_EDGE_LEFT_MASK;
				ptr_func->out_pos_xs = 0;
			}
			else
			{
				ptr_func->tdr_edge &= ~TILE_EDGE_LEFT_MASK;
			}
			/* set fixed left tile loss */
			if (TILE_EDGE_LEFT_MASK & ptr_func->tdr_edge)
			{
				ptr_func->in_pos_xs = ptr_func->out_pos_xs;
			}
			else
			{
				if (true == ptr_func->enable_flag)
				{
					ptr_func->in_pos_xs = ptr_func->out_pos_xs - ptr_func->l_tile_loss;
				}
				else
				{
					ptr_func->in_pos_xs = ptr_func->out_pos_xs;
				}
			}
			/* set fixed right tile loss */
			if (TILE_EDGE_RIGHT_MASK & ptr_func->tdr_edge)
			{
				ptr_func->in_pos_xe = ptr_func->out_pos_xe;
			}
			else
			{
				if (true == ptr_func->enable_flag)
				{
					ptr_func->in_pos_xe = ptr_func->out_pos_xe + ptr_func->r_tile_loss;
				}
				else
				{
					ptr_func->in_pos_xe = ptr_func->out_pos_xe;
				}
			}
			/* clip size */
			if (ptr_func->in_pos_xs < 0)
			{
				ptr_func->in_pos_xs = 0;
			}
			if (ptr_func->in_pos_xe + 1 > ptr_func->full_size_x_in)
			{
				ptr_func->in_pos_xe = ptr_func->full_size_x_in - 1;
			}
			/* align input position */
			if (in_const_x > 1)
			{
				int val_s = TILE_MOD(ptr_func->in_pos_xs, in_const_x);
				int val_e = TILE_MOD(ptr_func->in_pos_xe + 1, in_const_x);
				if (0 != val_s)
				{
					ptr_func->in_pos_xs -= val_s;
				}
				if (0 != val_e)
				{
					ptr_func->in_pos_xe += in_const_x - val_e;
				}
			}
		}
    }
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_backward_by_func_pre_y(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
    if (false == ptr_tile_reg_map->skip_y_cal)
    {
		if (false == ptr_func->tdr_v_disable_flag)
		{
			int in_const_y = ptr_func->in_const_y;
			/* update output bottom edge */
			if (ptr_func->out_pos_ye + 1 >= ptr_func->full_size_y_out)
			{
				ptr_func->tdr_edge |= TILE_EDGE_BOTTOM_MASK;
				ptr_func->out_pos_ye = ptr_func->full_size_y_out - 1;
			}
			else
			{
				ptr_func->tdr_edge &= ~TILE_EDGE_BOTTOM_MASK;
			}
			/* update output top edge */
			if (ptr_func->out_pos_ys <= 0)
			{
				ptr_func->tdr_edge |= TILE_EDGE_TOP_MASK;
				ptr_func->out_pos_ys = 0;
			}
			else
			{
				ptr_func->tdr_edge &= ~TILE_EDGE_TOP_MASK;
			}
			/* set fixed top tile loss */
			if (TILE_EDGE_TOP_MASK & ptr_func->tdr_edge)
			{
				ptr_func->in_pos_ys = ptr_func->out_pos_ys;
			}
			else
			{
				if (true == ptr_func->enable_flag)
				{
					ptr_func->in_pos_ys = ptr_func->out_pos_ys - ptr_func->t_tile_loss;
				}
				else
				{
					ptr_func->in_pos_ys = ptr_func->out_pos_ys;
				}
			}
			/* set fixed bottom tile loss */
			if (TILE_EDGE_BOTTOM_MASK & ptr_func->tdr_edge)
			{
				ptr_func->in_pos_ye = ptr_func->out_pos_ye;
			}
			else
			{
				if (true == ptr_func->enable_flag)
				{
					ptr_func->in_pos_ye = ptr_func->out_pos_ye + ptr_func->b_tile_loss;
				}
				else
				{
					ptr_func->in_pos_ye = ptr_func->out_pos_ye;
				}
			}
			/* clip size */
			if (ptr_func->in_pos_ys < 0)
			{
				ptr_func->in_pos_ys = 0;
			}
			if (ptr_func->in_pos_ye + 1 > ptr_func->full_size_y_in)
			{
				ptr_func->in_pos_ye = ptr_func->full_size_y_in - 1;
			}
			/* align input position */
			if (in_const_y > 1)
			{
				int val_s = TILE_MOD(ptr_func->in_pos_ys, in_const_y);
				int val_e = TILE_MOD(ptr_func->in_pos_ye + 1, in_const_y);
				if (0 != val_s)
				{
					ptr_func->in_pos_ys -= val_s;
				}
				if (0 != val_e)
				{
					ptr_func->in_pos_ye += in_const_y - val_e;
				}
			}
		}
    }
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_backward_by_func_post_x(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
	if (false == ptr_tile_reg_map->skip_x_cal)
	{
		if (false == ptr_func->tdr_h_disable_flag)
		{
			if (ptr_func->enable_flag)
			{
				/* check min x size */
				if (ptr_func->in_pos_xs + ptr_func->in_min_width > ptr_func->in_pos_xe + 1)
				{
					int min_xsize = ptr_func->in_min_width;
					int max_in_pos_xe = ptr_tile_reg_map->first_frame?(ptr_func->full_size_x_in - 1):ptr_func->max_in_pos_xe;
					ptr_func->in_pos_xe = ptr_func->in_pos_xs + min_xsize - 1;
					if (ptr_func->in_pos_xe >= max_in_pos_xe)
					{
						if (max_in_pos_xe + 1 >= ptr_func->full_size_x_in)
						{
							if (ptr_func->max_h_edge_flag)
							{
								ptr_func->in_pos_xe = max_in_pos_xe;
								ptr_func->in_pos_xs = max_in_pos_xe + 1 - min_xsize;
							}
							else
							{
								ptr_func->in_pos_xe = max_in_pos_xe - ptr_func->in_const_x;
								ptr_func->in_pos_xs = max_in_pos_xe + 1 - min_xsize - ptr_func->in_const_x;
							}
						}
						else
						{
							ptr_func->in_pos_xe = max_in_pos_xe;
							ptr_func->in_pos_xs = max_in_pos_xe + 1 - min_xsize;
						}
						/* align xs position */
						if (ptr_func->in_const_x > 1)
						{
							int val_s = TILE_MOD(ptr_func->in_pos_xs, ptr_func->in_const_x);
							if (0 != val_s)
							{
								ptr_func->in_pos_xs -= val_s;
							}
						}
					}
					/* align xe position */
					if (ptr_func->in_const_x > 1)
					{
						int val_e = TILE_MOD(ptr_func->in_pos_xe + 1, ptr_func->in_const_x);
						if (0 != val_e)
						{
							ptr_func->in_pos_xe += ptr_func->in_const_x - val_e;
						}
					}
				}
				/* check crop & update in_tile_width_max */
				if ((0x0 == (TILE_TYPE_CROP_EN & ptr_func->type)) || (ptr_func->type & TILE_TYPE_LOSS))
				{
					/* in_tile_width_max */
					if (ptr_func->in_tile_width && ptr_func->out_tile_width_max)
					{
						if (ptr_func->in_tile_width > ptr_func->out_tile_width_max + ptr_func->l_tile_loss + ptr_func->r_tile_loss)
						{
							ptr_func->in_tile_width_max = ptr_func->out_tile_width_max + ptr_func->l_tile_loss + ptr_func->r_tile_loss;
						}
						else
						{
							ptr_func->in_tile_width_max = ptr_func->in_tile_width;
						}
					}
					else if (ptr_func->out_tile_width_max)
					{
						ptr_func->in_tile_width_max = ptr_func->out_tile_width_max + ptr_func->l_tile_loss + ptr_func->r_tile_loss;
					}
					else
					{
						ptr_func->in_tile_width_max = ptr_func->in_tile_width;
					}
					/* in_tile_width_max_str */
					if (ptr_func->in_tile_width && ptr_func->out_tile_width_max_str)
					{
						if (ptr_func->in_tile_width > ptr_func->out_tile_width_max_str + ptr_func->r_tile_loss)
						{
							ptr_func->in_tile_width_max_str = ptr_func->out_tile_width_max_str + ptr_func->r_tile_loss;
						}
						else
						{
							ptr_func->in_tile_width_max_str = ptr_func->in_tile_width;
						}
					}
					else if (ptr_func->out_tile_width_max_str)
					{
						ptr_func->in_tile_width_max_str = ptr_func->out_tile_width_max_str + ptr_func->r_tile_loss;
					}
					else
					{
						ptr_func->in_tile_width_max_str = ptr_func->in_tile_width;
					}
					/* prevent from min edge error */
					if (ptr_func->out_tile_width_max_str && (ptr_func->out_tile_width_max_str < ptr_func->full_size_x_out) && (ptr_func->in_tile_width_max_str >= ptr_func->full_size_x_in))
					{
						ptr_func->in_tile_width_max_str = ptr_func->full_size_x_in - ptr_func->in_const_x;
					}
					/* in_tile_width_max_end */
					if (ptr_func->in_tile_width && ptr_func->out_tile_width_max_end)
					{
						if (ptr_func->in_tile_width > ptr_func->out_tile_width_max_end +ptr_func->l_tile_loss)
						{
							ptr_func->in_tile_width_max_end = ptr_func->out_tile_width_max_end + ptr_func->l_tile_loss;
						}
						else
						{
							ptr_func->in_tile_width_max_end = ptr_func->in_tile_width;
						}
					}
					else if (ptr_func->out_tile_width_max_end)
					{
						ptr_func->in_tile_width_max_end = ptr_func->out_tile_width_max_end + ptr_func->l_tile_loss;
					}
					else
					{
						ptr_func->in_tile_width_max_end = ptr_func->in_tile_width;
					}
					/* smart tile + ufd */
					ptr_func->in_tile_width_loss = ptr_func->out_tile_width_loss + ptr_func->l_tile_loss + ptr_func->r_tile_loss;
					if (ptr_func->max_out_crop_xe + ptr_func->r_tile_loss < ptr_func->max_in_pos_xe)
					{
						ptr_func->max_in_crop_xe = ptr_func->max_out_crop_xe + ptr_func->r_tile_loss;
					}
					else
					{
						ptr_func->max_in_crop_xe = ptr_func->max_in_pos_xe;
					}
					if (ptr_func->min_tile_crop_out_pos_xe + ptr_func->r_tile_loss < ptr_func->max_in_pos_xe)
					{
						ptr_func->min_tile_crop_in_pos_xe = ptr_func->min_tile_crop_out_pos_xe + ptr_func->r_tile_loss;
					}
					else
					{
						ptr_func->min_tile_crop_in_pos_xe = ptr_func->max_in_pos_xe;
					}
				}
				else
				{
					ptr_func->in_tile_width_max = ptr_func->in_tile_width;
					ptr_func->in_tile_width_max_str = ptr_func->in_tile_width;
					ptr_func->in_tile_width_max_end = ptr_func->in_tile_width;
					/* smart tile + ufd */
					ptr_func->in_tile_width_loss = 0;
					ptr_func->max_in_crop_xe = ptr_func->max_in_pos_xe;
					ptr_func->min_tile_crop_in_pos_xe = ptr_func->min_tile_in_pos_xe;
				}
			}
			else
			{
				/* in_tile_width_max */
				if (ptr_func->in_tile_width && ptr_func->out_tile_width_max)
				{
					if (ptr_func->in_tile_width > ptr_func->out_tile_width_max)
					{
						ptr_func->in_tile_width_max = ptr_func->out_tile_width_max;
					}
					else
					{
						ptr_func->in_tile_width_max = ptr_func->in_tile_width;
					}
				}
				else if (ptr_func->out_tile_width_max)
				{
					ptr_func->in_tile_width_max = ptr_func->out_tile_width_max;
				}
				else
				{
					ptr_func->in_tile_width_max = ptr_func->in_tile_width;
				}
				/* in_tile_width_max_str */
				if (ptr_func->in_tile_width && ptr_func->out_tile_width_max_str)
				{
					if (ptr_func->in_tile_width > ptr_func->out_tile_width_max_str)
					{
						ptr_func->in_tile_width_max_str = ptr_func->out_tile_width_max_str;
					}
					else
					{
						ptr_func->in_tile_width_max_str = ptr_func->in_tile_width;
					}
				}
				else if (ptr_func->out_tile_width_max_str)
				{
					ptr_func->in_tile_width_max_str = ptr_func->out_tile_width_max_str;
				}
				else
				{
					ptr_func->in_tile_width_max_str = ptr_func->in_tile_width;
				}
				/* in_tile_width_max_end */
				if (ptr_func->in_tile_width && ptr_func->out_tile_width_max_end)
				{
					if (ptr_func->in_tile_width > ptr_func->out_tile_width_max_end)
					{
						ptr_func->in_tile_width_max_end = ptr_func->out_tile_width_max_end;
					}
					else
					{
						ptr_func->in_tile_width_max_end = ptr_func->in_tile_width;
					}
				}
				else if (ptr_func->out_tile_width_max_end)
				{
					ptr_func->in_tile_width_max_end = ptr_func->out_tile_width_max_end;
				}
				else
				{
					ptr_func->in_tile_width_max_end = ptr_func->in_tile_width;
				}
				/* smart tile + ufd */
				ptr_func->in_tile_width_loss = ptr_func->out_tile_width_loss;
				ptr_func->max_in_crop_xe = ptr_func->max_out_crop_xe;
				ptr_func->min_tile_crop_in_pos_xe = ptr_func->min_tile_crop_out_pos_xe;
			}
			/* check align mis-match */
			if ((0x0 == (TILE_TYPE_CROP_EN & ptr_func->type)) || (ptr_func->type & TILE_TYPE_LOSS) || (false == ptr_func->enable_flag))
			{
				/* right edge */
				if (ptr_func->in_pos_xe + 1 >= ptr_func->full_size_x_in)
				{
					if (ptr_func->in_tile_width_max_end)
					{
						if (ptr_func->in_tile_width_max_end + ptr_func->in_pos_xs < ptr_func->full_size_x_in)
						{
							ptr_func->in_pos_xe = ptr_func->full_size_x_in - 1 - ptr_func->in_const_x;
						}
					}
				}
				/* intermeidate */
				if (ptr_func->in_pos_xe + 1 < ptr_func->full_size_x_in)
				{
					if (ptr_func->in_tile_width_max)
					{
						if (ptr_func->in_tile_width_max + ptr_func->in_pos_xs < ptr_func->in_pos_xe + 1)
						{
							int in_const_x = ptr_func->in_const_x;
							ptr_func->in_pos_xe = ptr_func->in_pos_xs + ptr_func->in_tile_width_max - 1;
							if (in_const_x > 1)
							{
								/* shift in_pos_xe */
								int val_e = TILE_MOD(ptr_func->in_pos_xe + 1, in_const_x);
								if (0 != val_e)
								{
									ptr_func->in_pos_xe -= val_e;
								}
							}
						}
					}
				}
				/* left edge */
				if (ptr_func->in_pos_xs <= 0)
				{
					if (ptr_func->in_tile_width_max_str)
					{
						if (ptr_func->in_tile_width_max_str < ptr_func->in_pos_xe + 1)
						{
							int in_const_x = ptr_func->in_const_x;
							ptr_func->in_pos_xe = ptr_func->in_tile_width_max_str - 1;
							if (in_const_x > 1)
							{
								/* shift in_pos_xe */
								int val_e = TILE_MOD(ptr_func->in_pos_xe + 1, in_const_x);
								if (0 != val_e)
								{
									ptr_func->in_pos_xe -= val_e;
								}
							}
						}
					}
				}
			}
			/* clip input size */
			if (ptr_func->in_pos_xe + 1 > ptr_func->full_size_x_in)
			{
				ptr_func->in_pos_xe = ptr_func->full_size_x_in - 1;
			}
			if (ptr_func->in_pos_xs <= 0)
			{
				ptr_func->in_pos_xs = 0;
			}
			/* output don't touch left edge */
			if (TILE_EDGE_LEFT_MASK != (ptr_func->tdr_edge & TILE_EDGE_LEFT_MASK))
			{
				/* input touch left edge */
				if (ptr_func->in_pos_xs <= 0)
				{
					ptr_func->tdr_edge |= TILE_EDGE_LEFT_MASK;
				}
			}
			/* output touch left edge */
			else
			{
				/* input don't touch left edge */
				if (ptr_func->in_pos_xs > 0)
				{
					/* crop module */
					if (TILE_TYPE_CROP_EN & ptr_func->type)
					{
						ptr_func->tdr_edge &= ~TILE_EDGE_LEFT_MASK;
					}
				}
			}
			/* config max_h_edge_flag */
			if (ptr_func->in_tile_width)
			{
				/* bound in_pos_xe with in_tile_width and make it align */
				if (ptr_func->in_pos_xe + 1 > ptr_func->in_pos_xs + ptr_func->in_tile_width)
				{
					int in_const_x = ptr_func->in_const_x;
					ptr_func->in_pos_xe = ptr_func->in_pos_xs + ptr_func->in_tile_width - 1;
					if (in_const_x > 1)
					{
						int val_e = TILE_MOD(ptr_func->in_pos_xe + 1, in_const_x);
						if (0 != val_e)
						{
							ptr_func->in_pos_xe -= val_e;
						}
					}
				}
				/* module with crop */
				if ((TILE_TYPE_CROP_EN & ptr_func->type) && ptr_func->enable_flag)
				{
					if (0x0 == (ptr_func->type & TILE_TYPE_LOSS))
					{
						ptr_func->crop_h_end_flag &= ptr_func->h_end_flag;
					}
					if (ptr_func->in_pos_xs + ptr_func->in_tile_width < ptr_func->full_size_x_in)
					{
						ptr_func->max_h_edge_flag = false;
					}
					else
					{
						if (0x0 == (ptr_func->type & TILE_TYPE_LOSS))
						{
							ptr_func->max_h_edge_flag = true;
						}
					}
				}
				/* module without crop */
				else
				{
					if (ptr_func->max_h_edge_flag)
					{
						if (ptr_func->in_pos_xs + ptr_func->in_tile_width < ptr_func->full_size_x_in)
						{
							ptr_func->max_h_edge_flag = false;
						}
					}
				}
			}
			else
			{
				/* module with crop */
				if ((TILE_TYPE_CROP_EN & ptr_func->type) && ptr_func->enable_flag)
				{
					if (0x0 == (ptr_func->type & TILE_TYPE_LOSS))
					{
						ptr_func->crop_h_end_flag &= ptr_func->h_end_flag;
						ptr_func->max_h_edge_flag = true;
					}
				}
			}
			/* update tile edge flag due to dz crop & tile size */
			if (ptr_func->in_pos_xe + 1 < ptr_func->full_size_x_in)
			{
				ptr_func->tdr_edge &= ~TILE_EDGE_RIGHT_MASK;
			}
			/* output don't touch right edge */
			if (TILE_EDGE_RIGHT_MASK != (ptr_func->tdr_edge & TILE_EDGE_RIGHT_MASK))
			{
				/* input touch right edge */
				if (ptr_func->in_pos_xe + 1 >= ptr_func->full_size_x_in)
				{
					/* crop module */
					if ((TILE_TYPE_CROP_EN & ptr_func->type) && (0x0 == (ptr_func->type & TILE_TYPE_LOSS)) &&
						ptr_func->enable_flag)
					{
						ptr_func->tdr_edge |= TILE_EDGE_RIGHT_MASK;
					}
					/* non-crop module */
					else
					{
						 /* reduce input size if not able to touch edge */
						if (ptr_func->max_h_edge_flag)
						{
							ptr_func->tdr_edge |= TILE_EDGE_RIGHT_MASK;
						}
						else
						{
							/* reduce input size if not able to touch edge */
							ptr_func->in_pos_xe -= ptr_func->in_const_x;
						}
					}
				}
			}
			/* sync end flag with input end position */
			if (ptr_func->h_end_flag)
			{
				if (ptr_func->in_pos_xe < ptr_func->max_in_pos_xe)
				{
					if (ptr_func->valid_h_no)
					{
						/* diff view to check max last xe */
						if (ptr_func->max_last_input_xe_pos < ptr_func->max_in_pos_xe)
						{
							ptr_func->h_end_flag = false;
						}
					}
					else
					{
						ptr_func->h_end_flag = false;
					}
				}
			}
			else
			{
				/* hit right edge with h_end_flag = false */
				if (ptr_func->in_pos_xe + 1 >= ptr_func->full_size_x_in)
				{
					if (ptr_func->crop_h_end_flag && ptr_func->max_h_edge_flag)
					{
						ptr_func->h_end_flag = true;
					}
				}
				else
				{
					if (ptr_func->crop_h_end_flag)
					{
						if (ptr_func->valid_h_no)
						{
							/* diff view to check max last xe */
							if (ptr_func->max_last_input_xe_pos >= ptr_func->max_in_pos_xe)
							{
								ptr_func->h_end_flag = true;
							}
						}
						else
						{
							if (ptr_func->in_pos_xe >= ptr_func->max_in_pos_xe)
							{
								ptr_func->h_end_flag = true;
							}
						}
					}
				}
			}
			/* backup backward */
			ptr_func->backward_input_xs_pos = ptr_func->in_pos_xs;
			ptr_func->backward_input_xe_pos = ptr_func->in_pos_xe;
			ptr_func->backward_output_xs_pos = ptr_func->out_pos_xs;
			ptr_func->backward_output_xe_pos = ptr_func->out_pos_xe;
		}
		/* backup flag */
		ptr_func->backward_tdr_h_disable_flag = ptr_func->tdr_h_disable_flag;
		ptr_func->backward_h_end_flag = ptr_func->h_end_flag;
	}
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_backward_by_func_post_x_inv(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
	if (false == ptr_tile_reg_map->skip_x_cal)
	{
		if (false == ptr_func->tdr_h_disable_flag)
		{
			if (ptr_func->enable_flag)
			{
				/* check min x size */
				if (ptr_func->in_pos_xs + ptr_func->in_min_width > ptr_func->in_pos_xe + 1)
				{
					int min_xsize = ptr_func->in_min_width;
					int min_in_pos_xs = ptr_tile_reg_map->first_frame?0:ptr_func->min_in_pos_xs;
					ptr_func->in_pos_xs = ptr_func->in_pos_xe + 1 -  min_xsize;
					if (ptr_func->in_pos_xs <= min_in_pos_xs)
					{
						if (0 == min_in_pos_xs)
						{
							if (ptr_func->max_h_edge_flag)
							{
								ptr_func->in_pos_xs = 0;
								ptr_func->in_pos_xe = min_xsize - 1;
							}
							else
							{
								ptr_func->in_pos_xs = ptr_func->in_const_x;
								ptr_func->in_pos_xe = min_xsize - 1 + ptr_func->in_const_x;
							}
						}
						else
						{
							ptr_func->in_pos_xs = min_in_pos_xs;
							ptr_func->in_pos_xe = min_in_pos_xs + min_xsize - 1;
						}
						/* align xe position */
						if (ptr_func->in_const_x > 1)
						{
							int val_e = TILE_MOD(ptr_func->in_pos_xe + 1, ptr_func->in_const_x);
							if (0 != val_e)
							{
								ptr_func->in_pos_xe += ptr_func->in_const_x - val_e;
							}
						}
					}
					/* align xs position */
					if (ptr_func->in_const_x > 1)
					{
						int val_s = TILE_MOD(ptr_func->in_pos_xs, ptr_func->in_const_x);
						if (0 != val_s)
						{
							ptr_func->in_pos_xs -= val_s;
						}
					}
				}
				/* check crop & update in_tile_width_max */
				if ((0x0 == (TILE_TYPE_CROP_EN & ptr_func->type)) || (ptr_func->type & TILE_TYPE_LOSS))
				{
					/* in_tile_width_max */
					if (ptr_func->in_tile_width && ptr_func->out_tile_width_max)
					{
						if (ptr_func->in_tile_width > ptr_func->out_tile_width_max + ptr_func->l_tile_loss + ptr_func->r_tile_loss)
						{
							ptr_func->in_tile_width_max = ptr_func->out_tile_width_max + ptr_func->l_tile_loss + ptr_func->r_tile_loss;
						}
						else
						{
							ptr_func->in_tile_width_max = ptr_func->in_tile_width;
						}
					}
					else if (ptr_func->out_tile_width_max)
					{
						ptr_func->in_tile_width_max = ptr_func->out_tile_width_max + ptr_func->l_tile_loss + ptr_func->r_tile_loss;
					}
					else
					{
						ptr_func->in_tile_width_max = ptr_func->in_tile_width;
					}
					/* in_tile_width_max_str */
					if (ptr_func->in_tile_width && ptr_func->out_tile_width_max_str)
					{
						if (ptr_func->in_tile_width > ptr_func->out_tile_width_max_str + ptr_func->l_tile_loss)
						{
							ptr_func->in_tile_width_max_str = ptr_func->out_tile_width_max_str + ptr_func->l_tile_loss;
						}
						else
						{
							ptr_func->in_tile_width_max_str = ptr_func->in_tile_width;
						}
					}
					else if (ptr_func->out_tile_width_max_str)
					{
						ptr_func->in_tile_width_max_str = ptr_func->out_tile_width_max_str + ptr_func->l_tile_loss;
					}
					else
					{
						ptr_func->in_tile_width_max_str = ptr_func->in_tile_width;
					}
					/* prevent from min edge error */
					if (ptr_func->out_tile_width_max_str && (ptr_func->out_tile_width_max_str < ptr_func->full_size_x_out) && (ptr_func->in_tile_width_max_str >= ptr_func->full_size_x_in))
					{
						ptr_func->in_tile_width_max_str = ptr_func->full_size_x_in - ptr_func->in_const_x;
					}
					/* in_tile_width_max_end */
					if (ptr_func->in_tile_width && ptr_func->out_tile_width_max_end)
					{
						if (ptr_func->in_tile_width > ptr_func->out_tile_width_max_end + ptr_func->r_tile_loss)
						{
							ptr_func->in_tile_width_max_end = ptr_func->out_tile_width_max_end + ptr_func->r_tile_loss;
						}
						else
						{
							ptr_func->in_tile_width_max_end = ptr_func->in_tile_width;
						}
					}
					else if (ptr_func->out_tile_width_max_end)
					{
						ptr_func->in_tile_width_max_end = ptr_func->out_tile_width_max_end + ptr_func->r_tile_loss;
					}
					else
					{
						ptr_func->in_tile_width_max_end = ptr_func->in_tile_width;
					}
					/* smart tile + ufd */
					ptr_func->in_tile_width_loss = ptr_func->out_tile_width_loss + ptr_func->l_tile_loss + ptr_func->r_tile_loss;
					if (ptr_func->min_in_pos_xs + ptr_func->l_tile_loss < ptr_func->min_out_crop_xs)
					{
						ptr_func->min_in_crop_xs = ptr_func->min_out_crop_xs - ptr_func->l_tile_loss;
					}
					else
					{
						ptr_func->min_in_crop_xs = ptr_func->min_in_pos_xs;
					}
					if (ptr_func->min_in_pos_xs + ptr_func->l_tile_loss < ptr_func->min_tile_crop_out_pos_xs)
					{
						ptr_func->min_tile_crop_in_pos_xs = ptr_func->min_tile_crop_out_pos_xs - ptr_func->l_tile_loss;
					}
					else
					{
						ptr_func->min_tile_crop_in_pos_xs = ptr_func->min_in_pos_xs;
					}
				}
				else
				{
					ptr_func->in_tile_width_max = ptr_func->in_tile_width;
					ptr_func->in_tile_width_max_str = ptr_func->in_tile_width;
					ptr_func->in_tile_width_max_end = ptr_func->in_tile_width;
					/* smart tile + ufd */
					ptr_func->in_tile_width_loss = 0;
					ptr_func->min_in_crop_xs = ptr_func->min_in_pos_xs;
					ptr_func->min_tile_crop_in_pos_xs = ptr_func->min_tile_in_pos_xs;
				}
			}
			else
			{
				/* in_tile_width_max */
				if (ptr_func->in_tile_width && ptr_func->out_tile_width_max)
				{
					if (ptr_func->in_tile_width > ptr_func->out_tile_width_max)
					{
						ptr_func->in_tile_width_max = ptr_func->out_tile_width_max;
					}
					else
					{
						ptr_func->in_tile_width_max = ptr_func->in_tile_width;
					}
				}
				else if (ptr_func->out_tile_width_max)
				{
					ptr_func->in_tile_width_max = ptr_func->out_tile_width_max;
				}
				else
				{
					ptr_func->in_tile_width_max = ptr_func->in_tile_width;
				}
				/* in_tile_width_max_str */
				if (ptr_func->in_tile_width && ptr_func->out_tile_width_max_str)
				{
					if (ptr_func->in_tile_width > ptr_func->out_tile_width_max_str)
					{
						ptr_func->in_tile_width_max_str = ptr_func->out_tile_width_max_str;
					}
					else
					{
						ptr_func->in_tile_width_max_str = ptr_func->in_tile_width;
					}
				}
				else if (ptr_func->out_tile_width_max_str)
				{
					ptr_func->in_tile_width_max_str = ptr_func->out_tile_width_max_str;
				}
				else
				{
					ptr_func->in_tile_width_max_str = ptr_func->in_tile_width;
				}
				/* in_tile_width_max_end */
				if (ptr_func->in_tile_width && ptr_func->out_tile_width_max_end)
				{
					if (ptr_func->in_tile_width > ptr_func->out_tile_width_max_end)
					{
						ptr_func->in_tile_width_max_end = ptr_func->out_tile_width_max_end;
					}
					else
					{
						ptr_func->in_tile_width_max_end = ptr_func->in_tile_width;
					}
				}
				else if (ptr_func->out_tile_width_max_end)
				{
					ptr_func->in_tile_width_max_end = ptr_func->out_tile_width_max_end;
				}
				else
				{
					ptr_func->in_tile_width_max_end = ptr_func->in_tile_width;
				}
				/* smart tile + ufd */
				ptr_func->in_tile_width_loss = ptr_func->out_tile_width_loss;
				ptr_func->min_in_crop_xs = ptr_func->min_out_crop_xs;
				ptr_func->min_tile_crop_in_pos_xs = ptr_func->min_tile_crop_out_pos_xs;
			}
			/* check align mis-match */
			if ((0x0 == (TILE_TYPE_CROP_EN & ptr_func->type)) || (ptr_func->type & TILE_TYPE_LOSS) || (false == ptr_func->enable_flag))
			{
				/* left edge */
				if (ptr_func->in_pos_xs <= 0)
				{
					if (ptr_func->in_tile_width_max_end)
					{
						if (ptr_func->in_tile_width_max_end < ptr_func->in_pos_xe + 1)
						{
							ptr_func->in_pos_xs = ptr_func->in_const_x;
						}
					}
				}
				/* intermeidate */
				if (ptr_func->in_pos_xs > 0)
				{
					if (ptr_func->in_tile_width_max)
					{
						if (ptr_func->in_tile_width_max + ptr_func->in_pos_xs < ptr_func->in_pos_xe + 1)
						{
							int in_const_x = ptr_func->in_const_x;
							ptr_func->in_pos_xs = ptr_func->in_pos_xe - ptr_func->in_tile_width_max + 1;
							if (in_const_x > 1)
							{
								/* shift in_pos_xs */
								int val_s = TILE_MOD(ptr_func->in_pos_xs, in_const_x);
								if (0 != val_s)
								{
									ptr_func->in_pos_xs += in_const_x - val_s;
								}
							}
						}
					}
				}
				/* right edge */
				if (ptr_func->in_pos_xe + 1 >= ptr_func->full_size_x_in)
				{
					if (ptr_func->in_tile_width_max_str)
					{
						if (ptr_func->in_tile_width_max_str + ptr_func->in_pos_xs <  ptr_func->full_size_x_in)
						{
							int in_const_x = ptr_func->in_const_x;
							ptr_func->in_pos_xs = ptr_func->full_size_x_in - ptr_func->in_tile_width_max_str;
							if (in_const_x > 1)
							{
								/* shift in_pos_xs */
								int val_s = TILE_MOD(ptr_func->in_pos_xs, in_const_x);
								if (0 != val_s)
								{
									ptr_func->in_pos_xs += in_const_x - val_s;
								}
							}
						}
					}
				}
			}
			/* clip input size */
			if (ptr_func->in_pos_xs < 0)
			{
				ptr_func->in_pos_xs = 0;
			}
			if (ptr_func->in_pos_xe + 1 >= ptr_func->full_size_x_in)
			{
				ptr_func->in_pos_xe = ptr_func->full_size_x_in - 1;
			}
			/* config max_h_edge_flag */
			if (ptr_func->in_tile_width)
			{
				/* bound in_pos_xe with in_tile_width and make it align */
				if (ptr_func->in_pos_xe + 1 > ptr_func->in_pos_xs + ptr_func->in_tile_width)
				{
					int in_const_x = ptr_func->in_const_x;
					ptr_func->in_pos_xs = ptr_func->in_pos_xe - ptr_func->in_tile_width + 1;
					if (in_const_x > 1)
					{
						int val_s = TILE_MOD(ptr_func->in_pos_xs, in_const_x);
						if (0 != val_s)
						{
							ptr_func->in_pos_xs += in_const_x - val_s;
						}
					}
				}
				/* module with crop */
				if ((TILE_TYPE_CROP_EN & ptr_func->type) && ptr_func->enable_flag)
				{
					if (0x0 == (ptr_func->type & TILE_TYPE_LOSS))
					{
						ptr_func->crop_h_end_flag &= ptr_func->h_end_flag;
					}
					/* note boundary, xs = 0 */
					if (ptr_func->in_pos_xe >= ptr_func->in_tile_width)
					{
						ptr_func->max_h_edge_flag = false;
					}
					else
					{
						if (0x0 == (ptr_func->type & TILE_TYPE_LOSS))
						{
							ptr_func->max_h_edge_flag = true;
						}
					}
				}
				/* module without crop */
				else
				{
					if (ptr_func->max_h_edge_flag)
					{
						/* note boundary, xs = 0 */
						if (ptr_func->in_pos_xe >= ptr_func->in_tile_width)
						{
							ptr_func->max_h_edge_flag = false;
						}
					}
				}
			}
			else
			{
				/* crop module */
				if ((TILE_TYPE_CROP_EN & ptr_func->type) && ptr_func->enable_flag)
				{
					if (0x0 == (ptr_func->type & TILE_TYPE_LOSS))
					{
						ptr_func->crop_h_end_flag &= ptr_func->h_end_flag;
						ptr_func->max_h_edge_flag = true;
					}
				}
			}
			/* update tile edge flag due to dz crop & tile size */
			if (ptr_func->in_pos_xs > 0)
			{
				ptr_func->tdr_edge &= ~TILE_EDGE_LEFT_MASK;
			}
			/* output don't touch left edge */
			if (TILE_EDGE_LEFT_MASK != (ptr_func->tdr_edge & TILE_EDGE_LEFT_MASK))
			{
				/* input touch left edge */
				if (ptr_func->in_pos_xs <= 0)
				{
					/* crop module */
					if ((TILE_TYPE_CROP_EN & ptr_func->type) && (0x0 == (ptr_func->type & TILE_TYPE_LOSS)) &&
						ptr_func->enable_flag)
					{
						ptr_func->tdr_edge |= TILE_EDGE_LEFT_MASK;
					}
					/* non-crop module */
					else
					{
						/* reduce input size if not able to touch edge */
						if (ptr_func->max_h_edge_flag)
						{
							ptr_func->tdr_edge |= TILE_EDGE_LEFT_MASK;
						}
						else
						{
							ptr_func->in_pos_xs = ptr_func->in_const_x;
						}
					}
				}
			}
			/* output don't touch right edge */
			if (TILE_EDGE_RIGHT_MASK != (ptr_func->tdr_edge & TILE_EDGE_RIGHT_MASK))
			{
				/* input touch right edge */
				if (ptr_func->in_pos_xe + 1 >= ptr_func->full_size_x_in)
				{
					ptr_func->tdr_edge |= TILE_EDGE_RIGHT_MASK;
				}
			}
			/* output touch right edge */
			else
			{
				/* input don't touch right edge */
				if (ptr_func->in_pos_xe + 1 < ptr_func->full_size_x_in)
				{
					/* crop module */
					if (TILE_TYPE_CROP_EN & ptr_func->type)
					{
						ptr_func->tdr_edge &= ~TILE_EDGE_RIGHT_MASK;
					}
				}
			}
			/* sync end flag with input start position */
			if (ptr_func->h_end_flag)
			{
				if (ptr_func->in_pos_xs > ptr_func->min_in_pos_xs)
				{
					/* diff view to check min last xs */
					if (ptr_func->valid_h_no)
					{
						if (ptr_func->min_last_input_xs_pos > ptr_func->min_in_pos_xs)
						{
							ptr_func->h_end_flag = false;
						}
					}
					else
					{
						ptr_func->h_end_flag = false;
					}
				}
			}
			else
			{
				/* hit left edge with h_end_flag = false */
				if (ptr_func->in_pos_xs <= 0)
				{
					if (ptr_func->crop_h_end_flag && ptr_func->max_h_edge_flag)
					{
						ptr_func->h_end_flag = true;
					}
				}
				else
				{
					if (ptr_func->crop_h_end_flag)
					{
						if (ptr_func->valid_h_no)
						{
							/* diff view to check max last xe */
							if (ptr_func->min_last_input_xs_pos <= ptr_func->min_in_pos_xs)
							{
								ptr_func->h_end_flag = true;
							}
						}
						else
						{
							if (ptr_func->in_pos_xs <= ptr_func->min_in_pos_xs)
							{
								ptr_func->h_end_flag = true;
							}
						}
					}
				}
			}
			/* backup backward */
			ptr_func->backward_input_xs_pos = ptr_func->in_pos_xs;
			ptr_func->backward_input_xe_pos = ptr_func->in_pos_xe;
			ptr_func->backward_output_xs_pos = ptr_func->out_pos_xs;
			ptr_func->backward_output_xe_pos = ptr_func->out_pos_xe;
		}
		/* backup flag */
		ptr_func->backward_tdr_h_disable_flag = ptr_func->tdr_h_disable_flag;
		ptr_func->backward_h_end_flag = ptr_func->h_end_flag;
    }
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_backward_by_func_post_y(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
	if (false == ptr_tile_reg_map->skip_y_cal)
	{
		if (false == ptr_func->tdr_v_disable_flag)
		{
			if (ptr_func->enable_flag)
			{
				/* check min x size */
				if (ptr_func->in_pos_ys + ptr_func->in_min_height > ptr_func->in_pos_ye + 1)
				{
					int min_ysize = ptr_func->in_min_height;
					int max_in_pos_ye = ptr_tile_reg_map->first_frame?(ptr_func->full_size_y_in - 1):ptr_func->max_in_pos_ye;
					ptr_func->in_pos_ye = ptr_func->in_pos_ys + min_ysize - 1;
					if (ptr_func->in_pos_ye >= max_in_pos_ye)
					{
						if (max_in_pos_ye + 1 >= ptr_func->full_size_y_in)
						{
							if (ptr_func->max_v_edge_flag)
							{
								ptr_func->in_pos_ye = max_in_pos_ye;
								ptr_func->in_pos_ys = max_in_pos_ye + 1 - min_ysize;
							}
							else
							{
								ptr_func->in_pos_ye = max_in_pos_ye - ptr_func->in_const_y;
								ptr_func->in_pos_ys = max_in_pos_ye + 1 - min_ysize - ptr_func->in_const_y;
							}
						}
						else
						{
							ptr_func->in_pos_ye = max_in_pos_ye;
							ptr_func->in_pos_ys = max_in_pos_ye + 1 - min_ysize;
						}
						/* align ys position */
						if (ptr_func->in_const_y > 1)
						{
							int val_s = TILE_MOD(ptr_func->in_pos_ys, ptr_func->in_const_y);
							if (0 != val_s)
							{
								ptr_func->in_pos_ys -= val_s;
							}
						}
					}
					/* align input position */
					if (ptr_func->in_const_y > 1)
					{
						int val_e = TILE_MOD(ptr_func->in_pos_ye + 1, ptr_func->in_const_y);
						if (0 != val_e)
						{
							ptr_func->in_pos_ye += ptr_func->in_const_y - val_e;
						}
					}
				}
				/* check crop & update in_tile_height_max */
				if ((0x0 == (TILE_TYPE_CROP_EN & ptr_func->type)) || (ptr_func->type & TILE_TYPE_LOSS))
				{
					/* in_tile_height_max */
					if (ptr_func->in_tile_height && ptr_func->out_tile_height_max)
					{
						if (ptr_func->in_tile_height > ptr_func->out_tile_height_max + ptr_func->t_tile_loss + ptr_func->b_tile_loss)
						{
							ptr_func->in_tile_height_max = ptr_func->out_tile_height_max + ptr_func->t_tile_loss + ptr_func->b_tile_loss;
						}
						else
						{
							ptr_func->in_tile_height_max = ptr_func->in_tile_height;
						}
					}
					else if (ptr_func->out_tile_height_max)
					{
						ptr_func->in_tile_height_max = ptr_func->out_tile_height_max + ptr_func->t_tile_loss + ptr_func->b_tile_loss;
					}
					else
					{
						ptr_func->in_tile_height_max = ptr_func->in_tile_height;
					}
					/* in_tile_height_max_str */
					if (ptr_func->in_tile_height && ptr_func->out_tile_height_max_str)
					{
						if (ptr_func->in_tile_height > ptr_func->out_tile_height_max_str + ptr_func->b_tile_loss)
						{
							ptr_func->in_tile_height_max_str = ptr_func->out_tile_height_max_str + ptr_func->b_tile_loss;
						}
						else
						{
							ptr_func->in_tile_height_max_str = ptr_func->in_tile_height;
						}
					}
					else if (ptr_func->out_tile_height_max_str)
					{
						ptr_func->in_tile_height_max_str = ptr_func->out_tile_height_max_str + ptr_func->b_tile_loss;
					}
					else
					{
						ptr_func->in_tile_height_max_str = ptr_func->in_tile_height;
					}
					/* prevent from min edge error */
					if (ptr_func->out_tile_height_max_str && (ptr_func->out_tile_height_max_str < ptr_func->full_size_y_out) && (ptr_func->in_tile_height_max_str >= ptr_func->full_size_y_in))
					{
						ptr_func->in_tile_height_max_str = ptr_func->full_size_y_in - ptr_func->in_const_y;
					}
					/* in_tile_height_max_end */
					if (ptr_func->in_tile_height && ptr_func->out_tile_height_max_end)
					{
						if (ptr_func->in_tile_height > ptr_func->out_tile_height_max_end + ptr_func->t_tile_loss)
						{
							ptr_func->in_tile_height_max_end = ptr_func->out_tile_height_max_end + ptr_func->t_tile_loss;
						}
						else
						{
							ptr_func->in_tile_height_max_end = ptr_func->in_tile_height;
						}
					}
					else if (ptr_func->out_tile_height_max_end)
					{
						ptr_func->in_tile_height_max_end = ptr_func->out_tile_height_max_end + ptr_func->t_tile_loss;
					}
					else
					{
						ptr_func->in_tile_height_max_end = ptr_func->in_tile_height;
					}
				}
				else
				{
					ptr_func->in_tile_height_max = ptr_func->in_tile_height;
					ptr_func->in_tile_height_max_str = ptr_func->in_tile_height;
					ptr_func->in_tile_height_max_end = ptr_func->in_tile_height;
				}
			}
			else
			{
				/* in_tile_height_max */
				if (ptr_func->in_tile_height && ptr_func->out_tile_height_max)
				{
					if (ptr_func->in_tile_height > ptr_func->out_tile_height_max)
					{
						ptr_func->in_tile_height_max = ptr_func->out_tile_height_max;
					}
					else
					{
						ptr_func->in_tile_height_max = ptr_func->in_tile_height;
					}
				}
				else if (ptr_func->out_tile_height_max)
				{
					ptr_func->in_tile_height_max = ptr_func->out_tile_height_max;
				}
				else
				{
					ptr_func->in_tile_height_max = ptr_func->in_tile_height;
				}
				/* in_tile_height_max_str */
				if (ptr_func->in_tile_height && ptr_func->out_tile_height_max_str)
				{
					if (ptr_func->in_tile_height > ptr_func->out_tile_height_max_str)
					{
						ptr_func->in_tile_height_max_str = ptr_func->out_tile_height_max_str;
					}
					else
					{
						ptr_func->in_tile_height_max_str = ptr_func->in_tile_height;
					}
				}
				else if (ptr_func->out_tile_height_max_str)
				{
					ptr_func->in_tile_height_max_str = ptr_func->out_tile_height_max_str;
				}
				else
				{
					ptr_func->in_tile_height_max_str = ptr_func->in_tile_height;
				}
				/* in_tile_height_max_end */
				if (ptr_func->in_tile_height && ptr_func->out_tile_height_max_end)
				{
					if (ptr_func->in_tile_height > ptr_func->out_tile_height_max_end)
					{
						ptr_func->in_tile_height_max_end = ptr_func->out_tile_height_max_end;
					}
					else
					{
						ptr_func->in_tile_height_max_end = ptr_func->in_tile_height;
					}
				}
				else if (ptr_func->out_tile_height_max_end)
				{
					ptr_func->in_tile_height_max_end = ptr_func->out_tile_height_max_end;
				}
				else
				{
					ptr_func->in_tile_height_max_end = ptr_func->in_tile_height;
				}
			}
			/* check align mis-match */
			if ((0x0 == (TILE_TYPE_CROP_EN & ptr_func->type)) || (ptr_func->type & TILE_TYPE_LOSS) || (false == ptr_func->enable_flag))
			{
				/* bottom edge */
				if (ptr_func->in_pos_ye + 1 >= ptr_func->full_size_y_in)
				{
					if (ptr_func->in_tile_height_max_end)
					{
						if (ptr_func->in_tile_height_max_end + ptr_func->in_pos_ys < ptr_func->full_size_y_in)
						{
							ptr_func->in_pos_ye = ptr_func->full_size_y_in - 1 - ptr_func->in_const_y;
						}
					}
				}
				/* intermeidate */
				if (ptr_func->in_pos_ye + 1 < ptr_func->full_size_y_in)
				{
					if (ptr_func->in_tile_height_max)
					{
						if (ptr_func->in_tile_height_max + ptr_func->in_pos_ys < ptr_func->in_pos_ye + 1)
						{
							int in_const_y = ptr_func->in_const_y;
							ptr_func->in_pos_ye = ptr_func->in_pos_ys + ptr_func->in_tile_height_max - 1;
							if (in_const_y > 1)
							{
								/* shift in_pos_ye */
								int val_e = TILE_MOD(ptr_func->in_pos_ye + 1, in_const_y);
								if (0 != val_e)
								{
									ptr_func->in_pos_ye -= val_e;
								}
							}
						}
					}
				}
				/* bottom edge */
				if (ptr_func->in_pos_ys <= 0)
				{
					if (ptr_func->in_tile_height_max_str)
					{
						if (ptr_func->in_tile_height_max_str < ptr_func->in_pos_ye + 1)
						{
							int in_const_y = ptr_func->in_const_y;
							ptr_func->in_pos_ye = ptr_func->in_tile_height_max_str - 1;
							if (in_const_y > 1)
							{
								/* shift in_pos_ye */
								int val_e = TILE_MOD(ptr_func->in_pos_ye + 1, in_const_y);
								if (0 != val_e)
								{
									ptr_func->in_pos_ye -= val_e;
								}
							}
						}
					}
				}
			}
			/* clip input size */
			if (ptr_func->in_pos_ye + 1 >= ptr_func->full_size_y_in)
			{
				ptr_func->in_pos_ye = ptr_func->full_size_y_in - 1;
			}
			if (ptr_func->in_pos_ys <= 0)
			{
				ptr_func->in_pos_ys = 0;
			}
			/* config max_v_edge_flag */
			if (ptr_func->in_tile_height)
			{
				/* check input end position with input tile size and make it align */
				if (ptr_func->in_pos_ye + 1 > ptr_func->in_pos_ys + ptr_func->in_tile_height)
				{
					int in_const_y = ptr_func->in_const_y;
					ptr_func->in_pos_ye = ptr_func->in_pos_ys + ptr_func->in_tile_height - 1;
					if (in_const_y > 1)
					{
						int val_e = TILE_MOD(ptr_func->in_pos_ye + 1, in_const_y);
						if (0 != val_e)
						{
							ptr_func->in_pos_ye -= val_e;
						}
					}
				}
				/* crop module */
				if ((TILE_TYPE_CROP_EN & ptr_func->type) && ptr_func->enable_flag)
				{
					if (0x0 == (ptr_func->type & TILE_TYPE_LOSS))
					{
						ptr_func->crop_v_end_flag &= ptr_func->v_end_flag;
					}
					if (ptr_func->in_pos_ys + ptr_func->in_tile_height < ptr_func->full_size_y_in)
					{
						ptr_func->max_v_edge_flag = false;
					}
					else
					{
						if (0x0 == (ptr_func->type & TILE_TYPE_LOSS))
						{
							ptr_func->max_v_edge_flag = true;
						}
					}
				}
				/* non-crop module */
				else
				{
					if (ptr_func->max_v_edge_flag)
					{
						if (ptr_func->in_pos_ys + ptr_func->in_tile_height < ptr_func->full_size_y_in)
						{
							ptr_func->max_v_edge_flag = false;
						}
					}
				}
			}
			else
			{
				/* crop module */
				if ((TILE_TYPE_CROP_EN & ptr_func->type) && ptr_func->enable_flag)
				{
					if (0x0 == (ptr_func->type & TILE_TYPE_LOSS))
					{
						ptr_func->crop_v_end_flag &= ptr_func->v_end_flag;
						ptr_func->max_v_edge_flag = true;
					}
				}
			}
			/* update tile edge flag due to dz crop & tile size */
			if (ptr_func->in_pos_ye + 1 < ptr_func->full_size_y_in)
			{
				ptr_func->tdr_edge &= ~TILE_EDGE_BOTTOM_MASK;
			}
			/* output don't touch bottom edge */
			if (TILE_EDGE_BOTTOM_MASK != (ptr_func->tdr_edge & TILE_EDGE_BOTTOM_MASK))
			{
				/* input touch bottom edge */
				if (ptr_func->in_pos_ye + 1 >= ptr_func->full_size_y_in)
				{
					/* crop module */
					if ((TILE_TYPE_CROP_EN & ptr_func->type) && (0x0 == (ptr_func->type & TILE_TYPE_LOSS)) &&
						ptr_func->enable_flag)
					{
						ptr_func->tdr_edge |= TILE_EDGE_BOTTOM_MASK;
					}
					/* non-crop module */
					else
					{
						if (ptr_func->max_v_edge_flag)
						{
							ptr_func->tdr_edge |= TILE_EDGE_BOTTOM_MASK;
						}
						else
						{
							/* reduce input size if not able to touch edge */
							ptr_func->in_pos_ye -= ptr_func->in_const_y;
						}
					}
				}
			}
			/* output don't touch top edge */
			if (TILE_EDGE_TOP_MASK != (ptr_func->tdr_edge & TILE_EDGE_TOP_MASK))
			{
				/* input touch top edge */
				if (ptr_func->in_pos_ys <= 0)
				{
					ptr_func->tdr_edge |= TILE_EDGE_TOP_MASK;
				}
			}
			/* output touch top edge */
			else
			{
				/* input don't touch top edge */
				if (ptr_func->in_pos_ys > 0)
				{
					/* crop module */
					if (TILE_TYPE_CROP_EN & ptr_func->type)
					{
						ptr_func->tdr_edge &= ~TILE_EDGE_TOP_MASK;
					}
				}
			}
			/* sync end flag with end position */
			if (ptr_func->v_end_flag)
			{
				if (ptr_func->in_pos_ye < ptr_func->max_in_pos_ye)
				{
					/* diff view to check max last ye */
					if (ptr_func->valid_v_no)
					{
						if (ptr_func->max_last_input_ye_pos < ptr_func->max_in_pos_ye)
						{
							ptr_func->v_end_flag = false;
						}
					}
					else
					{
						ptr_func->v_end_flag = false;
					}
				}
			}
			else
			{
				/* hit bottom edge with v_end_flag = false */
				if (ptr_func->in_pos_ye + 1 >= ptr_func->full_size_y_in)
				{
					if (ptr_func->crop_v_end_flag && ptr_func->max_v_edge_flag)
					{
						ptr_func->v_end_flag = true;
					}
				}
				else
				{
					if (ptr_func->crop_v_end_flag)
					{
						if (ptr_func->valid_v_no)
						{
							/* diff view to check max last xe */
							if (ptr_func->max_last_input_ye_pos >= ptr_func->max_in_pos_ye)
							{
								ptr_func->v_end_flag = true;
							}
						}
						else
						{
							if (ptr_func->in_pos_ye >= ptr_func->max_in_pos_ye)
							{
								ptr_func->v_end_flag = true;
							}
						}
					}
				}
			}
			/* backup backward */
			ptr_func->backward_input_ys_pos = ptr_func->in_pos_ys;
			ptr_func->backward_input_ye_pos = ptr_func->in_pos_ye;
			ptr_func->backward_output_ys_pos = ptr_func->out_pos_ys;
			ptr_func->backward_output_ye_pos = ptr_func->out_pos_ye;
		}
		/* backup flag */
		ptr_func->backward_tdr_v_disable_flag = ptr_func->tdr_v_disable_flag;
		ptr_func->backward_v_end_flag = ptr_func->v_end_flag;
	}
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_backward_by_func(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
	/* no output_disable & run mode check */
	result = tile_backward_by_func_pre_x(ptr_func, ptr_tile_reg_map);
    if (ISP_MESSAGE_TILE_OK == result)
    {
		result = tile_backward_by_func_pre_y(ptr_func, ptr_tile_reg_map);
	}
    if (ISP_MESSAGE_TILE_OK == result)
    {
		/* back comp by module, force tdr enable end function */
		if (ptr_func->enable_flag)
		{
			if ((!ptr_tile_reg_map->skip_x_cal && !ptr_func->tdr_h_disable_flag) ||
				(!ptr_tile_reg_map->skip_y_cal && !ptr_func->tdr_v_disable_flag) ||
				(0 == ptr_func->tot_branch_num))
			{
				/* back func should handle alignment by itself */
				result = tile_back_func_run(ptr_func, ptr_tile_reg_map);
			}
		}
	}
    if (ISP_MESSAGE_TILE_OK == result)
    {
        /* check cal order, input */
		if (ptr_func->in_cal_order & TILE_ORDER_RIGHT_TO_LEFT)
		{
			result = tile_backward_by_func_post_x_inv(ptr_func, ptr_tile_reg_map);
		}
		else
		{
			result = tile_backward_by_func_post_x(ptr_func, ptr_tile_reg_map);
		}
	}
	if (ISP_MESSAGE_TILE_OK == result)
	{
		result = tile_backward_by_func_post_y(ptr_func, ptr_tile_reg_map);
	}
    return result;
}

static int tile_cal_lcm(int a, int b)
{
    int m=a, n=b;
    /* fast return */
    if (1 == a)
    {
        return b;
    }
    if (1 == b)
    {
        return a;
    }
	if (a == b)
	{
        return a;
	}
    while (m != n)
    {
        if (m > n)
        {
            m -= n;
        }
        else
        {
            n -= m;
        }
    }
    if (1 == m)
    {
        return a*b;
    }
    else
    {
        return TILE_INT_DIV(a*b, m);
    }
}

static ISP_TILE_MESSAGE_ENUM tile_forward_by_func_pre_x(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
    if (false == ptr_tile_reg_map->skip_x_cal)
    {
		if (false == ptr_func->tdr_h_disable_flag)
		{
			if (ptr_func->enable_flag)
			{
				/* update input left edge */
				if (ptr_func->in_pos_xs <= 0)
				{
					ptr_func->tdr_edge |= TILE_EDGE_LEFT_MASK;
				}
				/* update input right edge */
				if (ptr_func->in_pos_xe + 1 < ptr_func->full_size_x_in)
				{
					ptr_func->tdr_edge &= ~TILE_EDGE_RIGHT_MASK;
				}
				else
				{
					ptr_func->tdr_edge |= TILE_EDGE_RIGHT_MASK;
				}
				/* sub-in cannot change main path */
				if ((TILE_RUN_MODE_SUB_IN == ptr_func->run_mode) && (TILE_TYPE_CROP_EN & ptr_func->type))
				{
					// skip input position check due to hw size limitation
					if (ptr_func->backward_output_xs_pos > 0)
					{
						ptr_func->out_pos_xs = ptr_func->in_pos_xs + ptr_func->l_tile_loss;
					}
					if (ptr_func->backward_output_xe_pos + 1 < ptr_func->full_size_x_in)
					{
						ptr_func->out_pos_xe = ptr_func->in_pos_xe - ptr_func->r_tile_loss;
					}
				}
				else
				{
					/* set fixed left tile loss */
					if (TILE_EDGE_LEFT_MASK & ptr_func->tdr_edge)
					{
						ptr_func->out_pos_xs = ptr_func->in_pos_xs;
					}
					else
					{
						ptr_func->out_pos_xs = ptr_func->in_pos_xs + ptr_func->l_tile_loss;
					}
					/* set fixed right tile loss */
					if (TILE_EDGE_RIGHT_MASK & ptr_func->tdr_edge)
					{
						ptr_func->out_pos_xe = ptr_func->in_pos_xe;
					}
					else
					{
						ptr_func->out_pos_xe = ptr_func->in_pos_xe - ptr_func->r_tile_loss;
					}
				}
			}
			else
			{
				ptr_func->out_pos_xs = ptr_func->in_pos_xs;
				ptr_func->out_pos_xe = ptr_func->in_pos_xe;
			}
		}
    }
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_forward_by_func_pre_x_inv(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
    if (false == ptr_tile_reg_map->skip_x_cal)
    {
	    if (false == ptr_func->tdr_h_disable_flag)
		{
			if (ptr_func->enable_flag)
			{
				/* update input right edge */
				if (ptr_func->in_pos_xe + 1 >= ptr_func->full_size_x_in)
				{
					ptr_func->tdr_edge |= TILE_EDGE_RIGHT_MASK;
				}
				/* update input left edge */
				if (ptr_func->in_pos_xs > 0)
				{
					ptr_func->tdr_edge &= ~TILE_EDGE_LEFT_MASK;
				}
				else
				{
					ptr_func->tdr_edge |= TILE_EDGE_LEFT_MASK;
				}
				/* sub-in cannot change main path */
				if ((TILE_RUN_MODE_SUB_IN == ptr_func->run_mode) && (TILE_TYPE_CROP_EN & ptr_func->type))
				{
					// skip input position check due to hw size limitation
					if (ptr_func->backward_output_xs_pos > 0)
					{
						ptr_func->out_pos_xs = ptr_func->in_pos_xs + ptr_func->l_tile_loss;
					}
					if (ptr_func->backward_output_xe_pos + 1 < ptr_func->full_size_x_in)
					{
						ptr_func->out_pos_xe = ptr_func->in_pos_xe - ptr_func->r_tile_loss;
					}
				}
				else
				{
					/* set fixed right tile loss */
					if (TILE_EDGE_RIGHT_MASK & ptr_func->tdr_edge)
					{
						ptr_func->out_pos_xe = ptr_func->in_pos_xe;
					}
					else
					{
						ptr_func->out_pos_xe = ptr_func->in_pos_xe - ptr_func->r_tile_loss;
					}
					/* set fixed left tile loss */
					if (TILE_EDGE_LEFT_MASK & ptr_func->tdr_edge)
					{
						ptr_func->out_pos_xs = ptr_func->in_pos_xs;
					}
					else
					{
						ptr_func->out_pos_xs = ptr_func->in_pos_xs + ptr_func->l_tile_loss;
					}
				}
			}
			else
			{
				ptr_func->out_pos_xs = ptr_func->in_pos_xs;
				ptr_func->out_pos_xe = ptr_func->in_pos_xe;
			}
		}
    }
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_forward_by_func_pre_y(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
    if (false == ptr_tile_reg_map->skip_y_cal)
    {
		if (false == ptr_func->tdr_v_disable_flag)
		{
			if (ptr_func->enable_flag)
			{
				/* update input top edge */
				if (ptr_func->in_pos_ys <= 0)
				{
					ptr_func->tdr_edge |= TILE_EDGE_TOP_MASK;
				}
				/* update input bottom edge */
				if (ptr_func->in_pos_ye + 1 < ptr_func->full_size_y_in)
				{
					ptr_func->tdr_edge &= ~TILE_EDGE_BOTTOM_MASK;
				}
				else
				{
					ptr_func->tdr_edge |= TILE_EDGE_BOTTOM_MASK;
				}
				/* sub-in cannot change main path */
				if ((TILE_RUN_MODE_SUB_IN == ptr_func->run_mode) && (TILE_TYPE_CROP_EN & ptr_func->type))
				{
					// skip input position check due to hw size limitation
					if (ptr_func->backward_output_ys_pos > 0)
					{
						ptr_func->out_pos_ys = ptr_func->in_pos_ys + ptr_func->t_tile_loss;
					}
					if (ptr_func->backward_output_ye_pos + 1 < ptr_func->full_size_y_in)
					{
						ptr_func->out_pos_ye = ptr_func->in_pos_ye - ptr_func->b_tile_loss;
					}
				}
				else
				{
					/* set fixed top tile loss */
					if (TILE_EDGE_TOP_MASK & ptr_func->tdr_edge)
					{
						ptr_func->out_pos_ys = ptr_func->in_pos_ys;
					}
					else
					{
						ptr_func->out_pos_ys = ptr_func->in_pos_ys + ptr_func->t_tile_loss;
					}
					/* set fixed bottom tile loss */
					if (TILE_EDGE_BOTTOM_MASK & ptr_func->tdr_edge)
					{
						ptr_func->out_pos_ye = ptr_func->in_pos_ye;
					}
					else
					{
						ptr_func->out_pos_ye = ptr_func->in_pos_ye - ptr_func->b_tile_loss;
					}
				}
			}
			else
			{
				ptr_func->out_pos_ys = ptr_func->in_pos_ys;
				ptr_func->out_pos_ye = ptr_func->in_pos_ye;
			}
		}
	}
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_forward_by_func_post_x(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
    if (false == ptr_tile_reg_map->skip_x_cal)
    {
		if (false == ptr_func->tdr_h_disable_flag)
		{
			/* crop module */
			if (TILE_TYPE_CROP_EN & ptr_func->type)
			{
				/* input touch left edge */
				if (TILE_EDGE_LEFT_MASK & ptr_func->tdr_edge)
				{
					/* output don't touch left edge */
					if (ptr_func->out_pos_xs > 0)
					{
						ptr_func->tdr_edge &= ~TILE_EDGE_LEFT_MASK;
					}
				}
				/* input don't touch left edge */
				else
				{
					/* output touch left edge */
					if (ptr_func->out_pos_xs <= 0)
					{
						ptr_func->tdr_edge |= TILE_EDGE_LEFT_MASK;
					}
				}
				/* input touch right edge */
				if (TILE_EDGE_RIGHT_MASK & ptr_func->tdr_edge)
				{
					/* output don't touch right edge */
					if (ptr_func->out_pos_xe + 1 < ptr_func->full_size_x_out)
					{
						ptr_func->tdr_edge &= ~TILE_EDGE_RIGHT_MASK;
					}
				}
				/* input don't touch right edge */
				else
				{
					/* output touch right edge */
					if (ptr_func->out_pos_xe + 1 >= ptr_func->full_size_x_out)
					{
							ptr_func->tdr_edge |= TILE_EDGE_RIGHT_MASK;
					}
				}
			}
			/* for all modules - set left edge if output touch left edge */
			if (ptr_func->out_pos_xs <= 0)
			{
				ptr_func->tdr_edge |= TILE_EDGE_LEFT_MASK;
			}
			if ((false == ptr_tile_reg_map->first_pass) && (false == ptr_tile_reg_map->first_frame))
			{
				if (ptr_tile_reg_map->tdr_ctrl_en)
				{
					if (TILE_RUN_MODE_MAIN == ptr_func->run_mode)
					{
						if (ptr_func->in_max_width)
						{
							if (ptr_func->in_log_width + ptr_func->in_pos_xs < ptr_func->in_pos_xe + 1)
							{
								ptr_func->in_log_width = ptr_func->in_pos_xe - ptr_func->in_pos_xs + 1;
							}
						}
						if (ptr_func->out_max_width)
						{
							if (ptr_func->out_log_width + ptr_func->out_pos_xs < ptr_func->out_pos_xe + 1)
							{
								ptr_func->out_log_width = ptr_func->out_pos_xe - ptr_func->out_pos_xs + 1;
							}
						}
					}
				}
			}
		}
	}
	return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_forward_by_func_post_x_inv(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
    if (false == ptr_tile_reg_map->skip_x_cal)
    {
		if (false == ptr_func->tdr_h_disable_flag)
		{
			/* crop module */
			if (TILE_TYPE_CROP_EN & ptr_func->type)
			{
				/* input touch right edge */
				if (TILE_EDGE_RIGHT_MASK & ptr_func->tdr_edge)
				{
					/* output don't touch right edge*/
					if (ptr_func->out_pos_xe + 1 < ptr_func->full_size_x_out)
					{
						ptr_func->tdr_edge &= ~TILE_EDGE_RIGHT_MASK;
					}
				}
				/* input don't touch right edge */
				else
				{
					/* output touch right edge*/
					if (ptr_func->out_pos_xe + 1 >= ptr_func->full_size_x_out)
					{
						ptr_func->tdr_edge |= TILE_EDGE_RIGHT_MASK;
					}
				}
				/* input touch left edge */
				if (TILE_EDGE_LEFT_MASK & ptr_func->tdr_edge)
				{
					/* output don't touch left edge */
					if (ptr_func->out_pos_xs > 0)
					{
						ptr_func->tdr_edge &= ~TILE_EDGE_LEFT_MASK;
					}
				}
				/* input don't touch left edge */
				else
				{
					/* output touch left edge */
					if (ptr_func->out_pos_xs <= 0)
					{
						ptr_func->tdr_edge |= TILE_EDGE_LEFT_MASK;
					}
				}
			}
			/* for all modules - set right edge if output touch right edge */
			if (ptr_func->out_pos_xe + 1 >= ptr_func->full_size_x_out)
			{
				ptr_func->tdr_edge |= TILE_EDGE_RIGHT_MASK;
			}
			if ((false == ptr_tile_reg_map->first_pass) && (false == ptr_tile_reg_map->first_frame))
			{
				if (ptr_tile_reg_map->tdr_ctrl_en)
				{
					if (TILE_RUN_MODE_MAIN == ptr_func->run_mode)
					{
						if (ptr_func->in_max_width)
						{
							if (ptr_func->in_log_width + ptr_func->in_pos_xs < ptr_func->in_pos_xe + 1)
							{
								ptr_func->in_log_width = ptr_func->in_pos_xe - ptr_func->in_pos_xs + 1;
							}
						}
						if (ptr_func->out_max_width)
						{
							if (ptr_func->out_log_width + ptr_func->out_pos_xs < ptr_func->out_pos_xe + 1)
							{
								ptr_func->out_log_width = ptr_func->out_pos_xe - ptr_func->out_pos_xs + 1;
							}
						}
					}
				}
			}
		}
    }
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_forward_by_func_post_y(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
    if (false == ptr_tile_reg_map->skip_y_cal)
    {
		if (false == ptr_func->tdr_v_disable_flag)
		{
			/* crop module */
			if (TILE_TYPE_CROP_EN & ptr_func->type)
			{
				/* input touch top edge */
				if (TILE_EDGE_TOP_MASK & ptr_func->tdr_edge)
				{
					/* output don't touch top edge */
					if (ptr_func->out_pos_ys > 0)
					{
						ptr_func->tdr_edge &= ~TILE_EDGE_TOP_MASK;
					}
				}
				/* input don't touch top edge */
				else
				{
					/* output touch top edge */
					if (ptr_func->out_pos_ys <= 0)
					{
						ptr_func->tdr_edge |= TILE_EDGE_TOP_MASK;
					}
				}
				/* input touch bottom edge */
				if (TILE_EDGE_BOTTOM_MASK & ptr_func->tdr_edge)
				{
					/* output don't touch bottom edge */
					if (ptr_func->out_pos_ye + 1 < ptr_func->full_size_y_out)
					{
						ptr_func->tdr_edge &= ~TILE_EDGE_BOTTOM_MASK;
					}
				}
				/* input don't touch bottom edge */
				else
				{
					/* output touch bottom edge */
					if (ptr_func->out_pos_ye + 1 >= ptr_func->full_size_y_out)
					{
						ptr_func->tdr_edge |= TILE_EDGE_BOTTOM_MASK;
					}
				}
			}
			/* for all modules - set top edge if output touch top edge */
			if (ptr_func->out_pos_ys <= 0)
			{
				ptr_func->tdr_edge |= TILE_EDGE_TOP_MASK;
			}
			if ((false == ptr_tile_reg_map->first_pass) && (false == ptr_tile_reg_map->first_frame))
			{
				if (ptr_tile_reg_map->tdr_ctrl_en)
				{
					if (TILE_RUN_MODE_MAIN == ptr_func->run_mode)
					{
						if (ptr_func->in_max_height)
						{
							if (ptr_func->in_log_height + ptr_func->in_pos_ys < ptr_func->in_pos_ye + 1)
							{
								ptr_func->in_log_height = ptr_func->in_pos_ye - ptr_func->in_pos_ys + 1;
							}
						}
						if (ptr_func->out_max_height)
						{
							if (ptr_func->out_log_height + ptr_func->out_pos_ys < ptr_func->out_pos_ye + 1)
							{
								ptr_func->out_log_height = ptr_func->out_pos_ye - ptr_func->out_pos_ys + 1;
							}
						}
					}
				}
			}
		}
    }
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_forward_by_func(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
	/* check cal order, input */
	if (ptr_func->in_cal_order & TILE_ORDER_RIGHT_TO_LEFT)
	{
		result = tile_forward_by_func_pre_x_inv(ptr_func, ptr_tile_reg_map);
	}
	else
	{
		result = tile_forward_by_func_pre_x(ptr_func, ptr_tile_reg_map);
	}
	if (ISP_MESSAGE_TILE_OK == result)
	{
		result = tile_forward_by_func_pre_y(ptr_func, ptr_tile_reg_map);
    }
    if (ISP_MESSAGE_TILE_OK == result)
    {
		/* forward comp */
		if (ptr_func->enable_flag)
		{
			if ((!ptr_tile_reg_map->skip_x_cal && !ptr_func->tdr_h_disable_flag) ||
				(!ptr_tile_reg_map->skip_y_cal && !ptr_func->tdr_v_disable_flag))
			{
				result = tile_for_func_run(ptr_func, ptr_tile_reg_map);
			}
		}
	}
    if (ISP_MESSAGE_TILE_OK == result)
    {
       /* check cal order, output */
		if (ptr_func->out_cal_order & TILE_ORDER_RIGHT_TO_LEFT)
		{
			result = tile_forward_by_func_post_x_inv(ptr_func, ptr_tile_reg_map);
		}
		else
		{
			result = tile_forward_by_func_post_x(ptr_func, ptr_tile_reg_map);
		}
    }
	if (ISP_MESSAGE_TILE_OK == result)
	{
		result = tile_forward_by_func_post_y(ptr_func, ptr_tile_reg_map);
	}
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_forward_by_func_no_back_pre_x(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
    if (false == ptr_tile_reg_map->skip_x_cal)
    {
		if (false == ptr_func->tdr_h_disable_flag)
		{
			/* update input left edge */
			if (ptr_func->in_pos_xs <= 0)
			{
				ptr_func->tdr_edge |= TILE_EDGE_LEFT_MASK;
			}
			else
			{
				ptr_func->tdr_edge &= ~TILE_EDGE_LEFT_MASK;
			}
			/* set fixed left tile loss */
			if (TILE_EDGE_LEFT_MASK & ptr_func->tdr_edge)
			{
				ptr_func->out_pos_xs = ptr_func->in_pos_xs;
			}
			else
			{
				if (true == ptr_func->enable_flag)
				{
					ptr_func->out_pos_xs = ptr_func->in_pos_xs + ptr_func->l_tile_loss;
				}
				else
				{
					ptr_func->out_pos_xs = ptr_func->in_pos_xs;
				}
			}
			/* update input right edge */
			if (ptr_func->in_pos_xe + 1 < ptr_func->full_size_x_in)
			{
				ptr_func->tdr_edge &= ~TILE_EDGE_RIGHT_MASK;
			}
			else
			{
				ptr_func->tdr_edge |= TILE_EDGE_RIGHT_MASK;
			}
			/* set fixed right tile loss */
			if (TILE_EDGE_RIGHT_MASK & ptr_func->tdr_edge)
			{
				ptr_func->out_pos_xe = ptr_func->in_pos_xe;
			}
			else
			{
				if (true == ptr_func->enable_flag)
				{
					ptr_func->out_pos_xe = ptr_func->in_pos_xe - ptr_func->r_tile_loss;
				}
				else
				{
					ptr_func->out_pos_xe = ptr_func->in_pos_xe;
				}
			}
		}
    }
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_forward_by_func_no_back_pre_y(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
    if (false == ptr_tile_reg_map->skip_y_cal)
    {
		if (false == ptr_func->tdr_v_disable_flag)
		{
			/* update input top edge */
			if (ptr_func->in_pos_ys <= 0)
			{
				ptr_func->tdr_edge |= TILE_EDGE_TOP_MASK;
			}
			else
			{
				ptr_func->tdr_edge &= ~TILE_EDGE_TOP_MASK;
			}
			/* set fixed top tile loss */
			if (TILE_EDGE_TOP_MASK & ptr_func->tdr_edge)
			{
				ptr_func->out_pos_ys = ptr_func->in_pos_ys;
			}
			else
			{
				if (true == ptr_func->enable_flag)
				{
					ptr_func->out_pos_ys = ptr_func->in_pos_ys + ptr_func->t_tile_loss;
				}
				else
				{
					ptr_func->out_pos_ys = ptr_func->in_pos_ys;
				}
			}
			/* update input bottom edge */
			if (ptr_func->in_pos_ye + 1 < ptr_func->full_size_y_in)
			{
				ptr_func->tdr_edge &= ~TILE_EDGE_BOTTOM_MASK;
			}
			else
			{
				ptr_func->tdr_edge |= TILE_EDGE_BOTTOM_MASK;
			}
			/* set fixed bottom tile loss */
			if (TILE_EDGE_BOTTOM_MASK & ptr_func->tdr_edge)
			{
				ptr_func->out_pos_ye = ptr_func->in_pos_ye;
			}
			else
			{
				if (true == ptr_func->enable_flag)
				{
					ptr_func->out_pos_ye = ptr_func->in_pos_ye - ptr_func->b_tile_loss;
				}
				else
				{
					ptr_func->out_pos_ye = ptr_func->in_pos_ye;
				}
			}
		}
    }
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_forward_by_func_no_back_post_x(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
    if (false == ptr_tile_reg_map->skip_x_cal)
    {
		if (false == ptr_func->tdr_h_disable_flag)
		{
			/* update output right edge */
			if (ptr_func->out_pos_xe + 1 >= ptr_func->full_size_x_out)
			{
				ptr_func->tdr_edge |= TILE_EDGE_RIGHT_MASK;
			}
			else
			{
				ptr_func->tdr_edge &= ~TILE_EDGE_RIGHT_MASK;
			}
			/* update output left edge */
			if (ptr_func->out_pos_xs > 0)
			{
				ptr_func->tdr_edge &= ~TILE_EDGE_LEFT_MASK;
			}
			else
			{
				ptr_func->tdr_edge |= TILE_EDGE_LEFT_MASK;
			}
		}
    }
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_forward_by_func_no_back_post_y(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
    if (false == ptr_tile_reg_map->skip_y_cal)
    {
	    if (false == ptr_func->tdr_v_disable_flag)
		{
			/* update output bottom edge */
			if (ptr_func->out_pos_ye + 1 >= ptr_func->full_size_y_out)
			{
				ptr_func->tdr_edge |= TILE_EDGE_BOTTOM_MASK;
			}
			else
			{
				ptr_func->tdr_edge &= ~TILE_EDGE_BOTTOM_MASK;
			}
			/* update output top edge */
			if (ptr_func->out_pos_ys > 0)
			{
				ptr_func->tdr_edge &= ~TILE_EDGE_TOP_MASK;
			}
			else
			{
				ptr_func->tdr_edge |= TILE_EDGE_TOP_MASK;
			}
		}
    }
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_forward_by_func_no_back(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
	result = tile_forward_by_func_no_back_pre_x(ptr_func, ptr_tile_reg_map);
	if (ISP_MESSAGE_TILE_OK == result)
	{
		result = tile_forward_by_func_no_back_pre_y(ptr_func, ptr_tile_reg_map);
    }
    if (ISP_MESSAGE_TILE_OK == result)
    {
		/* forward comp */
		if (ptr_func->enable_flag)
		{
			if ((!ptr_tile_reg_map->skip_x_cal && !ptr_func->tdr_h_disable_flag) ||
				(!ptr_tile_reg_map->skip_y_cal && !ptr_func->tdr_v_disable_flag))
			{
				result = tile_for_func_run(ptr_func, ptr_tile_reg_map);
			}
		}
	}
    if (ISP_MESSAGE_TILE_OK == result)
    {
		result = tile_forward_by_func_no_back_post_x(ptr_func, ptr_tile_reg_map);
    }
	if (ISP_MESSAGE_TILE_OK == result)
	{
		result = tile_forward_by_func_no_back_post_y(ptr_func, ptr_tile_reg_map);
	}
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_schedule_forward(FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    int module_no = ptr_tile_func_param->used_func_no;
    int i;
    /* clear valid flag */
	memset(ptr_tile_func_param->valid_flag, 0x0, 4 * ((unsigned int)(module_no + 31) >> 5));
    /* scheduling forward */
    for (i=0;i<module_no;i++)
    {
        int j;
        bool found_flag = false;
        unsigned char *ptr_order = (unsigned char *)&ptr_tile_func_param->scheduling_forward_order[i];
        for (j=0;j<module_no;j++)
        {
			unsigned int *ptr_valid = &ptr_tile_func_param->valid_flag[(unsigned int)j>>5];
            if (false == (*ptr_valid & (1<<(j & 0x1F))))/* skip valid */
            {
				TILE_FUNC_BLOCK_STRUCT *ptr_func =
					ptr_tile_func_param->func_list[j];
				int k;
                for (k=0;k<ptr_func->tot_prev_num;k++)
                {
                    /* start to set valid*/
                    if (PREVIOUS_BLK_NO_OF_START == ptr_func->prev_blk_num[k])
                    {
                        found_flag = true;
                    }
                    /* non-start to set valid if previous valid */
                    else
                    {
                        if (false == (ptr_tile_func_param->valid_flag[ptr_func->prev_blk_num[k]>>5] &
                            (1<<(ptr_func->prev_blk_num[k] & 0x1F))))
                        {
                            found_flag = false;
                            break;
                        }
                        else
                        {
                            found_flag = true;
                        }
                    }
                }
                if (found_flag)
                {
                    *ptr_order = j;
                    *ptr_valid |= 1<<(j & 0x1F);
                    break;
                }
            }
        }
        if (false == found_flag)
        {
            result = ISP_MESSAGE_SCHEDULING_FORWARD_ERROR;
            tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
            return result;
        }
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_forward_recusive_check(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
													FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param, bool *ptr_restart_flag)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
	if (false == ptr_tile_reg_map->skip_x_cal)
	{
		if (false == ptr_func->tdr_h_disable_flag)
		{
			/* check cal order, output */
			if (ptr_func->out_cal_order & TILE_ORDER_RIGHT_TO_LEFT)
			{
				/* error check */
				if ((ptr_func->out_tile_width && (ptr_func->out_pos_xe + 1 > ptr_func->out_pos_xs + ptr_func->out_tile_width)) ||
					((ptr_func->out_const_x > 1) && (0 != TILE_MOD(ptr_func->out_pos_xs, ptr_func->out_const_x))))
				{
					int i;
					*ptr_restart_flag = true;
					if (ptr_tile_reg_map->recursive_forward_en)
					{
						int tile_reg_map_run_mode = ptr_tile_reg_map->run_mode;
						/* xe mis-alignment to reduce source xe */
						for (i=0;i<ptr_tile_func_param->used_func_no;i++)
						{
							TILE_FUNC_BLOCK_STRUCT *ptr_start =
								ptr_tile_func_param->func_list[i];
							if (false == ptr_start->output_disable_flag)
							{
								if (tile_reg_map_run_mode == ptr_start->run_mode)
								{
									if (PREVIOUS_BLK_NO_OF_START == ptr_start->prev_blk_num[0])
									{
										ptr_start->in_pos_xs += ptr_start->in_const_x;
										if ((ptr_start->in_pos_xs > ptr_start->in_pos_xe) || (ptr_start->in_pos_xs > ptr_start->min_tile_in_pos_xs))
										{
											result = ISP_MESSAGE_INCORRECT_XE_INPUT_POS_REDUCED_BY_TILE_SIZE_ERROR;
											tile_driver_printf("Error [%s][%s] %s\r\n", ptr_start->func_name, ptr_func->func_name, tile_print_error_message(result));
											return result;
										}
									}
								}
							}
						}
					}
					else
					{
						result = ISP_MESSAGE_RECURSIVE_FOUND_ERROR;
						tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
						return result;
					}
				}
			}
			else
			{
				/* error check */
				if ((ptr_func->out_tile_width && (ptr_func->out_pos_xe + 1 > ptr_func->out_pos_xs + ptr_func->out_tile_width)) ||
					((ptr_func->out_const_x > 1) && (0 != ((ptr_func->out_pos_xe + 1) % ptr_func->out_const_x))))
				{
					int i;
					*ptr_restart_flag = true;
					if (ptr_tile_reg_map->recursive_forward_en)
					{
						int tile_reg_map_run_mode = ptr_tile_reg_map->run_mode;
						/* xe mis-alignment to reduce source xe */
						for (i=0;i<ptr_tile_func_param->used_func_no;i++)
						{
							TILE_FUNC_BLOCK_STRUCT *ptr_start =
								ptr_tile_func_param->func_list[i];
							if (false == ptr_start->output_disable_flag)
							{
								if (tile_reg_map_run_mode == ptr_start->run_mode)
								{
									if (PREVIOUS_BLK_NO_OF_START == ptr_start->prev_blk_num[0])
									{
										ptr_start->in_pos_xe -= ptr_start->in_const_x;
										if ((ptr_start->in_pos_xs > ptr_start->in_pos_xe) || (ptr_start->in_pos_xe < ptr_start->min_tile_in_pos_xe))
										{
											result = ISP_MESSAGE_INCORRECT_XE_INPUT_POS_REDUCED_BY_TILE_SIZE_ERROR;
											tile_driver_printf("Error [%s][%s] %s\r\n", ptr_start->func_name, ptr_func->func_name, tile_print_error_message(result));
											return result;
										}
									}
								}
							}
						}
					}
					else
					{
						result = ISP_MESSAGE_RECURSIVE_FOUND_ERROR;
						tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
						return result;
					}
				}
			}
		}
	}
    if (false == ptr_tile_reg_map->skip_y_cal)
	{
		if (false == ptr_func->tdr_v_disable_flag)
		{
			/* check cal order, not support */
			if ((ptr_func->out_tile_height && (ptr_func->out_pos_ye + 1 > ptr_func->out_pos_ys + ptr_func->out_tile_height))||
				((ptr_func->out_const_y > 1) && (0 != TILE_MOD(ptr_func->out_pos_ye + 1, ptr_func->out_const_y))))
			{
				int i;
				*ptr_restart_flag = true;
				if (ptr_tile_reg_map->recursive_forward_en)
				{
					int tile_reg_map_run_mode = ptr_tile_reg_map->run_mode;
					/* ye mis-alignment to reduce source ye */
					for (i=0;i<ptr_tile_func_param->used_func_no;i++)
					{
						TILE_FUNC_BLOCK_STRUCT *ptr_start =
							ptr_tile_func_param->func_list[i];
						if (false == ptr_start->output_disable_flag)
						{
							if (tile_reg_map_run_mode == ptr_start->run_mode)
							{
								if (PREVIOUS_BLK_NO_OF_START == ptr_start->prev_blk_num[0])
								{
									ptr_start->in_pos_ye -= ptr_start->in_const_y;
									if ((ptr_start->in_pos_ys > ptr_start->in_pos_ye) || (ptr_start->in_pos_ye < ptr_start->min_tile_in_pos_ye))
									{
										result = ISP_MESSAGE_INCORRECT_YE_INPUT_POS_REDUCED_BY_TILE_SIZE_ERROR;
										tile_driver_printf("Error [%s][%s] %s\r\n", ptr_start->func_name, ptr_func->func_name, tile_print_error_message(result));
										return result;
									}
								}
							}
						}
					}
				}
				else
				{
					result = ISP_MESSAGE_RECURSIVE_FOUND_ERROR;
					tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
					return result;
				}
			}
		}
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_forward_comp_no_back(TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
													FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    /* check early terminated */
	if (false == ptr_tile_reg_map->backup_x_skip_y)
    {
	    int i;
		int tile_reg_map_run_mode = ptr_tile_reg_map->run_mode;
		/* loop count can be reset for mis-alignment handle */
		for (i=0;i<ptr_tile_func_param->used_func_no;i++)
		{
			if (ISP_MESSAGE_TILE_OK == result)
			{
				unsigned char module_order = ptr_tile_func_param->scheduling_forward_order[i];
				TILE_FUNC_BLOCK_STRUCT *ptr_func =
					ptr_tile_func_param->func_list[module_order];

				/* skip output disable func */
				if (ptr_func->output_disable_flag)
				{
					continue;
				}
				if (tile_reg_map_run_mode != ptr_func->run_mode)
				{
					continue;
				}
				/* config forward in pos of func */
				result = tile_forward_input_config(ptr_func, ptr_tile_reg_map, ptr_tile_func_param);
				/* forward comp by func */
				if (ISP_MESSAGE_TILE_OK == result)
				{
					result = tile_forward_by_func_no_back(ptr_func, ptr_tile_reg_map);
				}
				/* check output smaller than tile size & out size */
				if (ISP_MESSAGE_TILE_OK == result)
				{
					result = tile_forward_output_check(ptr_func, ptr_tile_reg_map);
				}
			}
			else
			{
				return result;
			}
		}
	}
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_forward_comp(TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
													FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
    /* check early terminated */
	if (false == ptr_tile_reg_map->backup_x_skip_y)
    {
        int i, loop_count=0;
		int tile_reg_map_run_mode = ptr_tile_reg_map->run_mode;
        /* loop count can be reset for mis-alignment handle */
        for (i=0;i<ptr_tile_func_param->used_func_no;loop_count++)
        {
		unsigned char module_order = ptr_tile_func_param->scheduling_forward_order[i];
			TILE_FUNC_BLOCK_STRUCT *ptr_func =
				ptr_tile_func_param->func_list[module_order];
			/* skip output disable func */
			if (false == ptr_func->output_disable_flag)
			{
				if (tile_reg_map_run_mode == ptr_func->run_mode)
				{
					/* check forward loop count */
					if (loop_count >= MAX_FORWARD_FUNC_CAL_LOOP_NO)
					{
						result = ISP_MESSAGE_FORWARD_FUNC_CAL_LOOP_COUNT_OVER_MAX_ERROR;
						tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
						return result;
					}
					/* config forward in pos of func */
					result = tile_forward_input_config(ptr_func, ptr_tile_reg_map, ptr_tile_func_param);
					/* forward comp by func */
					if (ISP_MESSAGE_TILE_OK == result)
					{
						result = tile_forward_by_func(ptr_func, ptr_tile_reg_map);
					}
					/* check over tile size then forward or xe/ye mis-alignment happen */
					if (ISP_MESSAGE_TILE_OK == result)
					{
						bool restart_flag = false;
						/* check & apply recusive call if enabled */
						result = tile_forward_recusive_check(ptr_func, ptr_tile_reg_map, ptr_tile_func_param, &restart_flag);
						if ((ISP_MESSAGE_TILE_OK == result) && (true == restart_flag))
						{
							/* reduce source xe to meet alignment */
							i = 0;
							ptr_tile_func_param->for_recursive_count++;
							continue;
						}
					}
					/* check output smaller than tile size & out size */
					if (ISP_MESSAGE_TILE_OK == result)
					{
						result = tile_forward_output_check(ptr_func, ptr_tile_reg_map);
					}
                }
			}
            if (ISP_MESSAGE_TILE_OK == result)
            {
                i++;//increase count
            }
            else
            {
                return result;
            }
        }
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_forward_input_config(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
												  FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
	if (false == ptr_tile_reg_map->skip_x_cal)
	{
		if (false == ptr_func->tdr_h_disable_flag)
		{
			if (PREVIOUS_BLK_NO_OF_START != ptr_func->prev_blk_num[0])/* skip start module */
			{
				bool found_prev = false;
				int i;
				int tile_reg_map_run_mode = ptr_tile_reg_map->run_mode;
				for (i=0;i<ptr_func->tot_prev_num;i++)
				{
					TILE_FUNC_BLOCK_STRUCT *ptr_prev =
						ptr_tile_func_param->func_list[
						ptr_func->prev_blk_num[i]];

					if (ptr_prev->output_disable_flag)
					{
						continue;
					}
					if (tile_reg_map_run_mode == (tile_reg_map_run_mode & ptr_prev->run_mode))
					{
						if (found_prev)
						{
							if ((ptr_func->in_pos_xs != ptr_prev->out_pos_xs) ||
								(ptr_func->in_pos_xe != ptr_prev->out_pos_xe) ||
								(ptr_func->h_end_flag != ptr_prev->h_end_flag) ||
								(ptr_func->tdr_h_disable_flag != ptr_prev->tdr_h_disable_flag))
							{
								result = ISP_MESSAGE_DIFF_PREV_FORWARD_ERROR;
								tile_driver_printf("Error [%s][%s] %s\r\n", ptr_prev->func_name, ptr_func->func_name, tile_print_error_message(result));
								return result;
							}
						}
						else
						{
							ptr_func->tdr_h_disable_flag = ptr_prev->tdr_h_disable_flag;
							ptr_func->h_end_flag = ptr_prev->h_end_flag;
							if (false == ptr_prev->tdr_h_disable_flag)
							{
								ptr_func->in_pos_xs = ptr_prev->out_pos_xs;
								ptr_func->in_pos_xe = ptr_prev->out_pos_xe;
							}
							found_prev = true;
						}
					}
				}
				if (false == found_prev)
				{
					result = ISP_MESSAGE_DIFF_VIEW_BRANCH_MERGE_ERROR;
					tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
					return result;
				}
			}
			if (false == ptr_func->tdr_h_disable_flag)
			{
				/* check min tile */
				if ((ptr_func->in_pos_xs > ptr_func->min_tile_in_pos_xs) || (ptr_func->in_pos_xe < ptr_func->min_tile_in_pos_xe))
				{
					ptr_func->tdr_h_disable_flag = true;
				}
				else
				{
					if (ptr_func->enable_flag)
					{
						if (ptr_func->in_pos_xs + ptr_func->in_min_width > ptr_func->in_pos_xe + 1)
						{
							result = ISP_MESSAGE_UNDER_MIN_XSIZE_ERROR;
							tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
							return result;
						}
					}
				}
			}
		}
    }
    if (false == ptr_tile_reg_map->skip_y_cal)
	{
		if (false == ptr_func->tdr_v_disable_flag)
		{
			if (PREVIOUS_BLK_NO_OF_START != ptr_func->prev_blk_num[0])/* skip start module */
			{
				bool found_prev = false;
				int i;
				int tile_reg_map_run_mode = ptr_tile_reg_map->run_mode;
				for (i=0;i<ptr_func->tot_prev_num;i++)
				{
					TILE_FUNC_BLOCK_STRUCT *ptr_prev =
						ptr_tile_func_param->func_list[
						ptr_func->prev_blk_num[i]];
					if (ptr_prev->output_disable_flag)
					{
						continue;
					}
					if (tile_reg_map_run_mode == (tile_reg_map_run_mode & ptr_prev->run_mode))
					{
						if (found_prev)
						{
							if ((ptr_func->in_pos_ys != ptr_prev->out_pos_ys) ||
								(ptr_func->in_pos_ye != ptr_prev->out_pos_ye) ||
								(ptr_func->v_end_flag != ptr_prev->v_end_flag) ||
								(ptr_func->tdr_v_disable_flag != ptr_prev->tdr_v_disable_flag))
							{
								result = ISP_MESSAGE_DIFF_PREV_FORWARD_ERROR;
								tile_driver_printf("Error [%s][%s] %s\r\n", ptr_prev->func_name, ptr_func->func_name, tile_print_error_message(result));
								return result;
							}
						}
						else
						{
							ptr_func->tdr_v_disable_flag = ptr_prev->tdr_v_disable_flag;
							ptr_func->v_end_flag = ptr_prev->v_end_flag;
							if (false == ptr_prev->tdr_v_disable_flag)
							{
								ptr_func->in_pos_ys = ptr_prev->out_pos_ys;
								ptr_func->in_pos_ye = ptr_prev->out_pos_ye;
							}
							found_prev = true;
						}
					}
				}
				if (false == found_prev)
				{
					result = ISP_MESSAGE_DIFF_VIEW_BRANCH_MERGE_ERROR;
					tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
					return result;
				}
			}
			if (false == ptr_func->tdr_v_disable_flag)
			{
				/* check min tile */
				if ((ptr_func->in_pos_ys > ptr_func->min_tile_in_pos_ys) || (ptr_func->in_pos_ye < ptr_func->min_tile_in_pos_ye))
				{
					ptr_func->tdr_v_disable_flag = true;
				}
				else
				{
					if (ptr_func->enable_flag)
					{
						if (ptr_func->in_pos_ys + ptr_func->in_min_height > ptr_func->in_pos_ye + 1)
						{
							result = ISP_MESSAGE_UNDER_MIN_YSIZE_ERROR;
							tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
							return result;
						}
					}
				}
			}
		}
    }
	result = tile_check_input_config(ptr_func, ptr_tile_reg_map);
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_init_tdr_ctrl_flag(TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
												FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param)
{
	int i;
	int tile_reg_map_skip_x_cal = ptr_tile_reg_map->skip_x_cal;
	int tile_reg_map_skip_y_cal = ptr_tile_reg_map->skip_y_cal;
    if (ptr_tile_reg_map->backup_x_skip_y)
    {
		return ISP_MESSAGE_TILE_OK;
	}
	if (ptr_tile_reg_map->first_frame)
	{
		return ISP_MESSAGE_TILE_OK;
	}
	if (false == ptr_tile_reg_map->tdr_ctrl_en)
	{
		return ISP_MESSAGE_TILE_OK;
	}
	for (i=0;i<ptr_tile_func_param->used_func_no;i++)
	{
		TILE_FUNC_BLOCK_STRUCT *ptr_func = ptr_tile_func_param->func_list[i];
		if (false == ptr_func->output_disable_flag)
		{
			/* init end func */
			if (0 == ptr_func->tot_branch_num)
			{
				/* init no direct link */
				if (0x0 == (ptr_func->type & TILE_TYPE_DONT_CARE_END))
				{
					/* store horizontal tile parameters */
					if (false == tile_reg_map_skip_x_cal)
					{
						ptr_func->tdr_h_disable_flag = false;/* clear flag */
					}
					if (false == tile_reg_map_skip_y_cal)
					{
						ptr_func->tdr_v_disable_flag = false;/* clear flag */
					}
				}
			}
			else
			{
				/* init sub-in */
				if (TILE_RUN_MODE_SUB_IN == ptr_func->run_mode)
				{
					/* store horizontal tile parameters */
					if (false == tile_reg_map_skip_x_cal)
					{
						ptr_func->tdr_h_disable_flag = false;/* clear flag */
					}
					if (false == tile_reg_map_skip_y_cal)
					{
						ptr_func->tdr_v_disable_flag = false;/* clear flag */
					}
				}
			}
		}
	}
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_update_last_x_y(TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
	FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param,
	bool x_end_flag, bool y_end_flag)
{
	int curr_vertical_tile_no = ptr_tile_reg_map->curr_vertical_tile_no;
	int curr_horizontal_tile_no = ptr_tile_reg_map->curr_horizontal_tile_no;
	int first_frame = ptr_tile_reg_map->first_frame;
	int i;

	for (i = 0; i < ptr_tile_func_param->used_func_no; i++) {
		TILE_FUNC_BLOCK_STRUCT *ptr_func = ptr_tile_func_param->func_list[i];

		if (false == ptr_func->output_disable_flag)
		{
			if ((false == ptr_func->tdr_v_disable_flag) && (false == ptr_func->tdr_h_disable_flag))
			{
				/* skip update when frame mode */
				if (false == first_frame)
				{
					/* udpate valid_h_no by func */
					ptr_func->last_valid_tile_no = ptr_tile_reg_map->used_tile_no;
				}
			}
			/* skip backup x if v_no = 1 */
			if (false == y_end_flag)
			{
				/* store horizontal tile parameters */
				if(curr_vertical_tile_no == 0)
				{
					/* assign parameter pointer if there is enough buffer */
					if(curr_horizontal_tile_no < MAX_TILE_BACKUP_HORZ_NO)
					{
						struct tile_horz_backup *ptr_para =
							&ptr_func->horz_para[
							curr_horizontal_tile_no];

						HORZ_PARA_BACKUP(ptr_para, ptr_func);
					}
				}
			}
			/* update last x if not x end */
			if (false == x_end_flag)
			{
				if (false == ptr_func->tdr_h_disable_flag)
				{
					ptr_func->last_input_xs_pos = ptr_func->in_pos_xs;
					ptr_func->last_input_xe_pos = ptr_func->in_pos_xe;
					if (ptr_func->valid_h_no)
					{
						/* diff view to update min last xs & max last xe */
						if (ptr_func->in_pos_xs < ptr_func->min_last_input_xs_pos)
						{
							ptr_func->min_last_input_xs_pos = ptr_func->in_pos_xs;
						}
						if (ptr_func->max_last_input_xe_pos < ptr_func->in_pos_xe)
						{
							ptr_func->max_last_input_xe_pos = ptr_func->in_pos_xe;
						}
					}
					else
					{
						ptr_func->min_last_input_xs_pos = ptr_func->in_pos_xs;
						ptr_func->max_last_input_xe_pos = ptr_func->in_pos_xe;
					}
					ptr_func->last_output_xs_pos = ptr_func->out_pos_xs;
					ptr_func->last_output_xe_pos = ptr_func->out_pos_xe;
					/* skip update when frame mode */
					if (false == first_frame)
					{
						/* udpate valid_h_no by func */
						ptr_func->valid_h_no++;
					}
				}
			}
			else
			{
				/* update last y if not y end */
				if (false == y_end_flag)
				{
					/* reset valid_h_no */
					ptr_func->valid_h_no = 0;
					if (false == ptr_func->tdr_v_disable_flag)
					{
						ptr_func->last_input_ys_pos = ptr_func->in_pos_ys;
						ptr_func->last_input_ye_pos = ptr_func->in_pos_ye;
						if (ptr_func->valid_v_no)
						{
							if (ptr_func->max_last_input_ye_pos < ptr_func->in_pos_ye)
							{
								ptr_func->max_last_input_ye_pos = ptr_func->in_pos_ye;
							}
						}
						else
						{
							ptr_func->max_last_input_ye_pos = ptr_func->in_pos_ye;
						}
						ptr_func->last_output_ys_pos = ptr_func->out_pos_ys;
						ptr_func->last_output_ye_pos = ptr_func->out_pos_ye;
						/* skip update when frame mode */
						if (false == first_frame)
						{
							/* udpate valid_v_no by func */
							ptr_func->valid_v_no++;
							/* udpate last_valid_v_no by func */
							ptr_func->last_valid_v_no = ptr_tile_reg_map->curr_vertical_tile_no;
						}
					}
				}
			}
		}
    }
	ptr_tile_reg_map->backup_x_skip_y = false;/* must clear flag for direct-link test */
	ptr_tile_reg_map->skip_x_cal = false;/* must clear flag for direct-link test */
	/* check x end found */
	if (x_end_flag)
	{
		ptr_tile_reg_map->curr_horizontal_tile_no = 0;
		ptr_tile_reg_map->curr_vertical_tile_no++;
		ptr_tile_reg_map->skip_y_cal = false;/* must clear flag for direct-link test */
	}
	else
	{
		ptr_tile_reg_map->curr_horizontal_tile_no++;
		ptr_tile_reg_map->skip_y_cal = true;/* must clear flag for direct-link test */
	}
    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_forward_output_check(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
	if (false == ptr_tile_reg_map->skip_x_cal)
	{
		if (false == ptr_func->tdr_h_disable_flag)
		{
			/* check resizer output xe & ye by tile size with enable & skip buffer check false */
			if (ptr_func->out_tile_width)
			{
				if (ptr_func->out_pos_xe + 1 > ptr_func->out_pos_xs + ptr_func->out_tile_width)
				{
					result = ISP_MESSAGE_TILE_FORWARD_OUT_OVER_TILE_WIDTH_ERROR;
					tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
					return result;
				}
			}
			if (ptr_func->enable_flag)
			{
				if (0 >= ptr_func->out_pos_xs)
				{
					if (TILE_EDGE_LEFT_MASK != (ptr_func->tdr_edge & TILE_EDGE_LEFT_MASK))
					{
						result = ISP_MESSAGE_FORWARD_CHECK_LEFT_EDGE_ERROR;
						tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
						return result;
					}
				}
				else
				{
					if (ptr_func->tdr_edge & TILE_EDGE_LEFT_MASK)
					{
						result = ISP_MESSAGE_FORWARD_CHECK_LEFT_EDGE_ERROR;
						tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
						return result;
					}
				}
				if (ptr_func->out_pos_xe + 1 >= ptr_func->full_size_x_out)
				{
					if (TILE_EDGE_RIGHT_MASK != (ptr_func->tdr_edge & TILE_EDGE_RIGHT_MASK))
					{
						result = ISP_MESSAGE_FORWARD_CHECK_RIGHT_EDGE_ERROR;
						tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
						return result;
					}
				}
				else
				{
					if (ptr_func->tdr_edge & TILE_EDGE_RIGHT_MASK)
					{
						result = ISP_MESSAGE_FORWARD_CHECK_RIGHT_EDGE_ERROR;
						tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
						return result;
					}
				}
			}
		}
    }
    if (false == ptr_tile_reg_map->skip_y_cal)
	{
		if (false == ptr_func->tdr_v_disable_flag)
		{
			if (ptr_func->out_tile_height)
			{
				if (ptr_func->out_pos_ye + 1 > ptr_func->out_pos_ys + ptr_func->out_tile_height)
				{
					result = ISP_MESSAGE_TILE_FORWARD_OUT_OVER_TILE_HEIGHT_ERROR;
					tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
					return result;
				}
			}
			if (ptr_func->enable_flag)
			{
				if (0 == ptr_func->out_pos_ys)
				{
					if (TILE_EDGE_TOP_MASK != (ptr_func->tdr_edge & TILE_EDGE_TOP_MASK))
					{
						result = ISP_MESSAGE_FORWARD_CHECK_TOP_EDGE_ERROR;
						tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
						return result;
					}
				}
				else
				{
					if (ptr_func->tdr_edge & TILE_EDGE_TOP_MASK)
					{
						result = ISP_MESSAGE_FORWARD_CHECK_TOP_EDGE_ERROR;
						tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
						return result;
					}
				}
				if (ptr_func->out_pos_ye + 1 >= ptr_func->full_size_y_out)
				{
					if (TILE_EDGE_BOTTOM_MASK != (ptr_func->tdr_edge & TILE_EDGE_BOTTOM_MASK))
					{
						result = ISP_MESSAGE_FORWARD_CHECK_BOTTOM_EDGE_ERROR;
						tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
						return result;
					}
				}
				else
				{
					if (ptr_func->tdr_edge & TILE_EDGE_BOTTOM_MASK)
					{
						result = ISP_MESSAGE_FORWARD_CHECK_BOTTOM_EDGE_ERROR;
						tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
						return result;
					}
				}
			}
		}
    }
    if (ISP_MESSAGE_TILE_OK == result)
    {
        result = tile_check_output_config(ptr_func, ptr_tile_reg_map);
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_compare_forward_back(TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
												  FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
	int i;
	int tile_reg_map_run_mode = ptr_tile_reg_map->run_mode;
	int tile_reg_map_skip_x_cal = ptr_tile_reg_map->skip_x_cal;
	int tile_reg_map_skip_y_cal = ptr_tile_reg_map->skip_y_cal;
	if (ptr_tile_reg_map->backup_x_skip_y)
	{
		return ISP_MESSAGE_TILE_OK;
	}
	for (i=0;i<ptr_tile_func_param->used_func_no;i++)
	{
		/* check by forward order */
		unsigned char module_order = ptr_tile_func_param->scheduling_forward_order[i];
		TILE_FUNC_BLOCK_STRUCT *ptr_func = ptr_tile_func_param->func_list[module_order];
		if (ptr_func->output_disable_flag)
		{
			continue;
		}
		if (tile_reg_map_run_mode != ptr_func->run_mode)
		{
			continue;
		}
		if (false  == tile_reg_map_skip_x_cal)
		{
			if (false == ptr_func->tdr_h_disable_flag)
			{
				if ((ptr_func->in_pos_xs != ptr_func->backward_input_xs_pos) ||
					(ptr_func->in_pos_xe != ptr_func->backward_input_xe_pos) ||
					(ptr_func->out_pos_xs != ptr_func->backward_output_xs_pos) ||
					(ptr_func->out_pos_xe != ptr_func->backward_output_xe_pos) ||
					(ptr_func->h_end_flag != ptr_func->backward_h_end_flag))
				{
					result = ISP_MESSAGE_FOR_BACK_COMP_X_ERROR;
					tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
					return result;
				}
			}
		}
		if (false  == tile_reg_map_skip_y_cal)
		{
			if (false == ptr_func->tdr_v_disable_flag)
			{
				if ((ptr_func->in_pos_ys != ptr_func->backward_input_ys_pos) ||
					(ptr_func->in_pos_ye != ptr_func->backward_input_ye_pos) ||
					(ptr_func->out_pos_ys != ptr_func->backward_output_ys_pos) ||
					(ptr_func->out_pos_ye != ptr_func->backward_output_ye_pos) ||
					(ptr_func->v_end_flag != ptr_func->backward_v_end_flag))
				{
					result = ISP_MESSAGE_FOR_BACK_COMP_Y_ERROR;
					tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
					return result;
				}
			}
		}
	}
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_check_x_end_pos_with_flag(
	TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
	FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param,
	bool *ptr_x_end_flag, int curr_tile_no)
{
	ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
	bool x_end_flag = ptr_tile_func_param->func_list[
		ptr_tile_reg_map->first_func_en_no]->h_end_flag;

	*ptr_x_end_flag = x_end_flag;
	if (false == ptr_tile_reg_map->skip_x_cal)
	{
		int curr_horizontal_tile_no = ptr_tile_reg_map->curr_horizontal_tile_no;
		if ((0 == curr_horizontal_tile_no) || x_end_flag)
		{
			/* check first frame */
			if (ptr_tile_reg_map->first_frame)
			{
				int j;
				for (j=0;j<ptr_tile_func_param->used_func_no;j++)
				{
					TILE_FUNC_BLOCK_STRUCT *ptr_func =
						ptr_tile_func_param->func_list[j];
					if (false == ptr_func->output_disable_flag)
					{
				        /* not direct link */
				        if (0x0 == (ptr_func->type & TILE_TYPE_DONT_CARE_END))
				        {
						    /* check only run mode & end functions */
						    if (0 == ptr_func->tot_branch_num)
						    {
							    if (TILE_RUN_MODE_MAIN == ptr_func->run_mode)
							    {
								    if ((ptr_func->min_out_pos_xs < ptr_func->out_pos_xs) ||
									    (ptr_func->out_pos_xe < ptr_func->max_out_pos_xe))
								    {
									    result = ISP_MESSAGE_TILE_X_DIR_NOT_END_TOGETHER_ERROR;
									    tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
									    break;
								    }
							    }
						    }
                        }
					}
				}
			}
			else
			{
				int j;
				for (j=0;j<ptr_tile_func_param->used_func_no;j++)
				{
					TILE_FUNC_BLOCK_STRUCT *ptr_func =
						ptr_tile_func_param->func_list[j];
					if (false == ptr_func->output_disable_flag)
					{
						/* check only run mode & end functions */
						if (TILE_RUN_MODE_MAIN == ptr_func->run_mode)
						{
							if (ptr_func->in_cal_order & TILE_ORDER_RIGHT_TO_LEFT)
							{
								if (0 == ptr_func->valid_h_no)
								{
									/* check input x size is end */
									/* check cal order, output */
									if (ptr_func->in_pos_xe < ptr_func->max_in_pos_xe)
									{
										result = ISP_MESSAGE_TILE_X_DIR_NOT_END_TOGETHER_ERROR;
										tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
										break;
									}
								}
								if (x_end_flag)
								{
									/* check input x size is end */
									/* check cal order, output */
									if (ptr_func->min_in_pos_xs < ptr_func->in_pos_xs)
									{
										/* diff view to check min last xs */
										if (ptr_func->valid_h_no)
										{
											if (ptr_func->min_in_pos_xs < ptr_func->min_last_input_xs_pos)
											{
												result = ISP_MESSAGE_TILE_X_DIR_NOT_END_TOGETHER_ERROR;
												tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
												break;
											}
										}
										else
										{
											result = ISP_MESSAGE_TILE_X_DIR_NOT_END_TOGETHER_ERROR;
											tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
											break;
										}
									}
								}
							}
							else
							{
								if (0 == curr_horizontal_tile_no)
								{
									/* check input x size is end */
									/* check cal order, output */
									if (ptr_func->min_in_pos_xs < ptr_func->in_pos_xs)
									{
										result = ISP_MESSAGE_TILE_X_DIR_NOT_END_TOGETHER_ERROR;
										tile_driver_printf("Error: %s\r\n", tile_print_error_message(result));
										break;
									}
								}
								if (x_end_flag)
								{
									/* check input x size is end */
									/* check cal order, output */
									if (ptr_func->in_pos_xe < ptr_func->max_in_pos_xe)
									{
										if (ptr_func->valid_h_no)
										{
											/* diff view to check max last xe */
											if (ptr_func->max_last_input_xe_pos < ptr_func->max_in_pos_xe)
											{
												result = ISP_MESSAGE_TILE_X_DIR_NOT_END_TOGETHER_ERROR;
												tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
												break;
											}
										}
										else
										{
											result = ISP_MESSAGE_TILE_X_DIR_NOT_END_TOGETHER_ERROR;
											tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
											break;
										}
									}
								}
							}
						}
					}
				}
			}
			if (x_end_flag)
			{
				/* set title h no once */
				if (ptr_tile_reg_map->horizontal_tile_no == 0)
				{
					ptr_tile_reg_map->horizontal_tile_no = curr_tile_no + 1;
				}
			}
		}
	}
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_check_y_end_pos_with_flag(
	TILE_REG_MAP_STRUCT *ptr_tile_reg_map,
	FUNC_DESCRIPTION_STRUCT *ptr_tile_func_param, bool *ptr_y_end_flag)
{
	ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;
	bool y_end_flag = ptr_tile_func_param->func_list[
		ptr_tile_reg_map->first_func_en_no]->v_end_flag;

	*ptr_y_end_flag = y_end_flag;
	if (false == ptr_tile_reg_map->skip_y_cal)
	{
		int curr_vertical_tile_no = ptr_tile_reg_map->curr_vertical_tile_no;
		if ((0 == curr_vertical_tile_no) || y_end_flag)
		{
			/* check first frame */
			if (ptr_tile_reg_map->first_frame)
			{
				int j;
				for (j=0;j<ptr_tile_func_param->used_func_no;j++)
				{
					TILE_FUNC_BLOCK_STRUCT *ptr_func =
						ptr_tile_func_param->func_list[j];
					if (false == ptr_func->output_disable_flag)
					{
				        /* not direct link */
				        if (0x0 == (ptr_func->type & TILE_TYPE_DONT_CARE_END))
				        {
						    /* check only run mode & end functions */
						    if (0 == ptr_func->tot_branch_num)
						    {
							    if (TILE_RUN_MODE_MAIN == ptr_func->run_mode)
							    {
								    /* check output y size is end */
								    /* check cal order, not support */
								    if ((ptr_func->out_pos_ye < ptr_func->max_out_pos_ye) ||
									    (ptr_func->out_pos_ys > ptr_func->min_out_pos_ys))
								    {
									    result = ISP_MESSAGE_TILE_Y_DIR_NOT_END_TOGETHER_ERROR;
									    tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
									    break;
								    }
							    }
						    }
                        }
					}
				}
			}
			else
			{
				int j;
				for (j=0;j<ptr_tile_func_param->used_func_no;j++)
				{
					TILE_FUNC_BLOCK_STRUCT *ptr_func =
						ptr_tile_func_param->func_list[j];
					if (false == ptr_func->output_disable_flag)
					{
						/* check only run mode & end functions */
						if (TILE_RUN_MODE_MAIN == ptr_func->run_mode)
						{
							if (0 == ptr_func->valid_v_no)
							{
								/* check output y size is end */
								/* check cal order, not support */
								if (ptr_func->in_pos_ys > ptr_func->min_in_pos_ys)
								{
									result = ISP_MESSAGE_TILE_Y_DIR_NOT_END_TOGETHER_ERROR;
									tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
									break;
								}
							}
							if (y_end_flag)
							{
								/* check output y size is end */
								/* check cal order, not support */
								if (ptr_func->in_pos_ye < ptr_func->max_in_pos_ye)
								{
									/* diff view to check min last ye */
									if (ptr_func->valid_v_no)
									{
										if (ptr_func->max_last_input_ye_pos < ptr_func->max_in_pos_ye)
										{
											result = ISP_MESSAGE_TILE_Y_DIR_NOT_END_TOGETHER_ERROR;
											tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
											break;
										}
									}
									else
									{
										result = ISP_MESSAGE_TILE_Y_DIR_NOT_END_TOGETHER_ERROR;
										tile_driver_printf("Error [%s] %s\r\n", ptr_func->func_name, tile_print_error_message(result));
										break;
									}
								}
							}
						}
					}
				}
			}
		}
	}
    return result;
}

