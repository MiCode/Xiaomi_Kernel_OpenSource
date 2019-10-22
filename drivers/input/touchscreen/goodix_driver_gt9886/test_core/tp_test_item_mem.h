/******************** (C) COPYRIGHT 2017 Goodix ********************
* File Name          : tp_test_item_mem.cpp.h
* Author             : Bob Huang
* Version            : V1.0.0
* Date               : 01/11/2018
* Description        : Tp Test Item Result Memory Process
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE
#ifndef TP_TEST_ITEM_MEM_H
#define TP_TEST_ITEM_MEM_H

#include "test_item_def.h"

#ifdef __cplusplus
extern "C" {
#endif

extern s32 alloc_test_item_res(PST_TEST_ITEM *pp_test_item,
				u16 item_num);
extern s32 free_test_item_res(PST_TEST_ITEM *pp_test_item,
				u16 item_num);

extern s32 alloc_item_res_header(PST_TEST_ITEM p_test_item);
extern s32 free_item_res_header(PST_TEST_ITEM p_test_item);
extern s32 alloc_rawdata_test_item_res(PST_TEST_ITEM p_test_item);
extern s32 free_rawdata_test_item_res(PST_TEST_ITEM p_test_item);
extern s32 alloc_diffdata_test_item_res(PST_TEST_ITEM p_test_item);
extern s32 free_diffdata_test_item_res(PST_TEST_ITEM p_test_item);
extern s32 alloc_short_test_item_res(PST_TEST_ITEM p_test_item);
extern s32 free_short_test_item_res(PST_TEST_ITEM p_test_item);
extern s32 alloc_version_test_item_res(PST_TEST_ITEM p_test_item);
extern s32 free_version_test_item_res(PST_TEST_ITEM p_test_item);
extern s32 alloc_chip_cfg_proc_item_res(PST_TEST_ITEM p_test_item);
extern s32 free_chip_cfg_proc_item_res(PST_TEST_ITEM p_test_item);
extern s32 alloc_flash_test_item_res(PST_TEST_ITEM p_test_item);
extern s32 free_flash_test_item_res(PST_TEST_ITEM p_test_item);
extern s32 alloc_recover_cfg_item_res(PST_TEST_ITEM p_test_item);
extern s32 free_recover_cfg_item_res(PST_TEST_ITEM p_test_item);

#ifdef __cplusplus
}
#endif
#endif
#endif
