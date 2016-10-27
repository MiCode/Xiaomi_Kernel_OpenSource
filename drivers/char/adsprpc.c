/*
 * Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <linux/pagemap.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/hash.h>
#include <linux/msm_ion.h>
#include <soc/qcom/secure_buffer.h>
#include <soc/qcom/smd.h>
#include <soc/qcom/glink.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/subsystem_restart.h>
#include <linux/scatterlist.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/dma-contiguous.h>
#include <linux/cma.h>
#include <linux/iommu.h>
#include <linux/qcom_iommu.h>
#include <linux/kref.h>
#include <linux/sort.h>
#include <linux/msm_dma_iommu_mapping.h>
#include <asm/dma-iommu.h>
#include <soc/qcom/scm.h>
#include "adsprpc_compat.h"
#include "adsprpc_shared.h"
#include <soc/qcom/ramdump.h>
#include <linux/workqueue.h>
#include <linux/wait.h>


#define TZ_PIL_PROTECT_MEM_SUBSYS_ID 0x0C
#define TZ_PIL_CLEAR_PROTECT_MEM_SUBSYS_ID 0x0D
#define TZ_PIL_AUTH_QDSP6_PROC 1
#define ADSP_MMAP_HEAP_ADDR 4
#define FASTRPC_ENOSUCH 39
#define VMID_SSC_Q6     5

#define RPC_TIMEOUT	(5 * HZ)
#define BALIGN		128
#define NUM_CHANNELS	3		/*1 compute 1 cpz 1 mdsp*/
#define NUM_SESSIONS	8		/*8 compute*/

#define IS_CACHE_ALIGNED(x) (((x) & ((L1_CACHE_BYTES)-1)) == 0)

static void file_free_work_handler(struct work_struct *w);

static DECLARE_WAIT_QUEUE_HEAD(wait_queue);

static inline uint64_t buf_page_start(uint64_t buf)
{
	uint64_t start = (uint64_t) buf & PAGE_MASK;
	return start;
}

static inline uint64_t buf_page_offset(uint64_t buf)
{
	uint64_t offset = (uint64_t) buf & (PAGE_SIZE - 1);
	return offset;
}

static inline int buf_num_pages(uint64_t buf, ssize_t len)
{
	uint64_t start = buf_page_start(buf) >> PAGE_SHIFT;
	uint64_t end = (((uint64_t) buf + len - 1) & PAGE_MASK) >> PAGE_SHIFT;
	int nPages = end - start + 1;
	return nPages;
}

static inline uint64_t buf_page_size(uint32_t size)
{
	uint64_t sz = (size + (PAGE_SIZE - 1)) & PAGE_MASK;
	return sz > PAGE_SIZE ? sz : PAGE_SIZE;
}

static inline void *uint64_to_ptr(uint64_t addr)
{
	void *ptr = (void *)((uintptr_t)addr);
	return ptr;
}

static inline uint64_t ptr_to_uint64(void *ptr)
{
	uint64_t addr = (uint64_t)((uintptr_t)ptr);
	return addr;
}

struct fastrpc_file;

struct fastrpc_fl {
	struct fastrpc_file *fl;
	struct hlist_node hn;
};

struct fastrpc_buf {
	struct hlist_node hn;
	struct fastrpc_file *fl;
	void *virt;
	uint64_t phys;
	ssize_t size;
};

struct fastrpc_ctx_lst;

struct overlap {
	uintptr_t start;
	uintptr_t end;
	int raix;
	uintptr_t mstart;
	uintptr_t mend;
	uintptr_t offset;
};

struct smq_invoke_ctx {
	struct hlist_node hn;
	struct completion work;
	int retval;
	int pid;
	int tgid;
	remote_arg_t *lpra;
	remote_arg64_t *rpra;
	int *fds;
	struct fastrpc_mmap **maps;
	struct fastrpc_buf *buf;
	ssize_t used;
	struct fastrpc_file *fl;
	uint32_t sc;
	struct overlap *overs;
	struct overlap **overps;
	struct smq_msg msg;
};

struct fastrpc_ctx_lst {
	struct hlist_head pending;
	struct hlist_head interrupted;
};

struct fastrpc_smmu {
	struct dma_iommu_mapping *mapping;
	int cb;
	int enabled;
	int faults;
};

struct fastrpc_session_ctx {
	struct device *dev;
	struct fastrpc_smmu smmu;
};

struct fastrpc_channel_ctx {
	char *name;
	char *subsys;
	void *chan;
	struct device *dev;
	struct fastrpc_session_ctx session[NUM_SESSIONS];
	struct completion work;
	struct notifier_block nb;
	struct kref kref;
	unsigned long bitmap;
	int channel;
	int sesscount;
	int ssrcount;
	void *handle;
	int prevssrcount;
	int vmid;
	int ramdumpenabled;
	void *remoteheap_ramdump_dev;
	struct glink_link_info link_info;
	void *link_notify_handle;
	struct glink_open_config cfg;
	char *edge;
};

struct fastrpc_apps {
	struct fastrpc_channel_ctx *channel;
	struct cdev cdev;
	struct class *class;
	struct mutex smd_mutex;
	struct smq_phy_page range;
	struct hlist_head maps;
	struct hlist_head fls;
	struct workqueue_struct *wq;
	int pending_free;
	struct work_struct free_work;
	struct mutex flfree_mutex;
	dev_t dev_no;
	int compat;
	struct hlist_head drivers;
	spinlock_t hlock;
	struct ion_client *client;
	struct device *dev;
	struct device *modem_cma_dev;
	bool glink;
};

struct fastrpc_mmap {
	struct hlist_node hn;
	struct fastrpc_file *fl;
	struct fastrpc_apps *apps;
	int fd;
	uint32_t flags;
	struct dma_buf *buf;
	struct sg_table *table;
	struct dma_buf_attachment *attach;
	struct ion_handle *handle;
	uint64_t phys;
	ssize_t size;
	uintptr_t va;
	ssize_t len;
	int refs;
	uintptr_t raddr;
	int uncached;
};

struct fastrpc_file {
	struct hlist_node hn;
	spinlock_t hlock;
	struct hlist_head maps;
	struct hlist_head bufs;
	struct fastrpc_ctx_lst clst;
	struct fastrpc_session_ctx *sctx;
	uint32_t mode;
	int tgid;
	int cid;
	int ssrcount;
	struct fastrpc_apps *apps;
};

static struct fastrpc_apps gfa;

static struct fastrpc_channel_ctx gcinfo[NUM_CHANNELS] = {
	{
		.name = "adsprpc-smd",
		.subsys = "adsp",
		.channel = SMD_APPS_QDSP,
		.edge = "lpass",
	},
	{
		.name = "sdsprpc-smd",
		.subsys = "dsps",
		.channel = SMD_APPS_DSPS,
		.edge = "dsps",
		.vmid = VMID_SSC_Q6,
	},
	{
		.name = "mdsprpc-smd",
		.subsys = "mdsp",
		.channel = SMD_APPS_MODEM,
		.edge = "mdsp",
	},
};

static void fastrpc_buf_free(struct fastrpc_buf *buf, int cache)
{
	struct fastrpc_file *fl = buf == 0 ? 0 : buf->fl;
	int vmid;

	if (!fl)
		return;
	if (cache) {
		spin_lock(&fl->hlock);
		hlist_add_head(&buf->hn, &fl->bufs);
		spin_unlock(&fl->hlock);
		return;
	}
	if (!IS_ERR_OR_NULL(buf->virt)) {
		int destVM[1] = {VMID_HLOS};
		int destVMperm[1] = {PERM_READ | PERM_WRITE | PERM_EXEC};

		if (fl->sctx->smmu.cb)
			buf->phys &= ~((uint64_t)fl->sctx->smmu.cb << 32);
		vmid = fl->apps->channel[fl->cid].vmid;
		if (vmid) {
			int srcVM[2] = {VMID_HLOS, vmid};

			hyp_assign_phys(buf->phys, buf_page_size(buf->size),
				srcVM, 2, destVM, destVMperm, 1);
		}
		dma_free_coherent(fl->sctx->dev, buf->size, buf->virt,
					buf->phys);
	}
	kfree(buf);
}

static void fastrpc_buf_list_free(struct fastrpc_file *fl)
{
	struct fastrpc_buf *buf, *free;
	do {
		struct hlist_node *n;
		free = 0;
		spin_lock(&fl->hlock);
		hlist_for_each_entry_safe(buf, n, &fl->bufs, hn) {
			hlist_del_init(&buf->hn);
			free = buf;
			break;
		}
		spin_unlock(&fl->hlock);
		if (free)
			fastrpc_buf_free(free, 0);
	} while (free);
}

static void fastrpc_mmap_add(struct fastrpc_mmap *map)
{
	if (map->flags == ADSP_MMAP_HEAP_ADDR) {
		struct fastrpc_apps *me = &gfa;

		spin_lock(&me->hlock);
		hlist_add_head(&map->hn, &me->maps);
		spin_unlock(&me->hlock);
	} else {
		struct fastrpc_file *fl = map->fl;

		spin_lock(&fl->hlock);
		hlist_add_head(&map->hn, &fl->maps);
		spin_unlock(&fl->hlock);
	}
}

static int fastrpc_mmap_find(struct fastrpc_file *fl, int fd, uintptr_t va,
			ssize_t len, int mflags, struct fastrpc_mmap **ppmap)
{
	struct fastrpc_apps *me = &gfa;
	struct fastrpc_mmap *match = 0, *map;
	struct hlist_node *n;
	if (mflags == ADSP_MMAP_HEAP_ADDR) {
		spin_lock(&me->hlock);
		hlist_for_each_entry_safe(map, n, &me->maps, hn) {
			if (va >= map->va &&
				va + len <= map->va + map->len &&
				map->fd == fd) {
				map->refs++;
				match = map;
				break;
			}
		}
		spin_unlock(&me->hlock);
	} else {
		spin_lock(&fl->hlock);
		hlist_for_each_entry_safe(map, n, &fl->maps, hn) {
			if (va >= map->va &&
				va + len <= map->va + map->len &&
				map->fd == fd) {
				map->refs++;
				match = map;
				break;
			}
		}
		spin_unlock(&fl->hlock);
	}
	if (match) {
		*ppmap = match;
		return 0;
	}
	return -ENOTTY;
}

