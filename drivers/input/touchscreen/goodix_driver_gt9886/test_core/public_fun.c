/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name          : public_func.cpp
* Author             : Bob Huang
* Version            : V1.0.0
* Date               : 10/23/2017
* Description        : this file contain some public functions that do not depend
						on platform such as Windows,Arm or Android
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#ifdef __cplusplus
extern "C" {
#endif

#include "public_fun.h"

/*******************************************************************************
* Function Name	: compare_buffer
* Description	: compare_buffer
* Input			: u8* src_buf
				 : u8* dest_buf
				 : u8  size
* Output		: none
* Return 		: src_buf == dest_buf 1 else return 0
*******************************************************************************/
extern s32 compare_buffer(IN u8 *src_buf, IN u8 *dest_buf, IN u8 size)
{
	u8 index;
	for (index = 0; index < size; size++) {
		if (src_buf[index] != dest_buf[index]) {
			return 0;
		}
	} return 1;
}

/*******************************************************************************
* Function Name	: current_test_status
* Description	: compare_buffer
* Input			: IN u8 res
* Output		: IN_OUT P_TEST_ITEM_RES p_res
* Return		: 1,0
*******************************************************************************/
extern s32 current_test_status(IN u8 res, IN_OUT P_TEST_ITEM_RES p_res)
{
	if (res == 1)
		return 1;
	else {
		p_res->test_status = TEST_ABORTED;
		p_res->test_result = TEST_WARNING;
	}
	return 0;
}

#ifdef __cplusplus
}
#endif

#endif
