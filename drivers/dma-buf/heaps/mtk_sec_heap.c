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
#include <dt-bindings/memory/mtk-memory-port.h>
#include "mtk_heap_debug.h"
#include "mtk_iommu.h"

enum sec_heap_type {
	SVP_REGION = 0,
	SVP_PAGE,
	PROT_REGION,
	PROT_PAGE,
	PROT_2D_FR_REGION,
	WFD_REGION,
	SAPU_DATA_SHM_REGION,
	SAPU_ENGINE_SHM_REGION,

	SECURE_HEAPS_NUM,
};

enum HEAP_BASE_TYPE {
	HEAP_TYPE_INVALID = 0,
	REGION_BASE,
	PAGE_BASE,
	HEAP_BASE_NUM
};

#define NAME_LEN 32

struct secure_heap {
	bool heap_mapped; /* indicate whole region if it is mapped */
	struct mutex heap_lock;
	char heap_name[NAME_LEN];
	atomic64_t total_size;
	phys_addr_t region_pa;
	u32 region_size;
	struct sg_table *region_table;
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

	void *priv;
};

struct sec_heap_dev_info {
	struct device           *dev;
	enum dma_data_direction direction;
	unsigned long           map_attrs;
};

#define BUF_PRIV_MAX_CNT             MTK_M4U_DOM_NR_MAX
struct sec_buf_priv {
	bool                     mapped[BUF_PRIV_MAX_CNT];
	struct sec_heap_dev_info dev_info[BUF_PRIV_MAX_CNT];
	/* secure heap will not strore sgtable here */
	struct sg_table          *mapped_table[BUF_PRIV_MAX_CNT];
	struct mutex             map_lock; /* map iova lock */
	pid_t                    pid;
	pid_t                    tid;
	char                     pid_name[TASK_COMM_LEN];
	char                     tid_name[TASK_COMM_LEN];
	u32                      sec_handle;/* keep same type with tmem */
};

static struct secure_heap mtk_sec_heap[SECURE_HEAPS_NUM] = {
	[SVP_REGION] = {
		.heap_name = "mtk_svp_region-uncached",
		.tmem_type = TRUSTED_MEM_REQ_SVP_REGION,
		.heap_type = REGION_BASE,
	},
	[SVP_PAGE] = {
		.heap_name = "mtk_svp_page-uncached",
		.tmem_type = TRUSTED_MEM_REQ_SVP_PAGE,
		.heap_type = PAGE_BASE,
	},
	[PROT_REGION] = {
		.heap_name = "mtk_prot_region-uncached",
		.tmem_type = TRUSTED_MEM_REQ_PROT_REGION,
		.heap_type = REGION_BASE,
	},
	[PROT_PAGE] = {
		.heap_name = "mtk_prot_page-uncached",
		.tmem_type = TRUSTED_MEM_REQ_PROT_PAGE,
		.heap_type = PAGE_BASE,
	},
	[PROT_2D_FR_REGION] = {
		.heap_name = "mtk_2d_fr-uncached",
		.tmem_type = TRUSTED_MEM_REQ_2D_FR,
		.heap_type = REGION_BASE,
	},
	[WFD_REGION] = {
		.heap_name = "mtk_wfd-uncached",
		.tmem_type = TRUSTED_MEM_REQ_WFD,
		.heap_type = REGION_BASE,
	},
	[SAPU_DATA_SHM_REGION] = {
		.heap_name = "mtk_sapu_data_shm-uncached",
		.tmem_type = TRUSTED_MEM_REQ_SAPU_DATA_SHM,
		.heap_type = REGION_BASE,
	},
	[SAPU_ENGINE_SHM_REGION] = {
		.heap_name = "mtk_sapu_engine_shm-uncached",
		.tmem_type = TRUSTED_MEM_REQ_SAPU_ENGINE_SHM,
		.heap_type = REGION_BASE,
	},
};

/* Function Declcare */
static int is_mtk_secure_dmabuf(const struct dma_buf *dmabuf);

static struct secure_heap *dma_sec_heap_get(const struct dma_heap *heap)
{
	int i = 0;

