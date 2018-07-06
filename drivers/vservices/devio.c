/*
 * devio.c - cdev I/O for service devices
 *
 * Copyright (c) 2016 Cog Systems Pty Ltd
 *     Author: Philip Derrin <philip@cog.systems>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/pagemap.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/uio.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/security.h>
#include <linux/compat.h>

#include <vservices/types.h>
#include <vservices/buffer.h>
#include <vservices/transport.h>
#include <vservices/session.h>
#include <vservices/service.h>
#include <vservices/ioctl.h>
#include "session.h"

#define VSERVICES_DEVICE_MAX (VS_MAX_SERVICES * VS_MAX_SESSIONS)

struct vs_devio_priv {
	struct kref kref;
	bool running, reset;

	/* Receive queue */
	wait_queue_head_t recv_wq;
	atomic_t notify_pending;
	struct list_head recv_queue;
};

static void
vs_devio_priv_free(struct kref *kref)
{
	struct vs_devio_priv *priv = container_of(kref, struct vs_devio_priv,
			kref);

	WARN_ON(priv->running);
	WARN_ON(!list_empty_careful(&priv->recv_queue));
	WARN_ON(waitqueue_active(&priv->recv_wq));

	kfree(priv);
}

static void vs_devio_priv_put(struct vs_devio_priv *priv)
{
	kref_put(&priv->kref, vs_devio_priv_free);
}

static int
vs_devio_service_probe(struct vs_service_device *service)
{
	struct vs_devio_priv *priv;

	priv = kmalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	kref_init(&priv->kref);
	priv->running = false;
	priv->reset = false;
	init_waitqueue_head(&priv->recv_wq);
	atomic_set(&priv->notify_pending, 0);
	INIT_LIST_HEAD(&priv->recv_queue);

	dev_set_drvdata(&service->dev, priv);

	wake_up(&service->quota_wq);

	return 0;
}

static int
vs_devio_service_remove(struct vs_service_device *service)
{
	struct vs_devio_priv *priv = dev_get_drvdata(&service->dev);

	WARN_ON(priv->running);
	WARN_ON(!list_empty_careful(&priv->recv_queue));
	WARN_ON(waitqueue_active(&priv->recv_wq));

	vs_devio_priv_put(priv);

	return 0;
}

static int
vs_devio_service_receive(struct vs_service_device *service,
		struct vs_mbuf *mbuf)
{
	struct vs_devio_priv *priv = dev_get_drvdata(&service->dev);

	WARN_ON(!priv->running);

	spin_lock(&priv->recv_wq.lock);
	list_add_tail(&mbuf->queue, &priv->recv_queue);
	wake_up_locked(&priv->recv_wq);
	spin_unlock(&priv->recv_wq.lock);

	return 0;
}

static void
vs_devio_service_notify(struct vs_service_device *service, u32 flags)
{
	struct vs_devio_priv *priv = dev_get_drvdata(&service->dev);
	int old, cur;

	WARN_ON(!priv->running);

	if (!flags)
		return;

	/* open-coded atomic_or() */
	cur = atomic_read(&priv->notify_pending);
	while ((old = atomic_cmpxchg(&priv->notify_pending,
					cur, cur | flags)) != cur)
		cur = old;

	wake_up(&priv->recv_wq);
}

static void
vs_devio_service_start(struct vs_service_device *service)
{
	struct vs_devio_priv *priv = dev_get_drvdata(&service->dev);

	if (!priv->reset) {
		WARN_ON(priv->running);
		priv->running = true;
		wake_up(&service->quota_wq);
	}
}

