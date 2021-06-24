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

#define DUMP_INFO_LEN_MAX    (400)
#define SKIP_DMBUF_BUFFER_DUMP

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

/* Make sure same with system heap */
struct dma_heap_attachment {
	struct device *dev;
	struct sg_table *table;
	struct list_head list;
	bool mapped;

	bool uncached;
};


/* copy from struct system_heap_buffer */
struct sys_heap_buf_debug_use {
	struct dma_heap *heap;
	struct list_head attachments;
	struct mutex lock;
	unsigned long len;
};

struct mtk_heap_dump_t {
	struct seq_file *file;
	struct dma_heap *heap;
	long ret; /* used for storing return value */
};

struct heap_status_s {
	const char *heap_name;
	int heap_exist;
};

#if !IS_ENABLED(CONFIG_PROC_FS)
static inline int dma_buf_init_procfs(void)
{
	return 0;
}
static inline void dma_buf_uninit_procfs(void)
{
}
static inline void dma_buf_procfs_reinit(void)
{
}

#endif

static inline
unsigned long long get_current_time_ms(void) {
	unsigned long long cur_ts;

	cur_ts = sched_clock();
	do_div(cur_ts, 1000000);
	return cur_ts;
}

static inline
struct mtk_heap_priv_info *mtk_heap_priv_get(struct dma_heap *heap) {
	struct dma_heap_export_info *exp_info = (typeof(exp_info))heap;
	if (unlikely(!exp_info))
		return NULL;

	return (struct mtk_heap_priv_info *)exp_info->priv;
}

/* common function */
int dma_heap_default_attach_dump_cb(const struct dma_buf *dmabuf,
				    void *priv)
{
	struct mtk_heap_dump_t *dump_param = priv;
	struct seq_file *s = dump_param->file;
	struct dma_heap *dump_heap = dump_param->heap;
	struct sys_heap_buf_debug_use *buf = dmabuf->priv;
	struct dma_buf_attachment *attach_obj;
	struct dma_heap_attachment *a;
	/* dmabuf check */
	if (!buf || !buf->heap || buf->heap != dump_heap)
		return 0;

	dmabuf_dump(s, "\tinode:%-8d\tsize:0x%-8d =======>\n",
		    file_inode(dmabuf->file)->i_ino,
		    dmabuf->size);
	/*
	 * iova here use sgt in dma_buf_attachment,
	 * need set "cache_sgt_mapping = 1" for dmabuf_ops
	 */
	list_for_each_entry(attach_obj, &dmabuf->attachments, node) {
		a = attach_obj->priv;
		if (unlikely(!a)) {
			dmabuf_dump(s, "\t%s\n", dev_name(attach_obj->dev));
			continue;
		}
		dmabuf_dump(s, "\t%-16s\t0x%-16p\t0x%-16lx\t%-16lu\t%-8d"
#ifdef CONFIG_DMABUF_SYSFS_STATS
			    "%-8d"
#endif
			    "\n",
			    dev_name(attach_obj->dev),
			    sg_dma_address(a->table->sgl),
			    attach_obj->sgt,
			    attach_obj->dma_map_attrs,
			    attach_obj->dir
#ifdef CONFIG_DMABUF_SYSFS_STATS
			    , attach_obj->sysfs_entry->map_counter
#endif
			    );
	}
	dmabuf_dump(s, "\n");

	return 0;

}


static int dma_heap_total_cb(const struct dma_buf *dmabuf,
			     void *priv)
{
	struct mtk_heap_dump_t *dump_info = (typeof(dump_info))priv;
	const char *d_heap_name = dma_heap_get_name(dump_info->heap);

	/* not match */
	if (strncmp(dmabuf->exp_name, d_heap_name, strlen(d_heap_name)))
		return 0;

	dump_info->ret += dmabuf->size;

	return 0;
}

static long get_dma_heap_buffer_total(struct dma_heap *heap)
{
	struct mtk_heap_dump_t dump_info;

	if (!heap)
		return -1;

	dump_info.file = NULL;
	dump_info.heap = heap;
	dump_info.ret = 0; /* used to record total size */

	get_each_dmabuf(dma_heap_total_cb, (void *)&dump_info);

	return dump_info.ret;
}


#define __HEAP_DUMP_START(s, heap)                         \
/*
 * dmabuf_dump(s, "[%s] dmabuf heap show START @ %llu ms\n",  \
 *             dma_heap_get_name(heap), get_current_time_ms())
 */

#define __HEAP_DUMP_END(s, heap)                           \
/*
 * dmabuf_dump(s, "[%s] dmabuf heap show END @ %llu ms\n",    \
 *             dma_heap_get_name(heap), get_current_time_ms())
 */

/*
 * TODO:
 * add a heap total count, don't do for loop when need it.
 */
#define __HEAP_TOTAL_BUFFER_SZ_DUMP(s, heap)          \
dmabuf_dump(s, "[%s] buffer total size: %ld KB\n",    \
	    dma_heap_get_name(heap),                  \
	    get_dma_heap_buffer_total(heap)*4/PAGE_SIZE)

#define __HEAP_BUF_DUMP_START(s, heap)                    \
dmabuf_dump(s, "[%s] buffer dump start @%llu ms\n",       \
	    dma_heap_get_name(heap), get_current_time_ms())

#if defined(CONFIG_DMABUF_SYSFS_STATS)
#define __HEAP_ATTACH_DUMP_STAT(s, heap)                                   \
do {                                                                       \
	dmabuf_dump(s, "[%s] attach list dump Start @%llu ms\n",           \
		    dma_heap_get_name(heap), get_current_time_ms());       \
	dmabuf_dump(s, "\t%-16s\t%-16s\t%-16s\t%-16s\t%-8s\t%s\n",         \
		    "Dev", "iova", "sgt", "attrs", "dir", "map_iova_cnt"); \
} while (0)

#else

#define __HEAP_ATTACH_DUMP_STAT(s, heap)                             \
do {                                                                 \
	dmabuf_dump(s, "[%s] attach list dump Start @%llu ms\n",     \
		    dma_heap_get_name(heap), get_current_time_ms()); \
	dmabuf_dump(s, "\t%-16s\t%-16s\t%-16s\t%-16s\t%-8s\n",       \
		    "Dev", "iova", "sgt", "attrs", "dir");           \
} while (0)
#endif

#define __HEAP_ATTACH_DUMP_END(s, heap)                              \
	dmabuf_dump(s, "[%s] attachment list dump End @%llu ms\n",   \
		    dma_heap_get_name(heap), get_current_time_ms())

#define __HEAP_PAGE_POOL_DUMP(s, heap)                                       \
{                                                                            \
	struct dma_heap_export_info *exp_info = (typeof(exp_info))heap;      \
	if (exp_info->ops->get_pool_size) {                                  \
		dmabuf_dump(s, "[%s] page_pool size: %ld KB\n",              \
			    dma_heap_get_name(heap),                         \
			    exp_info->ops->get_pool_size(heap)*4/PAGE_SIZE); \
	} else {                                                             \
		dmabuf_dump(s, "[%s] No page_pool data\n",                   \
			    dma_heap_get_name(heap));                        \
	}                                                                    \
}

#endif /* _MTK_DMABUFHEAP_DEBUG_H */
