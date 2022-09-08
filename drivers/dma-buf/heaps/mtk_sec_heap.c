// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF mtk_sec heap exporter
 *
 * Copyright (C) 2021 MediaTek Inc.
 *
 */

#define pr_fmt(fmt) "dma_heap: mtk_sec "fmt

#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/iommu.h>
#include <linux/dma-heap.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include "page_pool.h"
#include "deferred-free-helper.h"

#include <public/trusted_mem_api.h>
#include "mtk_heap_priv.h"
#include "mtk_heap.h"
#include "mtk_iommu.h"
#include "iommu_pseudo.h"

enum sec_heap_region_type {
	/* MM heap */
	MM_HEAP_START,
	SVP_REGION,
	PROT_REGION,
	PROT_2D_FR_REGION,
	WFD_REGION,
	MM_HEAP_END,

	/* APU heap */
	APU_HEAP_START,
	SAPU_DATA_SHM_REGION,
	SAPU_ENGINE_SHM_REGION,
	APU_HEAP_END,

	REGION_HEAPS_NUM,
};

enum sec_heap_page_type {
	SVP_PAGE,
	PROT_PAGE,
	WFD_PAGE,
	PAGE_HEAPS_NUM,
};

enum HEAP_BASE_TYPE {
	HEAP_TYPE_INVALID = 0,
	REGION_BASE,
	PAGE_BASE,
	HEAP_BASE_NUM
};

#define NAME_LEN 32

enum REGION_TYPE {
	REGION_HEAP_NORMAL,
	REGION_HEAP_ALIGN,
	REGION_TYPE_NUM,
};

struct secure_heap_region {
	bool heap_mapped; /* indicate whole region if it is mapped */
	bool heap_filled; /* indicate whole region if it is filled */
	struct mutex heap_lock;
	const char *heap_name[REGION_TYPE_NUM];
	atomic64_t total_size;
	phys_addr_t region_pa;
	u32 region_size;
	struct sg_table *region_table;
	struct dma_heap *heap[REGION_TYPE_NUM];
	struct device *heap_dev;
	enum TRUSTED_MEM_REQ_TYPE tmem_type;
	enum HEAP_BASE_TYPE heap_type;
};

struct secure_heap_page {
	char heap_name[NAME_LEN];
	atomic64_t total_size;
	struct dma_heap *heap;
	struct device *heap_dev;
	enum TRUSTED_MEM_REQ_TYPE tmem_type;
	enum HEAP_BASE_TYPE heap_type;
};

struct mtk_sec_heap_buffer {
	struct dma_heap *heap;
	struct list_head attachments;
	struct mutex lock;
	unsigned long len;
	struct sg_table sg_table;
	int vmap_cnt;
	void *vaddr;
	struct deferred_freelist_item deferred_free;
	bool uncached;
	/* helper function */
	int (*show)(const struct dma_buf *dmabuf, struct seq_file *s);

	/* secure heap will not strore sgtable here */
	bool                     mapped[MTK_M4U_TAB_NR_MAX][MTK_M4U_DOM_NR_MAX];
	struct mtk_heap_dev_info dev_info[MTK_M4U_TAB_NR_MAX][MTK_M4U_DOM_NR_MAX];
	struct sg_table          *mapped_table[MTK_M4U_TAB_NR_MAX][MTK_M4U_DOM_NR_MAX];
	struct mutex             map_lock; /* map iova lock */
	pid_t                    pid;
	pid_t                    tid;
	char                     pid_name[TASK_COMM_LEN];
	char                     tid_name[TASK_COMM_LEN];
	unsigned long long       ts; /* us */

	/* private part for secure heap */
	u32                      sec_handle;/* keep same type with tmem */
	struct ssheap_buf_info   *ssheap; /* for page base */
};

static struct secure_heap_page mtk_sec_heap_page[PAGE_HEAPS_NUM] = {
	[SVP_PAGE] = {
		.heap_name = "mtk_svp_page-uncached",
		.tmem_type = TRUSTED_MEM_REQ_SVP_PAGE,
		.heap_type = PAGE_BASE,
	},
	[PROT_PAGE] = {
		.heap_name = "mtk_prot_page-uncached",
		.tmem_type = TRUSTED_MEM_REQ_PROT_PAGE,
		.heap_type = PAGE_BASE,
	},
	[WFD_PAGE] = {
		.heap_name = "mtk_wfd_page-uncached",
		.tmem_type = TRUSTED_MEM_REQ_WFD_PAGE,
		.heap_type = PAGE_BASE,
	},
};

static struct secure_heap_region mtk_sec_heap_region[REGION_HEAPS_NUM] = {
	[SVP_REGION] = {
		.heap_name = {"mtk_svp_region",
			      "mtk_svp_region-aligned"},
		.tmem_type = TRUSTED_MEM_REQ_SVP_REGION,
		.heap_type = REGION_BASE,
	},
	[PROT_REGION] = {
		.heap_name = {"mtk_prot_region",
			      "mtk_prot_region-aligned"},
		.tmem_type = TRUSTED_MEM_REQ_PROT_REGION,
		.heap_type = REGION_BASE,
	},
	[PROT_2D_FR_REGION] = {
		.heap_name = {"mtk_2d_fr_region",
			      "mtk_2d_fr_region-aligned"},
		.tmem_type = TRUSTED_MEM_REQ_2D_FR,
		.heap_type = REGION_BASE,
	},
	[WFD_REGION] = {
		.heap_name = {"mtk_wfd_region",
			      "mtk_wfd_region-aligned"},
		.tmem_type = TRUSTED_MEM_REQ_WFD_REGION,
		.heap_type = REGION_BASE,
	},
	[SAPU_DATA_SHM_REGION] = {
		.heap_name = {"mtk_sapu_data_shm_region",
			      "mtk_sapu_data_shm_region-aligned"},
		.tmem_type = TRUSTED_MEM_REQ_SAPU_DATA_SHM,
		.heap_type = REGION_BASE,
	},
	[SAPU_ENGINE_SHM_REGION] = {
		.heap_name = {"mtk_sapu_engine_shm_region",
			      "mtk_sapu_engine_shm_region-aligned"},
		.tmem_type = TRUSTED_MEM_REQ_SAPU_ENGINE_SHM,
		.heap_type = REGION_BASE,
	},
};

/* function declare */
static int is_region_base_dmabuf(const struct dma_buf *dmabuf);
static int is_page_base_dmabuf(const struct dma_buf *dmabuf);
static int sec_buf_priv_dump(const struct dma_buf *dmabuf,
			     struct seq_file *s);

static bool region_heap_is_aligned(struct dma_heap *heap)
{
	if (strstr(dma_heap_get_name(heap), "aligned"))
		return true;

	return false;
}

static int get_heap_base_type(const struct dma_heap *heap)
{
	int i, j, k;

	for (i = SVP_REGION; i < REGION_HEAPS_NUM; i++) {
		for (j = REGION_HEAP_NORMAL; j < REGION_TYPE_NUM; j++) {
			if (mtk_sec_heap_region[i].heap[j] == heap)
				return REGION_BASE;
		}
	}
	for (k = SVP_PAGE; k < PAGE_HEAPS_NUM; k++) {
		if (mtk_sec_heap_page[k].heap == heap)
			return PAGE_BASE;
	}
	return HEAP_TYPE_INVALID;
}

