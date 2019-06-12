/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name          : tp_test_item_id.h
* Author             : Bob Huang
* Version            : V1.0.0
* Date               : 12/31/2017
* Description        : touch panel test item id
					   //-----------------Very Important--------------------//
					   You can not modify each micro define,because this define
					   will be recorded in doc and send to custom.
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#ifndef TP_TEST_ITEM_ID_H
#define TP_TEST_ITEM_ID_H

#ifdef __cplusplus
extern "C" {
#endif
/*//touch panel project start with TP*/

/*//device information ID may occupy 0~4 reserved ID*/
#define TP_DEV_INFO_ID						0

/*//--------------------------------------------test item Id define start------------------------------------------------//*/
/*//test item ID define 0~4 reserved*/
#define TP_TEST_ITEM_START_ID				5
/*//data test*/
#define TP_RAWDATA_TEST_ITEMS_SET_ID		5
#define TP_RAWDATA_MAX_TEST_ITEM_ID			0
#define TP_RAWDATA_MIN_TEST_ITEM_ID			1
#define TP_KEYDATA_MAX_TEST_ITEM_ID			2
#define TP_KEYDATA_MIN_TEST_ITEM_ID			3
#define TP_ACCORD_TEST_ITEM_ID				4
#define TP_OFFSET_TEST_ITEM_ID				5
#define TP_UNIFORMITY_TEST_ITEM_ID			6
#define TP_RAWDATA_JITTER_TEST_ITEM_ID		7

#define TP_DIFFDATA_TEST_ITEMS_SET_ID		6
#define TP_DIFFDATA_JITTER_TEST_ITEM_ID		0

/*//hardware test*/
#define TP_SHORT_TEST_ITEM_ID				200
#define TP_RST_VOLT_TEST_ITEM_ID			201
#define TP_I2C_VOLT_TEST_ITEM_ID			202
#define TP_INT_VOLT_TEST_ITEM_ID			203
#define TP_PIN1_TEST_ITEM_ID				204
#define TP_PIN2_TEST_ITEM_ID				205

#define TP_ACTIVE_CURRENT_TEST_ITEM_ID		206
#define TP_SLEEP_CURRENT_TEST_ITEM_ID		207
#define TP_INT_CAP_TEST_ITEM_ID				208
#define TP_MODULE_TYPE_TEST_ITEM_ID			209
#define TP_GESTURE_TEST_ITEM_ID				210
#define TP_SPI_VOLT_TEST_ITEM_ID			211

/*//firmware test*/
#define TP_VERSION_TEST_ITEM_ID				600
#define TP_CHK_FW_RUN_STATE_ITEM_ID			601

/*//line test or key touch test*/
#define TP_KEY_TOUCH_TEST_ITEM_ID			800
#define TP_LINE_TEST_ITEM_ID				801

/*//other test*/
#define TP_CHIP_CFG_PROC_ITEM_ID				1200	/*/cfg check,check cfg in chip whether ok or not,or send config*/
#define TP_FLASH_TEST_ITEM_ID					1201	/*//flash test using send cfg ,reset chip,then read and compare cfg*/
#define TP_SEND_SPEC_CFG_ITEM_ID				1202	/*//after test send special config*/
#define TP_UPDATE_SPEC_FW_ITEM_ID				1203	/*//after test update special firmware*/
#define TP_CUSTOM_INFO_TEST_ITEM_ID				1204	/*//custom information:including write information or check information*/
#define TP_CHK_CHIP_TEST_RESULT_ITEM_ID			1205	/*//check chip test result*/
#define TP_CHK_MODULE_TEST_RESULT_ITEM_ID		1206	/*//check module test result*/
#define TP_RECOVER_CFG_ITEM_ID					1207	/*//recovery config to chip*/
#define TP_SAVE_RES_TO_IC_STORAGE_ITEM_ID		1208	/*//save result to ic storage*/
#define TP_CHIP_UID_PROCESS_ITEM_ID				1209	/*//tp chip uid process*/

#define TP_TEST_SUB_ITEM_END_ID					0xFF
#define TP_TEST_ITEM_END_ID						0xFFFF
/*//--------------------------------------------test item Id define end------------------------------------------------//*/
#ifdef __cplusplus
}
#endif
#endif
#endif