static int dma_alloc_memory(phys_addr_t *region_start, ssize_t size)
{
	struct fastrpc_apps *me = &gfa;
	void *vaddr = 0;
	DEFINE_DMA_ATTRS(attrs);

	if (me->dev == NULL) {
		pr_err("device adsprpc-mem is not initialized\n");
		return -ENODEV;
	}
	dma_set_attr(DMA_ATTR_SKIP_ZEROING, &attrs);
	dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &attrs);
	vaddr = dma_alloc_attrs(me->dev, size, region_start, GFP_KERNEL,
						&attrs);
	if (!vaddr) {
		pr_err("ADSPRPC: Failed to allocate %x remote heap memory\n",
						(unsigned int)size);
		return -ENOMEM;
	}
	return 0;
}

static int fastrpc_mmap_remove(struct fastrpc_file *fl, uintptr_t va,
			       ssize_t len, struct fastrpc_mmap **ppmap)
{
	struct fastrpc_mmap *match = 0, *map;
	struct hlist_node *n;
	struct fastrpc_apps *me = &gfa;

	spin_lock(&me->hlock);
	hlist_for_each_entry_safe(map, n, &me->maps, hn) {
		if (map->raddr == va &&
			map->raddr + map->len == va + len &&
			map->refs == 1) {
			match = map;
			hlist_del_init(&map->hn);
			break;
		}
	}
	spin_unlock(&me->hlock);
	if (match) {
		*ppmap = match;
		return 0;
	}
	spin_lock(&fl->hlock);
	hlist_for_each_entry_safe(map, n, &fl->maps, hn) {
		if (map->raddr == va &&
			map->raddr + map->len == va + len &&
			map->refs == 1) {
			match = map;
			hlist_del_init(&map->hn);
			break;
		}
	}
	spin_unlock(&fl->hlock);
	if (match) {
		*ppmap = match;
		return 0;
	}
	return -ENOTTY;
}

static void fastrpc_mmap_free(struct fastrpc_mmap *map)
{
	struct fastrpc_apps *me = &gfa;
	struct fastrpc_file *fl;
	int vmid;

	if (!map)
		return;
	fl = map->fl;
	if (map->flags == ADSP_MMAP_HEAP_ADDR) {
		spin_lock(&me->hlock);
		map->refs--;
		if (!map->refs)
			hlist_del_init(&map->hn);
		spin_unlock(&me->hlock);
	} else {
		spin_lock(&fl->hlock);
		map->refs--;
		if (!map->refs)
			hlist_del_init(&map->hn);
		spin_unlock(&fl->hlock);
	}
	if (map->refs > 0)
		return;
	if (map->flags == ADSP_MMAP_HEAP_ADDR) {
		DEFINE_DMA_ATTRS(attrs);

		if (me->dev == NULL) {
			pr_err("failed to free remote heap allocation\n");
			return;
		}
		if (map->phys) {
			dma_set_attr(DMA_ATTR_SKIP_ZEROING, &attrs);
			dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &attrs);
			dma_free_attrs(me->dev, map->size,
					&(map->va), map->phys,	&attrs);
		}
	} else {
		int destVM[1] = {VMID_HLOS};
		int destVMperm[1] = {PERM_READ | PERM_WRITE | PERM_EXEC};

		if (!IS_ERR_OR_NULL(map->handle))
			ion_free(fl->apps->client, map->handle);
		if (fl->sctx->smmu.enabled) {
			if (map->size || map->phys)
				msm_dma_unmap_sg(fl->sctx->dev,
					map->table->sgl,
					map->table->nents, DMA_BIDIRECTIONAL,
					map->buf);
		}
		vmid = fl->apps->channel[fl->cid].vmid;
		if (vmid && map->phys) {
			int srcVM[2] = {VMID_HLOS, vmid};

			hyp_assign_phys(map->phys, buf_page_size(map->size),
				srcVM, 2, destVM, destVMperm, 1);
		}

		if (!IS_ERR_OR_NULL(map->table))
			dma_buf_unmap_attachment(map->attach, map->table,
					DMA_BIDIRECTIONAL);
		if (!IS_ERR_OR_NULL(map->attach))
			dma_buf_detach(map->buf, map->attach);
		if (!IS_ERR_OR_NULL(map->buf))
			dma_buf_put(map->buf);
	}
	kfree(map);
}

static int fastrpc_mmap_create(struct fastrpc_file *fl, int fd, uintptr_t va,
			ssize_t len, int mflags, struct fastrpc_mmap **ppmap)
{
	struct fastrpc_apps *me = &gfa;
	struct fastrpc_session_ctx *sess = fl->sctx;
	struct fastrpc_mmap *map = 0;
	struct dma_attrs attrs;
	phys_addr_t region_start = 0;
	unsigned long flags;
	int err = 0, vmid;

	if (!fastrpc_mmap_find(fl, fd, va, len, mflags, ppmap))
		return 0;
	map = kzalloc(sizeof(*map), GFP_KERNEL);
	VERIFY(err, !IS_ERR_OR_NULL(map));
	if (err)
		goto bail;
	map->flags = mflags;
	map->refs = 1;
	INIT_HLIST_NODE(&map->hn);
	map->fl = fl;
	map->fd = fd;
	if (mflags == ADSP_MMAP_HEAP_ADDR) {
		map->apps = me;
		map->fl = 0;
		VERIFY(err, !dma_alloc_memory(&region_start, len));
		if (err)
			goto bail;
		map->phys = (uintptr_t)region_start;
		map->size = len;
	} else {
		VERIFY(err, !IS_ERR_OR_NULL(map->buf = dma_buf_get(fd)));
		if (err)
			goto bail;
		VERIFY(err, !IS_ERR_OR_NULL(map->attach =
				dma_buf_attach(map->buf, sess->dev)));
		if (err)
			goto bail;
		VERIFY(err, !IS_ERR_OR_NULL(map->table =
				dma_buf_map_attachment(map->attach,
					DMA_BIDIRECTIONAL)));
		if (err)
			goto bail;
		if (sess->smmu.enabled) {
			init_dma_attrs(&attrs);
			dma_set_attr(DMA_ATTR_EXEC_MAPPING, &attrs);
			VERIFY(err, map->table->nents ==
					msm_dma_map_sg_attrs(sess->dev,
					map->table->sgl, map->table->nents,
					DMA_BIDIRECTIONAL, map->buf, &attrs));
			if (err)
				goto bail;
		} else {
			VERIFY(err, map->table->nents == 1);
			if (err)
				goto bail;
		}
		VERIFY(err, !IS_ERR_OR_NULL(map->handle =
				ion_import_dma_buf(fl->apps->client, fd)));
		if (err)
			goto bail;
		VERIFY(err, !ion_handle_get_flags(fl->apps->client,
							map->handle, &flags));
		if (err)
			goto bail;
		map->uncached = !ION_IS_CACHED(flags);
		map->phys = sg_dma_address(map->table->sgl);
		if (sess->smmu.cb) {
			map->phys += ((uint64_t)sess->smmu.cb << 32);
			map->size = sg_dma_len(map->table->sgl);
		} else {
			map->size = buf_page_size(len);
		}
		vmid = fl->apps->channel[fl->cid].vmid;
		if (vmid) {
			int srcVM[1] = {VMID_HLOS};
			int destVM[2] = {VMID_HLOS, vmid};
			int destVMperm[2] = {PERM_READ | PERM_WRITE,
						PERM_READ | PERM_WRITE };

			VERIFY(err, !hyp_assign_phys(map->phys,
					buf_page_size(map->size),
					srcVM, 1, destVM, destVMperm, 2));
			if (err)
				goto bail;
		}
	}
	map->va = va;
	map->len = len;

	fastrpc_mmap_add(map);
	*ppmap = map;

bail:
	if (err && map)
		fastrpc_mmap_free(map);
	return err;
}

static int fastrpc_buf_alloc(struct fastrpc_file *fl, ssize_t size,
			     struct fastrpc_buf **obuf)
{
	int err = 0, vmid;
	struct fastrpc_buf *buf = 0, *fr = 0;
	struct hlist_node *n;

	VERIFY(err, size > 0);
	if (err)
		goto bail;

	/* find the smallest buffer that fits in the cache */
	spin_lock(&fl->hlock);
	hlist_for_each_entry_safe(buf, n, &fl->bufs, hn) {
		if (buf->size >= size && (!fr || fr->size > buf->size))
			fr = buf;
	}
	if (fr)
		hlist_del_init(&fr->hn);
	spin_unlock(&fl->hlock);
	if (fr) {
		*obuf = fr;
		return 0;
	}
	buf = 0;
	VERIFY(err, buf = kzalloc(sizeof(*buf), GFP_KERNEL));
	if (err)
		goto bail;
	INIT_HLIST_NODE(&buf->hn);
	buf->fl = fl;
	buf->virt = 0;
	buf->phys = 0;
	buf->size = size;
	buf->virt = dma_alloc_coherent(fl->sctx->dev, buf->size,
				       (void *)&buf->phys, GFP_KERNEL);
	if (IS_ERR_OR_NULL(buf->virt)) {
		/* free cache and retry */
		fastrpc_buf_list_free(fl);
		buf->virt = dma_alloc_coherent(fl->sctx->dev, buf->size,
					       (void *)&buf->phys, GFP_KERNEL);
		VERIFY(err, !IS_ERR_OR_NULL(buf->virt));
	}
	if (err)
		goto bail;
	if (fl->sctx->smmu.cb)
		buf->phys += ((uint64_t)fl->sctx->smmu.cb << 32);
	vmid = fl->apps->channel[fl->cid].vmid;
	if (vmid) {
		int srcVM[1] = {VMID_HLOS};
		int destVM[2] = {VMID_HLOS, vmid};
		int destVMperm[2] = {PERM_READ | PERM_WRITE,
					PERM_READ | PERM_WRITE};

		VERIFY(err, !hyp_assign_phys(buf->phys, buf_page_size(size),
			srcVM, 1, destVM, destVMperm, 2));
		if (err)
			goto bail;
	}

	*obuf = buf;
 bail:
	if (err && buf)
		fastrpc_buf_free(buf, 0);
	return err;
}

static int context_restore_interrupted(struct fastrpc_file *fl,
				       struct fastrpc_ioctl_invoke_fd *invokefd,
				       struct smq_invoke_ctx **po)
{
	int err = 0;
	struct smq_invoke_ctx *ctx = 0, *ictx = 0;
	struct hlist_node *n;
	struct fastrpc_ioctl_invoke *invoke = &invokefd->inv;
	spin_lock(&fl->hlock);
	hlist_for_each_entry_safe(ictx, n, &fl->clst.interrupted, hn) {
		if (ictx->pid == current->pid) {
			if (invoke->sc != ictx->sc || ictx->fl != fl)
				err = -1;
			else {
				ctx = ictx;
				hlist_del_init(&ctx->hn);
				hlist_add_head(&ctx->hn, &fl->clst.pending);
			}
			break;
		}
	}
	spin_unlock(&fl->hlock);
	if (ctx)
		*po = ctx;
	return err;
}