static struct secure_heap_region *sec_heap_region_get(const struct dma_heap *heap)
{
	int i, j;

	for (i = SVP_REGION; i < REGION_HEAPS_NUM; i++) {
		for (j = REGION_HEAP_NORMAL; j < REGION_TYPE_NUM; j++) {
			if (mtk_sec_heap_region[i].heap[j] == heap)
				return &mtk_sec_heap_region[i];
		}
	}
	return NULL;
}

static struct secure_heap_page *sec_heap_page_get(const struct dma_heap *heap)
{
	int i = 0;

	for (i = SVP_PAGE; i < PAGE_HEAPS_NUM; i++) {
		if (mtk_sec_heap_page[i].heap == heap)
			return &mtk_sec_heap_page[i];
	}
	return NULL;
}

static struct sg_table *dup_sg_table_sec(struct sg_table *table)
{
	struct sg_table *new_table;
	int ret, i;
	struct scatterlist *sg, *new_sg;

	new_table = kzalloc(sizeof(*new_table), GFP_KERNEL);
	if (!new_table)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(new_table, table->orig_nents, GFP_KERNEL);
	if (ret) {
		kfree(new_table);
		return ERR_PTR(-ENOMEM);
	}

	new_sg = new_table->sgl;
	for_each_sgtable_sg(table, sg, i) {
		/* skip copy dma_address, need get via map_attachment */
		sg_set_page(new_sg, sg_page(sg), sg->length, sg->offset);
		new_sg = sg_next(new_sg);
	}

	return new_table;
}

static int region_base_free(struct secure_heap_region *sec_heap, struct mtk_sec_heap_buffer *buffer)
{
	int i, j, ret;
	u32 sec_handle = 0;

	pr_info("%s start, [%s] size:0x%lx\n",
		__func__, dma_heap_get_name(buffer->heap), buffer->len);

	/* remove all domains' sgtable */
	for (i = 0; i < MTK_M4U_TAB_NR_MAX; i++) {
		for (j = 0; j < MTK_M4U_DOM_NR_MAX; j++) {
			struct sg_table *table = buffer->mapped_table[i][j];
			struct mtk_heap_dev_info dev_info = buffer->dev_info[i][j];
			unsigned long attrs = dev_info.map_attrs | DMA_ATTR_SKIP_CPU_SYNC;

			if (!buffer->mapped[i][j] || dev_is_normal_region(dev_info.dev))
				continue;
			pr_info("%s: free tab:%d, dom:%d iova:0x%lx, dev:%s\n", __func__,
				i, j, (unsigned long)sg_dma_address(table->sgl),
				dev_name(dev_info.dev));
			dma_unmap_sgtable(dev_info.dev, table, dev_info.direction, attrs);
			buffer->mapped[i][j] = false;
			sg_free_table(table);
			kfree(table);
		}
	}

	sec_handle = buffer->sec_handle;
	ret = trusted_mem_api_unref(sec_heap->tmem_type, sec_handle,
			      (uint8_t *)dma_heap_get_name(buffer->heap), 0, NULL);
	if (ret) {
		pr_err("%s error, trusted_mem_api_unref failed, heap:%s, ret:%d\n",
		       __func__, dma_heap_get_name(buffer->heap), ret);
		return ret;
	}

	if (atomic64_sub_return(buffer->len, &sec_heap->total_size) < 0)
		pr_warn("%s warn!, total memory overflow, 0x%lx!!\n", __func__,
			atomic64_read(&sec_heap->total_size));

	if (!atomic64_read(&sec_heap->total_size)) {
		if (sec_heap->heap_mapped) { /*need to lock ????? */
			dma_unmap_sgtable(sec_heap->heap_dev, sec_heap->region_table,
					  DMA_BIDIRECTIONAL, DMA_ATTR_SKIP_CPU_SYNC);
			sec_heap->heap_mapped = false;
		}
		sg_free_table(sec_heap->region_table);
		kfree(sec_heap->region_table);
		sec_heap->heap_filled = false;
		pr_info("%s: all secure memory already free, unmap heap_region iova\n", __func__);
	}

	pr_info("%s done, [%s] size:0x%lx, total_size:0x%lx\n",
		__func__, dma_heap_get_name(buffer->heap), buffer->len,
		atomic64_read(&sec_heap->total_size));
	return ret;
}


static int page_base_free(struct secure_heap_page *sec_heap, struct mtk_sec_heap_buffer *buffer)
{
	int i, j, ret;

	pr_info("%s start, [%s] size:0x%lx\n",
		__func__, dma_heap_get_name(buffer->heap), buffer->len);

	/* remove all domains' sgtable */
	for (i = 0; i < MTK_M4U_TAB_NR_MAX; i++) {
		for (j = 0; j < MTK_M4U_DOM_NR_MAX; j++) {
			struct sg_table *table = buffer->mapped_table[i][j];
			struct mtk_heap_dev_info dev_info = buffer->dev_info[i][j];
			unsigned long attrs = dev_info.map_attrs | DMA_ATTR_SKIP_CPU_SYNC;

			if (!buffer->mapped[i][j])
				continue;
			pr_info("%s: free tab:%d, region:%d iova:0x%lx, dev:%s\n", __func__,
				i, j, (unsigned long)sg_dma_address(table->sgl),
				dev_name(dev_info.dev));
			dma_unmap_sgtable(dev_info.dev, table, dev_info.direction, attrs);
			buffer->mapped[i][j] = false;
			sg_free_table(table);
			kfree(table);
		}
	}
	ret = trusted_mem_api_unref(sec_heap->tmem_type, buffer->sec_handle,
			      (uint8_t *)dma_heap_get_name(buffer->heap), 0, buffer->ssheap);
	if (ret) {
		pr_err("%s error, trusted_mem_api_unref failed, heap:%s, ret:%d\n",
		       __func__, dma_heap_get_name(buffer->heap), ret);
		return ret;
	}

	if (atomic64_sub_return(buffer->len, &sec_heap->total_size) < 0)
		pr_warn("%s, total memory overflow, 0x%lx!!\n", __func__,
			atomic64_read(&sec_heap->total_size));

	pr_info("%s done, [%s] size:0x%lx, total_size:0x%lx\n",
		__func__, dma_heap_get_name(buffer->heap), buffer->len,
		atomic64_read(&sec_heap->total_size));

	return ret;
}

