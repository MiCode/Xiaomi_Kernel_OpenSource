/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name          : tp_additional_option.h
* Author             : Bob Huang
* Version            : V1.0.0
* Date               : 10/26/2017
* Description        : this file offer some functions which are called close to test end
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#include "tp_additional_option.h"
#include "tp_dev_control.h"
#include "simple_mem_manager.h"
#include "tp_version_test.h"
#include "test_proc.h"
#include "tp_test_general_error_code_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
* Function Name	: tp_send_cfg_to_chip
* Description	: tp_send_cfg_to_chip
* Input 		: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: none
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
extern s32 tp_chip_cfg_proc(IN_OUT ptr32 p_data)
{
	s32 ret = 1;
	u16 i = 0;
	PST_CHIP_CONFIG_PROC_PARAM p_param = NULL;
	PST_CHIP_CONFIG_PROC_RES p_test_res = NULL;
	PST_TP_DEV p_dev = NULL;
	PST_TEST_ITEM p_test_item = (PST_TEST_ITEM) p_data;

	/*-----------------Judge parameter----------------*/
	if (0 == judge_test_item_legal((PST_TEST_ITEM) p_data)) {
		ret = 0;
		goto SEN_CFG_END;
	}

	p_param = (PST_CHIP_CONFIG_PROC_PARAM) p_test_item->param.ptr_param;
	p_test_res = (PST_CHIP_CONFIG_PROC_RES) p_test_item->test_res.ptr_res;
	p_dev = p_test_item->p_tp_dev;

	if (NULL != p_test_item->ptr_test_item_before_func) {
		ret &= p_test_item->ptr_test_item_before_func(p_test_item->
							ptr_test_item_before_func_param);
		if (0 == ret) {
			test_before_func_fail_proc(&p_test_res->item_res_header);
			goto SEN_CFG_END;
		}
	}

	p_test_res->item_res_header.test_item_err_code.
		ptr_err_code_set[0] = CFG_CHECK_TEST_NONE_ERROR;
	p_test_res->item_res_header.test_status = TEST_START;
	p_test_res->item_res_header.test_result = TEST_OK;
	/*clear config in res*/
	for (i = 0; i < p_test_res->cfg_len; i++) {
		p_test_res->p_read_cfg[i] = 0x00;
	}

	if (p_param->option == CHIP_CFG_SEND) {
		/*write cfg*/
		ret &= force_update_cfg(p_dev, p_param->config, p_param->cfg_len);
		if (0 == ret) {
			p_test_res->item_res_header.test_item_err_code.
				ptr_err_code_set[0] =
				CFG_CHECK_TEST_UPDATE_FAIL;
			p_test_res->item_res_header.test_result =
				TEST_NG;
			board_print_error
				("[send config test]update config error!\n");
			goto SEN_CFG_END;
		}
	}
	/*read cfg*/
	if (read_cfg_arr
		(p_dev, p_test_res->p_read_cfg, p_test_res->cfg_len) == 0) {
		ret = 0;
	} else {
		ret &= 1;
	}
	if (0 == ret) {
		p_test_res->item_res_header.test_item_err_code.
			ptr_err_code_set[0] = CFG_CHECK_TEST_READ_FAIL;
		p_test_res->item_res_header.test_result = TEST_NG;
		p_test_res->item_res_header.test_status = TEST_ABORTED;
		board_print_error
			("[send config test]read config error!\n");
		goto SEN_CFG_END;
	}
	p_test_res->item_res_header.test_status = TEST_FINISH;
SEN_CFG_END:
	if (p_test_item->ptr_test_item_finished_func) {
		ret &= p_test_item->
			ptr_test_item_finished_func(p_test_item->
						ptr_test_item_finished_func_param);
	}
	return ret;
}

