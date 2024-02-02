/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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
#include <linux/virtio_ids.h>
#include <linux/uaccess.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/subsystem_restart.h>
#include "adsprpc_compat.h"
#include "adsprpc_shared.h"

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

#define NUM_CHANNELS			4 /* adsp, mdsp, slpi, cdsp*/
#define NUM_DEVICES			2 /* adsprpc-smd, adsprpc-smd-secure */
#define MINOR_NUM_DEV			0
#define MINOR_NUM_SECURE_DEV		1
#define ADSP_MMAP_HEAP_ADDR		4
#define ADSP_MMAP_REMOTE_HEAP_ADDR	8
#define ADSP_MMAP_ADD_PAGES		0x1000

#define INIT_FILELEN_MAX		(2*1024*1024)
#define INIT_MEMLEN_MAX			(8*1024*1024)

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

#define PERF_KEYS \
	"count:flush:map:copy:rpmsg:getargs:putargs:invalidate:invoke:tid:ptr"
#define FASTRPC_STATIC_HANDLE_KERNEL	1
#define FASTRPC_STATIC_HANDLE_LISTENER	3
#define FASTRPC_STATIC_HANDLE_MAX	20

#define PERF_END (void)0

#define PERF(enb, cnt, ff) \
	{\
		struct timespec startT = {0};\
		int64_t *counter = cnt;\
		if (enb && counter) {\
			getnstimeofday(&startT);\
		} \
		ff ;\
		if (enb && counter) {\
			*counter += getnstimediff(&startT);\
		} \
	}

#define GET_COUNTER(perf_ptr, offset)  \
	(perf_ptr != NULL ?\
		(((offset >= 0) && (offset < PERF_KEY_MAX)) ?\
			(int64_t *)(perf_ptr + offset)\
				: (int64_t *)NULL) : (int64_t *)NULL)

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

struct fastrpc_apps {
	struct virtio_device *vdev;
	struct virtqueue *rvq;
	struct virtqueue *svq;
	void *rbufs;
	void *sbufs;
	unsigned int order;
	unsigned int num_bufs;
	unsigned int buf_size;
	int last_sbuf;