static void tmem_region_free(struct dma_buf *dmabuf)
{
	int ret = -EINVAL;
	struct secure_heap_region *sec_heap;
	struct mtk_sec_heap_buffer *buffer = NULL;

	dmabuf_release_check(dmabuf);

	buffer = dmabuf->priv;
	sec_heap = sec_heap_region_get(buffer->heap);
	if (!sec_heap) {
		pr_err("%s, can not find secure heap!!\n",
			__func__, buffer->heap ? dma_heap_get_name(buffer->heap) : "null ptr");
		return;
	}

	//pr_info("%s start, heap:%s, len:0x%lx\n", __func__, dmabuf->exp_name, buffer->len);

	ret = region_base_free(sec_heap, buffer);
	if (ret) {
		pr_err("%s fail, heap:%u\n", __func__, sec_heap->heap_type);
		return;
	}
	sg_free_table(&buffer->sg_table);
	kfree(buffer);

	//pr_info("%s done: [%s], size:0x%lx, total_size:0x%lx\n", __func__,
		//dmabuf->exp_name, dmabuf->size, atomic64_read(&sec_heap->total_size));
}

static void tmem_page_free(struct dma_buf *dmabuf)
{
	int ret = -EINVAL;
	struct secure_heap_page *sec_heap;
	struct mtk_sec_heap_buffer *buffer = NULL;

	dmabuf_release_check(dmabuf);

	buffer = dmabuf->priv;
	sec_heap = sec_heap_page_get(buffer->heap);
	if (!sec_heap) {
		pr_err("%s, can not find secure heap!!\n",
			__func__, buffer->heap ? dma_heap_get_name(buffer->heap) : "null ptr");
		return;
	}

	//pr_info("%s start, heap:%s, len:0x%lx\n", __func__, dmabuf->exp_name, buffer->len);

	ret = page_base_free(sec_heap, buffer);
	if (ret) {
		pr_err("%s fail, heap:%u\n", __func__, sec_heap->heap_type);
		return;
	}
	sg_free_table(&buffer->sg_table);
	kfree(buffer);

	//pr_info("%s done: [%s], size:0x%lx, total_size:0x%lx\n", __func__,
		//dmabuf->exp_name, dmabuf->size, atomic64_read(&sec_heap->total_size));
}

static int mtk_sec_heap_attach(struct dma_buf *dmabuf,
			       struct dma_buf_attachment *attachment)
{
	struct mtk_sec_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;
	struct sg_table *table;

	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return -ENOMEM;

	table = dup_sg_table_sec(&buffer->sg_table);
	if (IS_ERR(table)) {
		kfree(a);
		return -ENOMEM;
	}

	a->table = table;
	a->dev = attachment->dev;
	INIT_LIST_HEAD(&a->list);
	a->mapped = false;
	a->uncached = buffer->uncached;
	attachment->priv = a;

	mutex_lock(&buffer->lock);
	list_add(&a->list, &buffer->attachments);
	mutex_unlock(&buffer->lock);

	return 0;
}

static void mtk_sec_heap_detach(struct dma_buf *dmabuf,
				struct dma_buf_attachment *attachment)
{
	struct mtk_sec_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a = attachment->priv;

	mutex_lock(&buffer->lock);
	list_del(&a->list);
	mutex_unlock(&buffer->lock);

	sg_free_table(a->table);
	kfree(a->table);
	kfree(a);
}

/* source copy to dest */
static int copy_sec_sg_table(struct sg_table *source, struct sg_table *dest)
{
	int i;
	struct scatterlist *sgl, *dest_sgl;

	if (source->orig_nents != dest->orig_nents) {
		pr_err("%s err, nents not match %d-%d\n", __func__,
			source->orig_nents, dest->orig_nents);

		return -EINVAL;
	}

	/* copy mapped nents */
	dest->nents = source->nents;

	dest_sgl = dest->sgl;
	for_each_sg(source->sgl, sgl, source->orig_nents, i) {
		memcpy(dest_sgl, sgl, sizeof(*sgl));
		dest_sgl = sg_next(dest_sgl);
	}

	return 0;
};

/*
 * must check domain info before call fill_buffer_info
 * @Return 0: pass
 */
static int fill_sec_buffer_info(struct mtk_sec_heap_buffer *buf,
				struct sg_table *table,
				struct dma_buf_attachment *a,
				enum dma_data_direction dir,
				unsigned int tab_id, unsigned int dom_id)
{
	struct sg_table *new_table = NULL;
	int ret = 0;

	/*
	 * devices without iommus attribute,
	 * use common flow, skip set buf_info
	 */
	if (tab_id >= MTK_M4U_TAB_NR_MAX || dom_id >= MTK_M4U_DOM_NR_MAX)
		return 0;

	if (buf->mapped[tab_id][dom_id]) {
		pr_info("%s err: already mapped, no need fill again\n", __func__);
		return -EINVAL;
	}

	new_table = kzalloc(sizeof(*new_table), GFP_KERNEL);
	if (!new_table)
		return -ENOMEM;

	ret = sg_alloc_table(new_table, table->orig_nents, GFP_KERNEL);
	if (ret) {
		kfree(new_table);
		return -ENOMEM;
	}

	ret = copy_sec_sg_table(table, new_table);
	if (ret)
		return ret;

	buf->mapped_table[tab_id][dom_id] = new_table;
	buf->mapped[tab_id][dom_id] = true;
	buf->dev_info[tab_id][dom_id].dev = a->dev;
	buf->dev_info[tab_id][dom_id].direction = dir;
	buf->dev_info[tab_id][dom_id].map_attrs = a->dma_map_attrs;

	return 0;
}

static int check_map_alignment(struct sg_table *table)
{
	int i;
	struct scatterlist *sgl;

	for_each_sg(table->sgl, sgl, table->orig_nents, i) {
		unsigned int len = sgl->length;
		phys_addr_t s_phys = sg_phys(sgl);

		if (!IS_ALIGNED(len, SZ_1M)) {
			pr_err("%s err, size(0x%x) is not 1MB alignment\n", __func__, len);
			return -EINVAL;
		}
		if (!IS_ALIGNED(s_phys, SZ_1M)) {
			pr_err("%s err, s_phys(%pa) is not 1MB alignment\n", __func__, &s_phys);
			return -EINVAL;
		}
	}

	return 0;
}

static struct sg_table *mtk_sec_heap_page_map_dma_buf(struct dma_buf_attachment *attachment,
						 enum dma_data_direction direction)
{
	int ret;
	struct dma_heap_attachment *a = attachment->priv;
	struct sg_table *table = a->table;
	struct dma_buf *dmabuf = attachment->dmabuf;
	struct mtk_sec_heap_buffer *buffer;
	struct secure_heap_page *sec_heap;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(attachment->dev);
	unsigned int tab_id = MTK_M4U_TAB_NR_MAX, dom_id = MTK_M4U_DOM_NR_MAX;
	int attr = attachment->dma_map_attrs | DMA_ATTR_SKIP_CPU_SYNC;

	//pr_info("%s start dev:%s\n", __func__, dev_name(attachment->dev));

	/* non-iommu master */
	if (!fwspec) {
		ret = dma_map_sgtable(attachment->dev, table, direction, attr);
		if (ret) {
			pr_err("%s err, non-iommu-dev(%s) dma_map_sgtable failed\n",
			       __func__, dev_name(attachment->dev));
			return ERR_PTR(ret);
		}
		a->mapped = true;
		pr_info("%s done, non-iommu-dev(%s)\n", __func__, dev_name(attachment->dev));
		return table;
	}

