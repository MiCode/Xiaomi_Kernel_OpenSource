/*
 * drivers/char/tegra_gmi_access.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/rwlock_types.h>
#include <linux/io.h>
#include <linux/tegra_gmi_access.h>
#include <linux/proc_fs.h>
#include <linux/module.h>
#include <mach/iomap.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/module.h>
/*Data Structures*/
#define GMI_MAX_PRIO 4
#define GMI_MAX_DEVICES 6
#define GMI_HANDLE_SIGNATURE 0xABABABAB

static int gmi_init(void);

struct gmi_device {
	u32 signature;
	char *dev_name;
	u32 priority;
	u32 dev_index;
};

/* Async Request Queue Access API's */
struct gmi_async_req_queue_head_t {
	spinlock_t lock;
	struct list_head async_req_list;
};

struct gmi_async_req {
	struct list_head async_req_list;
	void *priv_data;
	gasync_callbackp cb;
};

struct gmi_driver {
	struct gmi_device *gdev_list[GMI_MAX_DEVICES];
	wait_queue_head_t waitq[GMI_MAX_PRIO];
	/* single queue and is the highest prio*/
	struct gmi_async_req_queue_head_t asyncq;
	/* Thread to service async pending requests */
	struct task_struct *athread;
	struct semaphore athread_lock;
	u32 cur_index;
	struct semaphore gbus_lock;
	struct semaphore qprot;
};

struct gmi_driver *gdrv;

static void enqueue_request(struct gmi_async_req_queue_head_t *q,
				struct gmi_async_req *new)
{
	unsigned long flags;
	spin_lock_irqsave(&q->lock, flags);
	list_add_tail(&new->async_req_list, &q->async_req_list);
	spin_unlock_irqrestore(&q->lock, flags);
}

static struct gmi_async_req *dequeue_request(
				struct gmi_async_req_queue_head_t *q)
{
	struct gmi_async_req *gasync_req = NULL;
	unsigned long flags;
	spin_lock_irqsave(&q->lock, flags);
	if (!list_empty(&q->async_req_list)) {
		gasync_req = list_first_entry(&q->async_req_list,
					struct gmi_async_req, async_req_list);
	list_del(&gasync_req->async_req_list);
	}
	spin_unlock_irqrestore(&q->lock, flags);

	return gasync_req;
}

static int request_pending(struct gmi_async_req_queue_head_t *q)
{

	unsigned long flags;
	int ret;
	spin_lock_irqsave(&q->lock, flags);
	ret = list_empty(&q->async_req_list);
	spin_unlock_irqrestore(&q->lock, flags);
	return !ret;
}


static int async_handler_thread(void *data)
{
	while (1) {
		struct gmi_async_req *areq;

		/* Blocked on lock, sleep */
		if (down_interruptible(&gdrv->athread_lock))
			continue;
		/* When we reach here bus lock is already taken */
		BUG_ON(!down_trylock(&gdrv->gbus_lock));
		areq = dequeue_request(&gdrv->asyncq);
		if (!areq) {
			release_gmi_access();
			continue;
		}
		areq->cb(areq->priv_data);
		kfree(areq);
		release_gmi_access();
	}
	/* unreachable */
	return 0;
}

int enqueue_gmi_async_request(u32 gdev_handle, gasync_callbackp cb, void *pdata)
{
	struct gmi_async_req *areq;

	if ((*(u32 *)gdev_handle) != GMI_HANDLE_SIGNATURE) {
		printk(KERN_ERR"\n Invalid Handle ");
		return -1;
	}

	if (cb == NULL)
		return -1;
	if (!gdrv)
		BUG();

	areq = kmalloc(sizeof(struct gmi_async_req), GFP_ATOMIC);
	areq->cb = cb;
	areq->priv_data = pdata;
	INIT_LIST_HEAD(&areq->async_req_list);
	enqueue_request(&gdrv->asyncq, areq);

	/* Unblock the thread bus lock granted */
	if (!down_trylock(&gdrv->gbus_lock))
		up(&gdrv->athread_lock);

	return 0;
}