static void
vs_devio_service_reset(struct vs_service_device *service)
{
	struct vs_devio_priv *priv = dev_get_drvdata(&service->dev);
	struct vs_mbuf *mbuf, *tmp;

	WARN_ON(!priv->running && !priv->reset);

	/*
	 * Mark the service as being in reset. This flag can never be cleared
	 * on an open device; the user must acknowledge the reset by closing
	 * and reopening the device.
	 */
	priv->reset = true;
	priv->running = false;

	spin_lock_irq(&priv->recv_wq.lock);
	list_for_each_entry_safe(mbuf, tmp, &priv->recv_queue, queue)
		vs_service_free_mbuf(service, mbuf);
	INIT_LIST_HEAD(&priv->recv_queue);
	spin_unlock_irq(&priv->recv_wq.lock);
	wake_up_all(&priv->recv_wq);
}

/*
 * This driver will be registered by the core server module, which must also
 * set its bus and owner function pointers.
 */
struct vs_service_driver vs_devio_server_driver = {
	/* No protocol, so the normal bus match will never bind this. */
	.protocol	= NULL,
	.is_server	= true,
	.rx_atomic	= true,

	.probe		= vs_devio_service_probe,
	.remove		= vs_devio_service_remove,
	.receive	= vs_devio_service_receive,
	.notify		= vs_devio_service_notify,
	.start		= vs_devio_service_start,
	.reset		= vs_devio_service_reset,

	/*
	 * Set reasonable default quotas. These can be overridden by passing
	 * nonzero values to IOCTL_VS_BIND_SERVER, which will set the
	 * service's *_quota_set fields.
	 */
	.in_quota_min	= 1,
	.in_quota_best	= 8,
	.out_quota_min	= 1,
	.out_quota_best	= 8,

	/* Mark the notify counts as invalid; the service's will be used. */
	.in_notify_count = (unsigned)-1,
	.out_notify_count = (unsigned)-1,

	.driver		= {
		.name			= "vservices-server-devio",
		.owner			= NULL, /* set by core server */
		.bus			= NULL, /* set by core server */
		.suppress_bind_attrs	= true, /* see vs_devio_poll */
	},
};
EXPORT_SYMBOL_GPL(vs_devio_server_driver);

static int
vs_devio_bind_server(struct vs_service_device *service,
		struct vs_ioctl_bind *bind)
{
	int ret = -ENODEV;

	/* Ensure the server module is loaded and the driver is registered. */
	if (!try_module_get(vs_devio_server_driver.driver.owner))
		goto fail_module_get;

	device_lock(&service->dev);
	ret = -EBUSY;
	if (service->dev.driver != NULL)
		goto fail_device_unbound;

	/* Set up the quota and notify counts. */
	service->in_quota_set = bind->recv_quota;
	service->out_quota_set = bind->send_quota;
	service->notify_send_bits = bind->send_notify_bits;
	service->notify_recv_bits = bind->recv_notify_bits;

	/* Manually probe the driver. */
	service->dev.driver = &vs_devio_server_driver.driver;
	ret = service->dev.bus->probe(&service->dev);
	if (ret < 0)
		goto fail_probe_driver;

	ret = device_bind_driver(&service->dev);
	if (ret < 0)
		goto fail_bind_driver;

	/* Pass the allocated quotas back to the user. */
	bind->recv_quota = service->recv_quota;
	bind->send_quota = service->send_quota;
	bind->msg_size = vs_service_max_mbuf_size(service);

	device_unlock(&service->dev);
	module_put(vs_devio_server_driver.driver.owner);

	return 0;

fail_bind_driver:
	ret = service->dev.bus->remove(&service->dev);
fail_probe_driver:
	service->dev.driver = NULL;
fail_device_unbound:
	device_unlock(&service->dev);
	module_put(vs_devio_server_driver.driver.owner);
fail_module_get:
	return ret;
}

/*
 * This driver will be registered by the core client module, which must also
 * set its bus and owner pointers.
 */
struct vs_service_driver vs_devio_client_driver = {
	/* No protocol, so the normal bus match will never bind this. */
	.protocol	= NULL,
	.is_server	= false,
	.rx_atomic	= true,

	.probe		= vs_devio_service_probe,
	.remove		= vs_devio_service_remove,
	.receive	= vs_devio_service_receive,
	.notify		= vs_devio_service_notify,
	.start		= vs_devio_service_start,
	.reset		= vs_devio_service_reset,

