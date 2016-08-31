/*
 * dma_buf exporter for nvmap
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define pr_fmt(fmt)	"nvmap: %s() " fmt, __func__

#include <linux/fdtable.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/export.h>
#include <linux/nvmap.h>
#include <linux/dma-buf.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/stringify.h>

#include <trace/events/nvmap.h>

#include "nvmap_priv.h"
#include "nvmap_ioctl.h"

#ifdef CONFIG_IOMMU_API
#define nvmap_masid_mapping(attach)   to_dma_iommu_mapping((attach)->dev)
#else
#define nvmap_masid_mapping(attach)   NULL
#endif

struct nvmap_handle_info {
	struct nvmap_handle *handle;
	struct list_head maps;
	struct mutex maps_lock;
};

/**
 * List node for maps of nvmap handles via the dma_buf API. These store the
 * necessary info for stashing mappings.
 *
 * @mapping Mapping for which this SGT is valid - for supporting multi-asid.
 * @dir DMA direction.
 * @sgt The scatter gather table to stash.
 * @refs Reference counting.
 * @maps_entry Entry on a given attachment's list of maps.
 * @stash_entry Entry on the stash list.
 * @owner The owner of this struct. There can be only one.
 */
struct nvmap_handle_sgt {
	struct dma_iommu_mapping *mapping;
	enum dma_data_direction dir;
	struct sg_table *sgt;
	struct device *dev;

	atomic_t refs;

	struct list_head maps_entry;
	struct list_head stash_entry; /* lock the stash before accessing. */

	struct nvmap_handle_info *owner;
} ____cacheline_aligned_in_smp;

static DEFINE_MUTEX(nvmap_stashed_maps_lock);
static LIST_HEAD(nvmap_stashed_maps);
static struct kmem_cache *handle_sgt_cache;

/*
 * Initialize a kmem cache for allocating nvmap_handle_sgt's.
 */
int nvmap_dmabuf_stash_init(void)
{
	handle_sgt_cache = KMEM_CACHE(nvmap_handle_sgt, 0);
	if (IS_ERR_OR_NULL(handle_sgt_cache)) {
		pr_err("Failed to make kmem cache for nvmap_handle_sgt.\n");
		return -ENOMEM;
	}

	return 0;
}

#ifdef CONFIG_NVMAP_DMABUF_STASH_STATS
struct nvmap_stash_stats {
	unsigned long long hits;
	unsigned long long all_hits;
	unsigned long long misses;
	unsigned long long evictions;

	unsigned long long stashed_iova;
	unsigned long long stashed_maps;
};

static DEFINE_SPINLOCK(nvmap_stat_lock);
static struct nvmap_stash_stats nvmap_stash_stats;

#define stash_stat_inc(var)			\
	do {					\
		spin_lock(&nvmap_stat_lock);	\
		nvmap_stash_stats.var += 1;	\
		spin_unlock(&nvmap_stat_lock);	\
	} while (0)
#define stash_stat_dec(var)			\
	do {					\
		spin_lock(&nvmap_stat_lock);	\
		nvmap_stash_stats.var -= 1;	\
		spin_unlock(&nvmap_stat_lock);	\
	} while (0)
#define stash_stat_add_iova(handle)					\
	do {								\
		spin_lock(&nvmap_stat_lock);				\
		nvmap_stash_stats.stashed_iova += (handle)->size;	\
		spin_unlock(&nvmap_stat_lock);				\
	} while (0)
#define stash_stat_sub_iova(handle)					\
	do {								\
		spin_lock(&nvmap_stat_lock);				\
		nvmap_stash_stats.stashed_iova -= (handle)->size;	\
		spin_unlock(&nvmap_stat_lock);				\
	} while (0)
#else
#define stash_stat_inc(var)
#define stash_stat_dec(var)
#define stash_stat_add_iova(handle)
#define stash_stat_sub_iova(handle)
#endif

static int nvmap_dmabuf_attach(struct dma_buf *dmabuf, struct device *dev,
			       struct dma_buf_attachment *attach)
{
	struct nvmap_handle_info *info = dmabuf->priv;

	trace_nvmap_dmabuf_attach(dmabuf, dev);

	dev_dbg(dev, "%s() 0x%p\n", __func__, info->handle);
	return 0;
}

