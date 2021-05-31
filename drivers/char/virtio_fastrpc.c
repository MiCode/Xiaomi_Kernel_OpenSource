// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/cdev.h>
#include <linux/completion.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/dma-buf.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/ion.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/virtio.h>
#include <linux/virtio_config.h>
#include <linux/uaccess.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/subsystem_restart.h>
#include "adsprpc_compat.h"
#include "adsprpc_shared.h"

#define VIRTIO_ID_FASTRPC				34
/* indicates remote invoke with buffer attributes is supported */
#define VIRTIO_FASTRPC_F_INVOKE_ATTR			1
/* indicates remote invoke with CRC is supported */
#define VIRTIO_FASTRPC_F_INVOKE_CRC			2
/* indicates remote mmap/munmap is supported */
#define VIRTIO_FASTRPC_F_MMAP				3
/* indicates QOS setting is supported */
#define VIRTIO_FASTRPC_F_CONTROL			4
/* indicates smmu passthrough is supported */
#define VIRTIO_FASTRPC_F_SMMU_PASSTHROUGH		5

#define NUM_CHANNELS			4 /* adsp, mdsp, slpi, cdsp0*/
#define NUM_DEVICES			2 /* adsprpc-smd, adsprpc-smd-secure */
#define M_FDLIST			16
#define MINOR_NUM_DEV			0
#define MINOR_NUM_SECURE_DEV		1
#define ADSP_MMAP_HEAP_ADDR		4
#define ADSP_MMAP_REMOTE_HEAP_ADDR	8
#define FASTRPC_DMAHANDLE_NOMAP		16
#define ADSP_MMAP_ADD_PAGES		0x1000

#define INIT_FILELEN_MAX		(2*1024*1024)
#define INIT_MEMLEN_MAX			(8*1024*1024)
#define MAX_CACHE_BUF_SIZE		(8*1024*1024)

#define FASTRPC_MSG_MAX			256
#define MAX_FASTRPC_BUF_SIZE		(128*1024)
#define DEBUGFS_SIZE			3072
#define PID_SIZE			10
#define UL_SIZE				25

#define VIRTIO_FASTRPC_CMD_OPEN		1
#define VIRTIO_FASTRPC_CMD_CLOSE	2
#define VIRTIO_FASTRPC_CMD_INVOKE	3
#define VIRTIO_FASTRPC_CMD_MMAP		4
#define VIRTIO_FASTRPC_CMD_MUNMAP	5
#define VIRTIO_FASTRPC_CMD_CONTROL	6

#define ADSP_DOMAIN_ID			0
#define MDSP_DOMAIN_ID			1
#define SDSP_DOMAIN_ID			1
#define CDSP_DOMAIN_ID			3

#define STATIC_PD			0
#define DYNAMIC_PD			1
#define GUEST_OS			2

#define K_COPY_FROM_USER(err, kernel, dst, src, size) \
	do {\
		if (!(kernel))\
			VERIFY(err, 0 == copy_from_user((dst),\
			(void const __user *)(src),\
							(size)));\
		else\
			memmove((dst), (src), (size));\
	} while (0)

#define K_COPY_TO_USER(err, kernel, dst, src, size) \
	do {\
		if (!(kernel))\
			VERIFY(err, 0 == copy_to_user((void __user *)(dst),\
						(src), (size)));\
		else\
			memmove((dst), (src), (size));\
	} while (0)

#define FASTRPC_STATIC_HANDLE_KERNEL	1
#define FASTRPC_STATIC_HANDLE_LISTENER	3
#define FASTRPC_STATIC_HANDLE_MAX	20

struct virt_msg_hdr {
	u32 pid;	/* GVM pid */
	u32 tid;	/* GVM tid */
	s32 cid;	/* channel id connected to DSP */
	u32 cmd;	/* command type */
	u16 len;	/* command length */
	u16 msgid;	/* unique message id */
	u32 result;	/* message return value */
} __packed;

struct virt_fastrpc_msg {
	struct completion work;
	u16 msgid;
	void *txbuf;
	void *rxbuf;
};

struct virt_open_msg {
	struct virt_msg_hdr hdr;	/* virtio fastrpc message header */
	u32 domain;			/* DSP domain id */
	u32 pd;				/* DSP PD */
} __packed;

struct virt_control_msg {
	struct virt_msg_hdr hdr;	/* virtio fastrpc message header */
	u32 enable;			/* latency control enable */
	u32 latency;			/* latency value */
} __packed;

struct virt_fastrpc_buf {
	u64 pv;		/* buffer physical address, 0 for non-ION buffer */
	u64 len;	/* buffer length */
};

struct virt_fastrpc_dmahandle {
	u32 fd;
	u32 offset;
};

struct virt_invoke_msg {
	struct virt_msg_hdr hdr;	/* virtio fastrpc message header */
	u32 handle;			/* remote handle */
	u32 sc;				/* scalars describing the data */
	struct virt_fastrpc_buf pra[0];	/* remote arguments list */
} __packed;

struct virt_mmap_msg {
	struct virt_msg_hdr hdr;	/* virtio fastrpc message header */
	u32 nents;                      /* number of map entries */
	u32 flags;			/* mmap flags */
	u64 size;			/* mmap length */
	u64 vapp;			/* application virtual address */
	u64 vdsp;			/* dsp address */
	struct virt_fastrpc_buf sgl[0]; /* sg list */
} __packed;

struct virt_munmap_msg {
	struct virt_msg_hdr hdr;	/* virtio fastrpc message header */
	u64 vdsp;			/* dsp address */
	u64 size;			/* mmap length */
} __packed;

struct virt_fastrpc_vq {
	/* protects vq */
	spinlock_t vq_lock;
	struct virtqueue *vq;
};

struct fastrpc_ctx_lst {
	struct hlist_head pending;
	struct hlist_head interrupted;
};

struct fastrpc_invoke_ctx {
	struct virt_fastrpc_msg *msg;
	size_t size;
	struct fastrpc_buf_desc *desc;
	struct hlist_node hn;
	struct fastrpc_mmap **maps;
	remote_arg_t *lpra;
	int *fds;
	unsigned int outbufs_offset;
	unsigned int *attrs;
	struct fastrpc_file *fl;
	int pid;
	int tgid;
	uint32_t sc;
	uint32_t handle;
};

struct fastrpc_apps {
	struct virtio_device *vdev;
	struct virt_fastrpc_vq rvq;
	struct virt_fastrpc_vq svq;
	void *rbufs;
	void *sbufs;
	unsigned int order;
	unsigned int num_bufs;
	unsigned int buf_size;
	unsigned int num_channels;
	int last_sbuf;

	bool has_invoke_attr;
	bool has_invoke_crc;
	bool has_mmap;
	bool has_control;

	struct device *dev;
	struct cdev cdev;
	struct class *class;
	dev_t dev_no;

	spinlock_t msglock;
	struct virt_fastrpc_msg *msgtable[FASTRPC_MSG_MAX];
};

struct fastrpc_file {
	spinlock_t hlock;
	struct hlist_head maps;
	struct hlist_head cached_bufs;
	struct hlist_head remote_bufs;
	struct fastrpc_ctx_lst clst;
	uint32_t mode;
	int tgid;
	int cid;
	int domain;
	int pd;
	int file_close;
	int dsp_proc_init;
	struct fastrpc_apps *apps;
	struct dentry *debugfs_file;
	struct mutex map_mutex;
	/* Identifies the device (MINOR_NUM_DEV / MINOR_NUM_SECURE_DEV) */
	int dev_minor;
	char *debug_buf;
};

struct fastrpc_mmap {
	struct hlist_node hn;
	struct fastrpc_file *fl;
	int fd;
	uint32_t flags;
	struct dma_buf *buf;
	struct sg_table *table;
	struct dma_buf_attachment *attach;
	uint64_t phys;
	size_t size;
	uintptr_t va;
	size_t len;
	uintptr_t raddr;
	int refs;
};

struct fastrpc_buf {
	struct hlist_node hn;
	struct hlist_node hn_rem;
	struct fastrpc_file *fl;
	size_t size;
	struct sg_table sgt;
	struct page **pages;
	void *va;
	unsigned long dma_attr;
	uintptr_t raddr;
	uint32_t flags;
	int remote;
};

enum fastrpc_buf_type {
	FASTRPC_BUF_TYPE_NORMAL,
	FASTRPC_BUF_TYPE_ION,
	FASTRPC_BUF_TYPE_INTERNAL,
};

struct fastrpc_buf_desc {
	enum fastrpc_buf_type type;
	struct fastrpc_buf *buf;
};

static struct fastrpc_apps gfa;
static struct dentry *debugfs_root;
static int virt_fastrpc_close(struct fastrpc_file *fl);
static int fastrpc_buf_alloc(struct fastrpc_file *fl, size_t size,
				unsigned long dma_attr, uint32_t rflags,
				int remote, struct fastrpc_buf **obuf);
static void fastrpc_buf_free(struct fastrpc_buf *buf, int cache);
static void context_free(struct fastrpc_invoke_ctx *ctx);

static inline int64_t getnstimediff(struct timespec64 *start)
{
	int64_t ns;
	struct timespec64 ts, b;

	ktime_get_real_ts64(&ts);
	b = timespec64_sub(ts, *start);
	ns = timespec64_to_ns(&b);
	return ns;
}
static void virt_init_vq(struct virt_fastrpc_vq *fastrpc_vq,
				struct virtqueue *vq)
{
	spin_lock_init(&fastrpc_vq->vq_lock);
	fastrpc_vq->vq = vq;
}

static void *get_a_tx_buf(void)
{
	struct fastrpc_apps *me = &gfa;
	unsigned int len;
	void *ret;
	unsigned long flags;

	/* support multiple concurrent senders */
	spin_lock_irqsave(&me->svq.vq_lock, flags);
	/*
	 * either pick the next unused tx buffer
	 * (half of our buffers are used for sending messages)
	 */
	if (me->last_sbuf < me->num_bufs / 2)
		ret = me->sbufs + me->buf_size * me->last_sbuf++;
	/* or recycle a used one */
	else
		ret = virtqueue_get_buf(me->svq.vq, &len);
	spin_unlock_irqrestore(&me->svq.vq_lock, flags);
	return ret;
}

static struct virt_fastrpc_msg *virt_alloc_msg(int size)
{
	struct fastrpc_apps *me = &gfa;
	struct virt_fastrpc_msg *msg;
	void *buf;
	unsigned long flags;
	int i;

	if (size > me->buf_size) {
		dev_err(me->dev, "message is too big (%d)\n", size);
		return NULL;
	}

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	if (!msg)
		return NULL;

	buf = get_a_tx_buf();
	if (!buf) {
		dev_err(me->dev, "can't get tx buffer\n");
		kfree(msg);
		return NULL;
	}

	msg->txbuf = buf;
	init_completion(&msg->work);
	spin_lock_irqsave(&me->msglock, flags);
	for (i = 0; i < FASTRPC_MSG_MAX; i++) {
		if (!me->msgtable[i]) {
			me->msgtable[i] = msg;
			msg->msgid = i;
			break;
		}
	}
	spin_unlock_irqrestore(&me->msglock, flags);

