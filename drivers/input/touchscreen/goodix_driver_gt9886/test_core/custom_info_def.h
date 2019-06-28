/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name          : custom_info_def.h
* Author             : Bob Huang
* Version            : V1.0.0
* Date               : 07/26/2017
* Description        : define custom info struct
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#ifndef CUSTOM_INFO_DEF_H
#define CUSTOM_INFO_DEF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "user_test_type_def.h"

#define CUSTOM_INFO_DESC_MAX_LEN	20
	typedef struct CUSTOM_INFO_BYTE {
		u8 index;
		u8 value;
		u8 desc[CUSTOM_INFO_DESC_MAX_LEN];
	} ST_CUSTOM_INFO_BYTE, *PST_CUSTOM_INFO_BYTE;

#ifdef __cplusplus
}
#endif
#endif
#endif