	buffer = dmabuf->priv;
	sec_heap = sec_heap_page_get(buffer->heap);
	if (!sec_heap) {
		pr_err("%s, dma_sec_heap_get failed\n", __func__);
		return NULL;
	}
	tab_id = MTK_M4U_TO_TAB(fwspec->ids[0]);
	dom_id = MTK_M4U_TO_DOM(fwspec->ids[0]);
	mutex_lock(&buffer->map_lock);
	/* device with iommus attribute and mapped before */
	if (buffer->mapped[tab_id][dom_id]) {
		/* mapped before, return saved table */
		ret = copy_sec_sg_table(buffer->mapped_table[tab_id][dom_id], table);
		if (ret) {
			pr_err("%s err, copy_sec_sg_table failed, dev:%s\n",
			       __func__, dev_name(attachment->dev));
			mutex_unlock(&buffer->map_lock);
			return ERR_PTR(-EINVAL);
		}
		a->mapped = true;
		pr_info("%s done(has mapped), dev:%s(%s), sec_handle:%u, len:0x%lx, iova:0x%lx, id:(%d,%d)\n",
			__func__, dev_name(buffer->dev_info[tab_id][dom_id].dev),
			dev_name(attachment->dev), buffer->sec_handle, buffer->len,
			(unsigned long)sg_dma_address(table->sgl),
			tab_id, dom_id);
		mutex_unlock(&buffer->map_lock);
		return table;
	}

	ret = check_map_alignment(table);
	if (ret) {
		pr_err("%s err, size or PA is not 1MB alignment, dev:%s\n",
		       __func__, dev_name(attachment->dev));
		mutex_unlock(&buffer->map_lock);
		return ERR_PTR(ret);
	}
	ret = dma_map_sgtable(attachment->dev, table, direction, attr);
	if (ret) {
		pr_err("%s err, iommu-dev(%s) dma_map_sgtable failed\n",
		       __func__, dev_name(attachment->dev));
		mutex_unlock(&buffer->map_lock);
		return ERR_PTR(ret);
	}
	ret = fill_sec_buffer_info(buffer, table, attachment, direction, tab_id, dom_id);
	if (ret) {
		pr_err("%s failed, fill_sec_buffer_info failed, dev:%s\n",
		       __func__, dev_name(attachment->dev));
		mutex_unlock(&buffer->map_lock);
		return ERR_PTR(-ENOMEM);
	}
	a->mapped = true;

	pr_info("%s done, dev:%s, sec_handle:%u, len:0x%lx, iova:0x%lx, id:(%d,%d)\n",
		__func__, dev_name(attachment->dev), buffer->sec_handle, buffer->len,
		(unsigned long)sg_dma_address(table->sgl), tab_id, dom_id);
	mutex_unlock(&buffer->map_lock);

	return table;
}

static struct sg_table *mtk_sec_heap_region_map_dma_buf(struct dma_buf_attachment *attachment,
						 enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	struct sg_table *table = a->table;
	struct dma_buf *dmabuf = attachment->dmabuf;
	struct mtk_sec_heap_buffer *buffer;
	struct secure_heap_region *sec_heap;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(attachment->dev);
	unsigned int tab_id = MTK_M4U_TAB_NR_MAX, dom_id = MTK_M4U_DOM_NR_MAX;
	int ret;
	dma_addr_t dma_address;
	uint64_t phy_addr = 0;
	/* for iommu mapping, should be skip cache sync */
	int attr = attachment->dma_map_attrs | DMA_ATTR_SKIP_CPU_SYNC;

	//pr_info("%s start dev:%s\n", __func__, dev_name(attachment->dev));

	/* non-iommu master */
	if (!fwspec) {
		ret = dma_map_sgtable(attachment->dev, table, direction, attr);
		if (ret) {
			pr_err("%s err, non-iommu-dev(%s) dma_map_sgtable failed\n",
			       __func__, dev_name(attachment->dev));
			return ERR_PTR(ret);
		}
		a->mapped = true;
		pr_info("%s done, non-iommu-dev(%s), pa:0x%lx\n", __func__,
			dev_name(attachment->dev),
			(unsigned long)sg_dma_address(table->sgl));
		return table;
	}

	if (is_disable_map_sec()) {
		pr_err("%s not support, dev:%s\n", __func__,
		       dev_name(attachment->dev));
		return ERR_PTR(-EINVAL);
	}

	buffer = dmabuf->priv;
	sec_heap = sec_heap_region_get(buffer->heap);
	if (!sec_heap) {
		pr_err("%s, sec_heap_region_get failed\n", __func__);
		return NULL;
	}
	mutex_lock(&sec_heap->heap_lock);
	if (!sec_heap->heap_mapped) {
		ret = check_map_alignment(sec_heap->region_table);
		if (ret) {
			pr_err("%s err, heap_region size or PA is not 1MB alignment, dev:%s\n",
			       __func__, dev_name(sec_heap->heap_dev));
			mutex_unlock(&sec_heap->heap_lock);
			return ERR_PTR(ret);
		}
		if (dma_map_sgtable(sec_heap->heap_dev, sec_heap->region_table,
				    DMA_BIDIRECTIONAL, DMA_ATTR_SKIP_CPU_SYNC)) {
			pr_err("%s err, heap_region(%s) dma_map_sgtable failed\n",
				__func__, dev_name(sec_heap->heap_dev));
			mutex_unlock(&sec_heap->heap_lock);
			return ERR_PTR(-ENOMEM);
		}
		sec_heap->heap_mapped = true;
		pr_info("%s heap_region map success, heap:%s, iova:0x%lx, sz:0x%lx\n",
			__func__, dma_heap_get_name(buffer->heap),
			(unsigned long)sg_dma_address(sec_heap->region_table->sgl),
			(unsigned long)sg_dma_len(sec_heap->region_table->sgl));
	}
	mutex_unlock(&sec_heap->heap_lock);

	tab_id = MTK_M4U_TO_TAB(fwspec->ids[0]);
	dom_id = MTK_M4U_TO_DOM(fwspec->ids[0]);
	phy_addr = (uint64_t)sg_phys(table->sgl);
	mutex_lock(&buffer->map_lock);
	/* device with iommus attribute and mapped before */
	if (buffer->mapped[tab_id][dom_id]) {
		/* mapped before, return saved table */
		ret = copy_sec_sg_table(buffer->mapped_table[tab_id][dom_id], table);
		if (ret) {
			pr_err("%s err, copy_sec_sg_table failed, dev:%s\n", __func__, dev_name(attachment->dev));
			mutex_unlock(&buffer->map_lock);
			return ERR_PTR(-EINVAL);
		}
		a->mapped = true;
		pr_info("%s done(has mapped), dev:%s(%s), sec_handle:%u, len:0x%lx, pa:0x%llx, iova:0x%lx, id:(%d,%d)\n",
			__func__, dev_name(buffer->dev_info[tab_id][dom_id].dev),
			dev_name(attachment->dev), buffer->sec_handle, buffer->len,
			phy_addr, (unsigned long)sg_dma_address(table->sgl),
			tab_id, dom_id);
		mutex_unlock(&buffer->map_lock);
		return table;
	}
	if (!dev_is_normal_region(attachment->dev)) {
		ret = check_map_alignment(table);
		if (ret) {
			pr_err("%s err, size or PA is not 1MB alignment, dev:%s\n", __func__, dev_name(attachment->dev));
			mutex_unlock(&buffer->map_lock);
			return ERR_PTR(ret);
		}
		ret = dma_map_sgtable(attachment->dev, table, direction, attr);
		if (ret) {
			pr_err("%s err, iommu-dev(%s) dma_map_sgtable failed\n", __func__, dev_name(attachment->dev));
			mutex_unlock(&buffer->map_lock);
			return ERR_PTR(ret);
		}
		pr_info("%s reserve_iommu-dev(%s) dma_map_sgtable done, iova:0x%lx, id:(%d,%d)\n",
			__func__, dev_name(attachment->dev), sg_dma_address(table->sgl),
			tab_id, dom_id);
		goto map_done;
	}

	if (buffer->len <= 0 || buffer->len > sec_heap->region_size ||
	    phy_addr < sec_heap->region_pa ||
	    phy_addr > sec_heap->region_pa + sec_heap->region_size ||
	    phy_addr + buffer->len > sec_heap->region_pa + sec_heap->region_size ||
	    phy_addr + buffer->len <= sec_heap->region_pa) {
		pr_err("%s err. req size/pa is invalid! heap:%s\n", __func__,
		       dma_heap_get_name(buffer->heap));
		mutex_unlock(&buffer->map_lock);
		return ERR_PTR(-ENOMEM);
	}
	dma_address = phy_addr - sec_heap->region_pa + sg_dma_address(sec_heap->region_table->sgl);
	sg_dma_address(table->sgl) = dma_address;
	sg_dma_len(table->sgl) = buffer->len;

map_done:
	ret = fill_sec_buffer_info(buffer, table, attachment, direction, tab_id, dom_id);
	if (ret) {
		pr_err("%s failed, fill_sec_buffer_info failed, dev:%s\n", __func__, dev_name(attachment->dev));
		mutex_unlock(&buffer->map_lock);
		return ERR_PTR(-ENOMEM);
	}
	a->mapped = true;

	pr_info("%s done, dev:%s, sec_handle:%u, len:0x%lx(0x%x), pa:0x%llx, iova:0x%lx, id:(%d,%d)\n",
		__func__, dev_name(attachment->dev), buffer->sec_handle, buffer->len,
		sg_dma_len(table->sgl), phy_addr, (unsigned long)sg_dma_address(table->sgl),
		tab_id, dom_id);
	mutex_unlock(&buffer->map_lock);

	return table;
}

