/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/uaccess.h>
#include <linux/err.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/export.h>
#ifndef CONFIG_ARM64
#include "mm/dma.h"
#endif

#include <linux/vmalloc.h>
#include "ion_profile.h"
#include <linux/debugfs.h>
#include "ion_priv.h"
#include "ion_drv_priv.h"
#include "mtk/mtk_ion.h"
#include "mtk/ion_drv.h"
#ifdef CONFIG_MTK_M4U
#include "m4u_v2_ext.h"
#endif
#ifdef CONFIG_MTK_PSEUDO_M4U
#include <mach/pseudo_m4u.h>
#endif
#ifdef CONFIG_PM
#include <linux/fb.h>
#endif
#include <aee.h>
#include <mmprofile.h>
#include <mmprofile_function.h>

#define ION_FUNC_ENTER
#define ION_FUNC_LEAVE

/* #pragma GCC optimize ("O0") */
#define DEFAULT_PAGE_SIZE 0x1000
#define PAGE_ORDER 12

struct ion_device *g_ion_device;
EXPORT_SYMBOL(g_ion_device);

#ifndef dmac_map_area
#define dmac_map_area __dma_map_area
#endif
#ifndef dmac_unmap_area
#define dmac_unmap_area __dma_unmap_area
#endif

#ifndef dmac_flush_range
#define dmac_flush_range __dma_flush_range
#endif

#define __ION_CACHE_SYNC_USER_VA_EN__
static void __ion_cache_mmp_start(enum ION_CACHE_SYNC_TYPE sync_type,
				  unsigned int size, unsigned int start)
{
	switch (sync_type) {
	case ION_CACHE_CLEAN_BY_RANGE:
	case ION_CACHE_CLEAN_BY_RANGE_USE_PA:
		mmprofile_log_ex(ion_mmp_events[PROFILE_DMA_CLEAN_RANGE],
				 MMPROFILE_FLAG_START, size, start);
		break;
	case ION_CACHE_INVALID_BY_RANGE:
	case ION_CACHE_INVALID_BY_RANGE_USE_PA:
		mmprofile_log_ex(ion_mmp_events[PROFILE_DMA_INVALID_RANGE],
				 MMPROFILE_FLAG_START, size, start);
		break;
	case ION_CACHE_FLUSH_BY_RANGE:
	case ION_CACHE_FLUSH_BY_RANGE_USE_PA:
		mmprofile_log_ex(ion_mmp_events[PROFILE_DMA_FLUSH_RANGE],
				 MMPROFILE_FLAG_START, size, start);
		break;
	case ION_CACHE_CLEAN_ALL:
		mmprofile_log_ex(ion_mmp_events[PROFILE_DMA_CLEAN_ALL],
				 MMPROFILE_FLAG_START, 1, 1);
		break;
	case ION_CACHE_INVALID_ALL:
		mmprofile_log_ex(ion_mmp_events[PROFILE_DMA_INVALID_ALL],
				 MMPROFILE_FLAG_START, 1, 1);
		break;
	case ION_CACHE_FLUSH_ALL:
		mmprofile_log_ex(ion_mmp_events[PROFILE_DMA_FLUSH_ALL],
				 MMPROFILE_FLAG_START, 1, 1);
		break;
	default:
		pr_notice("%s invalid type(%d)\n", __func__, (int)sync_type);
	}
}

static void __ion_cache_mmp_end(enum ION_CACHE_SYNC_TYPE sync_type,
				unsigned int size)
{
	switch (sync_type) {
	case ION_CACHE_CLEAN_BY_RANGE:
	case ION_CACHE_CLEAN_BY_RANGE_USE_PA:
		mmprofile_log_ex(ion_mmp_events[PROFILE_DMA_CLEAN_RANGE],
				 MMPROFILE_FLAG_END, size, 0);
		break;
	case ION_CACHE_INVALID_BY_RANGE:
	case ION_CACHE_INVALID_BY_RANGE_USE_PA:
		mmprofile_log_ex(ion_mmp_events[PROFILE_DMA_INVALID_RANGE],
				 MMPROFILE_FLAG_END, size, 0);
		break;
	case ION_CACHE_FLUSH_BY_RANGE:
	case ION_CACHE_FLUSH_BY_RANGE_USE_PA:
		mmprofile_log_ex(ion_mmp_events[PROFILE_DMA_FLUSH_RANGE],
				 MMPROFILE_FLAG_END, size, 0);
		break;
	case ION_CACHE_CLEAN_ALL:
		mmprofile_log_ex(ion_mmp_events[PROFILE_DMA_CLEAN_ALL],
				 MMPROFILE_FLAG_END, 1, 1);
		break;
	case ION_CACHE_INVALID_ALL:
		mmprofile_log_ex(ion_mmp_events[PROFILE_DMA_INVALID_ALL],
				 MMPROFILE_FLAG_END, 1, 1);
		break;
	case ION_CACHE_FLUSH_ALL:
		mmprofile_log_ex(ion_mmp_events[PROFILE_DMA_FLUSH_ALL],
				 MMPROFILE_FLAG_END, 1, 1);
		break;
	default:
		pr_notice("%s invalid type(%d)\n", __func__, (int)sync_type);
	}
}