static void nvmap_dmabuf_detach(struct dma_buf *dmabuf,
				struct dma_buf_attachment *attach)
{
	struct nvmap_handle_info *info = dmabuf->priv;

	trace_nvmap_dmabuf_detach(dmabuf, attach->dev);

	dev_dbg(attach->dev, "%s() 0x%p\n", __func__, info->handle);
}

/*
 * Add this sgt to the stash - should be called when the SGT's ref count hits
 * 0.
 */
static void __nvmap_dmabuf_add_stash(struct nvmap_handle_sgt *nvmap_sgt)
{
	pr_debug("Adding mapping to stash.\n");
	mutex_lock(&nvmap_stashed_maps_lock);
	list_add(&nvmap_sgt->stash_entry, &nvmap_stashed_maps);
	mutex_unlock(&nvmap_stashed_maps_lock);
	stash_stat_inc(stashed_maps);
	stash_stat_add_iova(nvmap_sgt->owner->handle);
}

/*
 * Make sure this mapping is no longer stashed - this corresponds to a "hit". If
 * the mapping is not stashed this is just a no-op.
 */
static void __nvmap_dmabuf_del_stash(struct nvmap_handle_sgt *nvmap_sgt)
{
	mutex_lock(&nvmap_stashed_maps_lock);
	if (list_empty(&nvmap_sgt->stash_entry)) {
		mutex_unlock(&nvmap_stashed_maps_lock);
		return;
	}

	pr_debug("Removing map from stash.\n");
	list_del_init(&nvmap_sgt->stash_entry);
	mutex_unlock(&nvmap_stashed_maps_lock);
	stash_stat_inc(hits);
	stash_stat_dec(stashed_maps);
	stash_stat_sub_iova(nvmap_sgt->owner->handle);
}

/*
 * Free an sgt completely. This will bypass the ref count. This also requires
 * the nvmap_sgt's owner's lock is already taken.
 */
static void __nvmap_dmabuf_free_sgt_locked(struct nvmap_handle_sgt *nvmap_sgt)
{
	struct nvmap_handle_info *info = nvmap_sgt->owner;
	DEFINE_DMA_ATTRS(attrs);

	list_del(&nvmap_sgt->maps_entry);

	if (info->handle->heap_pgalloc) {
		dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);
		dma_unmap_sg_attrs(nvmap_sgt->dev,
				   nvmap_sgt->sgt->sgl, nvmap_sgt->sgt->nents,
				   nvmap_sgt->dir, &attrs);
	}
	__nvmap_free_sg_table(NULL, info->handle, nvmap_sgt->sgt);

	WARN(atomic_read(&nvmap_sgt->refs), "nvmap: Freeing reffed SGT!");
	kmem_cache_free(handle_sgt_cache, nvmap_sgt);
}

/*
 * Evict an entry from the IOVA stash. This does not do anything to the actual
 * mapping itself - this merely takes the passed nvmap_sgt out of the stash
 * and decrements the necessary cache stats.
 */
void __nvmap_dmabuf_evict_stash_locked(struct nvmap_handle_sgt *nvmap_sgt)
{
	if (!list_empty(&nvmap_sgt->stash_entry))
		list_del_init(&nvmap_sgt->stash_entry);

	stash_stat_dec(stashed_maps);
	stash_stat_sub_iova(nvmap_sgt->owner->handle);
}

/*
 * Locks the stash before doing the eviction.
 */
void __nvmap_dmabuf_evict_stash(struct nvmap_handle_sgt *nvmap_sgt)
{
	mutex_lock(&nvmap_stashed_maps_lock);
	__nvmap_dmabuf_evict_stash_locked(nvmap_sgt);
	mutex_unlock(&nvmap_stashed_maps_lock);
}

/*
 * Prepare an SGT for potential stashing later on.
 */
static int __nvmap_dmabuf_prep_sgt_locked(struct dma_buf_attachment *attach,
				   enum dma_data_direction dir,
				   struct sg_table *sgt)
{
	struct nvmap_handle_sgt *nvmap_sgt;
	struct nvmap_handle_info *info = attach->dmabuf->priv;

	pr_debug("Prepping SGT.\n");
	nvmap_sgt = kmem_cache_alloc(handle_sgt_cache, GFP_KERNEL);
	if (IS_ERR_OR_NULL(nvmap_sgt)) {
		pr_err("Prepping SGT failed.\n");
		return -ENOMEM;
	}

