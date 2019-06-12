/******************** (C) COPYRIGHT 2018 Goodix ********************
* File Name				: test_item_init.h
* Author				: Zhitao Yang
* Version				: V1.0.0
* Date					: 28/03/2018
* Description			: initial test item
*******************************************************************************/
#ifndef TEST_ITEM_INIT_H
#define TEST_ITEM_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "user_test_type_def.h"
#include "test_item_def.h"
#include "test_order.h"

/*************************************init open test item********************************************/
extern s32 rawdata_test_before_func(ptr32 p_data, ptr32 p_param);
extern s32 release_rawdata_test_item(ptr32 p_data);
/*************************************init open test item end*****************************************/

/*************************************init short test item********************************************/
/*init short test param*/
extern s32 short_test_before_func(ptr32 p_data, ptr32 p_param);
extern s32 get_short_param(IN_OUT PST_TEST_ITEM p_test_item,
				PST_TEST_PARAM p_test_param);
extern s32 release_short_test_item(IN_OUT ptr32 p_data);

/*************************************init short test item end*****************************************/

/*************************************init version test item********************************************/
extern s32 version_test_before_func(ptr32 p_data, ptr32 p_param);
extern s32 version_test_finished_func(ptr32 p_data);
/*************************************init version test item end*****************************************/

/*************************************init flash test item********************************************/
extern s32 flash_test_before_func(ptr32 p_data, ptr32 p_param);
extern s32 release_flash_test_item(ptr32 p_data);

/*************************************init flash test item end*****************************************/

/*************************************init send config test item********************************************/
extern s32 send_cfg_test_before_func(ptr32 p_data, ptr32 p_param);
/*init send config test param*/
extern s32 get_send_cfg_param_mess(PST_TEST_ITEM p_test_item,
					PST_TEST_PARAM p_test_param);
extern s32 release_cfg_test_item(ptr32 p_data);
extern s32 release_send_cfg_param(PST_TEST_ITEM p_test_item);
/*************************************init send config test item end*****************************************/

/*************************************init recover config test item********************************************/
extern s32 cfg_recover_test_before_func(ptr32 p_data);
/*init cfg recover test param*/
extern s32 get_recover_cfg_param_mess(PST_TEST_ITEM p_test_item);
extern s32 release_cfg_recover_test_item(ptr32 p_data);
extern s32 release_cfg_recover_param(PST_TEST_ITEM p_test_item);
/*************************************init recover config test item end*****************************************/
#ifdef __cplusplus
}
#endif
#endif
