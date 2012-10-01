/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include <linux/scatterlist.h>
#include "adsprpc.h"

struct smq_invoke_ctx {
	struct completion work;
	int retval;
	atomic_t free;
};

struct smq_context_list {
	struct smq_invoke_ctx *ls;
	int size;
	int last;
};

struct fastrpc_apps {
	smd_channel_t *chan;
	struct smq_context_list clst;
	struct completion work;
	struct ion_client *iclient;
	struct cdev cdev;
	dev_t dev_no;
	spinlock_t wrlock;
	spinlock_t hlock;
	struct hlist_head htbl[RPC_HASH_SZ];
};

struct fastrpc_buf {
	struct ion_handle *handle;
	void *virt;
	ion_phys_addr_t phys;
	int size;
	int used;
};

struct fastrpc_device {
	uint32_t tgid;
	struct hlist_node hn;
	struct fastrpc_buf buf;
};

static struct fastrpc_apps gfa;

static void free_mem(struct fastrpc_buf *buf)
{
	struct fastrpc_apps *me = &gfa;

	if (buf->handle) {
		if (buf->virt) {
			ion_unmap_kernel(me->iclient, buf->handle);
			buf->virt = 0;
		}
		ion_free(me->iclient, buf->handle);
		buf->handle = 0;
	}
}

static int alloc_mem(struct fastrpc_buf *buf)
{
	struct ion_client *clnt = gfa.iclient;
	struct sg_table *sg;
	int err = 0;

	buf->handle = ion_alloc(clnt, buf->size, SZ_4K,
				ION_HEAP(ION_AUDIO_HEAP_ID), 0);
	VERIFY(err, 0 == IS_ERR_OR_NULL(buf->handle));
	if (err)
		goto bail;
	buf->virt = 0;
	VERIFY(err, 0 != (buf->virt = ion_map_kernel(clnt, buf->handle)));
	if (err)
		goto bail;
	VERIFY(err, 0 != (sg = ion_sg_table(clnt, buf->handle)));
	if (err)
		goto bail;
	VERIFY(err, 1 == sg->nents);
	if (err)
		goto bail;
	buf->phys = sg_dma_address(sg->sgl);
 bail:
	if (err && !IS_ERR_OR_NULL(buf->handle))
		free_mem(buf);
	return err;
}

static int context_list_ctor(struct smq_context_list *me, int size)
{
	int err = 0;
	VERIFY(err, 0 != (me->ls = kzalloc(size, GFP_KERNEL)));
	if (err)
		goto bail;
	me->size = size / sizeof(*me->ls);
	me->last = 0;
 bail:
	return err;
}

static void context_list_dtor(struct smq_context_list *me)
{
	kfree(me->ls);
	me->ls = 0;
}

static void context_list_alloc_ctx(struct smq_context_list *me,
					struct smq_invoke_ctx **po)
{
	int i = me->last;
	struct smq_invoke_ctx *ctx;

	for (;;) {
		i = i % me->size;
		ctx = &me->ls[i];
		if (atomic_read(&ctx->free) == 0)
			if (atomic_cmpxchg(&ctx->free, 0, 1) == 0)
				break;
		i++;
	}
	me->last = i;
	ctx->retval = -1;
	init_completion(&ctx->work);
	*po = ctx;
}

static void context_free(struct smq_invoke_ctx *me)
{
	if (me)
		atomic_set(&me->free, 0);
}

static void context_notify_user(struct smq_invoke_ctx *me, int retval)
{
	me->retval = retval;
	complete(&me->work);
}

static void context_notify_all_users(struct smq_context_list *me)
{
	int i;

	if (!me->ls)
		return;
	for (i = 0; i < me->size; ++i) {
		if (atomic_read(&me->ls[i].free) != 0)
			complete(&me->ls[i].work);
	}
}

static int get_page_list(uint32_t kernel, uint32_t sc, remote_arg_t *pra,
			struct fastrpc_buf *ibuf, struct fastrpc_buf *obuf)
{
	struct smq_phy_page *pgstart, *pages;
	struct smq_invoke_buf *list;
	int i, rlen, err = 0;
	int inbufs = REMOTE_SCALARS_INBUFS(sc);
	int outbufs = REMOTE_SCALARS_OUTBUFS(sc);