static void mtk_sec_heap_unmap_dma_buf(struct dma_buf_attachment *attachment,
				       struct sg_table *table,
				       enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	int attr = attachment->dma_map_attrs | DMA_ATTR_SKIP_CPU_SYNC;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(attachment->dev);

	if (!fwspec) {
		pr_info("%s, non-iommu-dev unmap, dev:%s\n", __func__, dev_name(attachment->dev));
		dma_unmap_sgtable(attachment->dev, table, direction, attr);
	}
	a->mapped = false;
}

static bool heap_is_region_base(enum HEAP_BASE_TYPE heap_type)
{
	if (heap_type == REGION_BASE)
		return true;

	return false;
}

static int fill_heap_sgtable(struct secure_heap_region *sec_heap,
				struct mtk_sec_heap_buffer *buffer)
{
	int ret;

	//pr_info("%s start [%s][%d]\n", __func__, dma_heap_get_name(buffer->heap),
		//sec_heap->tmem_type);

	if (!heap_is_region_base(sec_heap->heap_type)) {
		pr_info("%s skip page base filled\n", __func__);
		return 0;
	}
	/* TODO: race condition????????????????? */
	mutex_lock(&sec_heap->heap_lock);
	if (sec_heap->heap_filled) {
		mutex_unlock(&sec_heap->heap_lock);
		pr_info("%s, %s already filled\n", __func__, dma_heap_get_name(buffer->heap));
		return 0;
	}

	ret = trusted_mem_api_get_region_info(sec_heap->tmem_type,
		&sec_heap->region_pa, &sec_heap->region_size);
	if (!ret) {
		mutex_unlock(&sec_heap->heap_lock);
		pr_err("%s, [%s],get_region_info failed!\n", __func__,
		       dma_heap_get_name(buffer->heap));
		return -EINVAL;
	}

	sec_heap->region_table = kzalloc(sizeof(*sec_heap->region_table), GFP_KERNEL);
	if (!sec_heap->region_table) {
		mutex_unlock(&sec_heap->heap_lock);
		pr_err("%s, [%s] kzalloc_sgtable failed\n", __func__,
		       dma_heap_get_name(buffer->heap));
		return ret;
	}
	ret = sg_alloc_table(sec_heap->region_table, 1, GFP_KERNEL);
	if (ret) {
		mutex_unlock(&sec_heap->heap_lock);
		pr_err("%s, [%s] alloc_sgtable failed\n", __func__,
		       dma_heap_get_name(buffer->heap));
		return ret;
	}
	sg_set_page(sec_heap->region_table->sgl, phys_to_page(sec_heap->region_pa),
		    sec_heap->region_size, 0);
	sec_heap->heap_filled = true;
	mutex_unlock(&sec_heap->heap_lock);
	pr_info("%s [%s] fill done, region_pa:%pa, region_size:0x%x\n",
		__func__, dma_heap_get_name(buffer->heap), &sec_heap->region_pa,
		sec_heap->region_size);
	return 0;
}

static int mtk_sec_heap_dma_buf_get_flags(struct dma_buf *dmabuf, unsigned long *flags)
{
	struct mtk_sec_heap_buffer *buffer = dmabuf->priv;

	*flags = buffer->uncached;

	return 0;
}

static const struct dma_buf_ops sec_buf_region_ops = {
	/* one attachment can only map once */
	.cache_sgt_mapping = 1,
	.attach = mtk_sec_heap_attach,
	.detach = mtk_sec_heap_detach,
	.map_dma_buf = mtk_sec_heap_region_map_dma_buf,
	.unmap_dma_buf = mtk_sec_heap_unmap_dma_buf,
	.release = tmem_region_free,
	.get_flags = mtk_sec_heap_dma_buf_get_flags,
};

static const struct dma_buf_ops sec_buf_page_ops = {
	/* one attachment can only map once */
	.cache_sgt_mapping = 1,
	.attach = mtk_sec_heap_attach,
	.detach = mtk_sec_heap_detach,
	.map_dma_buf = mtk_sec_heap_page_map_dma_buf,
	.unmap_dma_buf = mtk_sec_heap_unmap_dma_buf,
	.release = tmem_page_free,
	.get_flags = mtk_sec_heap_dma_buf_get_flags,
};

static int is_region_base_dmabuf(const struct dma_buf *dmabuf)
{
	return dmabuf && dmabuf->ops == &sec_buf_region_ops;
}

static int is_page_base_dmabuf(const struct dma_buf *dmabuf)
{
	return dmabuf && dmabuf->ops == &sec_buf_page_ops;
}


