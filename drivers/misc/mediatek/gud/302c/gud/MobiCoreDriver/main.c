/*
 * Copyright (c) 2013-2016 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
/*
 * MobiCore Driver Kernel Module.
 *
 * This driver represents the command proxy on the lowest layer, from the
 * secure world to the non secure world, and vice versa.

 * This driver offers IOCTL commands, for access to the secure world, and has
 * the interface from the secure world to the normal world.
 * The access to the driver is possible with a file descriptor,
 * which has to be created by the fd = open(/dev/mobicore) command or
 * fd = open(/dev/mobicore-user)
 */
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/completion.h>
#include <linux/fdtable.h>
#include <linux/cdev.h>
#ifdef CONFIG_OF
#include <linux/of_irq.h>
#endif
#ifdef CONFIG_MT_TRUSTONIC_TEE_DEBUGFS
#include <linux/debugfs.h>
#endif
#include <net/net_namespace.h>
#include <net/sock.h>
#include <net/tcp_states.h>
#include <net/af_unix.h>

#include "main.h"
#include "fastcall.h"

#include "arm.h"
#include "mem.h"
#include "ops.h"
#include "pm.h"
#include "debug.h"
#include "logging.h"
#include "build_tag.h"

/* Define a MobiCore device structure for use with dev_debug() etc */
struct device_driver mcd_debug_name = {
	.name = "MobiCore"
};

struct device mcd_debug_subname = {
	.driver = &mcd_debug_name
};

struct device *mcd = &mcd_debug_subname;

/* We need 2 devices for admin and user interface*/
#define MC_DEV_MAX 2

/* Need to discover a chrdev region for the driver */
static dev_t mc_dev_admin, mc_dev_user;
struct cdev mc_admin_cdev, mc_user_cdev;
/* Device class for the driver assigned major */
static struct class *mc_device_class;

#ifndef FMODE_PATH
 #define FMODE_PATH 0x0
#endif

static struct sock *__get_socket(struct file *filp)
{
	struct sock *u_sock = NULL;
	struct inode *inode = filp->f_path.dentry->d_inode;

	/*
	 *	Socket ?
	 */
	if (S_ISSOCK(inode->i_mode) && !(filp->f_mode & FMODE_PATH)) {
		struct socket *sock = SOCKET_I(inode);
		struct sock *s = sock->sk;

		/*
		 *	PF_UNIX ?
		 */
		if (s && sock->ops && sock->ops->family == PF_UNIX)
			u_sock = s;
	}
	return u_sock;
}


/* MobiCore interrupt context data */
static struct mc_context ctx;

/* Get process context from file pointer */
static struct mc_instance *get_instance(struct file *file)
{
	return (struct mc_instance *)(file->private_data);
}

extern struct mc_mmu_table *find_mmu_table(unsigned int handle);
uint32_t mc_get_new_handle(void)
{
	uint32_t handle;
	struct mc_buffer *buffer;
	struct mc_mmu_table *table;


	mutex_lock(&ctx.cont_bufs_lock);
retry:
	handle = atomic_inc_return(&ctx.handle_counter);
	/* The handle must leave 12 bits (PAGE_SHIFT) for the 12 LSBs to be
	 * zero, as mmap requires the offset to be page-aligned, plus 1 bit for
	 * the MSB to be 0 too, so mmap does not see the offset as negative
	 * and fail.
	 */
	if ((handle << (PAGE_SHIFT+1)) == 0)  {
		atomic_set(&ctx.handle_counter, 1);
		handle = 1;
	}
	list_for_each_entry(buffer, &ctx.cont_bufs, list) {
		if (buffer->handle == handle)
			goto retry;
	}

	/* here we assume table_lock is already taken. */
	table = find_mmu_table(handle);
	if (table != NULL)
		goto retry;

	mutex_unlock(&ctx.cont_bufs_lock);

	return handle;
}

/* Clears the reserved bit of each page and frees the pages */
static inline void free_continguous_pages(void *addr, unsigned int order)
{
	int i;
	struct page *page = virt_to_page(addr);
	for (i = 0; i < (1<<order); i++) {
		MCDRV_DBG_VERBOSE(mcd, "free page at 0x%p", page);
		clear_bit(PG_reserved, &page->flags);
		page++;
	}

	MCDRV_DBG_VERBOSE(mcd, "freeing addr:%p, order:%x", addr, order);
	free_pages((unsigned long)addr, order);
}

/* Frees the memory associated with a buffer */
static int free_buffer(struct mc_buffer *buffer)
{
	if (buffer->handle == 0)
		return -EINVAL;

	if (buffer->addr == 0)
		return -EINVAL;

	if (!atomic_dec_and_test(&buffer->usage)) {
		MCDRV_DBG_VERBOSE(mcd, "Could not free %u, usage=%d",
				  buffer->handle,
				  atomic_read(&(buffer->usage)));
		return 0;
	}

	MCDRV_DBG_VERBOSE(mcd,
			  "h=%u phy=0x%llx, kaddr=0x%p len=%u buf=%p usage=%d",
			  buffer->handle, (u64)buffer->phys, buffer->addr,
			  buffer->len, buffer, atomic_read(&(buffer->usage)));

	list_del(&buffer->list);

	free_continguous_pages(buffer->addr, buffer->order);
	kfree(buffer);
	return 0;
}

static uint32_t mc_find_cont_wsm_addr(struct mc_instance *instance, void *uaddr,
	void **addr, uint32_t len)
{
	int ret = 0;
	struct mc_buffer *buffer;

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	mutex_lock(&instance->lock);

	mutex_lock(&ctx.cont_bufs_lock);

	/* search for the given handle in the buffers list */
	list_for_each_entry(buffer, &ctx.cont_bufs, list) {
		if (buffer->uaddr == uaddr && buffer->len == len) {
			*addr = buffer->addr;
			goto found;
		}
	}

	/* Coundn't find the buffer */
	ret = -EINVAL;

found:
	mutex_unlock(&ctx.cont_bufs_lock);
	mutex_unlock(&instance->lock);

	return ret;
}

