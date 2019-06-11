/******************** (C) COPYRIGHT 2017 Goodix ********************
* File Name          : tp_test_item_mem.cpp
* Author             : Bob Huang
* Version            : V1.0.0
* Date               : 01/11/2018
* Description        : Tp Test Item Result Memory Process
*******************************************************************************/

#include "tp_test_item_mem.h"
#include "test_proc.h"
#include "simple_mem_manager.h"
#include "tp_open_test.h"
#include "tp_short_test.h"
#include "tp_version_test.h"
#include "tp_additional_option.h"
#include <linux/string.h>

#ifdef __cplusplus
extern "C" {
#endif

static s32 init_for_buffer(u8 *p_buf, u16 buf_size);

/*******************************************************************************
* Function Name	: alloc_test_item_res
* Description	: alloc test item res
* Input			: none
* Output		: none
* Return		: s32(1:ok 0:fail)
*******************************************************************************/
s32 alloc_test_item_res(PST_TEST_ITEM *pp_test_item, u16 item_num)
{
	s32 ret = 0;
	u16 i = 0;
	for (i = 0; i < item_num; i++) {
		PST_TEST_ITEM p_test_item = pp_test_item[i];
		switch (p_test_item->test_item_id) {
			/*//----------------------tp open test-----------------------//*/
		case TP_RAWDATA_TEST_ITEMS_SET_ID:
			{
				if (1 ==
					alloc_rawdata_test_item_res
					(p_test_item)) {
					ret = 1;
				}
			} break;

		case TP_DIFFDATA_TEST_ITEMS_SET_ID:
			{
				if (1 ==
					alloc_diffdata_test_item_res
					(p_test_item)) {
					ret = 1;
				}
			}
			break;

			/*//----------------------hardware test-----------------------//*/
		case TP_SHORT_TEST_ITEM_ID:
			{
				if (1 ==
					alloc_short_test_item_res
					(p_test_item)) {
					ret = 1;
				}
			}
			break;

			/*//----------------------firmware test-----------------------//*/
		case TP_VERSION_TEST_ITEM_ID:
			{
				if (1 ==
					alloc_version_test_item_res
					(p_test_item)) {
					ret = 1;
				}
			}
			break;

			/*//----------------------other test-----------------------//*/
		case TP_CHIP_CFG_PROC_ITEM_ID:
			{
				if (1 ==
					alloc_chip_cfg_proc_item_res
					(p_test_item)) {
					ret = 1;
				}
			}
			break;

		case TP_FLASH_TEST_ITEM_ID:
			{
				if (1 ==
					alloc_flash_test_item_res
					(p_test_item)) {
					ret = 1;
				}
			}
			break;

		case TP_RECOVER_CFG_ITEM_ID:
			{
				if (1 ==
					alloc_recover_cfg_item_res
					(p_test_item)) {
					ret = 1;
				}
			}
			break;
		default:
			break;
		}
	}
	ret = 1;
	return ret;
}

/*******************************************************************************
* Function Name	: free_test_item_res
* Description	: free test item res
* Input			: none
* Output		: none
* Return		: s32(1:ok 0:fail)
*******************************************************************************/
s32 free_test_item_res(PST_TEST_ITEM *pp_test_item, u16 item_num)
{
	s32 ret = 0;
	u16 i = 0;
	for (i = 0; i < item_num; i++) {
		PST_TEST_ITEM p_test_item = pp_test_item[i];
		switch (p_test_item->test_item_id) {
			/*//----------------------tp open test-----------------------//*/
		case TP_RAWDATA_TEST_ITEMS_SET_ID:
			{
				if (1 ==
					free_rawdata_test_item_res
					(p_test_item)) {
					ret = 1;
				}
			}
			break;

		case TP_DIFFDATA_TEST_ITEMS_SET_ID:
			{
				if (1 ==
					free_diffdata_test_item_res
					(p_test_item)) {
					ret = 1;
				}
			}
			break;

			/*//----------------------hardware test-----------------------//*/
		case TP_SHORT_TEST_ITEM_ID:
			{
				if (1 ==
					free_short_test_item_res
					(p_test_item)) {
					ret = 1;
				}
			}
			break;

			/*//----------------------firmware test-----------------------//*/
		case TP_VERSION_TEST_ITEM_ID:
			{
				if (1 ==
					free_version_test_item_res
					(p_test_item)) {
					ret = 1;
				}
			}
			break;
			/*//----------------------other test-----------------------//*/
		case TP_CHIP_CFG_PROC_ITEM_ID:
			{
				if (1 ==
					free_chip_cfg_proc_item_res
					(p_test_item)) {
					ret = 1;
				}
			}
			break;

		case TP_FLASH_TEST_ITEM_ID:
			{
				if (1 ==
					free_flash_test_item_res
					(p_test_item)) {
					ret = 1;
				}
			}
			break;

		case TP_RECOVER_CFG_ITEM_ID:
			{
				if (1 ==
					free_recover_cfg_item_res
					(p_test_item)) {
					ret = 1;
				}
			}
			break;
		default:
			break;
		}
	}
	ret = 1;
	return ret;
}

/*******************************************************************************
* Function Name	: alloc_item_res_header
* Description	: alloc_item_res_header
* Input			: PST_TEST_ITEM p_test_item
* Output		: PST_TEST_ITEM p_test_item
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
s32 alloc_item_res_header(IN_OUT PST_TEST_ITEM p_test_item)
{
	P_TEST_ITEM_RES p_item_res_header = NULL;
	p_item_res_header =
	(P_TEST_ITEM_RES) p_test_item->test_res.ptr_res;

	/*//default initialization*/
	p_item_res_header->test_id = p_test_item->test_item_id;
	p_item_res_header->test_status = TEST_START;
	p_item_res_header->test_result = TEST_OK;

	/*//item_res_header.p_sub_id_set*/
	if (NULL != p_test_item->p_sub_id_set) {
		if (alloc_mem_in_heap
			((void **)&(p_item_res_header->p_sub_id_set),
			sizeof(ST_SUB_TEST_ITEM_ID_SET))) {
			p_item_res_header->p_sub_id_set->len =
				p_test_item->p_sub_id_set->len;
			if (alloc_mem_in_heap
				((void **)
				&(p_item_res_header->p_sub_id_set->
					p_id_set),
				p_item_res_header->p_sub_id_set->len *
				sizeof(u8))) {
				memcpy(p_item_res_header->p_sub_id_set->
						p_id_set,
						p_test_item->p_sub_id_set->
						p_id_set,
						p_test_item->p_sub_id_set->len);
			}
		}
		/*//item_res_header.test_item_err_code.ptr_err_code_set*/
		p_item_res_header->test_item_err_code.len = p_test_item->p_sub_id_set->len + 1;	/*//each sub test for 1byte,maybe modify in future*/
		alloc_mem_in_heap((void **)
				&(p_item_res_header->
					test_item_err_code.
					ptr_err_code_set),
				p_item_res_header->test_item_err_code.
					len * sizeof(u16));
		memset(p_item_res_header->test_item_err_code.
				ptr_err_code_set, 0,
				p_item_res_header->test_item_err_code.len);
	} else {
		p_item_res_header->p_sub_id_set = NULL;
		p_item_res_header->test_item_err_code.len = 1;	/*//main test for 1byte,maybe modify in future*/
		alloc_mem_in_heap((void **)
			&(p_item_res_header->
				test_item_err_code.
				ptr_err_code_set),
			p_item_res_header->test_item_err_code.
				len * sizeof(u16));
		memset(p_item_res_header->test_item_err_code.
				ptr_err_code_set, 0,
				p_item_res_header->test_item_err_code.len *
				sizeof(u16));
	}

	return 1;
}

