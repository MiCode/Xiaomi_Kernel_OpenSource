/******************** (C) COPYRIGHT 2018 Goodix ********************
* File Name          : save_test_data.h
* Author             :
* Version            : V1.0.0
* Date               : 28/03/2018
* Description        : save test data
*******************************************************************************/
#ifndef SAVE_TEST_DATA_H
#define SAVE_TEST_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "user_test_type_def.h"
#include "mxml.h"
#include "config.h"

#define		GT_SHORT					0x00400000
#define		VERSION_ERR					0x10000
#define		BEYOND_FLASH_LAYER_LIMIT	0x01000000
#define		SEND_CONFIG_ERR				0x00100000
#define		RECOVER_CONFIG_ERR			0x00200000
#define		TEST_ABORT					0x00000100


/***********************************print test data**************************************/
extern s32 print_rawdata_test_res(ptr32 p_data);
/*print short test result*/
extern s32 print_short_test_res(ptr32 p_data);
/*print version test result*/
extern s32 print_version_test_res(ptr32 p_data);
/*print flash test result*/
extern s32 print_flash_test_res(ptr32 p_data);
/*print send config test result*/
extern s32 print_send_cfg_test_res(ptr32 p_data);
/*print cfg recover test result*/
extern s32 print_cfg_recover_test_res(ptr32 p_data);
/**********************************print test data end**********************************/

#ifdef __cplusplus
}
#endif
#endif