	.driver		= {
		.name			= "vservices-client-devio",
		.owner			= NULL, /* set by core client */
		.bus			= NULL, /* set by core client */
		.suppress_bind_attrs	= true, /* see vs_devio_poll */
	},
};
EXPORT_SYMBOL_GPL(vs_devio_client_driver);

static int
vs_devio_bind_client(struct vs_service_device *service,
		struct vs_ioctl_bind *bind)
{
	int ret = -ENODEV;

	/* Ensure the client module is loaded and the driver is registered. */
	if (!try_module_get(vs_devio_client_driver.driver.owner))
		goto fail_module_get;

	device_lock(&service->dev);
	ret = -EBUSY;
	if (service->dev.driver != NULL)
		goto fail_device_unbound;

	/* Manually probe the driver. */
	service->dev.driver = &vs_devio_client_driver.driver;
	ret = service->dev.bus->probe(&service->dev);
	if (ret < 0)
		goto fail_probe_driver;

	ret = device_bind_driver(&service->dev);
	if (ret < 0)
		goto fail_bind_driver;

	/* Pass the allocated quotas back to the user. */
	bind->recv_quota = service->recv_quota;
	bind->send_quota = service->send_quota;
	bind->msg_size = vs_service_max_mbuf_size(service);
	bind->send_notify_bits = service->notify_send_bits;
	bind->recv_notify_bits = service->notify_recv_bits;

	device_unlock(&service->dev);
	module_put(vs_devio_client_driver.driver.owner);

	return 0;

fail_bind_driver:
	ret = service->dev.bus->remove(&service->dev);
fail_probe_driver:
	service->dev.driver = NULL;
fail_device_unbound:
	device_unlock(&service->dev);
	module_put(vs_devio_client_driver.driver.owner);
fail_module_get:
	return ret;
}

static struct vs_devio_priv *
vs_devio_priv_get_from_service(struct vs_service_device *service)
{
	struct vs_devio_priv *priv = NULL;
	struct device_driver *drv;

	if (!service)
		return NULL;

	device_lock(&service->dev);
	drv = service->dev.driver;

	if ((drv == &vs_devio_client_driver.driver) ||
			(drv == &vs_devio_server_driver.driver)) {
		vs_service_state_lock(service);
		priv = dev_get_drvdata(&service->dev);
		if (priv)
			kref_get(&priv->kref);
		vs_service_state_unlock(service);
	}

	device_unlock(&service->dev);

	return priv;
}

static int
vs_devio_open(struct inode *inode, struct file *file)
{
	struct vs_service_device *service;

	if (imajor(inode) != vservices_cdev_major)
		return -ENODEV;

	service = vs_service_lookup_by_devt(inode->i_rdev);
	if (!service)
		return -ENODEV;

	file->private_data = service;

	return 0;
}

static int
vs_devio_release(struct inode *inode, struct file *file)
{
	struct vs_service_device *service = file->private_data;

	if (service) {
		struct vs_devio_priv *priv =
			vs_devio_priv_get_from_service(service);

		if (priv) {
			device_release_driver(&service->dev);
			vs_devio_priv_put(priv);
		}

		file->private_data = NULL;
		vs_put_service(service);
	}

	return 0;
}

static struct iovec *
vs_devio_check_iov(struct vs_ioctl_iovec *io, bool is_send, ssize_t *total)
{
	struct iovec *iov;
	unsigned i;
	int ret;

	if (io->iovcnt > UIO_MAXIOV)
		return ERR_PTR(-EINVAL);

	iov = kmalloc(sizeof(*iov) * io->iovcnt, GFP_KERNEL);
	if (!iov)
		return ERR_PTR(-ENOMEM);

	if (copy_from_user(iov, io->iov, sizeof(*iov) * io->iovcnt)) {
		ret = -EFAULT;
		goto fail;
	}

	*total = 0;
	for (i = 0; i < io->iovcnt; i++) {
		ssize_t iov_len = (ssize_t)iov[i].iov_len;

		if (iov_len > MAX_RW_COUNT - *total) {
			ret = -EINVAL;
			goto fail;
		}

		if (!access_ok(is_send ? VERIFY_READ : VERIFY_WRITE,
					iov[i].iov_base, iov_len)) {
			ret = -EFAULT;
			goto fail;
		}

		*total += iov_len;
	}

	return iov;

fail:
	kfree(iov);
	return ERR_PTR(ret);
}