bool mc_check_owner_fd(struct mc_instance *instance, int32_t fd)
{
#ifndef __ARM_VE_A9X4_STD__
	struct file *fp;
	struct sock *s;
	struct files_struct *files;
	struct task_struct *peer = NULL;
	bool ret = false;

	MCDRV_DBG_VERBOSE(mcd, "Finding wsm for fd = %d", fd);
	if (!instance)
		return false;

	if (is_daemon(instance))
		return true;

	rcu_read_lock();
	fp = fcheck_files(current->files, fd);
	if (fp == NULL)
		goto out;
	s = __get_socket(fp);
	if (s)
		peer = get_pid_task(s->sk_peer_pid, PIDTYPE_PID);

	if (peer) {
		task_lock(peer);
		files = peer->files;
		if (!files)
			goto out;
		for (fd = 0; fd < files_fdtable(files)->max_fds; fd++) {
			fp = fcheck_files(files, fd);
			if (!fp)
				continue;
			if (fp->private_data == instance) {
				ret = true;
				break;
			}
		}
	} else {
		MCDRV_DBG(mcd, "Owner not found!");
	}
out:
	if (peer) {
		task_unlock(peer);
		put_task_struct(peer);
	}
	rcu_read_unlock();
	if (!ret)
		MCDRV_DBG(mcd, "Owner not found!");
	return ret;
#else
	return true;
#endif
}
static uint32_t mc_find_cont_wsm(struct mc_instance *instance, uint32_t handle,
	int32_t fd, phys_addr_t *phys, uint32_t *len)
{
	int ret = 0;
	struct mc_buffer *buffer;

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	if (WARN_ON(!is_daemon(instance))) {
		MCDRV_DBG_ERROR(mcd, "caller not MobiCore Daemon");
		return -EPERM;
	}

	mutex_lock(&instance->lock);

	mutex_lock(&ctx.cont_bufs_lock);

	/* search for the given handle in the buffers list */
	list_for_each_entry(buffer, &ctx.cont_bufs, list) {
		if (buffer->handle == handle) {
			if (mc_check_owner_fd(buffer->instance, fd)) {
				*phys = buffer->phys;
				*len = buffer->len;
				goto found;
			} else {
				break;
			}
		}
	}

	/* Couldn't find the buffer */
	ret = -EINVAL;

found:
	mutex_unlock(&ctx.cont_bufs_lock);
	mutex_unlock(&instance->lock);

	return ret;
}

/*
 * __free_buffer - Free a WSM buffer allocated with mobicore_allocate_wsm
 *
 * @instance
 * @handle		handle of the buffer
 *
 * Returns 0 if no error
 *
 */
static int __free_buffer(struct mc_instance *instance, uint32_t handle,
		bool unlock)
{
	int ret = 0;
	struct mc_buffer *buffer;
#ifndef MC_VM_UNMAP
	struct mm_struct *mm = current->mm;
#endif

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	mutex_lock(&ctx.cont_bufs_lock);
	/* search for the given handle in the buffers list */
	list_for_each_entry(buffer, &ctx.cont_bufs, list) {
		if (buffer->handle == handle)
			goto found_buffer;
	}
	ret = -EINVAL;
	goto err;
found_buffer:
	if (!is_daemon(instance) && buffer->instance != instance) {
		ret = -EPERM;
		goto err;
	}
	mutex_unlock(&ctx.cont_bufs_lock);
	/* Only unmap if the request is coming from the user space and
	 * it hasn't already been unmapped */
	if (!unlock && buffer->uaddr != NULL) {
#ifndef MC_VM_UNMAP
		/* do_munmap must be done with mm->mmap_sem taken */
		down_write(&mm->mmap_sem);
		ret = do_munmap(mm,
				(long unsigned int)buffer->uaddr,
				buffer->len);
		up_write(&mm->mmap_sem);

#else
		ret = vm_munmap((long unsigned int)buffer->uaddr, buffer->len);
#endif
		if (ret < 0) {
			/* Something is not right if we end up here, better not
			 * clean the buffer so we just leak memory instead of
			 * creating security issues */
			MCDRV_DBG_ERROR(mcd, "Memory can't be unmapped");
			return -EINVAL;
		}
	}

	mutex_lock(&ctx.cont_bufs_lock);
	/* search for the given handle in the buffers list */
	list_for_each_entry(buffer, &ctx.cont_bufs, list) {
		if (buffer->handle == handle)
			goto del_buffer;
	}
	ret = -EINVAL;
	goto err;

del_buffer:
	if (is_daemon(instance) || buffer->instance == instance)
		ret = free_buffer(buffer);
	else
		ret = -EPERM;
err:
	mutex_unlock(&ctx.cont_bufs_lock);
	return ret;
}

int mc_free_buffer(struct mc_instance *instance, uint32_t handle)
{
	int ret = 0;

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	mutex_lock(&instance->lock);

	ret = __free_buffer(instance, handle, false);
	mutex_unlock(&instance->lock);
	return ret;
}


int mc_get_buffer(struct mc_instance *instance,
	struct mc_buffer **buffer, unsigned long len)
{
	struct mc_buffer *cbuffer = NULL;
	void *addr = 0;
	phys_addr_t phys = 0;
	unsigned int order;
#if defined(DEBUG_VERBOSE)
	unsigned long allocated_size;
#endif
	int ret = 0;

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	if (len == 0) {
		MCDRV_DBG_WARN(mcd, "cannot allocate size 0");
		return -ENOMEM;
	}

	order = get_order(len);
	if (order > MAX_ORDER) {
		MCDRV_DBG_WARN(mcd, "Buffer size too large");
		return -ENOMEM;
	}
#if defined(DEBUG_VERBOSE)
	allocated_size = (1 << order) * PAGE_SIZE;
#endif

	if (mutex_lock_interruptible(&instance->lock))
		return -ERESTARTSYS;

	/* allocate a new buffer. */
	cbuffer = kzalloc(sizeof(*cbuffer), GFP_KERNEL);
	if (!cbuffer) {
		ret = -ENOMEM;
		goto end;
	}

	MCDRV_DBG_VERBOSE(mcd, "size %ld -> order %d --> %ld (2^n pages)",
			  len, order, allocated_size);

	addr = (void *)__get_free_pages(GFP_USER | __GFP_ZERO, order);
	if (!addr) {
		ret = -ENOMEM;
		goto end;
	}

	phys = virt_to_phys(addr);
	cbuffer->handle = mc_get_new_handle();
	cbuffer->phys = phys;
	cbuffer->addr = addr;
	cbuffer->order = order;
	cbuffer->len = len;
	cbuffer->instance = instance;
	cbuffer->uaddr = 0;
	/* Refcount +1 because the TLC is requesting it */
	atomic_set(&cbuffer->usage, 1);

	INIT_LIST_HEAD(&cbuffer->list);
	mutex_lock(&ctx.cont_bufs_lock);
	list_add(&cbuffer->list, &ctx.cont_bufs);
	mutex_unlock(&ctx.cont_bufs_lock);

