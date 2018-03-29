/*
* Copyright (C) 2011-2014 MediaTek Inc.
* 
* This program is free software: you can redistribute it and/or modify it under the terms of the 
* GNU General Public License version 2 as published by the Free Software Foundation.
* 
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __MTK_PP_H__
#define __MTK_PP_H__

#include <linux/mutex.h>	// spinlock
#include "mali_kernel_common.h"	// MALI_DEBUG_PRINT

#if defined(MTK_DEBUG) && defined(MTK_DEBUG_PROC_PRINT)

#define _MTKPP_DEBUG_LOG(...)	MALI_DEBUG_PRINT(0, (__VA_ARGS__))

#if defined(__GNUC__)
#define MTK_PP_FORMAT_PRINTF(x,y)		__attribute__((format(printf,x,y)))
#else
#define MTK_PP_FORMAT_PRINTF(x,y)
#endif

typedef enum MTKPP_ID_TAG
{
    MTKPP_ID_SYNC,
    
    MTKPP_ID_SIZE
} MTKPP_ID;

typedef enum MTKPP_BUFFERTYPE_TAG
{
    MTKPP_BUFFERTYPE_QUEUEBUFFER,
    MTKPP_BUFFERTYPE_RINGBUFFER
} MTKPP_BUFFERTYPE;

typedef struct MTK_PROC_PRINT_DATA_TAG
{
	MTKPP_BUFFERTYPE type;
	
	char *data;
	char **line;
	int data_array_size;
	int line_array_size;
	int current_data;
	int current_line;
		
	spinlock_t lock;
	unsigned long irqflags;
	
	void (*pfn_print)(struct MTK_PROC_PRINT_DATA_TAG *data, const char *fmt, ...) MTK_PP_FORMAT_PRINTF(2,3);
	
} MTK_PROC_PRINT_DATA;

void MTKPP_Init(void);
	
void MTKPP_Deinit(void);

MTK_PROC_PRINT_DATA *MTKPP_GetData(MTKPP_ID id);

#define MTKPP_LOG(id, ...) 																\
	do {																				\
		MTK_PROC_PRINT_DATA *mtkpp_data = MTKPP_GetData(id); 							\
		if (mtkpp_data != NULL) { mtkpp_data->pfn_print(mtkpp_data, __VA_ARGS__); }		\
	} while(0);
	
#endif 

#if !defined(MTK_DEBUG) || !defined(MTK_DEBUG_PROC_PRINT)

#define MTKPP_LOG(...)

#endif

#endif	/* __MTK_PP_H__ */

/******************************************************************************
 End of file (mtk_pp.h)
******************************************************************************/

