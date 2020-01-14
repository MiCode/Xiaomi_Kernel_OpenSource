/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name          : tp_board_flash_alloc.h
* Author             : Bob Huang
* Version            : V1.0.0
* Date               : 07/26/2017
* Description        : test parameter store in flash must obey this file's rules
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#ifndef TP_BOARD_FLASH_ALLOC_H
#define TP_BOARD_FLASH_ALLOC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "user_test_type_def.h"

#if ARM_CODE == 1
#include "flash_mem_alloc_def.h"
#include "user_test_type_def.h"

#define USER_TEST_FLASH_MEM_START_ADDR		(_STORAGE_MEM_ADDR)
#define USER_TEST_FLASH_MEM_LEN				(256*1024)

#endif

#ifdef __cplusplus
}
#endif
#endif
#endif
