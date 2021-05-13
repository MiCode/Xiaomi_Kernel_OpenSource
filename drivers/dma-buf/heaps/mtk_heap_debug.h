/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 *
 */

#ifndef _MTK_DMABUFHEAP_DEBUG_H
#define _MTK_DMABUFHEAP_DEBUG_H

#include <linux/dma-heap.h>
#include <linux/dma-buf.h>
#include <linux/seq_file.h>
#include <linux/sched/clock.h>

#define DUMP_INFO_LEN_MAX    (1000)

#define dmabuf_dump(file, fmt, args...)                \
	do {                                           \
		if (file)                              \
			seq_printf(file, fmt, ##args); \
		else                                   \
			pr_info(fmt, ##args);          \
	} while(0)


struct mtk_heap_priv_info {
	char *(*get_buf_dump_str)(const struct dma_buf *dmabuf,
				  const struct dma_heap *heap);
	char *(*get_buf_dump_fmt)(const struct dma_heap *heap);
	void (*show)(struct dma_heap *heap, void *seq_file);
};

struct mtk_heap_dump_t {
	struct seq_file *file;
	struct dma_heap *heap;
};

static inline
unsigned long long get_current_time_ms(void) {
	unsigned long long cur_ts;

	cur_ts = sched_clock();
	do_div(cur_ts, 1000000000);
	return cur_ts;
}

static inline
struct mtk_heap_priv_info *mtk_heap_priv_get(struct dma_heap *heap) {
	struct dma_heap_export_info *exp_info = (typeof(exp_info))heap;
	if (unlikely(!exp_info))
		return NULL;

	return (struct mtk_heap_priv_info *)exp_info->priv;
}

#endif /* _MTK_DMABUFHEAP_DEBUG_H */
