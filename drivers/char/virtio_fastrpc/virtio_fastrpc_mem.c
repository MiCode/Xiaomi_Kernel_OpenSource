// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "virtio_fastrpc_mem.h"

#define MAX_CACHE_BUF_SIZE		(8*1024*1024)
/* Maximum buffers cached in cached buffer list */
#define MAX_CACHED_BUFS		32

static inline void vfastrpc_free_pages(struct page **pages, int count)
{
	while (count--)
		__free_page(pages[count]);
	kvfree(pages);
}

static struct page **vfastrpc_alloc_pages(struct device *dev, unsigned int count, gfp_t gfp)
{
	struct page **pages;
	unsigned long order_mask = (2U << MAX_ORDER) - 1;
	unsigned int i = 0, nid = dev_to_node(dev);

	pages = kvzalloc(count * sizeof(*pages), GFP_KERNEL);
	if (!pages)
		return NULL;

	/* IOMMU can map any pages, so himem can also be used here */
	gfp |= __GFP_NOWARN | __GFP_HIGHMEM;
	gfp &= ~__GFP_COMP;

	while (count) {
		struct page *page = NULL;
		unsigned int order_size;

		/*
		 * Higher-order allocations are a convenience rather
		 * than a necessity, hence using __GFP_NORETRY until
		 * falling back to minimum-order allocations.
		 */
		for (order_mask &= (2U << __fls(count)) - 1;
		     order_mask; order_mask &= ~order_size) {
			unsigned int order = __fls(order_mask);
			gfp_t alloc_flags = gfp;

			order_size = 1U << order;
			if (order_mask > order_size)
				alloc_flags |= __GFP_NORETRY;

			page = alloc_pages_node(nid, alloc_flags, order);
			if (!page)
				continue;
			if (order)
				split_page(page, order);
			break;
		}
		if (!page) {
			vfastrpc_free_pages(pages, i);
			return NULL;
		}
		count -= order_size;
		while (order_size--)
			pages[i++] = page++;
	}
	return pages;
}

static struct page **vfastrpc_alloc_buffer(struct device *dev, struct vfastrpc_buf *buf,
		gfp_t gfp, pgprot_t prot)
{
	struct page **pages;
	unsigned int count = PAGE_ALIGN(buf->size) >> PAGE_SHIFT;

	pages = vfastrpc_alloc_pages(dev, count, gfp);
	if (!pages)
		return NULL;

	if (sg_alloc_table_from_pages(&buf->sgt, pages, count, 0,
				buf->size, GFP_KERNEL))
		goto out_free_pages;

	if (!(buf->dma_attr & DMA_ATTR_NO_KERNEL_MAPPING)) {
		buf->va = vmap(pages, count, VM_MAP, prot);
		if (!buf->va)
			goto out_free_sg;
	}
	return pages;

out_free_sg:
	sg_free_table(&buf->sgt);
out_free_pages:
	vfastrpc_free_pages(pages, count);
	return NULL;
}

static inline void vfastrpc_free_buffer(struct vfastrpc_buf *buf)
{
	unsigned int count = PAGE_ALIGN(buf->size) >> PAGE_SHIFT;

	vunmap(buf->va);
	sg_free_table(&buf->sgt);
	vfastrpc_free_pages(buf->pages, count);
}

void vfastrpc_buf_free(struct vfastrpc_buf *buf, int cache)
{
	struct vfastrpc_file *vfl = buf == NULL ? NULL : buf->vfl;
	struct fastrpc_file *fl = vfl == NULL ? NULL : to_fastrpc_file(vfl);

	if (!vfl || !fl)
		return;

	if (cache && buf->size < MAX_CACHE_BUF_SIZE) {
		spin_lock(&fl->hlock);
		if (fl->num_cached_buf > MAX_CACHED_BUFS) {
			spin_unlock(&fl->hlock);
			dev_dbg(vfl->apps->dev, "num_cached_buf reaches upper limit\n");
			goto skip_buf_cache;
		}
		hlist_add_head(&buf->hn, &fl->cached_bufs);
		fl->num_cached_buf++;
		dev_dbg(vfl->apps->dev, "%d buf is cached, size = 0x%lx",
				fl->num_cached_buf, buf->size);
		spin_unlock(&fl->hlock);
		return;
	}

skip_buf_cache:
	if (buf->remote) {
		spin_lock(&fl->hlock);
		hlist_del_init(&buf->hn_rem);
		spin_unlock(&fl->hlock);
		buf->remote = 0;
		buf->raddr = 0;
	}

	if (!IS_ERR_OR_NULL(buf->pages))
		vfastrpc_free_buffer(buf);
	kfree(buf);
}