	LOCK_MMAP(kernel);
	*obuf = *ibuf;
 retry:
	list = smq_invoke_buf_start((remote_arg_t *)obuf->virt, sc);
	pgstart = smq_phy_page_start(sc, list);
	pages = pgstart + 1;
	rlen = obuf->size - ((uint32_t)pages - (uint32_t)obuf->virt);
	if (rlen < 0) {
		rlen = ((uint32_t)pages - (uint32_t)obuf->virt) - obuf->size;
		obuf->size += buf_page_size(rlen);
		obuf->handle = 0;
		VERIFY(err, 0 == alloc_mem(obuf));
		if (err)
			goto bail;
		goto retry;
	}
	pgstart->addr = obuf->phys;
	pgstart->size = obuf->size;
	for (i = 0; i < inbufs + outbufs; ++i) {
		void *buf;
		int len, num;

		list[i].num = 0;
		list[i].pgidx = 0;
		len = pra[i].buf.len;
		if (!len)
			continue;
		buf = pra[i].buf.pv;
		num = buf_num_pages(buf, len);
		if (!kernel)
			list[i].num = buf_get_pages(buf, len, num,
				i >= inbufs, pages, rlen / sizeof(*pages));
		else
			list[i].num = 0;
		VERIFY(err, list[i].num >= 0);
		if (err)
			goto bail;
		if (list[i].num) {
			list[i].pgidx = pages - pgstart;
			pages = pages + list[i].num;
		} else if (rlen > sizeof(*pages)) {
			list[i].pgidx = pages - pgstart;
			pages = pages + 1;
		} else {
			if (obuf->handle != ibuf->handle)
				free_mem(obuf);
			obuf->size += buf_page_size(sizeof(*pages));
			obuf->handle = 0;
			VERIFY(err, 0 == alloc_mem(obuf));
			if (err)
				goto bail;
			goto retry;
		}
		rlen = obuf->size - ((uint32_t) pages - (uint32_t) obuf->virt);
	}
	obuf->used = obuf->size - rlen;
 bail:
	if (err && (obuf->handle != ibuf->handle))
		free_mem(obuf);
	UNLOCK_MMAP(kernel);
	return err;
}

static int get_args(uint32_t kernel, uint32_t sc, remote_arg_t *pra,
			remote_arg_t *rpra, remote_arg_t *upra,
			struct fastrpc_buf *ibuf, struct fastrpc_buf **abufs,
			int *nbufs)
{
	struct smq_invoke_buf *list;
	struct fastrpc_buf *pbuf = ibuf, *obufs = 0;
	struct smq_phy_page *pages;
	void *args;
	int i, rlen, size, used, inh, bufs = 0, err = 0;
	int inbufs = REMOTE_SCALARS_INBUFS(sc);
	int outbufs = REMOTE_SCALARS_OUTBUFS(sc);

	list = smq_invoke_buf_start(rpra, sc);
	pages = smq_phy_page_start(sc, list);
	used = ALIGN(pbuf->used, BALIGN);
	args = (void *)((char *)pbuf->virt + used);
	rlen = pbuf->size - used;
	for (i = 0; i < inbufs + outbufs; ++i) {
		int num;

		rpra[i].buf.len = pra[i].buf.len;
		if (!rpra[i].buf.len)
			continue;
		if (list[i].num) {
			rpra[i].buf.pv = pra[i].buf.pv;
			continue;
		}
		if (rlen < pra[i].buf.len) {
			struct fastrpc_buf *b;
			pbuf->used = pbuf->size - rlen;
			VERIFY(err, 0 != (b = krealloc(obufs,
				 (bufs + 1) * sizeof(*obufs), GFP_KERNEL)));
			if (err)
				goto bail;
			obufs = b;
			pbuf = obufs + bufs;
			pbuf->size = buf_num_pages(0, pra[i].buf.len) *
								PAGE_SIZE;
			VERIFY(err, 0 == alloc_mem(pbuf));
			if (err)
				goto bail;
			bufs++;
			args = pbuf->virt;
			rlen = pbuf->size;
		}
		num = buf_num_pages(args, pra[i].buf.len);
		if (pbuf == ibuf) {
			list[i].num = num;
			list[i].pgidx = 0;
		} else {
			list[i].num = 1;
			pages[list[i].pgidx].addr =
				buf_page_start((void *)(pbuf->phys +
							 (pbuf->size - rlen)));
			pages[list[i].pgidx].size =
				buf_page_size(pra[i].buf.len);
		}
		if (i < inbufs) {
			if (!kernel) {
				VERIFY(err, 0 == copy_from_user(args,
						pra[i].buf.pv, pra[i].buf.len));
				if (err)
					goto bail;
			} else {
				memmove(args, pra[i].buf.pv, pra[i].buf.len);
			}
		}
		rpra[i].buf.pv = args;
		args = (void *)((char *)args + ALIGN(pra[i].buf.len, BALIGN));
		rlen -= ALIGN(pra[i].buf.len, BALIGN);
	}
	for (i = 0; i < inbufs; ++i) {
		if (rpra[i].buf.len)
			dmac_flush_range(rpra[i].buf.pv,
				  (char *)rpra[i].buf.pv + rpra[i].buf.len);
	}
	pbuf->used = pbuf->size - rlen;
	size = sizeof(*rpra) * REMOTE_SCALARS_INHANDLES(sc);
	if (size) {
		inh = inbufs + outbufs;
		if (!kernel) {
			VERIFY(err, 0 == copy_from_user(&rpra[inh], &upra[inh],
							size));
			if (err)
				goto bail;
		} else {
			memmove(&rpra[inh], &upra[inh], size);
		}
	}
	dmac_flush_range(rpra, (char *)rpra + used);
 bail:
	*abufs = obufs;
	*nbufs = bufs;
	return err;
}

