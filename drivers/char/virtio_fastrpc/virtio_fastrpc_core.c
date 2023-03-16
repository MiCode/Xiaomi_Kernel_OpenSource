// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/ion.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <linux/crc32.h>
#include "virtio_fastrpc_core.h"
#include "virtio_fastrpc_mem.h"
#include "virtio_fastrpc_queue.h"

#define M_FDLIST			16
#define M_CRCLIST			64
#define M_DSP_PERF_LIST		12
#define M_KERNEL_PERF_LIST (PERF_KEY_MAX)
#define M_DSP_PERF_LIST		12

#define FASTRPC_DMAHANDLE_NOMAP	16

#define VIRTIO_FASTRPC_CMD_OPEN			1
#define VIRTIO_FASTRPC_CMD_CLOSE		2
#define VIRTIO_FASTRPC_CMD_INVOKE		3
#define VIRTIO_FASTRPC_CMD_MMAP			4
#define VIRTIO_FASTRPC_CMD_MUNMAP		5
#define VIRTIO_FASTRPC_CMD_CONTROL		6
#define VIRTIO_FASTRPC_CMD_GET_DSP_INFO		7
#define VIRTIO_FASTRPC_CMD_MUNMAP_FD		8
#define VIRTIO_FASTRPC_CMD_MEM_MAP		9
#define VIRTIO_FASTRPC_CMD_MEM_UNMAP		10

#define STATIC_PD			0
#define DYNAMIC_PD			1
#define GUEST_OS			2

#define FASTRPC_STATIC_HANDLE_KERNEL	1
#define FASTRPC_STATIC_HANDLE_LISTENER	3
#define FASTRPC_STATIC_HANDLE_MAX	20

#define UNSIGNED_PD_SUPPORT 1
#define PERF_CAPABILITY   (1 << 1)

#define PERF_END ((void)0)

#define PERF(enb, cnt, ff) \
	{\
		struct timespec64 startT = {0};\
		uint64_t *counter = cnt;\
		if (enb && counter) {\
			ktime_get_real_ts64(&startT);\
		} \
		ff; \
		if (enb && counter) {\
			*counter += getnstimediff(&startT);\
		} \
	}

#define GET_COUNTER(perf_ptr, offset)  \
	(perf_ptr != NULL ?\
		(((offset >= 0) && (offset < PERF_KEY_MAX)) ?\
			(uint64_t *)(perf_ptr + offset)\
				: (uint64_t *)NULL) : (uint64_t *)NULL)

/* set for cached mapping */
#define VFASTRPC_MAP_ATTR_CACHED	1

#define SIZE_OF_MAPPING(nents) \
	(sizeof(struct virt_fastrpc_mapping) + \
		nents * sizeof(struct virt_fastrpc_sgl))

enum virtio_fastrpc_invoke_attr {
	/* bit0, 1: FE/BE crc enabled, 0: FE/BE crc disabled */
	VIRTIO_FASTRPC_INVOKE_CRC = 1 << 0,
	VIRTIO_FASTRPC_INVOKE_PERF = 1 << 1,
	VIRTIO_FASTRPC_INVOKE_ASYNC = 1 << 2,
};

enum vfastrpc_buf_attr {
	/* bit0, 1: ION buffer, 0: normal buffer */
	VFASTRPC_BUF_ATTR_ION = 1 << 0,
	/* bit1, 1: buffer data is in an intermediate buffer, 0: buffer data is in invoke message */
	VFASTRPC_BUF_ATTR_INTERNAL = 1 << 1,
	/* bit2, 1: keep map at the end of invoke, 0: remove map at the end of invoke */
	VFASTRPC_BUF_ATTR_KEEP_MAP = 1 << 2,
};

static uint32_t kernel_capabilities[FASTRPC_MAX_ATTRIBUTES -
FASTRPC_MAX_DSP_ATTRIBUTES] = {
	PERF_CAPABILITY	/* PERF_LOGGING_V2_SUPPORT feature is supported, unsupported = 0 */
};

struct virt_fastrpc_cmd {
	struct hlist_node hn;
	struct virt_fastrpc_msg *msg;
	u32 tid;	/* thread id */
	u32 cmd;	/* cmd type */
};

struct virt_fastrpc_sgl {
	u64 pv;		/* buffer physical address */
	u64 len;	/* buffer length */
};

struct virt_fastrpc_mapping {
	s32 fd;
	s32 refcount;
	u64 va;			/* virtual address */
	u64 len;		/* mmap length, not necessarily be page aligned */
	u32 attr;		/* mmap attribute */
	u32 nents;		/* number of map entries */
	struct virt_fastrpc_sgl sgl[0];	/* scatter list */
} __packed;

struct virt_fastrpc_buf {
	u32 attrs;
	u32 crc;
	u64 pv;	/* buffer virtual address */
	u64 buf_len;	/* buffer length */
	u64 offset;	/* buffer offset */
	u64 payload_len;	/* payload length */
} __packed;

struct virt_open_msg {
	struct virt_msg_hdr hdr;	/* virtio fastrpc message header */
	u32 domain;			/* DSP domain id */
	u32 pd;				/* DSP PD */
	u32 attrs;			/* DSP PD attributes */
} __packed;

struct virt_cap_msg {
	struct virt_msg_hdr hdr;	/* virtio fastrpc message header */
	u32 domain;		/* DSP domain id */
	u32 dsp_caps[FASTRPC_MAX_DSP_ATTRIBUTES];	/* DSP capability */
} __packed;

struct virt_control_msg {
	struct virt_msg_hdr hdr;	/* virtio fastrpc message header */
	u32 enable;			/* latency control enable */
	u32 latency;			/* latency value */
} __packed;

struct virt_invoke_msg {
	struct virt_msg_hdr hdr;	/* virtio fastrpc message header */
	u32 handle;			/* remote handle */
	u32 sc;				/* scalars describing the data */
	u32 attrs;
	struct virt_fastrpc_buf pra[0];	/* remote arguments list */
} __packed;

struct virt_mmap_msg {
	struct virt_msg_hdr hdr;	/* virtio fastrpc message header */
	u32 flags;			/* mmap flags */
	u64 vdsp;			/* dsp address */
	struct virt_fastrpc_mapping mmap;    /* map description */
} __packed;

struct virt_munmap_msg {
	struct virt_msg_hdr hdr;	/* virtio fastrpc message header */
	u64 vdsp;			/* dsp address */
	u64 size;			/* mmap length */
} __packed;

struct virt_mem_map_msg {
	struct virt_msg_hdr hdr;    /* virtio fastrpc message header */
	s32 offset;          /* map offset, currently it has to be 0*/
	u32 flags;           /* flags defined in enum fastrpc_map_flags */
	s32 attrs;           /* attrs passed in from user for SMMU map, currently it is 0 */
	u64 raddr;           /* dsp address return from BE */
	struct virt_fastrpc_mapping mmap;    /* map description */
} __packed;

struct virt_mem_unmap_msg {
	struct virt_msg_hdr hdr;    /* virtio fastrpc message header */
	s32 fd;		/* ion fd */
	u64 len;	/* mapping length*/
	u64 raddr;	/* dsp address return from BE */
} __packed;

struct virt_munmap_fd_msg {
	struct virt_msg_hdr hdr;	/* message header */
	s32 fd;
	u32 flags;		/* control flags */
	u32 map_entries;	/* how many maps found with fd */
	u64 va;			/* buffer virtual address */
	u64 len;		/* buffer length */
} __packed;

static inline uint64_t ptr_to_uint64(void *ptr)
{
	uint64_t addr = (uint64_t)((uintptr_t)ptr);
	return addr;
}

static inline int64_t getnstimediff(struct timespec64 *start)
{
	int64_t ns;
	struct timespec64 ts, b;

	ktime_get_real_ts64(&ts);
	b = timespec64_sub(ts, *start);
	ns = timespec64_to_ns(&b);
	return ns;
}

enum fastrpc_proc_attr {
	/* Macro for Debug attr */
	FASTRPC_MODE_DEBUG	= 1 << 0,
	/* Macro for Ptrace */
	FASTRPC_MODE_PTRACE	= 1 << 1,
	/* Macro for CRC Check */
	FASTRPC_MODE_CRC	= 1 << 2,
	/* Macro for Unsigned PD */
	FASTRPC_MODE_UNSIGNED_MODULE	= 1 << 3,
	/* Macro for Adaptive QoS */
	FASTRPC_MODE_ADAPTIVE_QOS	= 1 << 4,
	/* Macro for System Process */
	FASTRPC_MODE_SYSTEM_PROCESS	= 1 << 5,
	/* Macro for Prvileged Process */
	FASTRPC_MODE_PRIVILEGED	= (1 << 6),
};

static void virt_free_msg(struct vfastrpc_file *vfl, struct virt_fastrpc_msg *msg)
{
	struct vfastrpc_apps *me = vfl->apps;
	unsigned long flags;

	spin_lock_irqsave(&me->msglock, flags);
	if (me->msgtable[msg->msgid] == msg)
		me->msgtable[msg->msgid] = NULL;
	else
		dev_err(me->dev, "can't find msg %d in table\n", msg->msgid);
	spin_unlock_irqrestore(&me->msglock, flags);

	kfree(msg);
}

static struct virt_fastrpc_msg *virt_alloc_msg(struct vfastrpc_file *vfl, int size)
{
	struct vfastrpc_apps *me = vfl->apps;
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

	buf = get_a_tx_buf(vfl);
	if (!buf) {
		dev_err(me->dev, "can't get tx buffer\n");
		virt_free_msg(vfl, msg);
		return NULL;
	}

	msg->txbuf = buf;
	return msg;
}

