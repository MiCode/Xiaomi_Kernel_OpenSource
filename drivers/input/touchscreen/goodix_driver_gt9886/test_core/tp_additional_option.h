/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name          : tp_additional_option.h
* Author             : Bob Huang
* Version            : V1.0.0
* Date               : 10/26/2017
* Description        : this file offer some functions which are called close to test end
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#ifndef TP_ADDITIONAL_OPTION_H
#define TP_ADDITIONAL_OPTION_H

#include "board_opr_interface.h"
#include "tp_additional_option_param.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************chip config proc*********************************/

#define CFG_CHECK_TEST_NONE_ERROR		0x00
#define CFG_CHECK_TEST_UPDATE_FAIL		0x01
#define CFG_CHECK_TEST_READ_FAIL		0x02
#define CFG_CHECK_TEST_CHECK_FAIL		0x03
#define CFG_SET_CHIP_COORD_MODE_FAIL	0x04

/*send config result*/
typedef struct CHIP_CONFIG_PROC_RES {
	ST_TEST_ITEM_RES item_res_header;
	u16 cfg_len;
	u8 *p_read_cfg;
} ST_CHIP_CONFIG_PROC_RES, *PST_CHIP_CONFIG_PROC_RES;

extern s32 tp_chip_cfg_proc(ptr32 p_data);

/*************send spec config test error code**************/
#define SPEC_CONFIG_ERROR_CODE_NONE		0x00
#define SPEC_CONFIG_UPDATE_FAIL			0x01
#define SPEC_CONFIG_READ_FAIL			0x02
#define SPEC_CONFIG_CHECK_FAIL			0x03
/*send special config result*/
typedef struct SEND_SPEC_CONFIG_RES {
	ST_TEST_ITEM_RES item_res_header;
	u16 read_cfg_len;
	u8 *read_config;
} ST_SEND_SPEC_CONFIG_RES, *PST_SEND_SPEC_CONFIG_RES;

/*************send spec config test error code**************/

extern s32 tp_send_spec_cfg_to_chip(ptr32 p_data);

/**********************************flash test***********************************/
/*flash test (TP_FLASH_TEST_ITEM_ID)*/

/*------------Error Code-------------*/
#define FLASH_TEST_ERROR_CODE_NONE		0	/*test ok*/
#define FLASH_TEST_CFG_WRITE_ERROR		1
#define FLASH_TEST_CFG_READ_ERROR		2
#define FLASH_TEST_RST_FAIL_ERROR		3
#define FLASH_TEST_CMP_ERROR			4

/*flash test result*/
typedef struct FLASH_TEST_RES {
	ST_TEST_ITEM_RES item_res_header;
	u16 cfg_len;
	u8 *config_arr;
} ST_FLASH_TEST_RES, *PST_FLASH_TEST_RES;

extern s32 tp_flash_test(ptr32 p_data);

/**********************************update special firmware***********************************/
/*//update special firmware(TP_UPDATE_SPEC_FW_ITEM_ID)
//------------------Error Code--------------------//*/
#define UPDATE_SPEC_FW_ERROR_NONE	0x00
#define UPDATE_SPEC_FW_FAIL			0x01

/*flash test result*/
typedef struct UPDATE_SPEC_FW_RES {
	ST_TEST_ITEM_RES item_res_header;
} ST_UPDATE_SPEC_FW_RES, *PST_UPDATE_SPEC_FW_RES;

extern s32 tp_update_spec_fw(ptr32 p_data);

/**********************************custom information************************************/
/*custom information(TP_CUSTOM_INFO_TEST_ITEM_ID)*/

/*--------------------Error Code----------------------*/
#define CUSTOM_INFO_NONE_ERROR		0x00
#define CUSTOM_INFO_WRITE_ERROR		0x01
#define CUSTOM_INFO_READ_ERROR		0x02
#define CUSTOM_INFO_CMP_ERROR		0x03
#define CUSTOM_INFO_RESET_ERROR		0x04

/*write custom information test result*/
typedef struct CUSTOM_INFO_RES {
	ST_TEST_ITEM_RES item_res_header;
	u8 info_len;
	PST_CUSTOM_INFO_BYTE p_info;
} ST_CUSTOM_INFO_RES, *PST_CUSTOM_INFO_RES;

