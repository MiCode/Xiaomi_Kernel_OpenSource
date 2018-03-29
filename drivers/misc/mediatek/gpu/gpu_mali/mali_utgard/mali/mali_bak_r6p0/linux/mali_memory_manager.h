/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2013-2015 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef __MALI_MEMORY_MANAGER_H__
#define __MALI_MEMORY_MANAGER_H__

#include "mali_osk.h"
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/rbtree.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include "mali_memory_types.h"
#include "mali_memory_os_alloc.h"
#include "mali_uk_types.h"

struct mali_allocation_manager {
	rwlock_t vm_lock;
	struct rb_root allocation_mgr_rb;
	struct list_head head;
	struct mutex list_mutex;
};

extern struct idr mali_backend_idr;
extern struct mutex mali_idr_mutex;

int mali_memory_manager_init(struct mali_allocation_manager *mgr);
void mali_memory_manager_uninit(struct mali_allocation_manager *mgr);

void  mali_mem_allocation_struct_destory(mali_mem_allocation *alloc);

mali_mem_backend *mali_mem_backend_struct_search(struct mali_allocation_manager *mgr, u32 mali_address);
_mali_osk_errcode_t _mali_ukk_mem_allocate(_mali_uk_alloc_mem_s *args);
_mali_osk_errcode_t _mali_ukk_mem_free(_mali_uk_free_mem_s *args);
_mali_osk_errcode_t _mali_ukk_mem_bind(_mali_uk_bind_mem_s *args);
_mali_osk_errcode_t _mali_ukk_mem_unbind(_mali_uk_unbind_mem_s *args);
_mali_osk_errcode_t _mali_ukk_mem_cow(_mali_uk_cow_mem_s *args);
_mali_osk_errcode_t _mali_ukk_mem_cow_modify_range(_mali_uk_cow_modify_range_s *args);
_mali_osk_errcode_t _mali_ukk_mem_usage_get(_mali_uk_profiling_memory_usage_get_s *args);

#endif