	nvmap_sgt->mapping = nvmap_masid_mapping(attach);
	nvmap_sgt->dir = dir;
	nvmap_sgt->sgt = sgt;
	nvmap_sgt->dev = attach->dev;
	nvmap_sgt->owner = info;
	INIT_LIST_HEAD(&nvmap_sgt->stash_entry);
	atomic_set(&nvmap_sgt->refs, 1);
	list_add(&nvmap_sgt->maps_entry, &info->maps);
	return 0;
}

/*
 * Called when an SGT is no longer being used by a device. This will not
 * necessarily free the SGT - instead it may stash the SGT.
 */
static void __nvmap_dmabuf_stash_sgt_locked(struct dma_buf_attachment *attach,
				    enum dma_data_direction dir,
				    struct sg_table *sgt)
{
	struct nvmap_handle_sgt *nvmap_sgt;
	struct nvmap_handle_info *info = attach->dmabuf->priv;

	pr_debug("Stashing SGT - if necessary.\n");
	list_for_each_entry(nvmap_sgt, &info->maps, maps_entry) {
		if (nvmap_sgt->sgt == sgt) {
			if (!atomic_sub_and_test(1, &nvmap_sgt->refs))
				goto done;

			/*
			 * If we get here, the ref count is zero. Stash the
			 * mapping.
			 */
#ifdef CONFIG_NVMAP_DMABUF_STASH
			__nvmap_dmabuf_add_stash(nvmap_sgt);
#else
			__nvmap_dmabuf_free_sgt_locked(nvmap_sgt);
#endif
			goto done;
		}
	}

done:
	return;
}

/*
 * Checks if there is already a map for this attachment. If so increment the
 * ref count on said map and return the associated sg_table. Otherwise return
 * NULL.
 *
 * If it turns out there is a map, this also checks to see if the map needs to
 * be removed from the stash - if so, the map is removed.
 */
static struct sg_table *__nvmap_dmabuf_get_sgt_locked(
	struct dma_buf_attachment *attach, enum dma_data_direction dir)
{
	struct nvmap_handle_sgt *nvmap_sgt;
	struct sg_table *sgt = NULL;
	struct nvmap_handle_info *info = attach->dmabuf->priv;

	pr_debug("Getting SGT from stash.\n");
	list_for_each_entry(nvmap_sgt, &info->maps, maps_entry) {
		if (nvmap_masid_mapping(attach) != nvmap_sgt->mapping)
			continue;

		/* We have a hit. */
		pr_debug("Stash hit (%s)!\n", dev_name(attach->dev));
		sgt = nvmap_sgt->sgt;
		atomic_inc(&nvmap_sgt->refs);
		__nvmap_dmabuf_del_stash(nvmap_sgt);
		stash_stat_inc(all_hits);
		break;
	}

	if (!sgt)
		stash_stat_inc(misses);
	return sgt;
}

/*
 * If stashing is disabled then the stash related ops become no-ops.
 */
static struct sg_table *nvmap_dmabuf_map_dma_buf(
	struct dma_buf_attachment *attach, enum dma_data_direction dir)
{
	struct nvmap_handle_info *info = attach->dmabuf->priv;
	int err, ents;
	struct sg_table *sgt;
	DEFINE_DMA_ATTRS(attrs);

	trace_nvmap_dmabuf_map_dma_buf(attach->dmabuf, attach->dev);

	mutex_lock(&info->maps_lock);
	atomic_inc(&info->handle->pin);
	sgt = __nvmap_dmabuf_get_sgt_locked(attach, dir);
	if (sgt)
		goto cache_hit;

	sgt = __nvmap_sg_table(NULL, info->handle);
	if (IS_ERR(sgt)) {
		atomic_dec(&info->handle->pin);
		mutex_unlock(&info->maps_lock);
		return sgt;
	}

