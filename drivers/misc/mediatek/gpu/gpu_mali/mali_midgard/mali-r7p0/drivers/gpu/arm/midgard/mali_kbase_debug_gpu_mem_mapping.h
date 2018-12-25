/*
 * (C) COPYRIGHT 2015 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */

#ifndef _KBASE_SHOW_MEM_MAPPING_H
#define _KBASE_SHOW_MEM_MAPPING_H

#include <linux/types.h>
#include <mali_kbase.h>

phys_addr_t kbase_debug_gpu_mem_mapping(struct kbase_context *kctx, u64 va);
bool kbase_debug_gpu_mem_mapping_check_pa(u64 pa);

#endif /* _KBASE_SHOW_MEM_MAPPING_H */
