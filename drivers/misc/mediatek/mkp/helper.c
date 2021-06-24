// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include "helper.h"

int is_module_or_bpf_addr(/*unsigned long x */const void *x)
{
#if (defined(CONFIG_MODULES) && defined(MODULES_VADDR)) || defined(BPF_JIT_REGION_START)
	unsigned long addr;
#endif

#if defined(CONFIG_MODULES) && defined(MODULES_VADDR)
	addr = (unsigned long)x;
	if (addr >= MODULES_VADDR && addr < MODULES_END)
		return MKP_DEMO_MODULE_CASE;
#endif
#if defined(BPF_JIT_REGION_START)
	addr = (unsigned long)x;
	if (addr >= BPF_JIT_REGION_START && addr < BPF_JIT_REGION_END)
		return MKP_DEMO_BPF_CASE;
#endif
	/* If it is not in the bpf or module region
	 * return 0
	 */
	return 0;
}
int mkp_set_mapping_xxx_helper(unsigned long addr, int nr_pages, uint32_t policy,
	int (*set_memory)(uint32_t policy, uint32_t handle))
{
	struct mkp_rb_node *data = NULL, *found = NULL;
	int ret = 0;
	uint32_t handle = 0;

	if (is_module_or_bpf_addr((void *)addr)) {
		int i;
		unsigned long pfn_cur = 0, pfn_next = 0;
		unsigned long start_pfn = 0, count = 1;
		phys_addr_t phys_addr = 0;

		for (i = 0; i < nr_pages; i++) {
			pfn_cur = vmalloc_to_pfn((void *)(addr+i*PAGE_SIZE));
			if (start_pfn == 0)
				start_pfn = pfn_cur;
			phys_addr = start_pfn << PAGE_SHIFT;
			found = mkp_rbtree_search(&mkp_rbtree, phys_addr);
			if (found != NULL) {
				if (found->addr == 0 && found->size == 0) {
					start_pfn = 0; count = 1;
					continue;
				}
				handle = found->handle;
				ret = set_memory(policy, handle);
				i = i + ((found->size) >> PAGE_SHIFT)-1;
				start_pfn = 0; count = 1;
			} else {
				if (i != nr_pages-1) { // not last two pages
					// check pfn_next is contiguous with pfn_cur
					pfn_next = vmalloc_to_pfn((void *)(addr+(i+1)*PAGE_SIZE));
					if (pfn_next == pfn_cur+1) {
						count++;
						continue;
					}
				}
				phys_addr = start_pfn << PAGE_SHIFT;
				handle = mkp_create_handle(policy, (unsigned long)phys_addr,
					count*PAGE_SIZE);
				if (handle == 0) {
					pr_info("%s:%d: Create handle fail\n", __func__, __LINE__);
					pr_info("pa: %pa, count: %lu\n", &phys_addr, count);
				}
				ret = set_memory(policy, handle);
				data = kmalloc(sizeof(struct mkp_rb_node), GFP_KERNEL);
				data->addr = phys_addr;
				data->size = count*PAGE_SIZE;
				data->handle = handle;
				ret = mkp_rbtree_insert(&mkp_rbtree, data);
				start_pfn = 0; count = 1;
			}
		}
	}
	return ret;
}
