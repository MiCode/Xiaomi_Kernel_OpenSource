/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2013-2014, 2016 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef __MALI_GPU_VMEM_H__
#define __MALI_GPU_VMEM_H__

#include "mali_osk.h"
#include "mali_session.h"
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/rbtree.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include "mali_memory_types.h"
#include "mali_memory_os_alloc.h"
#include "mali_memory_manager.h"



int mali_vma_offset_add(struct mali_allocation_manager *mgr,
			struct mali_vma_node *node);

void mali_vma_offset_remove(struct mali_allocation_manager *mgr,
			    struct mali_vma_node *node);

struct mali_vma_node *mali_vma_offset_search(struct mali_allocation_manager *mgr,
		unsigned long start,    unsigned long pages);

#endif
