/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name          : simple_mem_manager.h
* Author             : Bob Huang
* Version            : V1.0.0
* Date               : 08/23/2017
* Description        : we use this module manger memory pool
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#ifndef SIMPLE_MEM_MANAGER_H
#define SIMPLE_MEM_MANAGER_H

#include "user_test_type_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**********************memory manger in stack*******************************/
extern s32 alloc_mem_block(void **pp_buf, u16 buf_size);
extern s32 free_mem_block(void *p_buf);

/**********************memory manger in heap*******************************/
extern s32 alloc_mem_in_heap(void **pp_param, u32 data_size);
extern s32 free_mem_in_heap(void *p_param);

extern s32 free_mem_in_heap_p(void **pp_param);

#ifdef __cplusplus
}
#endif
#endif
#endif