/*******************************************************************************
* Function Name	: free_item_res_header
* Description	: free_item_res_header
* Input			: PST_TEST_ITEM p_test_item
: u16 buf_size
* Output		:
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
s32 free_item_res_header(IN_OUT PST_TEST_ITEM p_test_item)
{
	P_TEST_ITEM_RES p_item_res_header = NULL;
	p_item_res_header =
		(P_TEST_ITEM_RES) p_test_item->test_res.ptr_res;

	if (NULL !=
		p_item_res_header->test_item_err_code.ptr_err_code_set) {
		free_mem_in_heap((void *)p_item_res_header->
				test_item_err_code.ptr_err_code_set);
		p_item_res_header->test_item_err_code.ptr_err_code_set =
			NULL;
	}

	if (NULL != p_item_res_header->p_sub_id_set) {
		if (NULL != p_item_res_header->p_sub_id_set->p_id_set) {
			free_mem_in_heap((void *)p_item_res_header->
					p_sub_id_set->p_id_set);
			p_item_res_header->p_sub_id_set->p_id_set =
				NULL;
		}
		free_mem_in_heap((void *)p_item_res_header->
				p_sub_id_set);
		p_item_res_header->p_sub_id_set = NULL;
	}
	return 1;
}

/*******************************************************************************
* Function Name	: alloc_rawdata_test_item_res
* Description	: alloc_rawdata_test_item_res
* Input			: PST_TEST_ITEM p_test_item
* Output		: PST_TEST_ITEM p_test_item
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
s32 alloc_rawdata_test_item_res(IN_OUT PST_TEST_ITEM p_test_item)
{
	s32 ret = 0;
	u16 i = 0;

	PST_TP_DEV p_tp_dev = p_test_item->p_tp_dev;
	u16 buf_size = p_tp_dev->sc_drv_num * p_tp_dev->sc_sen_num;
	u8 key_buf_size = p_tp_dev->key_num;
	u8 data_byte = 0;
	PST_RAWDATA_TEST_RES *pp_rawdata_res =
		(PST_RAWDATA_TEST_RES *) (&p_test_item->test_res.ptr_res);
	PST_CUR_RAWDATA p_cur_rawdata = NULL;
	if (p_tp_dev->rawdata_option & _DATA_BIT_8) {
		data_byte = 1;
	} else if (p_tp_dev->rawdata_option & _DATA_BIT_16) {
		data_byte = 2;
	} else if (p_tp_dev->rawdata_option & _DATA_BIT_32) {
		data_byte = 4;
	}

	if (NULL != p_test_item->test_res.ptr_res) {
		return 1;
	}
	if (alloc_mem_in_heap
		((void **)pp_rawdata_res, sizeof(ST_RAWDATA_TEST_RES))) {

		ret = alloc_item_res_header(p_test_item);
		if (0 == ret) {
			return ret;
		}

		(*pp_rawdata_res)->p_max_raw_tmp_data = NULL;
		(*pp_rawdata_res)->p_min_raw_tmp_data = NULL;
		(*pp_rawdata_res)->p_beyond_rawdata_upper_limit_cnt =
			NULL;
		(*pp_rawdata_res)->p_beyond_rawdata_lower_limit_cnt =
			NULL;
		(*pp_rawdata_res)->p_beyond_jitter_limit_cnt = NULL;
		(*pp_rawdata_res)->p_beyond_accord_limit_cnt = NULL;
		(*pp_rawdata_res)->p_beyond_offset_limit_cnt = NULL;
		(*pp_rawdata_res)->p_beyond_key_upper_limit_cnt = NULL;
		(*pp_rawdata_res)->p_beyond_key_lower_limit_cnt = NULL;
		(*pp_rawdata_res)->beyond_uniform_limit_cnt = 0;
		(*pp_rawdata_res)->total_frame_cnt = 0;
		(*pp_rawdata_res)->open_test_result = OPEN_TEST_PASS;

		/*//max data tmp buffer*/
		if (alloc_mem_in_heap
			((void **)&((*pp_rawdata_res)->p_max_raw_tmp_data),
				sizeof(ST_RAW_TMP_DATA))) {
			(*pp_rawdata_res)->p_max_raw_tmp_data->len = 0;
			if (1 ==
				alloc_mem_block((void **)
					&((*pp_rawdata_res)->
						p_max_raw_tmp_data->
						p_data_buf),
					buf_size * sizeof(s32))) {
				for (i = 0; i < buf_size; i++) {
					(*pp_rawdata_res)->
						p_max_raw_tmp_data->
						p_data_buf[i] = 0;
				}
				(*pp_rawdata_res)->p_max_raw_tmp_data->
					len =
					p_tp_dev->sc_sen_num *
					p_tp_dev->sc_drv_num;
			}
		}

		/*//min data tmp buffer*/
		if (alloc_mem_in_heap
			((void **)&((*pp_rawdata_res)->p_min_raw_tmp_data),
			sizeof(ST_RAW_TMP_DATA))) {
			(*pp_rawdata_res)->p_min_raw_tmp_data->len = 0;
			if (alloc_mem_block
				((void **)
				&((*pp_rawdata_res)->p_min_raw_tmp_data->
					p_data_buf), buf_size * sizeof(s32))) {
				for (i = 0; i < buf_size; i++) {
					(*pp_rawdata_res)->
						p_min_raw_tmp_data->
							p_data_buf[i] = 0x7FFFFFFF;
				}
				(*pp_rawdata_res)->p_min_raw_tmp_data->
				len =
				p_tp_dev->sc_sen_num *
				p_tp_dev->sc_drv_num;
			}
		}
		/*//beyond rawdata upper limit buffer*/
		if (alloc_mem_in_heap
			((void **)
			&((*pp_rawdata_res)->
				p_beyond_rawdata_upper_limit_cnt), buf_size)) {
			init_for_buffer((u8 *) (*pp_rawdata_res)->
					p_beyond_rawdata_upper_limit_cnt,
					buf_size);
		}
		/*//beyond rawdata lower limit buffer*/
		if (alloc_mem_in_heap
			((void **)
			&((*pp_rawdata_res)->
				p_beyond_rawdata_lower_limit_cnt), buf_size)) {
			init_for_buffer((*pp_rawdata_res)->
					p_beyond_rawdata_lower_limit_cnt,
					buf_size);
		}
		/*//beyond jitter limit buffer*/
		if (alloc_mem_in_heap
			((void **)
			&((*pp_rawdata_res)->p_beyond_jitter_limit_cnt),
			buf_size)) {
			init_for_buffer((*pp_rawdata_res)->
					p_beyond_jitter_limit_cnt,
					buf_size);
		}
		/*//beyond accord limit buffer*/
		if (alloc_mem_in_heap
			((void **)
			&((*pp_rawdata_res)->p_beyond_accord_limit_cnt),
			buf_size)) {
			init_for_buffer((*pp_rawdata_res)->
					p_beyond_accord_limit_cnt,
					buf_size);
		}
		/*//beyond offset limit buffer*/
		if (alloc_mem_in_heap
			((void **)
			&((*pp_rawdata_res)->p_beyond_offset_limit_cnt),
			buf_size)) {
			init_for_buffer((*pp_rawdata_res)->
					p_beyond_offset_limit_cnt,
					buf_size);
		}
		/*//beyond key upper limit buffer*/
		if (alloc_mem_in_heap
			((void **)
			&((*pp_rawdata_res)->p_beyond_key_upper_limit_cnt),
			key_buf_size)) {
			init_for_buffer((*pp_rawdata_res)->
					p_beyond_key_upper_limit_cnt,
					key_buf_size);
		}
		/*//beyond key lower limit buffer*/
		if (alloc_mem_in_heap
			((void **)
			&((*pp_rawdata_res)->p_beyond_key_lower_limit_cnt),
			key_buf_size)) {
			init_for_buffer((*pp_rawdata_res)->
					p_beyond_key_lower_limit_cnt,
					key_buf_size);
		}

		if (alloc_mem_in_heap
			((void **)&(p_test_item->cur_data.ptr_cur_data),
			sizeof(ST_CUR_RAWDATA))) {
			buf_size = p_tp_dev->rawdata_len;
			p_cur_rawdata =
				(PST_CUR_RAWDATA) p_test_item->cur_data.
				ptr_cur_data;
			p_cur_rawdata->cur_frame_id = 0;
			p_cur_rawdata->data_len = buf_size;
			if (alloc_mem_block
				((void **)&(p_cur_rawdata->p_data_buf),
				buf_size * data_byte)) {
				init_for_buffer((u8 *) (p_cur_rawdata->
							p_data_buf),
						buf_size * data_byte);
			}
		}

		if (NULL !=
			(*pp_rawdata_res)->p_max_raw_tmp_data->p_data_buf
			&& NULL !=
			(*pp_rawdata_res)->p_min_raw_tmp_data->p_data_buf
			&& NULL !=
			(*pp_rawdata_res)->p_beyond_rawdata_upper_limit_cnt
			&& NULL !=
			(*pp_rawdata_res)->p_beyond_rawdata_lower_limit_cnt
			&& NULL !=
			(*pp_rawdata_res)->p_beyond_jitter_limit_cnt
			&& NULL !=
			(*pp_rawdata_res)->p_beyond_accord_limit_cnt
			&& NULL !=
			(*pp_rawdata_res)->p_beyond_offset_limit_cnt
			&& NULL !=
			(*pp_rawdata_res)->p_beyond_key_upper_limit_cnt
			&& NULL !=
			(*pp_rawdata_res)->p_beyond_key_lower_limit_cnt
			&& NULL !=
			(*pp_rawdata_res)->item_res_header.
			test_item_err_code.ptr_err_code_set
			&& NULL != p_test_item->cur_data.ptr_cur_data
			&& NULL !=
			((PST_CUR_RAWDATA) p_test_item->cur_data.
			ptr_cur_data)->p_data_buf) {
			ret = 1;
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: free_rawdata_test_item_res
* Description	: free_rawdata_test_item_res
* Input			: PST_TEST_ITEM p_test_item
* Output		: PST_TEST_ITEM p_test_item
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
s32 free_rawdata_test_item_res(PST_TEST_ITEM p_test_item)
{
	s32 ret = 1;
	PST_RAWDATA_TEST_RES *pp_rawdata_res = NULL;
	PST_CUR_RAWDATA p_curdata = NULL;
	if (NULL == p_test_item->test_res.ptr_res) {
		return 1;
	}
	pp_rawdata_res =
		(PST_RAWDATA_TEST_RES *) (&p_test_item->test_res.ptr_res);

	/*//free test item res header*/
	ret &= free_item_res_header(p_test_item);

	/*//free beyond rawdata upper limit buffer*/
	ret &= free_mem_in_heap((void *)(*pp_rawdata_res)->
				p_beyond_rawdata_upper_limit_cnt);
	(*pp_rawdata_res)->p_beyond_rawdata_upper_limit_cnt = NULL;

	/*//free beyond rawdata lower limit buffer*/
	ret &= free_mem_in_heap((void *)(*pp_rawdata_res)->
				p_beyond_rawdata_lower_limit_cnt);
	(*pp_rawdata_res)->p_beyond_rawdata_lower_limit_cnt = NULL;

	/*//free beyond jitter limit buffer*/
	ret &= free_mem_in_heap((void *)(*pp_rawdata_res)->
				p_beyond_jitter_limit_cnt);
	(*pp_rawdata_res)->p_beyond_jitter_limit_cnt = NULL;

	/*//free beyond accord limit buffer*/
	ret &= free_mem_in_heap((void *)(*pp_rawdata_res)->
				p_beyond_accord_limit_cnt);
	(*pp_rawdata_res)->p_beyond_accord_limit_cnt = NULL;

	/*//free beyond offset limit buffer*/
	ret &= free_mem_in_heap((void *)(*pp_rawdata_res)->
				p_beyond_offset_limit_cnt);
	(*pp_rawdata_res)->p_beyond_offset_limit_cnt = NULL;

	/*//free beyond key upper limit buffer*/
	ret &= free_mem_in_heap((void *)(*pp_rawdata_res)->
				p_beyond_key_upper_limit_cnt);
	(*pp_rawdata_res)->p_beyond_key_upper_limit_cnt = NULL;

	/*//free beyond key lower limit buffer*/
	ret &= free_mem_in_heap((void *)(*pp_rawdata_res)->
				p_beyond_key_lower_limit_cnt);
	(*pp_rawdata_res)->p_beyond_key_lower_limit_cnt = NULL;

	/*//free max tmp data buffer*/
	ret &= free_mem_block((void *)(*pp_rawdata_res)->
				p_max_raw_tmp_data->p_data_buf);
	(*pp_rawdata_res)->p_max_raw_tmp_data->p_data_buf = NULL;
	ret &= free_mem_in_heap((*pp_rawdata_res)->p_max_raw_tmp_data);
	(*pp_rawdata_res)->p_max_raw_tmp_data = NULL;

	/*//free min tmp data buffer*/
	ret &= free_mem_block((void *)(*pp_rawdata_res)->
				p_min_raw_tmp_data->p_data_buf);
	(*pp_rawdata_res)->p_min_raw_tmp_data->p_data_buf = NULL;
	ret &= free_mem_in_heap((*pp_rawdata_res)->p_min_raw_tmp_data);
	(*pp_rawdata_res)->p_min_raw_tmp_data = NULL;

	/*//free struct */
	ret &= free_mem_in_heap_p((void **)(pp_rawdata_res));

	/*//free current data*/
	p_curdata =
		(PST_CUR_RAWDATA) p_test_item->cur_data.ptr_cur_data;
	ret &= free_mem_block((void *)(p_curdata->p_data_buf));
	ret &= free_mem_in_heap_p((void **)
				&(p_test_item->cur_data.ptr_cur_data));

	return ret;
}

/*******************************************************************************
* Function Name	: alloc_diffdata_test_item_res
* Description	: alloc_diffdata_test_item_res
* Input			: PST_TEST_ITEM p_test_item
* Output		: PST_TEST_ITEM p_test_item
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
s32 alloc_diffdata_test_item_res(PST_TEST_ITEM p_test_item)
{
	s32 i = 0;
	s32 ret = 0;
	PST_TP_DEV p_tp_dev = p_test_item->p_tp_dev;
	u16 buf_size = p_tp_dev->sc_drv_num * p_tp_dev->sc_sen_num;
	u8 data_byte = 0;
	PST_DIFFDATA_TEST_RES *pp_diffdata_res = NULL;
	PST_CUR_DIFFDATA p_cur_diffdata = NULL;
	if (p_tp_dev->diffdata_option & _DATA_BIT_8) {
		data_byte = 1;
	} else if (p_tp_dev->diffdata_option & _DATA_BIT_16) {
		data_byte = 2;
	} else if (p_tp_dev->diffdata_option & _DATA_BIT_32) {
		data_byte = 4;
	}

	if (NULL != p_test_item->test_res.ptr_res) {
		return 1;
	}
	pp_diffdata_res =
		(PST_DIFFDATA_TEST_RES *) (&p_test_item->test_res.ptr_res);
	if (alloc_mem_in_heap
		((void **)pp_diffdata_res, sizeof(ST_DIFFDATA_TEST_RES))) {
		ret = alloc_item_res_header(p_test_item);
		if (0 == ret) {
			return ret;
		}
		/*//max data tmp buffer*/
		if (alloc_mem_in_heap
			((void **)
			&((*pp_diffdata_res)->p_max_diff_tmp_data),
			sizeof(ST_DIFF_TMP_DATA))) {
			buf_size = p_tp_dev->sc_drv_num * p_tp_dev->sc_sen_num;
			(*pp_diffdata_res)->p_max_diff_tmp_data->len =
				0;
			if (1 ==
				alloc_mem_block((void **)
						&((*pp_diffdata_res)->
							p_max_diff_tmp_data->
							p_data_buf),
						buf_size * sizeof(s32))) {
				for (i = 0; i < buf_size; i++) {
					(*pp_diffdata_res)->
						p_max_diff_tmp_data->
						p_data_buf[i] = 0;
				}
				(*pp_diffdata_res)->
					p_max_diff_tmp_data->len = buf_size;
			}
		}

		if (alloc_mem_in_heap
			((void **)&(p_test_item->cur_data.ptr_cur_data),
			sizeof(ST_CUR_DIFFDATA))) {
			buf_size = p_tp_dev->diffdata_len;
			p_cur_diffdata =
				(PST_CUR_DIFFDATA) p_test_item->cur_data.
				ptr_cur_data;
			p_cur_diffdata->cur_frame_id = 0;
			p_cur_diffdata->data_len = buf_size;
			if (alloc_mem_block
				((void **)&(p_cur_diffdata->p_data_buf),
				buf_size * data_byte)) {
				init_for_buffer((u8 *) (p_cur_diffdata->
						p_data_buf), buf_size * data_byte);
			}
		}
	}

	return ret;
}

