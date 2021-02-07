/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name				: short_test_func.c
* Author				: Bob Huang
* Version				: V1.0.0
* Date					: 07/26/2017
* Description			: We use this file unify a interface for short test
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#include "simple_mem_manager.h"
#include "tp_short_test.h"

#include "normandy_short_test.h"
#include "tp_test_general_error_code_def.h"
#include "test_proc.h"
#include <linux/string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
* Function Name	: short_test_func
* Description	: short test
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: none
* Return		: s32(0:test error 1:ok)
*******************************************************************************/
extern s32 short_test_func(IN_OUT ptr32 p_data)
{
	s32 ret = 0;
	u16 err_code = SHORT_TEST_NONE_ERROR;
	PST_TEST_ITEM p_test_item = (PST_TEST_ITEM) p_data;
	PST_SHORT_TEST_RES p_test_res = NULL;

	/*//-----------------Judge parameter----------------*/
	if (0 == judge_test_item_legal((PST_TEST_ITEM) p_data)) {
		ret = 0;
		goto SHORTTEST_END;
	}
	/*//prepared for test*/
		if (NULL != p_test_item->ptr_test_item_before_func) {
		ret = p_test_item->ptr_test_item_before_func(p_test_item->
							ptr_test_item_before_func_param);
		if (0 == ret) {
			board_print_error
				("[short test]callback function test before error!\n");
			err_code = TEST_BEFORE_FUNC_FAIL;
			goto SHORTTEST_END;
		}
	}

	p_test_res = (PST_SHORT_TEST_RES) p_test_item->test_res.ptr_res;
	p_test_res->item_res_header.test_item_err_code.
		ptr_err_code_set[0] = SHORT_TEST_NONE_ERROR;
	p_test_res->item_res_header.test_status = TEST_START;
	p_test_res->item_res_header.test_result = TEST_NG;

	ret = board_disable_dev_status(p_test_item->p_tp_dev);
	if (0 == ret) {
		board_print_error
			("[short test]disable device status faild!\n");
		err_code = DISABLE_DEV_STATUS_FAIL;
		goto SHORTTEST_END;
	}
	/*//selecting different branch according to chip id*/
	switch (p_test_item->p_tp_dev->chip_type) {
	case TP_NORMANDY:
	case TP_OSLO:
		{
			ret = normandy_short_test(p_data);
		}
		break;
	default:
		board_print_warning
			("chip type (%d) is not exist, please check it",
			p_test_item->p_tp_dev->chip_type);
		err_code = CHIP_TYPE_NOT_EXIST;
		break;
	}
	err_code =
		p_test_res->item_res_header.test_item_err_code.
		ptr_err_code_set[0];

SHORTTEST_END:

	/*// recovery chip on normal mode*/
	ret = short_test_end(p_test_item);
	if (ret == 0) {
		board_print_warning
			("[short test]recovry chip status error!\n");
		err_code = SHORT_TEST_END_FAIL;
	}
	/*//notify test result*/
	if (p_test_item->ptr_test_item_finished_func) {
		ret =
			p_test_item->
			ptr_test_item_finished_func(p_test_item->
						ptr_test_item_finished_func_param);
		if (ret == 0) {
			board_print_error
				("[short test]callback function after test error!\n");
			err_code = TEST_FINISH_FUNC_FAIL;
		}

	}
	if ((err_code == SHORT_TEST_NONE_ERROR)
		|| (err_code == SHORT_TEST_EXIST_SHORT)) {
		ret = 1;
	} else {
		ret = 0;
	}

	return ret;
}

/*******************************************************************************
* Function Name	: clear_short_mess_in_res
* Description	: clear_short_mess_in_res
* Input			: PST_TEST_ITEM p_test_item
* Output		: PST_TEST_ITEM p_test_item
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 clear_short_mess_in_res(IN_OUT PST_TEST_ITEM p_test_item)
{

	PST_SHORT_TEST_RES p_short_res =
		(PST_SHORT_TEST_RES) p_test_item->test_res.ptr_res;
	p_short_res->short_num = 0;
	p_short_res->item_res_header.test_id =
		p_test_item->test_item_id;
	p_short_res->item_res_header.test_status = TEST_START;
	p_short_res->item_res_header.test_result = TEST_OK;
	p_short_res->item_res_header.test_item_err_code.
		ptr_err_code_set[0] = SHORT_TEST_NONE_ERROR;
	return 1;
}

/*******************************************************************************
* Function Name	: store_short_mess_in_res
* Description	: store short message in result
* Input			: s32 chn1
* Input			: s32 chn2
* Input			: u16 r
* Output		: PST_SHORT_TEST_RES p_short_res
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 store_short_mess_in_res(IN u8 chn1, IN u8 chn2, IN u16 r,
				OUT PST_SHORT_TEST_RES p_short_res)
{
	s32 ret = 0;
	u8 i = 0;
	u8 repeat_cnt = 0;
	u8 repeat = 0;
	if (chn1 == chn2 || p_short_res->short_num >= MAX_SHORT_NUM) {
		goto STORE_END;
	}
	/*//filter repeating data*/
	for (i = 0; i < p_short_res->short_num; i++) {
		repeat_cnt = 0;
		if (p_short_res->short_mess_arr[4 * i + 0] == chn1) {
			repeat_cnt++;
		}

		if (p_short_res->short_mess_arr[4 * i + 0] == chn2) {
			repeat_cnt++;
		}

		if (p_short_res->short_mess_arr[4 * i + 1] == chn1) {
			repeat_cnt++;
		}

		if (p_short_res->short_mess_arr[4 * i + 1] == chn2) {
			repeat_cnt++;
		}
		if (repeat_cnt >= 2) {
			repeat = 1;
		}
	}
	if (repeat == 0) {
		p_short_res->short_mess_arr[4 * p_short_res->short_num + 0] = chn1;
		p_short_res->short_mess_arr[4 * p_short_res->short_num + 1] = chn2;
		p_short_res->short_mess_arr[4 * p_short_res->short_num + 2] = (r >> 8) & 0xFF;
		p_short_res->short_mess_arr[4 * p_short_res->short_num + 3] = r & 0xFF;
		if (p_short_res->short_num < MAX_SHORT_NUM) {
			p_short_res->short_num++;
		}
		ret = 1;
	}