/* region base size is 4K alignment */
static int region_base_alloc(struct secure_heap_region *sec_heap,
					struct mtk_sec_heap_buffer *buffer,
					unsigned long req_sz, bool aligned)
{
	int ret;
	u32 sec_handle = 0;
	u32 refcount = 0;/* tmem refcount */
	u32 alignment = aligned ? SZ_1M : 0;
	uint64_t phy_addr = 0;
	struct sg_table *table;

	pr_info("%s start: [%s], req_size:0x%lx, align:0x%x(%d)\n",
		__func__, dma_heap_get_name(buffer->heap), buffer->len, alignment, aligned);

	if (buffer->len > UINT_MAX) {
		pr_err("%s error. len more than UINT_MAX\n", __func__);
		return -EINVAL;
	}
	ret = trusted_mem_api_alloc(sec_heap->tmem_type, alignment, (unsigned int *)&buffer->len,
				    &refcount, &sec_handle,
				    (uint8_t *)dma_heap_get_name(buffer->heap),
				    0, NULL);
	if (ret == -ENOMEM) { //return value ???????????????????
		pr_err("%s error: security out of memory!! heap:%s\n",
			__func__, dma_heap_get_name(buffer->heap));
		return -ENOMEM;
	}
	if (!sec_handle) {
		pr_err("%s alloc security memory failed, req_size:0x%lx, total_size 0x%lx\n",
			__func__, req_sz, atomic64_read(&sec_heap->total_size));
		return -ENOMEM;
	}

	table = &buffer->sg_table;
	/* region base PA is continuous, so nent = 1 */
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret) {
		/* free buffer */
		pr_err("%s#%d Error. Allocate mem failed.\n",
			__func__, __LINE__);
		goto free_buffer;
	}
	ret = trusted_mem_api_query_pa(sec_heap->tmem_type, 0, buffer->len, &refcount,
				       &sec_handle, (u8 *)dma_heap_get_name(buffer->heap),
				       0, 0, &phy_addr);
	if (ret) {
		/* free buffer */
		pr_err("%s#%d Error. query pa failed.\n",
			__func__, __LINE__);
		goto free_buffer_struct;
	}
	sg_set_page(table->sgl, phys_to_page(phy_addr), buffer->len, 0);
	/* store seucre handle */
	buffer->sec_handle = sec_handle;

	ret = fill_heap_sgtable(sec_heap, buffer);
	if (ret) {
		pr_err("%s#%d Error. fill_heap_sgtable failed.\n",
			__func__, __LINE__);
		goto free_buffer_struct;
	}

	atomic64_add(buffer->len, &sec_heap->total_size);

	pr_info("%s done: [%s], req_size:0x%lx, align_sz:0x%lx, handle:%u, pa:0x%lx, total_sz:0x%lx\n",
		__func__, dma_heap_get_name(buffer->heap), req_sz, buffer->len,
		buffer->sec_handle, phy_addr, atomic64_read(&sec_heap->total_size));

	return 0;

free_buffer_struct:
	sg_free_table(table);
free_buffer:
	/* free secure handle */
	trusted_mem_api_unref(sec_heap->tmem_type, sec_handle,
			      (uint8_t *)dma_heap_get_name(buffer->heap), 0, NULL);

	return ret;
}

static int page_base_alloc(struct secure_heap_page *sec_heap, struct mtk_sec_heap_buffer *buffer,
				unsigned long req_sz)
{
	int ret;
	u32 sec_handle = 0;
	u32 refcount = 0;/* tmem refcount */
	struct ssheap_buf_info *ssheap = NULL;
	struct sg_table *table = &buffer->sg_table;

	pr_info("%s start: [%s], req_size:0x%lx\n",
		__func__, dma_heap_get_name(sec_heap->heap), buffer->len);

	if (buffer->len > UINT_MAX) {
		pr_err("%s error. len more than UINT_MAX\n", __func__);
		return -EINVAL;
	}
	ret = trusted_mem_api_alloc(sec_heap->tmem_type, 0, (unsigned int *)&buffer->len,
				    &refcount, &sec_handle,
				    (uint8_t *)dma_heap_get_name(sec_heap->heap),
				    0, &ssheap);
	if (!ssheap) {
		pr_err("%s error, alloc page base failed\n", __func__);
		return -ENOMEM;
	}

	ret = sg_alloc_table(table, ssheap->table->orig_nents, GFP_KERNEL);
	if (ret) {
		pr_err("%s error. sg_alloc_table failed\n", __func__);
		goto free_tmem;
	}

	ret = copy_sec_sg_table(ssheap->table, table);
	if (ret) {
		pr_err("%s error. copy_sec_sg_table failed\n", __func__);
		goto free_table;
	}
	buffer->len = ssheap->aligned_req_size;
	buffer->ssheap = ssheap;
	atomic64_add(buffer->len, &sec_heap->total_size);

	pr_info("%s done: [%s], req_size:0x%lx(0x%lx), align_sz:0x%lx, nent:%u--%lu, align:0x%lx, total_sz:0x%lx\n",
		__func__, dma_heap_get_name(sec_heap->heap), buffer->ssheap->req_size, req_sz,
		buffer->len, buffer->ssheap->table->orig_nents, buffer->ssheap->elems,
		buffer->ssheap->alignment, atomic64_read(&sec_heap->total_size));

	return 0;

free_table:
	sg_free_table(table);
free_tmem:
	trusted_mem_api_unref(sec_heap->tmem_type, sec_handle,
			      (uint8_t *)dma_heap_get_name(buffer->heap), 0, ssheap);

	return ret;
}

static void init_buffer_info(struct dma_heap *heap,
			     struct mtk_sec_heap_buffer *buffer)
{
	struct task_struct *task = current->group_leader;
	INIT_LIST_HEAD(&buffer->attachments);
	mutex_init(&buffer->lock);
	mutex_init(&buffer->map_lock);
	/* add alloc pid & tid info */
	get_task_comm(buffer->pid_name, task);
	get_task_comm(buffer->tid_name, current);
	buffer->pid = task_pid_nr(task);
	buffer->tid = task_pid_nr(current);

	/*
	 * in 32bit project compile the arithmetic division, the "/" will
	 * cause the __aeabi_uldivmod error.
	 *
	 * use DO_DMA_BUFFER_COMMON_DIV and DO_DMA_BUFFER_COMMON_MOD to
	 * intead "/".
	 *
	 * original code is
	 * buffer->ts  = sched_clock() / 1000;
	 */
	buffer->ts  = DO_DMA_BUFFER_COMMON_DIV(sched_clock(), 1000);
}

static struct dma_buf *alloc_dmabuf(struct dma_heap *heap, struct mtk_sec_heap_buffer *buffer,
					 const struct dma_buf_ops *ops, unsigned long fd_flags)
{
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	/* create the dmabuf */
	exp_info.exp_name = dma_heap_get_name(heap);
	exp_info.ops = ops;
	exp_info.size = buffer->len;
	exp_info.flags = fd_flags;
	exp_info.priv = buffer;

	return dma_buf_export(&exp_info);
}