static void context_list_ctor(struct fastrpc_ctx_lst *me)
{
	INIT_HLIST_HEAD(&me->interrupted);
	INIT_HLIST_HEAD(&me->pending);
	INIT_LIST_HEAD(&me->async_queue);
}

struct vfastrpc_file *vfastrpc_file_alloc(void)
{
	int err = 0;
	struct vfastrpc_file *vfl = NULL;
	struct fastrpc_file *fl = NULL;

	VERIFY(err, NULL != (vfl = kzalloc(sizeof(*vfl), GFP_KERNEL)));
	if (err)
		return NULL;
	fl = to_fastrpc_file(vfl);
	context_list_ctor(&fl->clst);
	spin_lock_init(&fl->hlock);
	spin_lock_init(&fl->aqlock);
	INIT_HLIST_HEAD(&fl->maps);
	INIT_HLIST_HEAD(&fl->cached_bufs);
	fl->num_cached_buf = 0;
	INIT_HLIST_HEAD(&fl->remote_bufs);
	INIT_HLIST_HEAD(&vfl->interrupted_cmds);
	init_waitqueue_head(&fl->async_wait_queue);
	fl->tgid = current->tgid;
	fl->tgid_open = current->tgid;
	fl->mode = FASTRPC_MODE_SERIAL;
	vfl->domain = -1;
	fl->cid = -1;
	fl->dsp_proc_init = 0;
	mutex_init(&fl->internal_map_mutex);
	mutex_init(&fl->map_mutex);
	return vfl;
}

static void context_free(struct vfastrpc_invoke_ctx *ctx)
{
	int i;
	struct vfastrpc_file *vfl = ctx->vfl;
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct vfastrpc_apps *me = vfl->apps;
	struct virt_invoke_msg *rsp = NULL;
	int nbufs = REMOTE_SCALARS_INBUFS(ctx->sc) +
		REMOTE_SCALARS_OUTBUFS(ctx->sc);

	spin_lock(&fl->hlock);
	hlist_del_init(&ctx->hn);
	spin_unlock(&fl->hlock);

	mutex_lock(&fl->map_mutex);
	for (i = 0; i < nbufs; i++)
		vfastrpc_mmap_free(vfl, ctx->maps[i], 0);
	mutex_unlock(&fl->map_mutex);

	if (ctx->msg) {
		rsp = ctx->msg->rxbuf;
		if (rsp)
			vfastrpc_rxbuf_send(vfl, rsp, me->buf_size);

		virt_free_msg(vfl, ctx->msg);
		ctx->msg = NULL;
	}
	if (ctx->desc) {
		for (i = 0; i < nbufs; i++) {
			if (ctx->desc[i].buf)
				vfastrpc_buf_free(ctx->desc[i].buf, 1);
		}
		kfree(ctx->desc);
		ctx->desc = NULL;
	}

	if (fl->profile)
		kfree(ctx->perf);

	kfree(ctx);
}

static void vfastrpc_context_list_dtor(struct vfastrpc_file *vfl)
{
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct fastrpc_ctx_lst *clst = &fl->clst;
	struct vfastrpc_invoke_ctx *ictx = NULL, *ctxfree;
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

static void vfastrpc_remote_buf_list_free(struct vfastrpc_file *vfl)
{
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct vfastrpc_buf *buf, *free;

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
			vfastrpc_buf_free(free, 0);
	} while (free);
}

static void vfastrpc_cached_buf_list_free(struct vfastrpc_file *vfl)
{
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct vfastrpc_buf *buf, *free;

	do {
		struct hlist_node *n;

		free = NULL;
		spin_lock(&fl->hlock);
		hlist_for_each_entry_safe(buf, n, &fl->cached_bufs, hn) {
			hlist_del_init(&buf->hn);
			fl->num_cached_buf--;
			free = buf;
			break;
		}
		spin_unlock(&fl->hlock);
		if (free)
			vfastrpc_buf_free(free, 0);
	} while (free);
}

static void vfastrpc_interrupted_cmd_list_free(struct vfastrpc_file *vfl)
{
	struct virt_fastrpc_cmd *cmd, *free;
	struct vfastrpc_apps *me = vfl->apps;
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct virt_invoke_msg *rsp = NULL;

	do {
		struct hlist_node *n;

		free = NULL;
		spin_lock(&fl->hlock);
		hlist_for_each_entry_safe(cmd, n, &vfl->interrupted_cmds, hn) {
			hlist_del_init(&cmd->hn);
			free = cmd;
			break;
		}
		spin_unlock(&fl->hlock);
		if (free) {
			if (free->msg) {
				rsp = free->msg->rxbuf;
				if (rsp)
					vfastrpc_rxbuf_send(vfl, rsp, me->buf_size);
				virt_free_msg(vfl, free->msg);
				free->msg = NULL;
			}
			kfree(free);
		}
	} while (free);
}

static int virt_fastrpc_close(struct vfastrpc_file *vfl)
{
	struct vfastrpc_apps *me = vfl->apps;
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct virt_msg_hdr *vmsg, *rsp = NULL;
	struct virt_fastrpc_msg *msg;
	int err;

	if (fl->cid < 0) {
		dev_err(me->dev, "close: channel id %d is invalid\n", fl->cid);
		return -EINVAL;
	}

	msg = virt_alloc_msg(vfl, sizeof(*vmsg));
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

	err = vfastrpc_txbuf_send(vfl, vmsg, sizeof(*vmsg));
	if (err)
		goto bail;

	wait_for_completion(&msg->work);

	rsp = msg->rxbuf;
	if (!rsp)
		goto bail;

	err = rsp->result;
bail:
	if (rsp)
		vfastrpc_rxbuf_send(vfl, rsp, me->buf_size);

	virt_free_msg(vfl, msg);

	return err;
}

int vfastrpc_file_free(struct vfastrpc_file *vfl)
{
	struct fastrpc_file *fl = vfl ? to_fastrpc_file(vfl) : NULL;
	struct vfastrpc_mmap *map = NULL, *lmap = NULL;
	unsigned long flags;

	if (!vfl || !fl)
		return 0;

	spin_lock(&fl->hlock);
	fl->file_close = 1;
	spin_unlock(&fl->hlock);

	debugfs_remove(fl->debugfs_file);
	kfree(fl->debug_buf);

	/* This cmd is only required when PD is opened on DSP */
	if (fl->dsp_proc_init == 1)
		virt_fastrpc_close(vfl);

	/* Dummy wake up to exit Async worker thread */
	spin_lock_irqsave(&fl->aqlock, flags);
	atomic_add(1, &fl->async_queue_job_count);
	wake_up_interruptible(&fl->async_wait_queue);
	spin_unlock_irqrestore(&fl->aqlock, flags);

	vfastrpc_context_list_dtor(vfl);
	vfastrpc_cached_buf_list_free(vfl);
	vfastrpc_remote_buf_list_free(vfl);
	vfastrpc_interrupted_cmd_list_free(vfl);

	mutex_lock(&fl->map_mutex);
	do {
		struct hlist_node *n = NULL;

		lmap = NULL;
		hlist_for_each_entry_safe(map, n, &fl->maps, hn) {
			lmap = map;
			break;
		}
		vfastrpc_mmap_free(vfl, lmap, 1);
	} while (lmap);
	mutex_unlock(&fl->map_mutex);

	mutex_destroy(&fl->map_mutex);
	mutex_destroy(&fl->internal_map_mutex);
	kfree(fl);
	return 0;
}

static int context_restore_interrupted(struct vfastrpc_file *vfl,
					struct fastrpc_ioctl_invoke *invoke,
					struct vfastrpc_invoke_ctx **po)
{
	int err = 0;
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct vfastrpc_apps *me = vfl->apps;
	struct vfastrpc_invoke_ctx *ctx = NULL, *ictx = NULL;
	struct hlist_node *n;

