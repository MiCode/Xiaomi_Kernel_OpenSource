/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2013-2016 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef __MALI_MEMORY_DEFER_BIND_H_
#define __MALI_MEMORY_DEFER_BIND_H_


#include "mali_osk.h"
#include "mali_session.h"

#include <linux/list.h>
#include <linux/mm.h>
#include <linux/rbtree.h>
#include <linux/spinlock.h>
#include <linux/types.h>


#include "mali_memory_types.h"
#include "mali_memory_os_alloc.h"
#include "mali_uk_types.h"

struct mali_gp_job;

typedef struct mali_defer_mem {
	struct list_head node;   /*dlist node in bind manager */
	u32 flag;
} mali_defer_mem;


typedef struct mali_defer_mem_block {
	struct list_head free_pages; /* page pool */
	atomic_t num_free_pages;
} mali_defer_mem_block;

/* varying memory list need to bind */
typedef struct mali_backend_bind_list {
	struct list_head node;
	struct mali_mem_backend *bkend;
	u32 vaddr;
	u32 page_num;
	struct mali_session_data *session;
	u32 flag;
} mali_backend_bind_lists;


typedef struct mali_defer_bind_manager {
	atomic_t num_used_pages;
	atomic_t num_dmem;
} mali_defer_bind_manager;

_mali_osk_errcode_t mali_mem_defer_bind_manager_init(void);
void mali_mem_defer_bind_manager_destory(void);
_mali_osk_errcode_t mali_mem_defer_bind(struct mali_gp_job *gp, struct mali_defer_mem_block *dmem_block);
_mali_osk_errcode_t mali_mem_defer_bind_allocation_prepare(mali_mem_allocation *alloc, struct list_head *list,  u32 *required_varying_memsize);
_mali_osk_errcode_t mali_mem_prepare_mem_for_job(struct mali_gp_job *next_gp_job, mali_defer_mem_block *dblock);
void mali_mem_defer_dmem_free(struct mali_gp_job *gp);

#endif