/* kernel va check
 * @return 0 : invalid va
 * @return 1 : valid kernel va
 */
static int __ion_is_kernel_va(unsigned long va, unsigned long size)
{
	int ret = 0;
	char data;

	if (unlikely(!va || !size))
		return 0;
	/* kernel space va check */
	if (va > TASK_SIZE) {
		if (probe_kernel_address((void *)va, data) ||
		    probe_kernel_address((void *)(va + size - 1), data)) {
			/* hole */
			ret = 0;
		} else {
			ret = 1;
		}
	}

	return ret;
}

/* user va check
 * @return 0 : invalid va
 * @return 1 : valid user va
 */
static int __ion_is_user_va(unsigned long va, unsigned long size)
{
	int ret = 0;
	char data;

	if (unlikely(!va || !size))
		return 0;
	if (va < TASK_SIZE) {
		/* user space va check */
		if (get_user(data, (char __user *)va) ||
		    get_user(data, (char __user *)(va + size - 1))) {
		    /* hole */
			ret = 0;
		} else {
			ret = 1;
		}
	}

	return ret;
}
static int __cache_sync_by_range(struct ion_client *client,
				 enum ION_CACHE_SYNC_TYPE sync_type,
				 unsigned long start, unsigned long size)
{
	int ret = 0;
	char ion_name[100];
	int is_user_addr;

	ret = __ion_is_user_va(start, size) ||
		  __ion_is_kernel_va(start, size);
	is_user_addr = __ion_is_user_va(start, size);
	if (!ret) {
		IONMSG("TASK_SIZE:0x%lx, PAGE_OFFSET:0x%lx\n",
		       (unsigned long)TASK_SIZE,
		       (unsigned long)PAGE_OFFSET);
		snprintf(ion_name, 100,
			 "[ION]CRDISPATCH_KEY(%s),(%d) sz/addr %lx/%lx\n",
			 (*client->dbg_name) ? client->dbg_name : client->name,
			 (unsigned int)current->pid,
			 size, start);
		IONMSG("%s %s\n", __func__, ion_name);

		aee_kernel_warning(ion_name, "[ION]");
		return -EFAULT;
	}

	__ion_cache_mmp_start(sync_type, size, start);

	switch (sync_type) {
	case ION_CACHE_CLEAN_BY_RANGE:
	case ION_CACHE_CLEAN_BY_RANGE_USE_PA:
		if (is_user_addr)
			__clean_dcache_user_area((void *)start, size);
		else
			__clean_dcache_area_poc((void *)start, size);
		break;
	case ION_CACHE_FLUSH_BY_RANGE:
	case ION_CACHE_FLUSH_BY_RANGE_USE_PA:
		if (is_user_addr)
			__flush_dcache_user_area((void *)start, size);
		else
			__flush_dcache_area((void *)start, size);
		break;
	case ION_CACHE_INVALID_BY_RANGE:
	case ION_CACHE_INVALID_BY_RANGE_USE_PA:
		if (is_user_addr)
			__inval_dcache_user_area((void *)start, size);
		else
			__inval_dcache_area((void *)start, size);
		break;
	default:
		aee_kernel_warning(
			"ION",
			"Pass wrong cache sync type. (%d):clt(%s)cache(%d)\n",
			(unsigned int)current->pid,
			client->dbg_name, sync_type);
		break;
	}

	__ion_cache_mmp_end(sync_type, size);

	return 0;
}