	struct mutex lock;
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

enum fastrpc_perfkeys {
	PERF_COUNT = 0,
	PERF_FLUSH = 1,
	PERF_MAP = 2,
	PERF_COPY = 3,
	PERF_LINK = 4,
	PERF_GETARGS = 5,
	PERF_PUTARGS = 6,
	PERF_INVARGS = 7,
	PERF_INVOKE = 8,
	PERF_KEY_MAX = 9,
};

struct fastrpc_perf {
	int64_t count;
	int64_t flush;
	int64_t map;
	int64_t copy;
	int64_t link;
	int64_t getargs;
	int64_t putargs;
	int64_t invargs;
	int64_t invoke;
	int64_t tid;
	struct hlist_node hn;
};

struct fastrpc_file {
	spinlock_t hlock;
	struct hlist_head maps;
	struct hlist_head perf;
	struct hlist_head remote_bufs;
	uint32_t mode;
	uint32_t profile;
	int tgid;
	int cid;
	int domain;
	int pd;
	int file_close;
	int dsp_proc_init;
	struct fastrpc_apps *apps;
	struct dentry *debugfs_file;
	struct mutex perf_mutex;
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
};

struct fastrpc_buf {
	struct hlist_node hn_rem;
	struct fastrpc_file *fl;
	size_t size;
	struct sg_table sgt;
	struct page **pages;
	uintptr_t raddr;
	uint32_t flags;
	int remote;
};

static struct fastrpc_apps gfa;
static struct dentry *debugfs_root;
static int virt_fastrpc_close(struct fastrpc_file *fl);

static inline int64_t getnstimediff(struct timespec *start)
{
	int64_t ns;
	struct timespec ts, b;

	getnstimeofday(&ts);
	b = timespec_sub(ts, *start);
	ns = timespec_to_ns(&b);
	return ns;
}

static inline int64_t *getperfcounter(struct fastrpc_file *fl, int key)
{
	int err = 0;
	int64_t *val = NULL;
	struct fastrpc_perf *perf = NULL, *fperf = NULL;
	struct hlist_node *n = NULL;

	VERIFY(err, !IS_ERR_OR_NULL(fl));
	if (err)
		goto bail;

	mutex_lock(&fl->perf_mutex);
	hlist_for_each_entry_safe(perf, n, &fl->perf, hn) {
		if (perf->tid == current->pid) {
			fperf = perf;
			break;
		}
	}

	if (IS_ERR_OR_NULL(fperf)) {
		fperf = kzalloc(sizeof(*fperf), GFP_KERNEL);

		VERIFY(err, !IS_ERR_OR_NULL(fperf));
		if (err) {
			mutex_unlock(&fl->perf_mutex);
			kfree(fperf);
			goto bail;
		}

		fperf->tid = current->pid;
		hlist_add_head(&fperf->hn, &fl->perf);
	}

	val = ((int64_t *)fperf) + key;
	mutex_unlock(&fl->perf_mutex);
bail:
	return val;
}

static void *get_a_tx_buf(void)
{
	struct fastrpc_apps *me = &gfa;
	unsigned int len;
	void *ret;

	/* support multiple concurrent senders */
	mutex_lock(&me->lock);
	/*
	 * either pick the next unused tx buffer
	 * (half of our buffers are used for sending messages)
	 */
	if (me->last_sbuf < me->num_bufs / 2)
		ret = me->sbufs + me->buf_size * me->last_sbuf++;
	/* or recycle a used one */
	else
		ret = virtqueue_get_buf(me->svq, &len);
	mutex_unlock(&me->lock);
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

static void fastrpc_mmap_free(struct fastrpc_mmap *map)
{
	struct fastrpc_apps *me = &gfa;

	if (!map)
		return;
	if (map->flags == ADSP_MMAP_HEAP_ADDR ||
				map->flags == ADSP_MMAP_REMOTE_HEAP_ADDR)
		dev_err(me->dev, "%s ADSP_MMAP_HEAP_ADDR is not supported\n",
				__func__);
	hlist_del_init(&map->hn);

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
		uintptr_t va, size_t len, int mflags,
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
			if (va == map->va &&
				len == map->len &&
				map->fd == fd) {
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

	map = kzalloc(sizeof(*map), GFP_KERNEL);
	VERIFY(err, !IS_ERR_OR_NULL(map));
	if (err)
		goto bail;

	INIT_HLIST_NODE(&map->hn);
	map->flags = mflags;
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
		fastrpc_mmap_free(map);
	return err;
}

static int virt_fastrpc_invoke(struct fastrpc_file *fl, uint32_t kernel,
				struct fastrpc_ioctl_invoke_crc *inv)
{
	struct fastrpc_apps *me = fl->apps;
	struct virt_invoke_msg *vmsg, *rsp = NULL;
	struct virt_fastrpc_msg *msg = NULL;
	struct fastrpc_ioctl_invoke *invoke = &inv->inv;
	struct scatterlist sg[1];
	int inbufs = REMOTE_SCALARS_INBUFS(invoke->sc);
	int outbufs = REMOTE_SCALARS_OUTBUFS(invoke->sc);
	int i, err, bufs;
	remote_arg_t *lpra = NULL;
	struct virt_fastrpc_buf *rpra;
	int *fds, outbufs_offset = 0;
	struct fastrpc_mmap **maps;
	size_t copylen = 0, size = 0;
	char *payload;
	struct timespec invoket = {0};
	int64_t *perf_counter = getperfcounter(fl, PERF_COUNT);

	if (fl->profile)
		getnstimeofday(&invoket);

	bufs = REMOTE_SCALARS_LENGTH(invoke->sc);
	size = bufs * sizeof(*lpra) + bufs * sizeof(*fds)
		+ bufs * sizeof(*maps);
	lpra = kzalloc(size, GFP_KERNEL);
	if (!lpra)
		return -ENOMEM;
	fds = (int *)&lpra[bufs];
	maps = (struct fastrpc_mmap **)&fds[bufs];
	K_COPY_FROM_USER(err, kernel, (void *)lpra, invoke->pra,
			bufs * sizeof(*lpra));
	if (err)
		goto bail;
	if (inv->fds) {
		K_COPY_FROM_USER(err, kernel, fds, inv->fds,
				bufs * sizeof(*fds));
		if (err)
			goto bail;
	} else {
		fds = NULL;
	}

	PERF(fl->profile, GET_COUNTER(perf_counter, PERF_MAP),
	/* calculate len required for copying */
	for (i = 0; i < inbufs + outbufs; i++) {
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
			outbufs_offset += len;
	}
	PERF_END);
	size = bufs * sizeof(*rpra) + copylen + sizeof(*vmsg);
	msg = virt_alloc_msg(size);
	if (!msg)
		goto bail;

	vmsg = (struct virt_invoke_msg *)msg->txbuf;
	if (kernel)
		vmsg->hdr.pid = 0;
	else
		vmsg->hdr.pid = fl->tgid;
	vmsg->hdr.tid = current->pid;
	vmsg->hdr.cid = fl->cid;
	vmsg->hdr.cmd = VIRTIO_FASTRPC_CMD_INVOKE;
	vmsg->hdr.len = size;
	vmsg->hdr.msgid = msg->msgid;
	vmsg->hdr.result = 0xffffffff;
	vmsg->handle = invoke->handle;
	vmsg->sc = invoke->sc;
	rpra = (struct virt_fastrpc_buf *)vmsg->pra;
	payload = (char *)&rpra[bufs];

	PERF(fl->profile, GET_COUNTER(perf_counter, PERF_COPY),
	for (i = 0; i < inbufs + outbufs; i++) {
		size_t len = lpra[i].buf.len;
		struct sg_table *table;
		struct virt_fastrpc_buf *sgbuf;
		struct scatterlist *sgl = NULL;
		int sgl_index = 0;

		if (fds && (fds[i] != -1)) {
			table = maps[i]->table;
			rpra[i].pv = len;
			rpra[i].len = table->nents *
				sizeof(struct virt_fastrpc_buf);
			sgbuf = (struct virt_fastrpc_buf *)payload;
			for_each_sg(table->sgl, sgl, table->nents, sgl_index) {
				sgbuf[sgl_index].pv = sg_dma_address(sgl);
				sgbuf[sgl_index].len = sg_dma_len(sgl);
			}
			payload += rpra[i].len;
		} else {
			/* copy non ion buffers */
			rpra[i].pv = 0;
			rpra[i].len = len;
			if (i < inbufs && len) {
				K_COPY_FROM_USER(err, kernel, payload,
						lpra[i].buf.pv, len);
				if (err)
					goto bail;
			}
			payload += len;
		}
	}
	PERF_END);

	if (fl->profile) {
		int64_t *count = GET_COUNTER(perf_counter, PERF_GETARGS);

		if (count)
			*count += getnstimediff(&invoket);
	}

	PERF(fl->profile, GET_COUNTER(perf_counter, PERF_LINK),
	sg_init_one(sg, vmsg, size);

	mutex_lock(&me->lock);
	err = virtqueue_add_outbuf(me->svq, sg, 1, vmsg, GFP_KERNEL);
	if (err) {
		dev_err(me->dev, "%s: fail to add output buffer\n", __func__);
		goto bail;
	}

	virtqueue_kick(me->svq);
	mutex_unlock(&me->lock);
	PERF_END);

	wait_for_completion(&msg->work);

	rsp = msg->rxbuf;
	err = rsp->hdr.result;
	if (err)
		goto bail;

	rpra = (struct virt_fastrpc_buf *)rsp->pra;
	payload = (char *)&rpra[bufs] + outbufs_offset;

	PERF(fl->profile, GET_COUNTER(perf_counter, PERF_PUTARGS),
	for (i = inbufs; i < inbufs + outbufs; i++) {
		if (!maps[i]) {
			K_COPY_TO_USER(err, kernel, lpra[i].buf.pv,
					payload, rpra[i].len);
			if (err)
				goto bail;
		} else {
			mutex_lock(&fl->map_mutex);
			fastrpc_mmap_free(maps[i]);
			mutex_unlock(&fl->map_mutex);
			maps[i] = NULL;
		}
		payload += rpra[i].len;
	}
	PERF_END);
bail:
	if (rsp) {
		sg_init_one(sg, rsp, me->buf_size);

		/* add the buffer back to the remote processor's virtqueue */
		if (virtqueue_add_inbuf(me->rvq, sg, 1, rsp, GFP_KERNEL))
			dev_err(me->dev,
				"%s: fail to add input buffer\n", __func__);
		else
			virtqueue_kick(me->rvq);
	}

	mutex_lock(&fl->map_mutex);
	for (i = 0; i < inbufs + outbufs; i++)
		fastrpc_mmap_free(maps[i]);
	mutex_unlock(&fl->map_mutex);
	if (msg)
		virt_free_msg(msg);
	kfree(lpra);

	return err;
}

static int fastrpc_internal_invoke(struct fastrpc_file *fl,
					uint32_t mode, uint32_t kernel,
					struct fastrpc_ioctl_invoke_crc *inv)
{
	struct fastrpc_ioctl_invoke *invoke = &inv->inv;
	struct fastrpc_apps *me = fl->apps;
	int domain = fl->domain;
	int handles, err = 0;
	struct timespec invoket = {0};
	int64_t *perf_counter = getperfcounter(fl, PERF_COUNT);

	if (fl->profile)
		getnstimeofday(&invoket);

	if (!kernel) {
		VERIFY(err, invoke->handle != FASTRPC_STATIC_HANDLE_KERNEL);
		if (err) {
			dev_err(me->dev, "user application %s trying to send a kernel RPC message to channel %d\n",
				current->comm, domain);
			goto bail;
		}
	}

	VERIFY(err, fl->domain >= 0 && fl->domain < NUM_CHANNELS);
	if (err) {
		dev_err(me->dev, "user application %s domain is not set\n",
				current->comm);
		err = -EBADR;
		goto bail;
	}

	handles = REMOTE_SCALARS_INHANDLES(invoke->sc) +
			REMOTE_SCALARS_OUTHANDLES(invoke->sc);
	if (handles) {
		dev_err(me->dev, "dma handle is not supported\n");
		err = -ENOTTY;
		goto bail;
	}

	err = virt_fastrpc_invoke(fl, kernel, inv);
	if (fl->profile) {
		if (invoke->handle != FASTRPC_STATIC_HANDLE_LISTENER) {
			int64_t *count = GET_COUNTER(perf_counter, PERF_INVOKE);

			if (count)
				*count += getnstimediff(&invoket);
		}
		if (invoke->handle > FASTRPC_STATIC_HANDLE_MAX) {
			int64_t *count = GET_COUNTER(perf_counter, PERF_COUNT);

			if (count)
				*count = *count + 1;
		}
	}
bail:
	return err;
}

static int fastrpc_debugfs_open(struct inode *inode, struct file *filp)
{
	filp->private_data = inode->i_private;
	return 0;
}

static ssize_t fastrpc_debugfs_read(struct file *filp, char __user *buffer,
					 size_t count, loff_t *position)
{
	struct fastrpc_file *fl = filp->private_data;
	char *fileinfo = NULL;
	unsigned int len = 0;
	int err = 0;

	fileinfo = kzalloc(DEBUGFS_SIZE, GFP_KERNEL);
	if (!fileinfo)
		goto bail;
	if (fl) {
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
				"\n%s %d\n", " CHANNEL =", fl->domain);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
				"%s %9s %d\n", "profile", ":", fl->profile);
	}

	if (len > DEBUGFS_SIZE)
		len = DEBUGFS_SIZE;
	err = simple_read_from_buffer(buffer, count, position, fileinfo, len);
	kfree(fileinfo);
bail:
	return err;
}

static const struct file_operations debugfs_fops = {
	.open = fastrpc_debugfs_open,
	.read = fastrpc_debugfs_read,
};

static inline void fastprc_free_pages(struct page **pages, int count)
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
			fastprc_free_pages(pages, i);
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
	return pages;

out_free_pages:
	fastprc_free_pages(pages, count);
	return NULL;
}