	if (i == FASTRPC_MSG_MAX) {
		dev_err(me->dev, "message queue is full\n");
		kfree(msg);
		return NULL;
	}
	return msg;
}

static void virt_free_msg(struct virt_fastrpc_msg *msg)
{
	struct fastrpc_apps *me = &gfa;
	unsigned long flags;

	spin_lock_irqsave(&me->msglock, flags);
	if (me->msgtable[msg->msgid] == msg)
		me->msgtable[msg->msgid] = NULL;
	else
		dev_err(me->dev, "can't find msg %d in table\n", msg->msgid);
	spin_unlock_irqrestore(&me->msglock, flags);

	kfree(msg);
}

static void fastrpc_mmap_add(struct fastrpc_mmap *map)
{
	if (map->flags == ADSP_MMAP_HEAP_ADDR ||
				map->flags == ADSP_MMAP_REMOTE_HEAP_ADDR) {
		struct fastrpc_apps *me = &gfa;

		dev_err(me->dev, "%s ADSP_MMAP_HEAP_ADDR is not supported\n",
				__func__);
	} else {
		struct fastrpc_file *fl = map->fl;

		hlist_add_head(&map->hn, &fl->maps);
	}
}

static void fastrpc_mmap_free(struct fastrpc_mmap *map, uint32_t flags)
{
	struct fastrpc_apps *me = &gfa;

	if (!map)
		return;

	if (map->flags == ADSP_MMAP_HEAP_ADDR ||
				map->flags == ADSP_MMAP_REMOTE_HEAP_ADDR) {
		dev_err(me->dev, "%s ADSP_MMAP_HEAP_ADDR is not supported\n",
				__func__);
	} else {
		map->refs--;
		if (!map->refs)
			hlist_del_init(&map->hn);
		if (map->refs > 0 && !flags)
			return;
	}
	if (!IS_ERR_OR_NULL(map->table))
		dma_buf_unmap_attachment(map->attach, map->table,
				DMA_BIDIRECTIONAL);
	if (!IS_ERR_OR_NULL(map->attach))
		dma_buf_detach(map->buf, map->attach);
	if (!IS_ERR_OR_NULL(map->buf))
		dma_buf_put(map->buf);

	kfree(map);
}

