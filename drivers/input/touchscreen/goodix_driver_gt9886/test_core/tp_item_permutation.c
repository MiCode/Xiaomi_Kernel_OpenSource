/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name			: test_proc.c
* Author			: Bob Huang
* Version			: V1.0.0
* Date				: 02/06/2018
* Description		: Sort item
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#include "tp_item_permutation.h"
#include "board_opr_interface.h"

/*-----------------------------static variable-------------------------------*/
const static u16 cs_item_permutation[] = {
	TP_CHK_CHIP_TEST_RESULT_ITEM_ID,
	TP_CHK_MODULE_TEST_RESULT_ITEM_ID,
	TP_I2C_VOLT_TEST_ITEM_ID,
	TP_INT_VOLT_TEST_ITEM_ID,
	TP_INT_CAP_TEST_ITEM_ID,
	TP_RST_VOLT_TEST_ITEM_ID,
	TP_PIN1_TEST_ITEM_ID,
	TP_PIN2_TEST_ITEM_ID,
	TP_CHIP_UID_PROCESS_ITEM_ID,
	TP_VERSION_TEST_ITEM_ID,
	TP_CHK_FW_RUN_STATE_ITEM_ID,
	TP_CUSTOM_INFO_TEST_ITEM_ID,
	TP_MODULE_TYPE_TEST_ITEM_ID,
	TP_CHIP_CFG_PROC_ITEM_ID,
	TP_GESTURE_TEST_ITEM_ID,
	TP_SLEEP_CURRENT_TEST_ITEM_ID,
	TP_FLASH_TEST_ITEM_ID,
	TP_SHORT_TEST_ITEM_ID,
	TP_ACTIVE_CURRENT_TEST_ITEM_ID,
	TP_RAWDATA_TEST_ITEMS_SET_ID,
	TP_DIFFDATA_TEST_ITEMS_SET_ID,
	TP_LINE_TEST_ITEM_ID,
	TP_KEY_TOUCH_TEST_ITEM_ID,
	TP_RECOVER_CFG_ITEM_ID,
	TP_UPDATE_SPEC_FW_ITEM_ID,
	TP_SEND_SPEC_CFG_ITEM_ID,
	TP_SAVE_RES_TO_IC_STORAGE_ITEM_ID,
	TP_TEST_ITEM_END_ID,
};

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
* Function Name	: get_index_from_permutaion_arr
* Description	: sort item get_index_from_permutaion_arr
* Input			: u16 item_id
* Output		: none
* Return		: s32(index)
*******************************************************************************/
extern s32 get_index_from_permutaion_arr(IN u16 item_id)
{
	s32 index_ret = -1;
	s32 arr_size = sizeof(cs_item_permutation);
	s32 i = 0;
	for (i = 0;
		i < arr_size
		&& cs_item_permutation[i] != TP_TEST_ITEM_END_ID; i++) {
		if (cs_item_permutation[i] == item_id) {
			index_ret = i;
			break;
		}
	} return index_ret;
}

/*******************************************************************************
* Function Name	: sort_item_permutation
* Description	: sort item permutation
* Input			: PST_TEST_PROC_DATA p_test_proc_data
* Output		: PST_TEST_PROC_DATA p_test_proc_data
* Return		: s32(1:ok 0:fail)
*******************************************************************************/
extern s32 sort_item_permutation(IN_OUT PST_TEST_PROC_DATA
				p_test_proc_data)
{
	s32 ret = 1;
	s32 i = 0, j = 0;
	s32 item_cnt = 0;
	s32 index1 = -1, index2 = -1;
	PST_TEST_ITEM p_item_tmp = NULL;
	if (NULL != p_test_proc_data) {
		item_cnt = p_test_proc_data->test_item_num;
		for (i = 0; i < item_cnt; i++) {
			for (j = i + 1; j < item_cnt; j++) {
				index1 =
					get_index_from_permutaion_arr
					(p_test_proc_data->
					pp_test_item_arr[i]->test_item_id);
				index2 =
					get_index_from_permutaion_arr
					(p_test_proc_data->
					pp_test_item_arr[j]->test_item_id);
				if (-1 == index1 || -1 == index2) {
					board_print_error("%s\n",
							"can not find test item!");
				}
				if (index1 != -1 && index2 != -1
					&& index1 > index2) {
					p_item_tmp =
						p_test_proc_data->
						pp_test_item_arr[i];
					p_test_proc_data->
						pp_test_item_arr[i] =
						p_test_proc_data->
						pp_test_item_arr[j];
					p_test_proc_data->
						pp_test_item_arr[j] =
						p_item_tmp;
				}
			}
		}
		ret = 1;
	}
	return ret;
}

#ifdef __cplusplus
}
#endif

#endif
