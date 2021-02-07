/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name		: test_proc.h
* Author		: Bob Huang
* Version		: V1.0.0
* Date			: 07/26/2017
* Description	: structs in this file also exist in pc,it is an interface
					of test item
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#ifndef TP_ITEM_PERMUTATION_H
#define TP_ITEM_PERMUTATION_H

#include "test_item_def.h"

#ifdef __cplusplus
extern "C" {
#endif
/*sort item permutation */
extern s32 sort_item_permutation(PST_TEST_PROC_DATA p_test_proc_data);
extern s32 get_index_from_permutaion_arr(u16 item_id);

#ifdef __cplusplus
}
#endif
#endif
#endif