static int fastrpc_mmap_find(struct fastrpc_file *fl, int fd,
		uintptr_t va, size_t len, int mflags, int refs,
		struct fastrpc_mmap **ppmap)
{
	struct fastrpc_apps *me = &gfa;
	struct fastrpc_mmap *match = NULL, *map = NULL;
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

static int fastrpc_mmap_remove(struct fastrpc_file *fl, uintptr_t va,
			       size_t len, struct fastrpc_mmap **ppmap)
{
	struct fastrpc_mmap *match = NULL, *map;
	struct hlist_node *n;

	hlist_for_each_entry_safe(map, n, &fl->maps, hn) {
		if (map->raddr == va &&
			map->raddr + map->len == va + len) {
			match = map;
			hlist_del_init(&map->hn);
			break;
		}
	}
	if (match) {
		*ppmap = match;
		return 0;
	}
	return -ENOTTY;
}

static int fastrpc_mmap_create(struct fastrpc_file *fl, int fd,
	uintptr_t va, size_t len, int mflags, struct fastrpc_mmap **ppmap)
{
	struct fastrpc_apps *me = &gfa;
	struct fastrpc_mmap *map = NULL;
	int err = 0, sgl_index = 0;
	unsigned long flags;
	struct scatterlist *sgl = NULL;

	if (!fastrpc_mmap_find(fl, fd, va, len, mflags, 1, ppmap))
		return 0;

	map = kzalloc(sizeof(*map), GFP_KERNEL);
	VERIFY(err, !IS_ERR_OR_NULL(map));
	if (err)
		goto bail;

	INIT_HLIST_NODE(&map->hn);
	map->flags = mflags;
	map->refs = 1;
	map->fl = fl;
	map->fd = fd;
	if (mflags == ADSP_MMAP_HEAP_ADDR ||
			mflags == ADSP_MMAP_REMOTE_HEAP_ADDR) {
		dev_err(me->dev, "%s ADSP_MMAP_HEAP_ADDR is not supported\n",
				__func__);
		err = -EINVAL;
		goto bail;
	} else {
		VERIFY(err, !IS_ERR_OR_NULL(map->buf = dma_buf_get(fd)));
		if (err) {
			dev_err(me->dev, "can't get dma buf fd %d\n", fd);
			goto bail;
		}
		VERIFY(err, !dma_buf_get_flags(map->buf, &flags));
		if (err) {
			dev_err(me->dev, "can't get dma buf flags %d\n", fd);
			goto bail;
		}

		VERIFY(err, !IS_ERR_OR_NULL(map->attach =
					dma_buf_attach(map->buf, me->dev)));
		if (err) {
			dev_err(me->dev, "can't attach dma buf\n");
			goto bail;
		}

		if (!(flags & ION_FLAG_CACHED))
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
	fastrpc_mmap_add(map);
	*ppmap = map;
bail:
	if (err && map)
		fastrpc_mmap_free(map, 0);
	return err;
}

static int virt_fastrpc_invoke(struct fastrpc_invoke_ctx *ctx)
{
	struct fastrpc_apps *me = &gfa;
	struct virt_fastrpc_msg *msg = ctx->msg;
	struct virt_invoke_msg *vmsg;
	struct scatterlist sg[1];
	unsigned long flags;
	int err = 0;

	if (!msg) {
		dev_err(me->dev, "%s: ctx msg is NULL\n", __func__);
		err = -EINVAL;
		goto bail;
	}
	vmsg = (struct virt_invoke_msg *)msg->txbuf;
	if (!vmsg) {
		dev_err(me->dev, "%s: invoke msg is NULL\n", __func__);
		err = -EINVAL;
		goto bail;
	}

	sg_init_one(sg, vmsg, ctx->size);

	spin_lock_irqsave(&me->svq.vq_lock, flags);
	err = virtqueue_add_outbuf(me->svq.vq, sg, 1, vmsg, GFP_KERNEL);
	if (err) {
		dev_err(me->dev, "%s: fail to add output buffer\n", __func__);
		spin_unlock_irqrestore(&me->svq.vq_lock, flags);
		goto bail;
	}

	virtqueue_kick(me->svq.vq);
	spin_unlock_irqrestore(&me->svq.vq_lock, flags);
bail:
	return err;
}

static int context_restore_interrupted(struct fastrpc_file *fl,
					struct fastrpc_ioctl_invoke *invoke,
					struct fastrpc_invoke_ctx **po)
{
	int err = 0;
	struct fastrpc_apps *me = fl->apps;
	struct fastrpc_invoke_ctx *ctx = NULL, *ictx = NULL;
	struct hlist_node *n;

	spin_lock(&fl->hlock);
	hlist_for_each_entry_safe(ictx, n, &fl->clst.interrupted, hn) {
		if (ictx->pid == current->pid) {
			if (invoke->sc != ictx->sc || ictx->fl != fl) {
				err = -EINVAL;
				dev_err(me->dev,
					"interrupted sc (0x%x) or fl (%pK) does not match with invoke sc (0x%x) or fl (%pK)\n",
					ictx->sc, ictx->fl, invoke->sc, fl);
			} else {
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

static int context_alloc(struct fastrpc_file *fl,
			struct fastrpc_ioctl_invoke_crc *invokefd,
			struct fastrpc_invoke_ctx **po)
{
	int err = 0, bufs, size = 0;
	struct fastrpc_invoke_ctx *ctx = NULL;
	struct fastrpc_ctx_lst *clst = &fl->clst;
	struct fastrpc_ioctl_invoke *invoke = &invokefd->inv;

	bufs = REMOTE_SCALARS_LENGTH(invoke->sc);
	size = bufs * sizeof(*ctx->lpra) + bufs * sizeof(*ctx->maps) +
		sizeof(*ctx->fds) * (bufs) +
		sizeof(*ctx->attrs) * (bufs);

	VERIFY(err, NULL != (ctx = kzalloc(sizeof(*ctx) + size, GFP_KERNEL)));
	if (err)
		goto bail;

	INIT_HLIST_NODE(&ctx->hn);
	hlist_add_fake(&ctx->hn);
	ctx->fl = fl;
	ctx->maps = (struct fastrpc_mmap **)(&ctx[1]);
	ctx->lpra = (remote_arg_t *)(&ctx->maps[bufs]);
	ctx->fds = (int *)(&ctx->lpra[bufs]);
	ctx->attrs = (unsigned int *)(&ctx->fds[bufs]);

	K_COPY_FROM_USER(err, 0, (void *)ctx->lpra, invoke->pra,
			bufs * sizeof(*ctx->lpra));
	if (err)
		goto bail;

	if (invokefd->fds) {
		K_COPY_FROM_USER(err, 0, ctx->fds, invokefd->fds,
				bufs * sizeof(*ctx->fds));
		if (err)
			goto bail;
	} else {
		ctx->fds = NULL;
	}
	if (invokefd->attrs) {
		K_COPY_FROM_USER(err, 0, ctx->attrs, invokefd->attrs,
				bufs * sizeof(*ctx->attrs));
		if (err)
			goto bail;
	}
	ctx->sc = invoke->sc;
	ctx->handle = invoke->handle;
	ctx->pid = current->pid;
	ctx->tgid = fl->tgid;

	spin_lock(&fl->hlock);
	hlist_add_head(&ctx->hn, &clst->pending);
	spin_unlock(&fl->hlock);

	*po = ctx;
bail:
	if (ctx && err)
		context_free(ctx);
	return err;
}

static void context_save_interrupted(struct fastrpc_invoke_ctx *ctx)
{
	struct fastrpc_ctx_lst *clst = &ctx->fl->clst;

	spin_lock(&ctx->fl->hlock);
	hlist_del_init(&ctx->hn);
	hlist_add_head(&ctx->hn, &clst->interrupted);
	spin_unlock(&ctx->fl->hlock);
}

static void context_free(struct fastrpc_invoke_ctx *ctx)
{
	int i;
	struct fastrpc_file *fl = ctx->fl;
	struct scatterlist sg[1];
	struct fastrpc_apps *me = &gfa;
	struct virt_invoke_msg *rsp = NULL;
	unsigned long flags;
	int nbufs = REMOTE_SCALARS_INBUFS(ctx->sc) +
			REMOTE_SCALARS_OUTBUFS(ctx->sc);

	spin_lock(&fl->hlock);
	hlist_del_init(&ctx->hn);
	spin_unlock(&fl->hlock);

	mutex_lock(&fl->map_mutex);
	for (i = 0; i < nbufs; i++)
		fastrpc_mmap_free(ctx->maps[i], 0);
	mutex_unlock(&fl->map_mutex);

	if (ctx->msg) {
		rsp = ctx->msg->rxbuf;
		if (rsp) {
			sg_init_one(sg, rsp, me->buf_size);

			spin_lock_irqsave(&me->rvq.vq_lock, flags);
			/* add the buffer back to the remote processor's virtqueue */
			if (virtqueue_add_inbuf(me->rvq.vq, sg, 1, rsp, GFP_KERNEL))
				dev_err(me->dev,
					"%s: fail to add input buffer\n", __func__);
			else
				virtqueue_kick(me->rvq.vq);
			spin_unlock_irqrestore(&me->rvq.vq_lock, flags);
		}

		virt_free_msg(ctx->msg);
		ctx->msg = NULL;
	}
	if (ctx->desc) {
		for (i = 0; i < nbufs; i++) {
			if (ctx->desc[i].buf)
				fastrpc_buf_free(ctx->desc[i].buf, 1);
		}
		kfree(ctx->desc);
		ctx->desc = NULL;
	}

	kfree(ctx);
}

static void context_list_ctor(struct fastrpc_ctx_lst *me)
{
	INIT_HLIST_HEAD(&me->interrupted);
	INIT_HLIST_HEAD(&me->pending);
}

static void fastrpc_context_list_dtor(struct fastrpc_file *fl)
{
	struct fastrpc_ctx_lst *clst = &fl->clst;
	struct fastrpc_invoke_ctx *ictx = NULL, *ctxfree;
	struct hlist_node *n;

	do {
		ctxfree = NULL;
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
		ctxfree = NULL;
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

static int get_args(struct fastrpc_invoke_ctx *ctx)
{
	struct fastrpc_file *fl = ctx->fl;
	struct fastrpc_apps *me = fl->apps;
	struct virt_invoke_msg *vmsg;
	int inbufs = REMOTE_SCALARS_INBUFS(ctx->sc);
	int outbufs = REMOTE_SCALARS_OUTBUFS(ctx->sc);
	int i, err = 0, bufs, handles, total;
	remote_arg_t *lpra = ctx->lpra;
	int *fds = ctx->fds;
	struct virt_fastrpc_buf *rpra;
	struct virt_fastrpc_dmahandle *handle;
	uint64_t *fdlist;
	unsigned int *attrs = ctx->attrs;
	struct fastrpc_mmap **maps = ctx->maps;
	size_t copylen = 0, size = 0, handle_len = 0, metalen;
	char *payload;

	bufs = inbufs + outbufs;
	handles = REMOTE_SCALARS_INHANDLES(ctx->sc)
		+ REMOTE_SCALARS_OUTHANDLES(ctx->sc);
	total = REMOTE_SCALARS_LENGTH(ctx->sc);

	/* calculate len required for copying */
	for (i = 0; i < bufs; i++) {
		size_t len = lpra[i].buf.len;

		if (!len)
			continue;
		if (fds && (fds[i] != -1)) {
			/* map ion buffers */
			mutex_lock(&fl->map_mutex);
			err = fastrpc_mmap_create(fl, fds[i],
					(uintptr_t)lpra[i].buf.pv,
					len, 0, &maps[i]);
			mutex_unlock(&fl->map_mutex);
			if (err)
				goto bail;
			len = maps[i]->table->nents *
				sizeof(struct virt_fastrpc_buf);
		}
		copylen += len;
		if (i < inbufs)
			ctx->outbufs_offset += len;
	}

	mutex_lock(&fl->map_mutex);
	for (i = bufs; i < total; i++) {
		int dmaflags = 0;

		if (attrs && (attrs[i] & FASTRPC_ATTR_NOMAP))
			dmaflags = FASTRPC_DMAHANDLE_NOMAP;
		if (fds && (fds[i] != -1)) {
			err = fastrpc_mmap_create(fl, fds[i],
					0, 0, dmaflags, &maps[i]);
			if (err) {
				mutex_unlock(&fl->map_mutex);
				goto bail;
			}
			handle_len += maps[i]->table->nents *
					sizeof(struct virt_fastrpc_buf);
		}
	}
	mutex_unlock(&fl->map_mutex);

	metalen = sizeof(*vmsg) + total * sizeof(*rpra)
		+ handles * sizeof(struct virt_fastrpc_dmahandle)
		+ sizeof(uint64_t) * M_FDLIST;
	size = metalen + copylen + handle_len;
	if (size > me->buf_size) {
		/* if user buffer contents exceed virtio buffer limits,
		 * try to alloc an internal buffer to copy
		 */
		copylen = 0;
		ctx->outbufs_offset = 0;
		ctx->desc = kcalloc(bufs, sizeof(*ctx->desc), GFP_KERNEL);
		if (!ctx->desc) {
			err = -ENOMEM;
			goto bail;
		}
		for (i = 0; i < bufs; i++) {
			size_t len = lpra[i].buf.len;

			if (maps[i]) {
				len = maps[i]->table->nents *
					sizeof(struct virt_fastrpc_buf);
				ctx->desc[i].type = FASTRPC_BUF_TYPE_ION;
			} else if (len < PAGE_SIZE) {
				ctx->desc[i].type = FASTRPC_BUF_TYPE_NORMAL;
			} else {
				ctx->desc[i].type = FASTRPC_BUF_TYPE_INTERNAL;
				len = PAGE_ALIGN(len);
				err = fastrpc_buf_alloc(fl, len, 0,
						0, 0, &ctx->desc[i].buf);
				if (err)
					goto bail;
				len = ctx->desc[i].buf->sgt.nents *
					sizeof(struct virt_fastrpc_buf);
			}
			copylen += len;
			if (i < inbufs)
				ctx->outbufs_offset += len;
		}
		size = metalen + copylen + handle_len;
	}

	ctx->msg = virt_alloc_msg(size);
	if (!ctx->msg) {
		err = -ENOMEM;
		goto bail;
	}

	ctx->size = size;
	vmsg = (struct virt_invoke_msg *)ctx->msg->txbuf;
	vmsg->hdr.pid = fl->tgid;
	vmsg->hdr.tid = current->pid;
	vmsg->hdr.cid = fl->cid;
	vmsg->hdr.cmd = VIRTIO_FASTRPC_CMD_INVOKE;
	vmsg->hdr.len = size;
	vmsg->hdr.msgid = ctx->msg->msgid;
	vmsg->hdr.result = 0xffffffff;
	vmsg->handle = ctx->handle;
	vmsg->sc = ctx->sc;
	rpra = (struct virt_fastrpc_buf *)vmsg->pra;
	handle = (struct virt_fastrpc_dmahandle *)&rpra[total];
	fdlist = (uint64_t *)&handle[handles];
	payload = (char *)&fdlist[M_FDLIST];

	for (i = 0; i < M_FDLIST; i++)
		fdlist[i] = 0;

	for (i = 0; i < bufs; i++) {
		size_t len = lpra[i].buf.len;
		struct sg_table *table;
		struct virt_fastrpc_buf *sgbuf;
		struct scatterlist *sgl = NULL;
		int index = 0;

		if (maps[i]) {
			table = maps[i]->table;
			rpra[i].pv = len;
			rpra[i].len = table->nents *
				sizeof(struct virt_fastrpc_buf);
			sgbuf = (struct virt_fastrpc_buf *)payload;
			for_each_sg(table->sgl, sgl, table->nents, index) {
				sgbuf[index].pv = sg_dma_address(sgl);
				sgbuf[index].len = sg_dma_len(sgl);
			}
			payload += rpra[i].len;
		} else if (ctx->desc &&
			   ctx->desc[i].type == FASTRPC_BUF_TYPE_INTERNAL) {
			table = &ctx->desc[i].buf->sgt;
			rpra[i].pv = len;
			rpra[i].len = table->nents *
				sizeof(struct virt_fastrpc_buf);
			sgbuf = (struct virt_fastrpc_buf *)payload;
			for_each_sg(table->sgl, sgl, table->nents, index) {
				sgbuf[index].pv = page_to_phys(sg_page(sgl));
				sgbuf[index].len = sgl->length;
			}
			if (i < inbufs && len) {
				K_COPY_FROM_USER(err, 0, ctx->desc[i].buf->va,
						lpra[i].buf.pv, len);
				if (err)
					goto bail;

			}
			payload += rpra[i].len;
		} else {
			/* copy non ion buffers */
			rpra[i].pv = 0;
			rpra[i].len = len;
			if (i < inbufs && len) {
				K_COPY_FROM_USER(err, 0, payload,
						lpra[i].buf.pv, len);
				if (err)
					goto bail;
			}
			payload += len;
		}
	}

	for (i = bufs; i < total; i++) {
		struct sg_table *table;
		struct virt_fastrpc_buf *sgbuf;
		struct scatterlist *sgl = NULL;
		int index = 0, hlist;

		if (fds && maps[i]) {
			/* fill in dma handle list */
			hlist = i - bufs;
			handle[hlist].fd = fds[i];
			handle[hlist].offset = (u32)lpra[i].buf.pv;
			/* copy dma handle sglist to data area */
			table = maps[i]->table;
			rpra[i].pv = lpra[i].buf.len;
			rpra[i].len = table->nents *
				sizeof(struct virt_fastrpc_buf);
			sgbuf = (struct virt_fastrpc_buf *)payload;
			for_each_sg(table->sgl, sgl, table->nents, index) {
				sgbuf[index].pv = sg_dma_address(sgl);
				sgbuf[index].len = sg_dma_len(sgl);
			}
			payload += rpra[i].len;
		}
	}
bail:
	return err;
}

static int put_args(struct fastrpc_invoke_ctx *ctx)
{
	int err = 0;
	struct fastrpc_apps *me = &gfa;
	struct fastrpc_file *fl = ctx->fl;
	struct virt_fastrpc_msg *msg = ctx->msg;
	struct virt_invoke_msg *rsp = NULL;
	int inbufs = REMOTE_SCALARS_INBUFS(ctx->sc);
	int outbufs = REMOTE_SCALARS_OUTBUFS(ctx->sc);
	int i, bufs, handles, total;
	remote_arg_t *lpra = ctx->lpra;
	struct virt_fastrpc_buf *rpra;
	struct virt_fastrpc_dmahandle *handle;
	uint64_t *fdlist;
	struct fastrpc_mmap **maps = ctx->maps, *mmap = NULL;
	char *payload;

	if (!msg) {
		dev_err(me->dev, "%s: ctx msg is NULL\n", __func__);
		err = -EINVAL;
		goto bail;
	}
	rsp = msg->rxbuf;
	if (!rsp) {
		dev_err(me->dev, "%s: response invoke msg is NULL\n", __func__);
		err = -EINVAL;
		goto bail;
	}
	err = rsp->hdr.result;
	if (err)
		goto bail;

	bufs = inbufs + outbufs;
	handles = REMOTE_SCALARS_INHANDLES(ctx->sc)
		+ REMOTE_SCALARS_OUTHANDLES(ctx->sc);
	total = REMOTE_SCALARS_LENGTH(ctx->sc);

	rpra = (struct virt_fastrpc_buf *)rsp->pra;
	handle = (struct virt_fastrpc_dmahandle *)&rpra[total];
	fdlist = (uint64_t *)&handle[handles];
	payload = (char *)&fdlist[M_FDLIST] + ctx->outbufs_offset;

	for (i = inbufs; i < bufs; i++) {
		if (maps[i]) {
			mutex_lock(&fl->map_mutex);
			fastrpc_mmap_free(maps[i], 0);
			mutex_unlock(&fl->map_mutex);
			maps[i] = NULL;
		} else if (ctx->desc &&
			   ctx->desc[i].type == FASTRPC_BUF_TYPE_INTERNAL) {
			K_COPY_TO_USER(err, 0, lpra[i].buf.pv,
					ctx->desc[i].buf->va, lpra[i].buf.len);
			if (err)
				goto bail;
		} else {
			K_COPY_TO_USER(err, 0, lpra[i].buf.pv,
					payload, rpra[i].len);
			if (err)
				goto bail;
		}
		payload += rpra[i].len;
	}

	mutex_lock(&fl->map_mutex);
	if (total) {
		for (i = 0; i < M_FDLIST; i++) {
			if (!fdlist[i])
				break;
			if (!fastrpc_mmap_find(fl, (int)fdlist[i], 0, 0,
						0, 0, &mmap))
				fastrpc_mmap_free(mmap, 0);
		}
	}
	mutex_unlock(&fl->map_mutex);
bail:
	return err;
}

static int fastrpc_internal_invoke(struct fastrpc_file *fl,
			uint32_t mode, struct fastrpc_ioctl_invoke_crc *inv)
{
	struct fastrpc_ioctl_invoke *invoke = &inv->inv;
	struct fastrpc_apps *me = fl->apps;
	struct fastrpc_invoke_ctx *ctx = NULL;
	int err = 0, interrupted = 0;

	VERIFY(err, invoke->handle != FASTRPC_STATIC_HANDLE_KERNEL);
	if (err) {
		dev_err(me->dev, "user application %s trying to send a kernel RPC message to channel %d\n",
			current->comm, fl->domain);
		goto bail;
	}

	VERIFY(err, fl->domain >= 0 && fl->domain < me->num_channels);
	if (err) {
		dev_err(me->dev, "user application %s domain is not set\n",
				current->comm);
		err = -EBADR;
		goto bail;
	}

	VERIFY(err, 0 == context_restore_interrupted(fl, invoke, &ctx));
	if (err)
		goto bail;
	if (ctx)
		goto wait;

	VERIFY(err, 0 == context_alloc(fl, inv, &ctx));
	if (err)
		goto bail;

	VERIFY(err, 0 == get_args(ctx));
	if (err)
		goto bail;

	VERIFY(err, 0 == virt_fastrpc_invoke(ctx));

	if (err)
		goto bail;

wait:
	interrupted = wait_for_completion_interruptible(&ctx->msg->work);
	VERIFY(err, 0 == (err = interrupted));
	if (err)
		goto bail;
	VERIFY(err, 0 == put_args(ctx));
	if (err)
		goto bail;
bail:
	if (ctx && interrupted == -ERESTARTSYS)
		context_save_interrupted(ctx);
	else if (ctx)
		context_free(ctx);

	return err;
}

static ssize_t fastrpc_debugfs_read(struct file *filp, char __user *buffer,
					 size_t count, loff_t *position)
{
	struct fastrpc_file *fl = filp->private_data;
	struct fastrpc_buf *buf = NULL;
	struct hlist_node *n;
	struct fastrpc_invoke_ctx *ictx = NULL;
	char *fileinfo = NULL;
	unsigned int len = 0;
	int err = 0;
	char single_line[UL_SIZE] = "----------------";
	char title[UL_SIZE] = "=========================";

	fileinfo = kzalloc(DEBUGFS_SIZE, GFP_KERNEL);
	if (!fileinfo)
		goto bail;
	if (fl) {
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
				"\n%s %d\n", "CHANNEL =", fl->domain);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"\n======%s %s %s======\n", title,
			" LIST OF BUFS ", title);
		spin_lock(&fl->hlock);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%-19s|%-19s|%-19s\n",
			"virt", "phys", "size");
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%s%s%s%s%s\n", single_line, single_line,
			single_line, single_line, single_line);
		hlist_for_each_entry_safe(buf, n, &fl->cached_bufs, hn) {
			len += scnprintf(fileinfo + len,
				DEBUGFS_SIZE - len,
				"0x%-17lX|0x%-17llX|%-19zu\n",
				(unsigned long)buf->va,
				(uint64_t)page_to_phys(buf->pages[0]),
				buf->size);
		}

		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"\n%s %s %s\n", title,
			" LIST OF PENDING CONTEXTS ", title);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%-20s|%-10s|%-10s|%-10s|%-20s\n",
			"sc", "pid", "tgid", "size", "handle");
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%s%s%s%s%s\n", single_line, single_line,
			single_line, single_line, single_line);
		hlist_for_each_entry_safe(ictx, n, &fl->clst.pending, hn) {
			len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
				"0x%-18X|%-10d|%-10d|%-10zu|0x%-20X\n\n",
				ictx->sc, ictx->pid, ictx->tgid,
				ictx->size, ictx->handle);
		}

		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"\n%s %s %s\n", title,
			" LIST OF INTERRUPTED CONTEXTS ", title);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%-20s|%-10s|%-10s|%-10s|%-20s\n",
			"sc", "pid", "tgid", "size", "handle");
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%s%s%s%s%s\n", single_line, single_line,
			single_line, single_line, single_line);
		hlist_for_each_entry_safe(ictx, n, &fl->clst.interrupted, hn) {
			len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
				"0x%-18X|%-10d|%-10d|%-10zu|0x%-20X\n\n",
				ictx->sc, ictx->pid, ictx->tgid,
				ictx->size, ictx->handle);
		}
		spin_unlock(&fl->hlock);
	}

	if (len > DEBUGFS_SIZE)
		len = DEBUGFS_SIZE;
	err = simple_read_from_buffer(buffer, count, position, fileinfo, len);
	kfree(fileinfo);
