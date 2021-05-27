// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF mtk_sec heap exporter
 *
 * Copyright (C) 2021 MediaTek Inc.
 *
 */

#define pr_fmt(fmt) "[MTK_DMABUF_HEAP: SEC] "fmt

#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/dma-heap.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "page_pool.h"
#include "deferred-free-helper.h"

#include <public/trusted_mem_api.h>
#include "mtk_heap_debug.h"

enum secure_feature_type {
	SVP_REGION = 0,
	SVP_PAGE,
	PROT_REGION,
	PROT_PAGE,
	PROT_2D_FR_REGION,
	WFD_REGION,
	SAPU_DATA_SHM_REGION,
	SAPU_ENGINE_SHM_REGION,

	__MAX_NR_SECURE_FEATURES,
};

#define NAME_LEN 32

struct sec_feature {
	char feat_name[NAME_LEN];
	const struct dma_heap_ops *ops;
	void *priv;
	struct dma_heap *heap;
};

//TODO: should replace by atomic_t
static size_t sec_heap_total_memory;

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

/* No domain concept in secure memory, set array count as 1 */
#define BUF_PRIV_MAX_CNT             1
struct sec_heap_priv {
	bool                     mapped[BUF_PRIV_MAX_CNT];
	struct sec_heap_dev_info dev_info[BUF_PRIV_MAX_CNT];
	/* secure heap will not strore sgtable here */
	struct sg_table          *mapped_table[BUF_PRIV_MAX_CNT];
	struct mutex             lock; /* map iova lock */
	pid_t                    pid;
	pid_t                    tid;
	char                     pid_name[TASK_COMM_LEN];
	char                     tid_name[TASK_COMM_LEN];
	u32                      sec_handle;/* keep same type with tmem */
};

struct dma_heap_attachment {
	struct device *dev;
	struct sg_table *table;
	struct list_head list;
	bool mapped;

	bool uncached;
};

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

static void tmem_free(enum TRUSTED_MEM_REQ_TYPE tmem_type,
		      struct dma_buf *dmabuf)
{
	struct mtk_sec_heap_buffer *buffer = dmabuf->priv;
	struct sec_heap_priv *buf_info = buffer->priv;
	u32 sec_handle = 0;
	struct sg_table *table = NULL;
	int i;

	pr_debug("[%s][%d] %s: enter priv 0x%lx\n",
		 dmabuf->exp_name, tmem_type,
		 __func__, dmabuf->priv);

	sec_heap_total_memory -= buffer->len;

	sec_handle = buf_info->sec_handle;
	trusted_mem_api_unref(tmem_type, sec_handle,
			      (uint8_t *)dma_heap_get_name(buffer->heap), 0);

	//free sgtable
	/* remove all domains' sgtable */
	for (i = 0; i < BUF_PRIV_MAX_CNT; i++) {
		table = buf_info->mapped_table[i];
		if (!table)
			continue;
		/* if we have secure iova, also need unmap here */

		sg_free_table(table);
		kfree(table);
	}

	mutex_lock(&dmabuf->lock);
	kfree(buf_info);
	kfree(buffer);

	pr_debug("%s: [%s][%d] exit, total %zu\n", __func__,
		 dmabuf->exp_name, tmem_type, sec_heap_total_memory);

	mutex_unlock(&dmabuf->lock);
}

static inline void svp_region_free(struct dma_buf *dmabuf)
{
	enum TRUSTED_MEM_REQ_TYPE tmem_type = TRUSTED_MEM_REQ_SVP_REGION;

	tmem_free(tmem_type, dmabuf);
}