static int put_args(uint32_t kernel, uint32_t sc, remote_arg_t *pra,
			remote_arg_t *rpra, remote_arg_t *upra)
{
	int i, inbufs, outbufs, outh, size;
	int err = 0;

	inbufs = REMOTE_SCALARS_INBUFS(sc);
	outbufs = REMOTE_SCALARS_OUTBUFS(sc);
	for (i = inbufs; i < inbufs + outbufs; ++i) {
		if (rpra[i].buf.pv != pra[i].buf.pv) {
			VERIFY(err, 0 == copy_to_user(pra[i].buf.pv,
					rpra[i].buf.pv, rpra[i].buf.len));
			if (err)
				goto bail;
		}
	}
	size = sizeof(*rpra) * REMOTE_SCALARS_OUTHANDLES(sc);
	if (size) {
		outh = inbufs + outbufs + REMOTE_SCALARS_INHANDLES(sc);
		if (!kernel) {
			VERIFY(err, 0 == copy_to_user(&upra[outh], &rpra[outh],
						size));
			if (err)
				goto bail;
		} else {
			memmove(&upra[outh], &rpra[outh], size);
		}
	}
 bail:
	return err;
}

static void inv_args(uint32_t sc, remote_arg_t *rpra, int used)
{
	int i, inbufs, outbufs;
	int inv = 0;

	inbufs = REMOTE_SCALARS_INBUFS(sc);
	outbufs = REMOTE_SCALARS_OUTBUFS(sc);
	for (i = inbufs; i < inbufs + outbufs; ++i) {
		if (buf_page_start(rpra) == buf_page_start(rpra[i].buf.pv))
			inv = 1;
		else if (rpra[i].buf.len)
			dmac_inv_range(rpra[i].buf.pv,
				(char *)rpra[i].buf.pv + rpra[i].buf.len);
	}

	if (inv || REMOTE_SCALARS_OUTHANDLES(sc))
		dmac_inv_range(rpra, (char *)rpra + used);
}

static int fastrpc_invoke_send(struct fastrpc_apps *me, uint32_t handle,
				 uint32_t sc, struct smq_invoke_ctx *ctx,
				 struct fastrpc_buf *buf)
{
	struct smq_msg msg;
	int err = 0, len;