#define CMP(aa, bb) ((aa) == (bb) ? 0 : (aa) < (bb) ? -1 : 1)
static int overlap_ptr_cmp(const void *a, const void *b)
{
	struct overlap *pa = *((struct overlap **)a);
	struct overlap *pb = *((struct overlap **)b);
	/* sort with lowest starting buffer first */
	int st = CMP(pa->start, pb->start);
	/* sort with highest ending buffer first */
	int ed = CMP(pb->end, pa->end);
	return st == 0 ? ed : st;
}

static void context_build_overlap(struct smq_invoke_ctx *ctx)
{
	int i;
	remote_arg_t *lpra = ctx->lpra;
	int inbufs = REMOTE_SCALARS_INBUFS(ctx->sc);
	int outbufs = REMOTE_SCALARS_OUTBUFS(ctx->sc);
	int nbufs = inbufs + outbufs;
	struct overlap max;
	for (i = 0; i < nbufs; ++i) {
		ctx->overs[i].start = (uintptr_t)lpra[i].buf.pv;
		ctx->overs[i].end = ctx->overs[i].start + lpra[i].buf.len;
		ctx->overs[i].raix = i;
		ctx->overps[i] = &ctx->overs[i];
	}
	sort(ctx->overps, nbufs, sizeof(*ctx->overps), overlap_ptr_cmp, 0);
	max.start = 0;
	max.end = 0;
	for (i = 0; i < nbufs; ++i) {
		if (ctx->overps[i]->start < max.end) {
			ctx->overps[i]->mstart = max.end;
			ctx->overps[i]->mend = ctx->overps[i]->end;
			ctx->overps[i]->offset = max.end -
				ctx->overps[i]->start;
			if (ctx->overps[i]->end > max.end) {
				max.end = ctx->overps[i]->end;
			} else {
				ctx->overps[i]->mend = 0;
				ctx->overps[i]->mstart = 0;
			}
		} else  {
			ctx->overps[i]->mend = ctx->overps[i]->end;
			ctx->overps[i]->mstart = ctx->overps[i]->start;
			ctx->overps[i]->offset = 0;
			max = *ctx->overps[i];
		}
	}
}

#define K_COPY_FROM_USER(err, kernel, dst, src, size) \
	do {\
		if (!(kernel))\
			VERIFY(err, 0 == copy_from_user((dst), (src),\
							(size)));\
		else\
			memmove((dst), (src), (size));\
	} while (0)

#define K_COPY_TO_USER(err, kernel, dst, src, size) \
	do {\
		if (!(kernel))\
			VERIFY(err, 0 == copy_to_user((dst), (src),\
						      (size)));\
		else\
			memmove((dst), (src), (size));\
	} while (0)


static void context_free(struct smq_invoke_ctx *ctx);

static int context_alloc(struct fastrpc_file *fl, uint32_t kernel,
			 struct fastrpc_ioctl_invoke_fd *invokefd,
			 struct smq_invoke_ctx **po)
{
	int err = 0, bufs, size = 0;
	struct smq_invoke_ctx *ctx = 0;
	struct fastrpc_ctx_lst *clst = &fl->clst;
	struct fastrpc_ioctl_invoke *invoke = &invokefd->inv;

	bufs = REMOTE_SCALARS_LENGTH(invoke->sc);
	size = bufs * sizeof(*ctx->lpra) + bufs * sizeof(*ctx->maps) +
		sizeof(*ctx->fds) * (bufs) +
		sizeof(*ctx->overs) * (bufs) +
		sizeof(*ctx->overps) * (bufs);

	VERIFY(err, ctx = kzalloc(sizeof(*ctx) + size, GFP_KERNEL));
	if (err)
		goto bail;

	INIT_HLIST_NODE(&ctx->hn);
	hlist_add_fake(&ctx->hn);
	ctx->fl = fl;
	ctx->maps = (struct fastrpc_mmap **)(&ctx[1]);
	ctx->lpra = (remote_arg_t *)(&ctx->maps[bufs]);
	ctx->fds = (int *)(&ctx->lpra[bufs]);
	ctx->overs = (struct overlap *)(&ctx->fds[bufs]);
	ctx->overps = (struct overlap **)(&ctx->overs[bufs]);

	K_COPY_FROM_USER(err, kernel, ctx->lpra, invoke->pra,
					bufs * sizeof(*ctx->lpra));
	if (err)
		goto bail;

	if (invokefd->fds) {
		K_COPY_FROM_USER(err, kernel, ctx->fds, invokefd->fds,
						bufs * sizeof(*ctx->fds));
		if (err)
			goto bail;
	}
	ctx->sc = invoke->sc;
	if (bufs)
		context_build_overlap(ctx);
	ctx->retval = -1;
	ctx->pid = current->pid;
	ctx->tgid = current->tgid;
	init_completion(&ctx->work);

	spin_lock(&fl->hlock);
	hlist_add_head(&ctx->hn, &clst->pending);
	spin_unlock(&fl->hlock);

	*po = ctx;
bail:
	if (ctx && err)
		context_free(ctx);
	return err;
}

static void context_save_interrupted(struct smq_invoke_ctx *ctx)
{
	struct fastrpc_ctx_lst *clst = &ctx->fl->clst;
	spin_lock(&ctx->fl->hlock);
	hlist_del_init(&ctx->hn);
	hlist_add_head(&ctx->hn, &clst->interrupted);
	spin_unlock(&ctx->fl->hlock);
	/* free the cache on power collapse */
	fastrpc_buf_list_free(ctx->fl);
}

static void context_free(struct smq_invoke_ctx *ctx)
{
	int i;
	int nbufs = REMOTE_SCALARS_INBUFS(ctx->sc) +
		    REMOTE_SCALARS_OUTBUFS(ctx->sc);
	spin_lock(&ctx->fl->hlock);
	hlist_del_init(&ctx->hn);
	spin_unlock(&ctx->fl->hlock);
	for (i = 0; i < nbufs; ++i)
		fastrpc_mmap_free(ctx->maps[i]);
	fastrpc_buf_free(ctx->buf, 1);
	kfree(ctx);
}

static void context_notify_user(struct smq_invoke_ctx *ctx, int retval)
{
	ctx->retval = retval;
	complete(&ctx->work);
}


static void fastrpc_notify_users(struct fastrpc_file *me)
{
	struct smq_invoke_ctx *ictx;
	struct hlist_node *n;
	spin_lock(&me->hlock);
	hlist_for_each_entry_safe(ictx, n, &me->clst.pending, hn) {
		complete(&ictx->work);
	}
	hlist_for_each_entry_safe(ictx, n, &me->clst.interrupted, hn) {
		complete(&ictx->work);
	}
	spin_unlock(&me->hlock);

}

static void fastrpc_notify_drivers(struct fastrpc_apps *me, int cid)
{
	struct fastrpc_file *fl;
	struct hlist_node *n;
	spin_lock(&me->hlock);
	hlist_for_each_entry_safe(fl, n, &me->drivers, hn) {
		if (fl->cid == cid)
			fastrpc_notify_users(fl);
	}
	spin_unlock(&me->hlock);

}
static void context_list_ctor(struct fastrpc_ctx_lst *me)
{
	INIT_HLIST_HEAD(&me->interrupted);
	INIT_HLIST_HEAD(&me->pending);
}

static void fastrpc_context_list_dtor(struct fastrpc_file *fl)
{
	struct fastrpc_ctx_lst *clst = &fl->clst;
	struct smq_invoke_ctx *ictx = 0, *ctxfree;
	struct hlist_node *n;
	do {
		ctxfree = 0;
		spin_lock(&fl->hlock);
		hlist_for_each_entry_safe(ictx, n, &clst->interrupted, hn) {
			hlist_del_init(&ictx->hn);
			ctxfree = ictx;
			break;
		}
		spin_unlock(&fl->hlock);
		if (ctxfree)
			context_free(ctxfree);
	} while (ctxfree);
	do {
		ctxfree = 0;
		spin_lock(&fl->hlock);
		hlist_for_each_entry_safe(ictx, n, &clst->pending, hn) {
			hlist_del_init(&ictx->hn);
			ctxfree = ictx;
			break;
		}
		spin_unlock(&fl->hlock);
		if (ctxfree)
			context_free(ctxfree);
	} while (ctxfree);
}

static int fastrpc_file_free(struct fastrpc_file *fl);
static void fastrpc_file_list_dtor(struct fastrpc_apps *me)
{
	struct fastrpc_file *fl, *free;
	struct hlist_node *n;
	do {
		free = 0;
		spin_lock(&me->hlock);
		hlist_for_each_entry_safe(fl, n, &me->drivers, hn) {
			hlist_del_init(&fl->hn);
			free = fl;
			break;
		}
		spin_unlock(&me->hlock);
		if (free)
			fastrpc_file_free(free);
	} while (free);
}

