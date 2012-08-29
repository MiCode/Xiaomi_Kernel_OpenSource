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
	int err = 0;

	buf->handle = ion_alloc(clnt, buf->size, SZ_4K,
				ION_HEAP(ION_AUDIO_HEAP_ID));
	VERIFY(0 == IS_ERR_OR_NULL(buf->handle));
	buf->virt = 0;
	VERIFY(0 != (buf->virt = ion_map_kernel(clnt, buf->handle,
						ION_SET_CACHE(CACHED))));
	VERIFY(0 == ion_phys(clnt, buf->handle, &buf->phys, &buf->size));
 bail:
	if (err && !IS_ERR_OR_NULL(buf->handle))
		free_mem(buf);
	return err;
}

static int context_list_ctor(struct smq_context_list *me, int size)
{
	int err = 0;
	VERIFY(0 != (me->ls = kzalloc(size, GFP_KERNEL)));
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
	int ii = me->last;
	struct smq_invoke_ctx *ctx;

	for (;;) {
		ii = ii % me->size;
		ctx = &me->ls[ii];
		if (atomic_read(&ctx->free) == 0)
			if (0 == atomic_cmpxchg(&ctx->free, 0, 1))
				break;
		ii++;
	}
	me->last = ii;
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
	int ii;

	if (!me->ls)
		return;
	for (ii = 0; ii < me->size; ++ii) {
		if (atomic_read(&me->ls[ii].free) != 0)
			complete(&me->ls[ii].work);
	}
}

static int get_page_list(uint32_t kernel, uint32_t sc, remote_arg_t *pra,
			struct fastrpc_buf *ibuf, struct fastrpc_buf *obuf)
{
	struct smq_phy_page *pgstart, *pages;
	struct smq_invoke_buf *list;
	int ii, rlen, err = 0;
	int inbufs = REMOTE_SCALARS_INBUFS(sc);
	int outbufs = REMOTE_SCALARS_OUTBUFS(sc);

