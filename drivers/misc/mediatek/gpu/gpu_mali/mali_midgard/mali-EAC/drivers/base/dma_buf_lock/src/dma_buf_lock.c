/*
 *
 * (C) COPYRIGHT ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */



#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/atomic.h>
#include <linux/dma-buf.h>
#include <linux/kds.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/anon_inodes.h>
#include <linux/file.h>

#include "dma_buf_lock.h"

/* Maximum number of buffers that a single handle can address */
#define DMA_BUF_LOCK_BUF_MAX 32

#define DMA_BUF_LOCK_DEBUG 1

static dev_t dma_buf_lock_dev;
static struct cdev dma_buf_lock_cdev;
static struct class *dma_buf_lock_class;
static char dma_buf_lock_dev_name[] = "dma_buf_lock";

#ifdef HAVE_UNLOCKED_IOCTL
static long dma_buf_lock_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
#else
static int dma_buf_lock_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);
#endif

static struct file_operations dma_buf_lock_fops =
{
	.owner   = THIS_MODULE,
#ifdef HAVE_UNLOCKED_IOCTL
	.unlocked_ioctl   = dma_buf_lock_ioctl,
#else
	.ioctl   = dma_buf_lock_ioctl,
#endif
	.compat_ioctl   = dma_buf_lock_ioctl,
};

typedef struct dma_buf_lock_resource
{
	int *list_of_dma_buf_fds;               /* List of buffers copied from userspace */
	atomic_t locked;                        /* Status of lock */
	struct dma_buf **dma_bufs;
	struct kds_resource **kds_resources;    /* List of KDS resources associated with buffers */
	struct kds_resource_set *resource_set;
	unsigned long exclusive;                /* Exclusive access bitmap */
	wait_queue_head_t wait;
	struct kds_callback cb;
	struct kref refcount;
	struct list_head link;
	int count;
} dma_buf_lock_resource;

static LIST_HEAD(dma_buf_lock_resource_list);
static DEFINE_MUTEX(dma_buf_lock_mutex);

static inline int is_dma_buf_lock_file(struct file *);
static void dma_buf_lock_dounlock(struct kref *ref);

static int dma_buf_lock_handle_release(struct inode *inode, struct file *file)
{
	dma_buf_lock_resource *resource;

	if (!is_dma_buf_lock_file(file))
		return -EINVAL;

	resource = file->private_data;
#if DMA_BUF_LOCK_DEBUG
	pr_debug("dma_buf_lock_handle_release\n");
#endif
	mutex_lock(&dma_buf_lock_mutex);
	kref_put(&resource->refcount, dma_buf_lock_dounlock);
	mutex_unlock(&dma_buf_lock_mutex);

	return 0;
}

static void dma_buf_lock_kds_callback(void *param1, void *param2)
{
	dma_buf_lock_resource *resource = param1;
#if DMA_BUF_LOCK_DEBUG
	pr_debug("dma_buf_lock_kds_callback\n");
#endif
	atomic_set(&resource->locked, 1);

	wake_up(&resource->wait);
}

static unsigned int dma_buf_lock_handle_poll(struct file *file,
                                             struct poll_table_struct *wait)
{
	dma_buf_lock_resource *resource;
	unsigned int ret = 0;

	if (!is_dma_buf_lock_file(file))
		return POLLERR;

	resource = file->private_data;
#if DMA_BUF_LOCK_DEBUG
	pr_debug("dma_buf_lock_handle_poll\n");
#endif
	if (1 == atomic_read(&resource->locked))
	{
		/* Resources have been locked */
		ret = POLLIN | POLLRDNORM;
		if (resource->exclusive)
		{
			ret |=  POLLOUT | POLLWRNORM;
		}
	}
	else
	{
		if (!poll_does_not_wait(wait)) 
		{
			poll_wait(file, &resource->wait, wait);
		}
	}
#if DMA_BUF_LOCK_DEBUG
	pr_debug("dma_buf_lock_handle_poll : return %i\n", ret);
#endif
	return ret;
}

static const struct file_operations dma_buf_lock_handle_fops = {
	.release	= dma_buf_lock_handle_release,
	.poll		= dma_buf_lock_handle_poll,
};

/*
 * is_dma_buf_lock_file - Check if struct file* is associated with dma_buf_lock
 */
static inline int is_dma_buf_lock_file(struct file *file)
{
	return file->f_op == &dma_buf_lock_handle_fops;
}



/*
 * Start requested lock.
 *
 * Allocates required memory, copies dma_buf_fd list from userspace,
 * acquires related KDS resources, and starts the lock.
 */