extern s32 tp_custom_info_proc(ptr32 p_data);

/**********************************Check Chip Test Result************************************/
/*//TP_CHK_CHIP_TEST_RESULT_ITEM_ID
//------------------Error Code--------------------//*/
#define CHECK_CHIP_TEST_ERROR_NONE		0x00

/*test result*/
typedef struct CHECK_CHIP_TEST_RES {
	ST_TEST_ITEM_RES item_res_header;
	u16 chip_test_res_info_len;
	u8 *chip_test_res_info;
} ST_CHECK_CHIP_TEST_RES, *PST_CHECK_CHIP_TEST_RES;
extern s32 tp_check_chip_test_result(ptr32 p_data);

/**********************************Check Module Test Result************************************/
/*TP_CHK_MODULE_TEST_RESULT_ITEM_ID*/
#define MAX_TEST_RES_LEN	8
/*--------------------Error Code---------------------*/
#define CHECK_MODULE_TEST_ERROR_NONE		0x00
#define CHECK_MODULE_TEST_READ_FAIL			0x01
#define CHECK_MODULE_TEST_RES_NG			0x02
#define CHECK_MODULE_TEST_CHKSUM_ERROR		0x03

/*test result*/
typedef struct CHECK_MODULE_TEST_RES {
	ST_TEST_ITEM_RES item_res_header;
	u8 test_res_buf[MAX_TEST_RES_LEN];
	u8 test_res_buf_len;
} ST_CHECK_MODULE_TEST_RES, *PST_CHECK_MODULE_TEST_RES;
extern s32 tp_check_module_test_result(ptr32 p_data);

/**********************************Recover Config To Chip************************************/
/*TP_RECOVER_CFG_ITEM_ID*/
#define RECOVER_CFG_ERROR_CODE_NONE		0	/*test ok*/
#define RECOVER_CFG_CFG_WRITE_ERROR		1
#define RECOVER_CFG_CFG_READ_ERROR		2
#define RECOVER_CFG_CMP_ERROR			3

/*test result*/
typedef struct RECOVER_CHIP_CFG_RES {
	ST_TEST_ITEM_RES item_res_header;
	u16 cfg_len;
	u8 *config_arr;
} ST_RECOVER_CHIP_CFG_RES, *PST_RECOVER_CHIP_CFG_RES;
extern s32 tp_recover_cfg_to_chip(ptr32 p_data);

/**********************************Save Result To Chip Storage************************************/
/*//TP_SAVE_RES_TO_IC_STORAGE_ITEM_ID
//--------------------Error Code---------------------//*/
#define SAVE_RES_TO_CHIP_ERROR_NONE			0
#define SAVE_RES_GET_TOTAL_RES_FAIL			1
#define SAVE_RES_WRITE_TO_CHIP_FAIL			2

/*test result*/
typedef struct SAVE_RES_TO_CHIP_RES {
	ST_TEST_ITEM_RES item_res_header;
	u8 test_res_buf[MAX_TEST_RES_LEN];
	u8 test_res_buf_len;
} ST_SAVE_RES_TO_CHIP_RES, *PST_SAVE_RES_TO_CHIP_RES;
extern s32 tp_save_res_to_chip(ptr32 p_data);

/**********************************Chip Uid Process************************************/
/*//TP_CHIP_UID_PROCESS_ITEM_ID
//--------------------Error Code---------------------//*/
#define CHIP_UID_PROC_ERROR_NONE		0
#define CHIP_UID_PROC_READ_ERROR		1
#define CHIP_UID_PROC_WRITE_ERROR		2

/*test result*/
#define MAX_CHIP_UID_LEN	32
typedef struct CHIP_UID_PROCESS_RES {
	ST_TEST_ITEM_RES item_res_header;
	u8 uid_len;
	u8 uid_arr[MAX_CHIP_UID_LEN];
} ST_CHIP_UID_PROCESS_RES, *PST_CHIP_UID_PROCESS_RES;
extern s32 tp_chip_uid_proc(ptr32 p_data);

/*************************************Private methods start********************************************/

/*************************************Private methods end********************************************/
#ifdef __cplusplus
}
#endif
#endif
#endif