/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _QCOM_SYSTEM_HEAP_H
#define _QCOM_SYSTEM_HEAP_H

#ifdef CONFIG_QCOM_DMABUF_HEAPS_SYSTEM
int qcom_system_heap_create(void);
#else
static int qcom_system_heap_create(void)
{
	return 1;
}
#endif

#endif /* _QCOM_SYSTEM_HEAP_H */