static long ion_sys_cache_sync(struct ion_client *client,
			       struct ion_sys_cache_sync_param *param,
			       int from_kernel)
{
	enum ION_CACHE_SYNC_TYPE sync_type = param->sync_type;
	struct ion_handle *kernel_handle;
	int ret = 0;
	unsigned long kernel_va;
	unsigned int kernel_size;
	unsigned long mva = 0;

	kernel_handle = ion_drv_get_handle(client, param->handle,
					   param->kernel_handle,
					   from_kernel);
	if (IS_ERR(kernel_handle)) {
		IONMSG("%s handle is invalid\n", __func__);
		return -EINVAL;
	}
	/* no use about handle, put here */
	ion_drv_put_kernel_handle(kernel_handle);

	if (sync_type < ION_CACHE_CLEAN_ALL) {
		/* range operation  */
		if (sync_type > ION_CACHE_FLUSH_BY_RANGE) {
		/* PA(means mva) mode, need map */
			mva = (unsigned long)param->va;
			ret = m4u_mva_map_kernel((unsigned int)mva,
						 param->size,
						 &kernel_va, &kernel_size);
			if (ret) {
				IONMSG("map kernel va fail!\n");
				m4u_mva_unmap_kernel(mva, param->size,
						     (unsigned long)&kernel_va);
				return -1;
			}
			param->va = (void *)kernel_va;
		}
		ret = __cache_sync_by_range(client, sync_type,
					    (unsigned long)param->va,
					    (size_t)param->size);
		if (ret < 0)
			IONMSG("__cache_sync_by_range FAIL!\n");

		/* range operation PA mode, unmap here */
		if (sync_type > ION_CACHE_FLUSH_BY_RANGE) {
			m4u_mva_unmap_kernel(mva, param->size,
					     (unsigned long)param->va);
		}
	} else {
		/* All cache operation, Disabled */
		char ion_name[100];

		snprintf(ion_name, 100,
			 "flush all CRDISPATCH_KEY(%s),(%d) sz/addr %lx/%lx\n",
			 (*client->dbg_name) ? client->dbg_name : client->name,
			 (unsigned int)current->pid,
			 (unsigned long)param->size,
			 (unsigned long)param->va);
		IONMSG("%s flush all,user check %s\n",
		       __func__, ion_name);
		ret = -EINVAL;

	}
	return ret;
}

int ion_sys_copy_client_name(const char *src, char *dst)
{
	int i;

	for (i = 0; i < ION_MM_DBG_NAME_LEN - 1; i++)
		dst[i] = src[i];

	dst[ION_MM_DBG_NAME_LEN - 1] = '\0';

	return 0;
}

/* only support kernel va */
static int ion_cache_sync_flush(unsigned long start, size_t size,
				enum ION_DMA_TYPE dma_type)
{
	mmprofile_log_ex(ion_mmp_events[PROFILE_DMA_FLUSH_RANGE],
			 MMPROFILE_FLAG_START, size, 0);
#ifdef CONFIG_ARM64
	__dma_flush_area((void *)start, size);
#else
	dmac_flush_range((void *)start, (void *)(start + size - 1));
#endif
	mmprofile_log_ex(ion_mmp_events[PROFILE_DMA_FLUSH_RANGE],
			 MMPROFILE_FLAG_END, size, 0);

	return 0;
}

/* ion_dma_op cache sync
 * here only support kernel va
 */
long ion_dma_op(struct ion_client *client, struct ion_dma_param *param,
		int from_kernel)
{
	unsigned long start;
	struct ion_handle *kernel_handle;
	enum ION_DMA_TYPE sync_type = param->dma_type;

	if (!from_kernel) {
		IONMSG("%s from user(%d)\n", __func__, from_kernel);
		return -EINVAL;
	}
	if (sync_type == ION_DMA_CACHE_FLUSH_ALL) {
		IONMSG("%s error FLUSH_ALL\n", __func__);
		return -EINVAL;
	}

	start = (unsigned long)param->va;
	kernel_handle = ion_drv_get_handle(client, param->handle,
					   param->kernel_handle, from_kernel);
	if (IS_ERR(kernel_handle)) {
		IONMSG("ion cache sync fail, user handle %d\n", param->handle);
		return -EINVAL;
	}

	if (sync_type == ION_DMA_MAP_AREA)
		ion_dma_map_area_va((void *)start,
				    (size_t)param->size,
				    param->dma_dir);
	else if (sync_type == ION_DMA_UNMAP_AREA)
		ion_dma_unmap_area_va((void *)start,
				      (size_t)param->size,
				      param->dma_dir);
	else if (sync_type == ION_DMA_FLUSH_BY_RANGE)
		ion_cache_sync_flush(start, (size_t)param->size, sync_type);

	ion_drv_put_kernel_handle(kernel_handle);

	return 0;
}

void ion_dma_map_area_va(void *start, size_t size, enum ION_DMA_DIR dir)
{
	if (dir == ION_DMA_FROM_DEVICE)
		dmac_map_area(start, size, DMA_FROM_DEVICE);
	else if (dir == ION_DMA_TO_DEVICE)
		dmac_map_area(start, size, DMA_TO_DEVICE);
	else if (dir == ION_DMA_BIDIRECTIONAL)
		dmac_map_area(start, size, DMA_BIDIRECTIONAL);
}