/*******************************************************************************
* Function Name	: tp_send_spec_cfg_to_chip
* Description	: tp send spec cfg to chip
* Input 		: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: none
* Return		: s32(0:error 1:Ok)
*******************************************************************************/
extern s32 tp_send_spec_cfg_to_chip(IN_OUT ptr32 p_data)
{
	s32 ret = 1;
	u16 i = 0;
	PST_SPEC_CONFIG_PARAM p_param = NULL;
	PST_SEND_SPEC_CONFIG_RES p_test_res = NULL;
	PST_TP_DEV p_dev = NULL;
	PST_TEST_ITEM p_test_item = (PST_TEST_ITEM) p_data;

	/*-----------------Judge parameter----------------*/
	if (0 == judge_test_item_legal((PST_TEST_ITEM) p_data)) {
		ret = 0;
		goto SEN_SPC_CFG_END;
	}

	p_param = (PST_SPEC_CONFIG_PARAM) p_test_item->param.ptr_param;
	p_test_res =
		(PST_SEND_SPEC_CONFIG_RES) p_test_item->test_res.ptr_res;
	p_dev = p_test_item->p_tp_dev;

	if (NULL != p_test_item->ptr_test_item_before_func) {
		ret &= p_test_item->ptr_test_item_before_func(p_test_item->
							ptr_test_item_before_func_param);
		if (0 == ret) {
			test_before_func_fail_proc(&p_test_res->
						item_res_header);
			goto SEN_SPC_CFG_END;
		}
	}

	p_test_res->item_res_header.test_item_err_code.
		ptr_err_code_set[0] = SPEC_CONFIG_ERROR_CODE_NONE;
	p_test_res->item_res_header.test_status = TEST_START;
	p_test_res->item_res_header.test_result = TEST_OK;
	/*clear config in res*/
	for (i = 0; i < p_test_res->read_cfg_len; i++) {
		p_test_res->read_config[i] = 0x00;
	}

	if (1 > p_test_res->read_cfg_len) {
		ret = 0;
		goto SEN_SPC_CFG_END;
	}
	/*write config to chip*/
	ret &= force_update_cfg(p_dev, p_param->config, p_param->cfg_len);
	if (0 == ret) {
		p_test_res->item_res_header.test_item_err_code.
			ptr_err_code_set[0] = SPEC_CONFIG_UPDATE_FAIL;
		p_test_res->item_res_header.test_result = TEST_NG;
		p_test_res->item_res_header.test_status = TEST_ABORTED;
		board_print_error
			("[send special config]update config error!\n");
		goto SEN_SPC_CFG_END;
	}
	/*read config from chip*/
	if (read_cfg_arr
		(p_dev, p_test_res->read_config,
		p_test_res->read_cfg_len) == 0) {
		ret = 0;
	} else {
		ret &= 1;
	}
	if (0 == ret) {
		p_test_res->item_res_header.test_item_err_code.
			ptr_err_code_set[0] = SPEC_CONFIG_READ_FAIL;
		p_test_res->item_res_header.test_result = TEST_NG;
		p_test_res->item_res_header.test_status = TEST_ABORTED;
		board_print_error
			("[send special config]read config error!\n");
		goto SEN_SPC_CFG_END;
	}
	/*check cfg*/
	ret &= cmp_cfg(p_dev, p_test_res->read_config, p_param->config,
			p_param->cfg_len);
	if (0 == ret) {
		p_test_res->item_res_header.test_item_err_code.
			ptr_err_code_set[0] = SPEC_CONFIG_CHECK_FAIL;
		p_test_res->item_res_header.test_result = TEST_NG;
		ret = 1;
		board_print_info
			("[send special config]cfg is not equal with chip config!\n");
	}
	p_test_res->item_res_header.test_status = TEST_FINISH;

SEN_SPC_CFG_END:
	if (p_test_item->ptr_test_item_finished_func) {
		ret &= p_test_item->ptr_test_item_finished_func(p_test_item->
						ptr_test_item_finished_func_param);
	}
	return ret;
}