	spin_lock(&fl->hlock);
	hlist_for_each_entry_safe(ictx, n, &fl->clst.interrupted, hn) {
		if (ictx->pid == current->pid) {
			if (invoke->sc != ictx->sc || ictx->vfl != vfl) {
				err = -EINVAL;
				dev_err(me->dev,
					"interrupted sc (0x%x) or vfl (%pK) does not match with invoke sc (0x%x) or vfl (%pK)\n",
					ictx->sc, ictx->vfl, invoke->sc, vfl);
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

static int context_alloc(struct vfastrpc_file *vfl,
			struct fastrpc_ioctl_invoke_async *invokefd,
			struct vfastrpc_invoke_ctx **po)
{
	int err = 0, bufs, size = 0;
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct vfastrpc_invoke_ctx *ctx = NULL;
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
	INIT_LIST_HEAD(&ctx->asyncn);
	ctx->vfl = vfl;
	ctx->maps = (struct vfastrpc_mmap **)(&ctx[1]);
	ctx->lpra = (remote_arg_t *)(&ctx->maps[bufs]);
	ctx->fds = (int *)(&ctx->lpra[bufs]);
	ctx->attrs = (unsigned int *)(&ctx->fds[bufs]);

	K_COPY_FROM_USER(err, fl->is_compat, (void *)ctx->lpra, invoke->pra,
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
	ctx->crc = (uint32_t *)invokefd->crc;
	ctx->perf_dsp = (uint64_t *)invokefd->perf_dsp;
	ctx->perf_kernel = (uint64_t *)invokefd->perf_kernel;

	if (fl->profile) {
		ctx->perf = kzalloc(sizeof(*(ctx->perf)), GFP_KERNEL);
		VERIFY(err, !IS_ERR_OR_NULL(ctx->perf));
		if (err) {
			kfree(ctx->perf);
			err = -ENOMEM;
			goto bail;
		}
		memset(ctx->perf, 0, sizeof(*(ctx->perf)));
		ctx->perf->tid = fl->tgid;
	}

	if (invokefd->job) {
		K_COPY_FROM_USER(err, 0, &ctx->asyncjob, invokefd->job,
				sizeof(ctx->asyncjob));
		if (err)
			goto bail;
	}

	spin_lock(&fl->hlock);
	hlist_add_head(&ctx->hn, &clst->pending);
	spin_unlock(&fl->hlock);

	*po = ctx;
bail:
	if (ctx && err)
		context_free(ctx);
	return err;
}

static void context_save_interrupted(struct vfastrpc_invoke_ctx *ctx)
{
	struct vfastrpc_file *vfl = ctx->vfl;
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct fastrpc_ctx_lst *clst = &fl->clst;

	spin_lock(&fl->hlock);
	hlist_del_init(&ctx->hn);
	hlist_add_head(&ctx->hn, &clst->interrupted);
	spin_unlock(&fl->hlock);
}

static void calc_compare_crc(struct vfastrpc_invoke_ctx *ctx, unsigned char *va,
		int len, uint32_t *lcrc, uint32_t *rcrc)
{
	struct vfastrpc_file *vfl = ctx->vfl;
	struct vfastrpc_apps *me = vfl->apps;

	if (unlikely(ctx->crc)) {
		*lcrc = crc32_be(0, va, len);
		if (rcrc && (*rcrc != *lcrc))
			dev_err(me->dev, "FE/BE crc is not matched 0x%x:0x%x\n",
					*lcrc, *rcrc);
	}
}

static int get_args(struct vfastrpc_invoke_ctx *ctx)
{
	struct vfastrpc_file *vfl = ctx->vfl;
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct vfastrpc_apps *me = vfl->apps;
	struct virt_invoke_msg *vmsg;
	int inbufs = REMOTE_SCALARS_INBUFS(ctx->sc);
	int outbufs = REMOTE_SCALARS_OUTBUFS(ctx->sc);
	int i, err = 0, bufs, handles, total;
	remote_arg_t *lpra = ctx->lpra;
	int *fds = ctx->fds;
	struct virt_fastrpc_buf *rpra;
	struct virt_fastrpc_mapping *vmmap;
	uint64_t *fdlist;
	uint32_t *crclist = NULL;
	uint32_t *early_hint = NULL;
	uint64_t *perf_dsp_list = NULL;
	unsigned int *attrs = ctx->attrs;
	struct vfastrpc_mmap **maps = ctx->maps;
	size_t copylen = 0, size = 0, handlelen = 0, metalen;
	char *payload;
	uint64_t *perf_counter = NULL;

	bufs = inbufs + outbufs;
	handles = REMOTE_SCALARS_INHANDLES(ctx->sc)
		+ REMOTE_SCALARS_OUTHANDLES(ctx->sc);
	total = REMOTE_SCALARS_LENGTH(ctx->sc);

	if (fl->profile)
		perf_counter = (uint64_t *)ctx->perf + PERF_COUNT;

	/* calculate len required for copying */
	PERF(fl->profile, GET_COUNTER(perf_counter, PERF_MAP),
	for (i = 0; i < bufs; i++) {
		size_t len = lpra[i].buf.len;

		if (!len)
			continue;
		if (fds && (fds[i] != -1) && attrs) {
			/* map ion buffers */
			mutex_lock(&fl->map_mutex);
			err = vfastrpc_mmap_create(vfl, fds[i], attrs[i],
					(uintptr_t)lpra[i].buf.pv,
					len, 0, &maps[i]);
			mutex_unlock(&fl->map_mutex);
			if (err)
				goto bail;
			len = SIZE_OF_MAPPING(maps[i]->table->nents);
		}
		copylen += len;
		if (i < inbufs)
			ctx->outbufs_offset += len;
	}
	PERF_END);

	mutex_lock(&fl->map_mutex);
	for (i = bufs; i < total; i++) {
		int dmaflags = 0;

		if (attrs && (attrs[i] & FASTRPC_ATTR_NOMAP))
			dmaflags = FASTRPC_DMAHANDLE_NOMAP;
		if (fds && (fds[i] != -1) && attrs) {
			err = vfastrpc_mmap_create(vfl, fds[i], attrs[i],
					0, 0, dmaflags, &maps[i]);
			if (err) {
				mutex_unlock(&fl->map_mutex);
				goto bail;
			}
			handlelen += SIZE_OF_MAPPING(maps[i]->table->nents);
		}
	}
	mutex_unlock(&fl->map_mutex);

	metalen = sizeof(*vmsg) + total * sizeof(*rpra)
		+ sizeof(uint64_t) * M_FDLIST + sizeof(uint32_t) * M_CRCLIST
		+ sizeof(uint32_t) + sizeof(uint64_t) * M_DSP_PERF_LIST;
	size = metalen + copylen + handlelen;
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
				len = SIZE_OF_MAPPING(maps[i]->table->nents);
				ctx->desc[i].type = VFASTRPC_BUF_TYPE_ION;
			} else if (len < PAGE_SIZE) {
				ctx->desc[i].type = VFASTRPC_BUF_TYPE_NORMAL;
			} else {
				ctx->desc[i].type = VFASTRPC_BUF_TYPE_INTERNAL;
				len = PAGE_ALIGN(len);
				err = vfastrpc_buf_alloc(vfl, len, 0,
						0, 0, PAGE_KERNEL, &ctx->desc[i].buf);
				if (err)
					goto bail;
				ctx->desc[i].buf->map_attr = VFASTRPC_MAP_ATTR_CACHED;
				len = SIZE_OF_MAPPING(ctx->desc[i].buf->sgt.nents);
			}
			copylen += len;
			if (i < inbufs)
				ctx->outbufs_offset += len;
		}
		size = metalen + copylen + handlelen;
	}

	ctx->msg = virt_alloc_msg(vfl, size);
	if (!ctx->msg) {
		err = -ENOMEM;
		goto bail;
	}
	ctx->msg->ctx = ctx;

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
	vmsg->attrs = ctx->crc ? VIRTIO_FASTRPC_INVOKE_CRC : 0;
	rpra = (struct virt_fastrpc_buf *)vmsg->pra;
	fdlist = (uint64_t *)(&rpra[total]);
	crclist = (uint32_t *)&fdlist[M_FDLIST];
	early_hint = (uint32_t *)&crclist[M_CRCLIST];
	perf_dsp_list = (uint64_t *)&early_hint[1];
	payload = (char *)&perf_dsp_list[M_DSP_PERF_LIST];

	memset(fdlist, 0, sizeof(uint64_t) * M_FDLIST + sizeof(uint32_t) * M_CRCLIST +
		sizeof(uint64_t) * M_DSP_PERF_LIST + sizeof(uint32_t));

	PERF(fl->profile, GET_COUNTER(perf_counter, PERF_COPY),
	for (i = 0; i < bufs; i++) {
		size_t len = lpra[i].buf.len;
		struct sg_table *table;
		struct virt_fastrpc_sgl *sgbuf;
		struct scatterlist *sgl = NULL;
		uint64_t buf = ptr_to_uint64(lpra[i].buf.pv);
		struct vm_area_struct *vma;
		int index = 0;
		uint64_t offset = 0;

		if (maps[i]) {
			table = maps[i]->table;
			rpra[i].attrs = VFASTRPC_BUF_ATTR_ION;
			if (attrs && (attrs[i] & FASTRPC_ATTR_KEEP_MAP))
				rpra[i].attrs |= VFASTRPC_BUF_ATTR_KEEP_MAP;
			rpra[i].crc = 0;
			rpra[i].pv = buf;
			rpra[i].buf_len = len;

			down_read(&current->mm->mmap_lock);
			VERIFY(err, NULL != (vma = find_vma(current->mm, maps[i]->va)));
			if (err) {
				up_read(&current->mm->mmap_lock);
				goto bail;
			}
			offset = buf - vma->vm_start;
			up_read(&current->mm->mmap_lock);
			VERIFY(err, offset + len <= (uintptr_t)maps[i]->size);
			if (err) {
				dev_err(me->dev,
						"buffer address is invalid for the fd passed for %d address 0x%llx and size %zu\n",
						i, (uintptr_t)lpra[i].buf.pv, lpra[i].buf.len);
				err = -EFAULT;
				goto bail;
			}
			rpra[i].offset = offset;
			rpra[i].payload_len = SIZE_OF_MAPPING(table->nents);

			vmmap = (struct virt_fastrpc_mapping *)payload;
			vmmap->fd = maps[i]->fd;
			vmmap->refcount = maps[i]->refs;
			vmmap->va = maps[i]->va;
			vmmap->len = maps[i]->len;
			vmmap->attr = VFASTRPC_MAP_ATTR_CACHED;
			vmmap->nents = table->nents;

			sgbuf = (struct virt_fastrpc_sgl *)vmmap->sgl;
			for_each_sg(table->sgl, sgl, table->nents, index) {
				sgbuf[index].pv = sg_dma_address(sgl);
				sgbuf[index].len = sg_dma_len(sgl);
			}

			calc_compare_crc(ctx, (uint8_t *)payload, (int)rpra[i].payload_len,
					&(rpra[i].crc), NULL);

			payload += rpra[i].payload_len;
		} else if (ctx->desc &&
			   ctx->desc[i].type == VFASTRPC_BUF_TYPE_INTERNAL) {
			table = &ctx->desc[i].buf->sgt;
			rpra[i].attrs = VFASTRPC_BUF_ATTR_INTERNAL;
			rpra[i].crc = 0;
			rpra[i].pv = buf;
			rpra[i].buf_len = len;
			rpra[i].offset = 0;
			rpra[i].payload_len = SIZE_OF_MAPPING(table->nents);

			vmmap = (struct virt_fastrpc_mapping *)payload;
			memset(vmmap, 0, sizeof(struct virt_fastrpc_mapping));
			vmmap->fd = -1;
			vmmap->refcount = 1;
			vmmap->va = (u64)(ctx->desc[i].buf->va);
			vmmap->len = ctx->desc[i].buf->size;
			vmmap->attr = ctx->desc[i].buf->map_attr;
			vmmap->nents = table->nents;

			sgbuf = (struct virt_fastrpc_sgl *)vmmap->sgl;
			for_each_sg(table->sgl, sgl, table->nents, index) {
				sgbuf[index].pv = page_to_phys(sg_page(sgl));
				sgbuf[index].len = sgl->length;
			}

			calc_compare_crc(ctx, (uint8_t *)payload, (int)rpra[i].payload_len,
					&(rpra[i].crc), NULL);

			/*
			 * no need to sync cache even though internal buffers are
			 * cached, since BE will do SMMU mapping with the same cache
			 * attribute to enable the IO coherency
			 */
			if (i < inbufs && len) {
				K_COPY_FROM_USER(err, 0, ctx->desc[i].buf->va,
						lpra[i].buf.pv, len);
				if (err)
					goto bail;
			}

			payload += rpra[i].payload_len;
		} else {
			/* copy non ion buffers */
			rpra[i].attrs = 0;
			rpra[i].crc = 0;
			rpra[i].pv = buf;
			rpra[i].buf_len = len;
			rpra[i].offset = 0;
			rpra[i].payload_len = len;

			if (i < inbufs && len) {
				K_COPY_FROM_USER(err, 0, payload,
						lpra[i].buf.pv, len);
				if (err)
					goto bail;
				calc_compare_crc(ctx, (uint8_t *)payload, (int)len,
						&(rpra[i].crc), NULL);
			}
			payload += len;
		}
	}
	PERF_END);

	for (i = bufs; i < total; i++) {
		struct sg_table *table;
		struct virt_fastrpc_sgl *sgbuf;
		struct scatterlist *sgl = NULL;
		int index = 0;

		if (fds && maps[i]) {
			/* copy dma handle sglist to data area */
			table = maps[i]->table;
			rpra[i].attrs = VFASTRPC_BUF_ATTR_ION;
			rpra[i].crc = 0;
			rpra[i].pv = 0;
			rpra[i].buf_len = lpra[i].buf.len;
			rpra[i].offset = (uint64_t)(uintptr_t)lpra[i].buf.pv;
			rpra[i].payload_len = SIZE_OF_MAPPING(table->nents);

			vmmap = (struct virt_fastrpc_mapping *)payload;
			vmmap->fd = maps[i]->fd;
			vmmap->refcount = maps[i]->refs;
			vmmap->va = maps[i]->va;
			vmmap->len = lpra[i].buf.len;
			vmmap->attr = VFASTRPC_MAP_ATTR_CACHED;
			vmmap->nents = table->nents;

			sgbuf = (struct virt_fastrpc_sgl *)vmmap->sgl;
			for_each_sg(table->sgl, sgl, table->nents, index) {
				sgbuf[index].pv = sg_dma_address(sgl);
				sgbuf[index].len = sg_dma_len(sgl);
			}

			calc_compare_crc(ctx, (uint8_t *)payload, (int)rpra[i].payload_len,
					&(rpra[i].crc), NULL);

			payload += rpra[i].payload_len;
		}
	}
bail:
	return err;
}

static int put_args(struct vfastrpc_invoke_ctx *ctx)
{
	int err = 0;
	struct vfastrpc_file *vfl = ctx->vfl;
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct vfastrpc_apps *me = vfl->apps;
	struct virt_fastrpc_msg *msg = ctx->msg;
	struct virt_invoke_msg *rsp = NULL;
	int inbufs = REMOTE_SCALARS_INBUFS(ctx->sc);
	int outbufs = REMOTE_SCALARS_OUTBUFS(ctx->sc);
	int i, bufs, handles, total;
	remote_arg_t *lpra = ctx->lpra;
	struct virt_fastrpc_buf *rpra;
	uint32_t crc;
	uint64_t *fdlist;
	uint32_t *crclist = NULL;
	uint32_t *early_hint = NULL;
	uint64_t *perf_dsp_list = NULL;
	struct vfastrpc_mmap **maps = ctx->maps, *mmap = NULL;
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
	fdlist = (uint64_t *)(&rpra[total]);
	crclist = (uint32_t *)(fdlist + M_FDLIST);
	early_hint = (uint32_t *)(crclist + M_CRCLIST);
	perf_dsp_list = (uint64_t *)(early_hint + 1);
	payload = (char *)&perf_dsp_list[M_DSP_PERF_LIST] + ctx->outbufs_offset;

	for (i = inbufs; i < bufs; i++) {
		if (maps[i]) {
			mutex_lock(&fl->map_mutex);
			vfastrpc_mmap_free(vfl, maps[i], 0);
			mutex_unlock(&fl->map_mutex);
			maps[i] = NULL;
		} else if (ctx->desc &&
			   ctx->desc[i].type == VFASTRPC_BUF_TYPE_INTERNAL) {
			K_COPY_TO_USER(err, 0, lpra[i].buf.pv,
					ctx->desc[i].buf->va, lpra[i].buf.len);
			if (err)
				goto bail;
		} else {
			calc_compare_crc(ctx, (uint8_t *)payload, (int)rpra[i].buf_len,
					&crc, &(rpra[i].crc));

			K_COPY_TO_USER(err, 0, lpra[i].buf.pv,
					payload, rpra[i].buf_len);
			if (err)
				goto bail;
		}
		payload += rpra[i].payload_len;
	}

	mutex_lock(&fl->map_mutex);
	if (total) {
		for (i = 0; i < M_FDLIST; i++) {
			if (!fdlist[i])
				break;
			if (!vfastrpc_mmap_find(vfl, (int)fdlist[i], 0, 0,
						0, 0, &mmap))
				vfastrpc_mmap_free(vfl, mmap, 0);
		}
	}
	mutex_unlock(&fl->map_mutex);
	if (ctx->crc && crclist && rpra)
		K_COPY_TO_USER(err, 0, ctx->crc,
				crclist, M_CRCLIST * sizeof(uint32_t));

bail:
	return err;
}

static int virt_fastrpc_invoke(struct vfastrpc_file *vfl, struct vfastrpc_invoke_ctx *ctx)
{
	struct vfastrpc_apps *me = vfl->apps;
	struct virt_fastrpc_msg *msg = ctx->msg;
	struct virt_invoke_msg *vmsg;
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

	err = vfastrpc_txbuf_send(vfl, vmsg, ctx->size);
bail:
	return err;
}

static void vfastrpc_update_invoke_count(uint32_t handle, uint64_t *perf_counter,
		struct timespec64 *invoket)
{
	if (handle != FASTRPC_STATIC_HANDLE_LISTENER) {
		uint64_t *count = GET_COUNTER(perf_counter, PERF_INVOKE);

		if (count)
			*count += getnstimediff(invoket);
	}
	if (handle > FASTRPC_STATIC_HANDLE_MAX) {
		uint64_t *count = GET_COUNTER(perf_counter, PERF_COUNT);

		if (count)
			*count += 1;
	}
}

void vfastrpc_queue_completed_async_job(struct vfastrpc_invoke_ctx *ctx)
{
	struct vfastrpc_file *vfl = ctx->vfl;
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	unsigned long flags;

	spin_lock_irqsave(&fl->aqlock, flags);
	list_add_tail(&ctx->asyncn, &fl->clst.async_queue);
	atomic_add(1, &fl->async_queue_job_count);
	wake_up_interruptible(&fl->async_wait_queue);
	spin_unlock_irqrestore(&fl->aqlock, flags);
}

int vfastrpc_internal_invoke(struct vfastrpc_file *vfl,
			uint32_t mode, struct fastrpc_ioctl_invoke_async *inv)
{
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct fastrpc_ioctl_invoke *invoke = &inv->inv;
	struct vfastrpc_apps *me = vfl->apps;
	struct vfastrpc_invoke_ctx *ctx = NULL;
	int err = 0, interrupted = 0;
	struct timespec64 invoket = {0};
	uint64_t *perf_counter = NULL;
	bool isasyncinvoke = false;

	VERIFY(err, invoke->handle != FASTRPC_STATIC_HANDLE_KERNEL);
	if (err) {
		dev_err(me->dev, "user application %s trying to send a kernel RPC message to channel %d\n",
			current->comm, vfl->domain);
		goto bail;
	}

	VERIFY(err, vfl->domain >= 0 && vfl->domain < me->num_channels);
	if (err) {
		dev_err(me->dev, "user application %s domain is not set\n",
				current->comm);
		err = -EBADR;
		goto bail;
	}

	if (fl->profile)
		ktime_get_real_ts64(&invoket);

	VERIFY(err, 0 == context_restore_interrupted(vfl, invoke, &ctx));
	if (err)
		goto bail;
	if (ctx)
		goto wait;

	VERIFY(err, 0 == context_alloc(vfl, inv, &ctx));
	if (err)
		goto bail;
	isasyncinvoke = (ctx->asyncjob.isasyncjob ? true : false);

	if (fl->profile)
		perf_counter = (uint64_t *)ctx->perf + PERF_COUNT;

	PERF(fl->profile, GET_COUNTER(perf_counter, PERF_GETARGS),
	VERIFY(err, 0 == get_args(ctx));
	PERF_END);
	if (err)
		goto bail;

	PERF(fl->profile, GET_COUNTER(perf_counter, PERF_LINK),
	VERIFY(err, 0 == virt_fastrpc_invoke(vfl, ctx));
	PERF_END);
	if (err)
		goto bail;

	if (isasyncinvoke)
		goto invoke_end;
wait:
	interrupted = wait_for_completion_interruptible(&ctx->msg->work);
	VERIFY(err, 0 == (err = interrupted));
	if (err)
		goto bail;
	PERF(fl->profile, GET_COUNTER(perf_counter, PERF_PUTARGS),
	VERIFY(err, 0 == put_args(ctx));
	PERF_END);
	if (err)
		goto bail;
bail:
	if (ctx && interrupted == -ERESTARTSYS)
		context_save_interrupted(ctx);
	else if (ctx) {
		if (fl->profile && !interrupted)
			vfastrpc_update_invoke_count(invoke->handle,
					perf_counter, &invoket);

		if (fl->profile && ctx->perf && ctx->perf_kernel)
			K_COPY_TO_USER_WITHOUT_ERR(0, ctx->perf_kernel,
					ctx->perf, M_KERNEL_PERF_LIST*sizeof(uint64_t));
		context_free(ctx);
	}

invoke_end:
	if (fl->profile && !interrupted && isasyncinvoke)
		vfastrpc_update_invoke_count(invoke->handle, perf_counter,
				&invoket);
	return err;
}

static int vfastrpc_wait_on_async_queue(
			struct fastrpc_ioctl_async_response *async_res,
			struct vfastrpc_file *vfl)
{
	int err = 0, ierr = 0, interrupted = 0;
	struct vfastrpc_invoke_ctx *ctx = NULL, *ictx = NULL, *n = NULL;
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct virt_invoke_msg *rsp = NULL;
	unsigned long flags;
	uint64_t *perf_counter = NULL;

read_async_job:
	interrupted = wait_event_interruptible(fl->async_wait_queue,
				atomic_read(&fl->async_queue_job_count));

	if (fl->file_close >= 1) {
		err = -EBADF;
		goto bail;
	}
	VERIFY(err, 0 == (err = interrupted));
	if (err)
		goto bail;

	spin_lock_irqsave(&fl->aqlock, flags);
	list_for_each_entry_safe(ictx, n, &fl->clst.async_queue, asyncn) {
		list_del_init(&ictx->asyncn);
		atomic_sub(1, &fl->async_queue_job_count);
		ctx = ictx;
		break;
	}
	spin_unlock_irqrestore(&fl->aqlock, flags);
	if (fl->profile && ctx)
		perf_counter = (uint64_t *)ctx->perf + PERF_COUNT;
	if (ctx) {
		async_res->jobid = ctx->asyncjob.jobid;
		rsp = (struct virt_invoke_msg *)ctx->msg->rxbuf;
		async_res->result = rsp->hdr.result;
		async_res->handle = ctx->handle;
		async_res->sc = ctx->sc;
		async_res->perf_dsp = (uint64_t *)ctx->perf_dsp;
		async_res->perf_kernel = (uint64_t *)ctx->perf_kernel;

		if (async_res->result != 0)
			goto bail;
		PERF(fl->profile, GET_COUNTER(perf_counter, PERF_PUTARGS),
		VERIFY(ierr, 0 == (ierr = put_args(ctx)));
		PERF_END);
		if (ierr)
			goto bail;
	} else {
		dev_err(vfl->apps->dev, "Invalid async job wake up\n");
		goto read_async_job;
	}
bail:
	if (ierr)
		async_res->result = ierr;
	if (ctx) {
		if (ctx->perf && ctx->perf_kernel &&
				ctx->handle > FASTRPC_STATIC_HANDLE_MAX)
			K_COPY_TO_USER_WITHOUT_ERR(0, ctx->perf_kernel,
					ctx->perf, M_KERNEL_PERF_LIST * sizeof(uint64_t));
		context_free(ctx);
	}
	return err;
}

static int vfastrpc_get_async_response(
		struct fastrpc_ioctl_async_response *async_res,
			void *param, struct vfastrpc_file *vfl)
{
	int err = 0;

	err = vfastrpc_wait_on_async_queue(async_res, vfl);
	if (err)
		goto bail;
	K_COPY_TO_USER(err, 0, param, async_res,
			sizeof(struct fastrpc_ioctl_async_response));
bail:
	return err;
}

int vfastrpc_internal_invoke2(struct vfastrpc_file *vfl,
				struct fastrpc_ioctl_invoke2 *inv2)
{
	union {
		struct fastrpc_ioctl_invoke_async inv;
		struct fastrpc_ioctl_async_response async_res;
	} p;
	struct fastrpc_dsp_capabilities *dsp_cap_ptr = NULL;
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	uint32_t size = 0;
	int err = 0, domain = vfl->domain;

	if (inv2->req == FASTRPC_INVOKE2_ASYNC ||
		inv2->req == FASTRPC_INVOKE2_ASYNC_RESPONSE) {
		VERIFY(err, domain == CDSP_DOMAIN_ID || domain == CDSP1_DOMAIN_ID);
		if (err)
			goto bail;

		dsp_cap_ptr = &vfl->apps->channel[domain].dsp_cap_kernel;
		VERIFY(err, dsp_cap_ptr->dsp_attributes[ASYNC_FASTRPC_CAP] == 1);
		if (err) {
			err = -EPROTONOSUPPORT;
			goto bail;
		}
	}
	switch (inv2->req) {
	case FASTRPC_INVOKE2_ASYNC:
		size = sizeof(struct fastrpc_ioctl_invoke_async);
		VERIFY(err, size >= inv2->size);
		if (err) {
			err = -EBADE;
			goto bail;
		}

		K_COPY_FROM_USER(err, 0, &p.inv, (void *)inv2->invparam, size);
		if (err)
			goto bail;

		VERIFY(err, 0 == (err = vfastrpc_internal_invoke(vfl, fl->mode,
						&p.inv)));
		if (err)
			goto bail;
		break;
	case FASTRPC_INVOKE2_ASYNC_RESPONSE:
		VERIFY(err,
		sizeof(struct fastrpc_ioctl_async_response) >= inv2->size);
		if (err) {
			err = -EBADE;
			goto bail;
		}
		err = vfastrpc_get_async_response(&p.async_res,
						(void *)inv2->invparam, vfl);
		break;
	case FASTRPC_INVOKE2_KERNEL_OPTIMIZATIONS:
		err = -ENOTTY;
		break;
	default:
		err = -ENOTTY;
		break;
	}
bail:
	return err;
}

static int virt_fastrpc_munmap(struct vfastrpc_file *vfl, uintptr_t raddr,
				size_t size)
{
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct vfastrpc_apps *me = vfl->apps;
	struct virt_munmap_msg *vmsg, *rsp = NULL;
	struct virt_fastrpc_msg *msg;
	int err;

	msg = virt_alloc_msg(vfl, sizeof(*vmsg));
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

	err = vfastrpc_txbuf_send(vfl, vmsg, sizeof(*vmsg));
	if (err)
		goto bail;

	wait_for_completion(&msg->work);

	rsp = msg->rxbuf;
	if (!rsp)
		goto bail;

	err = rsp->hdr.result;
bail:
	if (rsp)
		vfastrpc_rxbuf_send(vfl, rsp, me->buf_size);
	virt_free_msg(vfl, msg);

	return err;
}

int vfastrpc_internal_munmap(struct vfastrpc_file *vfl,
				   struct fastrpc_ioctl_munmap *ud)
{
	int err = 0;
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct vfastrpc_apps *me = vfl->apps;
	struct vfastrpc_mmap *map = NULL;
	struct vfastrpc_buf *rbuf = NULL, *free = NULL;
	struct hlist_node *n;

	VERIFY(err, fl->dsp_proc_init == 1);
	if (err) {
		dev_err(me->dev, "%s: user application %s trying to unmap without initialization\n",
			 __func__, current->comm);
		err = -EBADR;
		return err;
	}
	mutex_lock(&fl->internal_map_mutex);
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
		VERIFY(err, !virt_fastrpc_munmap(vfl, free->raddr, free->size));
		if (err)
			goto bail;
		vfastrpc_buf_free(rbuf, 0);
		mutex_unlock(&fl->internal_map_mutex);
		return err;
	}

	mutex_lock(&fl->map_mutex);
	VERIFY(err, !vfastrpc_mmap_remove(vfl, -1, ud->vaddrout, ud->size, &map));
	mutex_unlock(&fl->map_mutex);
	if (err) {
		dev_err(me->dev, "mapping not found to unmap va 0x%lx, len 0x%x\n",
				ud->vaddrout, (unsigned int)ud->size);
		goto bail;
	}
	VERIFY(err, !virt_fastrpc_munmap(vfl, map->raddr, map->size));
	if (err)
		goto bail;
	mutex_lock(&fl->map_mutex);
	vfastrpc_mmap_free(vfl, map, 0);
	mutex_unlock(&fl->map_mutex);
bail:
	if (err && map) {
		mutex_lock(&fl->map_mutex);
		vfastrpc_mmap_add(vfl, map);
		mutex_unlock(&fl->map_mutex);
	}
	mutex_unlock(&fl->internal_map_mutex);
	return err;
}

static struct virt_fastrpc_msg *find_interrupted_cmds(
		struct vfastrpc_file *vfl, u32 cmd)
{
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct virt_fastrpc_msg *msg = NULL;
	struct virt_fastrpc_cmd *vcmd = NULL;
	struct hlist_node *n;