static inline void svp_page_free(struct dma_buf *dmabuf)
{
	enum TRUSTED_MEM_REQ_TYPE tmem_type = TRUSTED_MEM_REQ_SVP_PAGE;

	tmem_free(tmem_type, dmabuf);
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

static struct sg_table *mtk_sec_heap_map_dma_buf(struct dma_buf_attachment *attachment,
						 enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	struct sg_table *table = a->table;

	/* for TZMP2 secure iova, here we need copy into sgtable */
	return table;
}

static void mtk_sec_heap_unmap_dma_buf(struct dma_buf_attachment *attachment,
				       struct sg_table *table,
				       enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	struct sg_table *sgt = a->table;

	/* set dma_address as 0 to clear secure handle*/
	sg_dma_address(sgt->sgl) = 0;
}

static const struct dma_buf_ops svp_region_heap_buf_ops = {
	.attach = mtk_sec_heap_attach,
	.detach = mtk_sec_heap_detach,
	.map_dma_buf = mtk_sec_heap_map_dma_buf,
	.unmap_dma_buf = mtk_sec_heap_unmap_dma_buf,
	.release = svp_region_free,
};

static const struct dma_buf_ops svp_page_heap_buf_ops = {
	.attach = mtk_sec_heap_attach,
	.detach = mtk_sec_heap_detach,
	.map_dma_buf = mtk_sec_heap_map_dma_buf,
	.unmap_dma_buf = mtk_sec_heap_unmap_dma_buf,
	.release = svp_page_free,
};

static inline void prot_region_free(struct dma_buf *dmabuf)
{
	enum TRUSTED_MEM_REQ_TYPE tmem_type = TRUSTED_MEM_REQ_PROT_REGION;

	tmem_free(tmem_type, dmabuf);
}

static inline void prot_page_free(struct dma_buf *dmabuf)
{
	enum TRUSTED_MEM_REQ_TYPE tmem_type = TRUSTED_MEM_REQ_PROT_PAGE;

	tmem_free(tmem_type, dmabuf);
}

static const struct dma_buf_ops prot_region_heap_buf_ops = {
	.attach = mtk_sec_heap_attach,
	.detach = mtk_sec_heap_detach,
	.map_dma_buf = mtk_sec_heap_map_dma_buf,
	.unmap_dma_buf = mtk_sec_heap_unmap_dma_buf,
	.release = prot_region_free,
};

static const struct dma_buf_ops prot_page_heap_buf_ops = {
	.attach = mtk_sec_heap_attach,
	.detach = mtk_sec_heap_detach,
	.map_dma_buf = mtk_sec_heap_map_dma_buf,
	.unmap_dma_buf = mtk_sec_heap_unmap_dma_buf,
	.release = prot_page_free,
};

static inline void wfd_free(struct dma_buf *dmabuf)
{
	enum TRUSTED_MEM_REQ_TYPE tmem_type = TRUSTED_MEM_REQ_WFD;

	tmem_free(tmem_type, dmabuf);
}

static const struct dma_buf_ops wfd_heap_buf_ops = {
	.attach = mtk_sec_heap_attach,
	.detach = mtk_sec_heap_detach,
	.map_dma_buf = mtk_sec_heap_map_dma_buf,
	.unmap_dma_buf = mtk_sec_heap_unmap_dma_buf,
	.release = wfd_free,
};

static inline void prot_2d_fr_free(struct dma_buf *dmabuf)
{
	enum TRUSTED_MEM_REQ_TYPE tmem_type = TRUSTED_MEM_REQ_2D_FR;

	tmem_free(tmem_type, dmabuf);
}

static const struct dma_buf_ops prot_2d_fr_heap_buf_ops = {
	.attach = mtk_sec_heap_attach,
	.detach = mtk_sec_heap_detach,
	.map_dma_buf = mtk_sec_heap_map_dma_buf,
	.unmap_dma_buf = mtk_sec_heap_unmap_dma_buf,
	.release = prot_2d_fr_free,
};

static inline void sapu_data_shm_free(struct dma_buf *dmabuf)
{
	enum TRUSTED_MEM_REQ_TYPE tmem_type = TRUSTED_MEM_REQ_SDSP_SHARED;

	tmem_free(tmem_type, dmabuf);
}

static const struct dma_buf_ops sapu_data_shm_heap_buf_ops = {
	.attach = mtk_sec_heap_attach,
	.detach = mtk_sec_heap_detach,
	.map_dma_buf = mtk_sec_heap_map_dma_buf,
	.unmap_dma_buf = mtk_sec_heap_unmap_dma_buf,
	.release = sapu_data_shm_free,
};

static inline void sapu_engine_shm_free(struct dma_buf *dmabuf)
{
	enum TRUSTED_MEM_REQ_TYPE tmem_type = TRUSTED_MEM_REQ_SDSP;

	tmem_free(tmem_type, dmabuf);
}

static const struct dma_buf_ops sapu_engine_shm_heap_buf_ops = {
	.attach = mtk_sec_heap_attach,
	.detach = mtk_sec_heap_detach,
	.map_dma_buf = mtk_sec_heap_map_dma_buf,
	.unmap_dma_buf = mtk_sec_heap_unmap_dma_buf,
	.release = sapu_engine_shm_free,
};

static struct dma_buf *tmem_allocate(enum TRUSTED_MEM_REQ_TYPE tmem_type,
				     const struct dma_buf_ops *heap_buf_ops,
				     struct dma_heap *heap,
				     unsigned long len,
				     unsigned long fd_flags,
				     unsigned long heap_flags)
{
	struct mtk_sec_heap_buffer *buffer;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct dma_buf *dmabuf;
	struct sg_table *table;
	int ret = -ENOMEM;
	struct sec_heap_priv *info;
	struct task_struct *task = current->group_leader;
	u32 sec_handle = 0;
	u32 refcount = 0;/* tmem refcount */

	pr_debug("[%s][%d] %s: enter: size 0x%lx\n",
		 dma_heap_get_name(heap), tmem_type, __func__, len);

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	info = kzalloc(sizeof(*info), GFP_KERNEL);
	table = kzalloc(sizeof(*table), GFP_KERNEL);
	if (!buffer || !info || !table) {
		pr_info("%s#%d Error. Allocate mem failed.\n",
			__func__, __LINE__);
		return ERR_PTR(-ENOMEM);
	}

	INIT_LIST_HEAD(&buffer->attachments);
	mutex_init(&buffer->lock);
	buffer->heap = heap;
	buffer->len = len;
	buffer->uncached = true;/* all secure memory set as uncached buffer */

	ret = trusted_mem_api_alloc(tmem_type, 0, len, &refcount,
				    &sec_handle,
				    (uint8_t *)dma_heap_get_name(heap),
				    0);

	if (ret == -ENOMEM) {
		pr_info("%s security out of memory, heap:%s\n",
			__func__, dma_heap_get_name(heap));
	}

	if (sec_handle <= 0) {
		pr_info("%s alloc security memory failed, total size %zu\n",
			__func__, sec_heap_total_memory);
		//TODO: should dump used memory here
		ret = -ENOMEM;
		goto free_buffer_struct;
	}

	table = &buffer->sg_table;

	/* secure memory doesn't have page struct
	 * alloc one node to record secure handle
	 */
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret) {
		//free buffer
		pr_info("%s#%d Error. Allocate mem failed.\n",
			__func__, __LINE__);
		goto free_buffer;
	}
	sg_set_page(table->sgl, 0, 0, 0);

	/* create the dmabuf */
	exp_info.exp_name = dma_heap_get_name(heap);
	exp_info.ops = heap_buf_ops;
	exp_info.size = buffer->len;
	exp_info.flags = fd_flags;
	exp_info.priv = buffer;
	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		pr_info("%s dma_buf_export fail\n", __func__);
		goto free_buffer;
	}

	/* add debug info */
	buffer->priv = info;
	mutex_init(&info->lock);
	/* add alloc pid & tid info*/
	get_task_comm(info->pid_name, task);
	get_task_comm(info->tid_name, current);
	info->pid = task_pid_nr(task);
	info->tid = task_pid_nr(current);

	/* store seucre handle */
	info->sec_handle = sec_handle;

	sec_heap_total_memory += len;
	pr_debug("[%s][%d] %s: dmabuf:%p, sec_handle 0x%lx, size 0x%lx\n",
		 dma_heap_get_name(heap), tmem_type, __func__,
		 dmabuf,
		 sg_dma_address(buffer->sg_table.sgl),
		 buffer->len);
	return dmabuf;