	VERIFY(0 != try_module_get(THIS_MODULE));
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
		VERIFY(0 == alloc_mem(obuf));
		goto retry;
	}
	pgstart->addr = obuf->phys;
	pgstart->size = obuf->size;
	for (ii = 0; ii < inbufs + outbufs; ++ii) {
		void *buf;
		int len, num;

		len = pra[ii].buf.len;
		if (!len)
			continue;
		buf = pra[ii].buf.pv;
		num = buf_num_pages(buf, len);
		if (!kernel)
			list[ii].num = buf_get_pages(buf, len, num,
				ii >= inbufs, pages, rlen / sizeof(*pages));
		else
			list[ii].num = 0;
		VERIFY(list[ii].num >= 0);
		if (list[ii].num) {
			list[ii].pgidx = pages - pgstart;
			pages = pages + list[ii].num;
		} else if (rlen > sizeof(*pages)) {
			list[ii].pgidx = pages - pgstart;
			pages = pages + 1;
		} else {
			if (obuf->handle != ibuf->handle)
				free_mem(obuf);
			obuf->size += buf_page_size(sizeof(*pages));
			obuf->handle = 0;
			VERIFY(0 == alloc_mem(obuf));
			goto retry;
		}
		rlen = obuf->size - ((uint32_t) pages - (uint32_t) obuf->virt);
	}
	obuf->used = obuf->size - rlen;
 bail:
	if (err && (obuf->handle != ibuf->handle))
		free_mem(obuf);
	UNLOCK_MMAP(kernel);
	module_put(THIS_MODULE);
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
	int ii, rlen, size, used, inh, bufs = 0, err = 0;
	int inbufs = REMOTE_SCALARS_INBUFS(sc);
	int outbufs = REMOTE_SCALARS_OUTBUFS(sc);

	list = smq_invoke_buf_start(rpra, sc);
	pages = smq_phy_page_start(sc, list);
	used = ALIGN_8(pbuf->used);
	args = (void *)((char *)pbuf->virt + used);
	rlen = pbuf->size - used;
	for (ii = 0; ii < inbufs + outbufs; ++ii) {
		int num;

		rpra[ii].buf.len = pra[ii].buf.len;
		if (list[ii].num) {
			rpra[ii].buf.pv = pra[ii].buf.pv;
			continue;
		}
		if (rlen < pra[ii].buf.len) {
			struct fastrpc_buf *b;
			pbuf->used = pbuf->size - rlen;
			VERIFY(0 != (b = krealloc(obufs,
				 (bufs + 1) * sizeof(*obufs), GFP_KERNEL)));
			obufs = b;
			pbuf = obufs + bufs;
			pbuf->size = buf_num_pages(0, pra[ii].buf.len) *
								PAGE_SIZE;
			VERIFY(0 == alloc_mem(pbuf));
			bufs++;
			args = pbuf->virt;
			rlen = pbuf->size;
		}
		num = buf_num_pages(args, pra[ii].buf.len);
		if (pbuf == ibuf) {
			list[ii].num = num;
			list[ii].pgidx = 0;
		} else {
			list[ii].num = 1;
			pages[list[ii].pgidx].addr =
				buf_page_start((void *)(pbuf->phys +
							 (pbuf->size - rlen)));
			pages[list[ii].pgidx].size =
				buf_page_size(pra[ii].buf.len);
		}
		if (ii < inbufs) {
			if (!kernel)
				VERIFY(0 == copy_from_user(args, pra[ii].buf.pv,
							pra[ii].buf.len));
			else
				memmove(args, pra[ii].buf.pv, pra[ii].buf.len);
		}
		rpra[ii].buf.pv = args;
		args = (void *)((char *)args + ALIGN_8(pra[ii].buf.len));
		rlen -= ALIGN_8(pra[ii].buf.len);
	}
	for (ii = 0; ii < inbufs; ++ii) {
		if (rpra[ii].buf.len)
			dmac_flush_range(rpra[ii].buf.pv,
				  (char *)rpra[ii].buf.pv + rpra[ii].buf.len);
	}
	pbuf->used = pbuf->size - rlen;
	size = sizeof(*rpra) * REMOTE_SCALARS_INHANDLES(sc);
	if (size) {
		inh = inbufs + outbufs;
		if (!kernel)
			VERIFY(0 == copy_from_user(&rpra[inh], &upra[inh],
							size));
		else
			memmove(&rpra[inh], &upra[inh], size);
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
	int ii, inbufs, outbufs, outh, size;
	int err = 0;

	inbufs = REMOTE_SCALARS_INBUFS(sc);
	outbufs = REMOTE_SCALARS_OUTBUFS(sc);
	for (ii = inbufs; ii < inbufs + outbufs; ++ii) {
		if (rpra[ii].buf.pv != pra[ii].buf.pv)
			VERIFY(0 == copy_to_user(pra[ii].buf.pv,
					rpra[ii].buf.pv, rpra[ii].buf.len));
	}
	size = sizeof(*rpra) * REMOTE_SCALARS_OUTHANDLES(sc);
	if (size) {
		outh = inbufs + outbufs + REMOTE_SCALARS_INHANDLES(sc);
		if (!kernel)
			VERIFY(0 == copy_to_user(&upra[outh], &rpra[outh],
						size));
		else
			memmove(&upra[outh], &rpra[outh], size);
	}
 bail:
	return err;
}

static void inv_args(uint32_t sc, remote_arg_t *rpra, int used)
{
	int ii, inbufs, outbufs;
	int inv = 0;

	inbufs = REMOTE_SCALARS_INBUFS(sc);
	outbufs = REMOTE_SCALARS_OUTBUFS(sc);
	for (ii = inbufs; ii < inbufs + outbufs; ++ii) {
		if (buf_page_start(rpra) == buf_page_start(rpra[ii].buf.pv))
			inv = 1;
		else
			dmac_inv_range(rpra[ii].buf.pv,
				(char *)rpra[ii].buf.pv + rpra[ii].buf.len);
	}

	if (inv || REMOTE_SCALARS_OUTHANDLES(sc))
		dmac_inv_range(rpra, (char *)rpra + used);
}

static int fastrpc_invoke_send(struct fastrpc_apps *me, remote_handle_t handle,
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
	VERIFY(len == sizeof(msg));
 bail:
	return err;
}