	if (info->handle->heap_pgalloc && info->handle->alloc) {
		dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);
		ents = dma_map_sg_attrs(attach->dev, sgt->sgl,
					sgt->nents, dir, &attrs);
		if (ents <= 0) {
			err = -ENOMEM;
			goto err_map;
		}
		BUG_ON(ents != 1);
	} else if (info->handle->alloc) {
		/* carveout has linear map setup. */
		mutex_lock(&info->handle->lock);
		sg_dma_address(sgt->sgl) = info->handle->carveout->base;
		mutex_unlock(&info->handle->lock);
	} else {
		goto err_map;
	}

	if (__nvmap_dmabuf_prep_sgt_locked(attach, dir, sgt)) {
		WARN(1, "No mem to prep sgt.\n");
		err = -ENOMEM;
		goto err_prep;
	}

cache_hit:
#ifdef CONFIG_NVMAP_DMABUF_STASH
	BUG_ON(attach->priv && attach->priv != sgt);
#endif
	attach->priv = sgt;
	mutex_unlock(&info->maps_lock);
	return sgt;

err_prep:
	dma_unmap_sg_attrs(attach->dev, sgt->sgl, sgt->nents, dir, &attrs);
err_map:
	__nvmap_free_sg_table(NULL, info->handle, sgt);
	atomic_dec(&info->handle->pin);
	mutex_unlock(&info->maps_lock);
	return ERR_PTR(err);
}

static void nvmap_dmabuf_unmap_dma_buf(struct dma_buf_attachment *attach,
				       struct sg_table *sgt,
				       enum dma_data_direction dir)
{
	struct nvmap_handle_info *info = attach->dmabuf->priv;

	trace_nvmap_dmabuf_unmap_dma_buf(attach->dmabuf, attach->dev);

	mutex_lock(&info->maps_lock);
	if (!atomic_add_unless(&info->handle->pin, -1, 0)) {
		mutex_unlock(&info->maps_lock);
		WARN(1, "Unpinning handle that has yet to be pinned!\n");
		return;
	}
	__nvmap_dmabuf_stash_sgt_locked(attach, dir, sgt);
	mutex_unlock(&info->maps_lock);
}

static void nvmap_dmabuf_release(struct dma_buf *dmabuf)
{
	struct nvmap_handle_info *info = dmabuf->priv;
	struct nvmap_handle_sgt *nvmap_sgt;

	trace_nvmap_dmabuf_release(info->handle->owner ?
				   info->handle->owner->name : "unknown",
				   info->handle,
				   dmabuf);

	mutex_lock(&info->maps_lock);
	while (!list_empty(&info->maps)) {
		nvmap_sgt = list_first_entry(&info->maps,
					     struct nvmap_handle_sgt,
					     maps_entry);
		__nvmap_dmabuf_evict_stash(nvmap_sgt);
		__nvmap_dmabuf_free_sgt_locked(nvmap_sgt);
	}
	mutex_unlock(&info->maps_lock);

	dma_buf_detach(info->handle->dmabuf, info->handle->attachment);
	info->handle->dmabuf = NULL;
	nvmap_handle_put(info->handle);
	kfree(info);
}

static int nvmap_dmabuf_begin_cpu_access(struct dma_buf *dmabuf,
					  size_t start, size_t len,
					  enum dma_data_direction dir)
{
	struct nvmap_handle_info *info = dmabuf->priv;

	trace_nvmap_dmabuf_begin_cpu_access(dmabuf, start, len);
	return __nvmap_cache_maint(NULL, info->handle, start, start + len,
				   NVMAP_CACHE_OP_INV, 1);
}

static void nvmap_dmabuf_end_cpu_access(struct dma_buf *dmabuf,
					size_t start, size_t len,
					enum dma_data_direction dir)
{
	struct nvmap_handle_info *info = dmabuf->priv;

	trace_nvmap_dmabuf_end_cpu_access(dmabuf, start, len);
	__nvmap_cache_maint(NULL, info->handle, start, start + len,
				   NVMAP_CACHE_OP_WB_INV, 1);

}

static void *nvmap_dmabuf_kmap(struct dma_buf *dmabuf, unsigned long page_num)
{
	struct nvmap_handle_info *info = dmabuf->priv;

	trace_nvmap_dmabuf_kmap(dmabuf);
	return __nvmap_kmap(info->handle, page_num);
}

static void nvmap_dmabuf_kunmap(struct dma_buf *dmabuf,
		unsigned long page_num, void *addr)
{
	struct nvmap_handle_info *info = dmabuf->priv;

	trace_nvmap_dmabuf_kunmap(dmabuf);
	__nvmap_kunmap(info->handle, page_num, addr);
}

