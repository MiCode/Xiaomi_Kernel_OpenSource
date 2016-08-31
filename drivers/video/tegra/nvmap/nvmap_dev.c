/*
 * drivers/video/tegra/nvmap/nvmap_dev.c
 *
 * User-space interface to nvmap
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/backing-dev.h>
#include <linux/bitmap.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/oom.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/nvmap.h>
#include <linux/module.h>
#include <linux/resource.h>
#include <linux/security.h>
#include <linux/stat.h>

#include <asm/cputype.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>

#define CREATE_TRACE_POINTS
#include <trace/events/nvmap.h>

#include "nvmap_priv.h"
#include "nvmap_ioctl.h"

#define NVMAP_CARVEOUT_KILLER_RETRY_TIME 100 /* msecs */

#ifdef CONFIG_NVMAP_CACHE_MAINT_BY_SET_WAYS
size_t cache_maint_inner_threshold = SZ_2M;
#endif
#ifdef CONFIG_NVMAP_OUTER_CACHE_MAINT_BY_SET_WAYS
size_t cache_maint_outer_threshold = SZ_1M;
#endif

struct nvmap_carveout_node {
	unsigned int		heap_bit;
	struct nvmap_heap	*carveout;
	int			index;
	struct list_head	clients;
	spinlock_t		clients_lock;
	phys_addr_t			base;
	size_t			size;
};

struct platform_device *nvmap_pdev;
EXPORT_SYMBOL(nvmap_pdev);
struct nvmap_device *nvmap_dev;
EXPORT_SYMBOL(nvmap_dev);
struct nvmap_share *nvmap_share;
EXPORT_SYMBOL(nvmap_share);

static struct backing_dev_info nvmap_bdi = {
	.ra_pages	= 0,
	.capabilities	= (BDI_CAP_NO_ACCT_AND_WRITEBACK |
			   BDI_CAP_READ_MAP | BDI_CAP_WRITE_MAP),
};

static struct device_dma_parameters nvmap_dma_parameters = {
	.max_segment_size = UINT_MAX,
};

static int nvmap_open(struct inode *inode, struct file *filp);
static int nvmap_release(struct inode *inode, struct file *filp);
static long nvmap_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static int nvmap_map(struct file *filp, struct vm_area_struct *vma);
static void nvmap_vma_open(struct vm_area_struct *vma);
static void nvmap_vma_close(struct vm_area_struct *vma);
static int nvmap_vma_fault(struct vm_area_struct *vma, struct vm_fault *vmf);

static const struct file_operations nvmap_user_fops = {
	.owner		= THIS_MODULE,
	.open		= nvmap_open,
	.release	= nvmap_release,
	.unlocked_ioctl	= nvmap_ioctl,
	.mmap		= nvmap_map,
};

static const struct file_operations nvmap_super_fops = {
	.owner		= THIS_MODULE,
	.open		= nvmap_open,
	.release	= nvmap_release,
	.unlocked_ioctl	= nvmap_ioctl,
	.mmap		= nvmap_map,
};

static struct vm_operations_struct nvmap_vma_ops = {
	.open		= nvmap_vma_open,
	.close		= nvmap_vma_close,
	.fault		= nvmap_vma_fault,
};

int is_nvmap_vma(struct vm_area_struct *vma)
{
	return vma->vm_ops == &nvmap_vma_ops;
}

struct device *nvmap_client_to_device(struct nvmap_client *client)
{
	if (!client)
		return 0;
	if (client->super)
		return nvmap_dev->dev_super.this_device;
	else
		return nvmap_dev->dev_user.this_device;
}

struct nvmap_share *nvmap_get_share_from_dev(struct nvmap_device *dev)
{
	return &dev->iovmm_master;
}

struct nvmap_deferred_ops *nvmap_get_deferred_ops_from_dev(
		struct nvmap_device *dev)
{
	return &dev->deferred_ops;
}

/* allocates a PTE for the caller's use; returns the PTE pointer or
 * a negative errno. not safe from IRQs */
pte_t **nvmap_alloc_pte_irq(struct nvmap_device *dev, void **vaddr)
{
	unsigned long bit;

	spin_lock(&dev->ptelock);
	bit = find_next_zero_bit(dev->ptebits, NVMAP_NUM_PTES, dev->lastpte);
	if (bit == NVMAP_NUM_PTES) {
		bit = find_first_zero_bit(dev->ptebits, dev->lastpte);
		if (bit == dev->lastpte)
			bit = NVMAP_NUM_PTES;
	}

	if (bit == NVMAP_NUM_PTES) {
		spin_unlock(&dev->ptelock);
		return ERR_PTR(-ENOMEM);
	}

	dev->lastpte = bit;
	set_bit(bit, dev->ptebits);
	spin_unlock(&dev->ptelock);

	*vaddr = dev->vm_rgn->addr + bit * PAGE_SIZE;
	return &(dev->ptes[bit]);
}

/* allocates a PTE for the caller's use; returns the PTE pointer or
 * a negative errno. must be called from sleepable contexts */
pte_t **nvmap_alloc_pte(struct nvmap_device *dev, void **vaddr)
{
	int ret;
	pte_t **pte;
	ret = wait_event_interruptible(dev->pte_wait,
			!IS_ERR(pte = nvmap_alloc_pte_irq(dev, vaddr)));

	if (ret == -ERESTARTSYS)
		return ERR_PTR(-EINTR);

	return pte;
}

/* frees a PTE */
void nvmap_free_pte(struct nvmap_device *dev, pte_t **pte)
{
	unsigned long addr;
	unsigned int bit = pte - dev->ptes;

	if (WARN_ON(bit >= NVMAP_NUM_PTES))
		return;

	addr = (unsigned long)dev->vm_rgn->addr + bit * PAGE_SIZE;
	set_pte_at(&init_mm, addr, *pte, 0);

	spin_lock(&dev->ptelock);
	clear_bit(bit, dev->ptebits);
	spin_unlock(&dev->ptelock);
	wake_up(&dev->pte_wait);
}