bail:
	return err;
}

static const struct file_operations debugfs_fops = {
	.open = simple_open,
	.read = fastrpc_debugfs_read,
};

static inline void fastrpc_free_pages(struct page **pages, int count)
{
	while (count--)
		__free_page(pages[count]);
	kvfree(pages);
}

static struct page **fastrpc_alloc_pages(unsigned int count, gfp_t gfp)
{
	struct page **pages;
	unsigned long order_mask = (2U << MAX_ORDER) - 1;
	unsigned int i = 0, array_size = count * sizeof(*pages);

	if (array_size <= PAGE_SIZE)
		pages = kzalloc(array_size, GFP_KERNEL);
	else
		pages = vzalloc(array_size);
	if (!pages)
		return NULL;

	/* IOMMU can map any pages, so himem can also be used here */
	gfp |= __GFP_NOWARN | __GFP_HIGHMEM;

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

			order_size = 1U << order;
			page = alloc_pages(order ?
					   (gfp | __GFP_NORETRY) &
						~__GFP_RECLAIM : gfp, order);
			if (!page)
				continue;
			if (!order)
				break;
			if (!PageCompound(page)) {
				split_page(page, order);
				break;
			} else if (!split_huge_page(page)) {
				break;
			}
			__free_pages(page, order);
		}
		if (!page) {
			fastrpc_free_pages(pages, i);
			return NULL;
		}
		count -= order_size;
		while (order_size--)
			pages[i++] = page++;
	}
	return pages;
}

static struct page **fastrpc_alloc_buffer(struct fastrpc_buf *buf, gfp_t gfp)
{
	struct page **pages;
	unsigned int count = PAGE_ALIGN(buf->size) >> PAGE_SHIFT;

	pages = fastrpc_alloc_pages(count, gfp);
	if (!pages)
		return NULL;

	if (sg_alloc_table_from_pages(&buf->sgt, pages, count, 0,
				buf->size, GFP_KERNEL))
		goto out_free_pages;

	if (!(buf->dma_attr & DMA_ATTR_NO_KERNEL_MAPPING)) {
		buf->va = vmap(pages, count, VM_USERMAP,
				pgprot_noncached(PAGE_KERNEL));
		if (!buf->va)
			goto out_free_sg;
	}
	return pages;

out_free_sg:
	sg_free_table(&buf->sgt);
out_free_pages:
	fastrpc_free_pages(pages, count);
	return NULL;
}

static inline void fastrpc_free_buffer(struct fastrpc_buf *buf)
{
	unsigned int count = PAGE_ALIGN(buf->size) >> PAGE_SHIFT;

	vunmap(buf->va);
	sg_free_table(&buf->sgt);
	fastrpc_free_pages(buf->pages, count);
}

static void fastrpc_buf_free(struct fastrpc_buf *buf, int cache)
{
	struct fastrpc_file *fl = buf == NULL ? NULL : buf->fl;

	if (!fl)
		return;

	if (cache && buf->size < MAX_CACHE_BUF_SIZE) {
		spin_lock(&fl->hlock);
		hlist_add_head(&buf->hn, &fl->cached_bufs);
		spin_unlock(&fl->hlock);
		return;
	}

	if (buf->remote) {
		spin_lock(&fl->hlock);
		hlist_del_init(&buf->hn_rem);
		spin_unlock(&fl->hlock);
		buf->remote = 0;
		buf->raddr = 0;
	}

	if (!IS_ERR_OR_NULL(buf->pages))
		fastrpc_free_buffer(buf);
	kfree(buf);
}

static int fastrpc_buf_alloc(struct fastrpc_file *fl, size_t size,
				unsigned long dma_attr, uint32_t rflags,
				int remote, struct fastrpc_buf **obuf)
{
	struct fastrpc_apps *me = &gfa;
	struct fastrpc_buf *buf = NULL, *fr = NULL;
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
		if (fr)
			hlist_del_init(&fr->hn);
		spin_unlock(&fl->hlock);
		if (fr) {
			*obuf = fr;
			return 0;
		}
	}

	VERIFY(err, NULL != (buf = kzalloc(sizeof(*buf), GFP_KERNEL)));
	if (err)
		goto bail;
	buf->fl = fl;
	buf->size = size;
	buf->va = NULL;
	buf->dma_attr = dma_attr;
	buf->flags = rflags;
	buf->raddr = 0;
	buf->remote = 0;
	buf->pages = fastrpc_alloc_buffer(buf, GFP_KERNEL);
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
		fastrpc_buf_free(buf, 0);
	return err;
}

static void fastrpc_cached_buf_list_free(struct fastrpc_file *fl)
{
	struct fastrpc_buf *buf, *free;

	do {
		struct hlist_node *n;

		free = NULL;
		spin_lock(&fl->hlock);
		hlist_for_each_entry_safe(buf, n, &fl->cached_bufs, hn) {
			hlist_del_init(&buf->hn);
			free = buf;
			break;
		}
		spin_unlock(&fl->hlock);
		if (free)
			fastrpc_buf_free(free, 0);
	} while (free);
}


static void fastrpc_remote_buf_list_free(struct fastrpc_file *fl)
{
	struct fastrpc_buf *buf, *free;

	do {
		struct hlist_node *n;

		free = NULL;
		spin_lock(&fl->hlock);
		hlist_for_each_entry_safe(buf, n, &fl->remote_bufs, hn_rem) {
			free = buf;
			break;
		}
		spin_unlock(&fl->hlock);
		if (free)
			fastrpc_buf_free(free, 0);
	} while (free);
}

