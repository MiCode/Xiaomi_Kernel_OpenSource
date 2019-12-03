/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name		: public_func.h
* Author		: Bob Huang
* Version		: V1.0.0
* Date			: 10/23/2017
* Description	: this file contain some public functions that do not depend
						on platform such as Windows,Arm or Android
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#ifndef PUBLIC_FUN_H
#define PUBLIC_FUN_H

#include "user_test_type_def.h"
#include "test_item_def.h"

#ifdef __cplusplus
extern "C" {
#endif

extern s32 compare_buffer(u8 *src_buf, u8 *dest_buf, u8 size);
extern s32 current_test_status(u8 res, P_TEST_ITEM_RES p_res);

#ifdef __cplusplus
}
#endif
#endif
#endif