static int dma_buf_lock_dolock(dma_buf_lock_k_request *request)
{
	dma_buf_lock_resource *resource;
	int size;
	int fd;
	int i;
	int ret;

	if (NULL == request->list_of_dma_buf_fds)
	{
		return -EINVAL;
	}
	if (request->count <= 0)
	{
		return -EINVAL;
	}
	if (request->count > DMA_BUF_LOCK_BUF_MAX)
	{
		return -EINVAL;
	}
	if (request->exclusive != DMA_BUF_LOCK_NONEXCLUSIVE &&
	    request->exclusive != DMA_BUF_LOCK_EXCLUSIVE)
	{
		return -EINVAL;
	}

	resource = kzalloc(sizeof(dma_buf_lock_resource), GFP_KERNEL);
	if (NULL == resource)
	{
		return -ENOMEM;
	}

	atomic_set(&resource->locked, 0);
	kref_init(&resource->refcount);
	INIT_LIST_HEAD(&resource->link);
	resource->count = request->count;

	/* Allocate space to store dma_buf_fds received from user space */
	size = request->count * sizeof(int);
	resource->list_of_dma_buf_fds = kmalloc(size, GFP_KERNEL);

	if (NULL == resource->list_of_dma_buf_fds)
	{
		kfree(resource);
		return -ENOMEM;
	}

	/* Allocate space to store dma_buf pointers associated with dma_buf_fds */
	size = sizeof(struct dma_buf *) * request->count;
	resource->dma_bufs = kmalloc(size, GFP_KERNEL);

	if (NULL == resource->dma_bufs)
	{
		kfree(resource->list_of_dma_buf_fds);
		kfree(resource);
		return -ENOMEM;
	}
	/* Allocate space to store kds_resources associated with dma_buf_fds */
	size = sizeof(struct kds_resource *) * request->count;
	resource->kds_resources = kmalloc(size, GFP_KERNEL);

	if (NULL == resource->kds_resources)
	{
		kfree(resource->dma_bufs);
		kfree(resource->list_of_dma_buf_fds);
		kfree(resource);
		return -ENOMEM;
	}

	/* Copy requested list of dma_buf_fds from user space */
	size = request->count * sizeof(int);
	if (0 != copy_from_user(resource->list_of_dma_buf_fds, (void __user *)request->list_of_dma_buf_fds, size))
	{
		kfree(resource->list_of_dma_buf_fds);
		kfree(resource->dma_bufs);
		kfree(resource->kds_resources);
		kfree(resource);
		return -ENOMEM;
	}
#if DMA_BUF_LOCK_DEBUG
	for (i = 0; i < request->count; i++)
	{
		pr_debug("dma_buf %i = %X\n", i, resource->list_of_dma_buf_fds[i]);
	}
#endif

	/* Add resource to global list */
	mutex_lock(&dma_buf_lock_mutex);

	list_add(&resource->link, &dma_buf_lock_resource_list);

	mutex_unlock(&dma_buf_lock_mutex);

	for (i = 0; i < request->count; i++)
	{
		/* Convert fd into dma_buf structure */
		resource->dma_bufs[i] = dma_buf_get(resource->list_of_dma_buf_fds[i]);

		if (IS_ERR_VALUE(PTR_ERR(resource->dma_bufs[i])))
		{
			mutex_lock(&dma_buf_lock_mutex);
			kref_put(&resource->refcount, dma_buf_lock_dounlock);
			mutex_unlock(&dma_buf_lock_mutex);
			return -EINVAL;
		}

		/*Get kds_resource associated with dma_buf */
		resource->kds_resources[i] = get_dma_buf_kds_resource(resource->dma_bufs[i]);

		if (NULL == resource->kds_resources[i])
		{
			mutex_lock(&dma_buf_lock_mutex);
			kref_put(&resource->refcount, dma_buf_lock_dounlock);
			mutex_unlock(&dma_buf_lock_mutex);
			return -EINVAL;
		}
#if DMA_BUF_LOCK_DEBUG
		pr_debug("dma_buf_lock_dolock : dma_buf_fd %i dma_buf %X kds_resource %X\n", resource->list_of_dma_buf_fds[i],
		       (unsigned int)resource->dma_bufs[i], (unsigned int)resource->kds_resources[i]);
#endif	
	}

	kds_callback_init(&resource->cb, 1, dma_buf_lock_kds_callback);
	init_waitqueue_head(&resource->wait);

	kref_get(&resource->refcount);

	/* Create file descriptor associated with lock request */
	fd = anon_inode_getfd("dma_buf_lock", &dma_buf_lock_handle_fops, 
	                      (void *)resource, 0);
	if (fd < 0)
	{
		mutex_lock(&dma_buf_lock_mutex);
		kref_put(&resource->refcount, dma_buf_lock_dounlock);
		kref_put(&resource->refcount, dma_buf_lock_dounlock);
		mutex_unlock(&dma_buf_lock_mutex);
		return fd;
	}

	resource->exclusive = request->exclusive;

	/* Start locking process */
	ret = kds_async_waitall(&resource->resource_set,
	                        &resource->cb, resource, NULL,
	                        request->count,  &resource->exclusive,
	                        resource->kds_resources);

	if (IS_ERR_VALUE(ret))
	{
		put_unused_fd(fd);

		mutex_lock(&dma_buf_lock_mutex);
		kref_put(&resource->refcount, dma_buf_lock_dounlock);
		mutex_unlock(&dma_buf_lock_mutex);

		return ret;
	}

#if DMA_BUF_LOCK_DEBUG
	pr_debug("dma_buf_lock_dolock : complete\n");
#endif
	mutex_lock(&dma_buf_lock_mutex);
	kref_put(&resource->refcount, dma_buf_lock_dounlock);
	mutex_unlock(&dma_buf_lock_mutex);

	return fd;
}