/* get pte for the virtual address */
pte_t **nvmap_vaddr_to_pte(struct nvmap_device *dev, unsigned long vaddr)
{
	unsigned int bit;

	BUG_ON(vaddr < (unsigned long)dev->vm_rgn->addr);
	bit = (vaddr - (unsigned long)dev->vm_rgn->addr) >> PAGE_SHIFT;
	BUG_ON(bit >= NVMAP_NUM_PTES);
	return &(dev->ptes[bit]);
}

/*
 * Verifies that the passed ID is a valid handle ID. Then the passed client's
 * reference to the handle is returned.
 *
 * Note: to call this function make sure you own the client ref lock.
 */
struct nvmap_handle_ref *__nvmap_validate_id_locked(struct nvmap_client *c,
						    unsigned long id)
{
	struct rb_node *n = c->handle_refs.rb_node;

	while (n) {
		struct nvmap_handle_ref *ref;
		ref = rb_entry(n, struct nvmap_handle_ref, node);
		if ((unsigned long)ref->handle == id)
			return ref;
		else if (id > (unsigned long)ref->handle)
			n = n->rb_right;
		else
			n = n->rb_left;
	}

	return NULL;
}

struct nvmap_handle *nvmap_get_handle_id(struct nvmap_client *client,
					 unsigned long id)
{
#ifdef CONFIG_NVMAP_USE_FD_FOR_HANDLE
	return nvmap_handle_get((struct nvmap_handle *)id);
#else
	struct nvmap_handle_ref *ref;
	struct nvmap_handle *h = NULL;

	nvmap_ref_lock(client);
	ref = __nvmap_validate_id_locked(client, id);
	if (ref)
		h = ref->handle;
	if (h)
		h = nvmap_handle_get(h);
	nvmap_ref_unlock(client);
	return h;
#endif
}

unsigned long nvmap_carveout_usage(struct nvmap_client *c,
				   struct nvmap_heap_block *b)
{
	struct nvmap_heap *h = nvmap_block_to_heap(b);
	struct nvmap_carveout_node *n;
	int i;

	for (i = 0; i < nvmap_dev->nr_carveouts; i++) {
		n = &nvmap_dev->heaps[i];
		if (n->carveout == h)
			return n->heap_bit;
	}
	return 0;
}

/*
 * This routine is used to flush the carveout memory from cache.
 * Why cache flush is needed for carveout? Consider the case, where a piece of
 * carveout is allocated as cached and released. After this, if the same memory is
 * allocated for uncached request and the memory is not flushed out from cache.
 * In this case, the client might pass this to H/W engine and it could start modify
 * the memory. As this was cached earlier, it might have some portion of it in cache.
 * During cpu request to read/write other memory, the cached portion of this memory
 * might get flushed back to main memory and would cause corruptions, if it happens
 * after H/W writes data to memory.
 *
 * But flushing out the memory blindly on each carveout allocation is redundant.
 *
 * In order to optimize the carveout buffer cache flushes, the following
 * strategy is used.
 *
 * The whole Carveout is flushed out from cache during its initialization.
 * During allocation, carveout buffers are not flused from cache.
 * During deallocation, carveout buffers are flushed, if they were allocated as cached.
 * if they were allocated as uncached/writecombined, no cache flush is needed.
 * Just draining store buffers is enough.
 */
int nvmap_flush_heap_block(struct nvmap_client *client,
	struct nvmap_heap_block *block, size_t len, unsigned int prot)
{
	pte_t **pte;
	void *addr;
	uintptr_t kaddr;
	phys_addr_t phys = block->base;
	phys_addr_t end = block->base + len;

	if (prot == NVMAP_HANDLE_UNCACHEABLE || prot == NVMAP_HANDLE_WRITE_COMBINE)
		goto out;

#ifdef CONFIG_NVMAP_CACHE_MAINT_BY_SET_WAYS
	if (len >= cache_maint_inner_threshold) {
		inner_flush_cache_all();
		if (prot != NVMAP_HANDLE_INNER_CACHEABLE)
			outer_flush_range(block->base, block->base + len);
		goto out;
	}
#endif

	pte = nvmap_alloc_pte(nvmap_dev, &addr);
	if (IS_ERR(pte))
		return PTR_ERR(pte);

	kaddr = (uintptr_t)addr;

	while (phys < end) {
		phys_addr_t next = (phys + PAGE_SIZE) & PAGE_MASK;
		unsigned long pfn = __phys_to_pfn(phys);
		void *base = (void *)kaddr + (phys & ~PAGE_MASK);

		next = min(next, end);
		set_pte_at(&init_mm, kaddr, *pte, pfn_pte(pfn, pgprot_kernel));
		nvmap_flush_tlb_kernel_page(kaddr);
		__cpuc_flush_dcache_area(base, next - phys);
		phys = next;
	}

	if (prot != NVMAP_HANDLE_INNER_CACHEABLE)
		outer_flush_range(block->base, block->base + len);

	nvmap_free_pte(nvmap_dev, pte);
out:
	wmb();
	return 0;
}

void nvmap_carveout_commit_add(struct nvmap_client *client,
			       struct nvmap_carveout_node *node,
			       size_t len)
{
	spin_lock(&node->clients_lock);
	BUG_ON(list_empty(&client->carveout_commit[node->index].list) &&
	       client->carveout_commit[node->index].commit != 0);

	client->carveout_commit[node->index].commit += len;
	/* if this client isn't already on the list of nodes for this heap,
	   add it */
	if (list_empty(&client->carveout_commit[node->index].list)) {
		list_add(&client->carveout_commit[node->index].list,
			 &node->clients);
	}
	spin_unlock(&node->clients_lock);
}

void nvmap_carveout_commit_subtract(struct nvmap_client *client,
				    struct nvmap_carveout_node *node,
				    size_t len)
{
	if (!client)
		return;

	spin_lock(&node->clients_lock);
	BUG_ON(client->carveout_commit[node->index].commit < len);
	client->carveout_commit[node->index].commit -= len;
	/* if no more allocation in this carveout for this node, delete it */
	if (!client->carveout_commit[node->index].commit)
		list_del_init(&client->carveout_commit[node->index].list);
	spin_unlock(&node->clients_lock);
}

