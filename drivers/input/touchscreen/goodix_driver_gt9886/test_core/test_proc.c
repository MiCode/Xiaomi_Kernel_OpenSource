/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name          : test_proc.c
* Author             : Bob Huang
* Version            : V1.0.0
* Date               : 07/26/2017
* Description        : structs in this file also exist in pc,it is an interface
					   of test item
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#include "test_proc.h"

#include "tp_open_test.h"
#include "tp_short_test.h"
#include "tp_version_test.h"
#include "tp_additional_option.h"
#include "simple_mem_manager.h"
#include "tp_test_general_error_code_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
* Function Name	: get_arr_id_from_sub_item
* Description	: get array index from sub test item array
* Input			: PST_TEST_ITEM p_test_item
* Output		: none
* Return		: s32(array id)
*******************************************************************************/
extern s32 get_arr_id_from_sub_item(IN PST_TEST_ITEM p_test_item,
				IN u8 sub_item_id)
{
	s32 ret = 0xFF;
	u8 i = 0;
	PST_SUB_TEST_ITEM_ID_SET p_sub_id_set = NULL;
	p_sub_id_set = p_test_item->p_sub_id_set;
	for (i = 0; i < p_sub_id_set->len; i++) {
		if (p_sub_id_set->p_id_set[i] == sub_item_id) {
			ret = i;
		}
	} return ret;
}

/*******************************************************************************
* Function Name	: test_proc_func
* Description	: test process functiton
* Input			: PST_TEST_PROC_DATA p_test_proc_data
* Output		: PST_TEST_PROC_DATA p_test_proc_data
* Return		: s32(0:fail 1:ok) If all functions in test_proc_func excuting ok
				then the return value is 1
*******************************************************************************/
extern s32 test_proc_func(IN_OUT PST_TEST_PROC_DATA p_test_proc_data)
{

	u16 i = 0;
	s32 res = 1;
	/*off line run*/
	p_test_proc_data->test_proc_status = TEST_START;
	if (NULL != p_test_proc_data->p_test_items_before_func_ptr) {
		res &= p_test_proc_data->
			p_test_items_before_func_ptr(p_test_proc_data->
					p_test_items_before_func_ptr_param);
	}
	/*(2)run all test items*/
	p_test_proc_data->test_proc_status = TEST_DOING;
	for (i = 0; i < p_test_proc_data->test_item_num; i++) {
		if (1 == p_test_proc_data->test_break_flag) {
			p_test_proc_data->test_proc_status =
				TEST_ABORTED;
			break;
		}
		if (NULL != p_test_proc_data->pp_test_item_arr[i] &&
			NULL !=
			p_test_proc_data->pp_test_item_arr[i]->
			ptr_test_func) {
			res &= p_test_proc_data->pp_test_item_arr[i]->
				ptr_test_func(p_test_proc_data->
					pp_test_item_arr[i]);
		}
	}
	if (1 == p_test_proc_data->test_break_flag) {
		p_test_proc_data->test_proc_status = TEST_ABORTED;
	}

	if (NULL != p_test_proc_data->p_test_items_finished_func_ptr) {
		res &= p_test_proc_data->
			p_test_items_finished_func_ptr(p_test_proc_data->
							p_test_items_finished_func_ptr_param);
	}
	p_test_proc_data->test_proc_status = TEST_FINISH;
	return res;
}

