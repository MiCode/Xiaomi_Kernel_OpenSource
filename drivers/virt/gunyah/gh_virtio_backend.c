// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/file.h>
#include <linux/workqueue.h>
#include <linux/eventfd.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/gh_virtio_backend.h>
#include <linux/of_irq.h>
#include <uapi/linux/virtio_mmio.h>
#include <linux/gunyah/gh_rm_drv.h>
#include <linux/pgtable.h>
#include <soc/qcom/secure_buffer.h>
#include <dt-bindings/interrupt-controller/arm-gic.h>
#include "hcall_virtio.h"
#include "gh_virtio_backend.h"

#define CREATE_TRACE_POINTS
#include <trace/events/gh_virtio_backend.h>
#undef CREATE_TRACE_POINTS

#define MAX_DEVICE_NAME		32
#define MAX_VM_NAME		32
#define MAX_CDEV_NAME		64
#define MAX_VM_DEVICES		32
#define VIRTIO_BE_CLASS		"gh_virtio_backend"
#define MAX_QUEUES		4
#define MAX_IO_CONTEXTS		MAX_QUEUES

#define VIRTIO_PRINT_MARKER	"gh_virtio_backend"

#define assert_virq		gh_hcall_virtio_mmio_backend_assert_virq
#define set_dev_features	gh_hcall_virtio_mmio_backend_set_dev_features
#define set_queue_num_max	gh_hcall_virtio_mmio_backend_set_queue_num_max
#define get_drv_features	gh_hcall_virtio_mmio_backend_get_drv_features
#define get_queue_info		gh_hcall_virtio_mmio_backend_get_queue_info
#define get_event		gh_hcall_virtio_mmio_backend_get_event
#define ack_reset		gh_hcall_virtio_mmio_backend_ack_reset

static DEFINE_MUTEX(vm_mutex);
static DEFINE_IDA(vm_minor_id);
static DEFINE_SPINLOCK(vm_list_lock);
static LIST_HEAD(vm_list);
static struct class *vb_dev_class;
static dev_t vbe_dev;

enum {
	VM_STATE_NOT_ACTIVE,
	VM_STATE_ACTIVE,
};

struct shared_memory {
	struct resource r;
	u32 gunyah_label, shm_memparcel;
	/* Remove once API transition complete */
	bool has_lookup_sgl;
};

struct virt_machine {
	struct list_head list;
	char vm_name[MAX_VM_NAME];
	char cdev_name[MAX_CDEV_NAME];
	int minor;
	int open_count;
	int hyp_assign_done;
	struct cdev cdev;
	struct device *class_dev;
	struct shared_memory *shmem;
	int shmem_entries;
	spinlock_t vb_dev_lock;
	struct mutex mutex;
	struct list_head vb_dev_list;
	phys_addr_t shmem_addr;
	u64 shmem_size;
	wait_queue_head_t app_ready_queue;
	int state;
	int app_ready;
	int waiting_for_app_ready;
};

struct ioevent_context {
	int fd;
	struct eventfd_ctx *ctx;
};

struct irq_context {
	struct eventfd_ctx *ctx;
	wait_queue_entry_t wait;
	poll_table pt;
	struct fd fd;
	struct work_struct shutdown_work;
};

struct virtio_backend_device {
	struct list_head list;
	spinlock_t lock;
	wait_queue_head_t evt_queue;
	wait_queue_head_t notify_queue;
	u32 refcount;
	int notify;
	int ack_driver_ok;
	int evt_avail;
	u64 cur_event_data, vdev_event_data;
	u64 cur_event, vdev_event;
	int linux_irq;
	u32 label;
	struct device *dev;
	struct virt_machine *vm;
	struct ioevent_context ioctx[MAX_IO_CONTEXTS];
	struct irq_context irq;
	char int_name[32];
	u32 features[2];
	u32 queue_num_max[MAX_QUEUES];
	struct mutex mutex;
	gh_capid_t cap_id;
	/* Backend program supplied config data */
	char *config_data;
	u32 config_size;
	/* Page shared with frontend */
	char __iomem *config_shared_buf;
	u64  config_shared_size;
};

static struct virtio_backend_device *
vb_dev_get(struct virt_machine *vm, u32 label)
{
	struct virtio_backend_device *vb_dev = NULL, *tmp;

	spin_lock(&vm->vb_dev_lock);
	if (vm->state != VM_STATE_ACTIVE)
		goto done;
	list_for_each_entry(tmp, &vm->vb_dev_list, list) {
		if (label == tmp->label) {
			if (tmp->refcount < U32_MAX)
				vb_dev = tmp;
			break;
		}
	}
	if (vb_dev)
		vb_dev->refcount++;

done:
	spin_unlock(&vm->vb_dev_lock);

	return vb_dev;
}

static void vb_dev_put(struct virtio_backend_device *vb_dev)
{
	struct virt_machine *vm = vb_dev->vm;

	spin_lock(&vm->vb_dev_lock);
	vb_dev->refcount--;
	if (!vb_dev->refcount && vb_dev->notify)
		wake_up(&vb_dev->notify_queue);
	spin_unlock(&vm->vb_dev_lock);
}

static void
irqfd_shutdown(struct work_struct *work)
{
	struct irq_context *irq = container_of(work,
				struct irq_context, shutdown_work);
	struct virtio_backend_device *vb_dev;
	unsigned long iflags;
	u64 isr;

	vb_dev = container_of(irq, struct virtio_backend_device, irq);

	spin_lock_irqsave(&vb_dev->lock, iflags);
	if (irq->ctx) {
		eventfd_ctx_remove_wait_queue(irq->ctx, &irq->wait, &isr);
		eventfd_ctx_put(irq->ctx);
		fdput(irq->fd);
		irq->ctx = NULL;
		irq->fd.file = NULL;
	}
	spin_unlock_irqrestore(&vb_dev->lock, iflags);
}

static int vb_dev_irqfd_wakeup(wait_queue_entry_t *wait, unsigned int mode,
							int sync, void *key)
{
	struct irq_context *irq = container_of(wait, struct irq_context, wait);
	struct virtio_backend_device *vb_dev;
	__poll_t flags = key_to_poll(key);

	vb_dev = container_of(irq, struct virtio_backend_device, irq);

	if (flags & EPOLLIN) {
		int rc = assert_virq(vb_dev->cap_id, 1);

		trace_gh_virtio_backend_irq_inj(vb_dev->label, rc);
	}

	if (flags & EPOLLHUP)
		queue_work(system_wq, &irq->shutdown_work);

	return 0;
}

static void vb_dev_irqfd_ptable_queue_proc(struct file *file,
			wait_queue_head_t *wqh,	poll_table *pt)
{
	struct irq_context *irq = container_of(pt, struct irq_context, pt);

	add_wait_queue(wqh, &irq->wait);
}

