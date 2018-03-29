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

#ifndef __MTK_MEM_RECORD_H__
#define __MTK_MEM_RECORD_H__

#if defined (__cplusplus)
extern "C" {
#endif

void MTKMemRecordAdd(int i32Bytes);
void MTKMemRecordRemove(int i32Bytes);
int MTKMemRecordInit(void);
void MTKMemRecordDeInit(void);


#if defined (__cplusplus)
}
#endif

#endif	/* __MTK_MEM_RECORD_H__ */

/******************************************************************************
 End of file (mtk_mem_record.h)
******************************************************************************/

