/******************** (C) COPYRIGHT 2016 Goodix ****************************
* File Name          : version_test.c
* Author             : Bob Huang
* Version            : V1.0.0
* Date               : 07/26/2017
* Description        : test touch panel version or update firmware
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#include "test_item_def.h"
#include "tp_version_test.h"
#include "tp_dev_control.h"
#include "board_opr_interface.h"
#include "tp_test_general_error_code_def.h"
#include "test_proc.h"

#ifdef __cplusplus
extern "C" {
#endif

static s32 compare_version_equal(PST_TEST_ITEM p_test_item);
static s32 compare_version_equal_greater(PST_TEST_ITEM p_test_item);
static s32 compare_version(PST_TEST_ITEM p_test_item);

/*******************************************************************************
* Function Name	: fw_version_test
* Description	: firmware version test
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: none
* Return		: u8(0:test error 1:ok)
*******************************************************************************/
extern s32 fw_version_test(IN_OUT ptr32 p_data)
{
	PST_TEST_ITEM p_test_item = NULL;
	PST_VERSION_PARAM p_ver_param = NULL;
	PST_VERSION_TEST_RES p_test_res = NULL;
	u16 err_code = VERSION_TEST_NONE_ERROR;
	u8 i = 0;
	s32 ret = 0;

	p_test_item = (PST_TEST_ITEM) p_data;
	/*-----------------Judge parameter----------------*/
	if (0 == judge_test_item_legal((PST_TEST_ITEM) p_data)) {
		ret = 0;
		goto TEST_ITEM_END;
	}

	p_ver_param = (PST_VERSION_PARAM) p_test_item->param.ptr_param;
	p_test_res =
		(PST_VERSION_TEST_RES) p_test_item->test_res.ptr_res;

	/*callback function test before*/
	if (NULL != p_test_item->ptr_test_item_before_func) {
		ret =
			p_test_item->ptr_test_item_before_func(p_test_item->
							ptr_test_item_before_func_param);
		if (0 == ret) {
			board_print_error
				("[version test]callback function test before error!\n");
			err_code = TEST_BEFORE_FUNC_FAIL;
			goto TEST_ITEM_END;
		}
	}

	p_test_res->cur_ver_data_len = p_ver_param->ver_param_len;
	p_test_res->item_res_header.test_status = TEST_START;
	p_test_res->item_res_header.test_result = TEST_NG;
	p_test_res->item_res_header.test_item_err_code.
		ptr_err_code_set[0] = VERSION_TEST_NONE_ERROR;
	for (i = 0; i < p_ver_param->ver_param_len; i++) {	/*//clear res*/
		p_test_res->cur_ver_data[i] = 0x00;
	}

	/*//force update*/
	if (1 == p_ver_param->b_force_update) {
		/*//update fw*/
	}
	/*//compare version*/
	ret = compare_version(p_test_item);
	if (ret == 0) {
		if (1 == p_ver_param->b_force_update) {
			p_test_res->item_res_header.test_item_err_code.
				ptr_err_code_set[0] =
				VERSION_TEST_VERSIONMATCH_FAIL_AFTER;
		}
		p_test_res->item_res_header.test_result = TEST_NG;
		p_test_res->item_res_header.test_status = TEST_ABORTED;
		board_print_debug("compare fw version error!\n");
		err_code =
			p_test_res->item_res_header.test_item_err_code.
			ptr_err_code_set[0];
		/*//goto TEST_ITEM_END;*/
	}

	if (p_test_res->item_res_header.test_result == TEST_OK) {
		p_test_res->item_res_header.test_status = TEST_FINISH;
		goto TEST_ITEM_END;
	} else {
		if (0 == p_ver_param->b_auto_update) {
			p_test_res->item_res_header.test_status =
				TEST_FINISH;
			goto TEST_ITEM_END;
		} else if (0 == p_ver_param->b_force_update) {
			/*//compare version again*/
			ret = compare_version(p_test_item);
			if (0 == ret) {
				if (1 == p_ver_param->b_force_update) {
					p_test_res->item_res_header.
						test_item_err_code.
						ptr_err_code_set[0] =
						VERSION_TEST_VERSIONMATCH_FAIL_AFTER;
				}
				p_test_res->item_res_header.
					test_result = TEST_NG;
				p_test_res->item_res_header.
					test_status = TEST_ABORTED;
				board_print_error
					("auto update:compare fw version error!\n");
				err_code =
					p_test_res->item_res_header.
					test_item_err_code.
					ptr_err_code_set[0];
				goto TEST_ITEM_END;
			}
		}
	}

	p_test_res->item_res_header.test_status = TEST_FINISH;
TEST_ITEM_END:
	if (NULL != p_test_item->ptr_test_item_finished_func) {
		ret = p_test_item->
			ptr_test_item_finished_func(p_test_item->
						ptr_test_item_finished_func_param);
		if (ret == 0) {
			board_print_error
				("[version test]callback function after test error!\n");
			err_code = TEST_FINISH_FUNC_FAIL;
		}
	}

	if (err_code == VERSION_TEST_NONE_ERROR) {
		ret = 1;
	} else {
		ret = 0;
	}
	return ret;
}