static int vb_dev_irqfd(struct virtio_backend_device *vb_dev,
		struct virtio_irqfd *ifd)
{
	struct fd f;
	int ret = -EBUSY;
	struct eventfd_ctx *eventfd = NULL;
	__poll_t events;
	unsigned long flags;

	f.file = NULL;

	spin_lock_irqsave(&vb_dev->lock, flags);

	if (vb_dev->irq.fd.file)
		goto fail;

	f = fdget(ifd->fd);
	if (!f.file)
		goto fail;

	eventfd = eventfd_ctx_fileget(f.file);
	if (IS_ERR(eventfd)) {
		ret = PTR_ERR(eventfd);
		goto fail;
	}
	vb_dev->irq.ctx = eventfd;
	vb_dev->irq.fd = f;

	spin_unlock_irqrestore(&vb_dev->lock, flags);

	init_waitqueue_func_entry(&vb_dev->irq.wait, vb_dev_irqfd_wakeup);
	INIT_WORK(&vb_dev->irq.shutdown_work, irqfd_shutdown);
	init_poll_funcptr(&vb_dev->irq.pt, vb_dev_irqfd_ptable_queue_proc);
	events = vfs_poll(f.file, &vb_dev->irq.pt);
	if (events & EPOLLIN)
		pr_err("%s: Premature injection of interrupt\n", VIRTIO_PRINT_MARKER);

	return 0;

fail:
	if (eventfd && !IS_ERR(eventfd))
		eventfd_ctx_put(eventfd);
	if (f.file)
		fdput(f);

	spin_unlock_irqrestore(&vb_dev->lock, flags);

	return ret;
}

static int vb_dev_ioeventfd(struct virtio_backend_device *vb_dev,
				struct virtio_eventfd *efd)
{
	struct eventfd_ctx *ctx = NULL;
	int i = efd->queue_num;
	unsigned long flags;
	int ret = -EBUSY;

	if (efd->fd <= 0)
		return -EINVAL;

	spin_lock_irqsave(&vb_dev->lock, flags);
	if (vb_dev->ioctx[i].fd > 0)
		goto out;

	ctx = eventfd_ctx_fdget(efd->fd);
	if (IS_ERR(ctx)) {
		ret = PTR_ERR(ctx);
		goto out;
	}

	vb_dev->ioctx[i].ctx = ctx;
	vb_dev->ioctx[i].fd = efd->fd;

	ret = 0;

out:
	spin_unlock_irqrestore(&vb_dev->lock, flags);

	return ret;
}

static void signal_vqs(struct virtio_backend_device *vb_dev)
{
	int i;
	u64 flags;

	for (i = 0; i < MAX_IO_CONTEXTS; ++i) {
		flags = 1 << i;
		if ((vb_dev->vdev_event_data & flags) && vb_dev->ioctx[i].ctx) {
			eventfd_signal(vb_dev->ioctx[i].ctx, 1);
			vb_dev->vdev_event_data &= ~flags;
			trace_gh_virtio_backend_queue_notify(vb_dev->label, i);
		}
	}
}