/*******************************************************************************
* Function Name	: tp_flash_test
* Description	: tp_flash_test
* Input 		: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: none
* Return		: s32(0:error 1:Ok)
*******************************************************************************/
extern s32 tp_flash_test(IN_OUT ptr32 p_data)
{
	s32 ret = 1;
	u16 i = 0;
	PST_FLASH_TEST_PARAM p_param = NULL;
	PST_FLASH_TEST_RES p_test_res = NULL;
	PST_TP_DEV p_dev = NULL;
	PST_TEST_ITEM p_test_item = (PST_TEST_ITEM) p_data;

	/*-----------------Judge parameter----------------*/
	if (0 == judge_test_item_legal((PST_TEST_ITEM) p_data)) {
		ret = 0;
		goto FLASH_TEST_END;
	}

	p_param = (PST_FLASH_TEST_PARAM) p_test_item->param.ptr_param;
	p_test_res = (PST_FLASH_TEST_RES) p_test_item->test_res.ptr_res;
	p_dev = p_test_item->p_tp_dev;

	if (NULL != p_test_item->ptr_test_item_before_func) {
		ret &= p_test_item->ptr_test_item_before_func(p_test_item->
						ptr_test_item_before_func_param);
		if (0 == ret) {
			test_before_func_fail_proc(&p_test_res->item_res_header);
			goto FLASH_TEST_END;
		}
	}

	p_test_res->item_res_header.test_item_err_code.
		ptr_err_code_set[0] = FLASH_TEST_ERROR_CODE_NONE;
	p_test_res->item_res_header.test_status = TEST_START;
	p_test_res->item_res_header.test_result = TEST_OK;
	/*clear config in res*/
	for (i = 0; i < p_test_res->cfg_len; i++) {
		p_test_res->config_arr[i] = 0x00;
	}

	/*Write cfg to ram*/
	if ((p_param->option & FLASH_TEST_SEND_CFG_OPT) ==
		FLASH_TEST_SEND_CFG_OPT) {
		/*write cfg*/
		ret &= force_update_cfg(p_dev, p_param->config_arr, p_param->cfg_len);
		if (0 == ret) {
			p_test_res->item_res_header.test_item_err_code.
				ptr_err_code_set[0] =
				FLASH_TEST_CFG_WRITE_ERROR;
			p_test_res->item_res_header.test_result =
				TEST_NG;
			board_print_error
				("[flash test]update config error!\n");
			goto FLASH_TEST_END;
		}
	}
	/*wait firmware transfer config to flash from ram*/
	if ((p_param->option & FLASH_TEST_DEFAULT_DELAY) ==
		FLASH_TEST_DEFAULT_DELAY) {
		cfg_to_flash_delay(p_dev);
	} else {
		board_delay_ms(p_param->delay_time);
	}

	/*Reset chip and wait firmware initialization*/
	ret &= reset_chip_proc(p_dev, RESET_TO_AP);
	if (0 == ret) {
		p_test_res->item_res_header.test_item_err_code.
			ptr_err_code_set[0] = FLASH_TEST_RST_FAIL_ERROR;
		p_test_res->item_res_header.test_result = TEST_NG;
		p_test_res->item_res_header.test_status = TEST_ABORTED;
		goto FLASH_TEST_END;
	}
	/*read cfg*/
	if (read_cfg_arr
		(p_dev, p_test_res->config_arr, p_test_res->cfg_len) == 0) {
		ret = 0;
	} else {
		ret &= 1;
	}
	if (0 == ret) {
		p_test_res->item_res_header.test_item_err_code.
			ptr_err_code_set[0] = FLASH_TEST_CFG_READ_ERROR;
		p_test_res->item_res_header.test_result = TEST_NG;
		p_test_res->item_res_header.test_status = TEST_ABORTED;
		board_print_error("[flash test]read config error!\n");
		goto FLASH_TEST_END;
	}
	p_test_res->item_res_header.test_status = TEST_FINISH;

FLASH_TEST_END:
	if (p_test_item->ptr_test_item_finished_func) {
		ret &= p_test_item->ptr_test_item_finished_func(p_test_item->
						ptr_test_item_finished_func_param);
	}
	return ret;
}