static int get_args(uint32_t kernel, struct smq_invoke_ctx *ctx)
{
	remote_arg64_t *rpra;
	remote_arg_t *lpra = ctx->lpra;
	struct smq_invoke_buf *list;
	struct smq_phy_page *pages, *ipage;
	uint32_t sc = ctx->sc;
	int inbufs = REMOTE_SCALARS_INBUFS(sc);
	int outbufs = REMOTE_SCALARS_OUTBUFS(sc);
	int bufs = inbufs + outbufs;
	uintptr_t args;
	ssize_t rlen = 0, copylen = 0, metalen = 0;
	int i, inh, oix;
	int err = 0;
	int mflags = 0;

	/* calculate size of the metadata */
	rpra = 0;
	list = smq_invoke_buf_start(rpra, sc);
	pages = smq_phy_page_start(sc, list);
	ipage = pages;

	for (i = 0; i < bufs; ++i) {
		uintptr_t buf = (uintptr_t)lpra[i].buf.pv;
		ssize_t len = lpra[i].buf.len;
		if (ctx->fds[i])
			fastrpc_mmap_create(ctx->fl, ctx->fds[i], buf, len,
					    mflags, &ctx->maps[i]);
		ipage += 1;
	}
	metalen = copylen = (ssize_t)&ipage[0];
	/* calculate len requreed for copying */
	for (oix = 0; oix < inbufs + outbufs; ++oix) {
		int i = ctx->overps[oix]->raix;
		ssize_t len = lpra[i].buf.len;
		if (!len)
			continue;
		if (ctx->maps[i])
			continue;
		if (ctx->overps[oix]->offset == 0)
			copylen = ALIGN(copylen, BALIGN);
		copylen += ctx->overps[oix]->mend - ctx->overps[oix]->mstart;
	}
	ctx->used = copylen;

	/* allocate new buffer */
	if (copylen) {
		VERIFY(err, !fastrpc_buf_alloc(ctx->fl, copylen, &ctx->buf));
		if (err)
			goto bail;
	}
	/* copy metadata */
	rpra = ctx->buf->virt;
	ctx->rpra = rpra;
	list = smq_invoke_buf_start(rpra, sc);
	pages = smq_phy_page_start(sc, list);
	ipage = pages;
	args = (uintptr_t)ctx->buf->virt + metalen;
	for (i = 0; i < bufs; ++i) {
		ssize_t len = lpra[i].buf.len;
		list[i].num = 0;
		list[i].pgidx = 0;
		if (!len)
			continue;
		list[i].num = 1;
		list[i].pgidx = ipage - pages;
		ipage++;
	}
	/* map ion buffers */
	for (i = 0; i < inbufs + outbufs; ++i) {
		struct fastrpc_mmap *map = ctx->maps[i];
		uint64_t buf = ptr_to_uint64(lpra[i].buf.pv);
		ssize_t len = lpra[i].buf.len;
		rpra[i].buf.pv = 0;
		rpra[i].buf.len = len;
		if (!len)
			continue;
		if (map) {
			struct vm_area_struct *vma;
			uintptr_t offset;
			int num = buf_num_pages(buf, len);
			int idx = list[i].pgidx;

			down_read(&current->mm->mmap_sem);
			VERIFY(err, NULL != (vma = find_vma(current->mm,
								map->va)));
			if (err) {
				up_read(&current->mm->mmap_sem);
				goto bail;
			}
			offset = buf_page_start(buf) - vma->vm_start;
			up_read(&current->mm->mmap_sem);

			VERIFY(err, offset < (uintptr_t)map->size);
			if (err)
				goto bail;
			pages[idx].addr = map->phys + offset;
			pages[idx].size = num << PAGE_SHIFT;
		}
		rpra[i].buf.pv = buf;
	}
	/* copy non ion buffers */
	rlen = copylen - metalen;
	for (oix = 0; oix < inbufs + outbufs; ++oix) {
		int i = ctx->overps[oix]->raix;
		struct fastrpc_mmap *map = ctx->maps[i];
		int mlen = ctx->overps[oix]->mend - ctx->overps[oix]->mstart;
		uint64_t buf;
		ssize_t len = lpra[i].buf.len;
		if (!len)
			continue;
		if (map)
			continue;
		if (ctx->overps[oix]->offset == 0) {
			rlen -= ALIGN(args, BALIGN) - args;
			args = ALIGN(args, BALIGN);
		}
		VERIFY(err, rlen >= mlen);
		if (err)
			goto bail;
		rpra[i].buf.pv = (args - ctx->overps[oix]->offset);
		pages[list[i].pgidx].addr = ctx->buf->phys -
					    ctx->overps[oix]->offset +
					    (copylen - rlen);
		pages[list[i].pgidx].addr =
			buf_page_start(pages[list[i].pgidx].addr);
		buf = rpra[i].buf.pv;
		pages[list[i].pgidx].size = buf_num_pages(buf, len) * PAGE_SIZE;
		if (i < inbufs) {
			K_COPY_FROM_USER(err, kernel, uint64_to_ptr(buf),
					lpra[i].buf.pv, len);
			if (err)
				goto bail;
		}
		args = args + mlen;
		rlen -= mlen;
	}

	for (oix = 0; oix < inbufs + outbufs; ++oix) {
		int i = ctx->overps[oix]->raix;
		struct fastrpc_mmap *map = ctx->maps[i];
		if (map && map->uncached)
			continue;
		if (rpra[i].buf.len && ctx->overps[oix]->mstart)
			dmac_flush_range(uint64_to_ptr(rpra[i].buf.pv),
			uint64_to_ptr(rpra[i].buf.pv + rpra[i].buf.len));
	}
	inh = inbufs + outbufs;
	for (i = 0; i < REMOTE_SCALARS_INHANDLES(sc); i++) {
		rpra[inh + i].buf.pv = ptr_to_uint64(ctx->lpra[inh + i].buf.pv);
		rpra[inh + i].buf.len = ctx->lpra[inh + i].buf.len;
		rpra[inh + i].h = ctx->lpra[inh + i].h;
	}
	dmac_flush_range((char *)rpra, (char *)rpra + ctx->used);
 bail:
	return err;
}

static int put_args(uint32_t kernel, struct smq_invoke_ctx *ctx,
		    remote_arg_t *upra)
{
	uint32_t sc = ctx->sc;
	remote_arg64_t *rpra = ctx->rpra;
	int i, inbufs, outbufs, outh, size;
	int err = 0;

	inbufs = REMOTE_SCALARS_INBUFS(sc);
	outbufs = REMOTE_SCALARS_OUTBUFS(sc);
	for (i = inbufs; i < inbufs + outbufs; ++i) {
		if (!ctx->maps[i]) {
			K_COPY_TO_USER(err, kernel,
				ctx->lpra[i].buf.pv,
				uint64_to_ptr(rpra[i].buf.pv),
				rpra[i].buf.len);
			if (err)
				goto bail;
		} else {
			fastrpc_mmap_free(ctx->maps[i]);
			ctx->maps[i] = 0;
		}
	}
	size = sizeof(*rpra) * REMOTE_SCALARS_OUTHANDLES(sc);
	if (size) {
		outh = inbufs + outbufs + REMOTE_SCALARS_INHANDLES(sc);
		K_COPY_TO_USER(err, kernel, &upra[outh], &rpra[outh], size);
		if (err)
			goto bail;
	}
 bail:
	return err;
}

static void inv_args_pre(uint32_t sc, remote_arg64_t *rpra)
{
	int i, inbufs, outbufs;
	uintptr_t end;

	inbufs = REMOTE_SCALARS_INBUFS(sc);
	outbufs = REMOTE_SCALARS_OUTBUFS(sc);
	for (i = inbufs; i < inbufs + outbufs; ++i) {
		if (!rpra[i].buf.len)
			continue;
		if (buf_page_start(ptr_to_uint64((void *)rpra)) ==
				buf_page_start(rpra[i].buf.pv))
			continue;
		if (!IS_CACHE_ALIGNED((uintptr_t)uint64_to_ptr(rpra[i].buf.pv)))
			dmac_flush_range(uint64_to_ptr(rpra[i].buf.pv),
				(char *)(uint64_to_ptr(rpra[i].buf.pv + 1)));
		end = (uintptr_t)uint64_to_ptr(rpra[i].buf.pv +
							rpra[i].buf.len);
		if (!IS_CACHE_ALIGNED(end))
			dmac_flush_range((char *)end,
				(char *)end + 1);
	}
}

static void inv_args(struct smq_invoke_ctx *ctx)
{
	int i, inbufs, outbufs;
	uint32_t sc = ctx->sc;
	remote_arg64_t *rpra = ctx->rpra;
	int used = ctx->used;
	int inv = 0;

	inbufs = REMOTE_SCALARS_INBUFS(sc);
	outbufs = REMOTE_SCALARS_OUTBUFS(sc);
	for (i = inbufs; i < inbufs + outbufs; ++i) {
		struct fastrpc_mmap *map = ctx->maps[i];
		if (map && map->uncached)
			continue;
		if (!rpra[i].buf.len)
			continue;
		if (buf_page_start(ptr_to_uint64((void *)rpra)) ==
				buf_page_start(rpra[i].buf.pv)) {
			inv = 1;
			continue;
		}
		if (map && map->handle)
			msm_ion_do_cache_op(ctx->fl->apps->client, map->handle,
				(char *)uint64_to_ptr(rpra[i].buf.pv),
				rpra[i].buf.len, ION_IOC_INV_CACHES);
		else
			dmac_inv_range((char *)uint64_to_ptr(rpra[i].buf.pv),
				(char *)uint64_to_ptr(rpra[i].buf.pv
						 + rpra[i].buf.len));
	}

	if (inv || REMOTE_SCALARS_OUTHANDLES(sc))
		dmac_inv_range(rpra, (char *)rpra + used);
}

static int fastrpc_invoke_send(struct smq_invoke_ctx *ctx,
			       uint32_t kernel, uint32_t handle)
{
	struct smq_msg *msg = &ctx->msg;
	struct fastrpc_file *fl = ctx->fl;
	int err = 0, len;

	VERIFY(err, 0 != fl->apps->channel[fl->cid].chan);
	if (err)
		goto bail;
	msg->pid = current->tgid;
	msg->tid = current->pid;
	if (kernel)
		msg->pid = 0;
	msg->invoke.header.ctx = ptr_to_uint64(ctx);
	msg->invoke.header.handle = handle;
	msg->invoke.header.sc = ctx->sc;
	msg->invoke.page.addr = ctx->buf ? ctx->buf->phys : 0;
	msg->invoke.page.size = buf_page_size(ctx->used);

	if (fl->apps->glink) {
		err = glink_tx(fl->apps->channel[fl->cid].chan,
			(void *)&fl->apps->channel[fl->cid], msg, sizeof(*msg),
			GLINK_TX_REQ_INTENT);
	} else {
		spin_lock(&fl->apps->hlock);
		len = smd_write((smd_channel_t *)
				fl->apps->channel[fl->cid].chan,
				msg, sizeof(*msg));
		spin_unlock(&fl->apps->hlock);
		VERIFY(err, len == sizeof(*msg));
	}
 bail:
	return err;
}

static void fastrpc_smd_read_handler(int cid)
{
	struct fastrpc_apps *me = &gfa;
	struct smq_invoke_rsp rsp = {0};
	int ret = 0;

	do {
		ret = smd_read_from_cb(me->channel[cid].chan, &rsp,
					sizeof(rsp));
		if (ret != sizeof(rsp))
			break;
		context_notify_user(uint64_to_ptr(rsp.ctx), rsp.retval);
	} while (ret == sizeof(rsp));
}

static void smd_event_handler(void *priv, unsigned event)
{
	struct fastrpc_apps *me = &gfa;
	int cid = (int)(uintptr_t)priv;

	switch (event) {
	case SMD_EVENT_OPEN:
		complete(&me->channel[cid].work);
		break;
	case SMD_EVENT_CLOSE:
		fastrpc_notify_drivers(me, cid);
		break;
	case SMD_EVENT_DATA:
		fastrpc_smd_read_handler(cid);
		break;
	}
}

