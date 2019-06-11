/******************** (C) COPYRIGHT 2018 Goodix ********************
* File Name          : simple_mem_manager_mach.cpp
* Author             :
* Version            : V1.0.0
* Date               : 05/14/2018
* Description        : we use this module manager memory,or support dynamic memory
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#include "simple_mem_manager.h"
#include "board_opr_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
* Function Name	: alloc_mem_block
* Description	: allocate memory block
* Input			: void** pp_buf(pointer to buffer pointer)
				: u16 buf_size (no more than 8K)
* Output		: void** pp_buf
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
extern s32 alloc_mem_block(IN_OUT void **pp_buf, IN u16 buf_size)
{
	s32 ret = 0;
	(*pp_buf) = (void *)malloc(buf_size * sizeof(u8));
	if (NULL != (*pp_buf)) {
		ret = 1;
	}
	return ret;
}

/*******************************************************************************
* Function Name	: free_mem_block
* Description	: free memory block
* Input			: void* p_buf
* Output		: none
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
extern s32 free_mem_block(IN void *p_buf)
{
	s32 ret = 0;
	if (p_buf != NULL) {
		ret = free_mem_in_heap(p_buf);
	}
	return ret;
}

/*******************************************************************************
* Function Name	: alloc_mem_in_heap
* Description	: alloc memory in heap
* Input			: void** pp_param(pointer to buffer pointer)
				: u32 data_size
* Output		: void** pp_param
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
extern s32 alloc_mem_in_heap(IN_OUT void **pp_param, IN u32 data_size)
{
	s32 ret = 0;
	(*pp_param) = (void *)malloc(data_size * sizeof(u8));
	if (NULL != (*pp_param)) {
		ret = 1;
	}

	return ret;
}

/*******************************************************************************
* Function Name	: free_mem_in_heap
* Description	: free memory in heap
* Input			: void* p_param(buffer pointer)
* Output		: void* p_param
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
extern s32 free_mem_in_heap(IN void *p_param)
{
	if (p_param != NULL) {
		free(p_param);
		p_param = NULL;
	}
	return 1;
}
/*******************************************************************************
* Function Name	: free_mem_in_heap_p
* Description	: free memory in heap
* Input			: void** pp_param(pointer to buffer pointer)
* Output		: void** pp_param
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
extern s32 free_mem_in_heap_p(IN_OUT void **pp_param)
{
	if (*pp_param != NULL) {
		free(*pp_param);
		*pp_param = NULL;
	}
	return 1;
}

#ifdef __cplusplus
}
#endif

#endif