/*******************************************************************************
* Function Name	: compare_version
* Description	: compare version
* Input			: PST_TEST_ITEM p_test_item
* Output		: none
* Return		: s32(0:error 1:handled)
*******************************************************************************/
static s32 compare_version(PST_TEST_ITEM p_test_item)
{
	u8 i = 0, retry_cnt = 3;
	s32 ret = 0;
	s32 cmp_ret = 0;
	PST_VERSION_PARAM p_ver_param =
		(PST_VERSION_PARAM) p_test_item->param.ptr_param;
	PST_VERSION_TEST_RES p_test_res =
		(PST_VERSION_TEST_RES) p_test_item->test_res.ptr_res;
	/*//read ic version */
	for (i = 0; i < retry_cnt; i++) {
		ret =
			chip_reg_read(p_test_item->p_tp_dev,
				p_ver_param->ic_ver_addr,
				p_test_res->cur_ver_data,
				p_ver_param->ver_param_len);
		if (1 == ret) {
			break;
		}
	}
	if (i >= retry_cnt) {
		board_print_error("%s\n",
				"in compare version, read version fail");
		goto COMPARE_END;
	}

	if (p_ver_param->ver_cmp_opt == VERSION_CMP_EQUAL) {
		cmp_ret = compare_version_equal(p_test_item);
	} else if (p_ver_param->ver_cmp_opt ==
			VERSION_CMP_GREATOR_EQUAL) {
		cmp_ret = compare_version_equal_greater(p_test_item);
	}

	if (0 == cmp_ret) {
		board_print_debug("%s\n",
			"in compare version, version match fail");
		p_test_res->item_res_header.test_item_err_code.
			ptr_err_code_set[0] =
			VERSION_TEST_VERSION_MATCH_FAIL;
		p_test_res->item_res_header.test_result = TEST_NG;
		ret = 0;
		goto COMPARE_END;
	} else {
		p_test_res->item_res_header.test_result = TEST_OK;
		p_test_res->item_res_header.test_item_err_code.
			ptr_err_code_set[0] = VERSION_TEST_NONE_ERROR;
		ret = 1;
	}
COMPARE_END:
	return ret;
}

/*******************************************************************************
* Function Name	: compare_version_equal
* Description	: compare_version_equal
* Input			: PST_TEST_ITEM p_test_item
* Output		: none
* Return		: s32(0:error 1:cmp ok)
*******************************************************************************/
static s32 compare_version_equal(IN PST_TEST_ITEM p_test_item)
{
	u8 i = 0;
	s32 ret = 0;
	PST_VERSION_PARAM p_ver_param =
		(PST_VERSION_PARAM) p_test_item->param.ptr_param;
	PST_VERSION_TEST_RES p_test_res =
		(PST_VERSION_TEST_RES) p_test_item->test_res.ptr_res;

	/*//compare version*/
	for (i = 0; i < p_ver_param->ver_param_len; i++) {
		if (p_test_res->cur_ver_data[i] !=
			p_ver_param->ver_param_str[i]) {
			break;
		}
	}

	if (i == p_ver_param->ver_param_len) {
		ret = 1;
	}
	return ret;
}

/*******************************************************************************
* Function Name	: compare_version_equal_greater
* Description	: compare_version_equal_greater
* Input			: PST_TEST_ITEM p_test_item
* Output		: none
* Return		: s32(0:error 1:cmp ok)
*******************************************************************************/
static s32 compare_version_equal_greater(IN PST_TEST_ITEM p_test_item)
{
	s32 ret = 0;
	u8 i = 0;
	PST_VERSION_PARAM p_ver_param =
		(PST_VERSION_PARAM) p_test_item->param.ptr_param;
	PST_VERSION_TEST_RES p_test_res =
		(PST_VERSION_TEST_RES) p_test_item->test_res.ptr_res;
	ret = compare_version_equal(p_test_item);
	if (1 == ret) {
		return ret;
	}
	/*//compare version
	//do not care about public version and special version updating*/
	for (i = 0; i < p_ver_param->ver_param_len; i++) {
		if (p_test_res->cur_ver_data[i] <
			p_ver_param->ver_param_str[i]) {
			return 0;
		}
		if (p_test_res->cur_ver_data[i] >
			p_ver_param->ver_param_str[i]) {
			return 1;
		}
	}

	return 1;
}
#ifdef __cplusplus
}
#endif

#endif