static void fastrpc_init(struct fastrpc_apps *me)
{
	int i;
	INIT_HLIST_HEAD(&me->drivers);
	INIT_HLIST_HEAD(&me->fls);
	spin_lock_init(&me->hlock);
	mutex_init(&me->smd_mutex);
	me->channel = &gcinfo[0];
	for (i = 0; i < NUM_CHANNELS; i++) {
		init_completion(&me->channel[i].work);
		me->channel[i].bitmap = 0;
		me->channel[i].sesscount = 0;
	}
}

static int fastrpc_release_current_dsp_process(struct fastrpc_file *fl);

static int fastrpc_internal_invoke(struct fastrpc_file *fl, uint32_t mode,
				   uint32_t kernel,
				   struct fastrpc_ioctl_invoke_fd *invokefd)
{
	struct smq_invoke_ctx *ctx = 0;
	struct fastrpc_ioctl_invoke *invoke = &invokefd->inv;
	int cid = fl->cid;
	int interrupted = 0;
	int err = 0;

	if (!kernel) {
		VERIFY(err, 0 == context_restore_interrupted(fl, invokefd,
								&ctx));
		if (err)
			goto bail;
		if (fl->sctx->smmu.faults)
			err = FASTRPC_ENOSUCH;
		if (err)
			goto bail;
		if (ctx)
			goto wait;
	}

	VERIFY(err, 0 == context_alloc(fl, kernel, invokefd, &ctx));
	if (err)
		goto bail;

	if (REMOTE_SCALARS_LENGTH(ctx->sc)) {
		VERIFY(err, 0 == get_args(kernel, ctx));
		if (err)
			goto bail;
	}

	inv_args_pre(ctx->sc, ctx->rpra);
	if (FASTRPC_MODE_SERIAL == mode)
		inv_args(ctx);
	VERIFY(err, 0 == fastrpc_invoke_send(ctx, kernel, invoke->handle));
	if (err)
		goto bail;
	if (FASTRPC_MODE_PARALLEL == mode)
		inv_args(ctx);
 wait:
	if (kernel)
		wait_for_completion(&ctx->work);
	else {
		interrupted = wait_for_completion_interruptible(&ctx->work);
		VERIFY(err, 0 == (err = interrupted));
		if (err)
			goto bail;
	}
	VERIFY(err, 0 == (err = ctx->retval));
	if (err)
		goto bail;
	VERIFY(err, 0 == put_args(kernel, ctx, invoke->pra));
	if (err)
		goto bail;
 bail:
	if (ctx && interrupted == -ERESTARTSYS)
		context_save_interrupted(ctx);
	else if (ctx)
		context_free(ctx);
	if (fl->ssrcount != fl->apps->channel[cid].ssrcount)
		err = ECONNRESET;
	return err;
}

static int fastrpc_init_process(struct fastrpc_file *fl,
				struct fastrpc_ioctl_init *init)
{
	int err = 0;
	struct fastrpc_ioctl_invoke_fd ioctl;
	struct smq_phy_page pages[1];
	struct fastrpc_mmap *file = 0, *mem = 0;
	if (init->flags == FASTRPC_INIT_ATTACH) {
		remote_arg_t ra[1];
		int tgid = current->tgid;
		ra[0].buf.pv = (void *)&tgid;
		ra[0].buf.len = sizeof(tgid);
		ioctl.inv.handle = 1;
		ioctl.inv.sc = REMOTE_SCALARS_MAKE(0, 1, 0);
		ioctl.inv.pra = ra;
		ioctl.fds = 0;
		VERIFY(err, !(err = fastrpc_internal_invoke(fl,
			FASTRPC_MODE_PARALLEL, 1, &ioctl)));
		if (err)
			goto bail;
	} else if (init->flags == FASTRPC_INIT_CREATE) {
		remote_arg_t ra[4];
		int fds[4];
		int mflags = 0;
		struct {
			int pgid;
			int namelen;
			int filelen;
			int pageslen;
		} inbuf;
		inbuf.pgid = current->tgid;
		inbuf.namelen = strlen(current->comm) + 1;
		inbuf.filelen = init->filelen;
		VERIFY(err, !fastrpc_mmap_create(fl, init->filefd, init->file,
					init->filelen, mflags, &file));
		if (err)
			goto bail;
		inbuf.pageslen = 1;
		VERIFY(err, !fastrpc_mmap_create(fl, init->memfd, init->mem,
					init->memlen, mflags, &mem));
		if (err)
			goto bail;
		inbuf.pageslen = 1;
		ra[0].buf.pv = (void *)&inbuf;
		ra[0].buf.len = sizeof(inbuf);
		fds[0] = 0;

		ra[1].buf.pv = (void *)current->comm;
		ra[1].buf.len = inbuf.namelen;
		fds[1] = 0;

		ra[2].buf.pv = (void *)init->file;
		ra[2].buf.len = inbuf.filelen;
		fds[2] = init->filefd;

		pages[0].addr = mem->phys;
		pages[0].size = mem->size;
		ra[3].buf.pv = (void *)pages;
		ra[3].buf.len = 1 * sizeof(*pages);
		fds[3] = 0;

		ioctl.inv.handle = 1;
		ioctl.inv.sc = REMOTE_SCALARS_MAKE(6, 4, 0);
		ioctl.inv.pra = ra;
		ioctl.fds = fds;
		VERIFY(err, !(err = fastrpc_internal_invoke(fl,
			FASTRPC_MODE_PARALLEL, 1, &ioctl)));
		if (err)
			goto bail;
	} else {
		err = -ENOTTY;
	}
bail:
	if (mem && err)
		fastrpc_mmap_free(mem);
	if (file)
		fastrpc_mmap_free(file);
	return err;
}

static int fastrpc_release_current_dsp_process(struct fastrpc_file *fl)
{
	int err = 0;
	struct fastrpc_ioctl_invoke_fd ioctl;
	remote_arg_t ra[1];
	int tgid = 0;

	VERIFY(err, fl->apps->channel[fl->cid].chan != 0);
	if (err)
		goto bail;
	tgid = fl->tgid;
	ra[0].buf.pv = (void *)&tgid;
	ra[0].buf.len = sizeof(tgid);
	ioctl.inv.handle = 1;
	ioctl.inv.sc = REMOTE_SCALARS_MAKE(1, 1, 0);
	ioctl.inv.pra = ra;
	ioctl.fds = 0;
	VERIFY(err, 0 == (err = fastrpc_internal_invoke(fl,
		FASTRPC_MODE_PARALLEL, 1, &ioctl)));
bail:
	return err;
}

static int fastrpc_mmap_on_dsp(struct fastrpc_file *fl, uint32_t flags,
			       struct fastrpc_mmap *map)
{
	struct fastrpc_ioctl_invoke_fd ioctl;
	struct smq_phy_page page;
	int num = 1;
	remote_arg_t ra[3];
	int err = 0;
	struct {
		int pid;
		uint32_t flags;
		uintptr_t vaddrin;
		int num;
	} inargs;

	struct {
		uintptr_t vaddrout;
	} routargs;
	inargs.pid = current->tgid;
	inargs.vaddrin = (uintptr_t)map->va;
	inargs.flags = flags;
	inargs.num = fl->apps->compat ? num * sizeof(page) : num;
	ra[0].buf.pv = (void *)&inargs;
	ra[0].buf.len = sizeof(inargs);
	page.addr = map->phys;
	page.size = map->size;
	ra[1].buf.pv = (void *)&page;
	ra[1].buf.len = num * sizeof(page);

	ra[2].buf.pv = (void *)&routargs;
	ra[2].buf.len = sizeof(routargs);

	ioctl.inv.handle = 1;
	if (fl->apps->compat)
		ioctl.inv.sc = REMOTE_SCALARS_MAKE(4, 2, 1);
	else
		ioctl.inv.sc = REMOTE_SCALARS_MAKE(2, 2, 1);
	ioctl.inv.pra = ra;
	ioctl.fds = 0;
	VERIFY(err, 0 == (err = fastrpc_internal_invoke(fl,
		FASTRPC_MODE_PARALLEL, 1, &ioctl)));
	map->raddr = (uintptr_t)routargs.vaddrout;
	if (err)
		goto bail;
	if (flags == ADSP_MMAP_HEAP_ADDR) {
		struct scm_desc desc = {0};

		desc.args[0] = TZ_PIL_AUTH_QDSP6_PROC;
		desc.args[1] = map->phys;
		desc.args[2] = map->size;
		desc.arginfo = SCM_ARGS(3);
		err = scm_call2(SCM_SIP_FNID(SCM_SVC_PIL,
			TZ_PIL_PROTECT_MEM_SUBSYS_ID), &desc);
	}

bail:
	return err;
}

static int fastrpc_munmap_on_dsp_rh(struct fastrpc_file *fl,
				 struct fastrpc_mmap *map)
{
	struct fastrpc_ioctl_invoke_fd ioctl;
	struct scm_desc desc = {0};
	remote_arg_t ra[1];
	int err = 0;
	struct {
		uint8_t skey;
	} routargs;

	ra[0].buf.pv = (void *)&routargs;
	ra[0].buf.len = sizeof(routargs);

	ioctl.inv.handle = 1;
	ioctl.inv.sc = REMOTE_SCALARS_MAKE(7, 0, 1);
	ioctl.inv.pra = ra;
	ioctl.fds = 0;

	VERIFY(err, 0 == (err = fastrpc_internal_invoke(fl,
			FASTRPC_MODE_PARALLEL, 1, &ioctl)));
	if (err)
		goto bail;
	desc.args[0] = TZ_PIL_AUTH_QDSP6_PROC;
	desc.args[1] = map->phys;
	desc.args[2] = map->size;
	desc.args[3] = routargs.skey;
	desc.arginfo = SCM_ARGS(4);
	err = scm_call2(SCM_SIP_FNID(SCM_SVC_PIL,
		TZ_PIL_CLEAR_PROTECT_MEM_SUBSYS_ID), &desc);

bail:
	return err;
}

static int fastrpc_munmap_on_dsp(struct fastrpc_file *fl,
				 struct fastrpc_mmap *map)
{
	struct fastrpc_ioctl_invoke_fd ioctl;
	remote_arg_t ra[1];
	int err = 0;
	struct {
		int pid;
		uintptr_t vaddrout;
		ssize_t size;
	} inargs;
	if (map->flags == ADSP_MMAP_HEAP_ADDR) {
		VERIFY(err, !fastrpc_munmap_on_dsp_rh(fl, map));
		if (err)
			goto bail;
	}