static void *nvmap_dmabuf_kmap_atomic(struct dma_buf *dmabuf,
				      unsigned long page_num)
{
	WARN(1, "%s() can't be called from atomic\n", __func__);
	return NULL;
}

static int nvmap_dmabuf_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct nvmap_handle_info *info = dmabuf->priv;

	trace_nvmap_dmabuf_mmap(dmabuf);

	return __nvmap_map(info->handle, vma);
}

static void *nvmap_dmabuf_vmap(struct dma_buf *dmabuf)
{
	struct nvmap_handle_info *info = dmabuf->priv;

	trace_nvmap_dmabuf_vmap(dmabuf);
	return __nvmap_mmap(info->handle);
}

static void nvmap_dmabuf_vunmap(struct dma_buf *dmabuf, void *vaddr)
{
	struct nvmap_handle_info *info = dmabuf->priv;

	trace_nvmap_dmabuf_vunmap(dmabuf);
	__nvmap_munmap(info->handle, vaddr);
}

static struct dma_buf_ops nvmap_dma_buf_ops = {
	.attach		= nvmap_dmabuf_attach,
	.detach		= nvmap_dmabuf_detach,
	.map_dma_buf	= nvmap_dmabuf_map_dma_buf,
	.unmap_dma_buf	= nvmap_dmabuf_unmap_dma_buf,
	.release	= nvmap_dmabuf_release,
	.begin_cpu_access = nvmap_dmabuf_begin_cpu_access,
	.end_cpu_access = nvmap_dmabuf_end_cpu_access,
	.kmap_atomic	= nvmap_dmabuf_kmap_atomic,
	.kmap		= nvmap_dmabuf_kmap,
	.kunmap		= nvmap_dmabuf_kunmap,
	.mmap		= nvmap_dmabuf_mmap,
	.vmap		= nvmap_dmabuf_vmap,
	.vunmap		= nvmap_dmabuf_vunmap,
};

/*
 * Make a dmabuf object for an nvmap handle.
 */
struct dma_buf *__nvmap_make_dmabuf(struct nvmap_client *client,
				    struct nvmap_handle *handle)
{
	int err;
	struct dma_buf *dmabuf;
	struct nvmap_handle_info *info;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		err = -ENOMEM;
		goto err_nomem;
	}
	info->handle = handle;
	INIT_LIST_HEAD(&info->maps);
	mutex_init(&info->maps_lock);

	dmabuf = dma_buf_export(info, &nvmap_dma_buf_ops, handle->size,
				O_RDWR);
	if (IS_ERR(dmabuf)) {
		err = PTR_ERR(dmabuf);
		goto err_export;
	}
	nvmap_handle_get(handle);

	trace_nvmap_make_dmabuf(client->name, handle, dmabuf);
	return dmabuf;

err_export:
	kfree(info);
err_nomem:
	return ERR_PTR(err);
}

int __nvmap_dmabuf_fd(struct dma_buf *dmabuf, int flags)
{
	int fd;

	if (!dmabuf || !dmabuf->file)
		return -EINVAL;
	/* Allocate fd from 1024 onwards to overcome
	 * __FD_SETSIZE limitation issue for select(),
	 * pselect() syscalls.
	 */
	fd = __alloc_fd(current->files, 1024,
			sysctl_nr_open, flags);
	if (fd < 0)
		return fd;

	fd_install(fd, dmabuf->file);

	return fd;
}

int nvmap_get_dmabuf_fd(struct nvmap_client *client, ulong id)
{
	int fd;
	struct dma_buf *dmabuf;

	dmabuf = __nvmap_dmabuf_export(client, id);
	if (IS_ERR(dmabuf))
		return PTR_ERR(dmabuf);
	fd = __nvmap_dmabuf_fd(dmabuf, O_CLOEXEC);
	if (fd < 0)
		goto err_out;
	return fd;

err_out:
	dma_buf_put(dmabuf);
	return fd;
}

struct dma_buf *__nvmap_dmabuf_export(struct nvmap_client *client,
				 unsigned long id)
{
	struct nvmap_handle *handle;
	struct dma_buf *buf;

	handle = nvmap_get_handle_id(client, id);
	if (!handle)
		return ERR_PTR(-EINVAL);
	buf = handle->dmabuf;
	if (WARN(!buf, "Attempting to get a freed dma_buf!\n")) {
		nvmap_handle_put(handle);
		return NULL;
	}

	get_dma_buf(buf);

	/*
	 * Don't want to take out refs on the handle here.
	 */
	nvmap_handle_put(handle);

	return handle->dmabuf;
}
EXPORT_SYMBOL(__nvmap_dmabuf_export);

