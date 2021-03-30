/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _QCOM_SECURE_SYSTEM_HEAP_H
#define _QCOM_SECURE_SYSTEM_HEAP_H

#include <linux/dma-heap.h>
#include <linux/err.h>
#include "qcom_dynamic_page_pool.h"

struct qcom_secure_system_heap {
	struct dynamic_page_pool **pool_list;
	int vmid;
	struct list_head list;
};

#ifdef CONFIG_QCOM_DMABUF_HEAPS_SYSTEM_SECURE
int qcom_secure_system_heap_create(char *name, int vmid);
#else
static int qcom_secure_system_heap_create(char *name, int vmid)
{
	return 0;
}
#endif

#endif /* _QCOM_SECURE_SYSTEM_HEAP_H */