	inargs.pid = current->tgid;
	inargs.size = map->size;
	inargs.vaddrout = map->raddr;
	ra[0].buf.pv = (void *)&inargs;
	ra[0].buf.len = sizeof(inargs);

	ioctl.inv.handle = 1;
	if (fl->apps->compat)
		ioctl.inv.sc = REMOTE_SCALARS_MAKE(5, 1, 0);
	else
		ioctl.inv.sc = REMOTE_SCALARS_MAKE(3, 1, 0);
	ioctl.inv.pra = ra;
	ioctl.fds = 0;
	VERIFY(err, 0 == (err = fastrpc_internal_invoke(fl,
		FASTRPC_MODE_PARALLEL, 1, &ioctl)));
bail:
	return err;
}

static int fastrpc_mmap_remove_ssr(struct fastrpc_file *fl)
{
	struct fastrpc_mmap *match = 0, *map = NULL;
	struct hlist_node *n = NULL;
	int err = 0, ret = 0;
	struct fastrpc_apps *me = &gfa;
	struct ramdump_segment *ramdump_segments_rh = NULL;

	spin_lock(&me->hlock);
	hlist_for_each_entry_safe(map, n, &me->maps, hn) {
			match = map;
			hlist_del_init(&map->hn);
			break;
	}
	spin_unlock(&me->hlock);

	if (match) {
		VERIFY(err, !fastrpc_munmap_on_dsp_rh(fl, match));
		if (err)
			goto bail;
		if (me->channel[0].ramdumpenabled) {
			ramdump_segments_rh = kcalloc(1,
				sizeof(struct ramdump_segment), GFP_KERNEL);
			if (ramdump_segments_rh) {
				ramdump_segments_rh->address = match->phys;
				ramdump_segments_rh->size = match->size;
				ret = do_elf_ramdump(
					me->channel[0].remoteheap_ramdump_dev,
					ramdump_segments_rh, 1);
				if (ret < 0)
					pr_err("ADSPRPC: unable to dump heap");
				kfree(ramdump_segments_rh);
			}
		}
		fastrpc_mmap_free(match);
	}
bail:
	if (err && match)
		fastrpc_mmap_add(match);
	return err;
}

static int fastrpc_mmap_remove(struct fastrpc_file *fl, uintptr_t va,
			     ssize_t len, struct fastrpc_mmap **ppmap);

static void fastrpc_mmap_add(struct fastrpc_mmap *map);

static int fastrpc_internal_munmap(struct fastrpc_file *fl,
				   struct fastrpc_ioctl_munmap *ud)
{
	int err = 0;
	struct fastrpc_mmap *map = 0;
	if (!fastrpc_mmap_remove(fl, ud->vaddrout, ud->size,
				 &map)) {
		VERIFY(err, !fastrpc_munmap_on_dsp(fl, map));
		if (err)
			goto bail;
		fastrpc_mmap_free(map);
	}
bail:
	if (err && map)
		fastrpc_mmap_add(map);
	return err;
}

static int fastrpc_internal_mmap(struct fastrpc_file *fl,
				 struct fastrpc_ioctl_mmap *ud)
{

	struct fastrpc_mmap *map = 0;
	int err = 0;
	if (!fastrpc_mmap_find(fl, ud->fd, (uintptr_t)ud->vaddrin, ud->size,
			       ud->flags, &map))
		return 0;

	VERIFY(err, !fastrpc_mmap_create(fl, ud->fd, (uintptr_t)ud->vaddrin,
					 ud->size, ud->flags, &map));
	if (err)
		goto bail;
	VERIFY(err, 0 == fastrpc_mmap_on_dsp(fl, ud->flags, map));
	if (err)
		goto bail;
	ud->vaddrout = map->raddr;
 bail:
	if (err && map)
		fastrpc_mmap_free(map);
	return err;
}

static void fastrpc_channel_close(struct kref *kref)
{
	struct fastrpc_apps *me = &gfa;
	struct fastrpc_channel_ctx *ctx;
	int cid;

	ctx = container_of(kref, struct fastrpc_channel_ctx, kref);
	if (!me->glink) {
		smd_close(ctx->chan);
	} else {
		glink_unregister_link_state_cb(ctx->link_notify_handle);
		glink_close(ctx->chan);
	}
	ctx->chan = 0;
	mutex_unlock(&me->smd_mutex);
	cid = ctx - &gcinfo[0];
	pr_info("'closed /dev/%s c %d %d'\n", gcinfo[cid].name,
						MAJOR(me->dev_no), cid);
}

static void fastrpc_context_list_dtor(struct fastrpc_file *fl);

static int fastrpc_file_free(struct fastrpc_file *fl)
{
	struct hlist_node *n;
	struct fastrpc_mmap *map = 0;
	int cid;

	if (!fl)
		return 0;
	cid = fl->cid;

	spin_lock(&fl->apps->hlock);
	hlist_del_init(&fl->hn);
	spin_unlock(&fl->apps->hlock);

	(void)fastrpc_release_current_dsp_process(fl);
	fastrpc_context_list_dtor(fl);
	fastrpc_buf_list_free(fl);
	hlist_for_each_entry_safe(map, n, &fl->maps, hn) {
		fastrpc_mmap_free(map);
	}
	if (fl->ssrcount == fl->apps->channel[cid].ssrcount)
		kref_put_mutex(&fl->apps->channel[cid].kref,
				fastrpc_channel_close, &fl->apps->smd_mutex);
	kfree(fl);
	return 0;
}

static int fastrpc_session_alloc(struct fastrpc_channel_ctx *chan, int *session)
{
	struct fastrpc_apps *me = &gfa;
	int idx = 0, err = 0;

	switch (chan->channel) {
	case SMD_APPS_QDSP:
		idx = ffz(chan->bitmap);
		VERIFY(err, idx < chan->sesscount);
		if (err)
			goto bail;
		set_bit(idx, &chan->bitmap);
		break;
	case SMD_APPS_DSPS:
		VERIFY(err, me->dev != NULL);
		if (err)
			goto bail;
		chan->session[0].dev = me->dev;
		break;
	case SMD_APPS_MODEM:
		VERIFY(err, me->dev != NULL);
		if (err)
			goto bail;
		chan->session[0].dev = me->modem_cma_dev;
		break;
	}

	chan->session[idx].smmu.faults = 0;
	*session = idx;
 bail:
	return err;
}

static int fastrpc_session_free(struct fastrpc_channel_ctx *chan, int session)
{
	int err = 0;

	if (chan->sesscount) {
		VERIFY(err, session < chan->sesscount);
		if (err)
			goto bail;
		clear_bit(session, &chan->bitmap);
	}
 bail:
	return err;
}

bool fastrpc_glink_notify_rx_intent_req(void *h, const void *priv, size_t size)
{
	if (0 != glink_queue_rx_intent(h, NULL, size))
		return false;
	return true;
}

void fastrpc_glink_notify_tx_done(void *handle, const void *priv,
		const void *pkt_priv, const void *ptr)
{
}

void fastrpc_glink_notify_rx(void *handle, const void *priv,
	const void *pkt_priv, const void *ptr, size_t size)
{
	struct smq_invoke_rsp *rsp = (struct smq_invoke_rsp *)ptr;
	int len = size;

	while (len >= sizeof(*rsp) && rsp) {
		context_notify_user(uint64_to_ptr(rsp->ctx), rsp->retval);
		rsp++;
		len = len - sizeof(*rsp);
	}
	glink_rx_done(handle, ptr, true);
}

void fastrpc_glink_notify_state(void *handle, const void *priv, unsigned event)
{
	struct fastrpc_apps *me = &gfa;
	int cid = (int)(uintptr_t)priv;

	switch (event) {
	case GLINK_CONNECTED:
		complete(&me->channel[cid].work);
		break;
	case GLINK_LOCAL_DISCONNECTED:
		break;
	case GLINK_REMOTE_DISCONNECTED:
		fastrpc_notify_drivers(me, cid);
		break;
	}
}

static int fastrpc_device_release(struct inode *inode, struct file *file)
{
	struct fastrpc_apps *me = &gfa;
	struct fastrpc_file *fl = (struct fastrpc_file *)file->private_data;
	struct fastrpc_fl *pfl = 0;
	int session, cid;
	int err = 0;

	VERIFY(err, pfl = kzalloc(sizeof(*pfl), GFP_KERNEL));
	if (err)
		goto bail;
	INIT_HLIST_NODE(&pfl->hn);
	if (fl) {
		cid = fl->cid;
		if (fl->sctx) {
			session = fl->sctx - &me->channel[cid].session[0];
			fastrpc_session_free(&me->channel[cid], session);
		}
		mutex_lock(&me->flfree_mutex);
		pfl->fl = fl;
		hlist_add_head(&pfl->hn, &me->fls);
		VERIFY(err, me->wq);
		if (err) {
			hlist_del_init(&pfl->hn);
			mutex_unlock(&me->flfree_mutex);
			goto bail;
		}
		if (!work_busy(&me->free_work)) {
			INIT_WORK(&me->free_work, file_free_work_handler);
			VERIFY(err, queue_work(me->wq, &me->free_work));
			if (err) {
				hlist_del_init(&pfl->hn);
				mutex_unlock(&me->flfree_mutex);
				goto bail;
			}
		}
		me->pending_free++;
		mutex_unlock(&me->flfree_mutex);
		file->private_data = 0;
	}
bail:
	if (err)
		kfree(pfl);
	return err;
}


static void file_free_work_handler(struct work_struct *w)
{
	struct fastrpc_apps *me = &gfa;
	struct fastrpc_fl *fl = 0, *freefl = 0;
	struct hlist_node *n = 0;

	while (1) {
		mutex_lock(&me->flfree_mutex);
		hlist_for_each_entry_safe(fl, n, &me->fls, hn) {
			hlist_del_init(&fl->hn);
			freefl = fl;
			break;
		}
		mutex_unlock(&me->flfree_mutex);
		if (freefl)
			fastrpc_file_free(freefl->fl);
		mutex_lock(&me->flfree_mutex);

		if (hlist_empty(&me->fls)) {
			me->pending_free = 0;
			wake_up_interruptible_all(&wait_queue);
			mutex_unlock(&me->flfree_mutex);
			break;
		}
		mutex_unlock(&me->flfree_mutex);
		kfree(freefl);
	}
	return;
}

static void fastrpc_glink_register_cb(struct glink_link_state_cb_info *cb_info,
					 void *priv)
{
	switch (cb_info->link_state) {
	case GLINK_LINK_STATE_UP:
		if (priv)
			complete(priv);
		break;
	case GLINK_LINK_STATE_DOWN:
		break;
	default:
		pr_err("adsprpc: unknown glnk state %d\n", cb_info->link_state);
		break;
	}
}