static void fastrpc_deinit(void)
{
	struct fastrpc_apps *me = &gfa;

	if (me->chan)
		(void)smd_close(me->chan);
	context_list_dtor(&me->clst);
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
		VERIFY(sizeof(rsp) ==
				 smd_read_from_cb(me->chan, &rsp, sizeof(rsp)));
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
		int ii;
		spin_lock_init(&me->hlock);
		spin_lock_init(&me->wrlock);
		init_completion(&me->work);
		for (ii = 0; ii < RPC_HASH_SZ; ++ii)
			INIT_HLIST_HEAD(&me->htbl[ii]);
		VERIFY(0 == context_list_ctor(&me->clst, SZ_4K));
		me->iclient = msm_ion_client_create(ION_HEAP_CARVEOUT_MASK,
							DEVICE_NAME);
		VERIFY(0 == IS_ERR_OR_NULL(me->iclient));
		VERIFY(0 == smd_named_open_on_edge(FASTRPC_SMD_GUID,
						SMD_APPS_QDSP, &me->chan,
						me, smd_event_handler));
		VERIFY(0 != wait_for_completion_timeout(&me->work,
							RPC_TIMEOUT));
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

	VERIFY(0 != try_module_get(THIS_MODULE));
	VERIFY(0 != (fd = kzalloc(sizeof(*fd), GFP_KERNEL)));
	fd->buf.size = PAGE_SIZE;
	VERIFY(0 == alloc_mem(&fd->buf));
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
	VERIFY(dev != 0);
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
	int ii, nbufs = 0, err = 0;

	sc = invoke->sc;
	obuf.handle = 0;
	if (REMOTE_SCALARS_LENGTH(sc)) {
		VERIFY(0 == get_dev(me, &dev));
		VERIFY(0 == get_page_list(kernel, sc, pra, &dev->buf, &obuf));
		rpra = (remote_arg_t *)obuf.virt;
		VERIFY(0 == get_args(kernel, sc, pra, rpra, invoke->pra, &obuf,
					&abufs, &nbufs));
	}

	context_list_alloc_ctx(&me->clst, &ctx);
	VERIFY(0 == fastrpc_invoke_send(me, invoke->handle, sc, ctx, &obuf));
	inv_args(sc, rpra, obuf.used);
	VERIFY(0 == (interrupted =
			wait_for_completion_interruptible(&ctx->work)));
	VERIFY(0 == (err = ctx->retval));
	VERIFY(0 == put_args(kernel, sc, pra, rpra, invoke->pra));
 bail:
	if (interrupted) {
		init_completion(&ctx->work);
		if (!kernel)
			(void)fastrpc_release_current_dsp_process();
		wait_for_completion(&ctx->work);
	}
	context_free(ctx);
	for (ii = 0, b = abufs; ii < nbufs; ++ii, ++b)
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
	VERIFY(0 == fastrpc_internal_invoke(me, 1, &ioctl, ra));
 bail:
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
	VERIFY(0 == fastrpc_internal_invoke(me, 1, &ioctl, ra));
 bail:
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
		VERIFY(0 == fastrpc_create_current_dsp_process());
 bail:
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
		VERIFY(0 == copy_from_user(&invoke, param, sizeof(invoke)));
		bufs = REMOTE_SCALARS_INBUFS(invoke.sc) +
			REMOTE_SCALARS_OUTBUFS(invoke.sc);
		if (bufs) {
			bufs = bufs * sizeof(*pra);
			VERIFY(0 != (pra = kmalloc(bufs, GFP_KERNEL)));
		}
		VERIFY(0 == copy_from_user(pra, invoke.pra, bufs));
		VERIFY(0 == (err = fastrpc_internal_invoke(me, 0, &invoke,
								pra)));
		break;
	default:
		err = -EINVAL;
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

	VERIFY(0 == fastrpc_init());
	VERIFY(0 == alloc_chrdev_region(&me->dev_no, 0, 1, DEVICE_NAME));
	cdev_init(&me->cdev, &fops);
	me->cdev.owner = THIS_MODULE;
	VERIFY(0 == cdev_add(&me->cdev, MKDEV(MAJOR(me->dev_no), 0), 1));
	pr_info("'mknod /dev/%s c %d 0'\n", DEVICE_NAME, MAJOR(me->dev_no));
 bail:
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