static inline void fastrpc_free_buffer(struct fastrpc_buf *buf)
{
	unsigned int count = PAGE_ALIGN(buf->size) >> PAGE_SHIFT;

	sg_free_table(&buf->sgt);
	fastprc_free_pages(buf->pages, count);
}

static void fastrpc_buf_free(struct fastrpc_buf *buf)
{
	struct fastrpc_file *fl = buf == NULL ? NULL : buf->fl;

	if (!fl)
		return;

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
				uint32_t rflags, int remote,
				struct fastrpc_buf **obuf)
{
	struct fastrpc_apps *me = &gfa;
	struct fastrpc_buf *buf = NULL;
	int err = 0;

	VERIFY(err, size > 0);
	if (err)
		goto bail;

	VERIFY(err, NULL != (buf = kzalloc(sizeof(*buf), GFP_KERNEL)));
	if (err)
		goto bail;
	buf->fl = fl;
	buf->size = size;
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
		fastrpc_buf_free(buf);
	return err;
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
			fastrpc_buf_free(free);
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

	spin_lock_init(&fl->hlock);
	INIT_HLIST_HEAD(&fl->maps);
	INIT_HLIST_HEAD(&fl->perf);
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
	mutex_init(&fl->perf_mutex);
	return 0;
}

static int fastrpc_file_free(struct fastrpc_file *fl)
{
	struct fastrpc_mmap *map = NULL, *lmap = NULL;
	struct fastrpc_perf *perf = NULL, *fperf = NULL;

	if (!fl)
		return 0;

	virt_fastrpc_close(fl);

	kfree(fl->debug_buf);

	spin_lock(&fl->hlock);
	fl->file_close = 1;
	spin_unlock(&fl->hlock);

	mutex_lock(&fl->map_mutex);
	do {
		struct hlist_node *n = NULL;

		lmap = NULL;
		hlist_for_each_entry_safe(map, n, &fl->maps, hn) {
			hlist_del_init(&map->hn);
			lmap = map;
			break;
		}
		fastrpc_mmap_free(lmap);
	} while (lmap);
	mutex_unlock(&fl->map_mutex);

	mutex_lock(&fl->perf_mutex);
	do {
		struct hlist_node *pn = NULL;

		fperf = NULL;
		hlist_for_each_entry_safe(perf, pn, &fl->perf, hn) {
			hlist_del_init(&perf->hn);
			fperf = perf;
			break;
		}
		kfree(fperf);
	} while (fperf);
	mutex_unlock(&fl->perf_mutex);
	mutex_destroy(&fl->perf_mutex);

	fastrpc_remote_buf_list_free(fl);
	mutex_destroy(&fl->map_mutex);
	kfree(fl);
	return 0;
}