void ion_dma_unmap_area_va(void *start, size_t size, enum ION_DMA_DIR dir)
{
	if (dir == ION_DMA_FROM_DEVICE)
		dmac_unmap_area(start, size, DMA_FROM_DEVICE);
	else if (dir == ION_DMA_TO_DEVICE)
		dmac_unmap_area(start, size, DMA_TO_DEVICE);
	else if (dir == ION_DMA_BIDIRECTIONAL)
		dmac_unmap_area(start, size, DMA_BIDIRECTIONAL);
}

void ion_cache_flush_all(void)
{
	mmprofile_log_ex(ion_mmp_events[PROFILE_DMA_FLUSH_ALL],
			 MMPROFILE_FLAG_START, 1, 1);
	//IONMSG("[disabled]: ION cache flush all.\n");
	/* outer_clean_all(); */
	mmprofile_log_ex(ion_mmp_events[PROFILE_DMA_FLUSH_ALL],
			 MMPROFILE_FLAG_END, 1, 1);
}

/* check user va if it belongs to its user space
 * @return 0: not in itself userspace va
 * @return 1: is sin itself userspace va
 * @Note: @Function: access_ok
 *        reference from copy_from_user to check user space address validity
 */
int __is_user_va(struct ion_client *client, unsigned long va, unsigned long sz)
{
	int ret = 0;
	struct vm_area_struct *vma;

	down_read(&current->mm->mmap_sem);
	vma = find_vma(current->mm, va);
	/* error case:
	 * 1. no vma
	 * 2. start is ahead of vma->start
	 * 3. start + size is behind of vma-end
	 */
	if (!vma ||
	    va < vma->vm_start ||
	    (va + sz) > vma->vm_end ||
	    sz == 0) {
		ret = 0;
	} else {
		ret = 1;
	}
	up_read(&current->mm->mmap_sem);
	if (!ret) {
		IONMSG("%s sz/va invalid (%d):clt(%s)va(0x%lx)sz(0x%lx)\n",
		       __func__, (unsigned int)current->pid,
		       (*client->dbg_name) ? client->dbg_name : client->name,
		       va, sz);
	}
	return ret;
}
static long ion_sys_dma_op(struct ion_client *client,
			   struct ion_dma_param *param, int from_kernel)
{
	long ret = 0;

	switch (param->dma_type) {
	case ION_DMA_MAP_AREA:
	case ION_DMA_UNMAP_AREA:
	case ION_DMA_FLUSH_BY_RANGE:
		ion_dma_op(client, param, from_kernel);
		break;
	case ION_DMA_MAP_AREA_VA:
		ion_dma_map_area_va(param->va, (size_t)param->size,
				    param->dma_dir);
		break;
	case ION_DMA_UNMAP_AREA_VA:
		ion_dma_unmap_area_va(param->va, (size_t)param->size,
				      param->dma_dir);
		break;
	case ION_DMA_CACHE_FLUSH_ALL:
		//IONMSG("error: flush all\n");
		ion_cache_flush_all();
		break;
	case ION_DMA_FLUSH_BY_RANGE_USE_VA:
		ion_cache_sync_flush((unsigned long)param->va,
				     (size_t)param->size,
				     ION_DMA_FLUSH_BY_RANGE_USE_VA);
		break;
	default:
		IONMSG("[ion_dbg][%s]: Error. Invalid command.\n", __func__);
		ret = -EFAULT;
		break;
	}
	return ret;
}