	spin_lock(&fl->hlock);
	hlist_for_each_entry_safe(vcmd, n, &vfl->interrupted_cmds, hn) {
		if (vcmd->tid == current->pid && vcmd->cmd == cmd) {
			msg = vcmd->msg;
			hlist_del_init(&vcmd->hn);
			kfree(vcmd);
			break;
		}
	}
	spin_unlock(&fl->hlock);
	return msg;
}

static int save_interrupted_cmds(struct vfastrpc_file *vfl,
		struct virt_fastrpc_msg *msg, u32 cmd)
{
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct virt_fastrpc_cmd *vcmd = NULL;
	int err = 0;

	VERIFY(err, NULL != (vcmd = kzalloc(sizeof(*vcmd), GFP_KERNEL)));
	if (err)
		return err;

	vcmd->tid = current->pid;
	vcmd->cmd = cmd;
	vcmd->msg = msg;
	INIT_HLIST_NODE(&vcmd->hn);

	spin_lock(&fl->hlock);
	hlist_add_head(&vcmd->hn, &vfl->interrupted_cmds);
	spin_unlock(&fl->hlock);
	return 0;
}

static int virt_fastrpc_munmap_fd(struct vfastrpc_file *vfl,
		struct fastrpc_ioctl_munmap_fd *ud, u32 map_entries,
		struct virt_fastrpc_msg *interrupted_msg)
{
	struct vfastrpc_apps *me = vfl->apps;
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct virt_munmap_fd_msg *vmsg, *rsp = NULL;
	struct virt_fastrpc_msg *msg;
	int err, total_size;
	int interrupted = 0;

