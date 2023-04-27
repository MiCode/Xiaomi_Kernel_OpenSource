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
#include <dt-bindings/memory/mtk-memory-port.h>

#define DUMP_INFO_LEN_MAX    (400)

/* Bit map */
#define HEAP_DUMP_SKIP_ATTACH     (1 << 0)
#define HEAP_DUMP_SKIP_RB_DUMP    (1 << 1)
#define HEAP_DUMP_HEAP_SKIP_POOL  (1 << 2)
#define HEAP_DUMP_STATS           (1 << 3)
#define HEAP_DUMP_DEC_1_REF       (1 << 4)
#define HEAP_DUMP_OOM             (1 << 5)

#define HANG_DMABUF_FILE_TAG	((void *)0x1)
typedef void (*hang_dump_cb)(const char *fmt, ...);
extern hang_dump_cb hang_dump_proc;

#define dmabuf_dump(file, fmt, args...)                \
	do {                                           \
		if (file == HANG_DMABUF_FILE_TAG) {             \
			if (hang_dump_proc != NULL)             \
				hang_dump_proc(fmt, ##args);    \
		}                                               \
		else if (file)                                  \
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

/* common function */
extern void dmabuf_release_check(const struct dma_buf *dmabuf);

#endif /* _MTK_DMABUFHEAP_DEBUG_H */