static long ion_sys_ioctl(struct ion_client *client, unsigned int cmd,
			  unsigned long arg, int from_kernel)
{
	struct ion_sys_data param;
	long ret = 0;
	unsigned long ret_copy = 0;
	ion_phys_addr_t phy_addr;

	ION_FUNC_ENTER;
	if (from_kernel)
		param = *(struct ion_sys_data *)arg;
	else
		ret_copy =
		    copy_from_user(&param, (void __user *)arg,
				   sizeof(struct ion_sys_data));

	switch (param.sys_cmd) {
	case ION_SYS_CACHE_SYNC:
		ret =
		    ion_sys_cache_sync(client, &param.cache_sync_param,
				       from_kernel);
		break;
	case ION_SYS_GET_PHYS:
		{
			struct ion_handle *kernel_handle;

			phy_addr = param.get_phys_param.phy_addr;
			kernel_handle =
			    ion_drv_get_handle(
					client,
					param.get_phys_param.handle,
					param.get_phys_param.kernel_handle,
					from_kernel);
			if (IS_ERR(kernel_handle)) {
				IONMSG("ion_get_phys fail!\n");
				ret = -EINVAL;
				break;
			}

			if (ion_phys(client, kernel_handle, &phy_addr,
				     (size_t *)&param.get_phys_param.len) <
			    0) {
				param.get_phys_param.phy_addr = 0;
				param.get_phys_param.len = 0;
				IONMSG(" %s: Error. Cannot get PA.\n",
				       __func__);
				ret = -EFAULT;
			}
			param.get_phys_param.phy_addr = (unsigned int)phy_addr;
			ion_drv_put_kernel_handle(kernel_handle);
		}
		break;
	case ION_SYS_SET_CLIENT_NAME:
		ion_sys_copy_client_name(param.client_name_param.name,
					 client->dbg_name);
		break;
	case ION_SYS_DMA_OP:
		ion_sys_dma_op(client, &param.dma_param, from_kernel);
		break;
	default:
		IONMSG(
			"[%s]: Error. Invalid command(%d).\n",
			  __func__, param.sys_cmd);
		ret = -EFAULT;
		break;
	}
	if (from_kernel)
		*(struct ion_sys_data *)arg = param;
	else
		ret_copy =
		    copy_to_user((void __user *)arg, &param,
				 sizeof(struct ion_sys_data));
	ION_FUNC_LEAVE;
	return ret;
}

static long _ion_ioctl(struct ion_client *client, unsigned int cmd,
		       unsigned long arg, int from_kernel)
{
	long ret = 0;

	ION_FUNC_ENTER;
	switch (cmd) {
	case ION_CMD_SYSTEM:
		ret = ion_sys_ioctl(client, cmd, arg, from_kernel);
		break;
	case ION_CMD_MULTIMEDIA:
		ret = ion_mm_ioctl(client, cmd, arg, from_kernel);
		break;
	}
	ION_FUNC_LEAVE;
	return ret;
}

long ion_kernel_ioctl(struct ion_client *client, unsigned int cmd,
		      unsigned long arg)
{
	return _ion_ioctl(client, cmd, arg, 1);
}
EXPORT_SYMBOL(ion_kernel_ioctl);

static long ion_custom_ioctl(struct ion_client *client, unsigned int cmd,
			     unsigned long arg)
{
	return _ion_ioctl(client, cmd, arg, 0);
}

#ifdef CONFIG_PM
/* FB event notifier */
static int ion_fb_event(struct notifier_block *notifier, unsigned long event,
			void *data)
{
	struct fb_event *fb_event = data;
	int blank;

	if (event != FB_EVENT_BLANK)
		return NOTIFY_DONE;

	blank = *(int *)fb_event->data;

	switch (blank) {
	case FB_BLANK_UNBLANK:
		break;
	case FB_BLANK_NORMAL:
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
	case FB_BLANK_POWERDOWN:
		IONMSG("%s: + screen-off +\n", __func__);
		shrink_ion_by_scenario(1);
		IONMSG("%s: - screen-off -\n", __func__);
		break;
	default:
		return -EINVAL;
	}

	return NOTIFY_OK;
}

static struct notifier_block ion_fb_notifier_block = {
	.notifier_call = ion_fb_event,
	.priority = 1,		/* Just exceeding 0 for higher priority */
};
#endif

struct ion_heap *ion_mtk_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_heap *heap = NULL;

	switch ((int)heap_data->type) {
	case ION_HEAP_TYPE_MULTIMEDIA:
		heap = ion_mm_heap_create(heap_data);
		break;
	case ION_HEAP_TYPE_FB:
		heap = ion_fb_heap_create(heap_data);
		break;
	case ION_HEAP_TYPE_MULTIMEDIA_SEC:
		heap = ion_sec_heap_create(heap_data);
		break;
	default:
		heap = ion_heap_create(heap_data);
	}

	if (IS_ERR_OR_NULL(heap)) {
		IONMSG("%s: error creat heap %s type %d base %lu size %zu\n",
		       __func__, heap_data->name, heap_data->type,
		       heap_data->base, heap_data->size);
		return ERR_PTR(-EINVAL);
	}

	heap->name = heap_data->name;
	heap->id = heap_data->id;
	return heap;
}

void ion_mtk_heap_destroy(struct ion_heap *heap)
{
	if (!heap)
		return;

	switch ((int)heap->type) {
	case ION_HEAP_TYPE_MULTIMEDIA:
		ion_mm_heap_destroy(heap);
		break;
	case ION_HEAP_TYPE_FB:
		ion_fb_heap_destroy(heap);
		break;
	case ION_HEAP_TYPE_MULTIMEDIA_SEC:
		ion_sec_heap_destroy(heap);
		break;
	default:
		ion_heap_destroy(heap);
	}
}