static long virtio_backend_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	struct virt_machine *vm = file->private_data;
	struct virtio_backend_device *vb_dev;
	void __user *argp = (void __user *)arg;
	struct virtio_eventfd efd;
	struct virtio_irqfd ifd;
	struct virtio_dev_features f;
	struct virtio_queue_max q;
	struct virtio_ack_reset r;
	struct virtio_config_data d;
	struct virtio_queue_info qi;
	struct gh_hcall_virtio_queue_info qinfo;
	struct virtio_driver_features df;
	struct virtio_event ve;
	u64 features;
	u32 label;
	int ret = 0, i;
	unsigned long flags;
	u64 org_event, org_data;
	u32 *p, *ack_reg;

	if (!vm)
		return -EINVAL;

	switch (cmd) {
	case GH_SET_APP_READY:
		spin_lock(&vm->vb_dev_lock);
		vm->app_ready = 1;
		if (vm->waiting_for_app_ready)
			wake_up_interruptible(&vm->app_ready_queue);
		spin_unlock(&vm->vb_dev_lock);
		pr_debug("%s: App is ready!!\n", VIRTIO_PRINT_MARKER);
		break;

	case GH_GET_SHARED_MEMORY_SIZE:
		if (copy_to_user(argp, &vm->shmem_size,
					sizeof(vm->shmem_size)))
			return -EFAULT;
		break;

	case GH_IOEVENTFD:
		if (copy_from_user(&efd, argp, sizeof(efd)))
			return -EFAULT;

		if (efd.queue_num >= MAX_IO_CONTEXTS)
			return -EINVAL;

		if (efd.flags != VBE_ASSIGN_IOEVENTFD)
			return -EOPNOTSUPP;

		vb_dev = vb_dev_get(vm, efd.label);
		if (!vb_dev)
			return -EINVAL;

		ret = vb_dev_ioeventfd(vb_dev, &efd);

		vb_dev_put(vb_dev);

		return ret;

	case GH_IRQFD:
		if (copy_from_user(&ifd, argp, sizeof(ifd)))
			return -EFAULT;

		if (!ifd.label || ifd.fd <= 0)
			return -EINVAL;

		if (ifd.flags != VBE_ASSIGN_IRQFD)
			return -EOPNOTSUPP;

		vb_dev = vb_dev_get(vm, ifd.label);
		if (!vb_dev)
			return -EINVAL;

		ret = vb_dev_irqfd(vb_dev, &ifd);

		vb_dev_put(vb_dev);

		return ret;

	case GH_WAIT_FOR_EVENT:
		if (copy_from_user(&ve, argp, sizeof(ve)))
			return -EFAULT;

		if (!ve.label)
			return -EINVAL;

		vb_dev = vb_dev_get(vm, ve.label);
		if (!vb_dev)
			return -EINVAL;

loop_back:
		if (!vb_dev->evt_avail) {
			ret = wait_event_interruptible(vb_dev->evt_queue,
					vb_dev->evt_avail);
			if (ret) {
				vb_dev_put(vb_dev);
				return ret;
			}
		}

		spin_lock_irqsave(&vb_dev->lock, flags);

		vb_dev->cur_event = 0;
		vb_dev->cur_event_data = 0;
		vb_dev->evt_avail = 0;
		ack_reg = (u32 *)(vb_dev->config_shared_buf + VIRTIO_MMIO_INTERRUPT_ACK);

		org_event = vb_dev->vdev_event;
		org_data = vb_dev->vdev_event_data;

		if (vb_dev->vdev_event & (EVENT_MODULE_EXIT | EVENT_VM_EXIT))
			vb_dev->cur_event = EVENT_APP_EXIT;
		else if (vb_dev->vdev_event & EVENT_RESET_RQST) {
			vb_dev->vdev_event &= ~EVENT_RESET_RQST;
			vb_dev->cur_event = EVENT_RESET_RQST;
			if (vb_dev->vdev_event)
				vb_dev->evt_avail = 1;
		} else if (vb_dev->vdev_event & EVENT_DRIVER_OK) {
			vb_dev->vdev_event &= ~EVENT_DRIVER_OK;
			if (!vb_dev->ack_driver_ok) {
				vb_dev->cur_event = EVENT_DRIVER_OK;
				if (vb_dev->vdev_event)
					vb_dev->evt_avail = 1;
			}
		} else if (vb_dev->vdev_event & EVENT_NEW_BUFFER) {
			vb_dev->vdev_event &= ~EVENT_NEW_BUFFER;
			if (vb_dev->vdev_event_data && vb_dev->ack_driver_ok)
				signal_vqs(vb_dev);
			if (vb_dev->vdev_event) {
				vb_dev->cur_event = vb_dev->vdev_event;
				vb_dev->vdev_event = 0;
				vb_dev->cur_event_data = vb_dev->vdev_event_data;
				vb_dev->vdev_event_data = 0;
				if (vb_dev->cur_event & EVENT_INTERRUPT_ACK)
					vb_dev->cur_event_data = readl_relaxed(ack_reg);
			}
		} else if (vb_dev->vdev_event & EVENT_INTERRUPT_ACK) {
			vb_dev->vdev_event &= ~EVENT_INTERRUPT_ACK;
			vb_dev->vdev_event_data = 0;
			vb_dev->cur_event = EVENT_INTERRUPT_ACK;
			vb_dev->cur_event_data = readl_relaxed(ack_reg);
		}

		spin_unlock_irqrestore(&vb_dev->lock, flags);

		trace_gh_virtio_backend_wait_event(vb_dev->label, vb_dev->cur_event,
				org_event, vb_dev->cur_event_data, org_data);

		if (!vb_dev->cur_event)
			goto loop_back;

		ve.event = (u32) vb_dev->cur_event;
		ve.event_data = (u32) vb_dev->cur_event_data;

		vb_dev_put(vb_dev);

		if (copy_to_user(argp, &ve, sizeof(ve)))
			return -EFAULT;

		break;

	case GH_GET_DRIVER_FEATURES:
		if (copy_from_user(&df, argp, sizeof(df)))
			return -EFAULT;

		if (!df.label || df.features_sel > 1)
			return -EINVAL;

		vb_dev = vb_dev_get(vm, df.label);
		if (!vb_dev)
			return -EINVAL;

		if (!vb_dev->cap_id) {
			vb_dev_put(vb_dev);
			return -EINVAL;
		}

		ret = get_drv_features(vb_dev->cap_id, df.features_sel, &features);

		vb_dev_put(vb_dev);

		pr_debug("%s: get_drv_feat %d/%x ret %d\n",
				VIRTIO_PRINT_MARKER, df.features_sel, features, ret);
		if (ret)
			return ret;

		df.features = (u32) features;
		if (copy_to_user(argp, &df, sizeof(df)))
			return -EFAULT;

		break;

	case GH_GET_QUEUE_INFO:
		if (copy_from_user(&qi, argp, sizeof(qi)))
			return -EFAULT;

		if (!qi.label || qi.queue_sel >= MAX_QUEUES)
			return -EINVAL;

		vb_dev = vb_dev_get(vm, qi.label);
		if (!vb_dev)
			return -EINVAL;

		if (!vb_dev->cap_id) {
			vb_dev_put(vb_dev);
			return -EINVAL;
		}

		ret = get_queue_info(vb_dev->cap_id, qi.queue_sel, &qinfo);
		vb_dev_put(vb_dev);

		pr_debug("%s: get_queue_info %d: que_num %d que_ready %d que_desc %llx\n",
			VIRTIO_PRINT_MARKER, qi.queue_sel, qinfo.queue_num,
			qinfo.queue_ready, qinfo.queue_desc);
		pr_debug("%s: que_driver %llx que_device %llx ret %d\n",
			VIRTIO_PRINT_MARKER, qinfo.queue_driver, qinfo.queue_device, ret);

		if (ret)
			return ret;

		qi.queue_num = (u32) qinfo.queue_num;
		qi.queue_ready = (u32) qinfo.queue_ready;
		qi.queue_desc = qinfo.queue_desc;
		qi.queue_driver = qinfo.queue_driver;
		qi.queue_device = qinfo.queue_device;

		if (copy_to_user(argp, &qi, sizeof(qi)))
			return -EFAULT;

		break;

	case GH_ACK_DRIVER_OK:
		label = (u32) arg;

		if (!label)
			return -EINVAL;

		vb_dev = vb_dev_get(vm, label);
		if (!vb_dev)
			return -EINVAL;

		vb_dev->ack_driver_ok = 1;
		vb_dev_put(vb_dev);
		pr_debug("%s: ack_driver_ok for label %x!\n", VIRTIO_PRINT_MARKER, label);

		break;

	case GH_ACK_RESET:
		if (copy_from_user(&r, argp, sizeof(r)))
			return -EFAULT;

		if (!r.label)
			return -EINVAL;

		vb_dev = vb_dev_get(vm, r.label);
		if (!vb_dev || !vb_dev->cap_id)
			return -EINVAL;

		ack_reset(vb_dev->cap_id);
		vb_dev_put(vb_dev);

		pr_debug("%s: ack_reset for label %x!\n", VIRTIO_PRINT_MARKER, r.label);
		break;

	case GH_SET_DEVICE_FEATURES:
		if (copy_from_user(&f, argp, sizeof(f)))
			return -EFAULT;

		if (!f.label || f.features_sel > 1)
			return -EINVAL;

		vb_dev = vb_dev_get(vm, f.label);
		if (!vb_dev)
			return -EINVAL;

		vb_dev->features[f.features_sel] = f.features;
		vb_dev_put(vb_dev);

		pr_debug("%s: label %d features %d %x\n", VIRTIO_PRINT_MARKER,
				f.label, f.features_sel, f.features);
		break;

	case GH_SET_QUEUE_NUM_MAX:
		if (copy_from_user(&q, argp, sizeof(q)))
			return -EFAULT;

		if (!q.label || q.queue_sel >= MAX_QUEUES)
			return -EINVAL;

		vb_dev = vb_dev_get(vm, q.label);
		if (!vb_dev)
			return -EINVAL;

		vb_dev->queue_num_max[q.queue_sel] = q.queue_num_max;

		vb_dev_put(vb_dev);

		pr_debug("%s: label %d queue_max %d %x\n", VIRTIO_PRINT_MARKER,
				q.label, q.queue_sel, q.queue_num_max);

		break;

	case GH_GET_DRIVER_CONFIG_DATA:
		if (copy_from_user(&d, argp, sizeof(d)))
			return -EFAULT;

		if (!d.label || !d.config_size || !d.config_data)
			return -EINVAL;

		vb_dev = vb_dev_get(vm, d.label);
		if (!vb_dev)
			return -EINVAL;

		mutex_lock(&vb_dev->mutex);
		if (!vb_dev->config_shared_buf) {
			mutex_unlock(&vb_dev->mutex);
			vb_dev_put(vb_dev);
			return -EINVAL;
		}
		ret = copy_to_user((char __user *)d.config_data,
			vb_dev->config_shared_buf, d.config_size);
		mutex_unlock(&vb_dev->mutex);
		vb_dev_put(vb_dev);
		return ret;

	case GH_SET_DEVICE_CONFIG_DATA:
		if (copy_from_user(&d, argp, sizeof(d)))
			return -EFAULT;

		if (!d.label || d.config_size > PAGE_SIZE ||
			!d.config_size || !d.config_data)
			return -EINVAL;

		vb_dev = vb_dev_get(vm, d.label);
		if (!vb_dev)
			return -EINVAL;

		mutex_lock(&vb_dev->mutex);
		if (vb_dev->config_data) {
			mutex_unlock(&vb_dev->mutex);
			vb_dev_put(vb_dev);
			return -EBUSY;
		}

		vb_dev->config_data = (char *)__get_free_pages(
					GFP_KERNEL | __GFP_ZERO, 0);
		if (!vb_dev->config_data) {
			mutex_unlock(&vb_dev->mutex);
			vb_dev_put(vb_dev);
			return -ENOMEM;
		}

		if (copy_from_user(vb_dev->config_data,
				u64_to_user_ptr(d.config_data), d.config_size)) {
			free_pages((unsigned long)vb_dev->config_data, 0);
			vb_dev->config_data = NULL;
			mutex_unlock(&vb_dev->mutex);
			vb_dev_put(vb_dev);
			return -EFAULT;
		}
		vb_dev->config_size = d.config_size;
		mutex_unlock(&vb_dev->mutex);
		vb_dev_put(vb_dev);
		p = (u32 *)vb_dev->config_data;
		for (i = 0; i < 4; ++i)
			pr_debug("%s: %x: %x\n", VIRTIO_PRINT_MARKER, i*4, p[i]);
		break;

	default:
		pr_err("%s: cmd %x not supported\n", VIRTIO_PRINT_MARKER, cmd);
		return -EINVAL;
	}

	return 0;
}