static struct nvmap_client *get_client_from_carveout_commit(
	struct nvmap_carveout_node *node, struct nvmap_carveout_commit *commit)
{
	struct nvmap_carveout_commit *first_commit = commit - node->index;
	return (void *)first_commit - offsetof(struct nvmap_client,
					       carveout_commit);
}

static
struct nvmap_heap_block *do_nvmap_carveout_alloc(struct nvmap_client *client,
					      struct nvmap_handle *handle,
					      unsigned long type)
{
	struct nvmap_carveout_node *co_heap;
	struct nvmap_device *dev = nvmap_dev;
	int i;

	for (i = 0; i < dev->nr_carveouts; i++) {
		struct nvmap_heap_block *block;
		co_heap = &dev->heaps[i];

		if (!(co_heap->heap_bit & type))
			continue;

		block = nvmap_heap_alloc(co_heap->carveout, handle);
		if (block)
			return block;
	}
	return NULL;
}

struct nvmap_heap_block *nvmap_carveout_alloc(struct nvmap_client *client,
					      struct nvmap_handle *handle,
					      unsigned long type)
{
	return do_nvmap_carveout_alloc(client, handle, type);
}

/* remove a handle from the device's tree of all handles; called
 * when freeing handles. */
int nvmap_handle_remove(struct nvmap_device *dev, struct nvmap_handle *h)
{
	spin_lock(&dev->handle_lock);

	/* re-test inside the spinlock if the handle really has no clients;
	 * only remove the handle if it is unreferenced */
	if (atomic_add_return(0, &h->ref) > 0) {
		spin_unlock(&dev->handle_lock);
		return -EBUSY;
	}
	smp_rmb();
	BUG_ON(atomic_read(&h->ref) < 0);
	BUG_ON(atomic_read(&h->pin) != 0);

	rb_erase(&h->node, &dev->handles);

	spin_unlock(&dev->handle_lock);
	return 0;
}

/* adds a newly-created handle to the device master tree */
void nvmap_handle_add(struct nvmap_device *dev, struct nvmap_handle *h)
{
	struct rb_node **p;
	struct rb_node *parent = NULL;

	spin_lock(&dev->handle_lock);
	p = &dev->handles.rb_node;
	while (*p) {
		struct nvmap_handle *b;

		parent = *p;
		b = rb_entry(parent, struct nvmap_handle, node);
		if (h > b)
			p = &parent->rb_right;
		else
			p = &parent->rb_left;
	}
	rb_link_node(&h->node, parent, p);
	rb_insert_color(&h->node, &dev->handles);
	spin_unlock(&dev->handle_lock);
}

/* validates that a handle is in the device master tree, and that the
 * client has permission to access it */
struct nvmap_handle *nvmap_validate_get(struct nvmap_client *client,
					unsigned long id, bool skip_val)
{
#ifdef CONFIG_NVMAP_USE_FD_FOR_HANDLE
	return nvmap_handle_get((struct nvmap_handle *)id);
#else
	struct nvmap_handle *h = NULL;
	struct rb_node *n;

	spin_lock(&nvmap_dev->handle_lock);

	n = nvmap_dev->handles.rb_node;

	while (n) {
		h = rb_entry(n, struct nvmap_handle, node);
		if ((unsigned long)h == id) {
			if (client->super || h->global ||
			    (h->owner == client) || skip_val)
				h = nvmap_handle_get(h);
			else
				h = nvmap_get_handle_id(client, id);
			spin_unlock(&nvmap_dev->handle_lock);
			return h;
		}
		if (id > (unsigned long)h)
			n = n->rb_right;
		else
			n = n->rb_left;
	}
	spin_unlock(&nvmap_dev->handle_lock);
	return NULL;
#endif
}

struct nvmap_client *__nvmap_create_client(struct nvmap_device *dev,
					   const char *name)
{
	struct nvmap_client *client;
	struct task_struct *task;
	int i;

	if (WARN_ON(!dev))
		return NULL;

	client = kzalloc(sizeof(*client) + (sizeof(struct nvmap_carveout_commit)
			 * dev->nr_carveouts), GFP_KERNEL);
	if (!client)
		return NULL;

	client->name = name;
	client->super = true;
	client->kernel_client = true;
	client->handle_refs = RB_ROOT;

	atomic_set(&client->iovm_commit, 0);

	for (i = 0; i < dev->nr_carveouts; i++) {
		INIT_LIST_HEAD(&client->carveout_commit[i].list);
		client->carveout_commit[i].commit = 0;
	}

	get_task_struct(current->group_leader);
	task_lock(current->group_leader);
	/* don't bother to store task struct for kernel threads,
	   they can't be killed anyway */
	if (current->flags & PF_KTHREAD) {
		put_task_struct(current->group_leader);
		task = NULL;
	} else {
		task = current->group_leader;
	}
	task_unlock(current->group_leader);
	client->task = task;

	mutex_init(&client->ref_lock);
	atomic_set(&client->count, 1);

	spin_lock(&dev->clients_lock);
	list_add(&client->list, &dev->clients);
	spin_unlock(&dev->clients_lock);
	return client;
}

static void destroy_client(struct nvmap_client *client)
{
	struct rb_node *n;
	int i;

	if (!client)
		return;

	spin_lock(&nvmap_dev->clients_lock);
	list_del(&client->list);
	spin_unlock(&nvmap_dev->clients_lock);

	while ((n = rb_first(&client->handle_refs))) {
		struct nvmap_handle_ref *ref;
		int pins, dupes;

		ref = rb_entry(n, struct nvmap_handle_ref, node);

		smp_rmb();
		pins = atomic_read(&ref->pin);

		while (pins--)
			__nvmap_unpin(ref);

		if (ref->handle->owner == client) {
			ref->handle->owner = NULL;
			ref->handle->owner_ref = NULL;
		}

		dma_buf_put(ref->handle->dmabuf);
		rb_erase(&ref->node, &client->handle_refs);

		dupes = atomic_read(&ref->dupes);
		while (dupes--)
			nvmap_handle_put(ref->handle);

		kfree(ref);
	}

	for (i = 0; i < nvmap_dev->nr_carveouts; i++)
		list_del(&client->carveout_commit[i].list);

	if (client->task)
		put_task_struct(client->task);

	kfree(client);
}