free_buffer:
	//free secure handle
	trusted_mem_api_unref(tmem_type, sec_handle,
			      (uint8_t *)dma_heap_get_name(buffer->heap), 0);
free_buffer_struct:
	sg_free_table(table);
	kfree(info);
	kfree(buffer);
	return ERR_PTR(ret);
}

static inline struct dma_buf *svp_region_allocate(struct dma_heap *heap,
					   unsigned long len,
					   unsigned long fd_flags,
					   unsigned long heap_flags)
{
	enum TRUSTED_MEM_REQ_TYPE tmem_type = TRUSTED_MEM_REQ_SVP_REGION;

	return tmem_allocate(tmem_type, &svp_region_heap_buf_ops,
			     heap, len, fd_flags, heap_flags);
}

static const struct dma_heap_ops svp_region_heap_ops = {
	.allocate = svp_region_allocate,
};

static inline struct dma_buf *svp_page_allocate(struct dma_heap *heap,
					   unsigned long len,
					   unsigned long fd_flags,
					   unsigned long heap_flags)
{
	enum TRUSTED_MEM_REQ_TYPE tmem_type = TRUSTED_MEM_REQ_SVP_PAGE;

	return tmem_allocate(tmem_type, &svp_page_heap_buf_ops,
			     heap, len, fd_flags, heap_flags);
}

