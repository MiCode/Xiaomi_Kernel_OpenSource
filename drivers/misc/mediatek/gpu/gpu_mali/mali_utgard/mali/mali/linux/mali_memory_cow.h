/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2013-2016 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef __MALI_MEMORY_COW_H__
#define __MALI_MEMORY_COW_H__

#include "mali_osk.h"
#include "mali_session.h"
#include "mali_memory_types.h"

int mali_mem_cow_cpu_map(mali_mem_backend *mem_bkend, struct vm_area_struct *vma);
_mali_osk_errcode_t mali_mem_cow_cpu_map_pages_locked(mali_mem_backend *mem_bkend,
		struct vm_area_struct *vma,
		unsigned long vaddr,
		int num);

_mali_osk_errcode_t mali_memory_do_cow(mali_mem_backend *target_bk,
				       u32 target_offset,
				       u32 target_size,
				       mali_mem_backend *backend,
				       u32 range_start,
				       u32 range_size);

_mali_osk_errcode_t mali_memory_cow_modify_range(mali_mem_backend *backend,
		u32 range_start,
		u32 range_size);

_mali_osk_errcode_t mali_memory_cow_os_memory(mali_mem_backend *target_bk,
		u32 target_offset,
		u32 target_size,
		mali_mem_backend *backend,
		u32 range_start,
		u32 range_size);

void _mali_mem_cow_copy_page(mali_page_node *src_node, mali_page_node *dst_node);

int mali_mem_cow_mali_map(mali_mem_backend *mem_bkend, u32 range_start, u32 range_size);
u32 mali_mem_cow_release(mali_mem_backend *mem_bkend, mali_bool is_mali_mapped);
_mali_osk_errcode_t mali_mem_cow_allocate_on_demand(mali_mem_backend *mem_bkend, u32 offset_page);
#endif

