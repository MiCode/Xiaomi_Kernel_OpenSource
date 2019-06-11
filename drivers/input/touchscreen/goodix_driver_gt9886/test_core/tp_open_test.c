/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name          : tp_open_test.h
* Author             : Bob Huang
* Version            : V1.0.0
* Date               : 07/26/2017
* Description        : touch panel circuit open test
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#include <linux/string.h>
#include "tp_open_test.h"
#include "tp_dev_control.h"
#include "test_proc.h"
#include "simple_mem_manager.h"
#include "tp_additional_option.h"
#include "tp_test_general_error_code_def.h"

#ifdef __cplusplus
extern "C" {
#endif

static s32 clear_rawtest_res_buf(PST_TEST_ITEM p_test_item);

/*analyze rawdata test result*/
static s32 analyze_rawtest_res(u8 offse_and_accord_flag,
			PST_TEST_ITEM p_test_item);

/*---------------rawdata limit test------------*/
/*modify max and min avg */
static s32 modify_statistic_data(PST_TEST_ITEM p_test_item);
/*-----------------------------rawdata as data source end-------------------------------*/

/*-----------------------------diffdata as data source start-------------------------------*/

static s32 clear_difftest_res_buf(PST_TEST_ITEM p_test_item);

/*analyze diffdata test result*/
static s32 analyze_difftest_res(u8 cur_frame_id,
				PST_TEST_ITEM p_test_item);

/*modify statistic data of diffdata*/
static s32 modify_diff_statistic_data(PST_TEST_ITEM p_test_item);

/*-----------------------------diffdata as data source end-------------------------------*/

/*******************************************************************************
* Function Name	: rawdata_test_sets_func
* Description	: rawdata test func
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: none
* Return		: u8(0:test error 1:ok)
*******************************************************************************/
extern s32 rawdata_test_sets_func(IN_OUT ptr32 p_data)
{
	//You can leran more details from code gtp
	s32 ret = 0;
	u8 i = 0;
	u8 offse_and_accord_flag = 0;
	u16 err_code = RAWDATA_TEST_SUCCESSFULLY;
	u16 get_rawdata_timeout = 3000;
	u8 max_frame_total_num = TOTAL_FRAME_NUM_MAX;
	u8 cur_frame_total_num = 16;
	u8 add_frame_num = 16;
	u8 filter_frame_cnt = 0;
	u8 filter_frame_num = 1;
	u8 b_filter_data = 0;
	PST_TEST_ITEM p_test_item = (PST_TEST_ITEM) p_data;
	PST_RAWDATA_TEST_RES p_test_res = NULL;
	PST_RAWDATA_TEST_CALLBACK_SET p_spec_callback_set = NULL;
	PST_CUR_RAWDATA p_cur_data = NULL;
	PST_TEST_PROC_DATA p_test_proc_data = NULL;

	/*-----------------Judge parameter----------------**/
	if (0 == judge_test_item_legal((PST_TEST_ITEM) p_data)) {
		ret = 0;
		goto EXIT_RAWDATA_TEST;
	}
	/*(1)callback function test before*/
		if (NULL != p_test_item->ptr_test_item_before_func) {
		ret =
			p_test_item->ptr_test_item_before_func(p_test_item->
							ptr_test_item_before_func_param);
		if (ret == 0) {
			board_print_error
				("[open test]callback function test before error!\n");
			err_code = TEST_BEFORE_FUNC_FAIL;
			goto EXIT_RAWDATA_TEST;
		}
	}
	p_test_proc_data =
		(PST_TEST_PROC_DATA) p_test_item->ptr_test_proc_data;
	p_test_res =
		(PST_RAWDATA_TEST_RES) p_test_item->test_res.ptr_res;
	/*p_test_res->item_res_header.test_item_err_code.ptr_err_code_set[0] = RAWDATA_TEST_SUCCESSFULLY;
	//p_test_res->item_res_header.test_result = TEST_NG;
	//p_test_res->item_res_header.test_status = TEST_START;*/

	/*(2)clear rawdate result buf*/
	clear_rawtest_res_buf(p_test_item);

	/*(3)sending command and then get raw data.*/
	ret =
		chip_mode_select(p_test_item->p_tp_dev, CHIP_RAW_DATA_MODE);
	if (0 == ret) {
		board_print_error("enter raw data mode error!\n");
		p_test_res->item_res_header.test_item_err_code.
			ptr_err_code_set[0] = RAWDATA_TEST_ENTER_MODE_FAIL;
		p_test_res->item_res_header.test_status = TEST_ABORTED;
		err_code = RAWDATA_TEST_ENTER_MODE_FAIL;
		goto RAWDATA_TEST_SETS_FUNC_END;
	}
	p_cur_data =
		(PST_CUR_RAWDATA) p_test_item->cur_data.ptr_cur_data;
	p_cur_data->cur_frame_id = 0;
	while (1) {
		if (p_test_proc_data->test_break_flag) {
			p_test_res->item_res_header.test_item_err_code.
				ptr_err_code_set[0] =
				RAWDATA_TEST_BREAK_OUT;
			p_test_res->item_res_header.test_status =
				TEST_ABORTED;
			err_code = RAWDATA_TEST_BREAK_OUT;
			board_print_error("rawdata test break out!\n");
			goto RAWDATA_TEST_SETS_FUNC_END;
		}
		/*get rawdata*/
		b_filter_data = 0;
		if (filter_frame_cnt < filter_frame_num) {
			b_filter_data = 1;
		}
		ret =
			board_get_rawdata(p_test_item->p_tp_dev,
					get_rawdata_timeout,
					b_filter_data, p_cur_data);
		if (0 == ret) {
			board_print_error("get rawdata error!\n");
			p_test_res->item_res_header.test_item_err_code.
				ptr_err_code_set[0] =
				RAWDATA_TEST_GET_DATA_TIME_OUT;
			p_test_res->item_res_header.test_status =
				TEST_ABORTED;
			err_code = RAWDATA_TEST_GET_DATA_TIME_OUT;
			goto RAWDATA_TEST_SETS_FUNC_END;
		}
		if (filter_frame_cnt < filter_frame_num) {
			filter_frame_cnt++;
			continue;
		}
		/*modify max & min & avg data*/
		modify_statistic_data(p_test_item);

		/*sub test items*/
		for (i = 0; i < p_test_item->p_sub_id_set->len; i++) {
			switch (p_test_item->p_sub_id_set->p_id_set[i]) {
			case TP_RAWDATA_MAX_TEST_ITEM_ID:
				rawdata_max_test_func(p_data);
				break;
			case TP_RAWDATA_MIN_TEST_ITEM_ID:
				rawdata_min_test_func(p_data);
				break;
			case TP_KEYDATA_MAX_TEST_ITEM_ID:
				keydata_max_test_func(p_data);
				break;
			case TP_KEYDATA_MIN_TEST_ITEM_ID:
				keydata_min_test_func(p_data);
				break;
			case TP_ACCORD_TEST_ITEM_ID:
				rawdata_accord_test_func(p_data);
				offse_and_accord_flag |= 0x01;
				break;
			case TP_OFFSET_TEST_ITEM_ID:
				rawdata_offset_test_func(p_data);
				offse_and_accord_flag |= 0x02;
				break;
			case TP_UNIFORMITY_TEST_ITEM_ID:
				rawdata_uniform_test_func(p_data);
				break;
			case TP_RAWDATA_JITTER_TEST_ITEM_ID:
				rawdata_jitter_test_func(p_data);
				break;
			default:
				break;
			}
		}

		p_spec_callback_set =
			(PST_RAWDATA_TEST_CALLBACK_SET) p_test_item->
			ptr_special_callback_data;
		if (NULL != p_spec_callback_set) {
			/*save current data*/
			ret =
				p_spec_callback_set->
				ptr_save_cur_data_func(p_spec_callback_set->
						ptr_save_cur_data_func_param);
			if (ret == 0) {
				p_test_res->item_res_header.
					test_item_err_code.
					ptr_err_code_set[0] =
					SAVE_CUR_DATA_ERR;
				err_code = SAVE_CUR_DATA_ERR;
				board_print_error
					("save current rawdata error!\n");
			}
		}
		/*count frame id*/
		p_cur_data->cur_frame_id++;

		p_test_res->total_frame_cnt = p_cur_data->cur_frame_id;

		/*(4)analysis result*/
		if ((cur_frame_total_num <= max_frame_total_num)
			&& (0 ==
			p_cur_data->cur_frame_id % add_frame_num)) {
			ret =
				analyze_rawtest_res(offse_and_accord_flag,
						p_test_item);
			if (ret == 0 || ret == 1) {
				break;
			} else if (ret == 2) {
				cur_frame_total_num += add_frame_num;
				p_test_res->open_test_result |=
					FIRST_TEST_FLAG;
			}
		}
		if (p_cur_data->cur_frame_id >= max_frame_total_num) {
			break;
		}

	}

	/*(5)clear flag*/
	p_test_res->open_test_result &= (~FIRST_TEST_FLAG);

	/*(6)modify test status*/
	p_test_res->item_res_header.test_status = TEST_FINISH;
	if (p_test_res->open_test_result == 0) {
		p_test_res->item_res_header.test_result = TEST_OK;
	} else {
		p_test_res->item_res_header.test_item_err_code.
			ptr_err_code_set[0] = RAWDATA_TEST_FAIL;
		p_test_res->item_res_header.test_result = TEST_NG;
	}
RAWDATA_TEST_SETS_FUNC_END:

	/*(7)enter coord mode*/
	ret = chip_mode_select(p_test_item->p_tp_dev, CHIP_COORD_MODE);
	if (ret == 0) {
		board_print_error("enter coordinate mode error!\n");
		p_test_res->item_res_header.test_item_err_code.
			ptr_err_code_set[0] = ENTER_COORD_MODE_ERR;
		err_code = ENTER_COORD_MODE_ERR;
	}
EXIT_RAWDATA_TEST:
	/*(8)callback function after test finished*/
	if (NULL != p_test_item->ptr_test_item_finished_func) {
		ret =
			p_test_item->
			ptr_test_item_finished_func(p_test_item->
						ptr_test_item_finished_func_param);
		if (ret == 0) {
			board_print_error
				("[open test]callback function after test error!\n");
			err_code = TEST_FINISH_FUNC_FAIL;
		}
	}

	if (err_code == RAWDATA_TEST_SUCCESSFULLY) {
		ret = 1;
	} else {
		ret = 0;
	}
	return ret;
}

/*******************************************************************************
* Function Name	: rawdata_max_test_func
* Description	: rawdata max limit test function
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: none
* Return		: s32(0:test ng 1:test ok)
*******************************************************************************/
extern s32 rawdata_max_test_func(IN_OUT ptr32 p_data)
{
	s32 ret = 1;
	s32 index = 0;
	PST_RAWDATA_TEST_RES p_raw_test_res = NULL;

	PST_TEST_ITEM p_test_item = (PST_TEST_ITEM) p_data;
	p_raw_test_res =
		(PST_RAWDATA_TEST_RES) p_test_item->test_res.ptr_res;

	/*judge test item result*/
	if (((p_raw_test_res->open_test_result & FIRST_TEST_FLAG) != 0)
		&&
		((p_raw_test_res->
		open_test_result & BEYOND_RAWDATA_UPPER_LIMIT) == 0)) {
		return ret;
	}

	index = get_arr_id_from_sub_item(p_test_item,
					TP_RAWDATA_MAX_TEST_ITEM_ID);
	if (0xFF != index) {
		u16 id = 0;
		u16 node_num = 0;
		s32 data_tmp = 0;
		s32 para_tmp = 0;
		PST_TP_DEV p_dev = p_test_item->p_tp_dev;
		node_num = p_dev->sc_sen_num * p_dev->sc_drv_num;

		/*upper limit*/
		for (id = 0; id < node_num; id++) {
			if (get_chn_need_check(p_test_item, id)) {
				data_tmp =
					get_chn_rawdata(p_test_item, id);
				para_tmp =
					get_chn_upper_limit(p_test_item,
							id);
				if (data_tmp > para_tmp) {
					/*mark this channel beyond upper limit count*/
					p_raw_test_res->
						p_beyond_rawdata_upper_limit_cnt
						[id]++;
					p_raw_test_res->
						open_test_result |=
						BEYOND_RAWDATA_UPPER_LIMIT;
					p_raw_test_res->item_res_header.
						test_item_err_code.
						ptr_err_code_set[index +
								1] =
						RAWDATA_TEST_BEYOND_MAX_LIMIT;
					ret = 0;
				}
			}
		}
	}

	return ret;
}

/*******************************************************************************
* Function Name	: rawdata_min_test_func
* Description	: rawdata min limit test function
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: none
* Return		: s32(0:test ng 1:test ok)
*******************************************************************************/
extern s32 rawdata_min_test_func(IN_OUT ptr32 p_data)
{
	s32 ret = 1;
	s32 index = 0;
	PST_RAWDATA_TEST_RES p_raw_test_res = NULL;

	PST_TEST_ITEM p_test_item = (PST_TEST_ITEM) p_data;
	p_raw_test_res =
		(PST_RAWDATA_TEST_RES) p_test_item->test_res.ptr_res;

	/*judge test item result*/
	if (((p_raw_test_res->open_test_result & FIRST_TEST_FLAG) != 0)
		&&
		((p_raw_test_res->
			open_test_result & BEYOND_RAWDATA_LOWER_LIMIT) == 0)) {
		return ret;
	}

	index = get_arr_id_from_sub_item(p_test_item,
				TP_RAWDATA_MIN_TEST_ITEM_ID);
	if (0xFF != index) {
		u16 id = 0;
		u16 node_num = 0;
		s32 data_tmp = 0;
		s32 para_tmp = 0;
		PST_TP_DEV p_dev = p_test_item->p_tp_dev;
		node_num = p_dev->sc_sen_num * p_dev->sc_drv_num;

		/*lower limit*/
		for (id = 0; id < node_num; id++) {
			if (get_chn_need_check(p_test_item, id)) {
				data_tmp = get_chn_rawdata(p_test_item, id);
				para_tmp = get_chn_lower_limit(p_test_item, id);
				if (data_tmp < para_tmp) {
					/*mark this channel beyond lower limit count*/
					p_raw_test_res->
						p_beyond_rawdata_lower_limit_cnt
						[id]++;
					p_raw_test_res->
						open_test_result |=
						BEYOND_RAWDATA_LOWER_LIMIT;
					p_raw_test_res->item_res_header.
						test_item_err_code.
						ptr_err_code_set[index + 1] = BEYOND_RAWDATA_LOWER_LIMIT;
					ret = 0;
				}
			}
		}
	}

	return ret;
}

/*******************************************************************************
* Function Name	: rawdata_accord_test_func
* Description	: accord test function
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: none
* Return		: s32(0:test ng 1:test ok)
*******************************************************************************/
extern s32 rawdata_accord_test_func(IN_OUT ptr32 p_data)
{
	s32 ret = 1;
	s32 index = 0;
	PST_RAWDATA_TEST_RES p_raw_test_res = NULL;
	PST_TEST_ITEM p_test_item = (PST_TEST_ITEM) p_data;
	p_raw_test_res =
		(PST_RAWDATA_TEST_RES) p_test_item->test_res.ptr_res;
	if ((p_raw_test_res->open_test_result & FIRST_TEST_FLAG) != 0
		&& (p_raw_test_res->
		open_test_result & BEYOND_ACCORD_LIMIT) == 0) {
		return ret;
	}

	index = get_arr_id_from_sub_item(p_test_item, TP_ACCORD_TEST_ITEM_ID);
	if (0xFF != index) {
		u16 id = 0;
		u16 node_num = 0;
		s32 data_tmp = 0;
		s32 para_tmp = 0;
		PST_TP_DEV p_dev = p_test_item->p_tp_dev;
		node_num = p_dev->sc_sen_num * p_dev->sc_drv_num;

		for (id = 0; id < node_num; id++) {
			if (get_chn_need_check(p_test_item, id)) {
				data_tmp = get_chn_accord(p_test_item, id);
				/*/upper limit*/
				para_tmp = get_chn_accord_limit(p_test_item, id);
				if (data_tmp > para_tmp) {
					/*mark this channel beyond accord limit count*/
					p_raw_test_res->
						p_beyond_accord_limit_cnt
						[id]++;
					p_raw_test_res->
						open_test_result |=
						BEYOND_ACCORD_LIMIT;
					p_raw_test_res->item_res_header.
						test_item_err_code.
						ptr_err_code_set[index + 1] =
						RAWDATA_TEST_BEYOND_ACCORD_LIMIT;
					ret = 0;
				}
			}
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: keydata_max_test_func
* Description	: keydata max limit test function
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: none
* Return		: s32(0:test ng 1:test ok)
*******************************************************************************/
extern s32 keydata_max_test_func(IN_OUT ptr32 p_data)
{
	s32 ret = 1;
	s32 index = 0;
	PST_RAWDATA_TEST_RES p_raw_test_res = NULL;

	PST_TEST_ITEM p_test_item = (PST_TEST_ITEM) p_data;
	p_raw_test_res =
		(PST_RAWDATA_TEST_RES) p_test_item->test_res.ptr_res;

	/*judge test item result*/
	if (((p_raw_test_res->open_test_result & FIRST_TEST_FLAG) != 0)
		&&
		((p_raw_test_res->
		open_test_result & KEY_BEYOND_UPPER_LIMIT) == 0)) {
		return ret;
	}

	index = get_arr_id_from_sub_item(p_test_item,
					TP_KEYDATA_MAX_TEST_ITEM_ID);
	if (0xFF != index) {
		u8 id = 0;
		u8 key_num = 0;
		s32 data_tmp = 0;
		s32 para_tmp = 0;
		PST_TP_DEV p_dev = p_test_item->p_tp_dev;
		key_num = p_dev->key_num;

		/*upper limit*/
		for (id = 0; id < key_num; id++) {
			if (get_key_need_check(p_test_item, id)) {
				data_tmp = get_keydata(p_test_item, id);
				para_tmp = get_key_upper_limit(p_test_item, id);
				if (data_tmp > para_tmp) {
					/*mark this channel beyond upper limit count*/
					p_raw_test_res->
						p_beyond_key_upper_limit_cnt
						[id]++;
					p_raw_test_res->
						open_test_result |=
						KEY_BEYOND_UPPER_LIMIT;
					p_raw_test_res->item_res_header.
						test_item_err_code.
						ptr_err_code_set[index + 1] =
						RAWDATA_TEST_BEYOND_KEY_MAX_LIMIT;
					ret = 0;
				}
			}
		}
	}

	return ret;
}

/*******************************************************************************
* Function Name	: keydata_min_test_func
* Description	: keydata min limit test function
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: none
* Return		: s32(0:test ng 1:test ok)
*******************************************************************************/
extern s32 keydata_min_test_func(IN_OUT ptr32 p_data)
{
	s32 ret = 1;
	s32 index = 0;
	PST_RAWDATA_TEST_RES p_raw_test_res = NULL;

	PST_TEST_ITEM p_test_item = (PST_TEST_ITEM) p_data;
	p_raw_test_res =
		(PST_RAWDATA_TEST_RES) p_test_item->test_res.ptr_res;

	/*judge test item result*/
	if (((p_raw_test_res->open_test_result & FIRST_TEST_FLAG) != 0)
		&&
		((p_raw_test_res->
		open_test_result & KEY_BEYOND_LOWER_LIMIT) == 0)) {
		return ret;
	}

	index = get_arr_id_from_sub_item(p_test_item,
				TP_KEYDATA_MIN_TEST_ITEM_ID);
	if (0xFF != index) {
		u8 id = 0;
		u8 key_num = 0;
		s32 data_tmp = 0;
		s32 para_tmp = 0;
		PST_TP_DEV p_dev = p_test_item->p_tp_dev;
		key_num = p_dev->key_num;
		/*PST_SUB_TEST_ITEM_ID_SET p_sub_item_id_set = NULL;
		//p_sub_item_id_set = (PST_SUB_TEST_ITEM_ID_SET)p_test_item->p_sub_id_set;*/

		/*lower limit*/
		for (id = 0; id < key_num; id++) {
			if (get_key_need_check(p_test_item, id)) {
				data_tmp = get_keydata(p_test_item, id);
				para_tmp = get_key_lower_limit(p_test_item, id);
				if (data_tmp < para_tmp) {
					/*mark this channel beyond lower limit count*/
					p_raw_test_res->
						p_beyond_key_lower_limit_cnt
						[id]++;
					p_raw_test_res->
						open_test_result |=
						KEY_BEYOND_LOWER_LIMIT;
					p_raw_test_res->item_res_header.
						test_item_err_code.
						ptr_err_code_set[index + 1] =
						RAWDATA_TEST_BEYOND_KEY_MIN_LIMIT;
					ret = 0;
				}
			}
		}
	}

	return ret;
}

/*******************************************************************************
* Function Name	: rawdata_jitter_test_func
* Description	: rawdata jitter test function
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
*				: u8 cur_frame_id
* Output		: none
* Return		: s32(0:test ng 1:test ok)
*******************************************************************************/
extern s32 rawdata_jitter_test_func(IN_OUT ptr32 p_data)
{
	s32 ret = 1;
	s32 index = 0;
	PST_RAWDATA_TEST_RES p_raw_test_res = NULL;
	PST_TEST_ITEM p_test_item = (PST_TEST_ITEM) p_data;
	PST_CUR_RAWDATA p_cur_data =
		(PST_CUR_RAWDATA) p_test_item->cur_data.ptr_cur_data;
	p_raw_test_res =
		(PST_RAWDATA_TEST_RES) p_test_item->test_res.ptr_res;
	if ((p_cur_data->cur_frame_id + 1) != 16) {
		return 1;
	}

	index = get_arr_id_from_sub_item(p_test_item,
					TP_RAWDATA_JITTER_TEST_ITEM_ID);
	if (0xFF != index) {
		u16 id = 0;
		s32 data_tmp = 0;
		s32 para_tmp = 0;
		u16 node_num = 0;
		PST_TP_DEV p_dev = p_test_item->p_tp_dev;
		node_num = p_dev->sc_sen_num * p_dev->sc_drv_num;
		para_tmp = get_rawdata_jitter_limit(p_test_item);
		for (id = 0; id < node_num; id++) {
			if (get_chn_need_check(p_test_item, id)) {
				data_tmp =
					p_raw_test_res->p_max_raw_tmp_data->
					p_data_buf[id] -
					p_raw_test_res->p_min_raw_tmp_data->
					p_data_buf[id];
				if (data_tmp > para_tmp) {
					/*mark this beyond jitter limit count*/
					p_raw_test_res->
						p_beyond_jitter_limit_cnt
						[id] |= 0x80;
					p_raw_test_res->
						open_test_result |=
						BEYOND_JITTER_LIMIT;
					p_raw_test_res->item_res_header.
						test_item_err_code.
						ptr_err_code_set[index + 1] =
						RAWDATA_TEST_BEYOND_JITTER_LIMIT;
					ret = 0;
				}
			}

		}
	}

	return ret;
}

/*******************************************************************************
* Function Name	: rawdata_uniform_test_func
* Description	: rawdata uniform test function
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: none
* Return		: s32(0:test ng 1:test ok)
*******************************************************************************/
extern s32 rawdata_uniform_test_func(IN_OUT ptr32 p_data)
{
	s32 ret = 1;
	s32 index = 0;
	PST_RAWDATA_TEST_RES p_raw_test_res = NULL;
	PST_TEST_ITEM p_test_item = (PST_TEST_ITEM) p_data;
	p_raw_test_res =
		(PST_RAWDATA_TEST_RES) p_test_item->test_res.ptr_res;
	if ((p_raw_test_res->open_test_result & FIRST_TEST_FLAG) != 0
		&& (p_raw_test_res->
		open_test_result & BEYOND_UNIFORMITY_LIMIT) == 0) {
		return ret;
	}

	index = get_arr_id_from_sub_item(p_test_item,
					TP_UNIFORMITY_TEST_ITEM_ID);
	if (0xFF != index) {
		s32 data_tmp = 0;
		s32 para_tmp = 0;
		data_tmp = get_uniform_value(p_test_item);
		para_tmp = get_uniform_limit(p_test_item);

		if (data_tmp < para_tmp) {
			/*mark this channel beyond lower limit*/
			p_raw_test_res->beyond_uniform_limit_cnt++;
			p_raw_test_res->open_test_result |=
				BEYOND_UNIFORMITY_LIMIT;
			p_raw_test_res->item_res_header.
				test_item_err_code.ptr_err_code_set[index + 1] =
				RAWDATA_TEST_BEYOND_UNIFORM_LIMIT;
			ret = 0;
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: rawdata_offset_test_func
* Description	: rawdata offset test function
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: none
* Return		: s32(0:test ng 1:test ok)
*******************************************************************************/
extern s32 rawdata_offset_test_func(IN_OUT ptr32 p_data)
{
	s32 ret = 1;
	s32 index = 0;
	PST_RAWDATA_TEST_RES p_raw_test_res = NULL;
	PST_TEST_ITEM p_test_item = (PST_TEST_ITEM) p_data;
	p_raw_test_res =
		(PST_RAWDATA_TEST_RES) p_test_item->test_res.ptr_res;
	if ((p_raw_test_res->open_test_result & FIRST_TEST_FLAG) != 0
		&& (p_raw_test_res->
		open_test_result & BEYOND_OFFSET_LIMIT) == 0) {
		return ret;
	}

	index = get_arr_id_from_sub_item(p_test_item,
				TP_OFFSET_TEST_ITEM_ID);
	if (0xFF != index) {
		u16 id = 0;
		u16 node_num = 0;
		s32 data_tmp = 0;
		s32 para_tmp = 0;
		/*PST_SUB_TEST_ITEM_ID_SET p_sub_item_id_set = NULL;*/
		PST_TP_DEV p_dev = p_test_item->p_tp_dev;
		/*p_sub_item_id_set = (PST_SUB_TEST_ITEM_ID_SET)p_test_item->p_sub_id_set;*/
		node_num = p_dev->sc_sen_num * p_dev->sc_drv_num;

		/*(1)sending command and then get rawdata.*/
		for (id = 0; id < node_num; id++) {
			data_tmp = get_chn_offset(p_test_item, id);
			if (get_chn_need_check(p_test_item, id)) {
				/*upper limit*/
				para_tmp =
					get_chn_offset_limit(p_test_item, id);
				if (data_tmp > para_tmp) {
					/*//mark this channel beyond offset limit count*/
					p_raw_test_res->
						p_beyond_offset_limit_cnt
						[id]++;
					p_raw_test_res->
						open_test_result |=
						BEYOND_OFFSET_LIMIT;
					p_raw_test_res->item_res_header.
						test_item_err_code.
						ptr_err_code_set[index + 1] =
						RAWDATA_TEST_BEYOND_OFFSET_LIMIT;
					ret = 0;
				}
			}
		}
	}

	return ret;
}

/*******************************************************************************
* Function Name	: diffdata_test_sets_func
* Description	: diffdata test sets function
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: none
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
extern s32 diffdata_test_sets_func(IN_OUT ptr32 p_data)
{
	/*//You can leran more details from code gtp*/
	s32 ret = 1;
	u8 i = 0;
	u16 get_diffdata_timeout = 3000;
	u8 max_frame_total_num = 64;
	u8 cur_frame_total_num = 16;
	u8 add_frame_num = 16;
	u8 filter_frame_cnt = 0;
	u8 filter_frame_num = 1;
	u8 b_filter_data = 0;
	PST_TEST_ITEM p_test_item = (PST_TEST_ITEM) p_data;
	PST_DIFFDATA_TEST_RES p_test_res = NULL;
	PST_DIFFDATA_TEST_CALLBACK_SET p_spec_callback_set = NULL;
	PST_CUR_DIFFDATA p_cur_data = NULL;
	PST_TEST_PROC_DATA p_test_proc_data = NULL;

	/*//-----------------Judge parameter----------------*/
	if (0 == judge_test_item_legal((PST_TEST_ITEM) p_data)) {
		ret = 0;
		goto DIFFDATA_TEST_SETS_FUNC_END;
	}

	p_test_proc_data =
		(PST_TEST_PROC_DATA) p_test_item->ptr_test_proc_data;
	p_test_res =
		(PST_DIFFDATA_TEST_RES) p_test_item->test_res.ptr_res;
	p_cur_data =
		(PST_CUR_DIFFDATA) p_test_item->cur_data.ptr_cur_data;

	/*//(1)callback function test before*/
	if (NULL != p_test_item->ptr_test_item_before_func) {
		ret &=
			p_test_item->ptr_test_item_before_func(p_test_item->
							ptr_test_item_before_func_param);
		if (ret == 0) {
			test_before_func_fail_proc(&p_test_res->
						item_res_header);
			goto DIFFDATA_TEST_SETS_FUNC_END;
		}
	}
	/*//clear result buffer*/
	clear_difftest_res_buf(p_test_item);

	/*//sending command and then get diff data.*/
	ret = chip_mode_select(p_test_item->p_tp_dev,
				CHIP_DIFF_DATA_MODE);
	if (0 == ret) {
		board_print_error("enter diff data mode error!\n");
		p_test_res->item_res_header.test_item_err_code.
			ptr_err_code_set[0] = DIFFDATA_TEST_ENTER_MODE_FAIL;
		p_test_res->item_res_header.test_result = TEST_NG;
		p_test_res->item_res_header.test_status = TEST_ABORTED;
		goto DIFFDATA_TEST_SETS_FUNC_END;
	}

	p_cur_data->cur_frame_id = 0;
	while (1) {
		if (p_test_proc_data->test_break_flag) {
			p_test_res->item_res_header.test_item_err_code.
				ptr_err_code_set[0] =
				DIFFDATA_TEST_BREAK_OUT;
			p_test_res->item_res_header.test_result =
				TEST_NG;
			p_test_res->item_res_header.test_status =
				TEST_ABORTED;
			goto DIFFDATA_TEST_SETS_FUNC_END;
		}
		/*//get diffdata*/
		b_filter_data = 0;
		if (filter_frame_cnt < filter_frame_num) {
			b_filter_data = 1;
		}
		ret = board_get_diffdata(p_test_item->p_tp_dev,
					get_diffdata_timeout,
					b_filter_data, p_cur_data);
		if (0 == ret) {
			board_print_error("get diffdata error!\n");
			p_test_res->item_res_header.test_item_err_code.
				ptr_err_code_set[0] =
				DIFFDATA_TEST_GET_DATA_TIME_OUT;
			p_test_res->item_res_header.test_result =
				TEST_NG;
			p_test_res->item_res_header.test_status =
				TEST_ABORTED;
			goto DIFFDATA_TEST_SETS_FUNC_END;
		}

		if (filter_frame_cnt < filter_frame_num) {
			filter_frame_cnt++;
			continue;
		}

		/*//statistic data modify*/
		ret = modify_diff_statistic_data(p_test_item);
		/*//sub test items*/
		for (i = 0; i < p_test_item->p_sub_id_set->len; i++) {
			switch (p_test_item->p_sub_id_set->p_id_set[i]) {
			case TP_DIFFDATA_JITTER_TEST_ITEM_ID:
				diffdata_jitter_test_func(p_data);
				break;
			default:
				break;
			}
		}

		p_spec_callback_set =
			(PST_DIFFDATA_TEST_CALLBACK_SET) p_test_item->
			ptr_special_callback_data;
		if (NULL != p_spec_callback_set) {
			/*//save current data*/
			p_spec_callback_set->
				ptr_save_cur_data_func(p_spec_callback_set->
						ptr_save_cur_data_func_param);
		}
		/*//count frame id*/
		p_cur_data->cur_frame_id++;
		p_test_res->total_frame_cnt = p_cur_data->cur_frame_id;
		/*//analysis result*/
		if ((cur_frame_total_num <= max_frame_total_num)
			&& (0 ==
			p_cur_data->cur_frame_id % add_frame_num)) {
			ret = analyze_difftest_res(p_cur_data->
						cur_frame_id,
						p_test_item);
			if (ret == 0) {
				break;
			} else if (ret == 1) {
				p_test_res->item_res_header.
					test_result = TEST_NG;
				p_test_res->item_res_header.
					test_status = TEST_ABORTED;
				break;
			} else if (ret == 2) {
				cur_frame_total_num += add_frame_num;
			}
		}
		if (p_cur_data->cur_frame_id >= max_frame_total_num) {
			break;
		}
	}

DIFFDATA_TEST_SETS_FUNC_END:

	/*//enter coord mode*/
	ret = chip_mode_select(p_test_item->p_tp_dev, CHIP_COORD_MODE);
	if (ret == 0) {
		board_print_error("enter coordinate mode error!\n");
		p_test_res->item_res_header.test_item_err_code.
			ptr_err_code_set[0] = ENTER_COORD_MODE_ERR;
		p_test_res->item_res_header.test_result = TEST_NG;
		p_test_res->item_res_header.test_status = TEST_ABORTED;
	}

	if (NULL != p_test_item->ptr_test_item_finished_func) {
		if (0 == p_test_item->
			ptr_test_item_finished_func(p_test_item->
						ptr_test_item_finished_func_param)) {
			board_print_error
				("[diffdata_test_sets_func]callback function after test error!\n");
			ret = 0;
		}
	}

	return ret;
}

/*******************************************************************************
* Function Name	: diffdata_jitter_test_func
* Description	: diffdata jitter test function
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: none
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
extern s32 diffdata_jitter_test_func(IN_OUT ptr32 p_data)
{
	/*//PST_TEST_ITEM p_test_item = (PST_TEST_ITEM)p_data;*/
	return 1;
}

/*******************************************************************************
* Function Name	: get rawdata
* Description	: get rawdata
* Input			: PST_TP_DEV p_dev(device )
* Input			: u16 time_out_ms
* Input			: u8 b_filter_data
* Output		: OUT PST_CUR_RAWDATA p_cur_data
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
extern s32 get_rawdata(IN PST_TP_DEV p_dev, IN u16 time_out_ms,
			IN u8 b_filter_data,
			OUT PST_CUR_RAWDATA p_cur_data)
{
	s32 ret = 0;
	u16 i = 0;
	u8 retry_cnt = 3;
	u8 data_size = 1;
	u16 wait_time = 0;
	s32 data_flag = 0;
	if (p_dev->rawdata_option & _DATA_BIT_8) {
		data_size = 1;
	} else if (p_dev->rawdata_option & _DATA_BIT_16) {
		data_size = 2;
	} else if (p_dev->rawdata_option & _DATA_BIT_32) {
		data_size = 4;
	} else if (p_dev->rawdata_option & _DATA_BIT_64) {
		data_size = 8;
	}

	while (wait_time < time_out_ms) {
		/*//Is there data ?*/
		data_flag = get_data_flag(p_dev);
		board_print_debug("get rawdata flag!");
		if (data_flag) {
			if (0 == b_filter_data) {
				for (i = 0; i < retry_cnt; i++) {
					ret = chip_reg_read(p_dev,
							p_dev->
							rawdata_addr,
							p_cur_data->
							p_data_buf,
							p_dev->
							rawdata_len *
							data_size);
					if (1 == ret) {
						break;
					}
				}
				if (0 == ret) {
					board_print_error
						("read rawdata error!\n");
					goto GET_RAWDATA_END;
				}

				if ((p_dev->
					rawdata_option &
					_DATA_LARGE_ENDIAN)
					|| (p_dev->rawdata_option &
					_DATA_DRV_SEN_INVERT)) {
					reshape_data(p_dev->rawdata_option,
					p_dev, p_cur_data);
				}
			}
			/*//clear data flag*/
			ret = clr_data_flag(p_dev);
			if (0 == ret) {
				board_print_error
					("clear dataflag error!\n");
				goto GET_RAWDATA_END;
			}
			goto GET_RAWDATA_END;
		}
		board_delay_ms(5);
		wait_time += 5;
	}
	if (data_flag == 0) {
		board_print_error("get_data_flag failed\n");
		return 0;
	}
GET_RAWDATA_END:

	return ret;
}

/*******************************************************************************
* Function Name	: get_diffdata
* Description	: get diffdata
* Input			: PST_TP_DEV p_tp_dev(device )
* Input			: u16 time_out_ms
* Input			: u8 b_filter_data
* Output		: PST_CUR_DIFFDATA p_cur_data
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
extern s32 get_diffdata(IN PST_TP_DEV p_dev, IN u16 time_out_ms,
			IN u8 b_filter_data,
			IN_OUT PST_CUR_DIFFDATA p_cur_data)
{
	s32 ret = 0;
	u8 i = 0;
	u8 retry_cnt = 3;
	u8 data_size = 0;
	u16 wait_time = 0;
	if (p_dev->diffdata_option & _DATA_BIT_8) {
		data_size = 1;
	} else if (p_dev->diffdata_option & _DATA_BIT_16) {
		data_size = 2;
	} else if (p_dev->diffdata_option & _DATA_BIT_32) {
		data_size = 4;
	} else if (p_dev->diffdata_option & _DATA_BIT_64) {
		data_size = 8;
	}

	while (wait_time < time_out_ms) {
		/*//Is there data ?*/
		if (get_data_flag(p_dev)) {
			if (0 == b_filter_data) {
				for (i = 0; i < retry_cnt; i++) {
					ret = chip_reg_read(p_dev,
							p_dev->
							diffdata_addr,
							(u8 *)
							p_cur_data->
							p_data_buf,
							p_dev->
							diffdata_len *
							data_size);
					if (1 == ret) {
						break;
					}
				}

				if (0 == ret) {
					board_print_error
						("read diffdata error!\n");
					goto GET_DIFFDATA_END;
				}
				if ((p_dev->
					diffdata_option &
					_DATA_LARGE_ENDIAN)
					|| (p_dev->diffdata_option &
					_DATA_DRV_SEN_INVERT)) {
					reshape_data(p_dev->diffdata_option,
					p_dev, p_cur_data);
				}
			}
			/*//clear data flag*/
			ret = clr_data_flag(p_dev);
			if (0 == ret) {
				board_print_error
					("clear dataflag error!\n");
				goto GET_DIFFDATA_END;
			}
			goto GET_DIFFDATA_END;
		}
		board_delay_ms(5);
		wait_time += 5;
	}
GET_DIFFDATA_END:

	return ret;
}

/*******************************************************************************
* Function Name	: get_data_size
* Description	: get byte count of data
* Input			: u8 data_option
* Output		:
* Return		: u8(byte cnt of data)
*******************************************************************************/
extern u8 get_data_size(u8 data_option)
{
	u8 data_size = 2;
	if (data_option & _DATA_BIT_8) {
		data_size = 1;
	} else if (data_option & _DATA_BIT_16) {
		data_size = 2;
	} else if (data_option & _DATA_BIT_32) {
		data_size = 4;
	} else if (data_option & _DATA_BIT_64) {
		data_size = 8;
	}
	return data_size;
}

/*******************************************************************************
* Function Name	: reshape_data
* Description	:
* Input			: u8 data_option
: PST_CUR_RAWDATA p_cur_data
: PST_TP_DEV p_dev
* Output		: PST_CUR_RAWDATA p_cur_data
* Return		: none
*******************************************************************************/
extern void reshape_data(IN u8 data_option, IN PST_TP_DEV p_dev,
			IN_OUT PST_CUR_RAWDATA p_cur_data)
{
	u8 data_size = 2;
	u8 data_tmp = 0;
	u16 i = 0;
	u8 j = 0;
	u8 *p_buf_tmp = NULL;
	u8 drv_id = 0;
	u8 sen_id = 0;
	u16 tmp_id = 0;
	u8 *p_buf_bak = NULL;

	if (data_option & _DATA_BIT_8) {
		data_size = 1;
	} else if (data_option & _DATA_BIT_16) {
		data_size = 2;
	} else if (data_option & _DATA_BIT_32) {
		data_size = 4;
	} else if (data_option & _DATA_BIT_64) {
		data_size = 8;
	}

	p_buf_tmp = p_cur_data->p_data_buf;
	/*//in arm or pc,data is little-endian in ram so we need change "large-endian" to "little-endian"*/
	if ((data_option & _DATA_LARGE_ENDIAN) && (data_size > 1)) {
		for (i = 0; i < p_dev->rawdata_len * data_size;
			i += data_size) {
			for (j = 0; j < data_size - j; j++) {
				data_tmp = p_buf_tmp[i + j];
				p_buf_tmp[i + j] =
					p_buf_tmp[i + data_size - j - 1];
				p_buf_tmp[i + data_size - j - 1] =
					data_tmp;
			}
		}
	}
	/*//storage data in driver direction must change in sensor direction in single-chip*/
	/*//drv sen invert*/
	if ((data_option & _DATA_DRV_SEN_INVERT)) {
		if (alloc_mem_block
			((void **)&p_buf_bak,
			p_dev->rawdata_len * data_size)) {
			memcpy(p_buf_bak, p_buf_tmp,
				p_dev->rawdata_len * data_size);
			for (i = 0;
				i < p_dev->sc_drv_num * p_dev->sc_sen_num;
				i++) {
				drv_id = i / p_dev->sc_sen_num;
				sen_id = i % p_dev->sc_sen_num;
				tmp_id =
					sen_id * p_dev->sc_drv_num + drv_id;
				for (j = 0; j < data_size; j++) {
					p_buf_tmp[tmp_id * data_size + j] =
						p_buf_bak[i * data_size +
							j];
				}
			}
			free_mem_block(p_buf_bak);
		}
	}
}

/*******************************************************************************
* Function Name	: get_chn_rawdata
* Description	: get specialized channel rawdata
* Input			: PST_TEST_ITEM p_test_item(item pointer)
: u16 chn_id(channel index)
* Output		: none
* Return		: s32
*******************************************************************************/
extern s32 get_chn_rawdata(IN PST_TEST_ITEM p_test_item, IN u16 chn_id)
{
	s32 ret = 0;
	PST_CUR_RAWDATA p_cur_rawdata =
		(PST_CUR_RAWDATA) (p_test_item->cur_data.ptr_cur_data);
	PST_TP_DEV p_dev = p_test_item->p_tp_dev;
	u16 len = p_dev->sc_sen_num * p_dev->sc_drv_num;

	/*//data is little-endian after reshaping*/
	if (len > chn_id) {
		if (p_dev->rawdata_option & _DATA_BIT_8) {
			ret = p_cur_rawdata->p_data_buf[chn_id];
		} else if (p_dev->rawdata_option & _DATA_BIT_16) {
			ret =
				(s16) ((p_cur_rawdata->
					p_data_buf[chn_id * 2]) +
					(p_cur_rawdata->
					p_data_buf[chn_id * 2 + 1] << 8));
		} else if (p_dev->rawdata_option & _DATA_BIT_32) {
			ret =
				(p_cur_rawdata->p_data_buf[chn_id * 4]) +
				(p_cur_rawdata->
				p_data_buf[chn_id * 4 + 1] << 8) +
				(p_cur_rawdata->
				p_data_buf[chn_id * 4 + 2] << 16) +
				(p_cur_rawdata->
				p_data_buf[chn_id * 4 + 3] << 24);
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: get_chn_need_check
* Description	: judge specialized channel whether need check or not.we use bit
to save memory.
* Input			: PST_TEST_ITEM p_test_item(item pointer)
: u16 chn_id(channel index)
* Output		: none
* Return		: s32(0:not need check 1:need check)
*******************************************************************************/
extern s32 get_chn_need_check(IN PST_TEST_ITEM p_test_item,
				IN u16 chn_id)
{
	s32 ret = 0;
	PST_TP_DEV p_dev = NULL;
	PST_RAWDATA_TEST_PARAM p_rawdata_test_param = NULL;
	if (NULL == p_test_item) {
		return 1;
	}
	p_dev = p_test_item->p_tp_dev;
	p_rawdata_test_param =
		(PST_RAWDATA_TEST_PARAM) (p_test_item->param.ptr_param);
	if (chn_id < p_dev->sc_sen_num * p_dev->sc_drv_num) {
		if (p_rawdata_test_param->p_need_check_node->
			p_data_buf[chn_id] == TP_FLAG_NEED_CHECK) {
			ret = 1;
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: get_chn_upper_limit
* Description	: get node upper limit
* Input			: PST_TEST_ITEM p_test_item(item pointer)
: u16 chn_id(channel index)
* Output		: none
* Return		: s32
*******************************************************************************/
extern s32 get_chn_upper_limit(IN PST_TEST_ITEM p_test_item,
				IN u16 chn_id)
{
	s32 ret = 0;
	u16 i = 0;
	PST_RAWDATA_TEST_PARAM p_rawdata_test_param =
		(PST_RAWDATA_TEST_PARAM) (p_test_item->param.ptr_param);
	PST_RAWDATA_SPECIAL_NODE_PARAM p_spec_node =
		p_rawdata_test_param->p_special_node;

	ret = p_rawdata_test_param->p_rawdata_limit->upper_limit;
	for (i = 0; i < p_spec_node->data_len; i++) {
		if (chn_id == p_spec_node->p_data_buf[i].node_id) {
			ret = p_spec_node->p_data_buf[i].high_raw_limit;
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: get_chn_lower_limit
* Description	: get node lower limit
* Input			: PST_TEST_ITEM p_test_item(item pointer)
: u16 chn_id(channel index)
* Output		: none
* Return		: s32
*******************************************************************************/
extern s32 get_chn_lower_limit(IN PST_TEST_ITEM p_test_item,
				IN u16 chn_id)
{
	s32 ret = 0;
	u16 i = 0;
	PST_RAWDATA_TEST_PARAM p_rawdata_test_param =
		(PST_RAWDATA_TEST_PARAM) (p_test_item->param.ptr_param);
	PST_RAWDATA_SPECIAL_NODE_PARAM p_spec_node =
		p_rawdata_test_param->p_special_node;
	ret = p_rawdata_test_param->p_rawdata_limit->lower_limit;
	for (i = 0; i < p_spec_node->data_len; i++) {
		if (chn_id == p_spec_node->p_data_buf[i].node_id) {
			ret = p_spec_node->p_data_buf[i].low_raw_limit;
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: get_chn_accord
* Description	: get chn accord(*1000)
* Input			: PST_TEST_ITEM p_test_item(item pointer)
: u16 chn_id(channel index)
* Output		: none
* Return		: s32
*******************************************************************************/
extern s32 get_chn_accord(IN PST_TEST_ITEM p_test_item, IN u16 chn_id)
{
	s32 ret = 0;
	u16 accord_opt = DEF_ACCORD_CALC;
	s32 tmp_data = 0x7FFFFFFF;
	s32 cur_data = 0;
	u8 i = 0;
	s16 pos_arr[4] = { -1, -1, -1, -1 };
	PST_TP_DEV p_dev = p_test_item->p_tp_dev;

	tmp_data = get_adjacent_node(chn_id, p_dev, pos_arr);
	accord_opt = get_chn_accord_option(p_test_item, chn_id);
	for (i = 0; i < 4; i++) {
		/*//upon,down,left,right*/
		if (0 == (accord_opt & (1 << i))) {
			pos_arr[i] = -1;
		}
	}
	cur_data = get_chn_rawdata(p_test_item, chn_id);
	for (i = 0; i < 4; i++) {
		if (-1 == pos_arr[i]) {
			continue;
		}
		tmp_data =
			get_chn_rawdata(p_test_item, (u16) pos_arr[i]);
		tmp_data =
			(tmp_data - cur_data) >
			0 ? (tmp_data - cur_data) : (cur_data - tmp_data);
		tmp_data = (s32) (1000 * tmp_data / cur_data);
		if (tmp_data > ret) {
			ret = tmp_data;
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: get_chn_accord_limit
* Description	: get chn accord limit(*1000)
* Input			: PST_TEST_ITEM p_test_item(item pointer)
: u16 chn_id(channel index)
* Output		: none
* Return		: s32
*******************************************************************************/
extern s32 get_chn_accord_limit(IN PST_TEST_ITEM p_test_item,
				IN u16 chn_id)
{
	s32 ret = 0;
	u16 i = 0;
	PST_RAWDATA_TEST_PARAM p_rawdata_test_param =
		(PST_RAWDATA_TEST_PARAM) (p_test_item->param.ptr_param);
	PST_RAWDATA_SPECIAL_NODE_PARAM p_spec_node =
		p_rawdata_test_param->p_special_node;
	ret = p_rawdata_test_param->p_raw_accord_limit->accord_limit;
	for (i = 0; i < p_spec_node->data_len; i++) {
		if (chn_id == p_spec_node->p_data_buf[i].node_id) {
			ret = p_spec_node->p_data_buf[i].accord_limit;
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: get_chn_accord_option
* Description	: get_chn_accord_option
* Input			: PST_TEST_ITEM p_test_item(item pointer)
: u16 chn_id(channel index)
* Output		: none
* Return		: u16
*******************************************************************************/
extern u16 get_chn_accord_option(PST_TEST_ITEM p_test_item, u16 chn_id)
{
	u16 ret = DEF_ACCORD_CALC;
	u16 i = 0;
	PST_RAWDATA_TEST_PARAM p_rawdata_test_param =
		(PST_RAWDATA_TEST_PARAM) (p_test_item->param.ptr_param);
	PST_RAWDATA_SPECIAL_NODE_PARAM p_spec_node =
		p_rawdata_test_param->p_special_node;

	ret = p_rawdata_test_param->p_raw_accord_limit->accord_option;
	for (i = 0; i < p_spec_node->data_len; i++) {
		if (chn_id == p_spec_node->p_data_buf[i].node_id) {
			ret = p_spec_node->p_data_buf[i].accord_option;
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: get_adjacent_node
* Description	: get adjacent node
* Input			: u16 node_id
: PST_TP_DEV p_dev
* Output		: s16* p_arr
* Return		: s32(adjacent node num)
*******************************************************************************/
extern u8 get_adjacent_node(IN u16 node_id, IN PST_TP_DEV p_dev,
				OUT s16 *p_arr)
{
	u8 ret = 4;
	u16 drv_id = 0;	/*//col_id*/
	u16 sen_id = 0;	/*//row_id*/

	drv_id = node_id % p_dev->sc_drv_num;
	sen_id = node_id / p_dev->sc_drv_num;

	p_arr[ADJ_POS_UP] = (sen_id - 1) * (p_dev->sc_drv_num) + drv_id;
	p_arr[ADJ_POS_DOWN] =
		(sen_id + 1) * (p_dev->sc_drv_num) + drv_id;
	p_arr[ADJ_POS_LEFT] =
		(sen_id) * (p_dev->sc_drv_num) + drv_id - 1;
	p_arr[ADJ_POS_RIGHT] =
		(sen_id) * (p_dev->sc_drv_num) + drv_id + 1;

	if (sen_id == 0) {
		p_arr[ADJ_POS_UP] = -1;
		ret--;
	} else if (sen_id == p_dev->sc_sen_num - 1) {
		p_arr[ADJ_POS_DOWN] = -1;
		ret--;
	}

	if (drv_id == 0) {
		p_arr[ADJ_POS_LEFT] = -1;
		ret--;
	} else if (drv_id == p_dev->sc_drv_num - 1) {
		p_arr[ADJ_POS_RIGHT] = -1;
		ret--;
	}
	return ret;
}

/*******************************************************************************
* Function Name	: get_chn_offset
* Description	: get chn offset(*1000)
* Input			: PST_TEST_ITEM p_test_item(item pointer)
: u16 chn_id(channel index)
* Return		: s32
*******************************************************************************/
extern s32 get_chn_offset(IN PST_TEST_ITEM p_test_item, IN u16 chn_id)
{
	s32 ret = 0;
	PST_RAWDATA_TEST_RES p_raw_test_res =
		(PST_RAWDATA_TEST_RES) p_test_item->test_res.ptr_res;

	PST_TP_DEV p_dev = p_test_item->p_tp_dev;
	u16 len = p_dev->sc_sen_num * p_dev->sc_drv_num;
	u32 tmp_data = 0;
	u32 tmp_raw = 0;
	u32 avg = 0;
	if (len > chn_id) {
		tmp_raw = get_chn_rawdata(p_test_item, chn_id);
		avg = p_raw_test_res->raw_avg;
		tmp_data =
			(tmp_raw - avg) >
			0 ? (tmp_raw - avg) : (avg - tmp_raw);
		ret = (s32) (1000 * tmp_data / p_raw_test_res->raw_avg);
	}
	return ret;
}

/*******************************************************************************
* Function Name	: get_chn_offset_limit
* Description	: get channel offset limit(*1000)
* Input			: PST_TEST_ITEM p_test_item(item pointer)
: u16 chn_id(channel index)
* Return		: s32
*******************************************************************************/
extern s32 get_chn_offset_limit(IN PST_TEST_ITEM p_test_item,
				IN u16 chn_id)
{
	s32 ret = 0;
	u16 i = 0;
	PST_RAWDATA_TEST_PARAM p_rawdata_test_param =
		(PST_RAWDATA_TEST_PARAM) (p_test_item->param.ptr_param);
	PST_RAWDATA_SPECIAL_NODE_PARAM p_spec_node =
		p_rawdata_test_param->p_special_node;
	ret = p_rawdata_test_param->p_raw_offset_limit->offset_limit;
	for (i = 0; i < p_spec_node->data_len; i++) {
		if (chn_id == p_spec_node->p_data_buf[i].node_id) {
			ret = p_spec_node->p_data_buf[i].offset_limit;
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: get_keydata
* Description	: get keydata
* Input			: PST_TEST_ITEM p_test_item(item pointer)
: u8 key_id(key id)
* Return		: s32
*******************************************************************************/
extern s32 get_keydata(IN PST_TEST_ITEM p_test_item, IN u8 key_id)
{
	s32 ret = 0;
	u16 tmp_id = 0;
	PST_TP_DEV p_dev = p_test_item->p_tp_dev;
	PST_CUR_RAWDATA p_cur_rawdata =
		(PST_CUR_RAWDATA) p_test_item->cur_data.ptr_cur_data;

	if (key_id < p_dev->key_num) {
		tmp_id = p_dev->sc_sen_num * p_dev->sc_drv_num +
			p_dev->key_pos_arr[key_id] - 1;

		if (p_dev->rawdata_option & _DATA_BIT_8) {
			ret = p_cur_rawdata->p_data_buf[tmp_id];
		} else if (p_dev->rawdata_option & _DATA_BIT_16) {
			ret = (s16) ((p_cur_rawdata->
					p_data_buf[tmp_id * 2]) +
					(p_cur_rawdata->
					p_data_buf[tmp_id * 2 + 1] << 8));
		} else if (p_dev->rawdata_option & _DATA_BIT_32) {
			ret =
				(p_cur_rawdata->p_data_buf[tmp_id * 4]) +
				(p_cur_rawdata->
				p_data_buf[tmp_id * 4 + 1] << 8) +
				(p_cur_rawdata->
				p_data_buf[tmp_id * 4 + 2] << 16) +
				(p_cur_rawdata->
				p_data_buf[tmp_id * 4 + 3] << 24);
		}
	}
	return ret;
}
/*******************************************************************************
* Function Name	: get_key_need_check
* Description	: judge specialized key whether need check or not
* Input			: PST_TEST_ITEM p_test_item(item pointer)
: u16 key_id(key id)
* Return		: s32(0:not need check 1:need check)
*******************************************************************************/
extern s32 get_key_need_check(IN PST_TEST_ITEM p_test_item,
			IN u16 key_id)
{
	s32 ret = 0;
	u16 key_pos = 0;

	PST_RAWDATA_TEST_PARAM p_rawdata_test_param =
		(PST_RAWDATA_TEST_PARAM) (p_test_item->param.ptr_param);
	PST_TP_DEV p_dev = p_test_item->p_tp_dev;

	if (key_id < p_dev->key_num) {
		key_pos = p_dev->key_pos_arr[key_id] - 1;
		if (p_rawdata_test_param->p_raw_key_limit->
			p_need_check_key->p_data_buf[key_pos] ==
		TP_FLAG_KEY_NEED_CHECK) {
			ret = 1;
		}
	}
return ret;
}

/*******************************************************************************
* Function Name	: get_key_upper_limit
* Description	: get key upper limit
* Input			: PST_TEST_ITEM p_test_item(item pointer)
: u8 key_id(key id)
* Return		: s32
*******************************************************************************/
extern s32 get_key_upper_limit(IN PST_TEST_ITEM p_test_item,
				IN u8 key_id)
{
	s32 ret = 0;
	PST_RAWDATA_TEST_PARAM p_rawdata_test_param =
		(PST_RAWDATA_TEST_PARAM) (p_test_item->param.ptr_param);
	u16 i = 0;
	u16 node_id;
	PST_TP_DEV p_tp_dev = p_test_item->p_tp_dev;
	PST_RAWDATA_SPECIAL_NODE_PARAM p_spec_node =
		p_rawdata_test_param->p_special_node;
	ret = p_rawdata_test_param->p_raw_key_limit->upper_limit;
	for (i = 0; i < p_spec_node->data_len; i++) {
		node_id = p_spec_node->p_data_buf[i].node_id;
		if ((node_id >=
			p_tp_dev->sc_drv_num * p_tp_dev->sc_sen_num)
			&& node_id -
			p_tp_dev->sc_drv_num * p_tp_dev->sc_sen_num ==
			key_id) {
			ret = p_spec_node->p_data_buf[i].high_raw_limit;
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: get_key_lower_limit
* Description	: get key lower limit
* Input			: PST_TEST_ITEM p_test_item(item pointer)
: u8 key_id(key id)
* Return		: s32
*******************************************************************************/
extern s32 get_key_lower_limit(IN PST_TEST_ITEM p_test_item,
				IN u8 key_id)
{
	s16 ret = 0;
	PST_RAWDATA_TEST_PARAM p_rawdata_test_param =
		(PST_RAWDATA_TEST_PARAM) (p_test_item->param.ptr_param);
	u16 i = 0;
	u16 node_id;
	PST_TP_DEV p_tp_dev = p_test_item->p_tp_dev;
	PST_RAWDATA_SPECIAL_NODE_PARAM p_spec_node =
		p_rawdata_test_param->p_special_node;
	ret = p_rawdata_test_param->p_raw_key_limit->lower_limit;
	for (i = 0; i < p_spec_node->data_len; i++) {
		node_id = p_spec_node->p_data_buf[i].node_id;
		if ((node_id >=
			p_tp_dev->sc_drv_num * p_tp_dev->sc_sen_num)
			&& (node_id -
			p_tp_dev->sc_drv_num * p_tp_dev->sc_sen_num ==
			key_id)) {
			ret = p_spec_node->p_data_buf[i].low_raw_limit;
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: get_uniform_value
* Description	: get uniform value(amplify 1000)
* Input			: PST_TEST_ITEM p_test_item
* Output		: none
* Return		: s32
*******************************************************************************/
extern s32 get_uniform_value(IN PST_TEST_ITEM p_test_item)
{
	s32 ret = 0;
	s32 max_data_tmp = 0;
	s32 min_data_tmp = 0;
	s32 data_tmp = 0;
	u16 i = 0;
	u8 flag = 0;
	PST_TP_DEV p_dev = p_test_item->p_tp_dev;
	for (i = 0; i < p_dev->sc_drv_num * p_dev->sc_sen_num; i++) {
		if (get_chn_need_check(p_test_item, i)) {
			data_tmp = get_chn_rawdata(p_test_item, i);
			/*//Initialize max_data_tmp and min_data_tmp*/
			if (flag == 0) {
				max_data_tmp = data_tmp;
				min_data_tmp = max_data_tmp;
				flag = 1;
			} else {
				/*//Update max_data_tmp*/
				if (data_tmp > max_data_tmp) {
					max_data_tmp = data_tmp;
				}
				/*//Update min_data_tmp*/
				if (data_tmp < min_data_tmp) {
					min_data_tmp = data_tmp;
				}
			}
		}
	}

	/*//avoid 0*/
	if (0 == max_data_tmp) {
		max_data_tmp++;
	}
	if (0 == min_data_tmp) {
		min_data_tmp++;
	}
	ret = (s32) (1000 * min_data_tmp / max_data_tmp);
	return ret;
}

/*******************************************************************************
* Function Name	: get_uniform_limit
* Description	: get uniform limit
* Input			: PST_TEST_ITEM p_test_item
* Output		: none
* Return		: s32
*******************************************************************************/
extern s32 get_uniform_limit(IN PST_TEST_ITEM p_test_item)
{
	s32 ret = 0;
	PST_RAWDATA_TEST_PARAM p_rawdata_test_param =
		(PST_RAWDATA_TEST_PARAM) (p_test_item->param.ptr_param);
	ret = p_rawdata_test_param->p_raw_uniform_limit->uniform_limit;
	return ret;
}

/*******************************************************************************
* Function Name	: get_rawdata_jitter_value
* Description	: get rawdata jitter value limit
* Input			: PST_TEST_ITEM p_test_item
* Output		: none
* Return		: s32
*******************************************************************************/
extern s32 get_rawdata_jitter_value(IN PST_TEST_ITEM p_test_item)
{
	s32 ret = 0;
	s32 tmp_data = 0;
	u16 i = 0;
	PST_RAWDATA_TEST_RES p_raw_test_res =
		(PST_RAWDATA_TEST_RES) p_test_item->test_res.ptr_res;
	for (i = 0; i < p_raw_test_res->p_max_raw_tmp_data->len; i++) {
		tmp_data =
			p_raw_test_res->p_max_raw_tmp_data->p_data_buf[i] -
			p_raw_test_res->p_min_raw_tmp_data->p_data_buf[i];
		if (ret < tmp_data) {
			ret = tmp_data;
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: get_rawdata_jitter_limit
* Description	: get rawdata jitter limit
* Input			: PST_TEST_ITEM p_test_item
* Output		: none
* Return		: s32
*******************************************************************************/
extern s32 get_rawdata_jitter_limit(IN PST_TEST_ITEM p_test_item)
{
	s32 ret = 0;
	PST_RAWDATA_TEST_PARAM p_rawdata_test_param =
		(PST_RAWDATA_TEST_PARAM) (p_test_item->param.ptr_param);
	ret = p_rawdata_test_param->p_raw_jitter_limit->jitter_limit;
	return ret;
}

//-----------------------------diffdata as data source start-------------------------------//
//
/*******************************************************************************
* Function Name	: get_chn_diffdata
* Description	: get specialized channel diffdata
* Input			: PST_TEST_ITEM p_test_item(item pointer)
: u16 chn_id(channel index)
* Output		: none
* Return		: s32
*******************************************************************************/
extern s32 get_chn_diffdata(PST_TEST_ITEM p_test_item, u16 chn_id)
{
	s32 ret = 0;
	PST_CUR_DIFFDATA p_cur_diffdata =
		(PST_CUR_DIFFDATA) (p_test_item->cur_data.ptr_cur_data);
	PST_TP_DEV p_dev = p_test_item->p_tp_dev;
	u16 len = p_dev->sc_sen_num * p_dev->sc_drv_num;

	/*//data is little-endian after reshaping*/
	if (len > chn_id) {
		if (p_dev->diffdata_option & _DATA_BIT_8) {
			ret = p_cur_diffdata->p_data_buf[chn_id];
		} else if (p_dev->diffdata_option & _DATA_BIT_16) {
			ret =
				(s16) ((p_cur_diffdata->
					p_data_buf[chn_id * 2]) +
					(p_cur_diffdata->
					p_data_buf[chn_id * 2 + 1] << 8));
		} else if (p_dev->diffdata_option & _DATA_BIT_32) {
			ret =
				(p_cur_diffdata->p_data_buf[chn_id * 4]) +
				(p_cur_diffdata->
				p_data_buf[chn_id * 4 + 1] << 8) +
				(p_cur_diffdata->
				p_data_buf[chn_id * 4 + 2] << 16) +
				(p_cur_diffdata->
				p_data_buf[chn_id * 4 + 3] << 24);
		}
	}
	return ret;
}

#ifdef __cplusplus
}
#endif

/*******************************************************************************
* Function Name	: init_for_buffer
* Description	: initialize temp buffer with 0
* Input			: u8* p_buf
: u16 buf_size
* Output		: u8* p_buf
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
static s32 init_for_buffer(u8 *p_buf, u16 buf_size)
{
	s32 ret = 1;
	u16 i = 0;
	for (i = 0; i < buf_size; i++) {
		p_buf[i] = 0;
	}
	return ret;
}

/*******************************************************************************
* Function Name	: get_need_check
* Description	: judge channel or key whether need check or not
* Input			: PST_TEST_ITEM p_test_item
: u16 node_num
: s32 beyond_limit_status
* Output		: PST_TEST_ITEM p_test_item
* Return		: s32(0:not need check 1:need check)
*******************************************************************************/
static s32 get_need_check(IN_OUT PST_TEST_ITEM p_test_item, IN u16 id,
				IN s32 beyond_limit_status)
{
	if ((beyond_limit_status == KEY_BEYOND_UPPER_LIMIT)
		|| (beyond_limit_status == KEY_BEYOND_LOWER_LIMIT)) {
		return get_key_need_check(p_test_item, id);
	} else {
		return get_chn_need_check(p_test_item, id);
	}
}

/*******************************************************************************
* Function Name	: analyze_test_item_res
* Description	: analyze rawdata subitem test result
* Input			: PST_TEST_ITEM p_test_item
: s32 beyond_limit_status
: u8* p_beyond_limit_num
: u16 node_num
* Output		: PST_TEST_ITEM p_test_item
* Return		: s32(beyond_limit_satus:test result is unstable 0:others)
*******************************************************************************/
static s32 analyze_test_item_res(IN_OUT PST_TEST_ITEM p_test_item,
				IN s32 beyond_limit_status,
				IN u8 *p_beyond_limit_cnt, IN u16 node_num)
{
	s32 ret = beyond_limit_status;
	u16 id = 0;
	s32 low_ratio = 10 * 0.1;
	s32 high_ratio = 10 * 0.9;
	PST_RAWDATA_TEST_RES p_raw_test_res = NULL;
	u16 cur_frame_id = 0;
	s32 tmp_open_test_result = 0;
	p_raw_test_res = (PST_RAWDATA_TEST_RES) p_test_item->test_res.ptr_res;
	cur_frame_id = p_raw_test_res->total_frame_cnt;
	tmp_open_test_result = p_raw_test_res->open_test_result;
	tmp_open_test_result &= ~beyond_limit_status;
	for (id = 0; id < node_num; id++) {
		if (get_need_check(p_test_item, id, beyond_limit_status)) {
			if (p_beyond_limit_cnt[id] * 10 >=
				high_ratio * cur_frame_id) {
				p_beyond_limit_cnt[id] |= 0x80;
				tmp_open_test_result |= beyond_limit_status;
				ret = 0;
			} else if (p_beyond_limit_cnt[id] * 10 >
					low_ratio * cur_frame_id) {
				p_beyond_limit_cnt[id] |= 0x80;
				tmp_open_test_result |= beyond_limit_status;
				if (cur_frame_id >= TOTAL_FRAME_NUM_MAX) {
					ret = 0;
				}
			} else {
				ret = 0;
			}
		}
	}
	p_raw_test_res->open_test_result = tmp_open_test_result;
	return ret;
}

/*******************************************************************************
* Function Name	: clear_rawtest_res_buf
* Description	: clear_rawtest_res_buf
* Input			: PST_TEST_ITEM p_test_item
* Output		: PST_TEST_ITEM p_test_item
* Return		: s32(1:ok 0:fail)
*******************************************************************************/
static s32 clear_rawtest_res_buf(IN_OUT PST_TEST_ITEM p_test_item)
{
	PST_RAWDATA_TEST_RES p_test_res =
		(PST_RAWDATA_TEST_RES) p_test_item->test_res.ptr_res;
	PST_TP_DEV p_dev = (PST_TP_DEV) p_test_item->p_tp_dev;
	u16 buf_size = p_dev->sc_drv_num * p_dev->sc_sen_num;
	u16 i = 0;
	u8 key_buf_size = p_dev->key_num;
	PST_CUR_RAWDATA p_cur_data =
		(PST_CUR_RAWDATA) p_test_item->cur_data.ptr_cur_data;

	p_cur_data->cur_frame_id = 0;
	p_test_res->item_res_header.test_status = TEST_START;
	p_test_res->item_res_header.test_result = TEST_NG;
	for (i = 0; i < p_test_res->item_res_header.test_item_err_code.len; i++) {
		p_test_res->item_res_header.test_item_err_code.
			ptr_err_code_set[i] = RAWDATA_TEST_SUCCESSFULLY;
	}
	p_test_res->open_test_result = OPEN_TEST_PASS;
	p_test_res->total_frame_cnt = 0;

	/*//max data record*/
	for (i = 0; i < buf_size; i++) {
		p_test_res->p_max_raw_tmp_data->p_data_buf[i] = 0;
	}

	/*/min data record*/
	for (i = 0; i < buf_size; i++) {
		p_test_res->p_min_raw_tmp_data->p_data_buf[i] = 0x7FFFFFFF;
	}

	init_for_buffer(p_test_res->p_beyond_rawdata_upper_limit_cnt, buf_size);
	init_for_buffer(p_test_res->p_beyond_rawdata_lower_limit_cnt, buf_size);
	init_for_buffer(p_test_res->p_beyond_jitter_limit_cnt, buf_size);
	init_for_buffer(p_test_res->p_beyond_accord_limit_cnt, buf_size);
	init_for_buffer(p_test_res->p_beyond_offset_limit_cnt, buf_size);
	init_for_buffer(p_test_res->p_beyond_key_upper_limit_cnt, key_buf_size);
	init_for_buffer(p_test_res->p_beyond_key_lower_limit_cnt, key_buf_size);
	p_test_res->beyond_uniform_limit_cnt = 0;
	return 1;
}

/*******************************************************************************
* Function Name	: analyze_rawtest_res
* Description	: analyze rawdata test result
* Input			: u8 offse_and_accord_flag
*				: PST_TEST_ITEM p_test_item
* Output		: PST_TEST_ITEM p_test_item
* Return		: s32(0:test result is pass 1:test result is fail 2:test result is unstable)
*******************************************************************************/
static s32 analyze_rawtest_res(IN u8 offse_and_accord_flag,
				IN_OUT PST_TEST_ITEM p_test_item)
{
	s32 ret = 0;
	u16 i = 0;
	s32 test_res_unstb = 0;
	s32 test_end = 0;
	u16 node_num = 0;
	u8 key_num = 0;
	s32 low_ratio = 10 * 0.1;
	s32 high_ratio = 10 * 0.9;
	u8 id_tmp = 0;

	PST_RAWDATA_TEST_RES p_test_res = NULL;

	PST_TP_DEV p_dev = p_test_item->p_tp_dev;
	node_num = p_dev->sc_sen_num * p_dev->sc_drv_num;
	p_test_res = (PST_RAWDATA_TEST_RES) p_test_item->test_res.ptr_res;
	key_num = p_dev->key_num;

	/*//cover error code*/
	for (i = 0; i < p_test_res->item_res_header.test_item_err_code.len; i++) {
		p_test_res->item_res_header.test_item_err_code.
			ptr_err_code_set[i] = RAWDATA_TEST_SUCCESSFULLY;
	}

	/*//analyze rawdata upper limit*/
	if ((p_test_res->open_test_result & BEYOND_RAWDATA_UPPER_LIMIT) != 0) {
		test_res_unstb |=
			analyze_test_item_res(p_test_item,
				BEYOND_RAWDATA_UPPER_LIMIT,
				p_test_res->
				p_beyond_rawdata_upper_limit_cnt,
				node_num);

		if (0 == (test_res_unstb & BEYOND_RAWDATA_UPPER_LIMIT)
			&& (p_test_res->
			open_test_result & BEYOND_RAWDATA_UPPER_LIMIT) != 0) {
			id_tmp =
				get_arr_id_from_sub_item(p_test_item,
						TP_RAWDATA_MAX_TEST_ITEM_ID);
			p_test_res->item_res_header.test_item_err_code.
				ptr_err_code_set[id_tmp + 1] =
				RAWDATA_TEST_BEYOND_MAX_LIMIT;
		}
	}
	/*//analyze rawdata lower limit*/
	if ((p_test_res->open_test_result & BEYOND_RAWDATA_LOWER_LIMIT) != 0) {
		test_res_unstb |=
			analyze_test_item_res(p_test_item,
					BEYOND_RAWDATA_LOWER_LIMIT,
					p_test_res->
					p_beyond_rawdata_lower_limit_cnt,
					node_num);

		if (0 == (test_res_unstb & BEYOND_RAWDATA_LOWER_LIMIT)
			&& (p_test_res->
			open_test_result & BEYOND_RAWDATA_LOWER_LIMIT) != 0) {
			id_tmp =
				get_arr_id_from_sub_item(p_test_item,
							TP_RAWDATA_MIN_TEST_ITEM_ID);
			p_test_res->item_res_header.test_item_err_code.
				ptr_err_code_set[id_tmp + 1] =
				RAWDATA_TEST_BEYOND_MIN_LIMIT;
		}
	}
	/*//analyze key upper limit*/
	if ((p_test_res->open_test_result & KEY_BEYOND_UPPER_LIMIT) != 0) {
		test_res_unstb |=
			analyze_test_item_res(p_test_item, KEY_BEYOND_UPPER_LIMIT,
					p_test_res->
					p_beyond_key_upper_limit_cnt,
					key_num);

		if (0 == (test_res_unstb & KEY_BEYOND_UPPER_LIMIT)
			&& (p_test_res->
			open_test_result & KEY_BEYOND_UPPER_LIMIT) != 0) {
			id_tmp = get_arr_id_from_sub_item(p_test_item,
							TP_KEYDATA_MAX_TEST_ITEM_ID);
			p_test_res->item_res_header.test_item_err_code.
				ptr_err_code_set[id_tmp + 1] =
				RAWDATA_TEST_BEYOND_KEY_MAX_LIMIT;
		}
	}
	/*//analyze key lower limit*/
	if ((p_test_res->open_test_result & KEY_BEYOND_LOWER_LIMIT) != 0) {
		test_res_unstb |=
			analyze_test_item_res(p_test_item, KEY_BEYOND_LOWER_LIMIT,
					p_test_res->
					p_beyond_key_lower_limit_cnt,
					key_num);

		if (0 == (test_res_unstb & KEY_BEYOND_LOWER_LIMIT)
			&& (p_test_res->
			open_test_result & KEY_BEYOND_LOWER_LIMIT) != 0) {
			id_tmp = get_arr_id_from_sub_item(p_test_item,
							TP_KEYDATA_MIN_TEST_ITEM_ID);
			p_test_res->item_res_header.test_item_err_code.
				ptr_err_code_set[id_tmp + 1] =
				RAWDATA_TEST_BEYOND_KEY_MIN_LIMIT;
		}
	}
	/*//analyze offset limit*/
	if ((p_test_res->open_test_result & BEYOND_OFFSET_LIMIT) != 0) {
		test_res_unstb |=
			analyze_test_item_res(p_test_item, BEYOND_OFFSET_LIMIT,
					p_test_res->p_beyond_offset_limit_cnt,
					node_num);

		if (0 == (test_res_unstb & BEYOND_OFFSET_LIMIT)
			&& (p_test_res->open_test_result & BEYOND_OFFSET_LIMIT) !=
			0) {
			id_tmp = get_arr_id_from_sub_item(p_test_item,
							TP_OFFSET_TEST_ITEM_ID);
			p_test_res->item_res_header.test_item_err_code.
				ptr_err_code_set[id_tmp + 1] =
				RAWDATA_TEST_BEYOND_OFFSET_LIMIT;
		}
	}

	/*//analyze uniform limit*/
	if ((p_test_res->open_test_result & BEYOND_UNIFORMITY_LIMIT) != 0) {
		p_test_res->open_test_result &= ~BEYOND_UNIFORMITY_LIMIT;
		if (p_test_res->beyond_uniform_limit_cnt * 10 >=
			high_ratio * p_test_res->total_frame_cnt) {
			p_test_res->open_test_result |= BEYOND_UNIFORMITY_LIMIT;
		} else if (p_test_res->beyond_uniform_limit_cnt * 10 >
				low_ratio * p_test_res->total_frame_cnt) {
			p_test_res->open_test_result |= BEYOND_UNIFORMITY_LIMIT;
			test_res_unstb |= BEYOND_UNIFORMITY_LIMIT;
		}

		if (0 == (test_res_unstb & BEYOND_UNIFORMITY_LIMIT)
			&& (p_test_res->
			open_test_result & BEYOND_UNIFORMITY_LIMIT) != 0) {
			id_tmp = get_arr_id_from_sub_item(p_test_item,
							TP_UNIFORMITY_TEST_ITEM_ID);
			p_test_res->item_res_header.test_item_err_code.
				ptr_err_code_set[id_tmp + 1] =
				RAWDATA_TEST_BEYOND_UNIFORM_LIMIT;
		}
	}

	/*//analyze accord limit*/
	if ((p_test_res->open_test_result & BEYOND_ACCORD_LIMIT) != 0) {
		test_res_unstb |=
			analyze_test_item_res(p_test_item, BEYOND_ACCORD_LIMIT,
					p_test_res->p_beyond_accord_limit_cnt,
					node_num);

		if (0 == (test_res_unstb & BEYOND_ACCORD_LIMIT)
			&& (p_test_res->open_test_result & BEYOND_ACCORD_LIMIT) !=
			0) {
			id_tmp = get_arr_id_from_sub_item(p_test_item,
							TP_ACCORD_TEST_ITEM_ID);
			p_test_res->item_res_header.test_item_err_code.
				ptr_err_code_set[id_tmp + 1] =
				RAWDATA_TEST_BEYOND_ACCORD_LIMIT;
		}
	}

	/*// analyze accord and offset limit*/
	if (offse_and_accord_flag == 0x03) {
		u32 tmp_accord_res = 0;
		for (i = 0; i < node_num; i++) {
			if (get_chn_need_check(p_test_item, i)) {
				if ((p_test_res->
					p_beyond_accord_limit_cnt[i] & 0x80) !=
					0) {
					if ((p_test_res->
						p_beyond_offset_limit_cnt[i] &
						0x80) != 0) {
						tmp_accord_res |=
							BEYOND_ACCORD_LIMIT;
					} else {
						p_test_res->
							p_beyond_accord_limit_cnt[i]
							&= 0x7f;
					}
				}
			}
		}
		if (tmp_accord_res == 0) {
			test_res_unstb &= ~BEYOND_ACCORD_LIMIT;
			p_test_res->open_test_result &= ~BEYOND_ACCORD_LIMIT;

			id_tmp = get_arr_id_from_sub_item(p_test_item,
							TP_ACCORD_TEST_ITEM_ID);
			p_test_res->item_res_header.test_item_err_code.
				ptr_err_code_set[id_tmp + 1] =
				RAWDATA_TEST_SUCCESSFULLY;
		}

	}

	if (p_test_res->open_test_result & BEYOND_JITTER_LIMIT) {
		id_tmp = get_arr_id_from_sub_item(p_test_item,
						TP_RAWDATA_JITTER_TEST_ITEM_ID);
		p_test_res->item_res_header.test_item_err_code.
			ptr_err_code_set[id_tmp + 1] =
			RAWDATA_TEST_BEYOND_JITTER_LIMIT;
		return 1;
	}
	test_end = p_test_res->open_test_result;
	test_end &= (~FIRST_TEST_FLAG);
	if (test_end == OPEN_TEST_PASS) {
		ret = 0;
	} else if (test_end == test_res_unstb) {
		ret = 2;
	} else {
		ret = 1;
	}
	return ret;
}

/*******************************************************************************
* Function Name	: modify_statistic_data
* Description	: modify max and min avg data
* Input			: PST_TEST_ITEM p_test_item
* Output		: PST_TEST_ITEM p_test_item
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
static s32 modify_statistic_data(IN_OUT PST_TEST_ITEM p_test_item)
{
	s32 ret = 0;
	u16 i = 0;
	s32 curval;
	u16 cnt = 0;
	PST_RAWDATA_TEST_RES p_raw_test_res =
		(PST_RAWDATA_TEST_RES) p_test_item->test_res.ptr_res;
	PST_TP_DEV p_dev = p_test_item->p_tp_dev;

	p_raw_test_res->raw_avg = 0;
	for (i = 0; i < p_dev->rawdata_len; i++) {
		if (get_chn_need_check(p_test_item, i)) {
			curval = get_chn_rawdata(p_test_item, i);
			p_raw_test_res->raw_avg += curval;
			cnt++;
			if (curval >
				p_raw_test_res->p_max_raw_tmp_data->p_data_buf[i]) {
				p_raw_test_res->p_max_raw_tmp_data->
					p_data_buf[i] = curval;
			}

			if (curval <
				p_raw_test_res->p_min_raw_tmp_data->p_data_buf[i]) {
				p_raw_test_res->p_min_raw_tmp_data->
					p_data_buf[i] = curval;
			}
		}

	}
	if (0 != cnt) {
		p_raw_test_res->raw_avg /= cnt;
	} else {
		p_raw_test_res->raw_avg = 0;
	}
	ret = 1;
	return ret;
}

/*******************************************************************************
* Function Name	: clear_difftest_res_buf
* Description	: clear_difftest_res_buf
* Input			: PST_TEST_ITEM p_test_item
* Output		: PST_TEST_ITEM p_test_item
* Return		: s32(1:ok 0:fail)
*******************************************************************************/
static s32 clear_difftest_res_buf(IN_OUT PST_TEST_ITEM p_test_item)
{
	PST_DIFFDATA_TEST_RES p_test_res =
		(PST_DIFFDATA_TEST_RES) p_test_item->test_res.ptr_res;
	PST_TP_DEV p_dev = (PST_TP_DEV) p_test_item->p_tp_dev;
	u16 buf_size = p_dev->sc_drv_num * p_dev->sc_sen_num;
	u16 i = 0;

	PST_CUR_DIFFDATA p_cur_data =
		(PST_CUR_DIFFDATA) p_test_item->cur_data.ptr_cur_data;

	p_cur_data->cur_frame_id = 0;
	p_test_res->item_res_header.test_status = TEST_START;
	p_test_res->item_res_header.test_result = TEST_OK;
	for (i = 0; i < p_test_res->item_res_header.test_item_err_code.len; i++) {
		p_test_res->item_res_header.test_item_err_code.
			ptr_err_code_set[i] = DIFFDATA_TEST_SUCCESSFULLY;
	}
	p_test_res->total_frame_cnt = 0;

	/*//max data record*/
	for (i = 0; i < buf_size && i < p_test_res->p_max_diff_tmp_data->len;
		i++) {
		p_test_res->p_max_diff_tmp_data->p_data_buf[i] = 0;
	}
	return 1;
}

/*******************************************************************************
* Function Name	: analyze_difftest_res
* Description	: analyze diffdata test result
* Input			: PST_TEST_ITEM p_test_item
* Output		: PST_TEST_ITEM p_test_item
* Return		: s32(0:test result is pass 1:test result is fail 2:test result is unstable)
*******************************************************************************/
static s32 analyze_difftest_res(u8 cur_frame_id, PST_TEST_ITEM p_test_item)
{
	s32 ret = 0;
	u16 i = 0;
	u16 node_num = 0;
	u8 id_tmp = 0;

	PST_DIFFDATA_TEST_RES p_test_res = NULL;
	PST_DIFFDATA_TEST_PARAM p_param = NULL;
	PST_TEST_ITEM p_raw_data_test_item = NULL;
	PST_TP_DEV p_dev = NULL;
	get_test_item_from_proc_data((PST_TEST_PROC_DATA) p_test_item->
				ptr_test_proc_data,
				TP_RAWDATA_TEST_ITEMS_SET_ID,
				&p_raw_data_test_item);

	p_param = (PST_DIFFDATA_TEST_PARAM) p_test_item->param.ptr_param;
	p_test_res = (PST_DIFFDATA_TEST_RES) p_test_item->test_res.ptr_res;
	p_dev = p_test_item->p_tp_dev;
	node_num = p_dev->sc_sen_num * p_dev->sc_drv_num;

	/*//cover error code*/
	for (i = 0; i < p_test_res->item_res_header.test_item_err_code.len; i++) {
		p_test_res->item_res_header.test_item_err_code.
			ptr_err_code_set[i] = DIFFDATA_TEST_SUCCESSFULLY;
	}

	/*//diff jitter test */
	id_tmp = get_arr_id_from_sub_item(p_test_item,
					TP_DIFFDATA_JITTER_TEST_ITEM_ID);
	for (i = 0; i < node_num; i++) {
		/*//share need_check_node with item 'TP_RAWDATA_TEST_ITEMS_SET_ID'*/
		if (get_chn_need_check(p_raw_data_test_item, i)) {
			if (p_test_res->p_max_diff_tmp_data->p_data_buf[i] >
				p_param->p_diff_jitter_limit->diff_jitter_limit) {
				p_test_res->item_res_header.test_item_err_code.
					ptr_err_code_set[id_tmp + 1] =
					DIFFTEST_JITTER_BEYOND_LIMIT;
				ret = 1;
				break;
			}
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: modify_diff_statistic_data
* Description	: modify statistic data of diffdata
* Input			: PST_TEST_ITEM p_test_item
* Output		: PST_TEST_ITEM p_test_item
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
static s32 modify_diff_statistic_data(PST_TEST_ITEM p_test_item)
{
	s32 ret = 0;
	u16 i = 0;
	s32 curval;
	PST_DIFFDATA_TEST_RES p_test_res =
		(PST_DIFFDATA_TEST_RES) p_test_item->test_res.ptr_res;

	PST_TP_DEV p_dev = p_test_item->p_tp_dev;
	PST_TEST_ITEM p_raw_data_test_item = NULL;
	get_test_item_from_proc_data((PST_TEST_PROC_DATA) p_test_item->
				ptr_test_proc_data,
				TP_RAWDATA_TEST_ITEMS_SET_ID,
				&p_raw_data_test_item);

	if (NULL == p_test_res->p_max_diff_tmp_data) {
		goto MODIFY_END;
	}

	for (i = 0; i < p_dev->diffdata_len; i++) {
		/*//share need_check_node with item 'TP_RAWDATA_TEST_ITEMS_SET_ID'*/
		if (get_chn_need_check(p_raw_data_test_item, i)) {
			curval = get_chn_diffdata(p_test_item, i);
			if (curval >
				p_test_res->p_max_diff_tmp_data->p_data_buf[i]) {
				p_test_res->p_max_diff_tmp_data->p_data_buf[i] =
					curval;
			}
		}
	}
	ret = 1;

MODIFY_END:
	return ret;
}

/*//-----------------------------diffdata as data source end-------------------------------*/

#endif