	if (interrupted_msg) {
		msg = interrupted_msg;
		goto wait;
	}

	total_size = sizeof(*vmsg);

	msg = virt_alloc_msg(vfl, total_size);
	if (!msg)
		return -ENOMEM;

	vmsg = (struct virt_munmap_fd_msg *)msg->txbuf;
	vmsg->hdr.pid = fl->tgid;
	vmsg->hdr.tid = current->pid;
	vmsg->hdr.cid = fl->cid;
	vmsg->hdr.cmd = VIRTIO_FASTRPC_CMD_MUNMAP_FD;
	vmsg->hdr.len = total_size;
	vmsg->hdr.msgid = msg->msgid;
	vmsg->hdr.result = 0xffffffff;
	vmsg->fd = ud->fd;
	vmsg->flags = ud->flags;
	vmsg->map_entries = map_entries;
	vmsg->va = ud->va;
	vmsg->len = ud->len;

	err = vfastrpc_txbuf_send(vfl, vmsg, total_size);
	if (err)
		goto bail;

wait:
	interrupted = wait_for_completion_interruptible(&msg->work);
	VERIFY(err, 0 == (err = interrupted));
	if (err)
		goto bail;

	rsp = msg->rxbuf;
	if (!rsp)
		goto bail;

	err = rsp->hdr.result;
bail:
	if (interrupted == -ERESTARTSYS) {
		err = save_interrupted_cmds(vfl, msg, VIRTIO_FASTRPC_CMD_MUNMAP_FD);
	} else {
		if (rsp)
			vfastrpc_rxbuf_send(vfl, rsp, me->buf_size);
		virt_free_msg(vfl, msg);
	}
	return err;
}

int vfastrpc_internal_munmap_fd(struct vfastrpc_file *vfl,
				struct fastrpc_ioctl_munmap_fd *ud)
{
	int err = 0, err1 = 0;
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct vfastrpc_apps *me = vfl->apps;
	struct virt_fastrpc_msg *msg = NULL;
	u32 map_entries = 0;

