/*
 * Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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
#include <soc/qcom/smd.h>
#include <soc/qcom/subsystem_notif.h>
#include <linux/msm_iommu_domains.h>
#include <linux/scatterlist.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/dma-contiguous.h>
#include <linux/iommu.h>
#include <linux/kref.h>
#include <linux/sort.h>
#include <asm/dma-iommu.h>
#include "adsprpc_compat.h"
#include "adsprpc_shared.h"
#include <linux/msm_audio_ion.h>
#include <soc/qcom/scm.h>

#define TZ_PIL_PROTECT_MEM_SUBSYS_ID 0x0C
#define TZ_PIL_CLEAR_PROTECT_MEM_SUBSYS_ID 0x0D
#define TZ_PIL_AUTH_QDSP6_PROC 1
#define ADSP_MMAP_HEAP_ADDR 4
#define AUDIO_ADSP_STREAM_ID	1
#define STREAM_ID	((uint64_t)AUDIO_ADSP_STREAM_ID << 32)
#define RPC_TIMEOUT	(5 * HZ)
#define BALIGN		32
#define NUM_CHANNELS    1 /*8 compute 2 cpz 1 modem*/

#define IS_CACHE_ALIGNED(x) (((x) & ((L1_CACHE_BYTES)-1)) == 0)

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

struct fastrpc_buf {
	struct hlist_node hn;
	struct fastrpc_file *fl;
	void *virt;
	dma_addr_t phys;
	ssize_t size;
	struct ion_handle *handle;
	struct ion_client *client;
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
};

struct fastrpc_ctx_lst {
	struct hlist_head pending;
	struct hlist_head interrupted;
};

struct fastrpc_smmu {
	struct device *dev;
	struct dma_iommu_mapping *mapping;
	int cb;
	int enabled;
};

struct fastrpc_chan_ctx {
	smd_channel_t *chan;
	struct device *dev;
	struct completion work;
	struct fastrpc_smmu smmu;
	struct kref kref;
	struct notifier_block nb;
	int ssrcount;
};

struct fastrpc_apps {
	struct fastrpc_chan_ctx channel[NUM_CHANNELS];
	int nchans;
	struct cdev cdev;
	struct class *class;
	struct mutex smd_mutex;
	struct smq_phy_page range;
	struct hlist_head maps;
	dev_t dev_no;
	int compat;
	struct hlist_head drivers;
	spinlock_t hlock;
	int32_t domain_id;
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
	uintptr_t phys;
	ssize_t size;
	uintptr_t va;
	ssize_t len;
	int refs;
	uintptr_t raddr;
	struct ion_handle *handle;
	struct ion_client *client;
};

struct fastrpc_channel_info {
	char *name;
	char *node;
	char *group;
	char *subsys;
	int channel;
};

struct fastrpc_file {
	struct hlist_node hn;
	spinlock_t hlock;
	struct hlist_head maps;
	struct hlist_head bufs;
	struct fastrpc_ctx_lst clst;
	struct fastrpc_chan_ctx *chan;
	uint32_t mode;
	int tgid;
	int cid;
	int ssrcount;
	struct fastrpc_apps *apps;
};

static struct fastrpc_apps gfa;

static const struct fastrpc_channel_info gcinfo[NUM_CHANNELS] = {
	{
		.name = "adsprpc-smd",
		.node = "qcom,msm-audio-ion",
		.subsys = "adsp",
		.channel = SMD_APPS_QDSP,
	},
};

