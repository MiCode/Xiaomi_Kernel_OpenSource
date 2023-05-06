/* SPDX-License-Identifier: GPL-2.0
 */
#ifndef _QCOM_EXTEND_SYSTEM_HEAP_H
#define _QCOM_EXTEND_SYSTEM_HEAP_H


extern void qcom_sys_heap_reserve_pool_init(const char *name,
		struct qcom_system_heap *sys_heap);
extern struct page *qcom_extend_sys_heap_alloc_largest_available(
		struct qcom_system_heap *sys_heap,
		unsigned long size, unsigned int max_order);
extern bool need_free_to_reserve_pool(struct qcom_system_heap *sys_heap, int order_index);

#endif /* _QCOM_EXTEND_SYSTEM_HEAP_H */