static ssize_t
vs_devio_send(struct vs_service_device *service, struct iovec *iov,
		size_t iovcnt, ssize_t to_send, bool nonblocking)
{
	struct vs_mbuf *mbuf = NULL;
	struct vs_devio_priv *priv;
	unsigned i;
	ssize_t offset = 0;
	ssize_t ret;
	DEFINE_WAIT(wait);

	priv = vs_devio_priv_get_from_service(service);
	ret = -ENODEV;
	if (!priv)
		goto fail_priv_get;

	vs_service_state_lock(service);

	/*
	 * Waiting alloc. We must open-code this because there is no real
	 * state structure or base state.
	 */
	ret = 0;
	while (!vs_service_send_mbufs_available(service)) {
		if (nonblocking) {
			ret = -EAGAIN;
			break;
		}

		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			break;
		}

		prepare_to_wait_exclusive(&service->quota_wq, &wait,
				TASK_INTERRUPTIBLE);

		vs_service_state_unlock(service);
		schedule();
		vs_service_state_lock(service);

		if (priv->reset) {
			ret = -ECONNRESET;
			break;
		}

		if (!priv->running) {
			ret = -ENOTCONN;
			break;
		}
	}
	finish_wait(&service->quota_wq, &wait);

	if (ret)
		goto fail_alloc;

	mbuf = vs_service_alloc_mbuf(service, to_send, GFP_KERNEL);
	if (IS_ERR(mbuf)) {
		ret = PTR_ERR(mbuf);
		goto fail_alloc;
	}

	/* Ready to send; copy data into the mbuf. */
	ret = -EFAULT;
	for (i = 0; i < iovcnt; i++) {
		if (copy_from_user(mbuf->data + offset, iov[i].iov_base,
					iov[i].iov_len))
			goto fail_copy;
		offset += iov[i].iov_len;
	}
	mbuf->size = to_send;

	/* Send the message. */
	ret = vs_service_send(service, mbuf);
	if (ret < 0)
		goto fail_send;

	/* Wake the next waiter, if there's more quota available. */
	if (waitqueue_active(&service->quota_wq) &&
			vs_service_send_mbufs_available(service) > 0)
		wake_up(&service->quota_wq);

	vs_service_state_unlock(service);
	vs_devio_priv_put(priv);

	return to_send;

fail_send:
fail_copy:
	vs_service_free_mbuf(service, mbuf);
	wake_up(&service->quota_wq);
fail_alloc:
	vs_service_state_unlock(service);
	vs_devio_priv_put(priv);
fail_priv_get:
	return ret;
}