static int fastrpc_open(struct inode *inode, struct file *filp)
{
	int err = 0;
	struct dentry *debugfs_file;
	struct fastrpc_file *fl = NULL;
	struct fastrpc_apps *me = &gfa;
	char strpid[PID_SIZE];
	int buf_size = 0;

	/*
	 * Indicates the device node opened
	 * MINOR_NUM_DEV or MINOR_NUM_SECURE_DEV
	 */
	int dev_minor = MINOR(inode->i_rdev);

	VERIFY(err, ((dev_minor == MINOR_NUM_DEV) ||
			(dev_minor == MINOR_NUM_SECURE_DEV)));
	if (err) {
		dev_err(me->dev, "Invalid dev minor num %d\n", dev_minor);
		return err;
	}

	VERIFY(err, NULL != (fl = kzalloc(sizeof(*fl), GFP_KERNEL)));
	if (err)
		return err;

	snprintf(strpid, PID_SIZE, "%d", current->pid);
	buf_size = strlen(current->comm) + strlen("_") + strlen(strpid) + 1;
	VERIFY(err, NULL != (fl->debug_buf = kzalloc(buf_size, GFP_KERNEL)));
	if (err) {
		kfree(fl);
		return err;
	}
	snprintf(fl->debug_buf, UL_SIZE, "%.10s%s%d",
			current->comm, "_", current->pid);
	debugfs_file = debugfs_create_file(fl->debug_buf, 0644, debugfs_root,
						fl, &debugfs_fops);

	context_list_ctor(&fl->clst);
	spin_lock_init(&fl->hlock);
	INIT_HLIST_HEAD(&fl->maps);
	INIT_HLIST_HEAD(&fl->cached_bufs);
	INIT_HLIST_HEAD(&fl->remote_bufs);
	fl->tgid = current->tgid;
	fl->apps = me;
	fl->mode = FASTRPC_MODE_SERIAL;
	fl->domain = -1;
	fl->cid = -1;
	fl->dev_minor = dev_minor;
	if (debugfs_file != NULL)
		fl->debugfs_file = debugfs_file;
	fl->dsp_proc_init = 0;
	filp->private_data = fl;
	mutex_init(&fl->map_mutex);
	return 0;
}

static int fastrpc_file_free(struct fastrpc_file *fl)
{
	struct fastrpc_mmap *map = NULL, *lmap = NULL;

	if (!fl)
		return 0;

	virt_fastrpc_close(fl);

	kfree(fl->debug_buf);

	spin_lock(&fl->hlock);
	fl->file_close = 1;
	spin_unlock(&fl->hlock);

	fastrpc_context_list_dtor(fl);
	fastrpc_cached_buf_list_free(fl);
	fastrpc_remote_buf_list_free(fl);

	mutex_lock(&fl->map_mutex);
	do {
		struct hlist_node *n = NULL;

		lmap = NULL;
		hlist_for_each_entry_safe(map, n, &fl->maps, hn) {
			hlist_del_init(&map->hn);
			lmap = map;
			break;
		}
		fastrpc_mmap_free(lmap, 1);
	} while (lmap);
	mutex_unlock(&fl->map_mutex);

	mutex_destroy(&fl->map_mutex);
	kfree(fl);
	return 0;
}

static int fastrpc_release(struct inode *inode, struct file *file)
{
	struct fastrpc_file *fl = (struct fastrpc_file *)file->private_data;

	if (fl) {
		debugfs_remove(fl->debugfs_file);
		fastrpc_file_free(fl);
		file->private_data = NULL;
	}
	return 0;
}


static inline void get_fastrpc_ioctl_mmap_64(
			struct fastrpc_ioctl_mmap_64 *mmap64,
			struct fastrpc_ioctl_mmap *immap)
{
	immap->fd = mmap64->fd;
	immap->flags = mmap64->flags;
	immap->vaddrin = (uintptr_t)mmap64->vaddrin;
	immap->size = mmap64->size;
}

static inline void put_fastrpc_ioctl_mmap_64(
			struct fastrpc_ioctl_mmap_64 *mmap64,
			struct fastrpc_ioctl_mmap *immap)
{
	mmap64->vaddrout = (uint64_t)immap->vaddrout;
}

static inline void get_fastrpc_ioctl_munmap_64(
			struct fastrpc_ioctl_munmap_64 *munmap64,
			struct fastrpc_ioctl_munmap *imunmap)
{
	imunmap->vaddrout = (uintptr_t)munmap64->vaddrout;
	imunmap->size = munmap64->size;
}

static int virt_fastrpc_munmap(struct fastrpc_file *fl, uintptr_t raddr,
				size_t size)
{
	struct fastrpc_apps *me = fl->apps;
	struct virt_munmap_msg *vmsg, *rsp = NULL;
	struct virt_fastrpc_msg *msg;
	struct scatterlist sg[1];
	int err;
	unsigned long flags;

	msg = virt_alloc_msg(sizeof(*vmsg));
	if (!msg)
		return -ENOMEM;

	vmsg = (struct virt_munmap_msg *)msg->txbuf;
	vmsg->hdr.pid = fl->tgid;
	vmsg->hdr.tid = current->pid;
	vmsg->hdr.cid = fl->cid;
	vmsg->hdr.cmd = VIRTIO_FASTRPC_CMD_MUNMAP;
	vmsg->hdr.len = sizeof(*vmsg);
	vmsg->hdr.msgid = msg->msgid;
	vmsg->hdr.result = 0xffffffff;
	vmsg->vdsp = raddr;
	vmsg->size = size;
	sg_init_one(sg, vmsg, sizeof(*vmsg));

	spin_lock_irqsave(&me->svq.vq_lock, flags);
	err = virtqueue_add_outbuf(me->svq.vq, sg, 1, vmsg, GFP_KERNEL);
	if (err) {
		dev_err(me->dev, "%s: fail to add output buffer\n", __func__);
		spin_unlock_irqrestore(&me->svq.vq_lock, flags);
		goto bail;
	}

	virtqueue_kick(me->svq.vq);
	spin_unlock_irqrestore(&me->svq.vq_lock, flags);

	wait_for_completion(&msg->work);

	rsp = msg->rxbuf;
	if (!rsp)
		goto bail;

	err = rsp->hdr.result;
bail:
	if (rsp) {
		sg_init_one(sg, rsp, me->buf_size);

		spin_lock_irqsave(&me->rvq.vq_lock, flags);
		/* add the buffer back to the remote processor's virtqueue */
		if (virtqueue_add_inbuf(me->rvq.vq, sg, 1, rsp, GFP_KERNEL))
			dev_err(me->dev,
				"%s: fail to add input buffer\n", __func__);
		else
			virtqueue_kick(me->rvq.vq);
		spin_unlock_irqrestore(&me->rvq.vq_lock, flags);
	}
	virt_free_msg(msg);

	return err;
}

static int fastrpc_internal_munmap(struct fastrpc_file *fl,
				   struct fastrpc_ioctl_munmap *ud)
{
	int err = 0;
	struct fastrpc_apps *me = fl->apps;
	struct fastrpc_mmap *map = NULL;
	struct fastrpc_buf *rbuf = NULL, *free = NULL;
	struct hlist_node *n;

	VERIFY(err, fl->dsp_proc_init == 1);
	if (err) {
		dev_err(me->dev, "%s: user application %s trying to unmap without initialization\n",
			 __func__, current->comm);
		err = -EBADR;
		goto bail;
	}

	spin_lock(&fl->hlock);
	hlist_for_each_entry_safe(rbuf, n, &fl->remote_bufs, hn_rem) {
		if (rbuf->raddr && (rbuf->flags == ADSP_MMAP_ADD_PAGES)) {
			if ((rbuf->raddr == ud->vaddrout) &&
				(rbuf->size == ud->size)) {
				free = rbuf;
				break;
			}
		}
	}
	spin_unlock(&fl->hlock);

	if (free) {
		VERIFY(err, !virt_fastrpc_munmap(fl, free->raddr, free->size));
		if (err)
			goto bail;
		fastrpc_buf_free(rbuf, 0);
		return err;
	}

	mutex_lock(&fl->map_mutex);
	VERIFY(err, !fastrpc_mmap_remove(fl, ud->vaddrout, ud->size, &map));
	mutex_unlock(&fl->map_mutex);
	if (err) {
		dev_err(me->dev, "mapping not found to unmap va 0x%lx, len 0x%x\n",
				ud->vaddrout, (unsigned int)ud->size);
		goto bail;
	}
	VERIFY(err, !virt_fastrpc_munmap(fl, map->raddr, map->size));
	if (err)
		goto bail;
	mutex_lock(&fl->map_mutex);
	fastrpc_mmap_free(map, 0);
	mutex_unlock(&fl->map_mutex);
bail:
	if (err && map) {
		mutex_lock(&fl->map_mutex);
		fastrpc_mmap_add(map);
		mutex_unlock(&fl->map_mutex);
	}
	return err;
}

static int fastrpc_internal_munmap_fd(struct fastrpc_file *fl,
				struct fastrpc_ioctl_munmap_fd *ud)
{
	int err = 0;
	struct fastrpc_apps *me = fl->apps;
	struct fastrpc_mmap *map = NULL;

	VERIFY(err, fl->dsp_proc_init == 1);
	if (err) {
		dev_err(me->dev, "%s: user application %s trying to unmap without initialization\n",
			__func__, current->comm);
		err = -EBADR;
		goto bail;
	}
	mutex_lock(&fl->map_mutex);
	if (fastrpc_mmap_find(fl, ud->fd, ud->va, ud->len, 0, 0, &map)) {
		dev_err(me->dev, "mapping not found to unmap fd 0x%x, va 0x%lx, len 0x%x\n",
			ud->fd, ud->va, (unsigned int)ud->len);
		err = -1;
		mutex_unlock(&fl->map_mutex);
		goto bail;
	}
	if (map)
		fastrpc_mmap_free(map, 0);
	mutex_unlock(&fl->map_mutex);
bail:
	return err;
}