	MCDRV_DBG_VERBOSE(mcd,
			  "phy=0x%llx-0x%llx, kaddr=0x%p h=%d buf=%p usage=%d",
			  (u64)phys,
			  (u64)(phys+allocated_size),
			  addr, cbuffer->handle,
			  cbuffer, atomic_read(&(cbuffer->usage)));
	*buffer = cbuffer;

end:
	if (ret)
		kfree(cbuffer);

	mutex_unlock(&instance->lock);
	return ret;
}

/*
 * __lock_buffer() - Locks a contiguous buffer - +1 refcount.
 * Assumes the instance lock is already taken!
 */
static int __lock_buffer(struct mc_instance *instance, uint32_t handle)
{
	int ret = 0;
	struct mc_buffer *buffer;

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	if (WARN_ON(!is_daemon(instance))) {
		MCDRV_DBG_ERROR(mcd, "caller not MobiCore Daemon");
		return -EPERM;
	}

	mutex_lock(&ctx.cont_bufs_lock);
	/* search for the given handle in the buffers list */
	list_for_each_entry(buffer, &ctx.cont_bufs, list) {
		if (buffer->handle == handle) {
			atomic_inc(&buffer->usage);
			MCDRV_DBG_VERBOSE(mcd, "handle=%u phy=0x%llx usage=%d",
					  buffer->handle, (u64)buffer->phys,
					  atomic_read(&(buffer->usage)));
			goto unlock;
		}
	}
	ret = -EINVAL;

unlock:
	mutex_unlock(&ctx.cont_bufs_lock);
	return ret;
}

static phys_addr_t get_mci_base_phys(unsigned int len)
{
	if (ctx.mci_base.phys) {
		return ctx.mci_base.phys;
	} else {
		unsigned int order = get_order(len);
		ctx.mcp = NULL;
		ctx.mci_base.order = order;
		ctx.mci_base.addr =
			(void *)__get_free_pages(GFP_USER | __GFP_ZERO, order);
		ctx.mci_base.len = (1 << order) * PAGE_SIZE;
		if (ctx.mci_base.addr == NULL) {
			MCDRV_DBG_WARN(mcd, "get_free_pages failed");
			memset(&ctx.mci_base, 0, sizeof(ctx.mci_base));
			return 0;
		}
		ctx.mci_base.phys = virt_to_phys(ctx.mci_base.addr);
		return ctx.mci_base.phys;
	}
}

/*
 * Create a MMU table from a virtual memory buffer which can be vmalloc
 * or user space virtual memory
 */
int mc_register_wsm_mmu(struct mc_instance *instance,
	void *buffer, uint32_t len,
	uint32_t *handle, phys_addr_t *phys)
{
	int ret = 0;
	struct mc_mmu_table *table = NULL;
	struct task_struct *task = current;
	void *kbuff = NULL;

	uint32_t index;
	uint64_t *mmu_table = NULL;
	uint32_t nb_of_1mb_section;
	unsigned int offset;
	unsigned int page_number;
	unsigned int *handles = NULL;
	unsigned int tmp_len;

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	if (len == 0) {
		MCDRV_DBG_ERROR(mcd, "len=0 is not supported!");
		return -EINVAL;
	}


	/* The offset of the buffer*/
	offset = (unsigned int)
		(((unsigned long)(buffer)) & (~PAGE_MASK));

	MCDRV_DBG_VERBOSE(mcd, "buffer: %p, len=%08x offset=%d",
			  buffer, len, offset);

	/* Number of 4k pages required */
	page_number = (offset + len) / PAGE_SIZE;
	if (((offset + len) & (~PAGE_MASK)) != 0)
		page_number++;

	/* Number of 1mb sections */
	nb_of_1mb_section = (page_number * PAGE_SIZE) / SZ_1M;
	if (((page_number * PAGE_SIZE) & (SZ_1M - 1)) != 0)
		nb_of_1mb_section++;

	/* since for both non-LPAE and LPAE cases we use uint64_t records
	 *  for the fake table we don't support more than 512 MB TA size
	 */
	if (nb_of_1mb_section > SZ_4K / sizeof(uint64_t)) {
		MCDRV_DBG_ERROR(mcd, "fake L1 table size too big");
		return -ENOMEM;
	}
	MCDRV_DBG_VERBOSE(mcd, "nb_of_1mb_section=%d", nb_of_1mb_section);
	if (nb_of_1mb_section > 1) {
		/* WSM buffer with size greater than 1Mb
		 * is available for open session command
		 * from the Daemon only
		 */
		if (!is_daemon(instance)) {
			MCDRV_DBG_ERROR(mcd, "caller not MobiCore Daemon");
			return -EPERM;
		}
		MCDRV_DBG_VERBOSE(mcd, "allocate %d L2 table",
				  nb_of_1mb_section);
		mmu_table = (uint64_t *)get_zeroed_page(GFP_KERNEL);
		MCDRV_DBG_VERBOSE(mcd, "mmu_table = 0x%p", mmu_table);
		if (mmu_table == NULL) {
			MCDRV_DBG_ERROR(mcd,
					"fake L1 table alloc. failed");
			return -ENOMEM;
		}
	}

	if (!mc_find_cont_wsm_addr(instance, buffer, &kbuff, len)) {
		buffer = kbuff;
		task = NULL;
	}

	/* This array is used to free mmu tables in case of any error */
	handles = kmalloc(sizeof(unsigned int)*nb_of_1mb_section,
			  GFP_KERNEL | __GFP_ZERO);
	if (handles == NULL) {
		MCDRV_DBG_ERROR(mcd, "auxiliary handles array alloc. failed");
		ret = -ENOMEM;
		goto err;
	}
	/* Each L1 record refers 1MB piece of TA blob
	 * for both non-LPAE and LPAE modes
	 */

	tmp_len = (len + offset > SZ_1M) ? (SZ_1M - offset) : len;
	for (index = 0; index < nb_of_1mb_section; index++) {
		table = mc_alloc_mmu_table(instance, task, buffer, tmp_len, 0);

		if (IS_ERR(table)) {
			MCDRV_DBG_ERROR(mcd, "mc_alloc_mmu_table() failed");
			ret = -EINVAL;
			goto err;
		}
		handles[index] = table->handle;

		if (mmu_table != NULL) {
			MCDRV_DBG_VERBOSE(mcd, "fake L1 %p add L2 descr 0x%llX",
					  mmu_table + index,
					  (u64)table->phys);
			mmu_table[index] = table->phys;
		}

		buffer += tmp_len;
		len -= tmp_len;
		tmp_len = (len > SZ_1M) ? SZ_1M : len;
	}
	if (mmu_table != NULL) {
		MCDRV_DBG_VERBOSE(mcd, "fake L1 buffer: %p, len=%zu",
				  mmu_table,
				  nb_of_1mb_section*sizeof(uint64_t));

		table = mc_alloc_mmu_table(
					instance,
					NULL,
					mmu_table,
					nb_of_1mb_section*sizeof(uint64_t),
					MC_MMU_TABLE_TYPE_WSM_FAKE_L1);
		if (IS_ERR(table)) {
			MCDRV_DBG_ERROR(mcd, "mc_alloc_mmu_table() failed");
			ret = -EINVAL;
			goto err;
		}
	}

	/* set response */
	*handle = table->handle;
	/* WARNING: daemon shouldn't know this either, but live with it */
	if (is_daemon(instance))
		*phys = table->phys;
	else
		*phys = 0;

	MCDRV_DBG_VERBOSE(mcd, "handle: %d, phys=0x%llX",
			  *handle, (u64)(*phys));

	MCDRV_DBG_VERBOSE(mcd, "exit with %d/0x%08X", ret, ret);

	kfree(handles);

	return ret;

err:
	if (handles != NULL) {
		for (index = 0; index < nb_of_1mb_section; index++)
			mc_free_mmu_table(instance, handles[index]);
		kfree(handles);
	}
	free_page((unsigned long)mmu_table);
	return ret;
}