static int fastrpc_release(struct inode *inode, struct file *file)
{
	struct fastrpc_file *fl = (struct fastrpc_file *)file->private_data;

	if (fl) {
		if (fl->debugfs_file != NULL)
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

	mutex_lock(&me->lock);
	err = virtqueue_add_outbuf(me->svq, sg, 1, vmsg, GFP_KERNEL);
	if (err) {
		dev_err(me->dev, "%s: fail to add output buffer\n", __func__);
		goto bail;
	}

	virtqueue_kick(me->svq);
	mutex_unlock(&me->lock);

	wait_for_completion(&msg->work);

	rsp = msg->rxbuf;
	err = rsp->hdr.result;
bail:
	if (rsp) {
		sg_init_one(sg, rsp, me->buf_size);

		/* add the buffer back to the remote processor's virtqueue */
		if (virtqueue_add_inbuf(me->rvq, sg, 1, rsp, GFP_KERNEL))
			dev_err(me->dev,
				"%s: fail to add input buffer\n", __func__);
		else
			virtqueue_kick(me->rvq);
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
		fastrpc_buf_free(rbuf);
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
	fastrpc_mmap_free(map);
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

	VERIFY(err, (fl && ud));
	if (err)
		goto bail;
	VERIFY(err, fl->dsp_proc_init == 1);
	if (err) {
		dev_err(me->dev, "%s: user application %s trying to unmap without initialization\n",
			__func__, current->comm);
		err = -EBADR;
		goto bail;
	}
	mutex_lock(&fl->map_mutex);
	if (fastrpc_mmap_find(fl, ud->fd, ud->va, ud->len, 0, &map)) {
		dev_err(me->dev, "mapping not found to unmap fd 0x%x, va 0x%lx, len 0x%x\n",
			ud->fd, ud->va, (unsigned int)ud->len);
		err = -1;
		mutex_unlock(&fl->map_mutex);
		goto bail;
	}
	if (map)
		fastrpc_mmap_free(map);
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

	mutex_lock(&me->lock);
	err = virtqueue_add_outbuf(me->svq, sg, 1, vmsg, GFP_KERNEL);
	if (err) {
		dev_err(me->dev, "%s: fail to add output buffer\n", __func__);
		goto bail;
	}

	virtqueue_kick(me->svq);
	mutex_unlock(&me->lock);

	wait_for_completion(&msg->work);

	rsp = msg->rxbuf;
	err = rsp->hdr.result;
	if (err)
		goto bail;
	*raddr = (uintptr_t)rsp->vdsp;
bail:
	if (rsp) {
		sg_init_one(sg, rsp, me->buf_size);

		/* add the buffer back to the remote processor's virtqueue */
		if (virtqueue_add_inbuf(me->rvq, sg, 1, rsp, GFP_KERNEL))
			dev_err(me->dev,
				"%s: fail to add input buffer\n", __func__);
		else
			virtqueue_kick(me->rvq);
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
		err = fastrpc_buf_alloc(fl, ud->size, ud->flags, 1, &rbuf);
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
		fastrpc_mmap_free(map);
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
	vmsg->latency = lp->level;
	sg_init_one(sg, vmsg, sizeof(*vmsg));

	mutex_lock(&me->lock);
	err = virtqueue_add_outbuf(me->svq, sg, 1, vmsg, GFP_KERNEL);
	if (err) {
		dev_err(me->dev, "%s: fail to add output buffer\n", __func__);
		goto bail;
	}

	virtqueue_kick(me->svq);
	mutex_unlock(&me->lock);

	wait_for_completion(&msg->work);

	rsp = msg->rxbuf;
	err = rsp->hdr.result;
bail:
	if (rsp) {
		sg_init_one(sg, rsp, me->buf_size);

		/* add the buffer back to the remote processor's virtqueue */
		if (virtqueue_add_inbuf(me->rvq, sg, 1, rsp, GFP_KERNEL))
			dev_err(me->dev,
				"%s: fail to add input buffer\n", __func__);
		else
			virtqueue_kick(me->rvq);
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
		if (me->has_control == false) {
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
		VERIFY(err, domain < NUM_CHANNELS);
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

	mutex_lock(&me->lock);
	err = virtqueue_add_outbuf(me->svq, sg, 1, vmsg, GFP_KERNEL);
	if (err) {
		dev_err(me->dev, "%s: fail to add output buffer\n", __func__);
		goto bail;
	}

	virtqueue_kick(me->svq);
	mutex_unlock(&me->lock);

	wait_for_completion(&msg->work);

	rsp = msg->rxbuf;
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

		/* add the buffer back to the remote processor's virtqueue */
		if (virtqueue_add_inbuf(me->rvq, sg, 1, rsp, GFP_KERNEL))
			dev_err(me->dev,
				"%s: fail to add input buffer\n", __func__);
		else
			virtqueue_kick(me->rvq);
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

	mutex_lock(&me->lock);
	err = virtqueue_add_outbuf(me->svq, sg, 1, vmsg, GFP_KERNEL);
	if (err) {
		dev_err(me->dev, "%s: fail to add output buffer\n", __func__);
		goto bail;
	}

	virtqueue_kick(me->svq);
	mutex_unlock(&me->lock);

	wait_for_completion(&msg->work);

	rsp = msg->rxbuf;
	err = rsp->result;
bail:
	if (rsp) {
		sg_init_one(sg, rsp, me->buf_size);

		/* add the buffer back to the remote processor's virtqueue */
		if (virtqueue_add_inbuf(me->rvq, sg, 1, rsp, GFP_KERNEL))
			dev_err(me->dev,
				"%s: fail to add input buffer\n", __func__);
		else
			virtqueue_kick(me->rvq);
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
		struct fastrpc_ioctl_perf perf;
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
		/* fall through */
	case FASTRPC_IOCTL_INVOKE_FD:
		if (!size)
			size = sizeof(struct fastrpc_ioctl_invoke_fd);
		/* fall through */
	case FASTRPC_IOCTL_INVOKE_ATTRS:
		if (!size)
			size = sizeof(struct fastrpc_ioctl_invoke_attrs);
		/* fall through */
	case FASTRPC_IOCTL_INVOKE_CRC:
		if (!size)
			size = sizeof(struct fastrpc_ioctl_invoke_crc);

		K_COPY_FROM_USER(err, 0, &p.inv, param, size);
		if (err)
			goto bail;

		if (p.inv.attrs && me->has_invoke_attr == false) {
			dev_err(me->dev, "invoke attr is not supported\n");
			err = -ENOTTY;
			goto bail;
		}
		if (p.inv.crc && me->has_invoke_crc == false) {
			dev_err(me->dev, "invoke crc is not supported\n");
			err = -ENOTTY;
			goto bail;
		}

		VERIFY(err, 0 == (err = fastrpc_internal_invoke(fl, fl->mode,
						0, &p.inv)));
		if (err)
			goto bail;
		break;
	case FASTRPC_IOCTL_MMAP:
		if (me->has_mmap == false) {
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
		if (me->has_mmap == false) {
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
		if (me->has_mmap == false) {
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
		if (me->has_mmap == false) {
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
		case FASTRPC_MODE_PROFILE:
			fl->profile = (uint32_t)ioctl_param;
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
	case FASTRPC_IOCTL_GETPERF:
		K_COPY_FROM_USER(err, 0, &p.perf,
					param, sizeof(p.perf));
		if (err)
			goto bail;
		p.perf.numkeys = sizeof(struct fastrpc_perf)/sizeof(int64_t);
		if (p.perf.keys) {
			char *keys = PERF_KEYS;

			K_COPY_TO_USER(err, 0, (void *)p.perf.keys,
						keys, strlen(keys)+1);
			if (err)
				goto bail;
		}
		if (p.perf.data) {
			struct fastrpc_perf *perf = NULL, *fperf = NULL;
			struct hlist_node *n = NULL;

			mutex_lock(&fl->perf_mutex);
			hlist_for_each_entry_safe(perf, n, &fl->perf, hn) {
				if (perf->tid == current->pid) {
					fperf = perf;
					break;
				}
			}

			mutex_unlock(&fl->perf_mutex);

			if (fperf) {
				K_COPY_TO_USER(err, 0,
					(void *)p.perf.data, fperf,
					sizeof(*fperf) -
					sizeof(struct hlist_node));
			}
		}
		K_COPY_TO_USER(err, 0, param, &p.perf, sizeof(p.perf));
		if (err)
			goto bail;
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
		/* fall through */
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
	mutex_init(&me->lock);
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

	rsp = virtqueue_get_buf(rvq, &len);
	if (!rsp) {
		dev_err(me->dev, "incoming signal, but no used buffer\n");
		return;
	}

	while (rsp) {
		err = recv_single(rsp, len);
		if (err)
			break;

		msgs_received++;

		rsp = virtqueue_get_buf(rvq, &len);
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

	me->svq = vqs[0];
	me->rvq = vqs[1];

	/* we expect symmetric tx/rx vrings */
	WARN_ON(virtqueue_get_vring_size(me->rvq) !=
			virtqueue_get_vring_size(me->svq));
	me->num_bufs = virtqueue_get_vring_size(me->rvq) * 2;

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

	debugfs_root = debugfs_create_dir("adsprpc", NULL);
	err = alloc_chrdev_region(&me->dev_no, 0, NUM_CHANNELS, DEVICE_NAME);
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

	virtio_device_ready(vdev);

	/* set up the receive buffers */
	for (i = 0; i < me->num_bufs / 2; i++) {
		struct scatterlist sg;
		void *cpu_addr = me->rbufs + i * me->buf_size;

		sg_init_one(&sg, cpu_addr, me->buf_size);
		err = virtqueue_add_inbuf(me->rvq, &sg, 1, cpu_addr,
				GFP_KERNEL);
		WARN_ON(err); /* sanity check; this can't really happen */
	}

	/* suppress "tx-complete" interrupts */
	virtqueue_disable_cb(me->svq);

	virtqueue_enable_cb(me->rvq);
	virtqueue_kick(me->rvq);

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
	unregister_chrdev_region(me->dev_no, NUM_CHANNELS);
alloc_chrdev_bail:
	vdev->config->del_vqs(vdev);
	return err;
}

static void virt_fastrpc_remove(struct virtio_device *vdev)
{
	struct fastrpc_apps *me = &gfa;

	device_destroy(me->class, MKDEV(MAJOR(me->dev_no), MINOR_NUM_DEV));
	device_destroy(me->class, MKDEV(MAJOR(me->dev_no),
					MINOR_NUM_SECURE_DEV));
	class_destroy(me->class);
	cdev_del(&me->cdev);
	unregister_chrdev_region(me->dev_no, NUM_CHANNELS);
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
	unregister_virtio_driver(&virtio_fastrpc_driver);
}
module_init(virtio_fastrpc_init);
module_exit(virtio_fastrpc_exit);

MODULE_DEVICE_TABLE(virtio, id_table);
MODULE_DESCRIPTION("Virtio fastrpc driver");
MODULE_LICENSE("GPL v2");