static ssize_t
vs_devio_recv(struct vs_service_device *service, struct iovec *iov,
		size_t iovcnt, u32 *notify_bits, ssize_t recv_space,
		bool nonblocking)
{
	struct vs_mbuf *mbuf = NULL;
	struct vs_devio_priv *priv;
	unsigned i;
	ssize_t offset = 0;
	ssize_t ret;
	DEFINE_WAIT(wait);

	priv = vs_devio_priv_get_from_service(service);
	ret = -ENODEV;
	if (!priv)
		goto fail_priv_get;

	/* Take the recv_wq lock, which also protects recv_queue. */
	spin_lock_irq(&priv->recv_wq.lock);

	/* Wait for a message, notification, or reset. */
	ret = wait_event_interruptible_exclusive_locked_irq(priv->recv_wq,
			!list_empty(&priv->recv_queue) || priv->reset ||
			atomic_read(&priv->notify_pending) || nonblocking);

	if (priv->reset)
		ret = -ECONNRESET; /* Service reset */
	else if (!ret && list_empty(&priv->recv_queue))
		ret = -EAGAIN; /* Nonblocking, or notification */

	if (ret < 0) {
		spin_unlock_irq(&priv->recv_wq.lock);
		goto no_mbuf;
	}

	/* Take the first mbuf from the list, and check its size. */
	mbuf = list_first_entry(&priv->recv_queue, struct vs_mbuf, queue);
	if (mbuf->size > recv_space) {
		spin_unlock_irq(&priv->recv_wq.lock);
		ret = -EMSGSIZE;
		goto fail_msg_size;
	}
	list_del_init(&mbuf->queue);

	spin_unlock_irq(&priv->recv_wq.lock);

	/* Copy to user. */
	ret = -EFAULT;
	for (i = 0; (mbuf->size > offset) && (i < iovcnt); i++) {
		size_t len = min(mbuf->size - offset, iov[i].iov_len);
		if (copy_to_user(iov[i].iov_base, mbuf->data + offset, len))
			goto fail_copy;
		offset += len;
	}
	ret = offset;

no_mbuf:
	/*
	 * Read and clear the pending notification bits. If any notifications
	 * are received, don't return an error, even if we failed to receive a
	 * message.
	 */
	*notify_bits = atomic_xchg(&priv->notify_pending, 0);
	if ((ret < 0) && *notify_bits)
		ret = 0;

fail_copy:
	if (mbuf)
		vs_service_free_mbuf(service, mbuf);
fail_msg_size:
	vs_devio_priv_put(priv);
fail_priv_get:
	return ret;
}

static int
vs_devio_check_perms(struct file *file, unsigned flags)
{
	if ((flags & MAY_READ) & !(file->f_mode & FMODE_READ))
		return -EBADF;

	if ((flags & MAY_WRITE) & !(file->f_mode & FMODE_WRITE))
		return -EBADF;

	return security_file_permission(file, flags);
}