/*******************************************************************************
* Function Name	: free_diffdata_test_item_res
* Description	: free_diffdata_test_item_res
* Input			: PST_TEST_ITEM p_test_item
* Output		: PST_TEST_ITEM p_test_item
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
s32 free_diffdata_test_item_res(PST_TEST_ITEM p_test_item)
{
	s32 ret = 1;
	PST_DIFFDATA_TEST_RES *pp_diffdata_res = NULL;
	PST_CUR_DIFFDATA p_curdata = NULL;
	if (NULL == p_test_item->test_res.ptr_res) {
		return 1;
	}
	/*//free struct */
	pp_diffdata_res =
		(PST_DIFFDATA_TEST_RES *) (&p_test_item->test_res.ptr_res);

	ret &= free_item_res_header(p_test_item);
	/*//free max tmp data buffer*/
	ret &= free_mem_block((void *)(*pp_diffdata_res)->
			p_max_diff_tmp_data->p_data_buf);
	(*pp_diffdata_res)->p_max_diff_tmp_data->p_data_buf = NULL;
	ret &= free_mem_in_heap((*pp_diffdata_res)->p_max_diff_tmp_data);
	(*pp_diffdata_res)->p_max_diff_tmp_data = NULL;

	ret = free_mem_in_heap_p((void **)(pp_diffdata_res));

	/*//free current data*/
	p_curdata =
		(PST_CUR_DIFFDATA) p_test_item->cur_data.ptr_cur_data;
	ret &= free_mem_block((void *)(p_curdata->p_data_buf));
	ret &= free_mem_in_heap_p((void **)
				&(p_test_item->cur_data.ptr_cur_data));

	return ret;
}