int request_gmi_access(u32 gdev_handle)
{
	struct gmi_device *gdev = (struct gmi_device *)gdev_handle;

	if ((*(u32 *)gdev_handle) != GMI_HANDLE_SIGNATURE) {
		printk("\n Invalid Handle ");
		return -1;
	}
	if (!gdrv || (gdev->priority >= GMI_MAX_PRIO) ||
				(gdev->dev_index >= GMI_MAX_DEVICES))
		BUG();

	down(&gdrv->qprot);

	if (down_trylock(&gdrv->gbus_lock)) {
		/* Bus already in use so block this request on the waitQueue */
		DEFINE_WAIT(wait_entry);
		prepare_to_wait_exclusive(&gdrv->waitq[gdev->priority],
					&wait_entry, TASK_UNINTERRUPTIBLE);
		up(&gdrv->qprot);

		schedule();

		finish_wait(&gdrv->waitq[gdev->priority], &wait_entry);

		/* When we reach here we would have the bus lock acquired */
		BUG_ON(!down_trylock(&gdrv->gbus_lock));
		return 0;
	} else{
		up(&gdrv->qprot);
		return 0; /* Got hold of the bus*/
	}
}
EXPORT_SYMBOL(request_gmi_access);

void release_gmi_access(void)
{
	int i;
	/* Check if any async request is pending serve it
		and unblock the thread*/
	if (request_pending(&gdrv->asyncq)) {
		up(&gdrv->athread_lock);
		return;
	}
	down(&gdrv->qprot);
	/* Wakeup one task the highest priority available */
	for (i = 0; i < GMI_MAX_PRIO; i++) {
		if (waitqueue_active(&gdrv->waitq[i])) {
			up(&gdrv->qprot);
			wake_up(&gdrv->waitq[i]);
			return;
		}
	}

	/*No task Waiting release the bus_lock */
	up(&gdrv->gbus_lock);
	up(&gdrv->qprot);
}
EXPORT_SYMBOL(release_gmi_access);

/* More than one device can register at same priority */
u32 register_gmi_device(const char *devName, u32 priority)
{
	struct gmi_device *gdev = NULL;

	if (!gdrv)
		gmi_init();

	down(&gdrv->qprot);

	if (!gdrv || (priority >= GMI_MAX_PRIO) ||
				(gdrv->cur_index >= GMI_MAX_DEVICES)) {
		up(&gdrv->qprot);
		return (u32)NULL;
	}

	gdev = kzalloc(sizeof(struct gmi_device), GFP_KERNEL);

	gdev->signature = GMI_HANDLE_SIGNATURE;
	gdev->priority = priority;
	/* Init devName :pending*/
	gdev->dev_index = gdrv->cur_index;
	gdrv->gdev_list[gdrv->cur_index++] = gdev;

	up(&gdrv->qprot);
	return (u32)gdev;
}
EXPORT_SYMBOL(register_gmi_device);

static int gmi_init(void)
{
	int i;
	gdrv = kzalloc(sizeof(*gdrv), GFP_KERNEL);
	sema_init(&gdrv->gbus_lock, 1);
	sema_init(&gdrv->athread_lock, 1);
	down(&gdrv->athread_lock);
	sema_init(&gdrv->qprot, 1);
	spin_lock_init(&(gdrv->asyncq.lock));

	INIT_LIST_HEAD(&gdrv->asyncq.async_req_list);
	for (i = 0; i < GMI_MAX_PRIO; i++)
		init_waitqueue_head(&gdrv->waitq[i]);

	gdrv->athread = kthread_create(async_handler_thread, 0 , "Athread");
	wake_up_process(gdrv->athread);

	return 0;
}

MODULE_AUTHOR("Nitin Sehgal <nsehgal@nvidia.com>");
MODULE_DESCRIPTION("Tegra GMI bus access control api's");
MODULE_LICENSE("GPL");