static int virt_fastrpc_mmap(struct fastrpc_file *fl, uint32_t flags,
			uintptr_t va, struct scatterlist *table,
			unsigned int nents, size_t size, uintptr_t *raddr)
{
	struct fastrpc_apps *me = fl->apps;
	struct virt_mmap_msg *vmsg, *rsp = NULL;
	struct virt_fastrpc_msg *msg;
	struct virt_fastrpc_buf *sgbuf;
	struct scatterlist sg[1];
	int err, sgbuf_size, total_size;
	struct scatterlist *sgl = NULL;
	int sgl_index = 0;
	unsigned long int_flags;

	sgbuf_size = nents * sizeof(*sgbuf);
	total_size = sizeof(*vmsg) + sgbuf_size;

	msg = virt_alloc_msg(total_size);
	if (!msg)
		return -ENOMEM;

	vmsg = (struct virt_mmap_msg *)msg->txbuf;
	vmsg->hdr.pid = fl->tgid;
	vmsg->hdr.tid = current->pid;
	vmsg->hdr.cid = fl->cid;
	vmsg->hdr.cmd = VIRTIO_FASTRPC_CMD_MMAP;
	vmsg->hdr.len = total_size;
	vmsg->hdr.msgid = msg->msgid;
	vmsg->hdr.result = 0xffffffff;
	vmsg->flags = flags;
	vmsg->size = size;
	vmsg->vapp = va;
	vmsg->vdsp = 0;
	vmsg->nents = nents;
	sgbuf = vmsg->sgl;

	for_each_sg(table, sgl, nents, sgl_index) {
		if (sg_dma_len(sgl)) {
			sgbuf[sgl_index].pv = sg_dma_address(sgl);
			sgbuf[sgl_index].len = sg_dma_len(sgl);
		} else {
			sgbuf[sgl_index].pv = page_to_phys(sg_page(sgl));
			sgbuf[sgl_index].len = sgl->length;
		}
	}

	sg_init_one(sg, vmsg, total_size);

	spin_lock_irqsave(&me->svq.vq_lock, int_flags);
	err = virtqueue_add_outbuf(me->svq.vq, sg, 1, vmsg, GFP_KERNEL);
	if (err) {
		dev_err(me->dev, "%s: fail to add output buffer\n", __func__);
		spin_unlock_irqrestore(&me->svq.vq_lock, int_flags);
		goto bail;
	}

	virtqueue_kick(me->svq.vq);
	spin_unlock_irqrestore(&me->svq.vq_lock, int_flags);

	wait_for_completion(&msg->work);

	rsp = msg->rxbuf;
	if (!rsp)
		goto bail;

	err = rsp->hdr.result;
	if (err)
		goto bail;
	*raddr = (uintptr_t)rsp->vdsp;
bail:
	if (rsp) {
		sg_init_one(sg, rsp, me->buf_size);

		spin_lock_irqsave(&me->rvq.vq_lock, int_flags);
		/* add the buffer back to the remote processor's virtqueue */
		if (virtqueue_add_inbuf(me->rvq.vq, sg, 1, rsp, GFP_KERNEL))
			dev_err(me->dev,
				"%s: fail to add input buffer\n", __func__);
		else
			virtqueue_kick(me->rvq.vq);
		spin_unlock_irqrestore(&me->rvq.vq_lock, int_flags);
	}
	virt_free_msg(msg);

	return err;
}

static int fastrpc_internal_mmap(struct fastrpc_file *fl,
				 struct fastrpc_ioctl_mmap *ud)
{
	struct fastrpc_apps *me = fl->apps;
	struct fastrpc_mmap *map = NULL;
	struct fastrpc_buf *rbuf = NULL;
	unsigned long dma_attr = 0;
	uintptr_t raddr = 0;
	int err = 0;

	VERIFY(err, fl->dsp_proc_init == 1);
	if (err) {
		dev_err(me->dev, "%s: user application %s trying to map without initialization\n",
			__func__, current->comm);
		err = -EBADR;
		goto bail;
	}
	if (ud->flags == ADSP_MMAP_ADD_PAGES) {
		if (ud->vaddrin) {
			err = -EINVAL;
			dev_err(me->dev, "%s: %s: ERROR: adding user allocated pages is not supported\n",
					current->comm, __func__);
			goto bail;
		}
		dma_attr = DMA_ATTR_NO_KERNEL_MAPPING;
		err = fastrpc_buf_alloc(fl, ud->size, dma_attr, ud->flags,
								1, &rbuf);
		if (err)
			goto bail;
		err = virt_fastrpc_mmap(fl, ud->flags, 0, rbuf->sgt.sgl,
					rbuf->sgt.nents, rbuf->size, &raddr);
		if (err)
			goto bail;
		rbuf->raddr = raddr;
	} else {
		uintptr_t va_to_dsp;

		mutex_lock(&fl->map_mutex);
		VERIFY(err, !fastrpc_mmap_create(fl, ud->fd,
				(uintptr_t)ud->vaddrin, ud->size,
				 ud->flags, &map));
		mutex_unlock(&fl->map_mutex);
		if (err)
			goto bail;

		if (ud->flags == ADSP_MMAP_HEAP_ADDR ||
			ud->flags == ADSP_MMAP_REMOTE_HEAP_ADDR)
			va_to_dsp = 0;
		else
			va_to_dsp = (uintptr_t)map->va;

		VERIFY(err, 0 == virt_fastrpc_mmap(fl, ud->flags, va_to_dsp,
					map->table->sgl, map->table->nents,
					map->size, &raddr));
		if (err)
			goto bail;
		map->raddr = raddr;
	}
	ud->vaddrout = raddr;
 bail:
	if (err && map) {
		mutex_lock(&fl->map_mutex);
		fastrpc_mmap_free(map, 0);
		mutex_unlock(&fl->map_mutex);
	}
	return err;
}

static int virt_fastrpc_control(struct fastrpc_file *fl,
				struct fastrpc_ctrl_latency *lp)
{
	struct fastrpc_apps *me = fl->apps;
	struct virt_control_msg *vmsg, *rsp = NULL;
	struct virt_fastrpc_msg *msg;
	struct scatterlist sg[1];
	int err;
	unsigned long flags;

	msg = virt_alloc_msg(sizeof(*vmsg));
	if (!msg)
		return -ENOMEM;

	vmsg = (struct virt_control_msg *)msg->txbuf;
	vmsg->hdr.pid = fl->tgid;
	vmsg->hdr.tid = current->pid;
	vmsg->hdr.cid = fl->cid;
	vmsg->hdr.cmd = VIRTIO_FASTRPC_CMD_CONTROL;
	vmsg->hdr.len = sizeof(*vmsg);
	vmsg->hdr.msgid = msg->msgid;
	vmsg->hdr.result = 0xffffffff;
	vmsg->enable = lp->enable;
	vmsg->latency = lp->latency;
	sg_init_one(sg, vmsg, sizeof(*vmsg));

	spin_lock_irqsave(&me->svq.vq_lock, flags);
	err = virtqueue_add_outbuf(me->svq.vq, sg, 1, vmsg, GFP_KERNEL);
	if (err) {
		dev_err(me->dev, "%s: fail to add output buffer\n", __func__);
		spin_unlock_irqrestore(&me->svq.vq_lock, flags);
		goto bail;
	}

	virtqueue_kick(me->svq.vq);
	spin_unlock_irqrestore(&me->svq.vq_lock, flags);

	wait_for_completion(&msg->work);

	rsp = msg->rxbuf;
	if (!rsp)
		goto bail;

	err = rsp->hdr.result;
bail:
	if (rsp) {
		sg_init_one(sg, rsp, me->buf_size);

		spin_lock_irqsave(&me->rvq.vq_lock, flags);
		/* add the buffer back to the remote processor's virtqueue */
		if (virtqueue_add_inbuf(me->rvq.vq, sg, 1, rsp, GFP_KERNEL))
			dev_err(me->dev,
				"%s: fail to add input buffer\n", __func__);
		else
			virtqueue_kick(me->rvq.vq);
		spin_unlock_irqrestore(&me->rvq.vq_lock, flags);
	}
	virt_free_msg(msg);

	return err;
}

static int fastrpc_internal_control(struct fastrpc_file *fl,
					struct fastrpc_ioctl_control *cp)
{
	struct fastrpc_apps *me = fl->apps;
	int err = 0;

	VERIFY(err, !IS_ERR_OR_NULL(fl) && !IS_ERR_OR_NULL(fl->apps));
	if (err)
		goto bail;
	VERIFY(err, !IS_ERR_OR_NULL(cp));
	if (err)
		goto bail;

	switch (cp->req) {
	case FASTRPC_CONTROL_LATENCY:
		if (!(me->has_control)) {
			dev_err(me->dev, "qos setting is not supported\n");
			err = -ENOTTY;
			goto bail;
		}
		virt_fastrpc_control(fl, &cp->lp);
		break;
	case FASTRPC_CONTROL_KALLOC:
		cp->kalloc.kalloc_support = 1;
		break;
	default:
		err = -ENOTTY;
		break;
	}
bail:
	return err;
}

static int fastrpc_ioctl_get_info(struct fastrpc_file *fl,
					uint32_t *info)
{
	int err = 0;
	uint32_t domain;

	VERIFY(err, fl != NULL);
	if (err)
		goto bail;
	if (fl->domain == -1) {
		domain = *info;
		VERIFY(err, domain < fl->apps->num_channels);
		if (err)
			goto bail;
		fl->domain = domain;
	}
	*info = 1;
bail:
	return err;
}

static int virt_fastrpc_open(struct fastrpc_file *fl)
{
	struct fastrpc_apps *me = fl->apps;
	struct virt_open_msg *vmsg, *rsp = NULL;
	struct virt_fastrpc_msg *msg;
	struct scatterlist sg[1];
	int err;
	unsigned long flags;

	msg = virt_alloc_msg(sizeof(*vmsg));
	if (!msg) {
		dev_err(me->dev, "%s: no memory\n", __func__);
		return -ENOMEM;
	}

	vmsg = (struct virt_open_msg *)msg->txbuf;
	vmsg->hdr.pid = fl->tgid;
	vmsg->hdr.tid = current->pid;
	vmsg->hdr.cid = -1;
	vmsg->hdr.cmd = VIRTIO_FASTRPC_CMD_OPEN;
	vmsg->hdr.len = sizeof(*vmsg);
	vmsg->hdr.msgid = msg->msgid;
	vmsg->hdr.result = 0xffffffff;
	vmsg->domain = fl->domain;
	vmsg->pd = fl->pd;
	sg_init_one(sg, vmsg, sizeof(*vmsg));

	spin_lock_irqsave(&me->svq.vq_lock, flags);
	err = virtqueue_add_outbuf(me->svq.vq, sg, 1, vmsg, GFP_KERNEL);
	if (err) {
		dev_err(me->dev, "%s: fail to add output buffer\n", __func__);
		spin_unlock_irqrestore(&me->svq.vq_lock, flags);
		goto bail;
	}

	virtqueue_kick(me->svq.vq);
	spin_unlock_irqrestore(&me->svq.vq_lock, flags);

	wait_for_completion(&msg->work);

	rsp = msg->rxbuf;
	if (!rsp)
		goto bail;

	err = rsp->hdr.result;
	if (err)
		goto bail;
	if (rsp->hdr.cid < 0) {
		dev_err(me->dev, "channel id %d is invalid\n", rsp->hdr.cid);
		err = -EINVAL;
		goto bail;
	}
	fl->cid = rsp->hdr.cid;
bail:
	if (rsp) {
		sg_init_one(sg, rsp, me->buf_size);

		spin_lock_irqsave(&me->rvq.vq_lock, flags);
		/* add the buffer back to the remote processor's virtqueue */
		if (virtqueue_add_inbuf(me->rvq.vq, sg, 1, rsp, GFP_KERNEL))
			dev_err(me->dev,
				"%s: fail to add input buffer\n", __func__);
		else
			virtqueue_kick(me->rvq.vq);
		spin_unlock_irqrestore(&me->rvq.vq_lock, flags);
	}
	virt_free_msg(msg);

	return err;
}