	msg.pid = current->tgid;
	msg.tid = current->pid;
	msg.invoke.header.ctx = ctx;
	msg.invoke.header.handle = handle;
	msg.invoke.header.sc = sc;
	msg.invoke.page.addr = buf->phys;
	msg.invoke.page.size = buf_page_size(buf->used);
	spin_lock(&me->wrlock);
	len = smd_write(me->chan, &msg, sizeof(msg));
	spin_unlock(&me->wrlock);
	VERIFY(err, len == sizeof(msg));
	return err;
}

static void fastrpc_deinit(void)
{
	struct fastrpc_apps *me = &gfa;

	if (me->chan)
		(void)smd_close(me->chan);
	context_list_dtor(&me->clst);
	if (me->iclient)
		ion_client_destroy(me->iclient);
	me->iclient = 0;
	me->chan = 0;
}

static void fastrpc_read_handler(void)
{
	struct fastrpc_apps *me = &gfa;
	struct smq_invoke_rsp rsp;
	int err = 0;

	do {
		VERIFY(err, sizeof(rsp) ==
				 smd_read_from_cb(me->chan, &rsp, sizeof(rsp)));
		if (err)
			goto bail;
		context_notify_user(rsp.ctx, rsp.retval);
	} while (!err);
 bail:
	return;
}

static void smd_event_handler(void *priv, unsigned event)
{
	struct fastrpc_apps *me = (struct fastrpc_apps *)priv;

	switch (event) {
	case SMD_EVENT_OPEN:
		complete(&(me->work));
		break;
	case SMD_EVENT_CLOSE:
		context_notify_all_users(&me->clst);
		break;
	case SMD_EVENT_DATA:
		fastrpc_read_handler();
		break;
	}
}

static int fastrpc_init(void)
{
	int err = 0;
	struct fastrpc_apps *me = &gfa;

	if (me->chan == 0) {
		int i;
		spin_lock_init(&me->hlock);
		spin_lock_init(&me->wrlock);
		init_completion(&me->work);
		for (i = 0; i < RPC_HASH_SZ; ++i)
			INIT_HLIST_HEAD(&me->htbl[i]);
		VERIFY(err, 0 == context_list_ctor(&me->clst, SZ_4K));
		if (err)
			goto bail;
		me->iclient = msm_ion_client_create(ION_HEAP_CARVEOUT_MASK,
							DEVICE_NAME);
		VERIFY(err, 0 == IS_ERR_OR_NULL(me->iclient));
		if (err)
			goto bail;
		VERIFY(err, 0 == smd_named_open_on_edge(FASTRPC_SMD_GUID,
						SMD_APPS_QDSP, &me->chan,
						me, smd_event_handler));
		if (err)
			goto bail;
		VERIFY(err, 0 != wait_for_completion_timeout(&me->work,
							RPC_TIMEOUT));
		if (err)
			goto bail;
	}
 bail:
	if (err)
		fastrpc_deinit();
	return err;
}

static void free_dev(struct fastrpc_device *dev)
{
	if (dev) {
		module_put(THIS_MODULE);
		free_mem(&dev->buf);
		kfree(dev);
	}
}

static int alloc_dev(struct fastrpc_device **dev)
{
	int err = 0;
	struct fastrpc_device *fd = 0;

	VERIFY(err, 0 != try_module_get(THIS_MODULE));
	if (err)
		goto bail;
	VERIFY(err, 0 != (fd = kzalloc(sizeof(*fd), GFP_KERNEL)));
	if (err)
		goto bail;
	fd->buf.size = PAGE_SIZE;
	VERIFY(err, 0 == alloc_mem(&fd->buf));
	if (err)
		goto bail;
	fd->tgid = current->tgid;
	INIT_HLIST_NODE(&fd->hn);
	*dev = fd;
 bail:
	if (err)
		free_dev(fd);
	return err;
}

static int get_dev(struct fastrpc_apps *me, struct fastrpc_device **rdev)
{
	struct hlist_head *head;
	struct fastrpc_device *dev = 0;
	struct hlist_node *n;
	uint32_t h = hash_32(current->tgid, RPC_HASH_BITS);
	int err = 0;

	spin_lock(&me->hlock);
	head = &me->htbl[h];
	hlist_for_each_entry(dev, n, head, hn) {
		if (dev->tgid == current->tgid) {
			hlist_del(&dev->hn);
			break;
		}
	}
	spin_unlock(&me->hlock);
	VERIFY(err, dev != 0);
	if (err)
		goto bail;
	*rdev = dev;
 bail:
	if (err) {
		free_dev(dev);
		err = alloc_dev(rdev);
	}
	return err;
}