static int virtio_backend_mmap(struct file *file,
				struct vm_area_struct *vma)
{
	struct virt_machine *vm = file->private_data;
	size_t mmap_size;

	if (!vm)
		return -EINVAL;

	mmap_size = vma->vm_end - vma->vm_start;
	if (mmap_size != vm->shmem_size)
		return -EINVAL;

	vma->vm_flags = vma->vm_flags | VM_DONTEXPAND | VM_DONTDUMP;

	if (io_remap_pfn_range(vma, vma->vm_start,
			__phys_to_pfn(vm->shmem_addr),
			mmap_size, vma->vm_page_prot)) {
		pr_err("%s: ioremap_pfn_range failed\n", VIRTIO_PRINT_MARKER);
		return -EAGAIN;
	}

	return 0;
}

static void init_vb_dev_open(struct virtio_backend_device *vb_dev)
{
	unsigned long flags;

	spin_lock_irqsave(&vb_dev->lock, flags);
	vb_dev->evt_avail = 0;
	vb_dev->cur_event_data = 0;
	vb_dev->vdev_event_data = 0;
	vb_dev->cur_event = 0;
	vb_dev->vdev_event = 0;
	vb_dev->notify = 0;
	spin_unlock_irqrestore(&vb_dev->lock, flags);
}

static int virtio_backend_open(struct inode *inode, struct file *filp)
{
	unsigned int minor = iminor(inode);
	struct virt_machine *vm = NULL, *tmp;
	struct virtio_backend_device *vb_dev;
	int count = 1;

	spin_lock(&vm_list_lock);
	list_for_each_entry(tmp, &vm_list, list) {
		if (tmp->minor != minor)
			continue;

		vm = tmp;
		break;
	}
	if (vm && !vm->open_count) {
		++vm->open_count;
		count = 0;
	}
	spin_unlock(&vm_list_lock);

	if (!vm) {
		pr_err("%s: VM with minor id %d not found\n",
				VIRTIO_PRINT_MARKER, minor);
		return -EINVAL;
	}

	if (count)
		return -EBUSY;

	spin_lock(&vm->vb_dev_lock);
	list_for_each_entry(vb_dev, &vm->vb_dev_list, list)
		init_vb_dev_open(vb_dev);

	vm->state = VM_STATE_ACTIVE;
	vm->app_ready = 0;
	spin_unlock(&vm->vb_dev_lock);

	filp->private_data = vm;

	return 0;
}

static void close_vb_dev(struct virtio_backend_device *vb_dev)
{
	unsigned long flags;
	int i;

	spin_lock_irqsave(&vb_dev->lock, flags);
	if (vb_dev->irq.ctx) {
		eventfd_ctx_put(vb_dev->irq.ctx);
		fdput(vb_dev->irq.fd);
		vb_dev->irq.ctx = NULL;
		vb_dev->irq.fd.file = NULL;
	}

	for (i = 0; i < MAX_IO_CONTEXTS; ++i) {
		if (vb_dev->ioctx[i].ctx) {
			eventfd_ctx_put(vb_dev->ioctx[i].ctx);
			vb_dev->ioctx[i].ctx = NULL;
			vb_dev->ioctx[i].fd = 0;
		}
	}

	vb_dev->evt_avail = 0;
	vb_dev->vdev_event = 0;
	vb_dev->vdev_event_data = 0;
	vb_dev->ack_driver_ok = 0;
	spin_unlock_irqrestore(&vb_dev->lock, flags);

	if (vb_dev->config_data) {
		free_pages((unsigned long)vb_dev->config_data, 0);
		vb_dev->config_data = NULL;
	}
}

static int virtio_backend_release(struct inode *inode, struct file *filp)
{
	struct virt_machine *vm = filp->private_data;
	struct virtio_backend_device *vb_dev = NULL;

	if (!vm)
		return 0;

	spin_lock(&vm->vb_dev_lock);
	vm->state = VM_STATE_NOT_ACTIVE;
	list_for_each_entry(vb_dev, &vm->vb_dev_list, list)
		close_vb_dev(vb_dev);
	spin_unlock(&vm->vb_dev_lock);

	spin_lock(&vm_list_lock);
	vm->open_count--;
	vm->app_ready = 0;
	spin_unlock(&vm_list_lock);

	return 0;
}

static const struct file_operations virtio_backend_fops = {
	.owner		= THIS_MODULE,
	.open		= virtio_backend_open,
	.mmap		= virtio_backend_mmap,
	.unlocked_ioctl	= virtio_backend_ioctl,
	.release	= virtio_backend_release,
};

static int vb_devclass_init(void)
{
	int ret;

	vb_dev_class = class_create(THIS_MODULE, VIRTIO_BE_CLASS);
	if (IS_ERR(vb_dev_class))
		return PTR_ERR(vb_dev_class);

	ret = alloc_chrdev_region(&vbe_dev, 0, MAX_VM_DEVICES, VIRTIO_BE_CLASS);
	if (ret < 0) {
		pr_err("%s: unable to allocate major number\n",
						VIRTIO_PRINT_MARKER);
		class_destroy(vb_dev_class);
		return ret;
	}

	return 0;
}