static long
vs_devio_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *ptr = (void __user *)arg;
	struct vs_service_device *service = file->private_data;
	struct vs_ioctl_bind bind;
	struct vs_ioctl_iovec io;
	u32 flags;
	long ret;
	ssize_t iov_total;
	struct iovec *iov;

	if (!service)
		return -ENODEV;

	switch (cmd) {
	case IOCTL_VS_RESET_SERVICE:
		ret = vs_devio_check_perms(file, MAY_WRITE);
		if (ret < 0)
			break;
		ret = vs_service_reset(service, service);
		break;
	case IOCTL_VS_GET_NAME:
		ret = vs_devio_check_perms(file, MAY_READ);
		if (ret < 0)
			break;
		if (service->name != NULL) {
			size_t len = strnlen(service->name,
					_IOC_SIZE(IOCTL_VS_GET_NAME) - 1);
			if (copy_to_user(ptr, service->name, len + 1))
				ret = -EFAULT;
		} else {
			ret = -EINVAL;
		}
		break;
	case IOCTL_VS_GET_PROTOCOL:
		ret = vs_devio_check_perms(file, MAY_READ);
		if (ret < 0)
			break;
		if (service->protocol != NULL) {
			size_t len = strnlen(service->protocol,
					_IOC_SIZE(IOCTL_VS_GET_PROTOCOL) - 1);
			if (copy_to_user(ptr, service->protocol, len + 1))
				ret = -EFAULT;
		} else {
			ret = -EINVAL;
		}
		break;
	case IOCTL_VS_BIND_CLIENT:
		ret = vs_devio_check_perms(file, MAY_EXEC);
		if (ret < 0)
			break;
		ret = vs_devio_bind_client(service, &bind);
		if (!ret && copy_to_user(ptr, &bind, sizeof(bind)))
			ret = -EFAULT;
		break;
	case IOCTL_VS_BIND_SERVER:
		ret = vs_devio_check_perms(file, MAY_EXEC);
		if (ret < 0)
			break;
		if (copy_from_user(&bind, ptr, sizeof(bind))) {
			ret = -EFAULT;
			break;
		}
		ret = vs_devio_bind_server(service, &bind);
		if (!ret && copy_to_user(ptr, &bind, sizeof(bind)))
			ret = -EFAULT;
		break;
	case IOCTL_VS_NOTIFY:
		ret = vs_devio_check_perms(file, MAY_WRITE);
		if (ret < 0)
			break;
		if (copy_from_user(&flags, ptr, sizeof(flags))) {
			ret = -EFAULT;
			break;
		}
		ret = vs_service_notify(service, flags);
		break;
	case IOCTL_VS_SEND:
		ret = vs_devio_check_perms(file, MAY_WRITE);
		if (ret < 0)
			break;
		if (copy_from_user(&io, ptr, sizeof(io))) {
			ret = -EFAULT;
			break;
		}

		iov = vs_devio_check_iov(&io, true, &iov_total);
		if (IS_ERR(iov)) {
			ret = PTR_ERR(iov);
			break;
		}

		ret = vs_devio_send(service, iov, io.iovcnt, iov_total,
				file->f_flags & O_NONBLOCK);
		kfree(iov);
		break;
	case IOCTL_VS_RECV:
		ret = vs_devio_check_perms(file, MAY_READ);
		if (ret < 0)
			break;
		if (copy_from_user(&io, ptr, sizeof(io))) {
			ret = -EFAULT;
			break;
		}

		iov = vs_devio_check_iov(&io, true, &iov_total);
		if (IS_ERR(iov)) {
			ret = PTR_ERR(iov);
			break;
		}

		ret = vs_devio_recv(service, iov, io.iovcnt,
			&io.notify_bits, iov_total,
			file->f_flags & O_NONBLOCK);
		kfree(iov);

		if (ret >= 0) {
			u32 __user *notify_bits_ptr = ptr + offsetof(
					struct vs_ioctl_iovec, notify_bits);
			if (copy_to_user(notify_bits_ptr, &io.notify_bits,
					sizeof(io.notify_bits)))
				ret = -EFAULT;
		}
		break;
	default:
		dev_dbg(&service->dev, "Unknown ioctl %#x, arg: %lx\n", cmd,
				arg);
		ret = -ENOSYS;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT

struct vs_compat_ioctl_bind {
	__u32 send_quota;
	__u32 recv_quota;
	__u32 send_notify_bits;
	__u32 recv_notify_bits;
	compat_size_t msg_size;
};

#define compat_ioctl_bind_conv(dest, src) ({ \
	dest.send_quota = src.send_quota;		\
	dest.recv_quota = src.recv_quota;		\
	dest.send_notify_bits = src.send_notify_bits;	\
	dest.recv_notify_bits = src.recv_notify_bits;	\
	dest.msg_size = (compat_size_t)src.msg_size;	\
})

#define COMPAT_IOCTL_VS_BIND_CLIENT _IOR('4', 3, struct vs_compat_ioctl_bind)
#define COMPAT_IOCTL_VS_BIND_SERVER _IOWR('4', 4, struct vs_compat_ioctl_bind)

struct vs_compat_ioctl_iovec {
	union {
		__u32 iovcnt; /* input */
		__u32 notify_bits; /* output (recv only) */
	};
	compat_uptr_t iov;
};

#define COMPAT_IOCTL_VS_SEND \
    _IOW('4', 6, struct vs_compat_ioctl_iovec)
#define COMPAT_IOCTL_VS_RECV \
    _IOWR('4', 7, struct vs_compat_ioctl_iovec)

