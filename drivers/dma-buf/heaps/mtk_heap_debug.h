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
#include "mtk_heap.h"

#define DUMP_INFO_LEN_MAX    (400)
#define SKIP_DMBUF_BUFFER_DUMP

/* Bit map */
#define HEAP_DUMP_SKIP_ATTACH     (1 << 0)
#define HEAP_DUMP_SKIP_FD         (1 << 1)

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
	void (*show)(struct dma_heap *heap, void *seq_file, int flag);
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
unsigned long long get_current_time_ms(void)
{
	unsigned long long cur_ts;

	cur_ts = sched_clock();
	do_div(cur_ts, 1000000);
	return cur_ts;
}

static inline
struct mtk_heap_priv_info *mtk_heap_priv_get(struct dma_heap *heap)
{
	struct dma_heap_export_info *exp_info = (typeof(exp_info))heap;
	if (unlikely(!exp_info))
		return NULL;

	return (struct mtk_heap_priv_info *)exp_info->priv;
}

/* common function */
int is_dmabuf_from_heap(struct dma_buf *dmabuf, struct dma_heap *heap)
{

	struct sys_heap_buf_debug_use *heap_buf;
	struct dma_heap *match_heap = heap;

	if (!dmabuf || !dmabuf->priv || !match_heap)
		return 0;
	heap_buf = dmabuf->priv;

	return (heap_buf->heap == match_heap);
}

int dma_heap_default_attach_dump_cb(const struct dma_buf *dmabuf,
				    void *priv)
{
	struct mtk_heap_dump_t *dump_param = priv;
	struct seq_file *s = dump_param->file;
	struct dma_heap *dump_heap = dump_param->heap;
	struct sys_heap_buf_debug_use *buf = dmabuf->priv;
	struct dma_buf_attachment *attach_obj;
	int ret;
	dma_addr_t iova = 0x0;
	/*
	 * if heap is NULL, dump all buffer
	 * if heap is not NULL, dump matched buffer
	 */
	if (dump_heap && (!buf || buf->heap != dump_heap))
		return 0;

	ret = dma_resv_lock_interruptible(dmabuf->resv, NULL);
	if (ret)
		return 0;

	dmabuf_dump(s, "\tinode:%-8d\tsize:0x%-8lx\tcount:%ld\tcache_sg:%d\texp:%s\tname:%s\n",
		    file_inode(dmabuf->file)->i_ino,
		    dmabuf->size,
		    file_count(dmabuf->file),
		    dmabuf->ops->cache_sgt_mapping,
		    dmabuf->exp_name?:"NULL",
		    dmabuf->name?:"NULL");

	list_for_each_entry(attach_obj, &dmabuf->attachments, node) {
		if (attach_obj->sgt)
			iova = sg_dma_address(attach_obj->sgt->sgl);

		dmabuf_dump(s, "\tdev:%-16s, iova:0x%-16lx, sgt:0x%-8p, attr:%-4lu, dir:%-4d"
#ifdef CONFIG_DMABUF_SYSFS_STATS
			    "map_iova_cnt:%-4d"
#endif
			    "\n",
			    dev_name(attach_obj->dev),
			    iova,
			    attach_obj->sgt,
			    attach_obj->dma_map_attrs,
			    attach_obj->dir
#ifdef CONFIG_DMABUF_SYSFS_STATS
			    , attach_obj->sysfs_entry->map_counter
#endif
			    );
	}
	dmabuf_dump(s, "\n");
	dma_resv_unlock(dmabuf->resv);

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
	    heap ? dma_heap_get_name(heap) : "all",    \
	    get_dma_heap_buffer_total(heap)*4/PAGE_SIZE)

#define __HEAP_BUF_DUMP_START(s, heap)                    \
dmabuf_dump(s, "[%s] buffer dump start @%llu ms\n",       \
	    heap ? dma_heap_get_name(heap) : "all",       \
	    get_current_time_ms())

#define __HEAP_ATTACH_DUMP_STAT(s, heap)                                  \
	dmabuf_dump(s, "[%s] attach list dump Start @%llu ms\n",          \
		    heap ? dma_heap_get_name(heap) : "all",               \
		    get_current_time_ms())

#define __HEAP_ATTACH_DUMP_END(s, heap)                             \
	dmabuf_dump(s, "[%s] attachment list dump End @%llu ms\n",  \
		    heap ? dma_heap_get_name(heap) : "all",         \
		    get_current_time_ms())

//#define __HEAP_ATTACH_DUMP(s, heap, dump_param)

#define __HEAP_PAGE_POOL_DUMP(s, heap)                                       \
{                                                                            \
	struct dma_heap_export_info *exp_info = (typeof(exp_info))heap;      \
	if (exp_info && exp_info->ops && exp_info->ops->get_pool_size) {     \
		dmabuf_dump(s, "[%s] page_pool size: %ld KB\n",              \
			    dma_heap_get_name(heap),                         \
			    exp_info->ops->get_pool_size(heap)*4/PAGE_SIZE); \
	} else {                                                             \
		dmabuf_dump(s, "[%s] No page_pool data\n",                   \
			    dma_heap_get_name(heap));                        \
	}                                                                    \
}

#endif /* _MTK_DMABUFHEAP_DEBUG_H */