static void vb_devclass_deinit(void)
{
	if (vb_dev_class) {
		class_destroy(vb_dev_class);
		vb_dev_class = NULL;
	}
	if (vbe_dev) {
		unregister_chrdev_region(vbe_dev, MAX_VM_DEVICES);
		vbe_dev = 0;
	}
}

static int vm_devnode_init(struct virt_machine *vm)
{
	int ret;

	vm->minor = ida_simple_get(&vm_minor_id, 0, MAX_VM_DEVICES, GFP_KERNEL);
	if (vm->minor < 0) {
		pr_err("%s: No more minor numbers left err %d\n",
				VIRTIO_PRINT_MARKER, vm->minor);
		return -ENODEV;
	}

	vm->class_dev = device_create(vb_dev_class, NULL,
			MKDEV(MAJOR(vbe_dev), vm->minor), NULL, vm->cdev_name);
	if (IS_ERR(vm->class_dev)) {
		ret = PTR_ERR(vm->class_dev);
		pr_err("%s: device_create failed for %s ret %d\n",
			VIRTIO_PRINT_MARKER, vm->cdev_name, ret);
		goto fail_device_create;
	}

	cdev_init(&vm->cdev, &virtio_backend_fops);

	ret = cdev_add(&vm->cdev, MKDEV(MAJOR(vbe_dev), vm->minor), 1);
	if (ret < 0) {
		pr_err("%s: cdev_add failed for %s ret %d\n", VIRTIO_PRINT_MARKER,
				vm->cdev_name, ret);
		goto fail_cdev_add;
	}

	return 0;

fail_cdev_add:
	device_destroy(vb_dev_class, MKDEV(MAJOR(vbe_dev), vm->minor));
fail_device_create:
	ida_simple_remove(&vm_minor_id, vm->minor);
	return ret;
}

static int
note_shared_buffers(struct device_node *np, struct virt_machine *vm)
{
	int idx = 0;
	u32 len, nr_entries = 0;

	if (of_find_property(np, "shared-buffers", &len))
		nr_entries = len / 4;

	if (!nr_entries) {
		pr_err("%s: No shared-buffers specified for vm %s\n",
			VIRTIO_PRINT_MARKER, vm->vm_name);
		return -EINVAL;
	}

	vm->shmem = kzalloc(nr_entries * sizeof(struct shared_memory),
					GFP_KERNEL);
	if (!vm->shmem)
		return -ENOMEM;

	for (idx = 0; idx < nr_entries; ++idx) {
		struct device_node *snp;
		int ret;

		snp = of_parse_phandle(np, "shared-buffers", idx);
		if (!snp) {
			kfree(vm->shmem);
			pr_err("%s: Can't parse shared-buffer property %d\n",
					VIRTIO_PRINT_MARKER, idx);
			return -EINVAL;
		}

		ret = of_address_to_resource(snp, 0, &vm->shmem[idx].r);
		if (ret) {
			of_node_put(snp);
			kfree(vm->shmem);
			pr_err("%s: Invalid address at index %d\n",
						VIRTIO_PRINT_MARKER, idx);
			return -EINVAL;
		}

		ret = of_property_read_u32(snp, "gunyah-label",
					&vm->shmem[idx].gunyah_label);
		if (ret) {
			of_node_put(snp);
			kfree(vm->shmem);
			pr_err("%s: gunyah-label property absent at index %d\n",
					 VIRTIO_PRINT_MARKER, idx);
			return -EINVAL;
		}

		of_node_put(snp);
	}

	vm->shmem_entries = nr_entries;

	return 0;
}

static struct virt_machine *
new_vm(const char *vm_name, struct device_node *np)
{
	struct virt_machine *vm;
	int ret;
	struct resource r;

	ret = of_address_to_resource(np, 0, &r);
	if (ret || !r.start || !resource_size(&r)) {
		pr_err("%s: Invalid shared memory for VM %s\n",
					VIRTIO_PRINT_MARKER, vm_name);
		return NULL;
	}

	vm = kzalloc(sizeof(*vm), GFP_KERNEL);
	if (!vm)
		return NULL;

	strlcpy(vm->vm_name, vm_name, sizeof(vm->vm_name));

	ret = note_shared_buffers(np, vm);
	if (ret) {
		kfree(vm);
		return NULL;
	}

	vm->shmem_addr = r.start;
	vm->shmem_size = resource_size(&r);
	vm->state = VM_STATE_NOT_ACTIVE;

	snprintf(vm->cdev_name, sizeof(vm->cdev_name),
			"gh_virtio_backend_%s", vm_name);
	spin_lock_init(&vm->vb_dev_lock);
	mutex_init(&vm->mutex);
	INIT_LIST_HEAD(&vm->vb_dev_list);

	ret = vm_devnode_init(vm);
	if (ret) {
		pr_err("%s: vm '%s' backend dev node create failed err %d\n",
				VIRTIO_PRINT_MARKER, vm->vm_name, ret);
		goto vm_devnode_fail;
	}

	init_waitqueue_head(&vm->app_ready_queue);
	spin_lock(&vm_list_lock);
	list_add(&vm->list, &vm_list);
	spin_unlock(&vm_list_lock);

	pr_info("%s: Recognized VM '%s' major/minor %d/%d shmem_addr %pK shmem_size %llx\n",
			VIRTIO_PRINT_MARKER, vm->vm_name, MAJOR(vbe_dev), vm->minor,
			(void *)vm->shmem_addr, vm->shmem_size);

	return vm;

vm_devnode_fail:
	kfree(vm);
	return NULL;
}

static struct virt_machine *find_vm_by_name(const char *vm_name)
{
	struct virt_machine *v = NULL, *tmp;

	spin_lock(&vm_list_lock);
	list_for_each_entry(tmp, &vm_list, list) {
		if (!strcmp(tmp->vm_name, vm_name)) {
			v = tmp;
			break;
		}
	}
	spin_unlock(&vm_list_lock);

	return v;
}