static int virt_fastrpc_close(struct fastrpc_file *fl)
{
	struct fastrpc_apps *me = fl->apps;
	struct virt_msg_hdr *vmsg, *rsp = NULL;
	struct virt_fastrpc_msg *msg;
	struct scatterlist sg[1];
	int err;
	unsigned long flags;

	if (fl->cid < 0) {
		dev_err(me->dev, "channel id %d is invalid\n", fl->cid);
		return -EINVAL;
	}

	msg = virt_alloc_msg(sizeof(*vmsg));
	if (!msg) {
		dev_err(me->dev, "%s: no memory\n", __func__);
		return -ENOMEM;
	}

	vmsg = (struct virt_msg_hdr *)msg->txbuf;
	vmsg->pid = fl->tgid;
	vmsg->tid = current->pid;
	vmsg->cid = fl->cid;
	vmsg->cmd = VIRTIO_FASTRPC_CMD_CLOSE;
	vmsg->len = sizeof(*vmsg);
	vmsg->msgid = msg->msgid;
	vmsg->result = 0xffffffff;
	sg_init_one(sg, vmsg, sizeof(*vmsg));

	spin_lock_irqsave(&me->svq.vq_lock, flags);
	err = virtqueue_add_outbuf(me->svq.vq, sg, 1, vmsg, GFP_KERNEL);
	if (err) {
		dev_err(me->dev, "%s: fail to add output buffer\n", __func__);
		spin_unlock_irqrestore(&me->svq.vq_lock, flags);
		goto bail;
	}

	virtqueue_kick(me->svq.vq);
	spin_unlock_irqrestore(&me->svq.vq_lock, flags);

	wait_for_completion(&msg->work);

	rsp = msg->rxbuf;
	if (!rsp)
		goto bail;

	err = rsp->result;
bail:
	if (rsp) {
		sg_init_one(sg, rsp, me->buf_size);

		spin_lock_irqsave(&me->rvq.vq_lock, flags);
		/* add the buffer back to the remote processor's virtqueue */
		if (virtqueue_add_inbuf(me->rvq.vq, sg, 1, rsp, GFP_KERNEL))
			dev_err(me->dev,
				"%s: fail to add input buffer\n", __func__);
		else
			virtqueue_kick(me->rvq.vq);
		spin_unlock_irqrestore(&me->rvq.vq_lock, flags);
	}
	virt_free_msg(msg);

	return err;
}

static int fastrpc_init_process(struct fastrpc_file *fl,
				struct fastrpc_ioctl_init_attrs *uproc)
{
	int err = 0;
	struct fastrpc_ioctl_init *init = &uproc->init;

	if (init->flags == FASTRPC_INIT_ATTACH ||
			init->flags == FASTRPC_INIT_ATTACH_SENSORS) {
		fl->pd = GUEST_OS;
		err = virt_fastrpc_open(fl);
		if (err)
			goto bail;
	} else if (init->flags == FASTRPC_INIT_CREATE) {
		fl->pd = DYNAMIC_PD;
		err = virt_fastrpc_open(fl);
		if (err)
			goto bail;
	} else if (init->flags == FASTRPC_INIT_CREATE_STATIC) {
		fl->pd = STATIC_PD;
		err = virt_fastrpc_open(fl);
		if (err)
			goto bail;
	} else {
		err = -ENOTTY;
		goto bail;
	}
	fl->dsp_proc_init = 1;
bail:
	return err;
}

static long fastrpc_ioctl(struct file *file, unsigned int ioctl_num,
				 unsigned long ioctl_param)
{
	union {
		struct fastrpc_ioctl_invoke_crc inv;
		struct fastrpc_ioctl_mmap mmap;
		struct fastrpc_ioctl_mmap_64 mmap64;
		struct fastrpc_ioctl_munmap munmap;
		struct fastrpc_ioctl_munmap_64 munmap64;
		struct fastrpc_ioctl_munmap_fd munmap_fd;
		struct fastrpc_ioctl_init_attrs init;
		struct fastrpc_ioctl_control cp;
	} p;
	union {
		struct fastrpc_ioctl_mmap mmap;
		struct fastrpc_ioctl_munmap munmap;
	} i;
	void *param = (char *)ioctl_param;
	struct fastrpc_file *fl = (struct fastrpc_file *)file->private_data;
	struct fastrpc_apps *me = &gfa;
	int size = 0, err = 0;
	uint32_t info;

	p.inv.fds = NULL;
	p.inv.attrs = NULL;
	p.inv.crc = NULL;

	spin_lock(&fl->hlock);
	if (fl->file_close == 1) {
		err = -EBADF;
		dev_warn(me->dev, "fastrpc_device_release is happening, So not sending any new requests to DSP\n");
		spin_unlock(&fl->hlock);
		goto bail;
	}
	spin_unlock(&fl->hlock);

	switch (ioctl_num) {
	case FASTRPC_IOCTL_INVOKE:
		size = sizeof(struct fastrpc_ioctl_invoke);
		fallthrough;
	case FASTRPC_IOCTL_INVOKE_FD:
		if (!size)
			size = sizeof(struct fastrpc_ioctl_invoke_fd);
		fallthrough;
	case FASTRPC_IOCTL_INVOKE_ATTRS:
		if (!size)
			size = sizeof(struct fastrpc_ioctl_invoke_attrs);
		fallthrough;
	case FASTRPC_IOCTL_INVOKE_CRC:
		if (!size)
			size = sizeof(struct fastrpc_ioctl_invoke_crc);

		K_COPY_FROM_USER(err, 0, &p.inv, param, size);
		if (err)
			goto bail;

		if (p.inv.attrs && !(me->has_invoke_attr)) {
			dev_err(me->dev, "invoke attr is not supported\n");
			err = -ENOTTY;
			goto bail;
		}
		if (p.inv.crc && !(me->has_invoke_crc)) {
			dev_err(me->dev, "invoke crc is not supported\n");
			err = -ENOTTY;
			goto bail;
		}

		VERIFY(err, 0 == (err = fastrpc_internal_invoke(fl, fl->mode,
								&p.inv)));
		if (err)
			goto bail;
		break;
	case FASTRPC_IOCTL_MMAP:
		if (!me->has_mmap) {
			dev_err(me->dev, "mmap is not supported\n");
			err = -ENOTTY;
			goto bail;
		}

		K_COPY_FROM_USER(err, 0, &p.mmap, param,
						sizeof(p.mmap));
		if (err)
			goto bail;
		VERIFY(err, 0 == (err = fastrpc_internal_mmap(fl, &p.mmap)));
		if (err)
			goto bail;
		K_COPY_TO_USER(err, 0, param, &p.mmap, sizeof(p.mmap));
		if (err)
			goto bail;
		break;
	case FASTRPC_IOCTL_MUNMAP:
		if (!(me->has_mmap)) {
			dev_err(me->dev, "munmap is not supported\n");
			err = -ENOTTY;
			goto bail;
		}

		K_COPY_FROM_USER(err, 0, &p.munmap, param,
						sizeof(p.munmap));
		if (err)
			goto bail;
		VERIFY(err, 0 == (err = fastrpc_internal_munmap(fl,
							&p.munmap)));
		if (err)
			goto bail;
		break;
	case FASTRPC_IOCTL_MMAP_64:
		if (!(me->has_mmap)) {
			dev_err(me->dev, "mmap is not supported\n");
			err = -ENOTTY;
			goto bail;
		}

		K_COPY_FROM_USER(err, 0, &p.mmap64, param,
						sizeof(p.mmap64));
		if (err)
			goto bail;
		get_fastrpc_ioctl_mmap_64(&p.mmap64, &i.mmap);
		VERIFY(err, 0 == (err = fastrpc_internal_mmap(fl, &i.mmap)));
		if (err)
			goto bail;
		put_fastrpc_ioctl_mmap_64(&p.mmap64, &i.mmap);
		K_COPY_TO_USER(err, 0, param, &p.mmap64, sizeof(p.mmap64));
		if (err)
			goto bail;
		break;
	case FASTRPC_IOCTL_MUNMAP_64:
		if (!(me->has_mmap)) {
			dev_err(me->dev, "munmap is not supported\n");
			err = -ENOTTY;
			goto bail;
		}

		K_COPY_FROM_USER(err, 0, &p.munmap64, param,
						sizeof(p.munmap64));
		if (err)
			goto bail;
		get_fastrpc_ioctl_munmap_64(&p.munmap64, &i.munmap);
		VERIFY(err, 0 == (err = fastrpc_internal_munmap(fl,
							&i.munmap)));
		if (err)
			goto bail;
		break;
	case FASTRPC_IOCTL_MUNMAP_FD:
		K_COPY_FROM_USER(err, 0, &p.munmap_fd, param,
			sizeof(p.munmap_fd));
		if (err)
			goto bail;
		VERIFY(err, 0 == (err = fastrpc_internal_munmap_fd(fl,
			&p.munmap_fd)));
		if (err)
			goto bail;
		break;
	case FASTRPC_IOCTL_SETMODE:
		switch ((uint32_t)ioctl_param) {
		case FASTRPC_MODE_PARALLEL:
		case FASTRPC_MODE_SERIAL:
			fl->mode = (uint32_t)ioctl_param;
			break;
		case FASTRPC_MODE_SESSION:
			err = -ENOTTY;
			dev_err(me->dev, "session mode is not supported\n");
			break;
		default:
			err = -ENOTTY;
			break;
		}
		break;
	case FASTRPC_IOCTL_CONTROL:
		K_COPY_FROM_USER(err, 0, &p.cp, param,
				sizeof(p.cp));
		if (err)
			goto bail;
		VERIFY(err, 0 == (err = fastrpc_internal_control(fl, &p.cp)));
		if (err)
			goto bail;
		if (p.cp.req == FASTRPC_CONTROL_KALLOC) {
			K_COPY_TO_USER(err, 0, param, &p.cp, sizeof(p.cp));
			if (err)
				goto bail;
		}
		break;
	case FASTRPC_IOCTL_GETINFO:
		K_COPY_FROM_USER(err, 0, &info, param, sizeof(info));
		if (err)
			goto bail;
		VERIFY(err, 0 == (err = fastrpc_ioctl_get_info(fl, &info)));
		if (err)
			goto bail;
		K_COPY_TO_USER(err, 0, param, &info, sizeof(info));
		if (err)
			goto bail;
		break;
	case FASTRPC_IOCTL_INIT:
		p.init.attrs = 0;
		p.init.siglen = 0;
		size = sizeof(struct fastrpc_ioctl_init);
		fallthrough;
	case FASTRPC_IOCTL_INIT_ATTRS:
		if (!size)
			size = sizeof(struct fastrpc_ioctl_init_attrs);
		K_COPY_FROM_USER(err, 0, &p.init, param, size);
		if (err)
			goto bail;
		VERIFY(err, p.init.init.filelen >= 0 &&
			p.init.init.filelen < INIT_FILELEN_MAX);
		if (err)
			goto bail;
		VERIFY(err, p.init.init.memlen >= 0 &&
			p.init.init.memlen < INIT_MEMLEN_MAX);
		if (err)
			goto bail;
		VERIFY(err, 0 == (err = fastrpc_init_process(fl, &p.init)));
		if (err)
			goto bail;
		break;

	default:
		err = -ENOTTY;
		dev_info(me->dev, "bad ioctl: %d\n", ioctl_num);
		break;
	}
 bail:
	return err;
}

static const struct file_operations fops = {
	.open = fastrpc_open,
	.release = fastrpc_release,
	.unlocked_ioctl = fastrpc_ioctl,
	.compat_ioctl = compat_fastrpc_device_ioctl,
};