static struct dma_buf *tmem_page_allocate(struct dma_heap *heap,
				     unsigned long len,
				     unsigned long fd_flags,
				     unsigned long heap_flags)
{
	int ret = -ENOMEM;
	struct dma_buf *dmabuf;
	struct mtk_sec_heap_buffer *buffer;
	struct secure_heap_page *sec_heap = sec_heap_page_get(heap);

	if (!sec_heap) {
		pr_err("%s, can not find secure heap!!\n",
			__func__, heap ? dma_heap_get_name(heap) : "null ptr");
		return ERR_PTR(-EINVAL);
	}

	//pr_info("%s start, heap:[%s], req_sz:0x%lx\n",
		 //__func__, dma_heap_get_name(heap), len);

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer) {
		pr_err("%s#%d Error. Allocate mem failed.\n",
			__func__, __LINE__);
		return ERR_PTR(-ENOMEM);
	}
	buffer->len = len;
	buffer->heap = heap;
	/* all page base memory set as noncached buffer */
	buffer->uncached = true;
	buffer->show = sec_buf_priv_dump;

	ret = page_base_alloc(sec_heap, buffer, len);
	if (ret)
		goto free_buffer;

	dmabuf = alloc_dmabuf(heap, buffer, &sec_buf_page_ops, fd_flags);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		pr_err("%s alloc_dmabuf fail\n", __func__);
		goto free_tmem;
	}

	init_buffer_info(heap, buffer);

	//pr_info("%s done: [%s], req_size:0x%lx, align_sz:0x%lx\n",
		//__func__, dma_heap_get_name(heap), len, buffer->len);

	return dmabuf;

free_tmem:
	trusted_mem_api_unref(sec_heap->tmem_type, buffer->sec_handle,
			      (uint8_t *)dma_heap_get_name(buffer->heap), 0, buffer->ssheap);

	sg_free_table(&buffer->sg_table);
free_buffer:
	kfree(buffer);
	return ERR_PTR(ret);
}

static struct dma_buf *tmem_region_allocate(struct dma_heap *heap,
				     unsigned long len,
				     unsigned long fd_flags,
				     unsigned long heap_flags)
{
	int ret = -ENOMEM;
	struct dma_buf *dmabuf;
	struct mtk_sec_heap_buffer *buffer;
	bool aligned = region_heap_is_aligned(heap);
	struct secure_heap_region *sec_heap = sec_heap_region_get(heap);

	if (!sec_heap) {
		pr_err("%s, can not find secure heap(%s)!!\n",
			__func__, heap ? dma_heap_get_name(heap) : "null ptr");
		return ERR_PTR(-EINVAL);
	}

	//pr_info("%s start, heap:[%s], req_sz: 0x%lx, aligned:%d\n",
		 //__func__, dma_heap_get_name(heap), len, aligned);

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	buffer->len = len;
	buffer->heap = heap;
	buffer->show = sec_buf_priv_dump;

	ret = region_base_alloc(sec_heap, buffer, len, aligned);
	if (ret)
		goto free_buffer;

	dmabuf = alloc_dmabuf(heap, buffer, &sec_buf_region_ops, fd_flags);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		pr_err("%s alloc_dmabuf fail\n", __func__);
		goto free_tmem;
	}
	init_buffer_info(heap, buffer);

	//pr_info("%s done: [%s], req_size:0x%lx, align_sz:0x%lx\n",
		//__func__, dma_heap_get_name(heap), len, buffer->len);

	return dmabuf;

free_tmem:
	trusted_mem_api_unref(sec_heap->tmem_type, buffer->sec_handle,
			      (uint8_t *)dma_heap_get_name(buffer->heap), 0, NULL);
	sg_free_table(&buffer->sg_table);
free_buffer:
	kfree(buffer);
	return ERR_PTR(ret);
}


static const struct dma_heap_ops sec_heap_page_ops = {
	.allocate = tmem_page_allocate,
};

static const struct dma_heap_ops sec_heap_region_ops = {
	.allocate = tmem_region_allocate,
};

static int sec_buf_priv_dump(const struct dma_buf *dmabuf,
			     struct seq_file *s)
{
	unsigned int i = 0, j = 0;
	dma_addr_t iova = 0;
	int region_buf = 0;
	struct mtk_sec_heap_buffer *buf = dmabuf->priv;
	u32 sec_handle = 0;

	dmabuf_dump(s, "\t\tbuf_priv: uncached:%d alloc_pid:%d(%s)tid:%d(%s) alloc_time:%luus\n",
		    !!buf->uncached,
		    buf->pid, buf->pid_name,
		    buf->tid, buf->tid_name,
		    buf->ts);

	/* region base, only has secure handle */
	if (is_page_base_dmabuf(dmabuf)) {
		region_buf = 0;
	} else if (is_region_base_dmabuf(dmabuf)) {
		region_buf = 1;
	} else {
		WARN_ON(1);
		return 0;
	}

	for (i = 0; i < MTK_M4U_TAB_NR_MAX; i++) {
		for (j = 0; j < MTK_M4U_DOM_NR_MAX; j++) {
			bool mapped = buf->mapped[i][j];
			struct device *dev = buf->dev_info[i][j].dev;
			struct sg_table *sgt = buf->mapped_table[i][j];
			char tmp_str[40];
			int len = 0;

			if (!sgt || !sgt->sgl || !dev || !dev_iommu_fwspec_get(dev))
				continue;

			iova = sg_dma_address(sgt->sgl);

			if (region_buf) {
				sec_handle = (dma_addr_t)dmabuf_to_secure_handle(dmabuf);
				len = scnprintf(tmp_str, 39, "sec_handle:0x%x", sec_handle);
				if (len >= 0)
					tmp_str[len] = '\0';/* No need memset */
			}

			dmabuf_dump(s,
				    "\t\tbuf_priv: tab:%-2u dom:%-2u map:%d iova:0x%-12lx %s attr:0x%-4lx dir:%-2d dev:%s\n",
				    i, j, mapped, iova,
				    region_buf ? tmp_str : "",
				    buf->dev_info[i][j].map_attrs,
				    buf->dev_info[i][j].direction,
				    dev_name(buf->dev_info[i][j].dev));
		}
	}

	return 0;
}

/**
 * return none-zero value means dump fail.
 *       maybe the input dmabuf isn't this heap buffer, no need dump
 *
 * return 0 means dump pass
 */
static int sec_heap_buf_priv_dump(const struct dma_buf *dmabuf,
				  struct dma_heap *heap,
				  void *priv)
{
	struct mtk_sec_heap_buffer *buf = dmabuf->priv;
	struct seq_file *s = priv;

	if (!is_mtk_sec_heap_dmabuf(dmabuf) || heap != buf->heap)
		return -EINVAL;

	if (buf->show)
		return buf->show(dmabuf, s);

	return -EINVAL;
}

static struct mtk_heap_priv_info mtk_sec_heap_priv = {
	.buf_priv_dump = sec_heap_buf_priv_dump,
};

int is_mtk_sec_heap_dmabuf(const struct dma_buf *dmabuf)
{
	if (!dmabuf)
		return 0;

	if (dmabuf->ops == &sec_buf_page_ops ||
	    dmabuf->ops == &sec_buf_region_ops)
		return 1;

	return 0;
}
EXPORT_SYMBOL_GPL(is_mtk_sec_heap_dmabuf);