static int gh_virtio_backend_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *np = pdev->dev.of_node, *vm_np;
	struct virtio_backend_device *vb_dev = NULL, *tmp;
	struct virt_machine *vm;
	const char *str;
	u32 label;

	if (!np)
		return -EINVAL;

	vm_np = of_parse_phandle(np, "qcom,vm", 0);
	if (!vm_np) {
		pr_err("%s: 'qcom,vm' property not present\n",
					VIRTIO_PRINT_MARKER);
		return -EINVAL;
	}

	/* Unique identifier of a VM */
	ret = of_property_read_string(vm_np, "vm_name", &str);
	if (ret) {
		of_node_put(vm_np);
		pr_err("%s: 'vm_name' property not present in VM node\n",
				VIRTIO_PRINT_MARKER);
		return ret;
	}

	if (strnlen(str, MAX_VM_NAME) == MAX_VM_NAME) {
		pr_err("%s: VM name %s too long\n", VIRTIO_PRINT_MARKER, str);
		of_node_put(vm_np);
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "qcom,label", &label);
	if (ret || !label) {
		pr_err("%s: Invalid qcom,label property\n",
						VIRTIO_PRINT_MARKER);
		of_node_put(vm_np);
		return -EINVAL;
	}

	mutex_lock(&vm_mutex);
	vm = find_vm_by_name(str);
	if (!vm) {
		vm = new_vm(str, vm_np);
		if (!vm) {
			of_node_put(vm_np);
			mutex_unlock(&vm_mutex);
			return -ENODEV;
		}
	}

	spin_lock(&vm->vb_dev_lock);
	list_for_each_entry(tmp, &vm->vb_dev_list, list) {
		if (label == tmp->label) {
			vb_dev = tmp;
			break;
		}
	}
	spin_unlock(&vm->vb_dev_lock);

	if (vb_dev) {
		vb_dev_put(vb_dev);
		of_node_put(vm_np);
		mutex_unlock(&vm_mutex);
		pr_err("%s: Duplicate qcom,label %d\n",
				VIRTIO_PRINT_MARKER, label);
		return -EINVAL;
	}

	vb_dev = devm_kzalloc(&pdev->dev, sizeof(*vb_dev),
				GFP_KERNEL | __GFP_ZERO);
	if (!vb_dev) {
		of_node_put(vm_np);
		mutex_unlock(&vm_mutex);
		return -ENOMEM;
	}

	vb_dev->label = label;
	vb_dev->vm = vm;
	vb_dev->dev = &pdev->dev;
	spin_lock_init(&vb_dev->lock);
	init_waitqueue_head(&vb_dev->evt_queue);
	init_waitqueue_head(&vb_dev->notify_queue);
	mutex_init(&vb_dev->mutex);

	spin_lock(&vm->vb_dev_lock);
	list_add(&vb_dev->list, &vm->vb_dev_list);
	spin_unlock(&vm->vb_dev_lock);

	of_node_put(vm_np);

	snprintf(vb_dev->int_name, sizeof(vb_dev->int_name),
				"virtio_dev_%x", vb_dev->label);

	platform_set_drvdata(pdev, vb_dev);
	mutex_unlock(&vm_mutex);

	pr_info("%s: Recognized virtio backend device with label %x\n",
			VIRTIO_PRINT_MARKER, vb_dev->label);
	return 0;
}

static int __exit gh_virtio_backend_remove(struct platform_device *pdev)
{
	struct virtio_backend_device *vb_dev = platform_get_drvdata(pdev);
	struct virt_machine *vm;
	u64 isr;
	int i;
	u32 refcount;
	int empty = 0;
	int count;
	unsigned long flags;

	if (!vb_dev)
		return 0;

	vm = vb_dev->vm;

	spin_lock(&vm->vb_dev_lock);
	if (vm->state == VM_STATE_ACTIVE) {
		spin_unlock(&vm->vb_dev_lock);
		return -EBUSY;
	}

	list_del(&vb_dev->list);
	vb_dev->label = 0;
	vb_dev->notify = 1;
	refcount = vb_dev->refcount;
	empty = list_empty(&vm->vb_dev_list);
	spin_unlock(&vm->vb_dev_lock);
retry:
	spin_lock_irqsave(&vb_dev->lock, flags);
	vb_dev->vdev_event = EVENT_MODULE_EXIT;
	vb_dev->vdev_event_data = 0;
	vb_dev->evt_avail = 1;
	spin_unlock_irqrestore(&vb_dev->lock, flags);

	wake_up_interruptible(&vb_dev->evt_queue);

	if (vb_dev->refcount)
		wait_event(vb_dev->notify_queue,
				!vb_dev->refcount);

	spin_lock(&vm->vb_dev_lock);
	refcount = vb_dev->refcount;
	spin_unlock(&vm->vb_dev_lock);

	if (refcount)
		goto retry;

	spin_lock_irqsave(&vb_dev->lock, flags);
	if (vb_dev->irq.ctx) {
		eventfd_ctx_remove_wait_queue(vb_dev->irq.ctx, &vb_dev->irq.wait, &isr);
		eventfd_ctx_put(vb_dev->irq.ctx);
		vb_dev->irq.ctx = NULL;
		fdput(vb_dev->irq.fd);
	}

	for (i = 0; i < MAX_IO_CONTEXTS; ++i) {
		if (vb_dev->ioctx[i].ctx) {
			eventfd_ctx_put(vb_dev->ioctx[i].ctx);
			vb_dev->ioctx[i].ctx = NULL;
		}
	}
	spin_unlock_irqrestore(&vb_dev->lock, flags);

	if (!empty)
		return 0;

	spin_lock(&vm_list_lock);
	list_del(&vm->list);
	count = vm->open_count;
	spin_unlock(&vm_list_lock);

	if (!count) {
		cdev_del(&vm->cdev);
		device_destroy(vb_dev_class, MKDEV(MAJOR(vbe_dev), vm->minor));
		ida_simple_remove(&vm_minor_id, vm->minor);
		kfree(vm);
	}

	return 0;
}

static irqreturn_t vdev_interrupt(int irq, void *data)
{
	struct virtio_backend_device *vb_dev = data;
	u64 event_data, event;
	int ret;
	unsigned long flags;

	ret = get_event(vb_dev->cap_id, &event_data, &event);
	trace_gh_virtio_backend_irq(vb_dev->label, event, event_data, ret);
	if (ret || !event)
		return IRQ_HANDLED;

	spin_lock_irqsave(&vb_dev->lock, flags);
	if (event == EVENT_NEW_BUFFER && vb_dev->ack_driver_ok) {
		vb_dev->vdev_event_data = event_data;
		signal_vqs(vb_dev);
		goto done;
	}
	vb_dev->vdev_event |= event;
	vb_dev->vdev_event_data |= event_data;
	vb_dev->evt_avail = 1;
	wake_up_interruptible(&vb_dev->evt_queue);
done:
	spin_unlock_irqrestore(&vb_dev->lock, flags);

	return IRQ_HANDLED;
}

static int
unshare_a_vm_buffer(gh_vmid_t self, gh_vmid_t peer, struct resource *r,
		    struct shared_memory *shmem)
{
	u32 src_vmlist[2] = {self, peer};
	int dst_vmlist[1] = {self};
	int dst_perms[1] = {PERM_READ | PERM_WRITE | PERM_EXEC};
	int ret;

	if (!shmem->has_lookup_sgl) {
		ret = gh_rm_mem_reclaim(shmem->shm_memparcel, 0);
		if (ret) {
			pr_err("%s: gh_rm_mem_reclaim failed for handle %x addr=%llx size=%lld err=%d\n",
				VIRTIO_PRINT_MARKER, shmem->shm_memparcel, r->start,
				resource_size(r), ret);
			return ret;
		}
	}