int mc_unregister_wsm_mmu(struct mc_instance *instance, uint32_t handle)
{
	int ret = 0;

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	/* free table (if no further locks exist) */
	mc_free_mmu_table(instance, handle);

	return ret;
}
/* Lock the object from handle, it could be a WSM MMU table or a cont buffer! */
static int mc_lock_handle(struct mc_instance *instance, uint32_t handle)
{
	int ret = 0;

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	if (WARN_ON(!is_daemon(instance))) {
		MCDRV_DBG_ERROR(mcd, "caller not MobiCore Daemon");
		return -EPERM;
	}

	mutex_lock(&instance->lock);
	ret = mc_lock_mmu_table(instance, handle);

	/* Handle was not a MMU table but a cont buffer */
	if (ret == -EINVAL) {
		/* Call the non locking variant! */
		ret = __lock_buffer(instance, handle);
	}

	mutex_unlock(&instance->lock);

	return ret;
}

static int mc_unlock_handle(struct mc_instance *instance, uint32_t handle)
{
	int ret = 0;

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	if (WARN_ON(!is_daemon(instance))) {
		MCDRV_DBG_ERROR(mcd, "caller not MobiCore Daemon");
		return -EPERM;
	}

	mutex_lock(&instance->lock);
	ret = mc_free_mmu_table(instance, handle);

	/* Not a MMU table, then it must be a buffer */
	if (ret == -EINVAL) {
		/* Call the non locking variant! */
		ret = __free_buffer(instance, handle, true);
	}
	mutex_unlock(&instance->lock);

	return ret;
}

static phys_addr_t mc_find_wsm_mmu(struct mc_instance *instance,
	uint32_t handle, int32_t fd)
{
	if (WARN(!instance, "No instance data available"))
		return 0;

	if (WARN_ON(!is_daemon(instance))) {
		MCDRV_DBG_ERROR(mcd, "caller not MobiCore Daemon");
		return 0;
	}

	return mc_find_mmu_table(handle, fd);
}

static int mc_clean_wsm_mmu(struct mc_instance *instance)
{
	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	if (WARN_ON(!is_daemon(instance))) {
		MCDRV_DBG_ERROR(mcd, "caller not MobiCore Daemon");
		return -EPERM;
	}

	mc_clean_mmu_tables();

	return 0;
}

static int mc_fd_mmap(struct file *file, struct vm_area_struct *vmarea)
{
	struct mc_instance *instance = get_instance(file);
	unsigned long len = vmarea->vm_end - vmarea->vm_start;
	uint32_t handle = vmarea->vm_pgoff;
	struct mc_buffer *buffer = 0;
	int ret = 0;

	MCDRV_DBG_VERBOSE(mcd, "start=0x%p, size=%ld, offset=%ld, mci=0x%llX",
			  (void *)vmarea->vm_start, len, vmarea->vm_pgoff,
			  (u64)ctx.mci_base.phys);

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	if (len == 0) {
		MCDRV_DBG_ERROR(mcd, "cannot allocate size 0");
		return -ENOMEM;
	}
	if (handle) {
		mutex_lock(&ctx.cont_bufs_lock);

		/* search for the buffer list. */
		list_for_each_entry(buffer, &ctx.cont_bufs, list) {
			/* Only allow mapping if the client owns it!*/
			if (buffer->handle == handle &&
			    buffer->instance == instance) {
				/* We shouldn't do remap with larger size */
				if (buffer->len > len)
					break;
				/* We can't allow mapping the buffer twice */
				if (!buffer->uaddr)
					goto found;
				else
					break;
				}
		}
		/* Nothing found return */
		mutex_unlock(&ctx.cont_bufs_lock);
		MCDRV_DBG_ERROR(mcd, "handle not found");
		return -EINVAL;

found:
		buffer->uaddr = (void *)vmarea->vm_start;
		vmarea->vm_flags |= VM_IO;
		/*
		 * Convert kernel address to user address. Kernel address begins
		 * at PAGE_OFFSET, user address range is below PAGE_OFFSET.
		 * Remapping the area is always done, so multiple mappings
		 * of one region are possible. Now remap kernel address
		 * space into user space
		 */
		ret = (int)remap_pfn_range(vmarea, vmarea->vm_start,
				page_to_pfn(virt_to_page(buffer->addr)),
				buffer->len, vmarea->vm_page_prot);
		/* If the remap failed then don't mark this buffer as marked
		 * since the unmaping will also fail */
		if (ret)
			buffer->uaddr = NULL;
		mutex_unlock(&ctx.cont_bufs_lock);
	} else {
		if (!is_daemon(instance))
			return -EPERM;

		if (!ctx.mci_base.addr)
			return -EFAULT;

		if (len != ctx.mci_base.len)
			return -EINVAL;

		vmarea->vm_flags |= VM_IO;
		/* Convert kernel address to user address. Kernel address begins
		 * at PAGE_OFFSET, user address range is below PAGE_OFFSET.
		 * Remapping the area is always done, so multiple mappings
		 * of one region are possible. Now remap kernel address
		 * space into user space */
		ret = (int)remap_pfn_range(vmarea, vmarea->vm_start,
				page_to_pfn(virt_to_page(ctx.mci_base.addr)),
				len, vmarea->vm_page_prot);
	}

	MCDRV_DBG_VERBOSE(mcd, "exit with %d/0x%08X", ret, ret);

	return ret;
}