/*
 * Increments ref count on the dma_buf. You are reponsbile for calling
 * dma_buf_put() on the returned dma_buf object.
 */
struct dma_buf *nvmap_dmabuf_export(struct nvmap_client *client,
				 unsigned long user_id)
{
	return __nvmap_dmabuf_export(client, unmarshal_user_id(user_id));
}

/*
 * Similar to nvmap_dmabuf_export() only use a ref to get the buf instead of a
 * user_id. You must dma_buf_put() the dma_buf object when you are done with
 * it.
 */
struct dma_buf *__nvmap_dmabuf_export_from_ref(struct nvmap_handle_ref *ref)
{
	if (!virt_addr_valid(ref))
		return ERR_PTR(-EINVAL);

	get_dma_buf(ref->handle->dmabuf);
	return ref->handle->dmabuf;
}

/*
 * Returns the nvmap handle ID associated with the passed dma_buf's fd. This
 * does not affect the ref count of the dma_buf.
 */
ulong nvmap_get_id_from_dmabuf_fd(struct nvmap_client *client, int fd)
{
	ulong id = -EINVAL;
	struct dma_buf *dmabuf;
	struct nvmap_handle_info *info;

	dmabuf = dma_buf_get(fd);
	if (IS_ERR(dmabuf))
		return PTR_ERR(dmabuf);
	if (dmabuf->ops == &nvmap_dma_buf_ops) {
		info = dmabuf->priv;
		id = (ulong) info->handle;
	}
	dma_buf_put(dmabuf);
	return id;
}

int nvmap_ioctl_share_dmabuf(struct file *filp, void __user *arg)
{
	struct nvmap_create_handle op;
	struct nvmap_client *client = filp->private_data;
	ulong handle;

	BUG_ON(!client);

	if (copy_from_user(&op, (void __user *)arg, sizeof(op)))
		return -EFAULT;

	handle = unmarshal_user_id(op.id);
	if (!handle)
		return -EINVAL;

	op.fd = nvmap_get_dmabuf_fd(client, handle);
	if (op.fd < 0)
		return op.fd;

	if (copy_to_user((void __user *)arg, &op, sizeof(op))) {
		sys_close(op.fd);
		return -EFAULT;
	}
	return 0;
}

int nvmap_get_dmabuf_param(struct dma_buf *dmabuf, u32 param, u64 *result)
{
	struct nvmap_handle_info *info;

	if (WARN_ON(!virt_addr_valid(dmabuf)))
		return -EINVAL;

	info = dmabuf->priv;
	return __nvmap_get_handle_param(NULL, info->handle, param, result);
}

struct sg_table *nvmap_dmabuf_sg_table(struct dma_buf *dmabuf)
{
	struct nvmap_handle_info *info;

	if (WARN_ON(!virt_addr_valid(dmabuf)))
		return ERR_PTR(-EINVAL);

	info = dmabuf->priv;
	return __nvmap_sg_table(NULL, info->handle);
}

void nvmap_dmabuf_free_sg_table(struct dma_buf *dmabuf, struct sg_table *sgt)
{
	if (WARN_ON(!virt_addr_valid(sgt)))
		return;

	__nvmap_free_sg_table(NULL, NULL, sgt);
}

void nvmap_set_dmabuf_private(struct dma_buf *dmabuf, void *priv,
		void (*delete)(void *priv))
{
	struct nvmap_handle_info *info;

	if (WARN_ON(!virt_addr_valid(dmabuf)))
		return;

	info = dmabuf->priv;
	info->handle->nvhost_priv = priv;
	info->handle->nvhost_priv_delete = delete;
}

void *nvmap_get_dmabuf_private(struct dma_buf *dmabuf)
{
	void *priv;
	struct nvmap_handle_info *info;

	if (WARN_ON(!virt_addr_valid(dmabuf)))
		return ERR_PTR(-EINVAL);

	info = dmabuf->priv;
	priv = info->handle->nvhost_priv;
	return priv;
}