static void add_dev(struct fastrpc_apps *me, struct fastrpc_device *dev)
{
	struct hlist_head *head;
	uint32_t h = hash_32(current->tgid, RPC_HASH_BITS);

	spin_lock(&me->hlock);
	head = &me->htbl[h];
	hlist_add_head(&dev->hn, head);
	spin_unlock(&me->hlock);
	return;
}

static int fastrpc_release_current_dsp_process(void);

static int fastrpc_internal_invoke(struct fastrpc_apps *me, uint32_t kernel,
			struct fastrpc_ioctl_invoke *invoke, remote_arg_t *pra)
{
	remote_arg_t *rpra = 0;
	struct fastrpc_device *dev = 0;
	struct smq_invoke_ctx *ctx = 0;
	struct fastrpc_buf obuf, *abufs = 0, *b;
	int interrupted = 0;
	uint32_t sc;
	int i, nbufs = 0, err = 0;

	sc = invoke->sc;
	obuf.handle = 0;
	if (REMOTE_SCALARS_LENGTH(sc)) {
		VERIFY(err, 0 == get_dev(me, &dev));
		if (err)
			goto bail;
		VERIFY(err, 0 == get_page_list(kernel, sc, pra, &dev->buf,
						&obuf));
		if (err)
			goto bail;
		rpra = (remote_arg_t *)obuf.virt;
		VERIFY(err, 0 == get_args(kernel, sc, pra, rpra, invoke->pra,
					&obuf, &abufs, &nbufs));
		if (err)
			goto bail;
	}

	context_list_alloc_ctx(&me->clst, &ctx);
	VERIFY(err, 0 == fastrpc_invoke_send(me, invoke->handle, sc, ctx,
						&obuf));
	if (err)
		goto bail;
	inv_args(sc, rpra, obuf.used);
	VERIFY(err, 0 == (interrupted =
			wait_for_completion_interruptible(&ctx->work)));
	if (err)
		goto bail;
	VERIFY(err, 0 == (err = ctx->retval));
	if (err)
		goto bail;
	VERIFY(err, 0 == put_args(kernel, sc, pra, rpra, invoke->pra));
	if (err)
		goto bail;
 bail:
	if (interrupted) {
		if (!kernel)
			(void)fastrpc_release_current_dsp_process();
		wait_for_completion(&ctx->work);
	}
	context_free(ctx);
	for (i = 0, b = abufs; i < nbufs; ++i, ++b)
		free_mem(b);
	kfree(abufs);
	if (dev) {
		add_dev(me, dev);
		if (obuf.handle != dev->buf.handle)
			free_mem(&obuf);
	}
	return err;
}

static int fastrpc_create_current_dsp_process(void)
{
	int err = 0;
	struct fastrpc_ioctl_invoke ioctl;
	struct fastrpc_apps *me = &gfa;
	remote_arg_t ra[1];
	int tgid = 0;

	tgid = current->tgid;
	ra[0].buf.pv = &tgid;
	ra[0].buf.len = sizeof(tgid);
	ioctl.handle = 1;
	ioctl.sc = REMOTE_SCALARS_MAKE(0, 1, 0);
	ioctl.pra = ra;
	VERIFY(err, 0 == fastrpc_internal_invoke(me, 1, &ioctl, ra));
	return err;
}

static int fastrpc_release_current_dsp_process(void)
{
	int err = 0;
	struct fastrpc_apps *me = &gfa;
	struct fastrpc_ioctl_invoke ioctl;
	remote_arg_t ra[1];
	int tgid = 0;

	tgid = current->tgid;
	ra[0].buf.pv = &tgid;
	ra[0].buf.len = sizeof(tgid);
	ioctl.handle = 1;
	ioctl.sc = REMOTE_SCALARS_MAKE(1, 1, 0);
	ioctl.pra = ra;
	VERIFY(err, 0 == fastrpc_internal_invoke(me, 1, &ioctl, ra));
	return err;
}

