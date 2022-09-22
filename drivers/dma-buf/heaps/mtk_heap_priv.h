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

/* common function */
static inline void dmabuf_release_check(const struct dma_buf *dmabuf)
{
	dma_addr_t iova = 0x0;
	const char *device_name = NULL;
	int attach_cnt = 0;
	struct dma_buf_attachment *attach_obj;

	if (!dma_resv_trylock(dmabuf->resv)) {
		/* get lock fail, maybe is using, skip check */
		return;
	}

	/* Don't dump inode number here, it will cause KASAN issue !! */
	if (WARN(!list_empty(&dmabuf->attachments),
		 "%s: size:%-8ld dbg_name:%s exp:%s, %s\n", __func__,
		 dmabuf->size,
		 dmabuf->name,
		 dmabuf->exp_name,
		 "Release dmabuf before detach all attachments, dump attach below:")) {

		/* dump all attachment info */
		list_for_each_entry(attach_obj, &dmabuf->attachments, node) {
			iova = (dma_addr_t)0;

			attach_cnt++;
			if (attach_obj->sgt && dev_iommu_fwspec_get(attach_obj->dev))
				iova = sg_dma_address(attach_obj->sgt->sgl);

			device_name = dev_name(attach_obj->dev);
			dmabuf_dump(NULL,
				    "attach[%d]: iova:0x%-12lx attr:%-4lx dir:%-2d dev:%s\n",
				    attach_cnt, iova,
				    attach_obj->dma_map_attrs,
				    attach_obj->dir,
				    device_name);
		}
		dmabuf_dump(NULL, "Total %d devices attached\n\n", attach_cnt);
	}
	dma_resv_unlock(dmabuf->resv);

}

#endif /* _MTK_DMABUFHEAP_DEBUG_H */