int ion_drv_create_heap(struct ion_platform_heap *heap_data)
{
	struct ion_heap *heap;

	heap = ion_mtk_heap_create(heap_data);
	if (IS_ERR_OR_NULL(heap)) {
		IONMSG("%s: %d heap is err or null.\n", __func__,
		       heap_data->id);
		return PTR_ERR(heap);
	}
	heap->name = heap_data->name;
	heap->id = heap_data->id;
	ion_device_add_heap(g_ion_device, heap);

	IONMSG("%s: create heap: %s\n", __func__, heap->name);
	return 0;
}

int ion_device_destroy_heaps(struct ion_device *dev)
{
	struct ion_heap *heap, *tmp;

	down_write(&dev->lock);

	plist_for_each_entry_safe(heap, tmp, &dev->heaps, node) {
		plist_del((struct plist_node *)heap, &dev->heaps);
		ion_mtk_heap_destroy(heap);
	}

	up_write(&dev->lock);

	return 0;
}

/*for clients ion mm heap summary size*/
static int ion_clients_summary_show(struct seq_file *s, void *unused)
{
	struct ion_device *dev = g_ion_device;
	struct rb_node *n, *m;
	int buffer_size = 0;
	unsigned int id = 0;
	enum mtk_ion_heap_type cam_heap = ION_HEAP_TYPE_MULTIMEDIA_FOR_CAMERA;
	enum mtk_ion_heap_type mm_heap = ION_HEAP_TYPE_MULTIMEDIA;

	if (!down_read_trylock(&dev->lock))
		return 0;
	seq_printf(s, "%-16.s %-8.s %-8.s\n", "client_name", "pid", "size");
	seq_puts(s, "------------------------------------------\n");
	for (n = rb_first(&dev->clients); n; n = rb_next(n)) {
		struct ion_client *client =
		    rb_entry(n, struct ion_client, node);
		{
			mutex_lock(&client->lock);
			for (m = rb_first(&client->handles); m;
			     m = rb_next(m)) {
				struct ion_handle *handle =
				    rb_entry(m, struct ion_handle,
					     node);
				id = handle->buffer->heap->id;

				if ((id == mm_heap || id == cam_heap) &&
				    handle->buffer->handle_count != 0)
					buffer_size +=
					    (int)(handle->buffer->size) /
					    (handle->buffer->handle_count);
			}
			if (!buffer_size) {
				mutex_unlock(&client->lock);
				continue;
			}
			seq_printf(s, "%-16s %-8d %-8d\n", client->name,
				   client->pid, buffer_size);
			buffer_size = 0;
			mutex_unlock(&client->lock);
		}
	}

	seq_puts(s, "-------------------------------------------\n");
	up_read(&dev->lock);

	return 0;
}

static int ion_debug_client_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_clients_summary_show, inode->i_private);
}