static inline int ioctl_check_pointer(unsigned int cmd, int __user *uarg)
{
	int err = 0;
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, uarg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, uarg, _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

	return 0;
}

/*
 * mc_fd_user_ioctl() - Will be called from user space as ioctl(..)
 * @file	pointer to file
 * @cmd		command
 * @arg		arguments
 *
 * Returns 0 for OK and an errno in case of error
 */
static long mc_fd_user_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	struct mc_instance *instance = get_instance(file);
	int __user *uarg = (int __user *)arg;
	int ret = -EINVAL;

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	if (ioctl_check_pointer(cmd, uarg))
		return -EFAULT;

	switch (cmd) {
	case MC_IO_FREE:
		ret = mc_free_buffer(instance, (uint32_t)arg);
		break;

	/* 32/64 bit interface compatiblity notice:
	 * mc_ioctl_reg_wsm has been defined with the buffer parameter
	 * as void* which means that the size and layout of the structure
	 * are different between 32 and 64 bit variants.
	 * However our 64 bit Linux driver must be able to service both
	 * 32 and 64 bit clients so we have to allow both IOCTLs. Though
	 * we have a bit of copy paste code we provide maximum backwards
	 * compatiblity */
	case MC_IO_REG_WSM:{
		struct mc_ioctl_reg_wsm reg;
		phys_addr_t phys = 0;
		if (copy_from_user(&reg, uarg, sizeof(reg)))
			return -EFAULT;

		ret = mc_register_wsm_mmu(instance,
			(void *)(uintptr_t)reg.buffer,
			reg.len, &reg.handle, &phys);
		reg.table_phys = phys;

		if (!ret) {
			if (copy_to_user(uarg, &reg, sizeof(reg))) {
				ret = -EFAULT;
				mc_unregister_wsm_mmu(instance, reg.handle);
			}
		}
		break;
	}
	case MC_COMPAT_REG_WSM:{
		struct mc_compat_ioctl_reg_wsm reg;
		phys_addr_t phys = 0;
		if (copy_from_user(&reg, uarg, sizeof(reg)))
			return -EFAULT;

		ret = mc_register_wsm_mmu(instance,
			(void *)(uintptr_t)reg.buffer,
			reg.len, &reg.handle, &phys);
		reg.table_phys = phys;

		if (!ret) {
			if (copy_to_user(uarg, &reg, sizeof(reg))) {
				ret = -EFAULT;
				mc_unregister_wsm_mmu(instance, reg.handle);
			}
		}
		break;
	}
	case MC_IO_UNREG_WSM:
		ret = mc_unregister_wsm_mmu(instance, (uint32_t)arg);
		break;

	case MC_IO_VERSION:
		ret = put_user(mc_get_version(), uarg);
		if (ret)
			MCDRV_DBG_ERROR(mcd,
					"IOCTL_GET_VERSION failed to put data");
		break;

	case MC_IO_MAP_WSM:{
		struct mc_ioctl_map map;
		struct mc_buffer *buffer = 0;
		if (copy_from_user(&map, uarg, sizeof(map)))
			return -EFAULT;

		/* Setup the WSM buffer structure! */
		if (mc_get_buffer(instance, &buffer, map.len))
			return -EFAULT;

		map.handle = buffer->handle;
		/* Trick: to keep the same interface with the user space, store
		   the handle in the physical address.
		   It is given back with the offset when mmap() is called. */
		map.phys_addr = buffer->handle << PAGE_SHIFT;
		map.reused = 0;
		if (copy_to_user(uarg, &map, sizeof(map)))
			ret = -EFAULT;
		else
			ret = 0;
		break;
	}
	default:
		MCDRV_DBG_ERROR(mcd, "unsupported cmd=0x%x", cmd);
		ret = -ENOIOCTLCMD;
		break;

	} /* end switch(cmd) */

#ifdef MC_MEM_TRACES
	mobicore_log_read();
#endif

	return (int)ret;
}

static long mc_fd_admin_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	struct mc_instance *instance = get_instance(file);
	int __user *uarg = (int __user *)arg;
	int ret = -EINVAL;

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	if (WARN_ON(!is_daemon(instance))) {
		MCDRV_DBG_ERROR(mcd, "caller not MobiCore Daemon");
		return -EPERM;
	}

	if (ioctl_check_pointer(cmd, uarg))
		return -EFAULT;

	switch (cmd) {
	case MC_IO_INIT: {
		struct mc_ioctl_init init;
		ctx.mcp = NULL;
		if (!ctx.mci_base.phys) {
			MCDRV_DBG_ERROR(mcd,
					"Cannot init MobiCore without MCI!");
			return -EINVAL;
		}
		if (copy_from_user(&init, uarg, sizeof(init)))
			return -EFAULT;

		ctx.mcp = ctx.mci_base.addr + init.mcp_offset;
		ret = mc_init(ctx.mci_base.phys, init.nq_length,
			init.mcp_offset, init.mcp_length);
		break;
	}
	case MC_IO_INFO: {
		struct mc_ioctl_info info;
		if (copy_from_user(&info, uarg, sizeof(info)))
			return -EFAULT;

		ret = mc_info(info.ext_info_id, &info.state,
			&info.ext_info);

		if (!ret) {
			if (copy_to_user(uarg, &info, sizeof(info)))
				ret = -EFAULT;
		}
		break;
	}
	case MC_IO_YIELD:
		ret = mc_yield();
		break;

	case MC_IO_NSIQ:
		ret = mc_nsiq();
		break;

	case MC_IO_LOCK_WSM: {
		ret = mc_lock_handle(instance, (uint32_t)arg);
		break;
	}
	case MC_IO_UNLOCK_WSM:
		ret = mc_unlock_handle(instance, (uint32_t)arg);
		break;
	case MC_IO_CLEAN_WSM:
		ret = mc_clean_wsm_mmu(instance);
		break;
	case MC_IO_RESOLVE_WSM: {
		phys_addr_t phys;
		struct mc_ioctl_resolv_wsm wsm;
		if (copy_from_user(&wsm, uarg, sizeof(wsm)))
			return -EFAULT;
		phys = mc_find_wsm_mmu(instance, wsm.handle, wsm.fd);
		if (!phys)
			return -EINVAL;

		wsm.phys = phys;
		if (copy_to_user(uarg, &wsm, sizeof(wsm)))
			return -EFAULT;
		ret = 0;
		break;
	}
	case MC_IO_RESOLVE_CONT_WSM: {
		struct mc_ioctl_resolv_cont_wsm cont_wsm;
		phys_addr_t phys = 0;
		uint32_t len = 0;
		if (copy_from_user(&cont_wsm, uarg, sizeof(cont_wsm)))
			return -EFAULT;
		ret = mc_find_cont_wsm(instance, cont_wsm.handle, cont_wsm.fd,
					&phys, &len);
		if (!ret) {
			cont_wsm.phys = phys;
			cont_wsm.length = len;
			if (copy_to_user(uarg, &cont_wsm, sizeof(cont_wsm)))
				ret = -EFAULT;
		}
		break;
	}
	case MC_IO_MAP_MCI:{
		struct mc_ioctl_map map;
		phys_addr_t phys_addr;
		if (copy_from_user(&map, uarg, sizeof(map)))
			return -EFAULT;

		map.reused = (ctx.mci_base.phys != 0);
		phys_addr = get_mci_base_phys(map.len);
		if (!phys_addr) {
			MCDRV_DBG_ERROR(mcd, "Failed to setup MCI buffer!");
			return -EFAULT;
		}
		map.phys_addr = 0;
		if (copy_to_user(uarg, &map, sizeof(map)))
			ret = -EFAULT;
		ret = 0;
		break;
	}
	case MC_IO_LOG_SETUP: {
#ifdef MC_MEM_TRACES
		ret = mobicore_log_setup();
#endif
		break;
	}

	/* The rest is handled commonly by user IOCTL */
	default:
		ret = mc_fd_user_ioctl(file, cmd, arg);
	} /* end switch(cmd) */

