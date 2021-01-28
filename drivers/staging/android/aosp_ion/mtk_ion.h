/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  Copyright (C) 2019 MediaTek Inc.
 */

#ifndef _MTK_ION_H
#define _MTK_ION_H

#include "ion.h"

struct ion_buf_info {
	struct list_head map_list;
	struct mutex map_lock;/* buffer map lock */
	struct dma_buf *dmabuf;
};

struct ion_dma_map_info {
	struct list_head link;
	struct dma_buf *dmabuf;
	struct device *dev;
	dma_addr_t  dma_addr;
	unsigned long buf_addr;
	size_t size;
};

#ifdef CONFIG_MTK_ION_DEBUG

int ion_sys_heap_debug_show(struct seq_file *s, void *unused);

#else /* CONFIG_MTK_ION_DEBUG */

int ion_sys_heap_debug_show(struct seq_file *s, void *unused)
{
	return 0;
}

#endif
#endif /* _MTK_ION_H */