static void dma_buf_lock_dounlock(struct kref *ref)
{
	int i;
	dma_buf_lock_resource *resource = container_of(ref, dma_buf_lock_resource, refcount);

	atomic_set(&resource->locked, 0);

	kds_callback_term(&resource->cb);

	kds_resource_set_release(&resource->resource_set);

	list_del(&resource->link);

	for (i = 0; i < resource->count; i++)
	{
		dma_buf_put(resource->dma_bufs[i]);
	}

	kfree(resource->kds_resources);
	kfree(resource->dma_bufs);
	kfree(resource->list_of_dma_buf_fds);
	kfree(resource);
}

static int __init dma_buf_lock_init(void)
{
	int err;
#if DMA_BUF_LOCK_DEBUG
	pr_debug("dma_buf_lock_init\n");
#endif
	err = alloc_chrdev_region(&dma_buf_lock_dev, 0, 1, dma_buf_lock_dev_name);

	if (0 == err)
	{
		cdev_init(&dma_buf_lock_cdev, &dma_buf_lock_fops);

		err = cdev_add(&dma_buf_lock_cdev, dma_buf_lock_dev, 1);

		if (0 == err)
		{
			dma_buf_lock_class = class_create(THIS_MODULE, dma_buf_lock_dev_name);
			if (IS_ERR(dma_buf_lock_class))
			{
				err = PTR_ERR(dma_buf_lock_class);
			}
			else
			{
				struct device *mdev;
				mdev = device_create(dma_buf_lock_class, NULL, dma_buf_lock_dev, NULL, dma_buf_lock_dev_name);
				if (!IS_ERR(mdev))
				{
					return 0;
				}

				err = PTR_ERR(mdev);
				class_destroy(dma_buf_lock_class);
			}
			cdev_del(&dma_buf_lock_cdev);
		}

		unregister_chrdev_region(dma_buf_lock_dev, 1);
	}
#if DMA_BUF_LOCK_DEBUG
	pr_debug("dma_buf_lock_init failed\n");
#endif
	return err;
}

static void __exit dma_buf_lock_exit(void)
{
#if DMA_BUF_LOCK_DEBUG
	pr_debug("dma_buf_lock_exit\n");
#endif

	/* Unlock all outstanding references */
	while (1)
	{
		mutex_lock(&dma_buf_lock_mutex);
		if (list_empty(&dma_buf_lock_resource_list))
		{
			mutex_unlock(&dma_buf_lock_mutex);
			break;
		}
		else
		{
			dma_buf_lock_resource *resource = list_entry(dma_buf_lock_resource_list.next, 
			                                             dma_buf_lock_resource, link);
			kref_put(&resource->refcount, dma_buf_lock_dounlock);
			mutex_unlock(&dma_buf_lock_mutex);
		}
	}

	device_destroy(dma_buf_lock_class, dma_buf_lock_dev);

	class_destroy(dma_buf_lock_class);

	cdev_del(&dma_buf_lock_cdev);

	unregister_chrdev_region(dma_buf_lock_dev, 1);
}

#ifdef HAVE_UNLOCKED_IOCTL
static long dma_buf_lock_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
#else
static int dma_buf_lock_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
#endif
{
	dma_buf_lock_k_request request;
	int size = _IOC_SIZE(cmd);

	if (_IOC_TYPE(cmd) != DMA_BUF_LOCK_IOC_MAGIC)
	{
		return -ENOTTY;

	}
	if ((_IOC_NR(cmd) < DMA_BUF_LOCK_IOC_MINNR) || (_IOC_NR(cmd) > DMA_BUF_LOCK_IOC_MAXNR))
	{
		return -ENOTTY;
	}

	switch (cmd)
	{
		case DMA_BUF_LOCK_FUNC_LOCK_ASYNC:
			if (size != sizeof(dma_buf_lock_k_request))
			{
				return -ENOTTY;
			}
			if (copy_from_user(&request, (void __user *)arg, size))
			{
				return -EFAULT;
			}
#if DMA_BUF_LOCK_DEBUG
			pr_debug("DMA_BUF_LOCK_FUNC_LOCK_ASYNC - %i\n", request.count);
#endif
			return dma_buf_lock_dolock(&request);
	}

	return -ENOTTY;
}

module_init(dma_buf_lock_init);
module_exit(dma_buf_lock_exit);

MODULE_LICENSE("GPL");