/*******************************************************************************
* Function Name	: tp_recover_cfg_to_chip
* Description	: tp_recover_cfg_to_chip
* Input 		: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: none
* Return		: s32(0:error 1:Ok)
*******************************************************************************/
extern s32 tp_recover_cfg_to_chip(IN_OUT ptr32 p_data)
{
	s32 ret = 1;
	PST_RECOVER_CHIP_CFG_PARAM p_param = NULL;
	PST_RECOVER_CHIP_CFG_RES p_test_res = NULL;
	PST_TP_DEV p_dev = NULL;
	PST_TEST_ITEM p_test_item = (PST_TEST_ITEM) p_data;

	/*-----------------Judge parameter----------------*/
	if (0 == judge_test_item_legal((PST_TEST_ITEM) p_data)) {
		ret = 0;
		goto RECOVER_CFG_END;
	}

	p_param = (PST_RECOVER_CHIP_CFG_PARAM) p_test_item->param.ptr_param;
	p_test_res = (PST_RECOVER_CHIP_CFG_RES) p_test_item->test_res.ptr_res;
	p_dev = p_test_item->p_tp_dev;

	if (NULL != p_test_item->ptr_test_item_before_func) {
		ret &= p_test_item->ptr_test_item_before_func(p_test_item->
						ptr_test_item_before_func_param);
		if (0 == ret) {
			test_before_func_fail_proc(&p_test_res->item_res_header);
			goto RECOVER_CFG_END;
		}
	}

	p_test_res->item_res_header.test_item_err_code.
		ptr_err_code_set[0] = RECOVER_CFG_ERROR_CODE_NONE;
	p_test_res->item_res_header.test_status = TEST_START;
	p_test_res->item_res_header.test_result = TEST_OK;

	/*Write cfg to ram*/
	ret &= force_update_cfg(p_dev, p_param->config_arr, p_param->cfg_len);
	if (0 == ret) {
		p_test_res->item_res_header.test_item_err_code.
			ptr_err_code_set[0] = RECOVER_CFG_CFG_WRITE_ERROR;
		p_test_res->item_res_header.test_result = TEST_NG;
		board_print_error
			("[recover config test]update config error!\n");
		goto RECOVER_CFG_END;
	}
	/*read cfg*/
	if (read_cfg_arr
		(p_dev, p_test_res->config_arr, p_test_res->cfg_len) == 0) {
		ret = 0;
	} else {
		ret &= 1;
	}
	if (0 == ret) {
		p_test_res->item_res_header.test_item_err_code.
			ptr_err_code_set[0] = RECOVER_CFG_CFG_READ_ERROR;
		p_test_res->item_res_header.test_result = TEST_NG;
		p_test_res->item_res_header.test_status = TEST_ABORTED;
		board_print_error
			("[recover config test]read config error!\n");
		goto RECOVER_CFG_END;
	}
	/*check cfg*/
	ret &= cmp_cfg(p_dev, p_test_res->config_arr, p_param->config_arr,
			p_param->cfg_len);
	if (0 == ret) {
		p_test_res->item_res_header.test_item_err_code.
			ptr_err_code_set[0] = RECOVER_CFG_CMP_ERROR;
		p_test_res->item_res_header.test_result = TEST_NG;
		ret = 1;
		board_print_info
			("[recover config test]cfg is not equal with chip config!\n");
	}
	p_test_res->item_res_header.test_status = TEST_FINISH;

RECOVER_CFG_END:
	if (p_test_item->ptr_test_item_finished_func) {
		ret &= p_test_item->ptr_test_item_finished_func(p_test_item->
						ptr_test_item_finished_func_param);
	}
	return ret;
}

#ifdef __cplusplus
}
#endif

#endif