static const struct file_operations debug_client_fops = {
	.open = ion_debug_client_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

#ifdef CONFIG_MTK_PSEUDO_M4U
struct device *g_iommu_device;
#endif
static int ion_drv_probe(struct platform_device *pdev)
{
	int i;
	struct ion_platform_data *pdata;
	unsigned int num_heaps;
#ifdef CONFIG_MTK_PSEUDO_M4U
	struct device *dev = &pdev->dev;

	if (!iommu_get_domain_for_dev(dev)) {
		IONMSG("%s, iommu is not ready, waiting\n", __func__);
		return -EPROBE_DEFER;
	}
	pdata = (struct ion_platform_data *)of_device_get_match_data(dev);
#else

	pdata = pdev->dev.platform_data;
#endif

	num_heaps = pdata->nr;
	g_ion_device = ion_device_create(ion_custom_ioctl);
	if (IS_ERR_OR_NULL(g_ion_device)) {
		IONMSG("ion_device_create() error! device=%p\n", g_ion_device);
		return PTR_ERR(g_ion_device);
	}

	/* create the heaps as specified in the board file */
	for (i = 0; i < num_heaps; i++) {
		struct ion_platform_heap *heap_data = &pdata->heaps[i];
		struct ion_heap *heap;

		if (heap_data->type == ION_HEAP_TYPE_CARVEOUT &&
		    heap_data->base == 0) {
			/* reserve for carveout heap failed */
			heap_data->size = 0;
			continue;
		}

		heap = ion_mtk_heap_create(heap_data);

		if (IS_ERR_OR_NULL(heap))
			continue;

		ion_device_add_heap(g_ion_device, heap);
	}

#ifdef CONFIG_MTK_PSEUDO_M4U
	g_iommu_device = dev;
#endif
	platform_set_drvdata(pdev, g_ion_device);
	#ifdef CONFIG_XEN
	g_ion_device->dev.this_device->archdata.dev_dma_ops = NULL;
	#endif
	arch_setup_dma_ops(g_ion_device->dev.this_device, 0, 0, NULL, false);
	/* debugfs_create_file("ion_profile", 0644, g_ion_device->debug_root,*/
	/*  NULL, &debug_profile_fops); */
	debugfs_create_file("clients_summary", 0644,
			    g_ion_device->clients_debug_root, NULL,
			    &debug_client_fops);
	debugfs_create_symlink("ion_mm_heap", g_ion_device->debug_root,
			       "./heaps/ion_mm_heap");

	ion_history_init();
	ion_profile_init();

	return 0;
}

int ion_drv_remove(struct platform_device *pdev)
{
	struct ion_device *idev = platform_get_drvdata(pdev);

	ion_device_destroy_heaps(idev);
	ion_device_destroy(idev);
	return 0;
}

static struct ion_platform_heap ion_drv_platform_heaps[] = {
	{
	 .type = (unsigned int)ION_HEAP_TYPE_SYSTEM_CONTIG,
	 .id = ION_HEAP_TYPE_SYSTEM_CONTIG,
	 .name = "ion_system_contig_heap",
	 .base = 0,
	 .size = 0,
	 .align = 0,
	 .priv = NULL,
	 },
	{
	 .type = (unsigned int)ION_HEAP_TYPE_MULTIMEDIA,
	 .id = ION_HEAP_TYPE_MULTIMEDIA,
	 .name = "ion_mm_heap",
	 .base = 0,
	 .size = 0,
	 .align = 0,
	 .priv = NULL,
	 },
	{
	 .type = (unsigned int)ION_HEAP_TYPE_MULTIMEDIA,
	 .id = ION_HEAP_TYPE_MULTIMEDIA_FOR_CAMERA,
	 .name = "ion_mm_heap_for_camera",
	 .base = 0,
	 .size = 0,
	 .align = 0,
	 .priv = NULL,
	 },
	{
	 .type = (unsigned int)ION_HEAP_TYPE_MULTIMEDIA_SEC,
	 .id = ION_HEAP_TYPE_MULTIMEDIA_SEC,
	 .name = "ion_sec_heap",
	 .base = 0,
	 .size = 0,
	 .align = 0,
	 .priv = NULL,
	 },
	{
	.type = (unsigned int)ION_HEAP_TYPE_MULTIMEDIA_SEC,
	.id = ION_HEAP_TYPE_MULTIMEDIA_PROT,
	.name = "ion_sec_heap_protected",
	.base = 0,
	.size = 0,
	.align = 0,
	.priv = NULL,
	},
	{
	.type = (unsigned int)ION_HEAP_TYPE_MULTIMEDIA_SEC,
	.id = ION_HEAP_TYPE_MULTIMEDIA_2D_FR,
	.name = "ion_sec_heap_2d_fr",
	.base = 0,
	.size = 0,
	.align = 0,
	.priv = NULL,
	},
	{
	 .type = (unsigned int)ION_HEAP_TYPE_MULTIMEDIA_SEC,
	 .id = ION_HEAP_TYPE_MULTIMEDIA_WFD,
	 .name = "ion_sec_heap_wfd",
	 .base = 0,
	 .size = 0,
	 .align = 0,
	 .priv = NULL,
	 },
	{
	 .type = (unsigned int)ION_HEAP_TYPE_MULTIMEDIA_SEC,
	 .id = ION_HEAP_TYPE_MULTIMEDIA_HAPP,
	 .name = "ion_sec_heap_happ",
	 .base = 0,
	 .size = 0,
	 .align = 0,
	 .priv = NULL,
	 },
	{
	 .type = (unsigned int)ION_HEAP_TYPE_MULTIMEDIA_SEC,
	 .id = ION_HEAP_TYPE_MULTIMEDIA_HAPP_EXTRA,
	 .name = "ion_sec_heap_happ_mem",
	 .base = 0,
	 .size = 0,
	 .align = 0,
	 .priv = NULL,
	 },
	{
	 .type = (unsigned int)ION_HEAP_TYPE_MULTIMEDIA_SEC,
	 .id = ION_HEAP_TYPE_MULTIMEDIA_SDSP,
	 .name = "ion_sec_heap_sdsp",
	 .base = 0,
	 .size = 0,
	 .align = 0,
	 .priv = NULL,
	 },
	{
	 .type = (unsigned int)ION_HEAP_TYPE_MULTIMEDIA_SEC,
	 .id = ION_HEAP_TYPE_MULTIMEDIA_SDSP_SHARED,
	 .name = "ion_sec_heap_sdsp_shared",
	 .base = 0,
	 .size = 0,
	 .align = 0,
	 .priv = NULL,
	 },
	{
	 .type = (unsigned int)ION_HEAP_TYPE_MULTIMEDIA,
	 .id = ION_HEAP_TYPE_MULTIMEDIA_MAP_MVA,
	 .name = "ion_mm_heap_for_va2mva",
	 .base = 0,
	 .size = 0,
	 .align = 0,
	 .priv = NULL,
	 },
	{
	 .type = (unsigned int)ION_HEAP_TYPE_MULTIMEDIA,
	 .id = ION_HEAP_TYPE_MULTIMEDIA_PA2MVA,
	 .name = "ion_mm_heap_for_pa2mva",
	 .base = 0,
	 .size = 0,
	 .align = 0,
	 .priv = NULL,
	 },
	{
	 .type = (unsigned int)ION_HEAP_TYPE_CARVEOUT,
	 .id = ION_HEAP_TYPE_CARVEOUT,
	 .name = "ion_carveout_heap",
	 .base = 0,
	 .size = 0x4000, /* reserve size, align to Mbytes; */
	 .align = 0x1000, /* this must not be 0 if enable */
	 .priv = NULL,
	 },
};

struct ion_platform_data ion_drv_platform_data = {
	.nr = ARRAY_SIZE(ion_drv_platform_heaps),
	.heaps = ion_drv_platform_heaps,
};

#ifdef CONFIG_MTK_PSEUDO_M4U
static const struct of_device_id mtk_ion_match_table[] = {
	{.compatible = "mediatek,ion", .data = &ion_drv_platform_data},
	{},
};
#endif
static struct platform_driver ion_driver = {
	.probe = ion_drv_probe,
	.remove = ion_drv_remove,
	.driver = {
		.name = "ion-drv",
#ifdef CONFIG_MTK_PSEUDO_M4U
		.of_match_table = mtk_ion_match_table,
#endif
	}
};

#ifndef CONFIG_MTK_PSEUDO_M4U
static struct platform_device ion_device = {
	.name = "ion-drv",
	.id = 0,
	.dev = {
		.platform_data = &ion_drv_platform_data,
		},

};
#endif
#include <linux/of_reserved_mem.h>
static int __init ion_reserve_memory_to_camera(
	struct reserved_mem *mem)
{
	int i;

	for (i = 0; i < ion_drv_platform_data.nr; i++) {
		if (ion_drv_platform_data.heaps[i].id ==
		    ION_HEAP_TYPE_CARVEOUT) {
			ion_drv_platform_data.heaps[i].base =
				mem->base;
			ion_drv_platform_data.heaps[i].size =
				mem->size;
		}
	}
	pr_info("%s: name:%s,base:%llx,size:0x%llx\n",
		__func__, mem->name,
		(unsigned long long)mem->base,
		(unsigned long long)mem->size);
	return 0;
}

RESERVEDMEM_OF_DECLARE(ion_camera_reserve,
		       "mediatek,ion-carveout-heap",
		       ion_reserve_memory_to_camera);

static int __init ion_init(void)
{
	IONMSG("%s()\n", __func__);
#ifndef CONFIG_MTK_PSEUDO_M4U
	if (platform_device_register(&ion_device)) {
		IONMSG("%s platform device register failed.\n", __func__);
		return -ENODEV;
	}
#endif
	if (platform_driver_register(&ion_driver)) {
#ifndef CONFIG_MTK_PSEUDO_M4U
		platform_device_unregister(&ion_device);
#endif
		IONMSG("%s platform driver register failed.\n", __func__);
		return -ENODEV;
	}

#ifdef CONFIG_PM
	if (!fb_register_client(&ion_fb_notifier_block))
		IONMSG("%s fd register notifer fail\n", __func__);
#endif
	return 0;
}

static void __exit ion_exit(void)
{
	IONMSG("%s()\n", __func__);
	platform_driver_unregister(&ion_driver);
#ifndef CONFIG_MTK_PSEUDO_M4U
	platform_device_unregister(&ion_device);
#endif
}

fs_initcall(ion_init);
__exitcall(ion_exit);
/*module_exit(ion_exit);*/