/*******************************************************************************
* Function Name	: register_test_item_func
* Description	: register test function.you must modify this function,while you
				add some tes functions.
* Input			: PST_TEST_PROC_DATA p_test_proc_data
* Output		: PST_TEST_PROC_DATA p_test_proc_data
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 register_test_item_func(IN_OUT PST_TEST_PROC_DATA
				p_test_proc_data)
{
	s32 ret = 1;
	u16 i = 0;
	PST_TEST_ITEM *pp_test_item_arr =
		p_test_proc_data->pp_test_item_arr;
	u16 item_num = p_test_proc_data->test_item_num;

	for (i = 0; i < item_num; i++) {
		switch (pp_test_item_arr[i]->test_item_id) {
			/*----------------------tp open test-----------------------*/
		case TP_RAWDATA_TEST_ITEMS_SET_ID:
			{
				pp_test_item_arr[i]->ptr_test_func =
					rawdata_test_sets_func;
			}
			break;

			/*----------------------hardware test-----------------------*/
		case TP_SHORT_TEST_ITEM_ID:
			{
				pp_test_item_arr[i]->ptr_test_func =
					short_test_func;
			}
			break;

			/*----------------------firmware test-----------------------*/
		case TP_VERSION_TEST_ITEM_ID:
			{
				pp_test_item_arr[i]->ptr_test_func =
					fw_version_test;
			}
			break;

			/*----------------------other test-----------------------*/
		case TP_CHIP_CFG_PROC_ITEM_ID:
			{
				pp_test_item_arr[i]->ptr_test_func =
					tp_chip_cfg_proc;
			}
			break;
		case TP_FLASH_TEST_ITEM_ID:
			{
				pp_test_item_arr[i]->ptr_test_func =
					tp_flash_test;
			}
			break;

		case TP_RECOVER_CFG_ITEM_ID:
			{
				pp_test_item_arr[i]->ptr_test_func =
					tp_recover_cfg_to_chip;
			}
			break;

		default:
			{
				pp_test_item_arr[i]->ptr_test_func =
					NULL;
			}
			break;
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: get_test_item_from_proc_data
* Description	: get test item from test proce data
* Input			: PST_TEST_PROC_DATA p_test_proc_data
* Input			: u16 test_item_id
* Output		: PST_TEST_ITEM* pp_test_item
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 get_test_item_from_proc_data(IN PST_TEST_PROC_DATA
		p_test_proc_data, IN u16 test_item_id, OUT PST_TEST_ITEM *
		pp_test_item)
{
	u16 i = 0;
	s32 ret = 0;
	PST_TEST_ITEM *pp_test_item_arr =
		p_test_proc_data->pp_test_item_arr;
	u16 item_num = p_test_proc_data->test_item_num;

	for (i = 0; i < item_num; i++) {
		if (pp_test_item_arr[i]->test_item_id == test_item_id) {
			*pp_test_item = pp_test_item_arr[i];
			ret = 1;
			break;
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: test_before_func_fail_proc
* Description	: test_before_func_fail_proc
* Input			: P_TEST_ITEM_RES p_test_res_head
* Output		: P_TEST_ITEM_RES p_test_res_head
* Return		: none
*******************************************************************************/
extern void test_before_func_fail_proc(IN_OUT P_TEST_ITEM_RES
		p_test_res_head)
{
	if (p_test_res_head->test_item_err_code.len >= 1) {
		p_test_res_head->test_item_err_code.
			ptr_err_code_set[0] = TEST_BEFORE_FUNC_FAIL;
	}
	p_test_res_head->test_status = TEST_ABORTED;
	p_test_res_head->test_result = TEST_NG;
	board_print_error
		("p_test_item->ptr_test_item_before_func fail\n");
}

/*******************************************************************************
* Function Name	: 0 == judge_test_item_legal
* Description	: 0 == judge_test_item_legal
* Input			: PST_TEST_ITEM p_test_item
* Output		: none
* Return		: 1:OK 0:Fail
*******************************************************************************/
extern s32 judge_test_item_legal(PST_TEST_ITEM p_test_item)
{
	s32 ret = 1;

	/*-----------------Judge parameter----------------*/
	if (NULL == p_test_item || NULL == p_test_item->param.ptr_param
		|| NULL == p_test_item->test_res.ptr_res
		|| NULL == p_test_item->p_tp_dev
		|| NULL == p_test_item->ptr_test_proc_data) {
		ret = 0;
	}
	return ret;
}

/*******************************************************************************
* Function Name	: start_test_interface
* Description	: start test interface
* Input			: ptr32 p_data(must cast to 'PST_TEST_PROC_DATA')
* Output		: ptr32 p_data
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 start_test_interface(IN_OUT ptr32 ptr)
{
	s32 ret = 1;
	ret = test_proc_func((PST_TEST_PROC_DATA) ptr);
	return ret;
}

/*******************************************************************************
* Function Name	: stop_test_interface
* Description	: stop test interface
* Input			: ptr32 p_data(must cast to 'PST_TEST_PROC_DATA')
* Output		: ptr32 p_data
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 stop_test_interface(IN_OUT ptr32 ptr)
{
	s32 ret = 1;
	u16 i = 0;
	PST_TEST_PROC_DATA p_test_proc_data = (PST_TEST_PROC_DATA) ptr;
	P_TEST_ITEM_RES p_test_res_header = NULL;
	p_test_proc_data->test_proc_status = TEST_ABORTED;

	/*set all test item */
	for (i = 0; i < p_test_proc_data->test_item_num; i++) {
		if ((NULL != p_test_proc_data->pp_test_item_arr[i]) &&
			(NULL !=
			(p_test_proc_data->pp_test_item_arr[i])->test_res.
			ptr_res)) {
			p_test_res_header =
				(P_TEST_ITEM_RES) (p_test_proc_data->
					pp_test_item_arr[i])->
				test_res.ptr_res;
			p_test_res_header->test_status = TEST_ABORTED;
		}
	}
	return ret;
}

#ifdef __cplusplus
}
#endif

#endif