static void fastrpc_buf_free(struct fastrpc_buf *buf, int cache)
{
	int err = 0;
	struct fastrpc_file *fl = buf == 0 ? 0 : buf->fl;
	if (!fl)
		return;
	if (cache) {
		spin_lock(&fl->hlock);
		hlist_add_head(&buf->hn, &fl->bufs);
		spin_unlock(&fl->hlock);
		return;
	}
	if (!IS_ERR_OR_NULL(buf->virt)) {
		VERIFY(err, !msm_audio_ion_free(buf->client, buf->handle));
		if (err) {
			pr_err("error freeing audio ion buffer\n");
			goto bail;
		}
	}
bail:
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
	if (!map)
		return;
	if (map->flags == ADSP_MMAP_HEAP_ADDR) {
		spin_lock(&me->hlock);
		map->refs--;
		if (!map->refs)
			hlist_del_init(&map->hn);
		spin_unlock(&me->hlock);
	} else {
		fl = map->fl;
		spin_lock(&fl->hlock);
		map->refs--;
		if (!map->refs)
			hlist_del_init(&map->hn);
		spin_unlock(&fl->hlock);
	}
	if (map->refs)
		return;
	ion_unmap_iommu(map->client, map->handle,
			me->domain_id, 0);
	ion_unmap_kernel(map->client, map->handle);
	msm_audio_ion_client_destroy(map->client);
	kfree(map);
}

static int fastrpc_buf_alloc(struct fastrpc_file *fl, ssize_t size,
			     struct fastrpc_buf **obuf)
{
	int err = 0;
	struct fastrpc_buf *buf = 0, *fr = 0;
	struct hlist_node *n;
	size_t len = 0;
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
	VERIFY(err, buf = kzalloc(sizeof(*buf), GFP_KERNEL));
	if (err)
		goto bail;
	INIT_HLIST_NODE(&buf->hn);

	buf->handle = NULL;
	buf->client = NULL;
	buf->fl = fl;
	buf->virt = 0;
	buf->phys = 0;
	buf->size = size;

	VERIFY(err, !msm_audio_ion_alloc(DEVICE_NAME, &(buf->client),
				&(buf->handle), buf->size,
				&(buf->phys), &len, &(buf->virt)));
	if (err) {
		pr_err("%s: Error allocating audio ion buf size: %zd\n",
				__func__, buf->size);
		goto bail;
	}
	if (len != buf->size)
		pr_info("allocating memory from audio ion size is %zd\n",
				len);

	buf->size = len;

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

	bufs = REMOTE_SCALARS_INBUFS(invoke->sc) +
			REMOTE_SCALARS_OUTBUFS(invoke->sc);
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

static int fastrpc_mmap_create(struct fastrpc_file *fl, int fd, uintptr_t va,
		ssize_t len, int mflags, struct fastrpc_mmap **ppmap);

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
		if (ctx->fds[i] > 0) {
			VERIFY(err, !fastrpc_mmap_create(ctx->fl,
						ctx->fds[i], buf, len, mflags,
						&ctx->maps[i]));
			if (err) {
				pr_err("error mapping the buffer\n");
				goto bail;
			}
		}
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
			uintptr_t offset = buf_page_start(buf)
				- buf_page_start(map->va);
			int num = buf_num_pages(buf, len);
			int idx = list[i].pgidx;
			pages[idx].addr = map->phys + offset;
			if (msm_audio_ion_is_smmu_available())
				pages[idx].addr |= STREAM_ID;
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
		if (msm_audio_ion_is_smmu_available())
			pages[list[i].pgidx].addr |= STREAM_ID;
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