struct nvmap_client *nvmap_client_get(struct nvmap_client *client)
{
	if (!virt_addr_valid(client))
		return NULL;

	if (!atomic_add_unless(&client->count, 1, 0))
		return NULL;

	return client;
}

struct nvmap_client *nvmap_client_get_file(int fd)
{
	struct nvmap_client *client = ERR_PTR(-EFAULT);
	struct file *f = fget(fd);
	if (!f)
		return ERR_PTR(-EINVAL);

	if ((f->f_op == &nvmap_user_fops) || (f->f_op == &nvmap_super_fops)) {
		client = f->private_data;
		atomic_inc(&client->count);
	}

	fput(f);
	return client;
}

void nvmap_client_put(struct nvmap_client *client)
{
	if (!client)
		return;

	if (!atomic_dec_return(&client->count))
		destroy_client(client);
}
EXPORT_SYMBOL(nvmap_client_put);

static int nvmap_open(struct inode *inode, struct file *filp)
{
	struct miscdevice *miscdev = filp->private_data;
	struct nvmap_device *dev = dev_get_drvdata(miscdev->parent);
	struct nvmap_client *priv;
	int ret;
	__attribute__((unused)) struct rlimit old_rlim, new_rlim;

	ret = nonseekable_open(inode, filp);
	if (unlikely(ret))
		return ret;

	BUG_ON(dev != nvmap_dev);
	priv = __nvmap_create_client(dev, "user");
	if (!priv)
		return -ENOMEM;
	trace_nvmap_open(priv, priv->name);

	priv->kernel_client = false;
	priv->super = (filp->f_op == &nvmap_super_fops);

	filp->f_mapping->backing_dev_info = &nvmap_bdi;

	filp->private_data = priv;
	return 0;
}

static int nvmap_release(struct inode *inode, struct file *filp)
{
	struct nvmap_client *priv = filp->private_data;

	trace_nvmap_release(priv, priv->name);
	nvmap_client_put(priv);
	return 0;
}

int __nvmap_map(struct nvmap_handle *h, struct vm_area_struct *vma)
{
	struct nvmap_vma_priv *priv;

	h = nvmap_handle_get(h);
	if (!h)
		return -EINVAL;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->offs = 0;
	priv->handle = h;
	atomic_set(&priv->count, 1);

	vma->vm_flags |= (VM_SHARED | VM_IO | VM_DONTEXPAND |
			  VM_MIXEDMAP | VM_DONTDUMP | VM_DONTCOPY);
	vma->vm_ops = &nvmap_vma_ops;
	BUG_ON(vma->vm_private_data != NULL);
	vma->vm_private_data = priv;
	vma->vm_page_prot = nvmap_pgprot(h, vma->vm_page_prot);
	return 0;
}

static int nvmap_map(struct file *filp, struct vm_area_struct *vma)
{
	struct nvmap_vma_priv *priv;

	/* after NVMAP_IOC_MMAP, the handle that is mapped by this VMA
	 * will be stored in vm_private_data and faulted in. until the
	 * ioctl is made, the VMA is mapped no-access */
	vma->vm_private_data = NULL;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->offs = 0;
	priv->handle = NULL;
	atomic_set(&priv->count, 1);

	vma->vm_flags |= (VM_SHARED | VM_IO | VM_DONTEXPAND |
			  VM_MIXEDMAP | VM_DONTDUMP | VM_DONTCOPY);
	vma->vm_ops = &nvmap_vma_ops;
	vma->vm_private_data = priv;

	return 0;
}

static long nvmap_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	void __user *uarg = (void __user *)arg;

	if (_IOC_TYPE(cmd) != NVMAP_IOC_MAGIC)
		return -ENOTTY;

	if (_IOC_NR(cmd) > NVMAP_IOC_MAXNR)
		return -ENOTTY;

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, uarg, _IOC_SIZE(cmd));
	if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, uarg, _IOC_SIZE(cmd));

	if (err)
		return -EFAULT;

	switch (cmd) {
	case NVMAP_IOC_CLAIM:
		nvmap_warn(filp->private_data, "preserved handles not"
			   "supported\n");
		err = -ENODEV;
		break;
	case NVMAP_IOC_CREATE:
	case NVMAP_IOC_FROM_ID:
	case NVMAP_IOC_FROM_FD:
		err = nvmap_ioctl_create(filp, cmd, uarg);
		break;

	case NVMAP_IOC_GET_ID:
		err = nvmap_ioctl_getid(filp, uarg);
		break;

	case NVMAP_IOC_GET_FD:
		err = nvmap_ioctl_getfd(filp, uarg);
		break;

	case NVMAP_IOC_PARAM:
		err = nvmap_ioctl_get_param(filp, uarg);
		break;

	case NVMAP_IOC_UNPIN_MULT:
	case NVMAP_IOC_PIN_MULT:
		err = nvmap_ioctl_pinop(filp, cmd == NVMAP_IOC_PIN_MULT, uarg);
		break;

	case NVMAP_IOC_ALLOC:
		err = nvmap_ioctl_alloc(filp, uarg);
		break;

	case NVMAP_IOC_ALLOC_KIND:
		err = nvmap_ioctl_alloc_kind(filp, uarg);
		break;

	case NVMAP_IOC_FREE:
		err = nvmap_ioctl_free(filp, arg);
		break;

	case NVMAP_IOC_MMAP:
		err = nvmap_map_into_caller_ptr(filp, uarg);
		break;

	case NVMAP_IOC_WRITE:
	case NVMAP_IOC_READ:
		err = nvmap_ioctl_rw_handle(filp, cmd == NVMAP_IOC_READ, uarg);
		break;

	case NVMAP_IOC_CACHE:
		err = nvmap_ioctl_cache_maint(filp, uarg);
		break;

	case NVMAP_IOC_SHARE:
		err = nvmap_ioctl_share_dmabuf(filp, uarg);
		break;

	default:
		return -ENOTTY;
	}
	return err;
}

/* to ensure that the backing store for the VMA isn't freed while a fork'd
 * reference still exists, nvmap_vma_open increments the reference count on
 * the handle, and nvmap_vma_close decrements it. alternatively, we could
 * disallow copying of the vma, or behave like pmem and zap the pages. FIXME.
*/
static void nvmap_vma_open(struct vm_area_struct *vma)
{
	struct nvmap_vma_priv *priv;

	priv = vma->vm_private_data;
	BUG_ON(!priv);

	atomic_inc(&priv->count);
}