static struct iovec *
vs_devio_check_compat_iov(struct vs_compat_ioctl_iovec *c_io,
	bool is_send, ssize_t *total)
{
	struct iovec *iov;
	struct compat_iovec *c_iov;

	unsigned i;
	int ret;

	if (c_io->iovcnt > UIO_MAXIOV)
		return ERR_PTR(-EINVAL);

	c_iov = kzalloc(sizeof(*c_iov) * c_io->iovcnt, GFP_KERNEL);
	if (!c_iov)
		return ERR_PTR(-ENOMEM);

	iov = kzalloc(sizeof(*iov) * c_io->iovcnt, GFP_KERNEL);
	if (!iov) {
		kfree(c_iov);
		return ERR_PTR(-ENOMEM);
	}

	if (copy_from_user(c_iov, (struct compat_iovec __user *)
		compat_ptr(c_io->iov), sizeof(*c_iov) * c_io->iovcnt)) {
		ret = -EFAULT;
		goto fail;
	}

	*total = 0;
	for (i = 0; i < c_io->iovcnt; i++) {
		ssize_t iov_len;
		iov[i].iov_base = compat_ptr (c_iov[i].iov_base);
		iov[i].iov_len = (compat_size_t) c_iov[i].iov_len;

		iov_len = (ssize_t)iov[i].iov_len;

		if (iov_len > MAX_RW_COUNT - *total) {
			ret = -EINVAL;
			goto fail;
		}

		if (!access_ok(is_send ? VERIFY_READ : VERIFY_WRITE,
					iov[i].iov_base, iov_len)) {
			ret = -EFAULT;
			goto fail;
		}

		*total += iov_len;
	}

	kfree (c_iov);
	return iov;

fail:
	kfree(c_iov);
	kfree(iov);
	return ERR_PTR(ret);
}

static long
vs_devio_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *ptr = (void __user *)arg;
	struct vs_service_device *service = file->private_data;
	struct vs_ioctl_bind bind;
	struct vs_compat_ioctl_bind compat_bind;
	struct vs_compat_ioctl_iovec compat_io;
	long ret;
	ssize_t iov_total;
	struct iovec *iov;

	if (!service)
		return -ENODEV;

	switch (cmd) {
	case IOCTL_VS_RESET_SERVICE:
	case IOCTL_VS_GET_NAME:
	case IOCTL_VS_GET_PROTOCOL:
		return vs_devio_ioctl (file, cmd, arg);
	case COMPAT_IOCTL_VS_SEND:
		ret = vs_devio_check_perms(file, MAY_WRITE);
		if (ret < 0)
			break;
		if (copy_from_user(&compat_io, ptr, sizeof(compat_io))) {
			ret = -EFAULT;
			break;
		}

		iov = vs_devio_check_compat_iov(&compat_io, true, &iov_total);
		if (IS_ERR(iov)) {
			ret = PTR_ERR(iov);
			break;
		}

		ret = vs_devio_send(service, iov, compat_io.iovcnt, iov_total,
				file->f_flags & O_NONBLOCK);
		kfree(iov);

		break;
	case COMPAT_IOCTL_VS_RECV:
		ret = vs_devio_check_perms(file, MAY_READ);
		if (ret < 0)
			break;
		if (copy_from_user(&compat_io, ptr, sizeof(compat_io))) {
			ret = -EFAULT;
			break;
		}

		iov = vs_devio_check_compat_iov(&compat_io, true, &iov_total);
		if (IS_ERR(iov)) {
			ret = PTR_ERR(iov);
			break;
		}

		ret = vs_devio_recv(service, iov, compat_io.iovcnt,
			&compat_io.notify_bits, iov_total,
			file->f_flags & O_NONBLOCK);
		kfree(iov);

		if (ret >= 0) {
			u32 __user *notify_bits_ptr = ptr + offsetof(
					struct vs_compat_ioctl_iovec, notify_bits);
			if (copy_to_user(notify_bits_ptr, &compat_io.notify_bits,
					sizeof(compat_io.notify_bits)))
				ret = -EFAULT;
		}
		break;
	case COMPAT_IOCTL_VS_BIND_CLIENT:
		ret = vs_devio_check_perms(file, MAY_EXEC);
		if (ret < 0)
			break;
		ret = vs_devio_bind_client(service, &bind);
		compat_ioctl_bind_conv(compat_bind, bind);
		if (!ret && copy_to_user(ptr, &compat_bind,
					sizeof(compat_bind)))
			ret = -EFAULT;
		break;
	case COMPAT_IOCTL_VS_BIND_SERVER:
		ret = vs_devio_check_perms(file, MAY_EXEC);
		if (ret < 0)
			break;
		if (copy_from_user(&compat_bind, ptr, sizeof(compat_bind))) {
			ret = -EFAULT;
			break;
		}
		compat_ioctl_bind_conv(bind, compat_bind);
		ret = vs_devio_bind_server(service, &bind);
		compat_ioctl_bind_conv(compat_bind, bind);
		if (!ret && copy_to_user(ptr, &compat_bind,
					sizeof(compat_bind)))
			ret = -EFAULT;
		break;
	default:
		dev_dbg(&service->dev, "Unknown ioctl %#x, arg: %lx\n", cmd,
				arg);
		ret = -ENOSYS;
		break;
	}

	return ret;
}