	for (i = 0; i < inbufs; ++i) {
		if (rpra[i].buf.len)
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
	int i, inbufs, outbufs, outh, num;
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
	num = REMOTE_SCALARS_OUTHANDLES(sc);
	if (num) {
		outh = inbufs + outbufs + REMOTE_SCALARS_INHANDLES(sc);
		K_COPY_TO_USER(err, kernel, &upra[outh], &ctx->lpra[outh],
				num * sizeof(*ctx->lpra));
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

static void inv_args(uint32_t sc, remote_arg64_t *rpra, int used)
{
	int i, inbufs, outbufs;
	int inv = 0;

	inbufs = REMOTE_SCALARS_INBUFS(sc);
	outbufs = REMOTE_SCALARS_OUTBUFS(sc);
	for (i = inbufs; i < inbufs + outbufs; ++i) {
		if (buf_page_start(ptr_to_uint64((void *)rpra)) ==
				buf_page_start(rpra[i].buf.pv))
			inv = 1;
		else if (rpra[i].buf.len)
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
	struct smq_msg msg = {0};
	struct fastrpc_file *fl = ctx->fl;
	int err = 0, len;
	msg.pid = current->tgid;
	msg.tid = current->pid;
	if (kernel)
		msg.pid = 0;
	msg.invoke.header.ctx = ptr_to_uint64(ctx);
	msg.invoke.header.handle = handle;
	msg.invoke.header.sc = ctx->sc;
	msg.invoke.page.addr = ctx->buf ? ctx->buf->phys : 0;
	msg.invoke.page.size = buf_page_size(ctx->used);
	if (msm_audio_ion_is_smmu_available()
		&& msg.invoke.page.addr != 0)
		msg.invoke.page.addr |= STREAM_ID;
	spin_lock(&fl->apps->hlock);
	len = smd_write(fl->chan->chan, &msg, sizeof(msg));
	spin_unlock(&fl->apps->hlock);
	VERIFY(err, len == sizeof(msg));
	return err;
}

static void fastrpc_deinit(void)
{
	struct fastrpc_apps *me = &gfa;
	int i;

	for (i = 0; i < NUM_CHANNELS; i++) {
		struct fastrpc_chan_ctx *chan = &me->channel[i];
		if (chan->chan) {
			(void)smd_close(chan->chan);
			chan->chan = 0;
		}
		if (chan->smmu.dev)
			chan->smmu.dev = 0;
		if (chan->smmu.mapping)
			chan->smmu.mapping = 0;
	}
}

static void fastrpc_read_handler(int cid)
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
		fastrpc_read_handler(cid);
		break;
	}
}

static void fastrpc_init(struct fastrpc_apps *me)
{
	int i;
	INIT_HLIST_HEAD(&me->drivers);
	INIT_HLIST_HEAD(&me->maps);
	spin_lock_init(&me->hlock);
	mutex_init(&me->smd_mutex);
	for (i = 0; i < NUM_CHANNELS; i++)
		init_completion(&me->channel[i].work);
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
		inv_args(ctx->sc, ctx->rpra, ctx->used);
	VERIFY(err, 0 == fastrpc_invoke_send(ctx, kernel, invoke->handle));
	if (err)
		goto bail;
	if (FASTRPC_MODE_PARALLEL == mode)
		inv_args(ctx->sc, ctx->rpra, ctx->used);
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
	int mflags = 0;
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
		struct {
			int pgid;
			int namelen;
			int filelen;
			int pageslen;
		} inbuf;
		inbuf.pgid = current->tgid;
		inbuf.namelen = strlen(current->comm);
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
		if (msm_audio_ion_is_smmu_available())
			pages[0].addr |= STREAM_ID;
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

	tgid = fl->tgid;
	ra[0].buf.pv = (void *)&tgid;
	ra[0].buf.len = sizeof(tgid);
	ioctl.inv.handle = 1;
	ioctl.inv.sc = REMOTE_SCALARS_MAKE(1, 1, 0);
	ioctl.inv.pra = ra;
	ioctl.fds = 0;
	VERIFY(err, 0 == (err = fastrpc_internal_invoke(fl,
		FASTRPC_MODE_PARALLEL, 1, &ioctl)));
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
	if (msm_audio_ion_is_smmu_available() && flags != ADSP_MMAP_HEAP_ADDR)
		page.addr |= STREAM_ID;
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
	int err = 0;
	remote_arg_t ra[1];
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
	struct fastrpc_mmap *match = 0, *map;
	struct hlist_node *n;
	int err = 0;
	struct fastrpc_apps *me = &gfa;
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

static int fastrpc_mmap_find(struct fastrpc_file *fl, int fd, uintptr_t va,
			ssize_t len, int mflags, struct fastrpc_mmap **ppmap)
{
	struct fastrpc_mmap *match = 0, *map;
	struct hlist_node *n;

	if (mflags == ADSP_MMAP_HEAP_ADDR) {
		struct fastrpc_apps *me = &gfa;
		spin_lock(&me->hlock);
		hlist_for_each_entry_safe(map, n, &me->maps, hn) {
			if (map->va >= va &&
					map->va + map->len <= va + len &&
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
			if (map->va >= va &&
					map->va + map->len <= va + len &&
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

static int fastrpc_mmap_create(struct fastrpc_file *fl, int fd, uintptr_t va,
			ssize_t len, int mflags, struct fastrpc_mmap **ppmap)
{
	int err = 0;
	struct fastrpc_mmap *map = 0;
	struct fastrpc_apps *me = &gfa;
	unsigned long ionflag = 0;
	size_t pa_len = 0;
	ion_phys_addr_t paddr = 0;
	void *virtual_addr = NULL;
	struct vm_area_struct *vma;
	unsigned long pfn;

	if (!fastrpc_mmap_find(fl, fd, va, len, mflags, ppmap))
		return 0;
	VERIFY(err, map = kzalloc(sizeof(*map), GFP_KERNEL));
	if (err)
		goto bail;
	map->flags = mflags;
	map->client = NULL;
	map->handle = NULL;
	map->fl = fl;
	map->fd = fd;
	VERIFY(err, !msm_audio_ion_import(DEVICE_NAME, &(map->client),
					&(map->handle), fd,
					&ionflag, len,
					&paddr, &pa_len, &virtual_addr));
	if (err) {
		pr_err("msm_audio_ion_import failed\n");
		goto bail;
	}
	if (mflags == ADSP_MMAP_HEAP_ADDR) {
		map->apps = me;
		map->fl = 0;
		VERIFY(err, 0 != (vma = find_vma(current->mm, va)));
		if (err)
			goto bail;
		VERIFY(err, 0 == follow_pfn(vma, va , &pfn));
		if (err)
			goto bail;
		map->phys = __pfn_to_phys(pfn);
		map->size = len;
	} else {
		map->phys = paddr;
		map->size = pa_len;
	}
	map->va = va;
	map->refs = 1;
	map->len = len;
	INIT_HLIST_NODE(&map->hn);
	fastrpc_mmap_add(map);

	*ppmap = map;

bail:
	if (err && map)
		fastrpc_mmap_free(map);
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

	VERIFY(err, !fastrpc_mmap_create(fl, ud->fd,
			(uintptr_t)ud->vaddrin, ud->size, ud->flags, &map));
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
	struct fastrpc_chan_ctx *ctx;
	int cid;

	ctx = container_of(kref, struct fastrpc_chan_ctx, kref);
	smd_close(ctx->chan);
	ctx->chan = 0;
	mutex_unlock(&me->smd_mutex);
	cid = ctx - &me->channel[0];
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
static int fastrpc_device_release(struct inode *inode, struct file *file)
{
	fastrpc_file_free((struct fastrpc_file *)file->private_data);
	file->private_data = 0;
	return 0;
}

static int fastrpc_device_open(struct inode *inode, struct file *filp)
{
	int cid = MINOR(inode->i_rdev);
	int err = 0;
	struct fastrpc_apps *me = &gfa;
	struct fastrpc_file *fl = 0;

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
	fl->chan = &me->channel[cid];
	fl->cid = cid;
	fl->tgid = current->tgid;
	fl->apps = me;

	fl->ssrcount = me->channel[cid].ssrcount;
	if ((kref_get_unless_zero(&me->channel[cid].kref) == 0) ||
	    (me->channel[cid].chan == 0)) {
		VERIFY(err, !smd_named_open_on_edge(FASTRPC_SMD_GUID,
						    gcinfo[cid].channel,
						    &me->channel[cid].chan,
						    (void *)(uintptr_t)cid,
						    smd_event_handler));
		if (err)
			goto bail;
		VERIFY(err, wait_for_completion_timeout(&me->channel[cid].work,
							 RPC_TIMEOUT));
		if (err)
			goto bail;
		kref_init(&me->channel[cid].kref);
		pr_info("'opened /dev/%s c %d %d'\n", gcinfo[cid].name,
						MAJOR(me->dev_no), cid);
		if (fastrpc_mmap_remove_ssr(fl))
			pr_err("ADSPRPC: SSR: Failed to unmap remote heap\n");
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
	struct fastrpc_chan_ctx *ctx;
	int cid;

	ctx = container_of(nb, struct fastrpc_chan_ctx, nb);
	cid = ctx - &me->channel[0];
	if (code == SUBSYS_BEFORE_SHUTDOWN) {
		mutex_lock(&me->smd_mutex);
		ctx->ssrcount++;
		if (ctx->chan) {
			smd_close(ctx->chan);
			ctx->chan = 0;
			pr_info("'closed /dev/%s c %d %d'\n", gcinfo[cid].name,
						MAJOR(me->dev_no), cid);
		}
		mutex_unlock(&me->smd_mutex);
		fastrpc_notify_drivers(me, cid);
	}

	return NOTIFY_DONE;
}

static const struct file_operations fops = {
	.open = fastrpc_device_open,
	.release = fastrpc_device_release,
	.unlocked_ioctl = fastrpc_device_ioctl,
	.compat_ioctl = compat_fastrpc_device_ioctl,
};

static int __init fastrpc_device_init(void)
{
	struct fastrpc_apps *me = &gfa;
	struct device_node *node;
	struct platform_device *pdev;
	struct iommu_group *group;
	struct iommu_domain *domain;
	int err = 0, i;

	memset(me, 0, sizeof(*me));

	fastrpc_init(me);
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
		me->channel[i].dev = device_create(me->class, NULL,
					MKDEV(MAJOR(me->dev_no), i),
					NULL, gcinfo[i].name);
		VERIFY(err, !IS_ERR(me->channel[i].dev));
		if (err)
			goto device_create_bail;
		me->channel[i].ssrcount = 0;
		me->channel[i].nb.notifier_call = fastrpc_restart_notifier_cb,
		(void)subsys_notif_register_notifier(gcinfo[i].subsys,
							&me->channel[i].nb);
		if (!gcinfo[i].node)
			continue;
		node = of_find_compatible_node(NULL, NULL, gcinfo[i].node);
		if (node) {
			pdev = of_find_device_by_node(node);
			if (pdev) {
				me->channel[i].smmu.dev = &pdev->dev;
				me->channel[i].smmu.enabled = 1;
				me->channel[i].smmu.cb = 1;
				dev_dbg(me->channel[i].dev,
					"%s: Using audio Context bank\n",
					__func__);
			}
		}
	}
	group = iommu_group_find("lpass_audio");
	if (!group) {
		pr_debug("Failed to find group lpass_audio deferred\n");
		err = -1;
		goto register_bail;
	}
	domain = iommu_group_get_iommudata(group);
	if (IS_ERR_OR_NULL(domain)) {
		pr_err("Failed to get domain data for group %p\n",
				group);
		err = -1;
		goto register_bail;
	}
	me->domain_id = msm_find_domain_no(domain);
	if (me->domain_id < 0) {
		pr_err("Failed to get domain index for domain %p\n",
				domain);
		err = -1;
		goto register_bail;
	}
	pr_debug("domain=%p, domain_id=%d, group=%p\n", domain,
			me->domain_id, group);

	return 0;

device_create_bail:
	class_destroy(me->class);
class_create_bail:
	cdev_del(&me->cdev);
cdev_init_bail:
	unregister_chrdev_region(me->dev_no, NUM_CHANNELS);
alloc_chrdev_bail:
	fastrpc_deinit();
register_bail:
	return err;
}

static void __exit fastrpc_device_exit(void)
{
	struct fastrpc_apps *me = &gfa;
	int i;

	fastrpc_file_list_dtor(me);
	fastrpc_deinit();
	for (i = 0; i < NUM_CHANNELS; i++) {
		device_destroy(me->class, MKDEV(MAJOR(me->dev_no), i));
		subsys_notif_unregister_notifier(gcinfo[i].subsys,
						&me->channel[i].nb);
	}
	class_destroy(me->class);
	cdev_del(&me->cdev);
	unregister_chrdev_region(me->dev_no, NUM_CHANNELS);
}

late_initcall(fastrpc_device_init);
module_exit(fastrpc_device_exit);

MODULE_LICENSE("GPL v2");