static void nvmap_vma_close(struct vm_area_struct *vma)
{
	struct nvmap_vma_priv *priv = vma->vm_private_data;

	if (priv) {
		if (!atomic_dec_return(&priv->count)) {
			if (priv->handle)
				nvmap_handle_put(priv->handle);
			kfree(priv);
		}
	}
	vma->vm_private_data = NULL;
}

static int nvmap_vma_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct page *page;
	struct nvmap_vma_priv *priv;
	unsigned long offs;

	offs = (unsigned long)(vmf->virtual_address - vma->vm_start);
	priv = vma->vm_private_data;
	if (!priv || !priv->handle || !priv->handle->alloc)
		return VM_FAULT_SIGBUS;

	offs += priv->offs;
	/* if the VMA was split for some reason, vm_pgoff will be the VMA's
	 * offset from the original VMA */
	offs += (vma->vm_pgoff << PAGE_SHIFT);

	if (offs >= priv->handle->size)
		return VM_FAULT_SIGBUS;

	if (!priv->handle->heap_pgalloc) {
		unsigned long pfn;
		BUG_ON(priv->handle->carveout->base & ~PAGE_MASK);
		pfn = ((priv->handle->carveout->base + offs) >> PAGE_SHIFT);
		if (!pfn_valid(pfn)) {
			vm_insert_pfn(vma,
				(unsigned long)vmf->virtual_address, pfn);
			return VM_FAULT_NOPAGE;
		}
		/* CMA memory would get here */
		page = pfn_to_page(pfn);
	} else {
		offs >>= PAGE_SHIFT;
		page = priv->handle->pgalloc.pages[offs];
	}

	if (page)
		get_page(page);
	vmf->page = page;
	return (page) ? 0 : VM_FAULT_SIGBUS;
}

#define DEBUGFS_OPEN_FOPS(name) \
static int nvmap_debug_##name##_open(struct inode *inode, \
					    struct file *file) \
{ \
	return single_open(file, nvmap_debug_##name##_show, \
			    inode->i_private); \
} \
\
static const struct file_operations debug_##name##_fops = { \
	.open = nvmap_debug_##name##_open, \
	.read = seq_read, \
	.llseek = seq_lseek, \
	.release = single_release, \
}

#define K(x) (x >> 10)

static void client_stringify(struct nvmap_client *client, struct seq_file *s)
{
	char task_comm[TASK_COMM_LEN];
	if (!client->task) {
		seq_printf(s, "%-18s %18s %8u", client->name, "kernel", 0);
		return;
	}
	get_task_comm(task_comm, client->task);
	seq_printf(s, "%-18s %18s %8u", client->name, task_comm,
		   client->task->pid);
}

static void allocations_stringify(struct nvmap_client *client,
				  struct seq_file *s, bool iovmm)
{
	struct rb_node *n;

	nvmap_ref_lock(client);
	n = rb_first(&client->handle_refs);
	for (; n != NULL; n = rb_next(n)) {
		struct nvmap_handle_ref *ref =
			rb_entry(n, struct nvmap_handle_ref, node);
		struct nvmap_handle *handle = ref->handle;
		if (handle->alloc && handle->heap_pgalloc == iovmm) {
			phys_addr_t base = iovmm ? 0 :
					   (handle->carveout->base);
			seq_printf(s,
				"%-18s %-18s %8llx %10zuK %8x %6u %6u %6u\n",
				"", "",
				(unsigned long long)base, K(handle->size),
				handle->userflags,
				atomic_read(&handle->ref),
				atomic_read(&ref->dupes),
				atomic_read(&ref->pin));
		}
	}
	nvmap_ref_unlock(client);
}

static int nvmap_debug_allocations_show(struct seq_file *s, void *unused)
{
	struct nvmap_carveout_node *node = s->private;
	struct nvmap_carveout_commit *commit;
	unsigned int total = 0;

	spin_lock(&node->clients_lock);
	seq_printf(s, "%-18s %18s %8s %11s\n",
		"CLIENT", "PROCESS", "PID", "SIZE");
	seq_printf(s, "%-18s %18s %8s %11s %8s %6s %6s %6s\n",
			"", "", "BASE", "SIZE", "FLAGS", "REFS",
			"DUPES", "PINS");
	list_for_each_entry(commit, &node->clients, list) {
		struct nvmap_client *client =
			get_client_from_carveout_commit(node, commit);
		client_stringify(client, s);
		seq_printf(s, " %10zuK\n", K(commit->commit));
		allocations_stringify(client, s, false);
		seq_printf(s, "\n");
		total += commit->commit;
	}
	seq_printf(s, "%-18s %-18s %8s %10uK\n", "total", "", "", K(total));
	spin_unlock(&node->clients_lock);
	return 0;
}

DEBUGFS_OPEN_FOPS(allocations);

static int nvmap_debug_clients_show(struct seq_file *s, void *unused)
{
	struct nvmap_carveout_node *node = s->private;
	struct nvmap_carveout_commit *commit;
	unsigned int total = 0;

	spin_lock(&node->clients_lock);
	seq_printf(s, "%-18s %18s %8s %11s\n",
		"CLIENT", "PROCESS", "PID", "SIZE");
	list_for_each_entry(commit, &node->clients, list) {
		struct nvmap_client *client =
			get_client_from_carveout_commit(node, commit);
		client_stringify(client, s);
		seq_printf(s, " %10zu\n", K(commit->commit));
		total += commit->commit;
	}
	seq_printf(s, "%-18s %18s %8s %10uK\n", "total", "", "", K(total));
	spin_unlock(&node->clients_lock);
	return 0;
}

DEBUGFS_OPEN_FOPS(clients);