static int fastrpc_glink_open(int cid, struct fastrpc_apps *me)
{
	int err = 0;
	struct glink_open_config *cfg = &me->channel[cid].cfg;
	struct glink_link_info *link_info = &me->channel[cid].link_info;

	link_info->edge = gcinfo[cid].edge;
	link_info->transport = "smem";
	link_info->glink_link_state_notif_cb = fastrpc_glink_register_cb;
	me->channel[cid].link_notify_handle = glink_register_link_state_cb(
					&me->channel[cid].link_info,
					(void *)(&me->channel[cid].work));
	VERIFY(err, !IS_ERR_OR_NULL(me->channel[cid].link_notify_handle));
	if (err)
		goto bail;

	VERIFY(err, wait_for_completion_timeout(&me->channel[cid].work,
						RPC_TIMEOUT));
	if (err)
		goto bail;

	cfg->priv = (void *)(uintptr_t)cid;
	cfg->edge = gcinfo[cid].edge;
	cfg->name = FASTRPC_GLINK_GUID;
	cfg->notify_rx = fastrpc_glink_notify_rx;
	cfg->notify_tx_done = fastrpc_glink_notify_tx_done;
	cfg->notify_state = fastrpc_glink_notify_state;
	cfg->notify_rx_intent_req = fastrpc_glink_notify_rx_intent_req;
	VERIFY(err, 0 != (me->channel[cid].chan = glink_open(cfg)));
bail:
	return err;
}

static int fastrpc_device_open(struct inode *inode, struct file *filp)
{
	int cid = MINOR(inode->i_rdev);
	int err = 0, session;
	int event;
	struct fastrpc_apps *me = &gfa;
	struct fastrpc_file *fl = 0;

	if (me->pending_free) {
		event = wait_event_interruptible_timeout(wait_queue,
						!me->pending_free, RPC_TIMEOUT);
		if (event == 0)
			pr_err("timed out..list is still not empty\n");
	}

	VERIFY(err, fl = kzalloc(sizeof(*fl), GFP_KERNEL));
	if (err)
		return err;

	filp->private_data = fl;

	mutex_lock(&me->smd_mutex);

	context_list_ctor(&fl->clst);
	spin_lock_init(&fl->hlock);
	INIT_HLIST_HEAD(&fl->maps);
	INIT_HLIST_HEAD(&fl->bufs);
	INIT_HLIST_NODE(&fl->hn);
	fl->tgid = current->tgid;
	fl->apps = me;
	fl->cid = cid;
	VERIFY(err, !fastrpc_session_alloc(&me->channel[cid], &session));
	if (err)
		goto bail;
	fl->sctx = &me->channel[cid].session[session];

	fl->ssrcount = me->channel[cid].ssrcount;
	if ((kref_get_unless_zero(&me->channel[cid].kref) == 0) ||
	    (me->channel[cid].chan == 0)) {
		if (me->glink) {
			VERIFY(err, 0 == fastrpc_glink_open(cid, me));
		} else {
			VERIFY(err, !smd_named_open_on_edge(FASTRPC_SMD_GUID,
				    gcinfo[cid].channel,
				    (smd_channel_t **)&me->channel[cid].chan,
				    (void *)(uintptr_t)cid,
				    smd_event_handler));
		}
		if (err)
			goto bail;

		VERIFY(err, wait_for_completion_timeout(&me->channel[cid].work,
							RPC_TIMEOUT));
		if (err) {
			me->channel[cid].chan = 0;
			goto bail;
		}
		kref_init(&me->channel[cid].kref);
		pr_info("'opened /dev/%s c %d %d'\n", gcinfo[cid].name,
						MAJOR(me->dev_no), cid);
		if (me->channel[cid].ssrcount !=
				 me->channel[cid].prevssrcount) {
			if (fastrpc_mmap_remove_ssr(fl))
				pr_err("ADSPRPC: SSR: Failed to unmap remote heap\n");
			me->channel[cid].prevssrcount =
						me->channel[cid].ssrcount;
		}
	}
	spin_lock(&me->hlock);
	hlist_add_head(&fl->hn, &me->drivers);
	spin_unlock(&me->hlock);

bail:
	mutex_unlock(&me->smd_mutex);

	if (err && fl)
		fastrpc_device_release(inode, filp);
	return err;
}


static long fastrpc_device_ioctl(struct file *file, unsigned int ioctl_num,
				 unsigned long ioctl_param)
{
	union {
		struct fastrpc_ioctl_invoke_fd invokefd;
		struct fastrpc_ioctl_mmap mmap;
		struct fastrpc_ioctl_munmap munmap;
		struct fastrpc_ioctl_init init;
	} p;
	void *param = (char *)ioctl_param;
	struct fastrpc_file *fl = (struct fastrpc_file *)file->private_data;
	int size = 0, err = 0;

	switch (ioctl_num) {
	case FASTRPC_IOCTL_INVOKE_FD:
	case FASTRPC_IOCTL_INVOKE:
		p.invokefd.fds = 0;
		size = (ioctl_num == FASTRPC_IOCTL_INVOKE) ?
				sizeof(p.invokefd.inv) : sizeof(p.invokefd);
		VERIFY(err, 0 == copy_from_user(&p.invokefd, param, size));
		if (err)
			goto bail;
		VERIFY(err, 0 == (err = fastrpc_internal_invoke(fl, fl->mode,
						0, &p.invokefd)));
		if (err)
			goto bail;
		break;
	case FASTRPC_IOCTL_MMAP:
		VERIFY(err, 0 == copy_from_user(&p.mmap, param,
						sizeof(p.mmap)));
		if (err)
			goto bail;
		VERIFY(err, 0 == (err = fastrpc_internal_mmap(fl, &p.mmap)));
		if (err)
			goto bail;
		VERIFY(err, 0 == copy_to_user(param, &p.mmap, sizeof(p.mmap)));
		if (err)
			goto bail;
		break;
	case FASTRPC_IOCTL_MUNMAP:
		VERIFY(err, 0 == copy_from_user(&p.munmap, param,
						sizeof(p.munmap)));
		if (err)
			goto bail;
		VERIFY(err, 0 == (err = fastrpc_internal_munmap(fl,
							&p.munmap)));
		if (err)
			goto bail;
		break;
	case FASTRPC_IOCTL_SETMODE:
		switch ((uint32_t)ioctl_param) {
		case FASTRPC_MODE_PARALLEL:
		case FASTRPC_MODE_SERIAL:
			fl->mode = (uint32_t)ioctl_param;
			break;
		default:
			err = -ENOTTY;
			break;
		}
		break;
	case FASTRPC_IOCTL_INIT:
		VERIFY(err, 0 == copy_from_user(&p.init, param,
						sizeof(p.init)));
		if (err)
			goto bail;
		VERIFY(err, 0 == fastrpc_init_process(fl, &p.init));
		if (err)
			goto bail;
		break;

	default:
		err = -ENOTTY;
		break;
	}
 bail:
	return err;
}

static int fastrpc_restart_notifier_cb(struct notifier_block *nb,
					unsigned long code,
					void *data)
{
	struct fastrpc_apps *me = &gfa;
	struct fastrpc_channel_ctx *ctx;
	struct notif_data *notifdata = data;
	int cid;

	ctx = container_of(nb, struct fastrpc_channel_ctx, nb);
	cid = ctx - &me->channel[0];
	if (code == SUBSYS_BEFORE_SHUTDOWN) {
		mutex_lock(&me->smd_mutex);
		ctx->ssrcount++;
		if (ctx->chan) {
			if (me->glink) {
				glink_unregister_link_state_cb(
					ctx->link_notify_handle);
				glink_close(ctx->chan);
			} else {
				smd_close(ctx->chan);
			}
			ctx->chan = 0;
			pr_info("'restart notifier: closed /dev/%s c %d %d'\n",
				 gcinfo[cid].name, MAJOR(me->dev_no), cid);
		}
		mutex_unlock(&me->smd_mutex);
		fastrpc_notify_drivers(me, cid);
	} else if (code == SUBSYS_RAMDUMP_NOTIFICATION) {
		if (me->channel[0].remoteheap_ramdump_dev &&
				notifdata->enable_ramdump) {
			me->channel[0].ramdumpenabled = 1;
		}
	}

	return NOTIFY_DONE;
}

static int fastrpc_smmu_fault_handler(struct iommu_domain *domain,
	struct device *dev, unsigned long iova, int flags, void *token)
{
	struct fastrpc_session_ctx *sess = (struct fastrpc_session_ctx *)token;
	int err = 0;

	VERIFY(err, sess != NULL);
	if (err)
		return err;
	sess->smmu.faults++;
	dev_err(dev, "ADSPRPC context fault: iova=0x%08lx, cb = %d, faults=%d",
					iova, sess->smmu.cb, sess->smmu.faults);
	return 0;
}

static const struct file_operations fops = {
	.open = fastrpc_device_open,
	.release = fastrpc_device_release,
	.unlocked_ioctl = fastrpc_device_ioctl,
	.compat_ioctl = compat_fastrpc_device_ioctl,
};

static struct of_device_id fastrpc_match_table[] = {
	{ .compatible = "qcom,msm-fastrpc-adsp", },
	{ .compatible = "qcom,msm-fastrpc-compute-cb", },
	{ .compatible = "qcom,msm-fastrpc-legacy-compute-cb", },
	{ .compatible = "qcom,msm-adsprpc-mem-region", },
	{ .compatible = "qcom,msm-mdsprpc-mem-region", },
	{}
};

static int fastrpc_cb_probe(struct device *dev)
{
	struct fastrpc_channel_ctx *chan;
	struct fastrpc_session_ctx *sess;
	struct of_phandle_args iommuspec;
	const char *name;
	int err = 0, i;
	int disable_htw = 1;

	VERIFY(err, 0 != (name = of_get_property(dev->of_node, "label", NULL)));
	if (err)
		goto bail;
	for (i = 0; i < NUM_CHANNELS; i++) {
		if (!gcinfo[i].name)
			continue;
		if (!strcmp(name, gcinfo[i].name))
			break;
	}
	VERIFY(err, i < NUM_CHANNELS);
	if (err)
		goto bail;
	chan = &gcinfo[i];
	VERIFY(err, chan->sesscount < NUM_SESSIONS);
	if (err)
		goto bail;

	VERIFY(err, !of_parse_phandle_with_args(dev->of_node, "iommus",
						"#iommu-cells", 0, &iommuspec));
	if (err)
		goto bail;
	sess = &chan->session[chan->sesscount];
	sess->smmu.cb = iommuspec.args[0];
	VERIFY(err, !IS_ERR_OR_NULL(sess->smmu.mapping =
				arm_iommu_create_mapping(&platform_bus_type,
						0x80000000, 0x7fffffff)));
	if (err)
		goto bail;
	iommu_domain_set_attr(sess->smmu.mapping->domain,
				DOMAIN_ATTR_COHERENT_HTW_DISABLE,
				&disable_htw);
	iommu_set_fault_handler(sess->smmu.mapping->domain,
				fastrpc_smmu_fault_handler, sess);
	VERIFY(err, !arm_iommu_attach_device(dev, sess->smmu.mapping));
	if (err)
		goto bail;
	sess->dev = dev;
	sess->smmu.enabled = 1;
	chan->sesscount++;
bail:
	return err;
}