#endif /* CONFIG_COMPAT */

static unsigned int
vs_devio_poll(struct file *file, struct poll_table_struct *wait)
{
	struct vs_service_device *service = file->private_data;
	struct vs_devio_priv *priv = vs_devio_priv_get_from_service(service);
	unsigned int flags = 0;

	poll_wait(file, &service->quota_wq, wait);

	if (priv) {
		/*
		 * Note: there is no way for us to ensure that all poll
		 * waiters on a given workqueue have gone away, other than to
		 * actually close the file. So, this poll_wait() is only safe
		 * if we never release our claim on the service before the
		 * file is closed.
		 *
		 * We try to guarantee this by only unbinding the devio driver
		 * on close, and setting suppress_bind_attrs in the driver so
		 * root can't unbind us with sysfs.
		 */
		poll_wait(file, &priv->recv_wq, wait);

		if (priv->reset) {
			/* Service reset; raise poll error. */
			flags |= POLLERR | POLLHUP;
		} else if (priv->running) {
			if (!list_empty_careful(&priv->recv_queue))
				flags |= POLLRDNORM | POLLIN;
			if (atomic_read(&priv->notify_pending))
				flags |= POLLRDNORM | POLLIN;
			if (vs_service_send_mbufs_available(service) > 0)
				flags |= POLLWRNORM | POLLOUT;
		}

		vs_devio_priv_put(priv);
	} else {
		/* No driver attached. Return error flags. */
		flags |= POLLERR | POLLHUP;
	}

	return flags;
}

static const struct file_operations vs_fops = {
	.owner		= THIS_MODULE,
	.open		= vs_devio_open,
	.release	= vs_devio_release,
	.unlocked_ioctl	= vs_devio_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= vs_devio_compat_ioctl,
#endif
	.poll		= vs_devio_poll,
};

int vservices_cdev_major;
static struct cdev vs_cdev;

int __init
vs_devio_init(void)
{
	dev_t dev;
	int r;

	r = alloc_chrdev_region(&dev, 0, VSERVICES_DEVICE_MAX,
			"vs_service");
	if (r < 0)
		goto fail_alloc_chrdev;
	vservices_cdev_major = MAJOR(dev);

	cdev_init(&vs_cdev, &vs_fops);
	r = cdev_add(&vs_cdev, dev, VSERVICES_DEVICE_MAX);
	if (r < 0)
		goto fail_cdev_add;

	return 0;

fail_cdev_add:
	unregister_chrdev_region(dev, VSERVICES_DEVICE_MAX);
fail_alloc_chrdev:
	return r;
}

void __exit
vs_devio_exit(void)
{
	cdev_del(&vs_cdev);
	unregister_chrdev_region(MKDEV(vservices_cdev_major, 0),
			VSERVICES_DEVICE_MAX);
}