/*
 * List detailed info for all buffers allocated.
 */
static int __nvmap_dmabuf_stashes_show(struct seq_file *s, void *data)
{
	struct nvmap_handle_sgt *nvmap_sgt;
	struct nvmap_handle *handle;
	struct nvmap_client *client;
	const char *name;
	phys_addr_t addr;

	mutex_lock(&nvmap_stashed_maps_lock);
	list_for_each_entry(nvmap_sgt, &nvmap_stashed_maps, stash_entry) {
		handle = nvmap_sgt->owner->handle;
		client = nvmap_client_get(handle->owner);
		name = "unknown";

		if (client) {
			if (strcmp(client->name, "user") == 0)
				name = client->task->comm;
			else
				name = client->name;
		}

		seq_printf(s, "%s: ", name);
		seq_printf(s, " flags = 0x%08lx, refs = %d\n",
			   handle->flags, atomic_read(&handle->ref));

		seq_printf(s, "  device = %s\n",
			   dev_name(handle->attachment->dev));
		addr = sg_dma_address(nvmap_sgt->sgt->sgl);
		seq_printf(s, "  IO addr = 0x%pa + 0x%x\n",
			&addr, handle->size);

		/* Cleanup. */
		nvmap_client_put(client);
	}
	mutex_unlock(&nvmap_stashed_maps_lock);

	return 0;
}

static int __nvmap_dmabuf_stashes_open(struct inode *inode,
				       struct file *file)
{
	return single_open(file, __nvmap_dmabuf_stashes_show, NULL);
}

static const struct file_operations nvmap_dmabuf_stashes_fops = {
	.open    = __nvmap_dmabuf_stashes_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

#define NVMAP_DMABUF_WO_TRIGGER_NODE(trigger, name)			\
	DEFINE_SIMPLE_ATTRIBUTE(__nvmap_dmabuf_##name##_fops, NULL,	\
				trigger, "%llu");
#define NVMAP_DMABUF_WO_DEBUGFS(name, root)				\
	do {								\
		if (!debugfs_create_file(__stringify(name), S_IWUSR, root, \
					 NULL, &__nvmap_dmabuf_##name##_fops))\
			return;						\
	} while (0)

#ifdef CONFIG_NVMAP_DMABUF_STASH_STATS

/*
 * Clear the stash stats counting.
 */
static int __nvmap_dmabuf_clear_stash_stats(void *data, u64 val)
{
	spin_lock(&nvmap_stat_lock);
	nvmap_stash_stats.hits = 0;
	nvmap_stash_stats.all_hits = 0;
	nvmap_stash_stats.misses = 0;
	nvmap_stash_stats.evictions = 0;
	spin_unlock(&nvmap_stat_lock);
	return 0;
}
NVMAP_DMABUF_WO_TRIGGER_NODE(__nvmap_dmabuf_clear_stash_stats, clear_stats);
#endif

void nvmap_dmabuf_debugfs_init(struct dentry *nvmap_root)
{
	struct dentry *dmabuf_root;

	if (!nvmap_root)
		return;

	dmabuf_root = debugfs_create_dir("dmabuf", nvmap_root);
	if (!dmabuf_root)
		return;

#if defined(CONFIG_NVMAP_DMABUF_STASH_STATS)
#define CACHE_STAT(root, stat)						\
	do {								\
		if (!debugfs_create_u64(__stringify(stat), S_IRUGO,	\
					root, &nvmap_stash_stats.stat)) \
			return;						\
	} while (0)

	CACHE_STAT(dmabuf_root, hits);
	CACHE_STAT(dmabuf_root, all_hits);
	CACHE_STAT(dmabuf_root, misses);
	CACHE_STAT(dmabuf_root, evictions);
	CACHE_STAT(dmabuf_root, stashed_iova);
	CACHE_STAT(dmabuf_root, stashed_maps);
	NVMAP_DMABUF_WO_DEBUGFS(clear_stats, dmabuf_root);
#endif

#define DMABUF_INFO_FILE(root, file)					\
	do {								\
		if (!debugfs_create_file(__stringify(file), S_IRUGO,	\
					 root, NULL,			\
					 &nvmap_dmabuf_##file##_fops))	\
			return;						\
	} while (0)

	DMABUF_INFO_FILE(dmabuf_root, stashes);
}