	VERIFY(err, fl->dsp_proc_init == 1);
	if (err) {
		dev_err(me->dev, "%s: user application %s trying to unmap without initialization\n",
			__func__, current->comm);
		err = -EBADR;
		return err;
	}

	mutex_lock(&fl->internal_map_mutex);
	msg = find_interrupted_cmds(vfl, VIRTIO_FASTRPC_CMD_MUNMAP_FD);
	if (msg)
		goto wait;

	mutex_lock(&fl->map_mutex);
	err = vfastrpc_mmap_remove_fd(vfl, ud->fd, &map_entries);
	mutex_unlock(&fl->map_mutex);

	if (!map_entries)
		goto skip_notify_be;

wait:
	err1 = virt_fastrpc_munmap_fd(vfl, ud, map_entries, msg);
	if (err1)
		dev_err(me->dev, "BE munmap fd failed err = %d\n", err1);

skip_notify_be:
	mutex_unlock(&fl->internal_map_mutex);
	err = err1 ? err1 : err;
	return err;
}

static int virt_fastrpc_mmap(struct vfastrpc_file *vfl, uint32_t flags,
			struct scatterlist *table, uintptr_t *raddr,
			struct virt_fastrpc_mapping *mmap)
{
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct vfastrpc_apps *me = vfl->apps;
	struct virt_mmap_msg *vmsg, *rsp = NULL;
	struct virt_fastrpc_msg *msg;
	struct virt_fastrpc_sgl *sgbuf;
	int err, sgbuf_size, total_size;
	struct scatterlist *sgl = NULL;
	int sgl_index = 0;

	sgbuf_size = mmap->nents * sizeof(*sgbuf);
	total_size = sizeof(*vmsg) + sgbuf_size;

	msg = virt_alloc_msg(vfl, total_size);
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
	vmsg->vdsp = 0;
	memcpy(&(vmsg->mmap), mmap, sizeof(*mmap));
	sgbuf = vmsg->mmap.sgl;

	for_each_sg(table, sgl, mmap->nents, sgl_index) {
		if (sg_dma_len(sgl)) {
			sgbuf[sgl_index].pv = sg_dma_address(sgl);
			sgbuf[sgl_index].len = sg_dma_len(sgl);
		} else {
			sgbuf[sgl_index].pv = page_to_phys(sg_page(sgl));
			sgbuf[sgl_index].len = sgl->length;
		}
	}

	err = vfastrpc_txbuf_send(vfl, vmsg, total_size);
	if (err)
		goto bail;

	wait_for_completion(&msg->work);

	rsp = msg->rxbuf;
	if (!rsp)
		goto bail;

	err = rsp->hdr.result;
	if (err)
		goto bail;
	*raddr = (uintptr_t)rsp->vdsp;
bail:
	if (rsp)
		vfastrpc_rxbuf_send(vfl, rsp, me->buf_size);
	virt_free_msg(vfl, msg);
	return err;
}

int vfastrpc_internal_mmap(struct vfastrpc_file *vfl,
				 struct fastrpc_ioctl_mmap *ud)
{
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct vfastrpc_apps *me = vfl->apps;
	struct vfastrpc_mmap *map = NULL;
	struct vfastrpc_buf *rbuf = NULL;
	struct virt_fastrpc_mapping vmmap;
	unsigned long dma_attr = 0;
	uintptr_t raddr = 0;
	int err = 0;