#ifdef MC_MEM_TRACES
	mobicore_log_read();
#endif

	return (int)ret;
}

/*
 * mc_fd_read() - This will be called from user space as read(...)
 * @file:	file pointer
 * @buffer:	buffer where to copy to(userspace)
 * @buffer_len:	number of requested data
 * @pos:	not used
 *
 * The read function is blocking until a interrupt occurs. In that case the
 * event counter is copied into user space and the function is finished.
 *
 * If OK this function returns the number of copied data otherwise it returns
 * errno
 */
static ssize_t mc_fd_read(struct file *file, char *buffer, size_t buffer_len,
			  loff_t *pos)
{
	int ret = 0, ssiq_counter;
	struct mc_instance *instance = get_instance(file);

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	/* avoid debug output on non-error, because this is call quite often */
	MCDRV_DBG_VERBOSE(mcd, "enter");

	/* only the MobiCore Daemon is allowed to call this function */
	if (WARN_ON(!is_daemon(instance))) {
		MCDRV_DBG_ERROR(mcd, "caller not MobiCore Daemon");
		return -EPERM;
	}

	if (buffer_len < sizeof(unsigned int)) {
		MCDRV_DBG_ERROR(mcd, "invalid length");
		return -EINVAL;
	}

	for (;;) {
		if (wait_for_completion_interruptible(&ctx.isr_comp)) {
			MCDRV_DBG_VERBOSE(mcd, "read interrupted");
			return -ERESTARTSYS;
		}

		ssiq_counter = atomic_read(&ctx.isr_counter);
		MCDRV_DBG_VERBOSE(mcd, "ssiq_counter=%i, ctx.counter=%i",
				  ssiq_counter, ctx.evt_counter);

		if (ssiq_counter != ctx.evt_counter) {
			/* read data and exit loop without error */
			ctx.evt_counter = ssiq_counter;
			ret = 0;
			break;
		}

		/* end loop if non-blocking */
		if (file->f_flags & O_NONBLOCK) {
			MCDRV_DBG_ERROR(mcd, "non-blocking read");
			return -EAGAIN;
		}

		if (signal_pending(current)) {
			MCDRV_DBG_VERBOSE(mcd, "received signal.");
			return -ERESTARTSYS;
		}
	}

	/* read data and exit loop */
	ret = copy_to_user(buffer, &ctx.evt_counter, sizeof(unsigned int));

	if (ret != 0) {
		MCDRV_DBG_ERROR(mcd, "copy_to_user failed");
		return -EFAULT;
	}

	ret = sizeof(unsigned int);

	return (ssize_t)ret;
}

/*
 * Initialize a new mobicore API instance object
 *
 * @return Instance or NULL if no allocation was possible.
 */
struct mc_instance *mc_alloc_instance(void)
{
	struct mc_instance *instance;

	instance = kzalloc(sizeof(*instance), GFP_KERNEL);
	if (instance == NULL)
		return NULL;

	/* get a unique ID for this instance (PIDs are not unique) */
	instance->handle = atomic_inc_return(&ctx.instance_counter);

	mutex_init(&instance->lock);

	return instance;
}

#if defined(TBASE_CORE_SWITCHER) && defined(DEBUG)
static ssize_t mc_fd_write(struct file *file, const char __user *buffer,
			size_t buffer_len, loff_t *x)
{
	uint32_t cpu_new;
	/* we only consider one digit */
	char buf[2];
	struct mc_instance *instance = get_instance(file);

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	/* Invalid data, nothing to do */
	if (buffer_len < 1)
		return -EINVAL;

	/* Invalid data, nothing to do */
	if (copy_from_user(buf, buffer, min(sizeof(buf), buffer_len)))
		return -EFAULT;

	if (buf[0] == 'n') {
		mc_nsiq();
	/* If it's a digit then switch cores */
	} else if ((buf[0] >= '0') && (buf[0] <= '9')) {
		cpu_new = buf[0] - '0';
		if (cpu_new <= 8) {
			MCDRV_DBG_VERBOSE(mcd, "Set Active Cpu: %d\n", cpu_new);
			mc_switch_core(cpu_new);
		}
	} else {
		return -EINVAL;
	}

	return buffer_len;
}
#endif

/*
 * Release a mobicore instance object and all objects related to it
 * @instance:	instance
 * Returns 0 if Ok or -E ERROR
 */
int mc_release_instance(struct mc_instance *instance)
{
	struct mc_buffer *buffer, *tmp;

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	mutex_lock(&instance->lock);
	mc_clear_mmu_tables(instance);

	mutex_lock(&ctx.cont_bufs_lock);
	/* release all mapped data */

	/* Check if some buffers are orphaned. */
	list_for_each_entry_safe(buffer, tmp, &ctx.cont_bufs, list) {
		/* It's safe here to only call free_buffer() without unmapping
		 * because mmap() takes a refcount to the file's fd so only
		 * time we end up here is when everything has been unmapped or
		 * the process called exit() */
		if (buffer->instance == instance) {
			buffer->instance = NULL;
			free_buffer(buffer);
		}
	}
	mutex_unlock(&ctx.cont_bufs_lock);

	mutex_unlock(&instance->lock);

	/* release instance context */
	kfree(instance);

	return 0;
}