/* return 0 means error */
u32 dmabuf_to_secure_handle(const struct dma_buf *dmabuf)
{
	int heap_base;
	struct mtk_sec_heap_buffer *buffer;

	if (!is_mtk_sec_heap_dmabuf(dmabuf)) {
		pr_err("%s err, dmabuf is not secure\n", __func__);
		return 0;
	}
	buffer = dmabuf->priv;
	heap_base = get_heap_base_type(buffer->heap);
	if (heap_base != REGION_BASE) {
		pr_warn("%s failed, heap(%s) not support sec_handle\n",
			__func__, dma_heap_get_name(buffer->heap));
		return 0;
	}

	pr_info("%s done, secure_handle:%u\n", __func__, buffer->sec_handle);
	return buffer->sec_handle;
}
EXPORT_SYMBOL_GPL(dmabuf_to_secure_handle);

int dmabuf_to_sec_id(const struct dma_buf *dmabuf, u32 *sec_hdl)
{
	struct mtk_sec_heap_buffer *buffer = NULL;
	struct secure_heap_region *sec_heap = NULL;

	if (!is_region_base_dmabuf(dmabuf)) {
		pr_err("%s err, dmabuf is not region base\n", __func__);
		return -1;
	}

	*sec_hdl = dmabuf_to_secure_handle(dmabuf);

	buffer = dmabuf->priv;
	sec_heap = sec_heap_region_get(buffer->heap);
	if (!sec_heap) {
		pr_err("%s, sec_heap_region_get(%s) failed!!\n", __func__,
		       buffer->heap ? dma_heap_get_name(buffer->heap) :
		       "null ptr");
		return -1;
	}

	return tmem_type2sec_id(sec_heap->tmem_type);
}
EXPORT_SYMBOL_GPL(dmabuf_to_sec_id);

/*
 * NOTE: the range of heap_id is (s, e), not [s, e] or [s, e)
 */
static int mtk_region_heap_create(struct device *dev,
		enum sec_heap_region_type s, enum sec_heap_region_type e)
{
	struct dma_heap_export_info exp_info;
	int i, j;

	/* region base & page base use same heap show */
	exp_info.priv = (void *)&mtk_sec_heap_priv;

	/* No need pagepool for secure heap */
	exp_info.ops = &sec_heap_region_ops;

	for (i = (s + 1); i < e; i++) {
		for (j = REGION_HEAP_NORMAL; j < REGION_TYPE_NUM; j++) {
			exp_info.name = mtk_sec_heap_region[i].heap_name[j];
			mtk_sec_heap_region[i].heap[j] = dma_heap_add(&exp_info);
			if (IS_ERR(mtk_sec_heap_region[i].heap[j]))
				return PTR_ERR(mtk_sec_heap_region[i].heap[j]);

			mtk_sec_heap_region[i].heap_dev = dev;
			dma_set_mask_and_coherent(mtk_sec_heap_region[i].heap_dev,
						  DMA_BIT_MASK(34));
			mutex_init(&mtk_sec_heap_region[i].heap_lock);

			pr_info("%s add heap[%s][%d] dev:%s, success\n", __func__,
				exp_info.name, mtk_sec_heap_region[i].tmem_type,
				dev_name(mtk_sec_heap_region[i].heap_dev));
		}
	}

	return 0;
}

static int apu_region_heap_probe(struct platform_device *pdev)
{
	int ret;

	ret = mtk_region_heap_create(&pdev->dev, APU_HEAP_START, APU_HEAP_END);
	if (ret)
		pr_err("%s failed\n", __func__);

	return ret;
}

static int mm_region_heap_probe(struct platform_device *pdev)
{
	int ret;

	ret = mtk_region_heap_create(&pdev->dev, MM_HEAP_START, MM_HEAP_END);
	if (ret)
		pr_err("%s failed\n", __func__);

	return ret;
}


static const struct of_device_id mm_region_match_table[] = {
	{.compatible = "mediatek,dmaheap-region-base"},
	{},
};

static const struct of_device_id apu_region_match_table[] = {
	{.compatible = "mediatek,dmaheap-region-base-apu"},
	{},
};

static struct platform_driver mm_region_base = {
	.probe = mm_region_heap_probe,
	.driver = {
		.name = "mm-region-base",
		.of_match_table = mm_region_match_table,
	},
};

static struct platform_driver apu_region_base = {
	.probe = apu_region_heap_probe,
	.driver = {
		.name = "apu-region-base",
		.of_match_table = apu_region_match_table,
	},
};

static struct platform_driver *const mtk_region_heap_drivers[] = {
	&mm_region_base,
	&apu_region_base,
};

static int mtk_page_heap_create(void)
{
	struct dma_heap_export_info exp_info;
	int i;

	/* No need pagepool for secure heap */
	exp_info.ops = &sec_heap_page_ops;
	for (i = SVP_PAGE; i < PAGE_HEAPS_NUM; i++) {
		/* param check */
		if (mtk_sec_heap_page[i].heap_type != PAGE_BASE) {
			pr_info("invalid heap param, %s, %d\n",
				mtk_sec_heap_page[i].heap_name,
				mtk_sec_heap_page[i].heap_type);
			continue;
		}

		exp_info.name = mtk_sec_heap_page[i].heap_name;

		mtk_sec_heap_page[i].heap = dma_heap_add(&exp_info);
		if (IS_ERR(mtk_sec_heap_page[i].heap)) {
			pr_err("%s error, dma_heap_add failed, heap:%s\n",
			       __func__, mtk_sec_heap_page[i].heap_name);
			return PTR_ERR(mtk_sec_heap_page[i].heap);
		}
		pr_info("%s add heap[%s][%d] success\n", __func__,
			exp_info.name, mtk_sec_heap_page[i].tmem_type);
	}

	return 0;
}

static int __init mtk_sec_heap_init(void)
{
	int ret;
	int i;

	pr_info("%s+\n", __func__);
	ret = mtk_page_heap_create();
	if (ret) {
		pr_err("page_base_heap_create failed\n");
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(mtk_region_heap_drivers); i++) {
		pr_info("%s, register %d\n", __func__, i);
		ret = platform_driver_register(mtk_region_heap_drivers[i]);
		if (ret < 0) {
			pr_err("Failed to register %s driver: %d\n",
				  mtk_region_heap_drivers[i]->driver.name, ret);
			goto err;
		}
	}
	pr_info("%s-\n", __func__);

	return 0;

err:
	while (--i >= 0)
		platform_driver_unregister(mtk_region_heap_drivers[i]);

	return ret;
}

static void __exit mtk_sec_heap_exit(void)
{
	int i;

	for (i = ARRAY_SIZE(mtk_region_heap_drivers) - 1; i >= 0; i--)
		platform_driver_unregister(mtk_region_heap_drivers[i]);
}

module_init(mtk_sec_heap_init);
module_exit(mtk_sec_heap_exit);
MODULE_LICENSE("GPL v2");