/*******************************************************************************
* Function Name	: alloc_short_test_item_res
* Description	: alloc_short_test_item_res
* Input			: PST_TEST_ITEM p_test_item
* Output		: PST_TEST_ITEM p_test_item
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
s32 alloc_short_test_item_res(PST_TEST_ITEM p_test_item)
{
	s32 ret = 0;
	PST_SHORT_TEST_RES *pp_test_res = NULL;
	if (NULL != p_test_item->test_res.ptr_res) {
		return 1;
	}
	pp_test_res =
		(PST_SHORT_TEST_RES *) (&p_test_item->test_res.ptr_res);
	if (alloc_mem_in_heap
		((void **)pp_test_res, sizeof(ST_SHORT_TEST_RES))) {
		ret = alloc_item_res_header(p_test_item);
		(*pp_test_res)->short_num = 0;
	}

	return ret;
}

/*******************************************************************************
* Function Name	: free_short_test_item_res
* Description	: free_short_test_item_res
* Input			: PST_TEST_ITEM p_test_item
* Output		: PST_TEST_ITEM p_test_item
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
s32 free_short_test_item_res(PST_TEST_ITEM p_test_item)
{
	s32 ret = 0;
	PST_SHORT_TEST_RES *pp_test_res = NULL;
	if (NULL == p_test_item->test_res.ptr_res) {
		return 1;
	}
	pp_test_res =
		(PST_SHORT_TEST_RES *) (&p_test_item->test_res.ptr_res);
	ret = free_item_res_header(p_test_item);
	ret = free_mem_in_heap_p((void **)pp_test_res);
	return ret;
}

/*******************************************************************************
* Function Name	: alloc_version_test_item_res
* Description	: alloc_version_test_item_res
* Input			: PST_TEST_ITEM p_test_item
* Output		: PST_TEST_ITEM p_test_item
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
s32 alloc_version_test_item_res(PST_TEST_ITEM p_test_item)
{
	s32 ret = 0;
	PST_VERSION_TEST_RES *pp_test_res = NULL;
	if (NULL != p_test_item->test_res.ptr_res) {
		return 1;
	}

	pp_test_res =
		(PST_VERSION_TEST_RES *) (&p_test_item->test_res.ptr_res);
	if (alloc_mem_in_heap
		((void **)pp_test_res, sizeof(ST_VERSION_TEST_RES))) {
		ret = alloc_item_res_header(p_test_item);
		(*pp_test_res)->cur_ver_data_len = 0;
	}

	return ret;
}

/*******************************************************************************
* Function Name	: free_version_test_item_res
* Description	: free_version_test_item_res
* Input			: PST_TEST_ITEM p_test_item
* Output		: PST_TEST_ITEM p_test_item
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
s32 free_version_test_item_res(PST_TEST_ITEM p_test_item)
{
	s32 ret = 0;
	PST_VERSION_TEST_RES *pp_test_res = NULL;
	if (NULL == p_test_item->test_res.ptr_res) {
		return 1;
	}
	pp_test_res =
		(PST_VERSION_TEST_RES *) (&p_test_item->test_res.ptr_res);
	ret = free_item_res_header(p_test_item);
	ret = free_mem_in_heap_p((void **)(pp_test_res));
	return ret;
}
/*******************************************************************************
* Function Name	: free_sleep_current_test_item_res
* Description	: free sleep current test item res
* Input			: PST_TEST_ITEM p_test_item
* Output		: PST_TEST_ITEM p_test_item
* Return		: s32(1:ok 0:fail)
*******************************************************************************/
s32 alloc_chip_cfg_proc_item_res(PST_TEST_ITEM p_test_item)
{

	s32 ret = 0;
	PST_CHIP_CONFIG_PROC_RES *pp_test_res = NULL;
	PST_CHIP_CONFIG_PROC_PARAM p_param = NULL;
	if (NULL != p_test_item->test_res.ptr_res) {
		return 1;
	}

	pp_test_res = (PST_CHIP_CONFIG_PROC_RES *) (&p_test_item->test_res.ptr_res);
	if (alloc_mem_in_heap
		((void **)pp_test_res, sizeof(ST_CHIP_CONFIG_PROC_RES))) {
		ret = alloc_item_res_header(p_test_item);
		p_param = (PST_CHIP_CONFIG_PROC_PARAM) p_test_item->param.ptr_param;
		if (NULL != p_param && 0 < p_param->cfg_len) {
			(*pp_test_res)->cfg_len = p_param->cfg_len;
			ret = alloc_mem_in_heap((void
						**)(&(*pp_test_res)->
							p_read_cfg),
						(*pp_test_res)->cfg_len *
							sizeof(u8));
		} else {
			(*pp_test_res)->cfg_len = 0;
			(*pp_test_res)->p_read_cfg = NULL;
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: free_cfg_check_test_item_res
* Description	: free sleep free_cfg_check_test_item_res test item res
* Input			: PST_TEST_ITEM p_test_item
* Output		: PST_TEST_ITEM p_test_item
* Return		: s32(1:ok 0:fail)
*******************************************************************************/
s32 free_chip_cfg_proc_item_res(PST_TEST_ITEM p_test_item)
{
	s32 ret = 0;
	PST_CHIP_CONFIG_PROC_RES *pp_test_res = NULL;
	if (NULL == p_test_item->test_res.ptr_res) {
		return 1;
	}
	pp_test_res = (PST_CHIP_CONFIG_PROC_RES *) (&p_test_item->test_res.ptr_res);
	ret = free_item_res_header(p_test_item);
	if (NULL != (*pp_test_res)->p_read_cfg) {
		ret = free_mem_in_heap_p((void **)&(*pp_test_res)->p_read_cfg);
	}
	ret = free_mem_in_heap_p((void **)(pp_test_res));
	return ret;
}

/*****************************************************************
* Function Name	: alloc_flash_test_item_res
* Description	: alloc_flash_test_item_res
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 alloc_flash_test_item_res(PST_TEST_ITEM p_test_item)
{
	s32 ret = 0;
	PST_FLASH_TEST_RES *pp_test_res = NULL;
	PST_FLASH_TEST_PARAM p_param = NULL;
	if (NULL != p_test_item->test_res.ptr_res) {
		return 1;
	}
	pp_test_res =
		(PST_FLASH_TEST_RES *) (&p_test_item->test_res.ptr_res);
	if (alloc_mem_in_heap
		((void **)pp_test_res, sizeof(ST_FLASH_TEST_RES))) {
		ret = alloc_item_res_header(p_test_item);

		p_param = (PST_FLASH_TEST_PARAM) p_test_item->param.ptr_param;
		if (NULL != p_param && 0 < p_param->cfg_len) {
			(*pp_test_res)->cfg_len = p_param->cfg_len;
			ret = alloc_mem_in_heap((void
						**)(&(*pp_test_res)->
							config_arr),
						(*pp_test_res)->cfg_len *
							sizeof(u8));
		} else {
			(*pp_test_res)->cfg_len = 0;
			(*pp_test_res)->config_arr = NULL;
		}
	}
	return ret;
}

/*****************************************************************
* Function Name	: free_flash_test_item_res
* Description	: free_flash_test_item_res
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 free_flash_test_item_res(PST_TEST_ITEM p_test_item)
{
	s32 ret = 0;
	PST_FLASH_TEST_RES *pp_test_res = NULL;
	if (NULL == p_test_item->test_res.ptr_res) {
		return 1;
	}
	pp_test_res = (PST_FLASH_TEST_RES *) (&p_test_item->test_res.ptr_res);
	ret = free_item_res_header(p_test_item);
	if (NULL != (*pp_test_res)->config_arr) {
		ret = free_mem_in_heap_p((void **)&(*pp_test_res)->config_arr);
	}

	ret = free_mem_in_heap_p((void **)(pp_test_res));
	return ret;
}

/*****************************************************************
* Function Name	: alloc_recover_cfg_item_res
* Description	: alloc_recover_cfg_item_res
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 alloc_recover_cfg_item_res(PST_TEST_ITEM p_test_item)
{
	s32 ret = 0;
	PST_RECOVER_CHIP_CFG_RES *pp_test_res = NULL;
	PST_RECOVER_CHIP_CFG_PARAM p_param =
		(PST_RECOVER_CHIP_CFG_PARAM) p_test_item->param.ptr_param;
	if (NULL != p_test_item->test_res.ptr_res) {
		return 1;
	}
	pp_test_res =
		(PST_RECOVER_CHIP_CFG_RES *) (&p_test_item->test_res.ptr_res);
	if (alloc_mem_in_heap
		((void **)pp_test_res, sizeof(ST_RECOVER_CHIP_CFG_RES))) {
		ret = alloc_item_res_header(p_test_item);
		if (NULL != p_param) {
			(*pp_test_res)->cfg_len = p_param->cfg_len;
			ret = alloc_mem_in_heap((void **)&(*pp_test_res)->
					config_arr,
					(*pp_test_res)->cfg_len *
					sizeof(u8));
		} else {
			(*pp_test_res)->cfg_len = 0;
			(*pp_test_res)->config_arr = NULL;
		}
	}
	return ret;
}

/*****************************************************************
* Function Name	: free_revover_cfg_item_res
* Description	: free_revover_cfg_item_res
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 free_recover_cfg_item_res(PST_TEST_ITEM p_test_item)
{
	s32 ret = 0;
	PST_RECOVER_CHIP_CFG_RES *pp_test_res = NULL;
	if (NULL == p_test_item->test_res.ptr_res) {
		return 1;
	}
	pp_test_res =
		(PST_RECOVER_CHIP_CFG_RES *) (&p_test_item->test_res.ptr_res);

	ret = free_item_res_header(p_test_item);
	if (NULL != (*pp_test_res)->config_arr) {
		ret = free_mem_in_heap_p((void **)&(*pp_test_res)->config_arr);
	}
	ret = free_mem_in_heap_p((void **)(pp_test_res));
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