static const struct dma_heap_ops svp_page_heap_ops = {
	.allocate = svp_page_allocate,
};

static inline struct dma_buf *prot_region_allocate(struct dma_heap *heap,
					    unsigned long len,
					    unsigned long fd_flags,
					    unsigned long heap_flags)
{
	enum TRUSTED_MEM_REQ_TYPE tmem_type = TRUSTED_MEM_REQ_PROT_REGION;

	return tmem_allocate(tmem_type, &prot_region_heap_buf_ops,
			     heap, len, fd_flags, heap_flags);
}

static const struct dma_heap_ops prot_region_heap_ops = {
	.allocate = prot_region_allocate,
};

static inline struct dma_buf *prot_page_allocate(struct dma_heap *heap,
					    unsigned long len,
					    unsigned long fd_flags,
					    unsigned long heap_flags)
{
	enum TRUSTED_MEM_REQ_TYPE tmem_type = TRUSTED_MEM_REQ_PROT_PAGE;

	return tmem_allocate(tmem_type, &prot_page_heap_buf_ops,
			     heap, len, fd_flags, heap_flags);
}

static const struct dma_heap_ops prot_page_heap_ops = {
	.allocate = prot_page_allocate,
};

static inline struct dma_buf *wfd_allocate(struct dma_heap *heap,
					    unsigned long len,
					    unsigned long fd_flags,
					    unsigned long heap_flags)
{
	enum TRUSTED_MEM_REQ_TYPE tmem_type = TRUSTED_MEM_REQ_WFD;

	return tmem_allocate(tmem_type, &wfd_heap_buf_ops,
			     heap, len, fd_flags, heap_flags);
}

static const struct dma_heap_ops wfd_heap_ops = {
	.allocate = wfd_allocate,
};

static inline struct dma_buf *prot_2d_fr_allocate(struct dma_heap *heap,
					    unsigned long len,
					    unsigned long fd_flags,
					    unsigned long heap_flags)
{
	enum TRUSTED_MEM_REQ_TYPE tmem_type = TRUSTED_MEM_REQ_2D_FR;

	return tmem_allocate(tmem_type, &prot_2d_fr_heap_buf_ops,
			     heap, len, fd_flags, heap_flags);
}

static const struct dma_heap_ops prot_2d_fr_heap_ops = {
	.allocate = prot_2d_fr_allocate,
};

static inline struct dma_buf *sapu_data_shm_allocate(struct dma_heap *heap,
					    unsigned long len,
					    unsigned long fd_flags,
					    unsigned long heap_flags)
{
	enum TRUSTED_MEM_REQ_TYPE tmem_type = TRUSTED_MEM_REQ_SDSP_SHARED;

	return tmem_allocate(tmem_type, &sapu_data_shm_heap_buf_ops,
			     heap, len, fd_flags, heap_flags);
}

static const struct dma_heap_ops sapu_data_shm_heap_ops = {
	.allocate = sapu_data_shm_allocate,
};

static inline struct dma_buf *sapu_engine_shm_allocate(struct dma_heap *heap,
					    unsigned long len,
					    unsigned long fd_flags,
					    unsigned long heap_flags)
{
	enum TRUSTED_MEM_REQ_TYPE tmem_type = TRUSTED_MEM_REQ_SDSP;

	return tmem_allocate(tmem_type, &sapu_engine_shm_heap_buf_ops,
			     heap, len, fd_flags, heap_flags);
}

