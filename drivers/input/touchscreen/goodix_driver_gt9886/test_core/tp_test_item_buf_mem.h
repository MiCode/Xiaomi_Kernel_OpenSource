/******************** (C) COPYRIGHT 2017 Goodix ********************
* File Name			: tp_test_item_buf_mem.h
* Author			: Bob Huang
* Version			: V1.0.0
* Date				: 01/11/2018
* Description		: Tp Test Item Current Buffer Memory Process
*******************************************************************************/
#ifndef TP_TEST_ITEM_BUF_MEM_H
#define TP_TEST_ITEM_BUF_MEM_H

#include "test_item_def.h"

#ifdef __cplusplus
extern "C" {
#endif
s32 alloc_test_item_cur_buf(PST_TEST_ITEM *pp_test_item, u16 item_num);
s32 free_test_item_cur_buf(PST_TEST_ITEM *pp_test_item, u16 item_num);

#ifdef __cplusplus
}
#endif
#endif
