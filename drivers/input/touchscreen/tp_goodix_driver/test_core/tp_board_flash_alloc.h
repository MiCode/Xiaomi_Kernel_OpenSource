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

#define USER_TEST_FLASH_MEM_START_ADDR 		(_STORAGE_MEM_ADDR)
#define USER_TEST_FLASH_MEM_LEN				(256*1024)

/*
//flash allocate algorithm
//little-endian model
//--------------Header-----------------//
// //
//      4 bytes Crc     (from item1~itemn)         //
//       4 bytes total parameter length    //
//-------------------------------------//

//------------test item1---------------//
//2 bytes test item id                             //
//2 bytes test item paramter len1          //
//len1 bytes for item1 parameter           //
//...4 bytes align                                         //
//-------------------------------------//

//------------test item2---------------//
//2 bytes test item id                             //
//2 bytes test item paramter len2          //
//len2 bytes for item1 parameter           //
//...4 bytes align                                         //
//-------------------------------------//

//------------test itemn---------------//
//2 bytes test item id                             //
//2 bytes test item paramter lenn          //
//lenn bytes for item1 parameter           //
//...4 bytes align                                         //
//-------------------------------------//
*/
#endif

#ifdef __cplusplus
}
#endif
#endif
#endif
