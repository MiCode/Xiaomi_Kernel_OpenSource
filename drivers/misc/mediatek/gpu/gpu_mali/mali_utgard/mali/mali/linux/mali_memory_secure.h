/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010, 2013, 2015-2016 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef __MALI_MEMORY_SECURE_H__
#define __MALI_MEMORY_SECURE_H__

#include "mali_session.h"
#include "mali_memory.h"
#include <linux/spinlock.h>

#include "mali_memory_types.h"

_mali_osk_errcode_t mali_mem_secure_attach_dma_buf(mali_mem_secure *secure_mem, u32 size, int mem_fd);

_mali_osk_errcode_t mali_mem_secure_mali_map(mali_mem_secure *secure_mem, struct mali_session_data *session, u32 vaddr, u32 props);

void mali_mem_secure_mali_unmap(mali_mem_allocation *alloc);

int mali_mem_secure_cpu_map(mali_mem_backend *mem_bkend, struct vm_area_struct *vma);

u32 mali_mem_secure_release(mali_mem_backend *mem_bkend);

#endif /* __MALI_MEMORY_SECURE_H__ */