static void cleanup_current_dev(void)
{
	struct fastrpc_apps *me = &gfa;
	uint32_t h = hash_32(current->tgid, RPC_HASH_BITS);
	struct hlist_head *head;
	struct hlist_node *pos;
	struct fastrpc_device *dev;

 rnext:
	dev = 0;
	spin_lock(&me->hlock);
	head = &me->htbl[h];
	hlist_for_each_entry(dev, pos, head, hn) {
		if (dev->tgid == current->tgid) {
			hlist_del(&dev->hn);
			break;
		}
	}
	spin_unlock(&me->hlock);
	if (dev) {
		free_dev(dev);
		goto rnext;
	}
	return;
}

static int fastrpc_device_release(struct inode *inode, struct file *file)
{
	(void)fastrpc_release_current_dsp_process();
	cleanup_current_dev();
	return 0;
}

static int fastrpc_device_open(struct inode *inode, struct file *filp)
{
	int err = 0;

	if (0 != try_module_get(THIS_MODULE)) {
		/* This call will cause a dev to be created
		 * which will addref this module
		 */
		VERIFY(err, 0 == fastrpc_create_current_dsp_process());
		if (err)
			cleanup_current_dev();
		module_put(THIS_MODULE);
	}
	return err;
}


static long fastrpc_device_ioctl(struct file *file, unsigned int ioctl_num,
				 unsigned long ioctl_param)
{
	struct fastrpc_apps *me = &gfa;
	struct fastrpc_ioctl_invoke invoke;
	remote_arg_t *pra = 0;
	void *param = (char *)ioctl_param;
	int bufs, err = 0;

	switch (ioctl_num) {
	case FASTRPC_IOCTL_INVOKE:
		VERIFY(err, 0 == copy_from_user(&invoke, param,
						sizeof(invoke)));
		if (err)
			goto bail;
		bufs = REMOTE_SCALARS_INBUFS(invoke.sc) +
			REMOTE_SCALARS_OUTBUFS(invoke.sc);
		if (bufs) {
			bufs = bufs * sizeof(*pra);
			VERIFY(err, 0 != (pra = kmalloc(bufs, GFP_KERNEL)));
			if (err)
				goto bail;
		}
		VERIFY(err, 0 == copy_from_user(pra, invoke.pra, bufs));
		if (err)
			goto bail;
		VERIFY(err, 0 == (err = fastrpc_internal_invoke(me, 0, &invoke,
								pra)));
		if (err)
			goto bail;
		break;
	default:
		err = -ENOTTY;
		break;
	}
 bail:
	kfree(pra);
	return err;
}

static const struct file_operations fops = {
	.open = fastrpc_device_open,
	.release = fastrpc_device_release,
	.unlocked_ioctl = fastrpc_device_ioctl,
};

static int __init fastrpc_device_init(void)
{
	struct fastrpc_apps *me = &gfa;
	int err = 0;

	VERIFY(err, 0 == fastrpc_init());
	if (err)
		goto bail;
	VERIFY(err, 0 == alloc_chrdev_region(&me->dev_no, 0, 1, DEVICE_NAME));
	if (err)
		goto bail;
	cdev_init(&me->cdev, &fops);
	me->cdev.owner = THIS_MODULE;
	VERIFY(err, 0 == cdev_add(&me->cdev, MKDEV(MAJOR(me->dev_no), 0), 1));
	if (err)
		goto bail;
	pr_info("'mknod /dev/%s c %d 0'\n", DEVICE_NAME, MAJOR(me->dev_no));
 bail:
	if (err) {
		if (me->dev_no)
			unregister_chrdev_region(me->dev_no, 1);
		fastrpc_deinit();
	}
	return err;
}

static void __exit fastrpc_device_exit(void)
{
	struct fastrpc_apps *me = &gfa;

	fastrpc_deinit();
	cdev_del(&me->cdev);
	unregister_chrdev_region(me->dev_no, 1);
}

module_init(fastrpc_device_init);
module_exit(fastrpc_device_exit);

MODULE_LICENSE("GPL v2");