/*
 * mc_fd_user_open() - Will be called from user space as fd = open(...)
 * A set of internal instance data are created and initialized.
 *
 * @inode
 * @file
 * Returns 0 if OK or -ENOMEM if no allocation was possible.
 */
static int mc_fd_user_open(struct inode *inode, struct file *file)
{
	struct mc_instance *instance;

	MCDRV_DBG_VERBOSE(mcd, "enter");

	instance = mc_alloc_instance();
	if (instance == NULL)
		return -ENOMEM;

	/* store instance data reference */
	file->private_data = instance;

	return 0;
}

static int mc_fd_admin_open(struct inode *inode, struct file *file)
{
	struct mc_instance *instance;

	/*
	 * The daemon is already set so we can't allow anybody else to open
	 * the admin interface.
	 */
	if (ctx.daemon_inst) {
		MCDRV_DBG_ERROR(mcd, "Daemon is already connected");
		return -EPERM;
	}
	/* Setup the usual variables */
	if (mc_fd_user_open(inode, file))
		return -ENOMEM;
	instance = get_instance(file);

	MCDRV_DBG(mcd, "accept this as MobiCore Daemon");

	ctx.daemon_inst = instance;
	ctx.daemon = current;
	instance->admin = true;
	init_completion(&ctx.isr_comp);
	/* init ssiq event counter */
	ctx.evt_counter = atomic_read(&(ctx.isr_counter));

	return 0;
}

/*
 * mc_fd_release() - This function will be called from user space as close(...)
 * The instance data are freed and the associated memory pages are unreserved.
 *
 * @inode
 * @file
 *
 * Returns 0
 */
static int mc_fd_release(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct mc_instance *instance = get_instance(file);

	MCDRV_DBG_VERBOSE(mcd, "enter");

	if (WARN(!instance, "No instance data available"))
		return -EFAULT;

	/* check if daemon closes us. */
	if (is_daemon(instance)) {
		MCDRV_DBG_WARN(mcd, "MobiCore Daemon died");
		ctx.daemon_inst = NULL;
		ctx.daemon = NULL;
	}

	ret = mc_release_instance(instance);

	/*
	 * ret is quite irrelevant here as most apps don't care about the
	 * return value from close() and it's quite difficult to recover
	 */
	MCDRV_DBG_VERBOSE(mcd, "exit with %d/0x%08X", ret, ret);

	return (int)ret;
}

/*
 * This function represents the interrupt function of the mcDrvModule.
 * It signals by incrementing of an event counter and the start of the read
 * waiting queue, the read function a interrupt has occurred.
 */
static irqreturn_t mc_ssiq_isr(int intr, void *context)
{
	/* increment interrupt event counter */
	atomic_inc(&(ctx.isr_counter));

	/* signal the daemon */
	complete(&ctx.isr_comp);
#ifdef MC_MEM_TRACES
	mobicore_log_read();
#endif
	return IRQ_HANDLED;
}

#ifdef CONFIG_MT_TRUSTONIC_TEE_DEBUGFS
uint8_t trustonic_swd_debug;
static ssize_t debugfs_read(struct file *filep, char __user *buf, size_t len, loff_t *ppos)
{
	char mybuf[2];

	if (*ppos != 0)
		return 0;
	mybuf[0] = trustonic_swd_debug + '0';
	mybuf[1] = '\n';
	if (copy_to_user(buf, mybuf + *ppos, 2))
		return -EFAULT;
	*ppos = 2;
	return 2;
}

static ssize_t debugfs_write(struct file *filep, const char __user *buf, size_t len, loff_t *ppos)
{
	uint8_t val=0;

	if (len >=2) {
		if (!copy_from_user(&val, &buf[0], 1))
			if (val >= '0' && val <= '9') {
				trustonic_swd_debug = val - '0';
			}
		return len;
	}

	return -EFAULT;
}

const struct file_operations debug_fops = {
	.read = debugfs_read,
	.write = debugfs_write
};
#endif

/* function table structure of this device driver. */
static const struct file_operations mc_admin_fops = {
	.owner		= THIS_MODULE,
	.open		= mc_fd_admin_open,
	.release	= mc_fd_release,
	.unlocked_ioctl	= mc_fd_admin_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = mc_fd_admin_ioctl,
#endif
	.mmap		= mc_fd_mmap,
	.read		= mc_fd_read,
};

/* function table structure of this device driver. */
static const struct file_operations mc_user_fops = {
	.owner		= THIS_MODULE,
	.open		= mc_fd_user_open,
	.release	= mc_fd_release,
	.unlocked_ioctl	= mc_fd_user_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = mc_fd_user_ioctl,
#endif
	.mmap		= mc_fd_mmap,
#if defined(TBASE_CORE_SWITCHER) && defined(DEBUG)
	.write          = mc_fd_write,
#endif
};

static int create_devices(void)
{
	int ret = 0;

	cdev_init(&mc_admin_cdev, &mc_admin_fops);
	cdev_init(&mc_user_cdev, &mc_user_fops);

	mc_device_class = class_create(THIS_MODULE, "mobicore");
	if (IS_ERR(mc_device_class)) {
		MCDRV_DBG_ERROR(mcd, "failed to create device class");
		ret = PTR_ERR(mc_device_class);
		goto out;
	}

	ret = alloc_chrdev_region(&mc_dev_admin, 0, MC_DEV_MAX, "mobicore");
	if (ret < 0) {
		MCDRV_DBG_ERROR(mcd, "failed to allocate char dev region");
		goto error;
	}
	mc_dev_user = MKDEV(MAJOR(mc_dev_admin), 1);

	MCDRV_DBG_VERBOSE(mcd, "%s: dev %d", "mobicore", MAJOR(mc_dev_admin));

	/* First the ADMIN node */
	ret = cdev_add(&mc_admin_cdev,  mc_dev_admin, 1);
	if (ret != 0) {
		MCDRV_DBG_ERROR(mcd, "admin device register failed");
		goto error;
	}
	mc_admin_cdev.owner = THIS_MODULE;
	device_create(mc_device_class, NULL, mc_dev_admin, NULL,
		      MC_ADMIN_DEVNODE);

	/* Then the user node */

	ret = cdev_add(&mc_user_cdev, mc_dev_user, 1);
	if (ret != 0) {
		MCDRV_DBG_ERROR(mcd, "user device register failed");
		goto error_unregister;
	}
	mc_user_cdev.owner = THIS_MODULE;
	device_create(mc_device_class, NULL, mc_dev_user, NULL,
		      MC_USER_DEVNODE);

	goto out;
error_unregister:
	device_destroy(mc_device_class, mc_dev_admin);
	device_destroy(mc_device_class, mc_dev_user);

	cdev_del(&mc_admin_cdev);
	cdev_del(&mc_user_cdev);
	unregister_chrdev_region(mc_dev_admin, MC_DEV_MAX);
error:
	class_destroy(mc_device_class);
out:
	return ret;
}

