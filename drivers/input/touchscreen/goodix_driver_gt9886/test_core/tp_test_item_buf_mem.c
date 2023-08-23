/******************** (C) COPYRIGHT 2017 Goodix ********************
* File Name          : TpTestItemResultMemProc.h
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

#ifdef __cplusplus
extern "C" {
#endif
/*******************************************************************************
* Function Name	: alloc_test_item_cur_buf
* Description	: alloc_test_item_cur_buf
* Input			: none
* Output		: none
* Return		: s32(1:ok false:fail)
*******************************************************************************/
extern s32 alloc_test_item_cur_buf(PST_TEST_ITEM *pp_test_item,
				u16 item_num)
{
	s32 bRet = 0;
	u32 i = 0;
	for (i = 0; i < item_num; i++) {
		PST_TEST_ITEM pTestItem = pp_test_item[i];
		switch (pTestItem->test_item_id) {
			/*//----------------------tp open test-----------------------//*/
		case TP_RAWDATA_TEST_ITEMS_SET_ID:
			{

			} break;

		case TP_DIFFDATA_TEST_ITEMS_SET_ID:
			{

			}
			break;

			/*//----------------------hardware test-----------------------//*/
		case TP_SHORT_TEST_ITEM_ID:
			{

			}
			break;

		case TP_RST_VOLT_TEST_ITEM_ID:
			{

			}
			break;

		case TP_I2C_VOLT_TEST_ITEM_ID:
			{

			}
			break;

		case TP_INT_VOLT_TEST_ITEM_ID:
			{

			}
			break;
		case TP_ACTIVE_CURRENT_TEST_ITEM_ID:
			{

			}
			break;

		case TP_SLEEP_CURRENT_TEST_ITEM_ID:
			{

			}
			break;

		case TP_INT_CAP_TEST_ITEM_ID:
			{

			}
			break;

		case TP_MODULE_TYPE_TEST_ITEM_ID:
			{

			}
			break;

		case TP_PIN1_TEST_ITEM_ID:
			{

			}
			break;

		case TP_PIN2_TEST_ITEM_ID:
			{

			}
			break;

			/*//----------------------firmware test-----------------------//*/
		case TP_VERSION_TEST_ITEM_ID:
			{

			}
			break;
		case TP_CHK_FW_RUN_STATE_ITEM_ID:
			{

			}
			break;

			/*//----------------line test or key touch test---------------//*/
		case TP_KEY_TOUCH_TEST_ITEM_ID:
			{

			}
			break;

		case TP_LINE_TEST_ITEM_ID:
			{

			}
			break;

			/*//----------------------other test-----------------------//*/
		case TP_CHIP_CFG_PROC_ITEM_ID:
			{

			}
			break;

		case TP_FLASH_TEST_ITEM_ID:
			{

			}
			break;

		case TP_SEND_SPEC_CFG_ITEM_ID:
			{

			}
			break;

		case TP_UPDATE_SPEC_FW_ITEM_ID:
			{

			}
			break;

		case TP_CUSTOM_INFO_TEST_ITEM_ID:
			{

			}
			break;

		case TP_CHK_CHIP_TEST_RESULT_ITEM_ID:
			{

			}
			break;
		case TP_CHK_MODULE_TEST_RESULT_ITEM_ID:
			{

			}
			break;
		case TP_RECOVER_CFG_ITEM_ID:
			{

			}
			break;
		case TP_SAVE_RES_TO_IC_STORAGE_ITEM_ID:
			{

			}
			break;

		default:
			break;
		}
	}
	bRet = 1;

	return bRet;
}

/*******************************************************************************
* Function Name	: free_test_item_cur_buf
* Description	: free_test_item_cur_buf
* Input			: PST_TEST_ITEM* pp_test_item
* Output		: u16 item_num
* Return		: s32(1:ok false:fail)
*******************************************************************************/
extern s32 free_test_item_cur_buf(PST_TEST_ITEM *pp_test_item,
				u16 item_num)
{
	s32 bRet = 0;
	u32 i = 0;
	for (i = 0; i < item_num; i++) {
		PST_TEST_ITEM pTestItem = pp_test_item[i];
		switch (pTestItem->test_item_id) {
			/*//----------------------tp open test-----------------------//*/
		case TP_RAWDATA_TEST_ITEMS_SET_ID:
			{

			}
			break;

		case TP_DIFFDATA_TEST_ITEMS_SET_ID:
			{

			}
			break;

			/*//----------------------hardware test-----------------------//*/
		case TP_SHORT_TEST_ITEM_ID:
			{

			}
			break;

		case TP_RST_VOLT_TEST_ITEM_ID:
			{

			}
			break;

		case TP_I2C_VOLT_TEST_ITEM_ID:
			{

			}
			break;

		case TP_INT_VOLT_TEST_ITEM_ID:
			{

			}
			break;
		case TP_ACTIVE_CURRENT_TEST_ITEM_ID:
			{

			}
			break;

		case TP_SLEEP_CURRENT_TEST_ITEM_ID:
			{

			}
			break;

		case TP_INT_CAP_TEST_ITEM_ID:
			{

			}
			break;

		case TP_MODULE_TYPE_TEST_ITEM_ID:
			{

			}
			break;

		case TP_PIN1_TEST_ITEM_ID:
			{

			}
			break;

		case TP_PIN2_TEST_ITEM_ID:
			{

			}
			break;

			/*//----------------------firmware test-----------------------//*/
		case TP_VERSION_TEST_ITEM_ID:
			{

			}
			break;
		case TP_CHK_FW_RUN_STATE_ITEM_ID:
			{

			}
			break;

			/*//----------------line test or key touch test---------------//*/
		case TP_KEY_TOUCH_TEST_ITEM_ID:
			{

			}
			break;

		case TP_LINE_TEST_ITEM_ID:
			{

			}
			break;

			/*//----------------------other test-----------------------//*/
		case TP_CHIP_CFG_PROC_ITEM_ID:
			{

			}
			break;

		case TP_FLASH_TEST_ITEM_ID:
			{

			}
			break;

		case TP_SEND_SPEC_CFG_ITEM_ID:
			{

			}
			break;

		case TP_UPDATE_SPEC_FW_ITEM_ID:
			{

			}
			break;
		case TP_CUSTOM_INFO_TEST_ITEM_ID:
			{

			}
			break;

		case TP_CHK_CHIP_TEST_RESULT_ITEM_ID:
			{

			}
			break;
		case TP_CHK_MODULE_TEST_RESULT_ITEM_ID:
			{

			}
			break;
		case TP_RECOVER_CFG_ITEM_ID:
			{

			}
			break;
		case TP_SAVE_RES_TO_IC_STORAGE_ITEM_ID:
			{

			}
			break;

		default:
			break;
		}
	}
	bRet = 1;

	return bRet;
}

#ifdef __cplusplus
}
#endif