static const struct dma_heap_ops sapu_engine_shm_heap_ops = {
	.allocate = sapu_engine_shm_allocate,
};

static int is_mtk_secure_dmabuf(const struct dma_buf *dmabuf) {

	if (!dmabuf)
		return 0;

	return 1;
}

/* no '\n' at end of str */
static char *sec_get_buf_dump_str(const struct dma_buf *dmabuf,
				  const struct dma_heap *heap) {
	struct mtk_sec_heap_buffer *buf = dmabuf->priv;
	struct sec_heap_priv *buf_priv = NULL;
	struct dma_heap *buf_heap = NULL;
	//int i;
	char *info_str;
	int len = 0;

	/* buffer check */
	if (!is_mtk_secure_dmabuf(dmabuf))
		return NULL;

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

static int sec_dump_buf_attach_cb(const struct dma_buf *dmabuf,
				  void *priv) {
	int attach_count = 0;
	struct mtk_heap_dump_t *dump_param = priv;
	struct seq_file *s = dump_param->file;
	struct dma_heap *dump_heap = dump_param->heap;
	struct mtk_sec_heap_buffer *buf;
	struct dma_heap *buf_heap;
	struct mtk_heap_priv_info *heap_priv = NULL;
	struct dma_buf_attachment *attach_obj;

	buf = dmabuf->priv;
	buf_heap = buf->heap;
	heap_priv = mtk_heap_priv_get(buf_heap);

	if (buf_heap != dump_heap)
		return 0;

	dmabuf_dump(s, "\tdmabuf=%p, size:0x%lx, exp_name:%s, dbg_name:%s, file_cnt:%d\n",
		    dmabuf, dmabuf->size,
		    dmabuf->exp_name,
		    dmabuf->name ?: "NULL",
		    file_count(dmabuf->file));
	dmabuf_dump(s, "\t\tDevice \tdma_map_attrs \tdma_data_dir\n");
	list_for_each_entry(attach_obj, &dmabuf->attachments, node) {
		dmabuf_dump(s, "\t\t%s \t%lu \t%d\n",
			    dev_name(attach_obj->dev),
			    attach_obj->dma_map_attrs,
			    attach_obj->dir);
		attach_count++;
	}
	dmabuf_dump(s, "\t--Total %d devices attached.\n\n",
		    attach_count);

	return 0;
}

static int sec_dump_buf_info_cb(const struct dma_buf *dmabuf,
			       void *priv) {
	struct mtk_heap_dump_t *dump_param = priv;
	struct seq_file *s = dump_param->file;
	struct dma_heap *dump_heap = dump_param->heap;
	struct mtk_sec_heap_buffer *buf;
	struct dma_heap *buf_heap;
	struct mtk_heap_priv_info *heap_priv = NULL;
	char *buf_dump_str = NULL;

	buf = dmabuf->priv;
	buf_heap = buf->heap;
	heap_priv = mtk_heap_priv_get(buf_heap);

	if (buf_heap != dump_heap)
		return 0;

	buf_dump_str = heap_priv->get_buf_dump_str(dmabuf, dump_heap);
	dmabuf_dump(s, "%s\n", buf_dump_str);

	kfree(buf_dump_str);

	return 0;
}

static void sec_dmaheap_show(struct dma_heap *heap,
			     void* seq_file) {
	struct seq_file *s = seq_file;
	long pool_sz = -1;
	const char *heap_name = dma_heap_get_name(heap);
	struct mtk_heap_dump_t dump_param;
	struct dma_heap_export_info *exp_info = (typeof(exp_info))heap;
	struct mtk_heap_priv_info *heap_priv = NULL;
	const char * dump_fmt = NULL;

	dump_param.heap = heap;
	dump_param.file = seq_file;

	dmabuf_dump(s, "------[%s] dmabuf heap show START @ %llu ms------\n",
		    heap_name, get_current_time_ms());
	dmabuf_dump(s, "\t------heap_total------\n");
	dmabuf_dump(s, "\tNEED updated\n");

	dmabuf_dump(s, "\t------page_pool show------\n");
	heap_priv = mtk_heap_priv_get(heap);
	if (exp_info->ops->get_pool_size)
		pool_sz = exp_info->ops->get_pool_size(heap);

	dmabuf_dump(s, "\tpool size(Byte): %ld\n", pool_sz);

	//mtk_dmabuf_heap_buffer_dump(s);
	dmabuf_dump(s, "\t------buffer dump start @%llu ms------\n", get_current_time_ms());

	dump_fmt = heap_priv->get_buf_dump_fmt(heap);
	dmabuf_dump(s, "\t%s\n", dump_fmt);
	kfree(dump_fmt);

	get_each_dmabuf(sec_dump_buf_info_cb, &dump_param);

	dmabuf_dump(s, "\tattachment list dump\n");
	get_each_dmabuf(sec_dump_buf_attach_cb, &dump_param);

	dmabuf_dump(s, "------[%s] dmabuf heap show END @ %llu ms------\n",
		    heap_name, get_current_time_ms());

}

static const struct mtk_heap_priv_info mtk_sec_heap_priv = {
	.get_buf_dump_str = sec_get_buf_dump_str,
	.get_buf_dump_fmt = sec_get_buf_dump_fmt,
	.show =             sec_dmaheap_show,
};

static struct sec_feature mtk_sec_heap[__MAX_NR_SECURE_FEATURES] = {
	[SVP_REGION] = {
		.feat_name = "mtk_svp_region-uncached",
		.ops = &svp_region_heap_ops,
		.priv = (void *)&mtk_sec_heap_priv,
	},
	[SVP_PAGE] = {
		.feat_name = "mtk_svp_page-uncached",
		.ops = &svp_region_heap_ops,
		.priv = (void *)&mtk_sec_heap_priv,
	},
	[PROT_REGION] = {
		.feat_name = "mtk_prot_region-uncached",
		.ops = &prot_region_heap_ops,
		.priv = (void *)&mtk_sec_heap_priv,
	},
	[PROT_PAGE] = {
		.feat_name = "mtk_prot_page-uncached",
		.ops = &prot_page_heap_ops,
		.priv = (void *)&mtk_sec_heap_priv,
	},
	[PROT_2D_FR_REGION] = {
		.feat_name = "mtk_2d_fr-uncached",
		.ops = &prot_2d_fr_heap_ops,
		.priv = (void *)&mtk_sec_heap_priv,
	},
	[WFD_REGION] = {
		.feat_name = "mtk_wfd-uncached",
		.ops = &wfd_heap_ops,
		.priv = (void *)&mtk_sec_heap_priv,
	},
	[SAPU_DATA_SHM_REGION] = {
		.feat_name = "mtk_sapu_data_shm-uncached",
		.ops = &sapu_data_shm_heap_ops,
		.priv = (void *)&mtk_sec_heap_priv,
	},
	[SAPU_ENGINE_SHM_REGION] = {
		.feat_name = "mtk_sapu_engine_shm-uncached",
		.ops = &sapu_engine_shm_heap_ops,
		.priv = (void *)&mtk_sec_heap_priv,
	},
};

static int mtk_sec_heap_create(void)
{
	struct dma_heap_export_info exp_info;
	int i;

	/* No need pagepool for secure heap */

	for (i = 0; i < __MAX_NR_SECURE_FEATURES; i++) {
		exp_info.name = mtk_sec_heap[i].feat_name;
		exp_info.ops = mtk_sec_heap[i].ops;
		exp_info.priv = mtk_sec_heap[i].priv;

		mtk_sec_heap[i].heap = dma_heap_add(&exp_info);
		if (IS_ERR(mtk_sec_heap[i].heap))
			return PTR_ERR(mtk_sec_heap[i].heap);
		pr_info("%s add heap[%s] success\n", __func__, exp_info.name);
	}

	return 0;
}

/* return 0 means error */
u32 dmabuf_to_secure_handle (struct dma_buf *dmabuf) {
	struct mtk_sec_heap_buffer *buffer;
	struct sec_heap_priv *buf_info;

	if (!is_mtk_secure_dmabuf(dmabuf))
		return 0;

	buffer = dmabuf->priv;
	buf_info = buffer->priv;

	return buf_info->sec_handle;
}
EXPORT_SYMBOL_GPL(dmabuf_to_secure_handle);

module_init(mtk_sec_heap_create);
MODULE_LICENSE("GPL v2");