int vfastrpc_buf_alloc(struct vfastrpc_file *vfl, size_t size,
				unsigned long dma_attr, uint32_t rflags,
				int remote, pgprot_t prot, struct vfastrpc_buf **obuf)
{
	struct vfastrpc_apps *me = vfl->apps;
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct vfastrpc_buf *buf = NULL, *fr = NULL;
	struct hlist_node *n;
	int err = 0;

	VERIFY(err, size > 0);
	if (err)
		goto bail;

	if (!remote) {
		/* find the smallest buffer that fits in the cache */
		spin_lock(&fl->hlock);
		hlist_for_each_entry_safe(buf, n, &fl->cached_bufs, hn) {
			if (buf->size >= size && (!fr || fr->size > buf->size))
				fr = buf;
		}
		if (fr) {
			hlist_del_init(&fr->hn);
			fl->num_cached_buf--;
		}
		spin_unlock(&fl->hlock);
		if (fr) {
			*obuf = fr;
			return 0;
		}
	}

	VERIFY(err, NULL != (buf = kzalloc(sizeof(*buf), GFP_KERNEL)));
	if (err)
		goto bail;
	buf->vfl = vfl;
	buf->size = size;
	buf->va = NULL;
	buf->dma_attr = dma_attr;
	buf->map_attr = 0;
	buf->flags = rflags;
	buf->raddr = 0;
	buf->remote = 0;
	buf->pages = vfastrpc_alloc_buffer(me->dev, buf, GFP_KERNEL, prot);
	if (IS_ERR_OR_NULL(buf->pages)) {
		err = -ENOMEM;
		dev_err(me->dev,
			"%s: %s: fastrpc_alloc_buffer failed for size 0x%zx, returned %ld\n",
			current->comm, __func__, size, PTR_ERR(buf->pages));
		goto bail;
	}

	if (remote) {
		INIT_HLIST_NODE(&buf->hn_rem);
		spin_lock(&fl->hlock);
		hlist_add_head(&buf->hn_rem, &fl->remote_bufs);
		spin_unlock(&fl->hlock);
		buf->remote = remote;
	}

	*obuf = buf;
 bail:
	if (err && buf)
		vfastrpc_buf_free(buf, 0);
	return err;
}

void vfastrpc_mmap_add(struct vfastrpc_file *vfl, struct vfastrpc_mmap *map)
{
	if (map->flags == ADSP_MMAP_HEAP_ADDR ||
				map->flags == ADSP_MMAP_REMOTE_HEAP_ADDR) {
		struct vfastrpc_apps *me = vfl->apps;

		dev_err(me->dev, "%s ADSP_MMAP_HEAP_ADDR is not supported\n",
				__func__);
	} else {
		struct vfastrpc_file *vfl = map->vfl;
		struct fastrpc_file *fl = to_fastrpc_file(vfl);

		hlist_add_head(&map->hn, &fl->maps);
	}
}

int vfastrpc_mmap_remove(struct vfastrpc_file *vfl, int fd,
		uintptr_t va, size_t len, struct vfastrpc_mmap **ppmap)
{
	struct vfastrpc_mmap *match = NULL, *map;
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct hlist_node *n;

	hlist_for_each_entry_safe(map, n, &fl->maps, hn) {
		if ((fd < 0 || map->fd == fd) && map->raddr == va &&
				map->raddr + map->len == va + len &&
				(map->refs == 1 ||
				 (map->refs == 2 &&
				  map->attr & FASTRPC_ATTR_KEEP_MAP))) {
			if (map->attr & FASTRPC_ATTR_KEEP_MAP)
				map->refs--;
			match = map;
			hlist_del_init(&map->hn);
			break;
		}
	}
	if (match) {
		*ppmap = match;
		return 0;
	}
	return -ETOOMANYREFS;
}

int vfastrpc_mmap_remove_fd(struct vfastrpc_file *vfl, int fd, u32 *entries)
{
	struct vfastrpc_mmap *match = NULL, *map = NULL;
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct hlist_node *n;
	int err = 0;

	*entries = 0;
	hlist_for_each_entry_safe(map, n, &fl->maps, hn) {
		if ((map->fd == fd) &&
				(map->attr & FASTRPC_ATTR_KEEP_MAP)) {
			(*entries)++;
			match = map;
			if (match->refs > 1) {
				dev_err(vfl->apps->dev,
						"%s map refs = %d is abnormal\n",
						__func__, match->refs);
				err = -ETOOMANYREFS;
			}
			vfastrpc_mmap_free(vfl, match, 0);
		}
	}
	return err;
}

void vfastrpc_mmap_free(struct vfastrpc_file *vfl,
		struct vfastrpc_mmap *map, uint32_t force_free)
{
	struct vfastrpc_apps *me = vfl->apps;

	if (!map)
		return;

