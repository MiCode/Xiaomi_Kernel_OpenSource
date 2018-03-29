/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2015 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef __MALI_MEMORY_UMP_BUF_H__
#define __MALI_MEMORY_UMP_BUF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "mali_uk_types.h"
#include "mali_osk.h"
#include "mali_memory.h"

int mali_mem_bind_ump_buf(mali_mem_allocation *alloc, mali_mem_backend *mem_backend, u32  secure_id, u32 flags);
void mali_mem_unbind_ump_buf(mali_mem_backend *mem_backend);

#ifdef __cplusplus
}
#endif

#endif /* __MALI_MEMORY_DMA_BUF_H__ */
