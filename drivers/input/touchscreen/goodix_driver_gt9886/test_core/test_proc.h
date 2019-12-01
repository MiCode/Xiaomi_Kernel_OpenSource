/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name          : test_proc.h
* Author             : Bob Huang
* Version            : V1.0.0
* Date               : 07/26/2017
* Description        : structs in this file also exist in pc,it is an interface
					   of test item
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#ifndef TEST_PROC_H
#define TEST_PROC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "test_item_def.h"
/*get array index from sub test item array*/
extern s32 get_arr_id_from_sub_item(PST_TEST_ITEM p_test_item, u8 sub_item_id);
/*test proc func*/
extern s32 test_proc_func(PST_TEST_PROC_DATA p_test_proc_data);
extern s32 get_test_item_by_another_item(u16 item_id,
		PST_TEST_ITEM p_src_test_item, PST_TEST_ITEM * pp_des_test_item);
extern s32 register_test_item_func(PST_TEST_PROC_DATA p_test_proc_data);
extern s32 get_test_item_from_proc_data(PST_TEST_PROC_DATA
		p_test_proc_data, u16 test_item_id, PST_TEST_ITEM *pp_test_item);

extern void test_before_func_fail_proc(P_TEST_ITEM_RES p_test_res_head);
extern s32 judge_test_item_legal(PST_TEST_ITEM p_test_item);
/*start test interface*/
extern s32 start_test_interface(ptr32 ptr);
/*stop test interface*/
extern s32 stop_test_interface(ptr32 ptr);
#ifdef __cplusplus
}
#endif
#endif
#endif