	for (; i < SECURE_HEAPS_NUM; i++) {
		if (mtk_sec_heap[i].heap == heap)
			return &mtk_sec_heap[i];
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

static void region_base_free(struct secure_heap *sec_heap, struct mtk_sec_heap_buffer *buffer)
{
	int i;
	u32 sec_handle = 0;
	struct sec_buf_priv *buf_info = buffer->priv;

	pr_info("%s start, [%s] size:0x%lx\n",
		__func__, sec_heap->heap_name, buffer->len);

	sec_handle = buf_info->sec_handle;
	trusted_mem_api_unref(sec_heap->tmem_type, sec_handle,
			      (uint8_t *)dma_heap_get_name(buffer->heap), 0, NULL);

	if (!atomic64_read(&sec_heap->total_size)) {
		if (sec_heap->heap_mapped) {
			dma_unmap_sgtable(sec_heap->heap_dev, sec_heap->region_table,
					  DMA_BIDIRECTIONAL, DMA_ATTR_SKIP_CPU_SYNC);
			sec_heap->heap_mapped = false;
		}
		sg_free_table(sec_heap->region_table);
		kfree(sec_heap->region_table);
		pr_info("%s: all secure memory already free, unmap heap_region iova\n", __func__);
	}

	/* remove all domains' sgtable */
	for (i = 0; i < BUF_PRIV_MAX_CNT; i++) {
		struct sg_table *table = buf_info->mapped_table[i];
		struct sec_heap_dev_info dev_info = buf_info->dev_info[i];
		unsigned long attrs = dev_info.map_attrs;

		if(buffer->uncached)
			attrs |= DMA_ATTR_SKIP_CPU_SYNC;
		if (!buf_info->mapped[i] || dev_is_normal_region(dev_info.dev))
			continue;
		pr_info("%s: free dom:%d iova:0x%lx, dev:%s\n", __func__,
			i, (unsigned long)sg_dma_address(table->sgl), dev_name(dev_info.dev));
		dma_unmap_sgtable(dev_info.dev, table, dev_info.direction, attrs);
		buf_info->mapped[i] = false;
		sg_free_table(table);
		kfree(table);
	}

	pr_info("%s done, [%s] size:0x%lx\n",
		__func__, sec_heap->heap_name, buffer->len);
}


static void page_base_free(struct secure_heap *sec_heap, struct mtk_sec_heap_buffer *buffer)
{
	int i;
	struct sec_buf_priv *buf_info = buffer->priv;

	pr_info("%s start, [%s] size:0x%lx\n",
		__func__, sec_heap->heap_name, buffer->len);

	/* remove all domains' sgtable */
	for (i = 0; i < BUF_PRIV_MAX_CNT; i++) {
		struct sg_table *table = buf_info->mapped_table[i];
		struct sec_heap_dev_info dev_info = buf_info->dev_info[i];
		unsigned long attrs = dev_info.map_attrs;

		if(buffer->uncached)
			attrs |= DMA_ATTR_SKIP_CPU_SYNC;
		if (!buf_info->mapped[i])
			continue;
		pr_info("%s: free region:%d iova:0x%lx, dev:%s\n", __func__,
			i, (unsigned long)sg_dma_address(table->sgl), dev_name(dev_info.dev));
		dma_unmap_sgtable(dev_info.dev, table, dev_info.direction, attrs);
		buf_info->mapped[i] = false;
		sg_free_table(table);
		kfree(table);
	}

	pr_info("%s done, [%s] size:0x%lx\n",
		__func__, sec_heap->heap_name, buffer->len);
}

static void tmem_free(struct dma_buf *dmabuf)
{
	struct secure_heap *sec_heap;
	struct mtk_sec_heap_buffer *buffer = NULL;
	struct sec_buf_priv *buf_info = NULL;

	if (unlikely(!is_mtk_secure_dmabuf(dmabuf))) {
		pr_err("%s err, dmabuf is not secure\n", __func__);
		return;
	}

	buffer = dmabuf->priv;
	sec_heap = dma_sec_heap_get(buffer->heap);
	if (!sec_heap) {
		pr_err("%s, can not find secure heap!!\n",
			__func__, buffer->heap ? dma_heap_get_name(buffer->heap) : "null ptr");
		return;
	}
	buf_info = buffer->priv;

	pr_info("%s start, [%s]: heap_ptr:0x%lx\n",
		__func__, dmabuf->exp_name, (unsigned long)sec_heap);

	if (atomic64_sub_return(buffer->len, &sec_heap->total_size) < 0)
		pr_warn("%s, total memory overflow, 0x%lx!!", __func__, atomic64_read(&sec_heap->total_size));

	if (sec_heap->heap_type == REGION_BASE)
		region_base_free(sec_heap, buffer);
	else if (sec_heap->heap_type == PAGE_BASE)
		page_base_free(sec_heap, buffer);
	else
		pr_err("%s err, heap_type(%d) is invalid\n", __func__, sec_heap->heap_type);

	/* TODO: why do you add lock???? */
	/* mutex_lock(&dmabuf->lock); */
	kfree(buf_info);
	kfree(buffer);
	/* mutex_unlock(&dmabuf->lock); */

	pr_info("%s done: [%s], size:0x%lx, total_size:0x%lx\n", __func__,
		dmabuf->exp_name, buffer->len, atomic64_read(&sec_heap->total_size));
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
		pr_info("nents not match %d-%d\n",
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
static int fill_sec_buffer_info(struct sec_buf_priv *info,
                           struct sg_table *table,
                           struct dma_buf_attachment *a,
                           enum dma_data_direction dir,
                           int iommu_dom_id) {
	struct sg_table *new_table = NULL;
	int ret = 0;

	/*
	 * devices without iommus attribute,
	 * use common flow, skip set buf_info
	 */
	if (iommu_dom_id >= BUF_PRIV_MAX_CNT)
		return 0;

	if (info->mapped[iommu_dom_id]) {
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

	info->mapped_table[iommu_dom_id] = new_table;
	info->mapped[iommu_dom_id] = true;
	info->dev_info[iommu_dom_id].dev = a->dev;
	info->dev_info[iommu_dom_id].direction = dir;
	info->dev_info[iommu_dom_id].map_attrs = a->dma_map_attrs;

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
		pr_info("%s pass, i:%d(%u), pa:%pa, size:0x%x", __func__, i, table->orig_nents, &s_phys, len);
	}

	return 0;
}

static struct sg_table *mtk_sec_heap_map_dma_buf(struct dma_buf_attachment *attachment,
						 enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	struct sg_table *table = a->table;
	struct dma_buf *dmabuf = attachment->dmabuf;
	struct mtk_sec_heap_buffer *buffer;
	struct secure_heap *sec_heap;
	struct sec_buf_priv *buf_info;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(attachment->dev);
	int dom_id = BUF_PRIV_MAX_CNT;
	//u32 refcount = 0;/* tmem refcount */
	int ret;
	dma_addr_t dma_address;
	uint64_t phy_addr = 0;
	int attr = attachment->dma_map_attrs;

	pr_info("%s start dev:%s\n", __func__, dev_name(attachment->dev));

	if (unlikely(!is_mtk_secure_dmabuf(dmabuf))) {
		pr_err("%s err, dmabuf is not secure\n", __func__);
		return NULL;
	}

	if (a->uncached)
		attr |= DMA_ATTR_SKIP_CPU_SYNC;

	/* non-iommu master */
	if (!fwspec) {
		ret = dma_map_sgtable(attachment->dev, table, direction, attr);
		if (ret) {
			pr_err("%s err, non-iommu-dev(%s) dma_map_sgtable failed\n",
			       __func__, dev_name(attachment->dev));
			return ERR_PTR(ret);
		}
		a->mapped = true;
		pr_info("%s success, non-iommu-dev(%s)\n", __func__, dev_name(attachment->dev));
		return table;
	}

	buffer = dmabuf->priv;
	buf_info = buffer->priv;
	sec_heap = dma_sec_heap_get(buffer->heap);
	if (!sec_heap) {
		pr_err("%s, dma_sec_heap_get failed\n", __func__);
		return NULL;
	}
	if (sec_heap->heap_type != REGION_BASE)
		goto skip_region_map;

	mutex_lock(&sec_heap->heap_lock);
	if (!sec_heap->heap_mapped) {
		if (dma_map_sgtable(sec_heap->heap_dev, sec_heap->region_table,
				    DMA_BIDIRECTIONAL, DMA_ATTR_SKIP_CPU_SYNC)) {
			pr_err("%s err, heap_region(%s) dma_map_sgtable failed\n",
				__func__, dev_name(sec_heap->heap_dev));
			mutex_unlock(&sec_heap->heap_lock);
			return ERR_PTR(-ENOMEM);
		}
		sec_heap->heap_mapped = true;
		pr_info("%s heap_region map success, heap:%s, iova:0x%lx, sz:0x%lx\n",
			__func__, sec_heap->heap_name,
			(unsigned long)sg_dma_address(sec_heap->region_table->sgl),
			(unsigned long)sg_dma_len(sec_heap->region_table->sgl));
	}
	mutex_unlock(&sec_heap->heap_lock);

skip_region_map:
	dom_id = MTK_M4U_TO_DOM(fwspec->ids[0]);
	phy_addr = (uint64_t)sg_phys(table->sgl);
	mutex_lock(&buf_info->map_lock);
	/* device with iommus attribute and mapped before */
	if (buf_info->mapped[dom_id]) {
		/* mapped before, return saved table */
		ret = copy_sec_sg_table(buf_info->mapped_table[dom_id], table);
		if (ret) {
			pr_err("%s err, copy_sec_sg_table failed, dev:%s\n", __func__, dev_name(attachment->dev));
			mutex_unlock(&buf_info->map_lock);
			return ERR_PTR(-EINVAL);
		}
		a->mapped = true;
		pr_info("%s done(has mapped), dev:%s(%s), sec_handle:%u, len:0x%lx, pa:0x%llx, iova:0x%lx\n", __func__,
			dev_name(buf_info->dev_info[dom_id].dev), dev_name(attachment->dev),
			buf_info->sec_handle, buffer->len, phy_addr, (unsigned long)sg_dma_address(table->sgl));
		mutex_unlock(&buf_info->map_lock);
		return table;
	}
	if (!dev_is_normal_region(attachment->dev) || sec_heap->heap_type == PAGE_BASE) {
		ret = check_map_alignment(table);
		if (ret) {
			pr_err("%s err, size or PA is not 1MB alignment, dev:%s\n", __func__, dev_name(attachment->dev));
			mutex_unlock(&buf_info->map_lock);
			return ERR_PTR(ret);
		}
		ret = dma_map_sgtable(attachment->dev, table, direction, attr);
		if (ret) {
			pr_err("%s err, iommu-dev(%s) dma_map_sgtable failed\n", __func__, dev_name(attachment->dev));
			mutex_unlock(&buf_info->map_lock);
			return ERR_PTR(ret);
		}
		pr_info("%s iommu-dev(%s) dma_map_sgtable done, iova:0x%lx\n",
			__func__, dev_name(attachment->dev), sg_dma_address(table->sgl));
		goto map_done;
	}
#if 0
	/* Maybe get pa by sg_table */
	ret = trusted_mem_api_query_pa(sec_heap->tmem_type, 0, buffer->len, &refcount,
				       &buf_info->sec_handle, (u8 *)dma_heap_get_name(sec_heap->heap),
				       0, 0, &phy_addr);
	if (ret) {
		pr_err("%s err. query pa failed. heap:%s\n", __func__, sec_heap->heap_name);
		mutex_unlock(&buf_info->map_lock);
		return ERR_PTR(-ENOMEM);
	}
#endif
	if (buffer->len <= 0 || buffer->len > sec_heap->region_size ||
	    phy_addr < sec_heap->region_pa ||
	    phy_addr > sec_heap->region_pa + sec_heap->region_size ||
	    phy_addr + buffer->len > sec_heap->region_pa + sec_heap->region_size ||
	    phy_addr + buffer->len <= sec_heap->region_pa) {
		pr_err("%s err. req size/pa is invalid! heap:%s\n", __func__, sec_heap->heap_name);
	   	mutex_unlock(&buf_info->map_lock);
		return ERR_PTR(-ENOMEM);
	}
	dma_address = phy_addr - sec_heap->region_pa + sg_dma_address(sec_heap->region_table->sgl);
	sg_dma_address(table->sgl) = dma_address;

map_done:
	ret = fill_sec_buffer_info(buf_info, table, attachment, direction, dom_id);
	if (ret) {
		pr_err("%s failed, fill_sec_buffer_info failed, dev:%s\n", __func__, dev_name(attachment->dev));
		mutex_unlock(&buf_info->map_lock);
		return ERR_PTR(-ENOMEM);
	}
	a->mapped = true;

	pr_info("%s done, dev:%s, sec_handle:%u, len:0x%lx, pa:0x%llx, iova:0x%lx\n",
		__func__, dev_name(attachment->dev), buf_info->sec_handle, buffer->len,
		phy_addr, (unsigned long)sg_dma_address(table->sgl));
	mutex_unlock(&buf_info->map_lock);

	return table;
}

static void mtk_sec_heap_unmap_dma_buf(struct dma_buf_attachment *attachment,
				       struct sg_table *table,
				       enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	int attr = attachment->dma_map_attrs;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(attachment->dev);

	if (a->uncached)
		attr |= DMA_ATTR_SKIP_CPU_SYNC;

	if (!fwspec) {
		pr_info("%s, non-iommu-dev unmap, dev:%s\n", __func__, dev_name(attachment->dev));
		dma_unmap_sgtable(attachment->dev, table, direction, 0);
	}
	a->mapped = false;
}

static bool heap_is_region_base(enum HEAP_BASE_TYPE heap_type)
{
	if (heap_type == REGION_BASE)
		return true;

	return false;
}

static int fill_heap_sgtable(struct secure_heap *sec_heap)
{
	int ret;

	pr_info("%s start [%s][%d]\n", __func__, sec_heap->heap_name, sec_heap->tmem_type);

	if (!heap_is_region_base(sec_heap->heap_type)) {
		pr_info("%s skip page base filled\n", __func__);
		return 0;
	}
	/* TODO: race condition????????????????? */
	mutex_lock(&sec_heap->heap_lock);
	if (sec_heap->heap_mapped) {
		mutex_unlock(&sec_heap->heap_lock);
		pr_info("%s, %s already filled\n", __func__, sec_heap->heap_name);
		return 0;
	}

	ret = trusted_mem_api_get_region_info(sec_heap->tmem_type,
		&sec_heap->region_pa, &sec_heap->region_size);
	if (!ret) {
		mutex_unlock(&sec_heap->heap_lock);
		pr_err("%s, [%s],get_region_info failed!\n", __func__, sec_heap->heap_name);
		return -EINVAL;
	}

	sec_heap->region_table = kzalloc(sizeof(*sec_heap->region_table), GFP_KERNEL);
	if (!sec_heap->region_table) {
		mutex_unlock(&sec_heap->heap_lock);
		pr_err("%s, [%s] kzalloc_sgtable failed\n", __func__, sec_heap->heap_name);
		return ret;
	}
	ret = sg_alloc_table(sec_heap->region_table, 1, GFP_KERNEL);
	if (ret) {
		mutex_unlock(&sec_heap->heap_lock);
		pr_err("%s, [%s] alloc_sgtable failed\n", __func__, sec_heap->heap_name);
		return ret;
	}
	atomic64_set(&sec_heap->total_size, 0);
	sg_set_page(sec_heap->region_table->sgl, phys_to_page(sec_heap->region_pa),
		    sec_heap->region_size, 0);
	mutex_unlock(&sec_heap->heap_lock);
	pr_info("%s [%s] fill done, region_pa:%pa, region_size:0x%x\n",
		__func__, sec_heap->heap_name, &sec_heap->region_pa, sec_heap->region_size);
	return 0;
}

static const struct dma_buf_ops sec_buf_ops = {
	/* one attachment can only map once */
	.cache_sgt_mapping = 1,
	.attach = mtk_sec_heap_attach,
	.detach = mtk_sec_heap_detach,
	.map_dma_buf = mtk_sec_heap_map_dma_buf,
	.unmap_dma_buf = mtk_sec_heap_unmap_dma_buf,
	.release = tmem_free,
};

/* region base size is 4K alignment */
static int region_base_alloc(struct secure_heap *sec_heap,
				struct mtk_sec_heap_buffer *buffer, struct sec_buf_priv *info)
{
	int ret;
	u32 sec_handle = 0;
	u32 refcount = 0;/* tmem refcount */
	uint64_t phy_addr = 0;
	struct sg_table *table;

	pr_info("%s start: [%s], req_size:0x%lx\n",
		__func__, dma_heap_get_name(sec_heap->heap), buffer->len);

	ret = trusted_mem_api_alloc(sec_heap->tmem_type, 0, (unsigned int *)&buffer->len, &refcount,
				    &sec_handle,
				    (uint8_t *)dma_heap_get_name(sec_heap->heap),
				    0, NULL);
	if (ret == -ENOMEM) { //return value ???????????????????
		pr_err("%s error: security out of memory!! heap:%s\n",
			__func__, dma_heap_get_name(sec_heap->heap));
		return -ENOMEM;
	}
	if (!sec_handle) {
		pr_err("%s alloc security memory failed, req_size:0x%lx, total_size 0x%lx\n",
			__func__, buffer->len, atomic64_read(&sec_heap->total_size));
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
				       &sec_handle, (u8 *)dma_heap_get_name(sec_heap->heap),
				       0, 0, &phy_addr);
	if (ret) {
		/* free buffer */
		pr_err("%s#%d Error. query pa failed.\n",
			__func__, __LINE__);
		goto free_buffer_struct;
	}
	sg_set_page(table->sgl, phys_to_page(phy_addr), buffer->len, 0);
	/* store seucre handle */
	info->sec_handle = sec_handle;

	ret = fill_heap_sgtable(sec_heap);
	if (ret) {
		pr_err("%s#%d Error. fill_heap_sgtable failed.\n",
			__func__, __LINE__);
		goto free_buffer_struct;
	}

	pr_info("%s done: [%s], req_size:0x%lx, handle:%u, pa:0x%lx\n",
		__func__, dma_heap_get_name(sec_heap->heap), buffer->len,
		info->sec_handle, phy_addr);

	return 0;

free_buffer_struct:
	sg_free_table(table);
free_buffer:
	/* free secure handle */
	trusted_mem_api_unref(sec_heap->tmem_type, sec_handle,
			      (uint8_t *)dma_heap_get_name(buffer->heap), 0, NULL);

	return ret;
}

static int page_base_alloc(struct secure_heap *sec_heap,
				struct mtk_sec_heap_buffer *buffer, struct sec_buf_priv *info)
{
	return 0;
}

static struct dma_buf *tmem_allocate(struct dma_heap *heap,
				     unsigned long len,
				     unsigned long fd_flags,
				     unsigned long heap_flags)
{
	struct secure_heap *sec_heap = dma_sec_heap_get(heap);
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct dma_buf *dmabuf;
	int ret = -ENOMEM;
	struct mtk_sec_heap_buffer *buffer;
	struct sec_buf_priv *info;
	struct task_struct *task = current->group_leader;

	if (!sec_heap) {
		pr_err("%s, can not find secure heap!!\n",
			__func__, heap ? dma_heap_get_name(heap) : "null ptr");
		return ERR_PTR(-EINVAL);
	}
	pr_info("%s start, [%s]:heap_ptr:0x%llx, size 0x%lx, total_sz:0x%lx\n",
		 __func__, dma_heap_get_name(heap), (unsigned long)sec_heap,
		 len, atomic64_read(&sec_heap->total_size));

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!buffer || !info) {
		pr_err("%s#%d Error. Allocate mem failed.\n",
			__func__, __LINE__);
		return ERR_PTR(-ENOMEM);
	}
	INIT_LIST_HEAD(&buffer->attachments);
	mutex_init(&buffer->lock);
	buffer->heap = heap;
	buffer->len = len;
	/* all secure memory set as uncached buffer */
	buffer->uncached = true;
	if (sec_heap->heap_type == REGION_BASE)
		ret = region_base_alloc(sec_heap, buffer, info);
	else if (sec_heap->heap_type == PAGE_BASE)
		ret = page_base_alloc(sec_heap, buffer, info);
	else
		pr_err("%s err, heap_type(%d) is invalid\n", __func__, sec_heap->heap_type);

	if (ret)
		goto free_buffer;

	/* create the dmabuf */
	exp_info.exp_name = dma_heap_get_name(heap);
	exp_info.ops = &sec_buf_ops;
	exp_info.size = buffer->len;
	exp_info.flags = fd_flags;
	exp_info.priv = buffer;
	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		pr_err("%s dma_buf_export fail\n", __func__);
		goto free_tmem;
	}
	/* add debug info */
	buffer->priv = info;
	mutex_init(&info->map_lock);
	/* add alloc pid & tid info */
	get_task_comm(info->pid_name, task);
	get_task_comm(info->tid_name, current);
	info->pid = task_pid_nr(task);
	info->tid = task_pid_nr(current);
	atomic64_add(len, &sec_heap->total_size);

	pr_info("%s done: [%s], req_size:0x%lx, total_size:0x%lx\n",
		__func__, dma_heap_get_name(heap), buffer->len,
		atomic64_read(&sec_heap->total_size));

	return dmabuf;

free_tmem:
	if (sec_heap->heap_type == REGION_BASE)
		trusted_mem_api_unref(sec_heap->tmem_type, info->sec_handle,
			      (uint8_t *)dma_heap_get_name(buffer->heap), 0, NULL);
	else /* TBD */
		trusted_mem_api_unref(sec_heap->tmem_type, info->sec_handle,
			      (uint8_t *)dma_heap_get_name(buffer->heap), 0, NULL);
	sg_free_table(&buffer->sg_table);
free_buffer:
	kfree(info);
	kfree(buffer);
	return ERR_PTR(ret);
}

static const struct dma_heap_ops sec_heap_ops = {
	.allocate = tmem_allocate,
};

/* no '\n' at end of str */
static char *sec_get_buf_dump_str(const struct dma_buf *dmabuf,
				  const struct dma_heap *heap) {
	struct mtk_sec_heap_buffer *buf = dmabuf->priv;
	struct sec_buf_priv *buf_priv = NULL;
	struct dma_heap *buf_heap = NULL;
	char *info_str;
	int len = 0;

	/* buffer check */
	if (!is_mtk_secure_dmabuf(dmabuf)) {
		pr_err("%s err, dmabuf is not secure\n", __func__);
		return NULL;
	}

	buf_priv = buf->priv;
	buf_heap = buf->heap;

	/* heap check */
	if (heap != buf_heap)
		return NULL;

	info_str = kzalloc(sizeof(char) * (DUMP_INFO_LEN_MAX + 1), GFP_KERNEL);
	if (!info_str)
		return NULL;

	len += scnprintf(info_str + len,
			 DUMP_INFO_LEN_MAX - len,
			 "%s \t%p \t0x%lx \t%s \t%s \t%d \t%x \t%x \t%ld \t%lu \t%d(%s) \t%d(%s)",
			 dma_heap_get_name(buf_heap),
			 dmabuf,
			 dmabuf->size, dmabuf->exp_name,
			 dmabuf->name ?: "NULL",
			 !!buf->uncached,
			 dmabuf->file->f_flags,
			 dmabuf->file->f_mode,
			 file_count(dmabuf->file),
			 file_inode(dmabuf->file)->i_ino,
			 /* after this is private part */
			 buf_priv->pid, buf_priv->pid_name,
			 buf_priv->tid, buf_priv->tid_name);

#if 0
	for(i = 0; i < BUF_PRIV_MAX_CNT; i++) {
		if (len >= BUF_PRIV_MAX_CNT) {
			pr_info("%s #%d: out of dump mem %d-%d\n",
				__func__, __LINE__, len, BUF_PRIV_MAX_CNT);
			break;
		}
		len += scnprintf(info_str + len,
				 DUMP_INFO_LEN_MAX - len,
				 " \t%d \t%s \t%d \t%lu \t%p",
				 buf_priv->mapped[i],
				 dev_name(buf_priv->dev_info[i].dev),
				 buf_priv->dev_info[i].direction,
				 buf_priv->dev_info[i].map_attrs,
				 buf_priv->mapped_table[i]);
	}
#endif
	return info_str;

}

/* no '\n' at end of str */
static char *sec_get_buf_dump_fmt(const struct dma_heap *heap) {
	//int i;
	char *fmt_str = NULL;
	int len = 0;

	fmt_str = kzalloc(sizeof(char) * (DUMP_INFO_LEN_MAX + 1), GFP_KERNEL);
	if (!fmt_str)
		return NULL;

	len += scnprintf(fmt_str + len,
			 DUMP_INFO_LEN_MAX - len,
			 "heap_name \tdmabuf \tsize(hex) \texp_name \tdmabuf_name \tuncached \tf_flag \tf_mode \tf_count \tino \tpid(name) \ttid(name)");

#if 0
	for(i = 0; i < BUF_PRIV_MAX_CNT; i++) {
		len += scnprintf(fmt_str + len,
				 DUMP_INFO_LEN_MAX - len,
				 " \tmapped-%d \tdev_name-%d \tdir-%d \tmap_attrs-%d \tsgt-%d",
				 i, i, i, i, i);
	}
#endif
	return fmt_str;

}

#ifndef SKIP_DMBUF_BUFFER_DUMP
static int sec_dump_buf_info_cb(const struct dma_buf *dmabuf,
			       void *priv) {
	struct mtk_heap_dump_t *dump_param = priv;
	struct seq_file *s = dump_param->file;
	struct dma_heap *dump_heap = dump_param->heap;
	struct mtk_sec_heap_buffer *buf = dmabuf->priv;
	struct dma_heap *buf_heap;
	struct mtk_heap_priv_info *heap_priv = NULL;
	char *buf_dump_str = NULL;

	/* dmabuf check */
	if (!buf || !buf->heap || buf->heap != dump_heap)
		return 0;

	buf_heap = buf->heap;
	heap_priv = mtk_heap_priv_get(buf_heap);

	buf_dump_str = heap_priv->get_buf_dump_str(dmabuf, dump_heap);
	dmabuf_dump(s, "%s\n", buf_dump_str);

	kfree(buf_dump_str);

	return 0;
}
#endif

static void sec_dmaheap_show(struct dma_heap *heap,
			     void* seq_file) {
	struct seq_file *s = seq_file;
	struct mtk_heap_dump_t dump_param;

#ifndef SKIP_DMBUF_BUFFER_DUMP
	struct mtk_heap_priv_info *heap_priv = NULL;
	const char * dump_fmt = NULL;
#endif
	dump_param.heap = heap;
	dump_param.file = seq_file;

	__HEAP_DUMP_START(s, heap);
	__HEAP_TOTAL_BUFFER_SZ_DUMP(s, heap);
	__HEAP_PAGE_POOL_DUMP(s, heap);

#ifndef SKIP_DMBUF_BUFFER_DUMP
	__HEAP_BUF_DUMP_START(s, heap);

	dump_fmt = heap_priv->get_buf_dump_fmt(heap);
	dmabuf_dump(s, "\t%s\n", dump_fmt);
	kfree(dump_fmt);
	get_each_dmabuf(sec_dump_buf_info_cb, &dump_param);
#endif

	__HEAP_ATTACH_DUMP_STAT(s, heap);
	get_each_dmabuf(dma_heap_default_attach_dump_cb, &dump_param);
	__HEAP_ATTACH_DUMP_END(s, heap);

	__HEAP_DUMP_END(s, heap);

}

static const struct mtk_heap_priv_info mtk_sec_heap_priv = {
	.get_buf_dump_str = sec_get_buf_dump_str,
	.get_buf_dump_fmt = sec_get_buf_dump_fmt,
	.show =             sec_dmaheap_show,
};

static int is_mtk_secure_dmabuf(const struct dma_buf *dmabuf) {
	int i = 0;
	struct mtk_sec_heap_buffer *buffer = NULL;

	if (!dmabuf || !dmabuf->priv)
		return 0;

	buffer = dmabuf->priv;

	for (; i < SECURE_HEAPS_NUM; i++) {
		if (mtk_sec_heap[i].heap == buffer->heap)
			return 1;
	}

	return 0;
}

/* return 0 means error */
u32 dmabuf_to_secure_handle(struct dma_buf *dmabuf)
{
	struct mtk_sec_heap_buffer *buffer;
	struct sec_buf_priv *buf_info;
	struct secure_heap *sec_heap;

	if (!is_mtk_secure_dmabuf(dmabuf)) {
		pr_err("%s err, dmabuf is not secure\n", __func__);
		return 0;
	}
	buffer = dmabuf->priv;
	sec_heap = dma_sec_heap_get(buffer->heap);
	if (!heap_is_region_base(sec_heap->heap_type)) {
		pr_warn("%s failed, heap(%s) not support sec_handle\n", __func__, sec_heap->heap_name);
		return 0;
	}

	buffer = dmabuf->priv;
	buf_info = buffer->priv;

	return buf_info->sec_handle;
}
EXPORT_SYMBOL_GPL(dmabuf_to_secure_handle);

static int mtk_sec_heap_create(struct device *dev)
{
	struct dma_heap_export_info exp_info;
	int i;

	/* No need pagepool for secure heap */
	exp_info.ops = &sec_heap_ops;
	exp_info.priv = (void *)&mtk_sec_heap_priv;
	for (i = 0; i < SECURE_HEAPS_NUM; i++) {

		/* param check */
		if (mtk_sec_heap[i].heap_type == HEAP_TYPE_INVALID) {
			pr_info("invalid heap param, %s, %d\n",
				mtk_sec_heap[i].heap_name,
				mtk_sec_heap[i].heap_type);
			continue;
		}

		exp_info.name = mtk_sec_heap[i].heap_name;

		mtk_sec_heap[i].heap = dma_heap_add(&exp_info);
		if (IS_ERR(mtk_sec_heap[i].heap))
			return PTR_ERR(mtk_sec_heap[i].heap);

		if (mtk_sec_heap[i].heap_type == REGION_BASE) {
			mtk_sec_heap[i].heap_dev = dev;
			dma_set_mask_and_coherent(mtk_sec_heap[i].heap_dev, DMA_BIT_MASK(34));
		}
		mutex_init(&mtk_sec_heap[i].heap_lock);
		pr_info("%s add heap[%s][%d] heap_ptr:0x%llx, dev:%s, success\n", __func__,
			exp_info.name, mtk_sec_heap[i].tmem_type, &mtk_sec_heap[i],
			mtk_sec_heap[i].heap_type == REGION_BASE ? dev_name(mtk_sec_heap[i].heap_dev) : "NULL");
	}

	return 0;
}

static const struct of_device_id mtk_sec_heap_of_ids[] = {
	{ .compatible = "mediatek,dmaheap-region-base"},
	{}
};

static int mtk_sec_heap_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;

	ret = mtk_sec_heap_create(dev);
	if (ret) {
		pr_err("%s failed\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int mtk_sec_heap_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver mtk_sec_drvier = {
	.probe	= mtk_sec_heap_probe,
	.remove	= mtk_sec_heap_remove,
	.driver	= {
		.name = "mtk-dmabuf-region-base",
		.of_match_table = mtk_sec_heap_of_ids,
	}
};

/* module_init(mtk_sec_heap_create); */
module_platform_driver(mtk_sec_drvier);
MODULE_LICENSE("GPL v2");