static void nvmap_iovmm_get_total_mss(u64 *pss, u64 *non_pss, u64 *total)
{
	int i;
	struct rb_node *n;
	struct nvmap_device *dev = nvmap_dev;

	*total = 0;
	if (pss)
		*pss = 0;
	if (non_pss)
		*non_pss = 0;
	if (!dev)
		return;
	spin_lock(&dev->handle_lock);
	n = rb_first(&dev->handles);
	for (; n != NULL; n = rb_next(n)) {
		struct nvmap_handle *h =
			rb_entry(n, struct nvmap_handle, node);

		if (!h || !h->alloc || !h->heap_pgalloc)
			continue;
		if (!non_pss) {
			*total += h->size;
			continue;
		}

		for (i = 0; i < h->size >> PAGE_SHIFT; i++) {
			int mapcount = page_mapcount(h->pgalloc.pages[i]);
			if (!mapcount)
				*non_pss += PAGE_SIZE;
			*total += PAGE_SIZE;
		}
	}
	if (pss && non_pss)
		*pss = *total - *non_pss;
	spin_unlock(&dev->handle_lock);
}

#define PRINT_MEM_STATS_NOTE(x) \
do { \
	seq_printf(s, "Note: total memory is precise account of pages " \
		"allocated by NvMap.\nIt doesn't match with all clients " \
		"\"%s\" accumulated as shared memory \nis accounted in " \
		"full in each clients \"%s\" that shared memory.\n", #x, #x); \
} while (0)

static int nvmap_debug_iovmm_clients_show(struct seq_file *s, void *unused)
{
	u64 total;
	struct nvmap_client *client;
	struct nvmap_device *dev = s->private;

	spin_lock(&dev->clients_lock);
	seq_printf(s, "%-18s %18s %8s %11s\n",
		"CLIENT", "PROCESS", "PID", "SIZE");
	list_for_each_entry(client, &dev->clients, list) {
		int iovm_commit = atomic_read(&client->iovm_commit);
		client_stringify(client, s);
		seq_printf(s, " %10uK\n", K(iovm_commit));
	}
	spin_unlock(&dev->clients_lock);
	nvmap_iovmm_get_total_mss(NULL, NULL, &total);
	seq_printf(s, "%-18s %18s %8s %10lluK\n", "total", "", "", K(total));
	PRINT_MEM_STATS_NOTE(SIZE);
	return 0;
}

DEBUGFS_OPEN_FOPS(iovmm_clients);

static int nvmap_debug_iovmm_allocations_show(struct seq_file *s, void *unused)
{
	u64 total;
	struct nvmap_client *client;
	struct nvmap_device *dev = s->private;

	spin_lock(&dev->clients_lock);
	seq_printf(s, "%-18s %18s %8s %11s\n",
		"CLIENT", "PROCESS", "PID", "SIZE");
	seq_printf(s, "%-18s %18s %8s %11s %8s %6s %6s %6s\n",
			"", "", "BASE", "SIZE", "FLAGS", "REFS",
			"DUPES", "PINS");
	list_for_each_entry(client, &dev->clients, list) {
		int iovm_commit = atomic_read(&client->iovm_commit);
		client_stringify(client, s);
		seq_printf(s, " %10uK\n", K(iovm_commit));
		allocations_stringify(client, s, true);
		seq_printf(s, "\n");
	}
	spin_unlock(&dev->clients_lock);
	nvmap_iovmm_get_total_mss(NULL, NULL, &total);
	seq_printf(s, "%-18s %-18s %8s %10lluK\n", "total", "", "", K(total));
	PRINT_MEM_STATS_NOTE(SIZE);
	return 0;
}

DEBUGFS_OPEN_FOPS(iovmm_allocations);

static void nvmap_iovmm_get_client_mss(struct nvmap_client *client, u64 *pss,
				   u64 *non_pss, u64 *total)
{
	int i;
	struct rb_node *n;

	*pss = *non_pss = *total = 0;
	nvmap_ref_lock(client);
	n = rb_first(&client->handle_refs);
	for (; n != NULL; n = rb_next(n)) {
		struct nvmap_handle_ref *ref =
			rb_entry(n, struct nvmap_handle_ref, node);
		struct nvmap_handle *h = ref->handle;

		if (!h || !h->alloc || !h->heap_pgalloc)
			continue;

		for (i = 0; i < h->size >> PAGE_SHIFT; i++) {
			int mapcount = page_mapcount(h->pgalloc.pages[i]);
			if (!mapcount)
				*non_pss += PAGE_SIZE;
			*total += PAGE_SIZE;
		}
		*pss = *total - *non_pss;
	}
	nvmap_ref_unlock(client);
}

static int nvmap_debug_iovmm_procrank_show(struct seq_file *s, void *unused)
{
	u64 pss, non_pss, total;
	struct nvmap_client *client;
	struct nvmap_device *dev = s->private;
	u64 total_memory, total_pss, total_non_pss;

	spin_lock(&dev->clients_lock);
	seq_printf(s, "%-18s %18s %8s %11s %11s %11s\n",
		"CLIENT", "PROCESS", "PID", "PSS", "NON-PSS", "TOTAL");
	list_for_each_entry(client, &dev->clients, list) {
		client_stringify(client, s);
		nvmap_iovmm_get_client_mss(client, &pss, &non_pss, &total);
		seq_printf(s, " %10lluK %10lluK %10lluK\n", K(pss),
			K(non_pss), K(total));
	}
	spin_unlock(&dev->clients_lock);

	nvmap_iovmm_get_total_mss(&total_pss, &total_non_pss, &total_memory);
	seq_printf(s, "%-18s %18s %8s %10lluK %10lluK %10lluK\n",
		"total", "", "", K(total_pss),
		K(total_non_pss), K(total_memory));
	PRINT_MEM_STATS_NOTE(TOTAL);
	return 0;
}

DEBUGFS_OPEN_FOPS(iovmm_procrank);

ulong nvmap_iovmm_get_used_pages(void)
{
	u64 total;

	nvmap_iovmm_get_total_mss(NULL, NULL, &total);
	return total >> PAGE_SHIFT;
}

static void nvmap_deferred_ops_init(struct nvmap_deferred_ops *deferred_ops)
{
	INIT_LIST_HEAD(&deferred_ops->ops_list);
	spin_lock_init(&deferred_ops->deferred_ops_lock);

#ifdef CONFIG_NVMAP_DEFERRED_CACHE_MAINT
	deferred_ops->enable_deferred_cache_maintenance = 1;
#else
	deferred_ops->enable_deferred_cache_maintenance = 0;
#endif /* CONFIG_NVMAP_DEFERRED_CACHE_MAINT */

	deferred_ops->deferred_maint_inner_requested = 0;
	deferred_ops->deferred_maint_inner_flushed = 0;
	deferred_ops->deferred_maint_outer_requested = 0;
	deferred_ops->deferred_maint_outer_flushed = 0;
}

static int nvmap_probe(struct platform_device *pdev)
{
	struct nvmap_platform_data *plat = pdev->dev.platform_data;
	struct nvmap_device *dev;
	struct dentry *nvmap_debug_root;
	unsigned int i;
	int e;

	if (!plat) {
		dev_err(&pdev->dev, "no platform data?\n");
		return -ENODEV;
	}

	/*
	 * The DMA mapping API uses these parameters to decide how to map the
	 * passed buffers. If the maximum physical segment size is set to
	 * smaller than the size of the buffer, then the buffer will be mapped
	 * as separate IO virtual address ranges.
	 */
	pdev->dev.dma_parms = &nvmap_dma_parameters;

	if (WARN_ON(nvmap_dev != NULL)) {
		dev_err(&pdev->dev, "only one nvmap device may be present\n");
		return -ENODEV;
	}

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&pdev->dev, "out of memory for device\n");
		return -ENOMEM;
	}

	dev->dev_user.minor = MISC_DYNAMIC_MINOR;
	dev->dev_user.name = "nvmap";
	dev->dev_user.fops = &nvmap_user_fops;
	dev->dev_user.parent = &pdev->dev;

	dev->dev_super.minor = MISC_DYNAMIC_MINOR;
	dev->dev_super.name = "knvmap";
	dev->dev_super.fops = &nvmap_super_fops;
	dev->dev_super.parent = &pdev->dev;

	dev->handles = RB_ROOT;

	init_waitqueue_head(&dev->pte_wait);

	init_waitqueue_head(&dev->iovmm_master.pin_wait);

	nvmap_deferred_ops_init(&dev->deferred_ops);

	mutex_init(&dev->iovmm_master.pin_lock);
