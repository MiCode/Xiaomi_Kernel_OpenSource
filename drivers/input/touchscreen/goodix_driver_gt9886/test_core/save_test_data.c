/******************** (C) COPYRIGHT 2018 Goodix ********************
* File Name          : save_test_data.cpp
* Author             :
* Version            : V1.0.0
* Date               : 27/03/2018
* Description        : save test data
*******************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

#include "save_test_data.h"

#include "tp_dev_control.h"
#include "tp_open_test.h"
#include "tp_short_test.h"
#include "tp_version_test.h"
#include "tp_additional_option.h"
#include "simple_mem_manager.h"
#include "test_order.h"
#include "generic_func.h"
#include "board_opr_interface.h"
/*******************************************************************************
* Function Name	: print_rawdata_test_res
* Description	: print rawdata test reult
* Input			: ptr32 p_data (You must cast it to type PST_TEST_ORDER )
* Output		: ptr32 p_data
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
extern s32 print_rawdata_test_res(ptr32 p_data)
{
	s32 ret = 1;
	u16 i = 0;
	PST_TEST_ITEM p_test_item = NULL;
	PST_RAWDATA_TEST_RES p_raw_test_res = NULL;
	s32 open_test_res = 0;
	PST_TP_DEV p_tp_dev = NULL;
	u16 node_num = 0;
	u8 key_num = 0;
	PST_SUB_TEST_ITEM_ID_SET p_sub_id_set = NULL;

	p_test_item = (PST_TEST_ITEM) p_data;
	p_raw_test_res =
		(PST_RAWDATA_TEST_RES) p_test_item->test_res.ptr_res;
	open_test_res = p_raw_test_res->open_test_result;
	p_tp_dev = p_test_item->p_tp_dev;
	node_num = p_tp_dev->sc_sen_num * p_tp_dev->sc_drv_num;
	key_num = p_tp_dev->key_num;
	p_sub_id_set = p_raw_test_res->item_res_header.p_sub_id_set;

	modify_test_result(open_test_res);
	board_print_info("\n---Show open test result!---\n");
	if ((p_raw_test_res->item_res_header.test_item_err_code.
		ptr_err_code_set[0] != RAWDATA_TEST_FAIL)
		&& (p_raw_test_res->item_res_header.test_item_err_code.
		ptr_err_code_set[0] != RAWDATA_TEST_SUCCESSFULLY)) {
		board_print_info
			("OPEN TEST ABORTED: OPEN TEST ERROR!\n");
		return 1;
	}
	if (p_sub_id_set->len == 0) {
		board_print_info("RAWDATA TEST PASS!\n");
	}

	for (i = 0; i < p_sub_id_set->len; i++) {
		switch (p_sub_id_set->p_id_set[i]) {
		case TP_RAWDATA_MAX_TEST_ITEM_ID:
			{
				if ((open_test_res &
					BEYOND_RAWDATA_UPPER_LIMIT) != 0) {
					board_print_info
						("BEYOND RAWDATA UPPER LIMIT!\n");
					/*print_beyond_limit_node(p_raw_test_res->p_beyond_rawdata_upper_limit_cnt,node_num);*/
				} else {
					board_print_info
						("RAWDATA UPPER LIMIT TEST PASS!\n");
				}
				break;
			}
		case TP_RAWDATA_MIN_TEST_ITEM_ID:
			{
				if ((open_test_res &
					BEYOND_RAWDATA_LOWER_LIMIT) != 0) {
					board_print_info
						("BEYOND RAWDATA LOWER LIMIT!\n");
					/*print_beyond_limit_node(p_raw_test_res->p_beyond_rawdata_lower_limit_cnt,node_num);*/
				} else {
					board_print_info
						("RAWDATA LOWER LIMIT TEST PASS!\n");
				}
				break;
			}
		case TP_KEYDATA_MAX_TEST_ITEM_ID:
			{
				if ((open_test_res &
					KEY_BEYOND_UPPER_LIMIT) != 0) {
					board_print_info
						("KEY BEYOND UPPER LIMIT!\n");
					/*print_beyond_limit_node(p_raw_test_res->p_beyond_key_upper_limit_cnt,key_num);*/
				} else {
					board_print_info
						("KEYDATA UPPER LIMIT TEST PASS!\n");
				}
				break;
			}
		case TP_KEYDATA_MIN_TEST_ITEM_ID:
			{
				if ((open_test_res &
					KEY_BEYOND_LOWER_LIMIT) != 0) {
					board_print_info
						("KEY BEYOND LOWER LIMIT!\n");
					/*print_beyond_limit_node(p_raw_test_res->p_beyond_key_lower_limit_cnt,key_num);*/
				} else {
					board_print_info
						("KEYDATA LOWER LIMIT TEST PASS!\n");
				}
				break;
			}
		case TP_OFFSET_TEST_ITEM_ID:
			{
				if ((open_test_res &
					BEYOND_OFFSET_LIMIT) != 0) {
					board_print_info
						("BEYOND OFFSET LIMIT!\n");
					/*print_beyond_limit_node(p_raw_test_res->p_beyond_offset_limit_cnt,node_num);*/
				} else {
					board_print_info
						("OFFSET TEST PASS!\n");
				}
				break;
			}
		case TP_RAWDATA_JITTER_TEST_ITEM_ID:
			{
				if ((open_test_res &
					BEYOND_JITTER_LIMIT) != 0) {
					board_print_info
						("BEYOND JITTER LIMIT!\n");
					/*rint_beyond_limit_node(p_raw_test_res->p_beyond_jitter_limit_cnt,node_num);*/
				} else {
					board_print_info
						("JITTER TEST PASS!\n");
				}
				break;
			}
		case TP_ACCORD_TEST_ITEM_ID:
			{
				if ((open_test_res &
					BEYOND_ACCORD_LIMIT) != 0) {
					board_print_info
						("BEYOND ACCORD LIMIT!\n");
					/*print_beyond_limit_node(p_raw_test_res->p_beyond_accord_limit_cnt,node_num);*/
				} else {
					board_print_info
						("ACCORD TEST PASS!\n");
				}
				break;
			}
		case TP_UNIFORMITY_TEST_ITEM_ID:
			{
				if ((open_test_res &
					BEYOND_UNIFORMITY_LIMIT) != 0) {
					board_print_info
						("BEYOND UNIFORMITY LIMIT!\n");
					/*print_beyond_limit_node(p_raw_test_res->p_beyond_jitter_limit_cnt,node_num);*/
				} else {
					board_print_info
						("UNIFORMITY TEST PASS!\n");
				}
				break;
			}
		default:
			break;
		}
	}

	return ret;
}