static void fastrpc_init(struct fastrpc_apps *me)
{
	spin_lock_init(&me->msglock);
}

static int recv_single(struct virt_msg_hdr *rsp, unsigned int len)
{
	struct fastrpc_apps *me = &gfa;
	struct virt_fastrpc_msg *msg;

	if (len != rsp->len) {
		dev_err(me->dev, "msg %u len mismatch,expected %u but %d found\n",
				rsp->cmd, rsp->len, len);
		return -EINVAL;
	}
	spin_lock(&me->msglock);
	msg = me->msgtable[rsp->msgid];
	spin_unlock(&me->msglock);

	if (!msg) {
		dev_err(me->dev, "msg %u already free in table[%u]\n",
				rsp->cmd, rsp->msgid);
		return -EINVAL;
	}
	msg->rxbuf = (void *)rsp;

	complete(&msg->work);
	return 0;
}

static void recv_done(struct virtqueue *rvq)
{

	struct fastrpc_apps *me = &gfa;
	struct virt_msg_hdr *rsp;
	unsigned int len, msgs_received = 0;
	int err;
	unsigned long flags;

	spin_lock_irqsave(&me->rvq.vq_lock, flags);
	rsp = virtqueue_get_buf(rvq, &len);
	if (!rsp) {
		spin_unlock_irqrestore(&me->rvq.vq_lock, flags);
		dev_err(me->dev, "incoming signal, but no used buffer\n");
		return;
	}
	spin_unlock_irqrestore(&me->rvq.vq_lock, flags);

	while (rsp) {
		err = recv_single(rsp, len);
		if (err)
			break;

		msgs_received++;

		spin_lock_irqsave(&me->rvq.vq_lock, flags);
		rsp = virtqueue_get_buf(rvq, &len);
		spin_unlock_irqrestore(&me->rvq.vq_lock, flags);
	}
}

static int init_vqs(struct fastrpc_apps *me)
{
	struct virtqueue *vqs[2];
	static const char * const names[] = { "output", "input" };
	vq_callback_t *cbs[] = { NULL, recv_done };
	size_t total_buf_space;
	void *bufs;
	int err;

	err = virtio_find_vqs(me->vdev, 2, vqs, cbs, names, NULL);
	if (err)
		return err;

	virt_init_vq(&me->svq, vqs[0]);
	virt_init_vq(&me->rvq, vqs[1]);

	/* we expect symmetric tx/rx vrings */
	WARN_ON(virtqueue_get_vring_size(me->rvq.vq) !=
			virtqueue_get_vring_size(me->svq.vq));
	me->num_bufs = virtqueue_get_vring_size(me->rvq.vq) * 2;

	me->buf_size = MAX_FASTRPC_BUF_SIZE;
	total_buf_space = me->num_bufs * me->buf_size;
	me->order = get_order(total_buf_space);
	bufs = (void *)__get_free_pages(GFP_KERNEL,
				me->order);
	if (!bufs) {
		err = -ENOMEM;
		goto vqs_del;
	}

	/* half of the buffers is dedicated for RX */
	me->rbufs = bufs;

	/* and half is dedicated for TX */
	me->sbufs = bufs + total_buf_space / 2;
	return 0;

vqs_del:
	me->vdev->config->del_vqs(me->vdev);
	return err;
}

/**
 ** virtio_fastrpc_pm_notifier() - PM notifier callback function.
 ** @nb:                Pointer to the notifier block.
 ** @event:        Suspend state event from PM module.
 ** @unused:        Null pointer from PM module.
 **
 ** This function is register as callback function to get notifications
 ** from the PM module on the system suspend state.
 **/
static int virtio_fastrpc_pm_notifier(struct notifier_block *nb,
					unsigned long event, void *unused)
{
	struct fastrpc_apps *me = &gfa;
	unsigned long flags;
	int i = 0;
	struct virt_fastrpc_msg *msg;

	if (event == PM_SUSPEND_PREPARE) {
		spin_lock_irqsave(&me->msglock, flags);
		for (i = 0; i < FASTRPC_MSG_MAX; i++) {
			if (me->msgtable[i]) {
				msg = me->msgtable[i];
				complete(&msg->work);
			}
		}
		spin_unlock_irqrestore(&me->msglock, flags);
	}
	return NOTIFY_DONE;
}

static struct notifier_block virtio_fastrpc_pm_nb = {
		.notifier_call = virtio_fastrpc_pm_notifier,
};

static int virt_fastrpc_probe(struct virtio_device *vdev)
{
	struct fastrpc_apps *me = &gfa;
	struct device *dev = NULL;
	struct device *secure_dev = NULL;
	int err, i;

	if (!virtio_has_feature(vdev, VIRTIO_F_VERSION_1))
		return -ENODEV;

	memset(me, 0, sizeof(*me));
	fastrpc_init(me);

	if (virtio_has_feature(vdev, VIRTIO_FASTRPC_F_INVOKE_ATTR))
		me->has_invoke_attr = true;

	if (virtio_has_feature(vdev, VIRTIO_FASTRPC_F_INVOKE_CRC))
		me->has_invoke_crc = true;

	if (virtio_has_feature(vdev, VIRTIO_FASTRPC_F_MMAP))
		me->has_mmap = true;

	if (virtio_has_feature(vdev, VIRTIO_FASTRPC_F_CONTROL))
		me->has_control = true;

	vdev->priv = me;
	me->vdev = vdev;
	me->dev = vdev->dev.parent;

	err = init_vqs(me);
	if (err) {
		dev_err(&vdev->dev, "failed to initialized virtqueue\n");
		return err;
	}

	if (of_get_property(me->dev->of_node, "qcom,domain_num", NULL) != NULL) {
		err = of_property_read_u32(me->dev->of_node, "qcom,domain_num",
					&me->num_channels);
		if (err) {
			dev_err(&vdev->dev, "failed to read domain_num %d\n", err);
			goto alloc_chrdev_bail;
		}
	} else {
		dev_dbg(&vdev->dev, "set domain_num to default value\n");
		me->num_channels = NUM_CHANNELS;
	}

	debugfs_root = debugfs_create_dir("adsprpc", NULL);
	err = alloc_chrdev_region(&me->dev_no, 0, me->num_channels, DEVICE_NAME);
	if (err)
		goto alloc_chrdev_bail;

	cdev_init(&me->cdev, &fops);
	me->cdev.owner = THIS_MODULE;
	err = cdev_add(&me->cdev, MKDEV(MAJOR(me->dev_no), 0), NUM_DEVICES);
	if (err)
		goto cdev_init_bail;

	me->class = class_create(THIS_MODULE, "fastrpc");
	if (IS_ERR(me->class))
		goto class_create_bail;

	/*
	 * Create devices and register with sysfs
	 * Create first device with minor number 0
	 */
	dev = device_create(me->class, NULL,
				MKDEV(MAJOR(me->dev_no), MINOR_NUM_DEV),
				NULL, DEVICE_NAME);
	if (IS_ERR_OR_NULL(dev))
		goto device_create_bail;

	/* Create secure device with minor number for secure device */
	secure_dev = device_create(me->class, NULL,
				MKDEV(MAJOR(me->dev_no), MINOR_NUM_SECURE_DEV),
				NULL, DEVICE_NAME_SECURE);
	if (IS_ERR_OR_NULL(secure_dev))
		goto device_create_bail;

	err = register_pm_notifier(&virtio_fastrpc_pm_nb);
	if (err) {
		pr_err("virtio_fastrpc: power state notifier error\n");
		goto device_create_bail;
	}

	virtio_device_ready(vdev);

	/* set up the receive buffers */
	for (i = 0; i < me->num_bufs / 2; i++) {
		struct scatterlist sg;
		void *cpu_addr = me->rbufs + i * me->buf_size;

		sg_init_one(&sg, cpu_addr, me->buf_size);
		err = virtqueue_add_inbuf(me->rvq.vq, &sg, 1, cpu_addr,
				GFP_KERNEL);
		WARN_ON(err); /* sanity check; this can't really happen */
	}

	/* suppress "tx-complete" interrupts */
	virtqueue_disable_cb(me->svq.vq);

	virtqueue_enable_cb(me->rvq.vq);
	virtqueue_kick(me->rvq.vq);

	dev_info(&vdev->dev, "Registered virtio fastrpc device\n");

	return 0;
device_create_bail:
	if (!IS_ERR_OR_NULL(dev))
		device_destroy(me->class, MKDEV(MAJOR(me->dev_no),
						MINOR_NUM_DEV));
	if (!IS_ERR_OR_NULL(secure_dev))
		device_destroy(me->class, MKDEV(MAJOR(me->dev_no),
						MINOR_NUM_SECURE_DEV));
	class_destroy(me->class);
class_create_bail:
	cdev_del(&me->cdev);
cdev_init_bail:
	unregister_chrdev_region(me->dev_no, me->num_channels);
alloc_chrdev_bail:
	vdev->config->del_vqs(vdev);
	return err;
}

static void virt_fastrpc_remove(struct virtio_device *vdev)
{
	struct fastrpc_apps *me = &gfa;

	unregister_pm_notifier(&virtio_fastrpc_pm_nb);
	device_destroy(me->class, MKDEV(MAJOR(me->dev_no), MINOR_NUM_DEV));
	device_destroy(me->class, MKDEV(MAJOR(me->dev_no),
					MINOR_NUM_SECURE_DEV));
	class_destroy(me->class);
	cdev_del(&me->cdev);
	unregister_chrdev_region(me->dev_no, me->num_channels);
	debugfs_remove_recursive(debugfs_root);

	vdev->config->reset(vdev);
	vdev->config->del_vqs(vdev);
	free_pages((unsigned long)me->rbufs, me->order);
}

const struct virtio_device_id id_table[] = {
	{ VIRTIO_ID_FASTRPC, VIRTIO_DEV_ANY_ID },
	{ 0 },
};

static unsigned int features[] = {
	VIRTIO_FASTRPC_F_INVOKE_ATTR,
	VIRTIO_FASTRPC_F_INVOKE_CRC,
	VIRTIO_FASTRPC_F_MMAP,
	VIRTIO_FASTRPC_F_CONTROL,
};

static struct virtio_driver virtio_fastrpc_driver = {
	.feature_table		= features,
	.feature_table_size	= ARRAY_SIZE(features),
	.driver.name		= KBUILD_MODNAME,
	.driver.owner		= THIS_MODULE,
	.id_table		= id_table,
	.probe			= virt_fastrpc_probe,
	.remove			= virt_fastrpc_remove,
};

static int __init virtio_fastrpc_init(void)
{
	return register_virtio_driver(&virtio_fastrpc_driver);
}

static void __exit virtio_fastrpc_exit(void)
{
	unregister_pm_notifier(&virtio_fastrpc_pm_nb);
	unregister_virtio_driver(&virtio_fastrpc_driver);
}
module_init(virtio_fastrpc_init);
module_exit(virtio_fastrpc_exit);

MODULE_DEVICE_TABLE(virtio, id_table);
MODULE_DESCRIPTION("Virtio fastrpc driver");
MODULE_LICENSE("GPL v2");