#ifdef CONFIG_NVMAP_PAGE_POOLS
	for (i = 0; i < NVMAP_NUM_POOLS; i++)
		nvmap_page_pool_init(&dev->iovmm_master.pools[i], i);
#endif

	dev->vm_rgn = alloc_vm_area(NVMAP_NUM_PTES * PAGE_SIZE, NULL);
	if (!dev->vm_rgn) {
		e = -ENOMEM;
		dev_err(&pdev->dev, "couldn't allocate remapping region\n");
		goto fail;
	}

	spin_lock_init(&dev->ptelock);
	spin_lock_init(&dev->handle_lock);
	INIT_LIST_HEAD(&dev->clients);
	spin_lock_init(&dev->clients_lock);

	for (i = 0; i < NVMAP_NUM_PTES; i++) {
		unsigned long addr;
		pgd_t *pgd;
		pud_t *pud;
		pmd_t *pmd;

		addr = (unsigned long)dev->vm_rgn->addr + (i * PAGE_SIZE);
		pgd = pgd_offset_k(addr);
		pud = pud_alloc(&init_mm, pgd, addr);
		if (!pud) {
			e = -ENOMEM;
			dev_err(&pdev->dev, "couldn't allocate page tables\n");
			goto fail;
		}
		pmd = pmd_alloc(&init_mm, pud, addr);
		if (!pmd) {
			e = -ENOMEM;
			dev_err(&pdev->dev, "couldn't allocate page tables\n");
			goto fail;
		}
		dev->ptes[i] = pte_alloc_kernel(pmd, addr);
		if (!dev->ptes[i]) {
			e = -ENOMEM;
			dev_err(&pdev->dev, "couldn't allocate page tables\n");
			goto fail;
		}
	}

	e = misc_register(&dev->dev_user);
	if (e) {
		dev_err(&pdev->dev, "unable to register miscdevice %s\n",
			dev->dev_user.name);
		goto fail;
	}

	e = misc_register(&dev->dev_super);
	if (e) {
		dev_err(&pdev->dev, "unable to register miscdevice %s\n",
			dev->dev_super.name);
		goto fail;
	}

	dev->nr_carveouts = 0;
	dev->heaps = kzalloc(sizeof(struct nvmap_carveout_node) *
			     plat->nr_carveouts, GFP_KERNEL);
	if (!dev->heaps) {
		e = -ENOMEM;
		dev_err(&pdev->dev, "couldn't allocate carveout memory\n");
		goto fail;
	}

	nvmap_debug_root = debugfs_create_dir("nvmap", NULL);
	if (IS_ERR_OR_NULL(nvmap_debug_root))
		dev_err(&pdev->dev, "couldn't create debug files\n");

	debugfs_create_bool("enable_deferred_cache_maintenance",
		S_IRUGO|S_IWUSR, nvmap_debug_root,
		(u32 *)&dev->deferred_ops.enable_deferred_cache_maintenance);

	debugfs_create_u32("max_handle_count", S_IRUGO,
			nvmap_debug_root, &nvmap_max_handle_count);

	debugfs_create_u64("deferred_maint_inner_requested", S_IRUGO|S_IWUSR,
			nvmap_debug_root,
			&dev->deferred_ops.deferred_maint_inner_requested);

	debugfs_create_u64("deferred_maint_inner_flushed", S_IRUGO|S_IWUSR,
			nvmap_debug_root,
			&dev->deferred_ops.deferred_maint_inner_flushed);
#ifdef CONFIG_OUTER_CACHE
	debugfs_create_u64("deferred_maint_outer_requested", S_IRUGO|S_IWUSR,
			nvmap_debug_root,
			&dev->deferred_ops.deferred_maint_outer_requested);

	debugfs_create_u64("deferred_maint_outer_flushed", S_IRUGO|S_IWUSR,
			nvmap_debug_root,
			&dev->deferred_ops.deferred_maint_outer_flushed);