/*******************************************************************************
* Function Name	: print_short_test_res
* Description	: print short test result
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ORDER )
* Output		:
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 print_short_test_res(ptr32 p_data)
{
	s32 ret = 1;
	u16 i = 0;
	PST_TEST_ITEM p_test_item = NULL;
	PST_SHORT_TEST_RES p_short_res = NULL;

	p_test_item = (PST_TEST_ITEM) p_data;
	p_short_res = (PST_SHORT_TEST_RES) p_test_item->test_res.ptr_res;
	board_print_info("\n---show short test result!---\n");
	modify_test_result(p_short_res->item_res_header.
			test_item_err_code.ptr_err_code_set[0]);
	if ((p_short_res->item_res_header.test_item_err_code.
		ptr_err_code_set[0] != SHORT_TEST_NONE_ERROR)
		&& (p_short_res->item_res_header.test_item_err_code.
		ptr_err_code_set[0] != SHORT_TEST_EXIST_SHORT)) {
		board_print_info
			("SHORT TEST ABORTED:SHORT TEST ERROR!\n");
		return 1;
	}

	if (p_short_res->short_num > 0) {
		board_print_info("SHORT TEST FAILD!\n");
		for (i = 0; i < p_short_res->short_num; i++) {
			if (p_short_res->short_mess_arr[4 * i + 0] ==
				CHANNEL_GND) {
				board_print_info("GND and ");
			} else if (p_short_res->
				short_mess_arr[4 * i + 0] ==
				CHANNEL_VDD) {
				board_print_info("VDD and ");
			} else {
				if (p_short_res->
					short_mess_arr[4 * i + 0] & 0x80) {
					board_print_info("drv-%d and ",
							p_short_res->
							short_mess_arr
							[4 * i +
							0] & 0x7f);
				} else {
					board_print_info("sen-%d and ",
							p_short_res->
							short_mess_arr
							[4 * i +
							0] & 0x7f);
				}
			}

			if (p_short_res->short_mess_arr[4 * i + 1] ==
				CHANNEL_GND) {
				board_print_info("GND short,");
			} else if (p_short_res->
				short_mess_arr[4 * i + 1] ==
				CHANNEL_VDD) {
				board_print_info("VDD short,");
			} else {
				if (p_short_res->
					short_mess_arr[4 * i + 1] & 0x80) {
					board_print_info
						("drv-%d short,",
						p_short_res->
						short_mess_arr[4 * i +
							1] & 0x7f);
				} else {
					board_print_info
						("sen-%d short,",
						p_short_res->
						short_mess_arr[4 * i +
							1] & 0x7f);
				}
			}
			board_print_info("short r=%d Kohm\n",
						(((p_short_res->
						short_mess_arr[4 * i +
							2] << 8) +
						p_short_res->
						short_mess_arr[4 * i +
							3])) / 10);
		}
	} else {
		board_print_info("SHORT TEST PASS!\n");
	}
	return ret;
}

/*******************************************************************************
* Function Name	: print_version_test_res
* Description	: print version test result
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ORDER )
* Output		: none
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
extern s32 print_version_test_res(ptr32 p_data)
{
	s32 ret = 1;

	PST_TEST_ITEM p_test_item = NULL;
	PST_VERSION_TEST_RES p_ver_test_res = NULL;
	PST_VERSION_PARAM p_ver_param = NULL;

	p_test_item = (PST_TEST_ITEM) p_data;
	p_ver_test_res =
		(PST_VERSION_TEST_RES) p_test_item->test_res.ptr_res;
	p_ver_param = (PST_VERSION_PARAM) p_test_item->param.ptr_param;
	board_print_info("\n---Show fw version test result!---\n");

	modify_test_result(p_ver_test_res->item_res_header.
			test_item_err_code.ptr_err_code_set[0]);
	if (p_ver_test_res->item_res_header.test_item_err_code.
		ptr_err_code_set[0] == VERSION_TEST_NONE_ERROR) {
		board_print_info("FW VERSION TEST PASS!\n");
	} else {
		board_print_info("VERSION TEST FAILD!\n");
	}
		return ret;
}

/*******************************************************************************
* Function Name	: print_flash_test_res
* Description	: print flash test result
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ORDER )
* Output		: none
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
extern s32 print_flash_test_res(ptr32 p_data)
{
	s32 ret = 1;
	PST_TEST_ITEM p_test_item = NULL;
	PST_FLASH_TEST_RES p_flash_test_res = NULL;

	p_test_item = (PST_TEST_ITEM) p_data;
	p_flash_test_res =
		(PST_FLASH_TEST_RES) p_test_item->test_res.ptr_res;
	board_print_info("\n---Show flash test result!---\n");

	modify_test_result(p_flash_test_res->item_res_header.
			test_item_err_code.ptr_err_code_set[0]);
	if (p_flash_test_res->item_res_header.test_item_err_code.
		ptr_err_code_set[0] == FLASH_TEST_ERROR_CODE_NONE) {
		board_print_info("FLASH TEST PASS!\n");
	} else {
		board_print_info("FLASH TEST FAILD!\n");
	}
	return ret;
}

/*******************************************************************************
* Function Name	: print_send_cfg_test_res
* Description	: print send config test result
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ORDER )
* Output		: none
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
extern s32 print_send_cfg_test_res(ptr32 p_data)
{
	s32 ret = 1;
	PST_TEST_ITEM p_test_item = NULL;
	PST_CHIP_CONFIG_PROC_RES p_send_cfg_test_res = NULL;

	p_test_item = (PST_TEST_ITEM) p_data;
	p_send_cfg_test_res =
		(PST_CHIP_CONFIG_PROC_RES) p_test_item->test_res.ptr_res;
	board_print_info("\n---Show send config test result!---\n");
	modify_test_result(p_send_cfg_test_res->item_res_header.
			test_item_err_code.ptr_err_code_set[0]);
	if (p_send_cfg_test_res->item_res_header.test_item_err_code.
		ptr_err_code_set[0] == CFG_CHECK_TEST_NONE_ERROR) {
		board_print_info("SEND CONFIG TEST PASS!\n");
	} else {
		board_print_info("SEND CONFIG FAILD!\n");
	}
	return ret;
}

/*******************************************************************************
* Function Name	: print_cfg_recover_test_res
* Description	: print config recover test result
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ORDER )
* Output		: none
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
extern s32 print_cfg_recover_test_res(ptr32 p_data)
{
	s32 ret = 1;
	PST_TEST_ITEM p_test_item = NULL;
	PST_RECOVER_CHIP_CFG_RES p_cfg_recover_test_res = NULL;
	p_test_item = (PST_TEST_ITEM) p_data;
	p_cfg_recover_test_res =
		(PST_RECOVER_CHIP_CFG_RES) p_test_item->test_res.ptr_res;
	board_print_info("\n---Show cfg recover test result!---\n");

	modify_test_result(p_cfg_recover_test_res->item_res_header.
			test_item_err_code.ptr_err_code_set[0]);
	if (p_cfg_recover_test_res->item_res_header.test_item_err_code.
		ptr_err_code_set[0] == RECOVER_CFG_ERROR_CODE_NONE) {
		board_print_info("CONFIG RECOVER TEST PASS!\n");
	} else {
		board_print_info("CONFIG RECOVER FAILD!\n");
	}
	return ret;
}

#ifdef __cplusplus
}
#endif
