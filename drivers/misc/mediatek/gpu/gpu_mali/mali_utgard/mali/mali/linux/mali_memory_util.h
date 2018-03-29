/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2013-2016 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef __MALI_MEMORY_UTIL_H__
#define __MALI_MEMORY_UTIL_H__

u32 mali_allocation_unref(struct mali_mem_allocation **alloc);

void mali_allocation_ref(struct mali_mem_allocation *alloc);

void mali_free_session_allocations(struct mali_session_data *session);

#endif