static int fastrpc_cb_legacy_probe(struct device *dev)
{
	struct device_node *domains_child_node = NULL;
	struct device_node *ctx_node = NULL;
	struct fastrpc_channel_ctx *chan;
	struct fastrpc_session_ctx *first_sess, *sess;
	const char *name;
	unsigned int *range = 0, range_size = 0;
	unsigned int *sids = 0, sids_size = 0;
	int err = 0, ret = 0, i;
	int disable_htw = 1;

	VERIFY(err, 0 != (domains_child_node = of_get_child_by_name(
			dev->of_node,
			"qcom,msm_fastrpc_compute_cb")));
	if (err)
		goto bail;
	VERIFY(err, 0 != (ctx_node = of_parse_phandle(
			domains_child_node,
			"qcom,adsp-shared-phandle", 0)));
	if (err)
		goto bail;
	VERIFY(err, 0 != of_get_property(domains_child_node,
				"qcom,adsp-shared-sids", &sids_size));
	if (err)
		goto bail;
	VERIFY(err, sids = kzalloc(sids_size, GFP_KERNEL));
	if (err)
		goto bail;
	ret = of_property_read_u32_array(domains_child_node,
					"qcom,adsp-shared-sids",
					sids,
					sids_size/sizeof(unsigned int));
	if (ret)
		goto bail;
	VERIFY(err, 0 != (name = of_get_property(ctx_node, "label", NULL)));
	if (err)
		goto bail;
	VERIFY(err, 0 != of_get_property(domains_child_node,
					"qcom,virtual-addr-pool", &range_size));
	if (err)
		goto bail;
	VERIFY(err, range = kzalloc(range_size, GFP_KERNEL));
	if (err)
		goto bail;
	ret = of_property_read_u32_array(domains_child_node,
					"qcom,virtual-addr-pool",
					range,
					range_size/sizeof(unsigned int));
	if (ret)
		goto bail;

	chan = &gcinfo[0];
	VERIFY(err, chan->sesscount < NUM_SESSIONS);
	if (err)
		goto bail;
	first_sess = &chan->session[chan->sesscount];
	first_sess->dev = msm_iommu_get_ctx(name);
	VERIFY(err, !IS_ERR_OR_NULL(first_sess->smmu.mapping =
			arm_iommu_create_mapping(
				msm_iommu_get_bus(first_sess->dev),
				range[0], range[1])));
	if (err)
		goto bail;
	iommu_domain_set_attr(first_sess->smmu.mapping->domain,
			DOMAIN_ATTR_COHERENT_HTW_DISABLE,
			&disable_htw);
	VERIFY(err, !arm_iommu_attach_device(first_sess->dev,
					first_sess->smmu.mapping));
	if (err)
		goto bail;
	VERIFY(err, (sids_size/sizeof(unsigned int)) <= NUM_SESSIONS);
	if (err)
		goto bail;
	for (i = 0; i < sids_size/sizeof(unsigned int); i++) {
		VERIFY(err, chan->sesscount < NUM_SESSIONS);
		if (err)
			goto bail;
		sess = &chan->session[chan->sesscount];
		sess->smmu.cb = sids[i];
		sess->dev = first_sess->dev;
		sess->smmu.enabled = 1;
		sess->smmu.mapping = first_sess->smmu.mapping;
		chan->sesscount++;
	}
bail:
	kfree(sids);
	kfree(range);
	return err;
}

static int fastrpc_probe(struct platform_device *pdev)
{
	int err = 0;
	struct fastrpc_apps *me = &gfa;
	struct device *dev = &pdev->dev;

	if (of_device_is_compatible(dev->of_node,
					"qcom,msm-fastrpc-compute-cb"))
		return fastrpc_cb_probe(dev);

	if (of_device_is_compatible(dev->of_node,
					"qcom,msm-fastrpc-legacy-compute-cb"))
		return fastrpc_cb_legacy_probe(dev);

	if (of_device_is_compatible(dev->of_node,
					"qcom,msm-adsprpc-mem-region")) {
		me->dev = dev;
		me->channel[0].remoteheap_ramdump_dev =
				create_ramdump_device("adsp_rh", dev);
		if (IS_ERR_OR_NULL(me->channel[0].remoteheap_ramdump_dev)) {
			pr_err("ADSPRPC: Unable to create adsp-remoteheap ramdump device.\n");
			me->channel[0].remoteheap_ramdump_dev = NULL;
		}
		return 0;
	}

	if (of_device_is_compatible(dev->of_node,
					"qcom,msm-mdsprpc-mem-region")) {
		me->modem_cma_dev = dev;
		return 0;
	}

	me->glink = of_property_read_bool(dev->of_node, "qcom,fastrpc-glink");
	pr_info("adsprpc: channel link type: %d\n", me->glink);

	VERIFY(err, !of_platform_populate(pdev->dev.of_node,
					  fastrpc_match_table,
					  NULL, &pdev->dev));
	if (err)
		goto bail;
bail:
	return err;
}

static void fastrpc_deinit(void)
{
	struct fastrpc_apps *me = &gfa;
	struct fastrpc_channel_ctx *chan = gcinfo;
	int i, j;

	for (i = 0; i < NUM_CHANNELS; i++, chan++) {
		if (chan->chan) {
			kref_put_mutex(&chan->kref,
				fastrpc_channel_close, &me->smd_mutex);
			chan->chan = 0;
		}
		for (j = 0; j < NUM_SESSIONS; j++) {
			struct fastrpc_session_ctx *sess = &chan->session[j];
			if (sess->smmu.enabled) {
				arm_iommu_detach_device(sess->dev);
				sess->dev = 0;
			}
			if (sess->smmu.mapping) {
				arm_iommu_release_mapping(sess->smmu.mapping);
				sess->smmu.mapping = 0;
			}
		}
	}
}

static struct platform_driver fastrpc_driver = {
	.probe = fastrpc_probe,
	.driver = {
		.name = "fastrpc",
		.owner = THIS_MODULE,
		.of_match_table = fastrpc_match_table,
	},
};

static int __init fastrpc_device_init(void)
{
	struct fastrpc_apps *me = &gfa;
	int err = 0, i;

	memset(me, 0, sizeof(*me));

	fastrpc_init(me);
	mutex_init(&me->flfree_mutex);
	me->wq = create_singlethread_workqueue("FILE_FREE");
	INIT_WORK(&me->free_work, file_free_work_handler);

	me->dev = NULL;
	VERIFY(err, 0 == platform_driver_register(&fastrpc_driver));
	if (err)
		goto register_bail;
	VERIFY(err, 0 == alloc_chrdev_region(&me->dev_no, 0, NUM_CHANNELS,
					DEVICE_NAME));
	if (err)
		goto alloc_chrdev_bail;
	cdev_init(&me->cdev, &fops);
	me->cdev.owner = THIS_MODULE;
	VERIFY(err, 0 == cdev_add(&me->cdev, MKDEV(MAJOR(me->dev_no), 0),
				NUM_CHANNELS));
	if (err)
		goto cdev_init_bail;
	me->class = class_create(THIS_MODULE, "fastrpc");
	VERIFY(err, !IS_ERR(me->class));
	if (err)
		goto class_create_bail;
	me->compat = (NULL == fops.compat_ioctl) ? 0 : 1;
	for (i = 0; i < NUM_CHANNELS; i++) {
		if (!gcinfo[i].name)
			continue;
		me->channel[i].dev = device_create(me->class, NULL,
					MKDEV(MAJOR(me->dev_no), i),
					NULL, gcinfo[i].name);
		VERIFY(err, !IS_ERR(me->channel[i].dev));
		if (err)
			goto device_create_bail;
		me->channel[i].ssrcount = 0;
		me->channel[i].prevssrcount = 0;
		me->channel[i].ramdumpenabled = 0;
		me->channel[i].remoteheap_ramdump_dev = 0;
		me->channel[i].nb.notifier_call = fastrpc_restart_notifier_cb;
		me->channel[i].handle = subsys_notif_register_notifier(
							gcinfo[i].subsys,
							&me->channel[i].nb);
	}

	me->client = msm_ion_client_create(DEVICE_NAME);
	VERIFY(err, !IS_ERR_OR_NULL(me->client));
	if (err)
		goto device_create_bail;
	return 0;
device_create_bail:
	for (i = 0; i < NUM_CHANNELS; i++) {
		if (IS_ERR_OR_NULL(me->channel[i].dev))
			continue;
		device_destroy(me->class, MKDEV(MAJOR(me->dev_no), i));
		subsys_notif_unregister_notifier(me->channel[i].handle,
						&me->channel[i].nb);
	}
	class_destroy(me->class);
class_create_bail:
	cdev_del(&me->cdev);
cdev_init_bail:
	unregister_chrdev_region(me->dev_no, NUM_CHANNELS);
alloc_chrdev_bail:
register_bail:
	fastrpc_deinit();
	return err;
}

static void __exit fastrpc_device_exit(void)
{
	struct fastrpc_apps *me = &gfa;
	int i;

	fastrpc_file_list_dtor(me);
	fastrpc_deinit();
	if (me->wq) {
		flush_workqueue(me->wq);
		destroy_workqueue(me->wq);
	}
	for (i = 0; i < NUM_CHANNELS; i++) {
		if (!gcinfo[i].name)
			continue;
		device_destroy(me->class, MKDEV(MAJOR(me->dev_no), i));
		subsys_notif_unregister_notifier(me->channel[i].handle,
						&me->channel[i].nb);
	}
	class_destroy(me->class);
	cdev_del(&me->cdev);
	unregister_chrdev_region(me->dev_no, NUM_CHANNELS);
	ion_client_destroy(me->client);
}

late_initcall(fastrpc_device_init);
module_exit(fastrpc_device_exit);

MODULE_LICENSE("GPL v2");