STORE_END:
	return ret;
}

/*******************************************************************************
* Function Name	: short_test_channel_map
* Description	:
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Input			: ENUM_CHN_MAP_MODE chn_enum
* Output		: none
* Return		: void
*******************************************************************************/
extern void short_test_channel_map(IN ptr32 p_data,
				IN ENUM_CHN_MAP_MODE chn_enum)
{
	u8 i = 0;
	u8 j = 0;
	u8 k = 0;
	u8 ch_cnt = 0;
	PST_TEST_ITEM p_item = NULL;
	PST_SHORT_TEST_RES p_short_res = NULL;
	PST_TP_DEV p_dev = NULL;
	u8 b_find = 0;
	u8 ic_chn_num;
	u8 valid_chn_mark[4 * MAX_SHORT_NUM];
	u8 *p_short_mess_tmp = NULL;

	p_item = (PST_TEST_ITEM) p_data;
	p_short_res = (PST_SHORT_TEST_RES) p_item->test_res.ptr_res;
	p_dev = p_item->p_tp_dev;

	if (p_dev->map_disable == 1) {
		return;
	}

	memset(valid_chn_mark, 0x00, 4 * p_short_res->short_num);

	for (i = 0; i < p_short_res->short_num * 4; i++) {
		if (i % 4 == 2 || i % 4 == 3) {
			continue;
		}

		switch (chn_enum) {
		case TX_RX_SHARED:
			{
				b_find = 0;
				j = p_short_res->
					short_mess_arr[i] &
					(~CHANNEL_TX_FLAG);

				if (0 == b_find) {
					for (ch_cnt = 0;
						ch_cnt <
						p_dev->max_drv_num;
						ch_cnt++) {
						if (p_dev->
							drv_map[ch_cnt] ==
							j) {
							p_short_res->
								short_mess_arr
								[i] =
								CHANNEL_TX_FLAG
								| ch_cnt;
							valid_chn_mark
								[i] = 1;
							b_find = 1;
							break;
						}
					}
				}

				if (0 == b_find) {
					for (ch_cnt = 0;
						ch_cnt <
						p_dev->max_sen_num;
						ch_cnt++) {
						if (p_dev->
							sen_map[ch_cnt] ==
							j) {
							p_short_res->
								short_mess_arr
								[i] =
								ch_cnt;
							valid_chn_mark
								[i] = 1;
							b_find = 1;
							break;
						}
					}
				}
			}
			break;
		case TX_RX_PACK_ADC:
			{
				j = p_short_res->short_mess_arr[i];
				if ((j & CHANNEL_TX_FLAG) == CHANNEL_TX_FLAG) {	/*//Drv Chancel*/

					j = j & (~CHANNEL_TX_FLAG);
					for (ch_cnt = 0;
						ch_cnt <
						p_dev->max_drv_num;
						ch_cnt++) {
						if (p_dev->
							drv_map[ch_cnt] ==
							j) {
							ic_chn_num =
								p_dev->
								pack_drv_2_adc_map
								[ch_cnt];
							if (ch_cnt < (p_dev->max_drv_num / 2)) { /*//the first G1 chancel*/

								for (k =
									0;
									k <
									(p_dev->
									max_drv_num
									/
									2);
									k++) {
									if (p_dev->cfg[p_dev->drv_start_addr - p_dev->cfg_start_addr + k] == ic_chn_num) {
										p_short_res->
											short_mess_arr
											[i]
											=
											CHANNEL_TX_FLAG
											|
											j;
										valid_chn_mark
											[i]
											=
											1;
										break;
									}
								}
							} else {	/*//the second G1 chancel*/
								for (k =
									0;
									k <
									p_dev->
									max_drv_num
									/
									2;
									k++) {
									if (p_dev->cfg[p_dev->drv1_start_addr - p_dev->cfg_start_addr + k] == ic_chn_num) {
										p_short_res->short_mess_arr[i] = CHANNEL_TX_FLAG | j;	//((p_dev->max_drv_num / 2) + k);
										valid_chn_mark
											[i]
											=
											1;
										break;
									}
								}
							}
							break;
						}
					}	/*//end for (ch_cnt = 0; ch_cnt < p_dev->max_drv_num; ch_cnt++)*/
				} else {	/*// Sen Chancel*/

					for (ch_cnt = 0;
						ch_cnt <
						p_dev->max_sen_num;
						ch_cnt++) {
						if (p_dev->
							sen_map[ch_cnt] ==
							j) {
							ic_chn_num =
								p_dev->
								pack_sen_2_adc_map
								[ch_cnt];
							if (ch_cnt < (p_dev->max_sen_num / 2)) {	/*//the first G1 chancel*/
								for (k =
									0;
									k <
									(p_dev->
									max_sen_num
									/
									2);
									k++) {

									if (p_dev->cfg[p_dev->sen_start_addr - p_dev->cfg_start_addr + k] == ic_chn_num) {
										p_short_res->
											short_mess_arr
											[i]
											=
											j;
										valid_chn_mark
											[i]
											=
											1;
										break;
									}
								}
							} else {	/*//the second G1 chancel*/
								for (k =
									0;
									k <
									p_dev->
									max_sen_num
									/
									2;
									k++) {

									if (p_dev->cfg[p_dev->sen1_start_addr - p_dev->cfg_start_addr + k] == ic_chn_num) {
										p_short_res->short_mess_arr[i] = j;	/*//(p_dev->max_sen_num / 2) + k;*/
										valid_chn_mark
											[i]
											=
											1;
										break;
									}
								}
							}
							break;
						}
					}	/*//end for (ch_cnt = 0; ch_cnt < p_dev->max_sen_num; ch_cnt++)*/
				}
			}
			break;
		case TX_RX_SEPERATION:
		default:
			{
				if ((p_short_res->
					short_mess_arr[i] &
					CHANNEL_TX_FLAG) ==
					CHANNEL_TX_FLAG) {
					j = p_short_res->
						short_mess_arr[i] &
						(~CHANNEL_TX_FLAG);
					for (ch_cnt = 0;
						ch_cnt <
						p_dev->max_drv_num;
						ch_cnt++) {
						if (p_dev->
							drv_map[ch_cnt] ==
							j) {
							p_short_res->
								short_mess_arr
								[i] =
								CHANNEL_TX_FLAG
								| ch_cnt;
							valid_chn_mark
								[i] = 1;
							break;
						}
					}
				} else {
					j = p_short_res->
						short_mess_arr[i];
					for (ch_cnt = 0;
						ch_cnt <
						p_dev->max_sen_num;
						ch_cnt++) {
						if (p_dev->
							sen_map[ch_cnt] ==
							j) {
							p_short_res->
								short_mess_arr
								[i] =
								ch_cnt;
							valid_chn_mark
								[i] = 1;
							break;
						}
					}
				}
			}
			break;
		}	/*// end switch (chn_enum)*/
	}		/*//end for(i = 0; i<p_short_res->short_num*4; i++)*/

	/*//check the short valid or not*/
	ic_chn_num = 0;
	p_short_mess_tmp = &p_short_res->short_mess_arr[0];
	for (i = 0; i < p_short_res->short_num * 4; i += 4) {
		if ((valid_chn_mark[i] != 1
			&& p_short_mess_tmp[0] != CHANNEL_GND
			&& p_short_mess_tmp[0] != CHANNEL_VDD)
			|| (valid_chn_mark[i + 1] != 1
			&& p_short_mess_tmp[1] != CHANNEL_GND
			&& p_short_mess_tmp[1] != CHANNEL_VDD)) {
			if (p_short_res->short_num * 4 - i - 4 >= 4) {
				memcpy(p_short_mess_tmp,
					p_short_mess_tmp + 4,
					p_short_res->short_num * 4 - (i +
							4));
			}
		} else {
			ic_chn_num++;
			p_short_mess_tmp += 4;
		}
	}
	p_short_res->short_num = ic_chn_num;
	if (p_short_res->short_num == 0) {
		p_short_res->item_res_header.test_item_err_code.
			ptr_err_code_set[0] = SHORT_TEST_NONE_ERROR;
		p_short_res->item_res_header.test_result = TEST_OK;
	}
}

/*******************************************************************************
* Function Name	: short_test_end
* Description	: (1)open esd and irq (2)reset chip
* Input			: PST_TEST_ITEM p_test_item
* Output		: none
* Return		: s32(0:have not handled 1:handled)
*******************************************************************************/
extern s32 short_test_end(PST_TEST_ITEM p_test_item)
{
	s32 ret = 1;
	if (board_recovery_dev_status(p_test_item->p_tp_dev) == 0) {
		ret = 0;
	}

	if (reset_chip_proc(p_test_item->p_tp_dev, RESET_TO_AP) == 0) {
		board_print_error("reset tp error!\n");
		ret = 0;
	}
	return ret;
}

#ifdef __cplusplus
}
#endif

#endif