#endif /* CONFIG_OUTER_CACHE */
	for (i = 0; i < plat->nr_carveouts; i++) {
		struct nvmap_carveout_node *node = &dev->heaps[dev->nr_carveouts];
		const struct nvmap_platform_carveout *co = &plat->carveouts[i];
		node->base = round_up(co->base, PAGE_SIZE);
		node->size = round_down(co->size -
					(node->base - co->base), PAGE_SIZE);
		if (!co->size)
			continue;

		node->carveout = nvmap_heap_create(
				dev->dev_user.this_device, co,
				node->base, node->size, node);

		if (!node->carveout) {
			e = -ENOMEM;
			dev_err(&pdev->dev, "couldn't create %s\n", co->name);
			goto fail_heaps;
		}
		node->index = dev->nr_carveouts;
		dev->nr_carveouts++;
		spin_lock_init(&node->clients_lock);
		INIT_LIST_HEAD(&node->clients);
		node->heap_bit = co->usage_mask;

		if (!IS_ERR_OR_NULL(nvmap_debug_root)) {
			struct dentry *heap_root =
				debugfs_create_dir(co->name, nvmap_debug_root);
			if (!IS_ERR_OR_NULL(heap_root)) {
				debugfs_create_file("clients", S_IRUGO,
					heap_root, node, &debug_clients_fops);
				debugfs_create_file("allocations", S_IRUGO,
					heap_root, node,
					&debug_allocations_fops);
				nvmap_heap_debugfs_init(heap_root,
					node->carveout);
			}
		}
	}
	if (!IS_ERR_OR_NULL(nvmap_debug_root)) {
		struct dentry *iovmm_root =
			debugfs_create_dir("iovmm", nvmap_debug_root);
		if (!IS_ERR_OR_NULL(iovmm_root)) {
			debugfs_create_file("clients", S_IRUGO, iovmm_root,
				dev, &debug_iovmm_clients_fops);
			debugfs_create_file("allocations", S_IRUGO, iovmm_root,
				dev, &debug_iovmm_allocations_fops);
			debugfs_create_file("procrank", S_IRUGO, iovmm_root,
				dev, &debug_iovmm_procrank_fops);
#ifdef CONFIG_NVMAP_PAGE_POOLS
			for (i = 0; i < NVMAP_NUM_POOLS; i++) {
				char name[40];
				char *memtype_string[] = {"uc", "wc",
							  "iwb", "wb"};
				sprintf(name, "%s_page_pool_available_pages",
					memtype_string[i]);
				debugfs_create_u32(name, S_IRUGO,
					iovmm_root,
					&dev->iovmm_master.pools[i].npages);
			}
#endif
		}
#ifdef CONFIG_NVMAP_CACHE_MAINT_BY_SET_WAYS
		debugfs_create_size_t("cache_maint_inner_threshold",
				      S_IRUSR | S_IWUSR,
				      nvmap_debug_root,
				      &cache_maint_inner_threshold);

		/* cortex-a9 */
		if ((read_cpuid_id() >> 4 & 0xfff) == 0xc09)
			cache_maint_inner_threshold = SZ_32K;
		pr_info("nvmap:inner cache maint threshold=%d",
			cache_maint_inner_threshold);
#endif
#ifdef CONFIG_NVMAP_OUTER_CACHE_MAINT_BY_SET_WAYS
		debugfs_create_size_t("cache_maint_outer_threshold",
				      S_IRUSR | S_IWUSR,
				      nvmap_debug_root,
				      &cache_maint_outer_threshold);
		pr_info("nvmap:outer cache maint threshold=%d",
			cache_maint_outer_threshold);
#endif
	}

	platform_set_drvdata(pdev, dev);
	nvmap_pdev = pdev;
	nvmap_dev = dev;
	nvmap_share = &dev->iovmm_master;

	nvmap_dmabuf_debugfs_init(nvmap_debug_root);
	e = nvmap_dmabuf_stash_init();
	if (e)
		goto fail_heaps;

	return 0;
fail_heaps:
	for (i = 0; i < dev->nr_carveouts; i++) {
		struct nvmap_carveout_node *node = &dev->heaps[i];
		nvmap_heap_destroy(node->carveout);
	}
fail:
	kfree(dev->heaps);
	if (dev->dev_super.minor != MISC_DYNAMIC_MINOR)
		misc_deregister(&dev->dev_super);
	if (dev->dev_user.minor != MISC_DYNAMIC_MINOR)
		misc_deregister(&dev->dev_user);
	if (dev->vm_rgn)
		free_vm_area(dev->vm_rgn);
	kfree(dev);
	nvmap_dev = NULL;
	return e;
}

static int nvmap_remove(struct platform_device *pdev)
{
	struct nvmap_device *dev = platform_get_drvdata(pdev);
	struct rb_node *n;
	struct nvmap_handle *h;
	int i;

	misc_deregister(&dev->dev_super);
	misc_deregister(&dev->dev_user);

	while ((n = rb_first(&dev->handles))) {
		h = rb_entry(n, struct nvmap_handle, node);
		rb_erase(&h->node, &dev->handles);
		kfree(h);
	}

	for (i = 0; i < dev->nr_carveouts; i++) {
		struct nvmap_carveout_node *node = &dev->heaps[i];
		nvmap_heap_destroy(node->carveout);
	}
	kfree(dev->heaps);

	free_vm_area(dev->vm_rgn);
	kfree(dev);
	nvmap_dev = NULL;
	return 0;
}

static int nvmap_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int nvmap_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver nvmap_driver = {
	.probe		= nvmap_probe,
	.remove		= nvmap_remove,
	.suspend	= nvmap_suspend,
	.resume		= nvmap_resume,

	.driver = {
		.name	= "tegra-nvmap",
		.owner	= THIS_MODULE,
	},
};

static int __init nvmap_init_driver(void)
{
	int e;

	nvmap_dev = NULL;

	e = nvmap_heap_init();
	if (e)
		goto fail;

	e = platform_driver_register(&nvmap_driver);
	if (e) {
		nvmap_heap_deinit();
		goto fail;
	}

fail:
	return e;
}
fs_initcall(nvmap_init_driver);

static void __exit nvmap_exit_driver(void)
{
	platform_driver_unregister(&nvmap_driver);
	nvmap_heap_deinit();
	nvmap_dev = NULL;
}
module_exit(nvmap_exit_driver);