	VERIFY(err, fl->dsp_proc_init == 1);
	if (err) {
		dev_err(me->dev, "%s: user application %s trying to map without initialization\n",
			__func__, current->comm);
		err = -EBADR;
		return err;
	}
	mutex_lock(&fl->internal_map_mutex);
	if (ud->flags == ADSP_MMAP_ADD_PAGES) {
		if (ud->vaddrin) {
			err = -EINVAL;
			dev_err(me->dev, "%s: %s: ERROR: adding user allocated pages is not supported\n",
					current->comm, __func__);
			goto bail;
		}
		dma_attr = DMA_ATTR_NO_KERNEL_MAPPING;
		err = vfastrpc_buf_alloc(vfl, ud->size, dma_attr, ud->flags,
								1, pgprot_noncached(PAGE_KERNEL),
								&rbuf);
		if (err)
			goto bail;

		vmmap.fd = -1;
		vmmap.refcount = 1;
		vmmap.va = 0;
		vmmap.attr = 0;
		vmmap.len = rbuf->size;
		vmmap.nents = rbuf->sgt.nents;
		err = virt_fastrpc_mmap(vfl, ud->flags, rbuf->sgt.sgl,
					&raddr, &vmmap);
		if (err)
			goto bail;
		rbuf->raddr = raddr;
	} else {
		uintptr_t va_to_dsp;

		mutex_lock(&fl->map_mutex);
		VERIFY(err, !vfastrpc_mmap_create(vfl, ud->fd, 0,
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

		vmmap.fd = map->fd;
		vmmap.refcount = map->refs;
		vmmap.va = va_to_dsp;
		vmmap.len = map->size;
		vmmap.attr = VFASTRPC_MAP_ATTR_CACHED;
		vmmap.nents = map->table->nents;
		VERIFY(err, 0 == virt_fastrpc_mmap(vfl, ud->flags,
					map->table->sgl, &raddr, &vmmap));
		if (err)
			goto bail;
		map->raddr = raddr;
	}
	ud->vaddrout = raddr;
 bail:
	if (err && map) {
		mutex_lock(&fl->map_mutex);
		vfastrpc_mmap_free(vfl, map, 0);
		mutex_unlock(&fl->map_mutex);
	}
	mutex_unlock(&fl->internal_map_mutex);
	return err;
}

static int virt_fastrpc_mem_map(struct vfastrpc_file *vfl, s32 offset,
		u32 flags, s32 attrs, struct virt_fastrpc_mapping *vmmap,
		struct scatterlist *table, uintptr_t *raddr)
{
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct vfastrpc_apps *me = vfl->apps;
	struct virt_mem_map_msg *vmsg, *rsp = NULL;
	struct virt_fastrpc_msg *msg;
	struct virt_fastrpc_sgl *sgbuf;
	int err, sgbuf_size, total_size;
	struct scatterlist *sgl = NULL;
	int sgl_index = 0;

	sgbuf_size = vmmap->nents * sizeof(*sgbuf);
	total_size = sizeof(*vmsg) + sgbuf_size;

	msg = virt_alloc_msg(vfl, total_size);
	if (!msg)
		return -ENOMEM;

	vmsg = (struct virt_mem_map_msg *)msg->txbuf;
	vmsg->hdr.pid = fl->tgid;
	vmsg->hdr.tid = current->pid;
	vmsg->hdr.cid = fl->cid;
	vmsg->hdr.cmd = VIRTIO_FASTRPC_CMD_MEM_MAP;
	vmsg->hdr.len = total_size;
	vmsg->hdr.msgid = msg->msgid;
	vmsg->hdr.result = 0xffffffff;
	vmsg->offset = offset;
	vmsg->flags = flags;
	vmsg->attrs = attrs;
	vmsg->raddr = 0;
	memcpy(&vmsg->mmap, vmmap, sizeof(*vmmap));
	sgbuf = vmsg->mmap.sgl;

	for_each_sg(table, sgl, vmmap->nents, sgl_index) {
		sgbuf[sgl_index].pv = sg_dma_address(sgl);
		sgbuf[sgl_index].len = sg_dma_len(sgl);
	}

	err = vfastrpc_txbuf_send(vfl, vmsg, total_size);
	if (err)
		goto bail;

	wait_for_completion(&msg->work);

	rsp = msg->rxbuf;
	if (!rsp)
		goto bail;

	err = rsp->hdr.result;
	if (err)
		goto bail;
	*raddr = (uintptr_t)rsp->raddr;
bail:
	if (rsp)
		vfastrpc_rxbuf_send(vfl, rsp, me->buf_size);
	virt_free_msg(vfl, msg);
	return err;
}

int vfastrpc_internal_mem_map(struct vfastrpc_file *vfl,
				struct fastrpc_ioctl_mem_map *ud)
{
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	int err = 0;
	struct vfastrpc_mmap *map = NULL;
	struct vfastrpc_apps *me = vfl->apps;
	struct virt_fastrpc_mapping vmmap;

	VERIFY(err, fl->dsp_proc_init == 1);
	if (err) {
		dev_err(me->dev, " %s: user application %s trying to map without initialization\n",
			__func__, current->comm);
		err = EBADR;
		return err;
	}

	mutex_lock(&fl->internal_map_mutex);
	mutex_lock(&fl->map_mutex);
	VERIFY(err, !vfastrpc_mmap_create(vfl, ud->m.fd, ud->m.attrs,
			ud->m.vaddrin, ud->m.length,
			 ud->m.flags, &map));
	mutex_unlock(&fl->map_mutex);
	if (err)
		goto bail;

	if (map->raddr) {
		err = -EEXIST;
		goto bail;
	}

	vmmap.fd = map->fd;
	vmmap.refcount = map->refs;
	vmmap.va = map->va;
	vmmap.len = map->size;
	vmmap.attr = VFASTRPC_MAP_ATTR_CACHED;
	vmmap.nents = map->table->nents;
	err = virt_fastrpc_mem_map(vfl, ud->m.offset, ud->m.flags, ud->m.attrs,
			&vmmap, map->table->sgl, &map->raddr);
	if (err)
		goto bail;
	ud->m.vaddrout = map->raddr;
bail:
	if (err) {
		dev_err(me->dev, "%s failed to map fd %d flags %d err %d\n",
			__func__, ud->m.fd, ud->m.flags, err);
		if (map) {
			mutex_lock(&fl->map_mutex);
			vfastrpc_mmap_free(vfl, map, 0);
			mutex_unlock(&fl->map_mutex);
		}
	}
	mutex_unlock(&fl->internal_map_mutex);
	return err;
}

static int virt_fastrpc_mem_unmap(struct vfastrpc_file *vfl, int fd, u64 size,
		uintptr_t raddr)
{
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct vfastrpc_apps *me = vfl->apps;
	struct virt_mem_unmap_msg *vmsg, *rsp = NULL;
	struct virt_fastrpc_msg *msg;
	int err;

	msg = virt_alloc_msg(vfl, sizeof(*vmsg));
	if (!msg)
		return -ENOMEM;

	vmsg = (struct virt_mem_unmap_msg *)msg->txbuf;
	vmsg->hdr.pid = fl->tgid;
	vmsg->hdr.tid = current->pid;
	vmsg->hdr.cid = fl->cid;
	vmsg->hdr.cmd = VIRTIO_FASTRPC_CMD_MEM_UNMAP;
	vmsg->hdr.len = sizeof(*vmsg);
	vmsg->hdr.msgid = msg->msgid;
	vmsg->hdr.result = 0xffffffff;
	vmsg->fd = fd;
	vmsg->len = size;
	vmsg->raddr = raddr;

	err = vfastrpc_txbuf_send(vfl, vmsg, sizeof(*vmsg));
	if (err)
		goto bail;

	wait_for_completion(&msg->work);

	rsp = msg->rxbuf;
	if (!rsp)
		goto bail;

	err = rsp->hdr.result;
bail:
	if (rsp)
		vfastrpc_rxbuf_send(vfl, rsp, me->buf_size);
	virt_free_msg(vfl, msg);

	return err;
}

int vfastrpc_internal_mem_unmap(struct vfastrpc_file *vfl,
				struct fastrpc_ioctl_mem_unmap *ud)
{
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	int err = 0;
	struct vfastrpc_mmap *map = NULL;
	struct vfastrpc_apps *me = vfl->apps;

	VERIFY(err, fl->dsp_proc_init == 1);
	if (err) {
		dev_err(me->dev, "%s: user application %s trying to map without initialization\n",
			__func__, current->comm);
		err = EBADR;
		return err;
	}

	mutex_lock(&fl->internal_map_mutex);
	mutex_lock(&fl->map_mutex);
	VERIFY(err, !vfastrpc_mmap_remove(vfl, ud->um.fd,
				(uintptr_t)ud->um.vaddr, ud->um.length, &map));
	mutex_unlock(&fl->map_mutex);
	if (err)
		goto bail;

	VERIFY(err, map->flags == FASTRPC_MAP_FD ||
			map->flags == FASTRPC_MAP_FD_DELAYED ||
			map->flags == FASTRPC_MAP_STATIC);
	if (err) {
		err = -EBADMSG;
		goto bail;
	}

	err = virt_fastrpc_mem_unmap(vfl, map->fd, map->size, map->raddr);
	if (err)
		goto bail;

	mutex_lock(&fl->map_mutex);
	vfastrpc_mmap_free(vfl, map, 0);
	mutex_unlock(&fl->map_mutex);
	map = NULL;
bail:
	if (err) {
		dev_err(me->dev, "%s failed to unmap fd %d addr 0x%llx length 0x%x err 0x%x\n",
			__func__, ud->um.fd, ud->um.vaddr, ud->um.length, err);
		/* Add back to map list in case of error to unmap on DSP */
		if (map) {
			mutex_lock(&fl->map_mutex);
			if (map->attr & FASTRPC_ATTR_KEEP_MAP)
				map->refs++;
			vfastrpc_mmap_add(vfl, map);
			mutex_unlock(&fl->map_mutex);
		}
	}
	mutex_unlock(&fl->internal_map_mutex);
	return err;
}

static int virt_fastrpc_control(struct vfastrpc_file *vfl,
				struct fastrpc_ctrl_latency *lp)
{
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct vfastrpc_apps *me = vfl->apps;
	struct virt_control_msg *vmsg, *rsp = NULL;
	struct virt_fastrpc_msg *msg;
	int err;

	msg = virt_alloc_msg(vfl, sizeof(*vmsg));
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

	err = vfastrpc_txbuf_send(vfl, vmsg, sizeof(*vmsg));
	if (err)
		goto bail;

	wait_for_completion(&msg->work);

	rsp = msg->rxbuf;
	if (!rsp)
		goto bail;

	err = rsp->hdr.result;
bail:
	if (rsp)
		vfastrpc_rxbuf_send(vfl, rsp, me->buf_size);
	virt_free_msg(vfl, msg);

	return err;
}

int vfastrpc_internal_control(struct vfastrpc_file *vfl,
					struct fastrpc_ioctl_control *cp)
{
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct vfastrpc_apps *me = vfl->apps;
	int err = 0;

	VERIFY(err, !IS_ERR_OR_NULL(fl) && !IS_ERR_OR_NULL(vfl->apps));
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
		virt_fastrpc_control(vfl, &cp->lp);
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

static int vfastrpc_set_process_info(struct vfastrpc_file *vfl)
{
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	int err = 0, buf_size = 0;
	char strpid[PID_SIZE];
	char cur_comm[TASK_COMM_LEN];

	memcpy(cur_comm, current->comm, TASK_COMM_LEN);
	cur_comm[TASK_COMM_LEN - 1] = '\0';
	fl->tgid = current->tgid;

	/*
	 * Third-party apps don't have permission to open the fastrpc device, so
	 * it is opened on their behalf by DSP HAL. This is detected by
	 * comparing current PID with the one stored during device open.
	 */
	if (current->tgid != fl->tgid_open)
		fl->untrusted_process = true;
	scnprintf(strpid, PID_SIZE, "%d", current->pid);
	if (vfl->apps->debugfs_root) {
		buf_size = strlen(cur_comm) + strlen("_")
			+ strlen(strpid) + 1;

		spin_lock(&fl->hlock);
		if (fl->debug_buf_alloced_attempted) {
			spin_unlock(&fl->hlock);
			return err;
		}
		fl->debug_buf_alloced_attempted = 1;
		spin_unlock(&fl->hlock);
		fl->debug_buf = kzalloc(buf_size, GFP_KERNEL);

		if (!fl->debug_buf) {
			err = -ENOMEM;
			spin_lock(&fl->hlock);
			fl->debug_buf_alloced_attempted = 0;
			spin_unlock(&fl->hlock);
			return err;
		}
		scnprintf(fl->debug_buf, buf_size, "%.10s%s%d",
			cur_comm, "_", current->pid);
		fl->debugfs_file = debugfs_create_file(fl->debug_buf, 0644,
			vfl->apps->debugfs_root, fl, vfl->apps->debugfs_fops);
		if (IS_ERR_OR_NULL(fl->debugfs_file)) {
			dev_warn(vfl->apps->dev, "%s: %s: failed to create debugfs file %s\n",
				cur_comm, __func__, fl->debug_buf);
			fl->debugfs_file = NULL;
			kfree(fl->debug_buf);
			fl->debug_buf = NULL;
			spin_lock(&fl->hlock);
			fl->debug_buf_alloced_attempted = 0;
			spin_unlock(&fl->hlock);
		}
	}
	return err;
}

int vfastrpc_internal_get_info(struct vfastrpc_file *vfl,
					uint32_t *info)
{
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	int err = 0;
	uint32_t domain;
	struct vfastrpc_channel_ctx *chan;

	VERIFY(err, fl != NULL);
	if (err)
		goto bail;
	err = vfastrpc_set_process_info(vfl);
	if (err)
		goto bail;

	if (vfl->domain == -1) {
		domain = *info;
		VERIFY(err, domain < vfl->apps->num_channels);
		if (err)
			goto bail;
		chan = &vfl->apps->channel[domain];
		/* Check to see if the device node is non-secure */
		if (fl->dev_minor == MINOR_NUM_DEV) {
			/*
			 * If an app is trying to offload to a secure remote
			 * channel by opening the non-secure device node, allow
			 * the access if the subsystem supports unsigned
			 * offload. Untrusted apps will be restricted from
			 * offloading to signed PD using DSP HAL.
			 */
			if (chan->secure == true
			&& !chan->unsigned_support) {
				dev_err(vfl->apps->dev,
				"cannot use domain %d with non-secure device\n", domain);
				err = -EACCES;
				goto bail;
			}
		}
		vfl->domain = domain;
	}
	*info = 1;
bail:
	return err;
}

static int virt_fastrpc_open(struct vfastrpc_file *vfl,
		struct fastrpc_ioctl_init_attrs *uproc)
{
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct vfastrpc_apps *me = vfl->apps;
	struct virt_open_msg *vmsg, *rsp = NULL;
	struct virt_fastrpc_msg *msg;
	int err;

	msg = virt_alloc_msg(vfl, sizeof(*vmsg));
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
	vmsg->domain = vfl->domain;
	vmsg->pd = fl->pd;
	vmsg->attrs = uproc->attrs;

	err = vfastrpc_txbuf_send(vfl, vmsg, sizeof(*vmsg));
	if (err)
		goto bail;
	wait_for_completion(&msg->work);

	rsp = msg->rxbuf;
	if (!rsp)
		goto bail;

	err = rsp->hdr.result;
	if (err)
		goto bail;
	if (rsp->hdr.cid < 0) {
		dev_err(me->dev, "open: channel id %d is invalid\n", rsp->hdr.cid);
		err = -EINVAL;
		goto bail;
	}
	fl->cid = rsp->hdr.cid;
bail:
	if (rsp)
		vfastrpc_rxbuf_send(vfl, rsp, me->buf_size);
	virt_free_msg(vfl, msg);

	return err;
}

int vfastrpc_internal_init_process(struct vfastrpc_file *vfl,
				struct fastrpc_ioctl_init_attrs *uproc)
{
	int err = 0;
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct fastrpc_ioctl_init *init = &uproc->init;
	int domain = vfl->domain;
	struct vfastrpc_channel_ctx *chan = &vfl->apps->channel[domain];

	if (chan->unsigned_support && fl->dev_minor == MINOR_NUM_DEV) {
		/*
		 * Make sure third party applications
		 * can spawn only unsigned PD when
		 * channel configured as secure.
		 */
		if (chan->secure && !(uproc->attrs & FASTRPC_MODE_UNSIGNED_MODULE)) {
			err = -ECONNREFUSED;
			goto bail;
		}
	} else if (!(chan->unsigned_support) && (uproc->attrs & FASTRPC_MODE_UNSIGNED_MODULE)) {
		err = -ECONNREFUSED;
		goto bail;
	}

	switch (init->flags) {
	case FASTRPC_INIT_CREATE:
		fl->pd = DYNAMIC_PD;
		/* Untrusted apps are not allowed to offload to signedPD on DSP. */
		if (fl->untrusted_process) {
			VERIFY(err, uproc->attrs & FASTRPC_MODE_UNSIGNED_MODULE);
			if (err) {
				err = -ECONNREFUSED;
				dev_err(vfl->apps->dev,
					"untrusted app trying to offload to signed remote process\n");
				goto bail;
			}
		}

		vfl->procattrs = uproc->attrs;
		break;
	case FASTRPC_INIT_CREATE_STATIC:
	case FASTRPC_INIT_ATTACH:
	case FASTRPC_INIT_ATTACH_SENSORS:
		err = -ECONNREFUSED;
		goto bail;
	default:
		return -ENOTTY;
	}
	err = virt_fastrpc_open(vfl, uproc);
	if (err)
		goto bail;
	fl->dsp_proc_init = 1;
bail:
	return err;
}

static int virt_fastrpc_get_dsp_info(struct vfastrpc_file *vfl,
		u32 *dsp_attributes)
{
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	struct vfastrpc_apps *me = vfl->apps;
	struct virt_cap_msg *vmsg, *rsp = NULL;
	struct virt_fastrpc_msg *msg;
	int err;

	msg = virt_alloc_msg(vfl, sizeof(*vmsg));
	if (!msg) {
		dev_err(me->dev, "%s: no memory\n", __func__);
		return -ENOMEM;
	}

	vmsg = (struct virt_cap_msg *)msg->txbuf;
	vmsg->hdr.pid = fl->tgid;
	vmsg->hdr.tid = current->pid;
	vmsg->hdr.cid = -1;
	vmsg->hdr.cmd = VIRTIO_FASTRPC_CMD_GET_DSP_INFO;
	vmsg->hdr.len = sizeof(*vmsg);
	vmsg->hdr.msgid = msg->msgid;
	vmsg->hdr.result = 0xffffffff;
	vmsg->domain = vfl->domain;
	memset(vmsg->dsp_caps, 0, FASTRPC_MAX_DSP_ATTRIBUTES * (sizeof(u32)));

	err = vfastrpc_txbuf_send(vfl, vmsg, sizeof(*vmsg));
	if (err)
		goto bail;
	wait_for_completion(&msg->work);

	rsp = msg->rxbuf;
	if (!rsp)
		goto bail;

	err = rsp->hdr.result;
	if (err)
		goto bail;
	memcpy(dsp_attributes, rsp->dsp_caps, FASTRPC_MAX_DSP_ATTRIBUTES * (sizeof(u32)));
bail:
	if (rsp)
		vfastrpc_rxbuf_send(vfl, rsp, me->buf_size);
	virt_free_msg(vfl, msg);

	return err;
}

int vfastrpc_get_info_from_kernel(
		struct fastrpc_ioctl_capability *cap,
		struct vfastrpc_file *vfl)
{
	int err = 0;
	uint32_t domain = cap->domain, attribute_ID = cap->attribute_ID;
	struct fastrpc_dsp_capabilities *dsp_cap_ptr = NULL;

	/*
	 * Check if number of attribute IDs obtained from userspace
	 * is less than the number of attribute IDs supported by
	 * kernel
	 */
	if (attribute_ID >= FASTRPC_MAX_ATTRIBUTES) {
		err = -EOVERFLOW;
		goto bail;
	}

	dsp_cap_ptr = &vfl->apps->channel[domain].dsp_cap_kernel;

	if (attribute_ID >= FASTRPC_MAX_DSP_ATTRIBUTES) {
		/* Driver capability, pass it to user */
		memcpy(&cap->capability,
			&kernel_capabilities[attribute_ID -
			FASTRPC_MAX_DSP_ATTRIBUTES],
			sizeof(cap->capability));
	} else if (!dsp_cap_ptr->is_cached) {
		/*
		 * Information not on kernel, query device for information
		 * and cache on kernel
		 */
		err = virt_fastrpc_get_dsp_info(vfl,
			  dsp_cap_ptr->dsp_attributes);
		if (err)
			goto bail;

		vfl->apps->channel[domain].unsigned_support =
			!!(dsp_cap_ptr->dsp_attributes[UNSIGNED_PD_SUPPORT]);

		/* WA for async invoke support, need to be removed later */
		dsp_cap_ptr->dsp_attributes[ASYNC_FASTRPC_CAP] = 1;

		memcpy(&cap->capability,
			&dsp_cap_ptr->dsp_attributes[attribute_ID],
			sizeof(cap->capability));

		dsp_cap_ptr->is_cached = 1;
	} else {
		/* Information on Kernel, pass it to user */
		memcpy(&cap->capability,
			&dsp_cap_ptr->dsp_attributes[attribute_ID],
			sizeof(cap->capability));
	}
bail:
	return err;
}

int vfastrpc_internal_get_dsp_info(struct fastrpc_ioctl_capability *cap,
		void *param, struct vfastrpc_file *vfl)
{
	int err = 0;

	K_COPY_FROM_USER(err, 0, cap, param, sizeof(struct fastrpc_ioctl_capability));
	if (err)
		goto bail;

	VERIFY(err, cap->domain < vfl->apps->num_channels);
	if (err) {
		err = -ECHRNG;
		goto bail;
	}
	cap->capability = 0;

	err = vfastrpc_get_info_from_kernel(cap, vfl);
	if (err)
		goto bail;
	K_COPY_TO_USER(err, 0, &((struct fastrpc_ioctl_capability *)
				param)->capability, &cap->capability, sizeof(cap->capability));
bail:
	return err;
}
