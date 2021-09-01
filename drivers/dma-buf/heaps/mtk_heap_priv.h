/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 *
 */

#ifndef _MTK_DMABUFHEAP_DEBUG_H
#define _MTK_DMABUFHEAP_DEBUG_H

#include <linux/dma-heap.h>
#include <linux/dma-buf.h>
#include <linux/dma-resv.h>

#include <linux/seq_file.h>
#include <linux/sched/clock.h>

#define DUMP_INFO_LEN_MAX    (400)
#define BUF_PRIV_MAX_CNT         MTK_M4U_DOM_NR_MAX

/* Bit map */
#define HEAP_DUMP_SKIP_ATTACH     (1 << 0)
#define HEAP_DUMP_SKIP_FD         (1 << 1)

#define dmabuf_dump(file, fmt, args...)                \
	do {                                           \
		if (file)                              \
			seq_printf(file, fmt, ##args); \
		else                                   \
			pr_info(fmt, ##args);          \
	} while (0)

/* mtk_heap private info, used for dump */
struct mtk_heap_priv_info {
	/* used for heap dump */
	void (*show)(struct dma_heap *heap, void *seq_file, int flag);

	/* used for buffer dump */
	int (*buf_priv_dump)(const struct dma_buf *dmabuf,
			     struct dma_heap *heap,
			     void *seq_file);
};

struct dma_heap_attachment {
	struct device *dev;
	struct sg_table *table;
	struct list_head list;
	bool mapped;

	bool uncached;
};

struct mtk_heap_dev_info {
	struct device           *dev;
	enum dma_data_direction direction;
	unsigned long           map_attrs;
};

/* copy from struct system_heap_buffer */
struct sys_heap_buf_debug_use {
	struct dma_heap *heap;
	struct list_head attachments;
	struct mutex lock;
	unsigned long len;
};

#endif /* _MTK_DMABUFHEAP_DEBUG_H */