	ret = hyp_assign_phys(r->start, resource_size(r), src_vmlist, 2,
				      dst_vmlist, dst_perms, 1);
	if (ret)
		pr_err("%s: hyp_assign_phys failed for addr=%llx size=%lld err=%d\n",
			VIRTIO_PRINT_MARKER, r->start, resource_size(r), ret);

	return ret;
}

static int unshare_vm_buffers(struct virt_machine *vm, gh_vmid_t peer)
{
	int ret, i;
	gh_vmid_t self_vmid;

	if (!vm->hyp_assign_done)
		return 0;

	ret = gh_rm_get_vmid(GH_PRIMARY_VM, &self_vmid);
	if (ret)
		return ret;

	for (i = 0; i < vm->shmem_entries; ++i) {
		ret = unshare_a_vm_buffer(self_vmid, peer, &vm->shmem[i].r,
				&vm->shmem[i]);
		if (ret)
			return ret;
	}
	vm->hyp_assign_done = 0;
	return 0;
}

static int share_a_vm_buffer(gh_vmid_t self, gh_vmid_t peer, int gunyah_label,
				struct resource *r, u32 *shm_memparcel,
				struct shared_memory *shmem)
{
	u32 src_vmlist[1] = {self};
	int src_perms[1] = {PERM_READ | PERM_WRITE | PERM_EXEC};
	int dst_vmlist[2] = {self, peer};
	int dst_perms[2] = {PERM_READ | PERM_WRITE, PERM_READ | PERM_WRITE};
	struct gh_acl_desc *acl;
	struct gh_sgl_desc *sgl;
	int ret;

	acl = kzalloc(offsetof(struct gh_acl_desc, acl_entries[2]), GFP_KERNEL);
	if (!acl)
		return -ENOMEM;
	sgl = kzalloc(offsetof(struct gh_sgl_desc, sgl_entries[1]), GFP_KERNEL);
	if (!sgl) {
		kfree(acl);
		return -ENOMEM;
	}

	ret = hyp_assign_phys(r->start, resource_size(r), src_vmlist, 1,
				      dst_vmlist, dst_perms, 2);
	if (ret) {
		pr_err("%s: hyp_assign_phys failed for addr=%llx size=%lld err=%d\n",
		       VIRTIO_PRINT_MARKER, r->start, resource_size(r), ret);
		kfree(acl);
		kfree(sgl);
		return ret;
	}

	acl->n_acl_entries = 2;
	acl->acl_entries[0].vmid = (u16)self;
	acl->acl_entries[0].perms = GH_RM_ACL_R | GH_RM_ACL_W;
	acl->acl_entries[1].vmid = (u16)peer;
	acl->acl_entries[1].perms = GH_RM_ACL_R | GH_RM_ACL_W;

	sgl->n_sgl_entries = 1;
	sgl->sgl_entries[0].ipa_base = r->start;
	sgl->sgl_entries[0].size = resource_size(r);

	/*
	 * gh_rm_mem_qcom_lookup_sgl is no longer supported and is replaced with
	 * gh_rm_mem_share. To ease this transition, fall back to the later on error.
	 */
	shmem->has_lookup_sgl = true;
	ret = gh_rm_mem_qcom_lookup_sgl(GH_RM_MEM_TYPE_NORMAL,
			gunyah_label, acl, sgl, NULL, shm_memparcel);
	if (ret) {
		shmem->has_lookup_sgl = false;
		ret = gh_rm_mem_share(GH_RM_MEM_TYPE_NORMAL, 0, gunyah_label,
				      acl, sgl, NULL, shm_memparcel);
	}
	if (ret) {
		pr_err("%s: Sharing memory failed %d\n", VIRTIO_PRINT_MARKER, ret);
		/* Attempt to assign resource back to HLOS */
		hyp_assign_phys(r->start, resource_size(r), dst_vmlist, 2,
				      src_vmlist, src_perms, 1);
	}

	kfree(acl);
	kfree(sgl);

	return ret;
}

static int share_vm_buffers(struct virt_machine *vm, gh_vmid_t peer)
{
	int i, ret;
	gh_vmid_t self_vmid;

	ret = gh_rm_get_vmid(GH_PRIMARY_VM, &self_vmid);
	if (ret)
		return ret;

	for (i = 0; i < vm->shmem_entries; ++i) {
		ret = share_a_vm_buffer(self_vmid, peer, vm->shmem[i].gunyah_label,
				&vm->shmem[i].r, &vm->shmem[i].shm_memparcel, &vm->shmem[i]);
		if (ret) {
			i--;
			goto unshare;
		}
	}

	vm->hyp_assign_done = 1;
	return 0;

unshare:
	for (; i >= 0; --i)
		unshare_a_vm_buffer(self_vmid, peer, &vm->shmem[i].r, &vm->shmem[i]);

	return ret;
}