	if (map->flags == ADSP_MMAP_HEAP_ADDR ||
				map->flags == ADSP_MMAP_REMOTE_HEAP_ADDR) {
		dev_err(me->dev, "%s ADSP_MMAP_HEAP_ADDR is not supported\n",
				__func__);
	} else {
		if (map->refs <= 0) {
			dev_warn(me->dev, "map refcnt = %d is abnormal\n", map->refs);
			return;
		}

		map->refs--;
		if (map->refs && force_free) {
			dev_warn(me->dev, "force free map, but refs = %d\n", map->refs);
			map->refs = 0;
		}

		if (!map->refs) {
			hlist_del_init(&map->hn);
			if (!IS_ERR_OR_NULL(map->table)) {
				dma_buf_unmap_attachment(map->attach, map->table,
						DMA_BIDIRECTIONAL);
				map->table = NULL;
			}

			if (!IS_ERR_OR_NULL(map->attach)) {
				dma_buf_detach(map->buf, map->attach);
				map->attach = NULL;
			}

			if (!IS_ERR_OR_NULL(map->buf)) {
				dma_buf_put(map->buf);
				map->buf = NULL;
			}
			kfree(map);
		}
	}
}

int vfastrpc_mmap_find(struct vfastrpc_file *vfl, int fd,
		uintptr_t va, size_t len, int mflags, int refs,
		struct vfastrpc_mmap **ppmap)
{
	struct vfastrpc_apps *me = vfl->apps;
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct vfastrpc_mmap *match = NULL, *map = NULL;
	struct hlist_node *n;

	if ((va + len) < va)
		return -EOVERFLOW;
	if (mflags == ADSP_MMAP_HEAP_ADDR ||
				 mflags == ADSP_MMAP_REMOTE_HEAP_ADDR) {
		dev_err(me->dev, "%s ADSP_MMAP_HEAP_ADDR is not supported\n",
				__func__);
	} else {
		hlist_for_each_entry_safe(map, n, &fl->maps, hn) {
			if (va >= map->va &&
				va + len <= map->va + map->len &&
				map->fd == fd) {
				if (refs) {
					if (map->refs + 1 == INT_MAX)
						return -ETOOMANYREFS;
					map->refs++;
				}
				match = map;
				break;
			}
		}
	}
	if (match) {
		*ppmap = match;
		return 0;
	}
	return -ENOTTY;
}

int vfastrpc_mmap_create(struct vfastrpc_file *vfl, int fd,
	unsigned int attr, uintptr_t va, size_t len, int mflags,
	struct vfastrpc_mmap **ppmap)
{
	struct vfastrpc_apps *me = vfl->apps;
	struct vfastrpc_mmap *map = NULL;
	int err = 0, sgl_index = 0;
	struct scatterlist *sgl = NULL;

	if (!vfastrpc_mmap_find(vfl, fd, va, len, mflags, 1, ppmap))
		return 0;

	map = kzalloc(sizeof(*map), GFP_KERNEL);
	VERIFY(err, !IS_ERR_OR_NULL(map));
	if (err)
		goto bail;

	INIT_HLIST_NODE(&map->hn);
	map->flags = mflags;
	map->refs = 1;
	map->vfl = vfl;
	map->fd = fd;
	map->attr = attr;
	if (mflags == ADSP_MMAP_HEAP_ADDR ||
			mflags == ADSP_MMAP_REMOTE_HEAP_ADDR) {
		dev_err(me->dev, "%s ADSP_MMAP_HEAP_ADDR is not supported\n",
				__func__);
		err = -EINVAL;
		goto bail;
	} else {
		if (map->attr && (map->attr & FASTRPC_ATTR_KEEP_MAP)) {
			map->refs = 2;
			dev_dbg(me->dev, "KEE_MAP is set for fd = %d\n", map->fd);
		}

		VERIFY(err, !IS_ERR_OR_NULL(map->buf = dma_buf_get(fd)));
		if (err) {
			dev_err(me->dev, "can't get dma buf fd %d\n", fd);
			goto bail;
		}

		VERIFY(err, !IS_ERR_OR_NULL(map->attach =
					dma_buf_attach(map->buf, me->dev)));
		if (err) {
			dev_err(me->dev, "can't attach dma buf\n");
			goto bail;
		}

		/*
		 * no need to sync cache even for cached buffers, depending on
		 * IO coherency
		 */
		map->attach->dma_map_attrs |= DMA_ATTR_SKIP_CPU_SYNC;
		VERIFY(err, !IS_ERR_OR_NULL(map->table =
					dma_buf_map_attachment(map->attach,
					DMA_BIDIRECTIONAL)));
		if (err) {
			dev_err(me->dev, "can't get sg table of dma buf\n");
			goto bail;
		}
		map->phys = sg_dma_address(map->table->sgl);
		for_each_sg(map->table->sgl, sgl, map->table->nents, sgl_index)
			map->size += sg_dma_len(sgl);
		map->va = va;
	}

	map->len = len;
	vfastrpc_mmap_add(vfl, map);
	*ppmap = map;
bail:
	if (err && map)
		vfastrpc_mmap_free(vfl, map, 0);
	return err;
}