/*
 * This function is called the kernel during startup or by a insmod command.
 * This device is installed and registered as cdev, then interrupt and
 * queue handling is set up
 */
static unsigned int mobicore_irq_id = MC_INTR_SSIQ;
static int __init mobicore_init(void)
{
	int ret = 0;
	dev_set_name(mcd, "mcd");
#ifdef CONFIG_MT_TRUSTONIC_TEE_DEBUGFS
	struct dentry *debug_root;
#endif
#ifdef CONFIG_OF
	struct device_node *node;
#if 0
	unsigned int irq_info[3] = {0, 0, 0};
#endif
#endif

	/* Do not remove or change the following trace.
	 * The string "MobiCore" is used to detect if <t-base is in of the image
	 */
	dev_info(mcd, "MobiCore Driver, Build: " "\n");
	dev_info(mcd, "MobiCore mcDrvModuleApi version is %i.%i\n",
		 MCDRVMODULEAPI_VERSION_MAJOR,
		 MCDRVMODULEAPI_VERSION_MINOR);
#ifdef MOBICORE_COMPONENT_BUILD_TAG
	dev_info(mcd, "MobiCore %s\n", MOBICORE_COMPONENT_BUILD_TAG);
#endif
	/* Hardware does not support ARM TrustZone -> Cannot continue! */
	if (!has_security_extensions()) {
		MCDRV_DBG_ERROR(mcd,
				"Hardware doesn't support ARM TrustZone!");
		return -ENODEV;
	}

	/* Running in secure mode -> Cannot load the driver! */
	if (is_secure_mode()) {
		MCDRV_DBG_ERROR(mcd, "Running in secure MODE!");
		return -ENODEV;
	}

	ret = mc_fastcall_init(&ctx);
	if (ret)
		goto error;

	init_completion(&ctx.isr_comp);

	/* initialize event counter for signaling of an IRQ to zero */
	atomic_set(&ctx.isr_counter, 0);

#ifdef CONFIG_OF
	node = of_find_compatible_node(NULL, NULL, "trustonic,mobicore");
#if 0
	if (of_property_read_u32_array(node, "interrupts", irq_info, ARRAY_SIZE(irq_info))) {
		MCDRV_DBG_ERROR(mcd,
				"Fail to get SSIQ id from device tree!");
		return -ENODEV;
	}
	mobicore_irq_id = irq_info[1];
#else
    mobicore_irq_id = irq_of_parse_and_map(node, 0);
#endif
	MCDRV_DBG_VERBOSE(mcd, "Interrupt from device tree is %d\n", mobicore_irq_id);
#endif

	/* set up S-SIQ interrupt handler ************************/
	ret = request_irq(mobicore_irq_id, mc_ssiq_isr, IRQF_TRIGGER_RISING,
			MC_ADMIN_DEVNODE, &ctx);
	if (ret != 0) {
		MCDRV_DBG_ERROR(mcd, "interrupt request failed");
		goto err_req_irq;
	}

#ifdef MC_PM_RUNTIME
	ret = mc_pm_initialize(&ctx);
	if (ret != 0) {
		MCDRV_DBG_ERROR(mcd, "Power Management init failed!");
		goto free_isr;
	}
#endif

	ret = create_devices();
	if (ret != 0)
		goto free_pm;

	ret = mc_init_mmu_tables();

#ifdef MC_CRYPTO_CLOCK_MANAGEMENT
	ret = mc_pm_clock_initialize();
#endif

	/*
	 * initialize unique number counters which we can use for
	 * handles. We start with 1 instead of 0.
	 */
	atomic_set(&ctx.handle_counter, 1);
	atomic_set(&ctx.instance_counter, 1);

	/* init list for contiguous buffers  */
	INIT_LIST_HEAD(&ctx.cont_bufs);

	/* init lock for the buffers list */
	mutex_init(&ctx.cont_bufs_lock);

	memset(&ctx.mci_base, 0, sizeof(ctx.mci_base));
	MCDRV_DBG(mcd, "initialized");
#ifdef CONFIG_MT_TRUSTONIC_TEE_DEBUGFS
	debug_root = debugfs_create_dir("trustonic", NULL);
	if (debug_root) {
		if (!debugfs_create_file("swd_debug", 0644, debug_root, NULL, &debug_fops)) {
			MCDRV_DBG_ERROR(mcd, "Create trustonic debugfs swd_debug failed!");
		}
	} else {
		MCDRV_DBG_ERROR(mcd, "Create trustonic debugfs directory failed!");
	}
#endif
	return 0;

free_pm:
#ifdef MC_PM_RUNTIME
	mc_pm_free();
free_isr:
#endif
	free_irq(MC_INTR_SSIQ, &ctx);
err_req_irq:
	mc_fastcall_destroy();
error:
	return ret;
}

/*
 * This function removes this device driver from the Linux device manager .
 */
static void __exit mobicore_exit(void)
{
	MCDRV_DBG_VERBOSE(mcd, "enter");
#ifdef MC_MEM_TRACES
	mobicore_log_free();
#endif

	mc_release_mmu_tables();

#ifdef MC_PM_RUNTIME
	mc_pm_free();
#endif

	device_destroy(mc_device_class, mc_dev_admin);
	device_destroy(mc_device_class, mc_dev_user);
	class_destroy(mc_device_class);
	unregister_chrdev_region(mc_dev_admin, MC_DEV_MAX);

	free_irq(mobicore_irq_id, &ctx);

	mc_fastcall_destroy();

#ifdef MC_CRYPTO_CLOCK_MANAGEMENT
	mc_pm_clock_finalize();
#endif

	MCDRV_DBG_VERBOSE(mcd, "exit");
}

bool mc_sleep_ready(void)
{
#ifdef MC_PM_RUNTIME
	return mc_pm_sleep_ready();
#else
	return true;
#endif
}

/* Linux Driver Module Macros */
module_init(mobicore_init);
module_exit(mobicore_exit);
MODULE_AUTHOR("Trustonic Limited");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MobiCore driver");