static int gh_virtio_mmio_init(gh_vmid_t vmid, const char *vm_name, gh_label_t label,
			gh_capid_t cap_id, int linux_irq, u64 base, u64 size)
{
	struct virt_machine *vm;
	struct virtio_backend_device *vb_dev;
	int i, ret, nr_words;
	u32 *p, *q;

	pr_debug("%s: vmid %d vm_name %s label %x cap_id %x irq %d base %pK size %d\n",
		VIRTIO_PRINT_MARKER, vmid, vm_name, label, cap_id, linux_irq, (void *)base, size);

	if (strnlen(vm_name, MAX_VM_NAME) == MAX_VM_NAME) {
		pr_err("%s: VM name %s too long\n", VIRTIO_PRINT_MARKER, vm_name);
		return -EINVAL;
	}

	vm = find_vm_by_name(vm_name);
	if (!vm) {
		pr_err("%s: VM name %s not found\n", VIRTIO_PRINT_MARKER, vm_name);
		return -ENODEV;
	}

	spin_lock(&vm->vb_dev_lock);
	if (!vm->app_ready)
		vm->waiting_for_app_ready = 1;
	spin_unlock(&vm->vb_dev_lock);

	ret = wait_event_interruptible_timeout(vm->app_ready_queue,
				vm->app_ready, 60*HZ);
	if (!ret || ret < 0) {
		pr_err("%s: Timing out for app to become ready\n", VIRTIO_PRINT_MARKER);
		return -ETIMEDOUT;
	}

	vb_dev = vb_dev_get(vm, label);
	if (!vb_dev) {
		pr_err("%s: Device with label %x not found\n",
VIRTIO_PRINT_MARKER, label);
		return -ENODEV;
	}

	mutex_lock(&vb_dev->mutex);

	if (!vm->hyp_assign_done) {
		ret = share_vm_buffers(vm, vmid);
		if (ret) {
			mutex_unlock(&vb_dev->mutex);
			vb_dev_put(vb_dev);
			return ret;
		}
	}

	if (!vb_dev->config_data || size < vb_dev->config_size) {
		pr_err("%s: Incorrect config_data for dev %u\n",
				VIRTIO_PRINT_MARKER, label);
		unshare_vm_buffers(vm, vmid);
		mutex_unlock(&vb_dev->mutex);
		vb_dev_put(vb_dev);
		return -EINVAL;
	}

	ret = request_irq(linux_irq, vdev_interrupt, 0,
			vb_dev->int_name, vb_dev);
	if (ret) {
		pr_err("%s: Unable to register interrupt handler for %d\n",
				VIRTIO_PRINT_MARKER, linux_irq);
		unshare_vm_buffers(vm, vmid);
		mutex_unlock(&vb_dev->mutex);
		vb_dev_put(vb_dev);
		return ret;
	}

	vb_dev->config_shared_buf = ioremap(base, size);
	if (!vb_dev->config_shared_buf) {
		pr_err("%s: Unable to map config page\n", VIRTIO_PRINT_MARKER);
		free_irq(linux_irq, vb_dev);
		unshare_vm_buffers(vm, vmid);
		mutex_unlock(&vb_dev->mutex);
		vb_dev_put(vb_dev);
		return -ENOMEM;
	}

	p = (u32 *)vb_dev->config_shared_buf;
	q = (u32 *)vb_dev->config_data;
	nr_words = vb_dev->config_size / 4;

	for (i = 0; i < nr_words; ++i)
		writel_relaxed(*q++, p++);

	p = (u32 *)vb_dev->config_shared_buf;
	for (i = 0; i < nr_words; ++i)
		pr_debug("%s: config_word %d %x\n", VIRTIO_PRINT_MARKER, i, readl_relaxed(p++));

	/* Read lower and higher 32 bit feature bits */
	for (i = 0; i < 2; ++i) {
		if (!vb_dev->features[i])
			continue;

		ret = set_dev_features(cap_id, i, vb_dev->features[i]);
		if (ret) {
			free_irq(linux_irq, vb_dev);
			iounmap(vb_dev->config_shared_buf);
			vb_dev->config_shared_buf = NULL;
			unshare_vm_buffers(vm, vmid);
			mutex_unlock(&vb_dev->mutex);
			vb_dev_put(vb_dev);
			pr_err("%s: set_features %d/%x failed ret %d\n",
				VIRTIO_PRINT_MARKER, i, vb_dev->features[i], ret);
			return ret;
		}
	}

	for (i = 0; i < MAX_QUEUES; ++i) {
		if (!vb_dev->queue_num_max[i])
			continue;

		ret = set_queue_num_max(cap_id, i, vb_dev->queue_num_max[i]);
		if (ret) {
			free_irq(linux_irq, vb_dev);
			iounmap(vb_dev->config_shared_buf);
			vb_dev->config_shared_buf = NULL;
			unshare_vm_buffers(vm, vmid);
			mutex_unlock(&vb_dev->mutex);
			vb_dev_put(vb_dev);
			pr_err("%s: set_queue_num_max %d/%x failed ret %d\n",
				VIRTIO_PRINT_MARKER, i, vb_dev->queue_num_max[i], ret);
			return ret;
		}
	}

	vb_dev->linux_irq = linux_irq;
	vb_dev->cap_id = cap_id;
	vb_dev->config_shared_size = size;
	mutex_unlock(&vb_dev->mutex);
	vb_dev_put(vb_dev);

	return 0;
}

/**
 * gh_virtio_mmio_exit: Handles virtio mmio cleanup on scenarios such as
 * virtual machine shutdown/reboot/crash. For a successful return of
 * gh_virtio_mmio_init on next VM boot, this function has to be called after the
 * previous instance of VM successfully resets.
 * @vmid: vmid of the virtual machine
 * @vm_name: Virtual machine name
 *
 * On success, the function will return 0. Otherwise, a negative number will be
 * returned.
 */
int gh_virtio_mmio_exit(gh_vmid_t vmid, const char *vm_name)
{
	struct virt_machine *vm;
	struct virtio_backend_device *vb_dev;
	unsigned long flags;
	int ret = -EINVAL;
	u32 refcount;

	vm = find_vm_by_name(vm_name);
	if (!vm) {
		pr_debug("%s: VM name %s not found\n", VIRTIO_PRINT_MARKER, vm_name);
		return 0;
	}

	spin_lock(&vm->vb_dev_lock);
	vm->state = VM_STATE_NOT_ACTIVE;

	list_for_each_entry(vb_dev, &vm->vb_dev_list, list) {
		spin_lock_irqsave(&vb_dev->lock, flags);
		vb_dev->vdev_event = EVENT_VM_EXIT;
		vb_dev->vdev_event_data = 0;
		vb_dev->evt_avail = 1;
		vb_dev->notify = 1;
		refcount = vb_dev->refcount;
		spin_unlock_irqrestore(&vb_dev->lock, flags);

		wake_up_interruptible(&vb_dev->evt_queue);
		spin_unlock(&vm->vb_dev_lock);
		if (refcount)
			wait_event(vb_dev->notify_queue, !vb_dev->refcount);

		free_irq(vb_dev->linux_irq, vb_dev);
		iounmap(vb_dev->config_shared_buf);
		vb_dev->config_shared_buf = NULL;
		spin_lock(&vm->vb_dev_lock);
	}

	vm->app_ready = 0;
	spin_unlock(&vm->vb_dev_lock);

	ret = unshare_vm_buffers(vm, vmid);

	return ret;
}
EXPORT_SYMBOL(gh_virtio_mmio_exit);

static const struct of_device_id gh_virtio_backend_match_table[] = {
	{ .compatible = "qcom,virtio_backend" },
	{ },
};

static struct platform_driver gh_virtio_backend_driver = {
	.probe = gh_virtio_backend_probe,
	.remove = gh_virtio_backend_remove,
	.driver = {
		.name = "gh_virtio_backend",
		.of_match_table = gh_virtio_backend_match_table,
	},
};

static int __init gh_virtio_backend_init(void)
{
	int ret;

	ret = vb_devclass_init();
	if (ret)
		return ret;

	ret = gh_rm_set_virtio_mmio_cb(gh_virtio_mmio_init);
	if (ret) {
		vb_devclass_deinit();
		return ret;
	}

	ret = platform_driver_register(&gh_virtio_backend_driver);
	if (ret) {
		gh_rm_unset_virtio_mmio_cb();
		vb_devclass_deinit();
	}

	return ret;
}
module_init(gh_virtio_backend_init);

static void __exit gh_virtio_backend_exit(void)
{
	gh_rm_unset_virtio_mmio_cb();
	platform_driver_unregister(&gh_virtio_backend_driver);
	vb_devclass_deinit();
}
module_exit(gh_virtio_backend_exit);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. Gunyah Virtio Backend driver");
MODULE_LICENSE("GPL v2");
