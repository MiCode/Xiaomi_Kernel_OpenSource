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
#ifdef CONFIG_PM
#include <linux/fb.h>
#endif

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
//#define ION_CACHE_SYNC_ALL_REDIRECTION_SUPPORT

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
static int __ion_is_kernel_va(unsigned long va, size_t size)
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
static int __ion_is_user_va(unsigned long va, size_t size)
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
				 unsigned long start, size_t size)
{
	int ret = 0;
	char ion_name[100];
	int is_user_addr;

	is_user_addr = __ion_is_user_va(start, size);
	ret = is_user_addr || __ion_is_kernel_va(start, size);

	if (!ret) {
		IONMSG("TASK_SIZE:0x%lx, PAGE_OFFSET:0x%lx\n",
		       (unsigned long)TASK_SIZE,
		       (unsigned long)PAGE_OFFSET);
		snprintf(ion_name, 100,
			 "[ION]CRDISPATCH_KEY(%s),(%d) sz/addr %zx/%lx",
			 (*client->dbg_name) ? client->dbg_name : client->name,
			 (unsigned int)current->pid,
			 size, start);
		IONMSG("%s %s\n", __func__, ion_name);
		aee_kernel_warning(ion_name, "[ION]: Wrong Address Range");
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

#ifndef CONFIG_ARM64
static struct vm_struct *cache_map_vm_st;
static int ion_cache_sync_init(void)
{
	cache_map_vm_st = get_vm_area(PAGE_SIZE, VM_ALLOC);
	if (!cache_map_vm_st)
		return -ENOMEM;

	return 0;
}

static void *ion_cache_map_page_va(struct page *page)
{
	int ret;
	struct page **ppage = &page;

	ret = map_vm_area(cache_map_vm_st, PAGE_KERNEL, ppage);
	if (ret) {
		IONMSG("error to map page\n");
		return NULL;
	}
	return cache_map_vm_st->addr;
}

static void ion_cache_unmap_page_va(unsigned long va)
{
	unmap_kernel_range((unsigned long)cache_map_vm_st->addr, PAGE_SIZE);
}

/* lock to protect cache_map_vm_st */
static DEFINE_MUTEX(ion_cache_sync_lock);

/* ion_sys_cache_sync_buf
 * cache sync full ion buffer by sg table
 * only used in ARM32 bit project,
 * for ARM64 bit, use ion_map_kernel to sync at once
 */
static int ion_sys_cache_sync_buf(struct ion_client *client,
				  enum ION_CACHE_SYNC_TYPE sync_type,
				  struct ion_handle *handle)
{
	struct ion_buffer *buffer;
	struct scatterlist *sg;
	int i, j;
	struct sg_table *table = NULL;
	int npages = 0;
	int npages_this_entry;
	struct page *page = NULL;
	unsigned long start = 0;
	int ret = 0;

	mutex_lock(&client->lock);
	buffer = handle->buffer;
	table = buffer->sg_table;
	npages = PAGE_ALIGN(buffer->size) / PAGE_SIZE;

	mutex_lock(&ion_cache_sync_lock);
	if (!cache_map_vm_st) {
		IONMSG(" warn: vm_struct retry\n");
		ion_cache_sync_init();

		if (!cache_map_vm_st) {
			IONMSG("error: vm_struct is NULL!\n");
			ret = -ENOMEM;
			goto out;
		}
	}

	for_each_sg(table->sgl, sg, table->nents, i) {
		npages_this_entry = PAGE_ALIGN(sg->length) / PAGE_SIZE;
		page = sg_page(sg);

		if (unlikely(i >= npages)) {
			IONMSG("err: pg=%d, npg=%d\n", i, npages);
			break;
		}

		for (j = 0; j < npages_this_entry; j++) {
			start = (unsigned long)ion_cache_map_page_va(page++);

			if (IS_ERR_OR_NULL((void *)start)) {
				IONMSG("can't cache sync: %lu\n", start);
				ret = -ENOMEM;
				goto out;
			}
			__cache_sync_by_range(
				client, sync_type, start, PAGE_SIZE);
			ion_cache_unmap_page_va(start);
		}
	}

out:
	mutex_unlock(&ion_cache_sync_lock);
	mutex_unlock(&client->lock);
	return ret;
}
#endif
static long ion_sys_cache_sync(struct ion_client *client,
			       struct ion_sys_cache_sync_param *param,
			       int from_kernel)
{
	enum ION_CACHE_SYNC_TYPE sync_type = param->sync_type;
	size_t sync_size = 0;
	unsigned long sync_va = 0;
	struct ion_handle *kernel_handle;
	struct ion_buffer *buffer;
	int ion_need_unmap_flag = 0;
	int ret = 0;
	unsigned long kernel_va = 0;
	unsigned int kernel_size = 0;

	/* Get kernel handle
	 * For cache sync all cases, some users
	 *     don't send valid hanlde, return error here.
	 */
	kernel_handle = ion_drv_get_handle(client, param->handle,
					   param->kernel_handle,
					   from_kernel);
	if (IS_ERR(kernel_handle)) {
		IONMSG("%s hdl inv[kernel %d][hdl %d-%p] clt[%s]\n",
		       __func__, from_kernel,
		       param->handle, param->kernel_handle,
		       (*client->dbg_name) ?
		       client->dbg_name : client->name);
		return -EINVAL;
	}

	buffer = kernel_handle->buffer;
	sync_va = (unsigned long)param->va;
	sync_size = param->size;

	switch (sync_type) {
	case ION_CACHE_CLEAN_BY_RANGE:
	case ION_CACHE_INVALID_BY_RANGE:
	case ION_CACHE_FLUSH_BY_RANGE:

#ifdef ION_CACHE_SYNC_ALL_REDIRECTION_SUPPORT
	/* Users call cache sync all with valid handle,
	 *     only do cache sync with its buffer.
	 */
	case ION_CACHE_CLEAN_ALL:
	case ION_CACHE_INVALID_ALL:
	case ION_CACHE_FLUSH_ALL:
#endif
		if (sync_size == 0 || sync_va == 0) {
			/* whole buffer cache sync
			 * get sync_va and sync_size here
			 */
			sync_size = (unsigned int)buffer->size;

			if (buffer->kmap_cnt != 0) {
				sync_va = (unsigned long)buffer->vaddr;
			} else {
				/* Do kernel map and do cache sync
				 * 32bit project, vmap space is small,
				 *    4MB as a boundary for mapping.
				 * 64bit vmap space is huge
				 */
#ifdef CONFIG_ARM64
				sync_va = (unsigned long)
					  ion_map_kernel(client, kernel_handle);
				ion_need_unmap_flag = 1;
#else
				if (sync_size <= SZ_4M) {
					sync_va = (unsigned long)
					ion_map_kernel(client, kernel_handle);
					ion_need_unmap_flag = 1;
				} else {
					ret =
					ion_sys_cache_sync_buf(client,
							       sync_type,
							       kernel_handle);
					goto out;
				}
#endif
			}
		}
			break;

	/* range PA(means mva) mode, need map
	 * NOTICE: m4u_mva_map_kernel only support m4u0
	 */
	case ION_CACHE_CLEAN_BY_RANGE_USE_PA:
	case ION_CACHE_INVALID_BY_RANGE_USE_PA:
	case ION_CACHE_FLUSH_BY_RANGE_USE_PA:
		ret = m4u_mva_map_kernel(
				(unsigned int)sync_va,
				sync_size, &kernel_va, &kernel_size);
		if (ret)
			goto err;
		sync_va = kernel_va;
		break;

	default:
		ret = -EINVAL;
		goto err;
	}

	ret = __cache_sync_by_range(client, sync_type,
				    sync_va, sync_size);
	if (ret < 0)
		goto err;

	/* range operation PA mode, unmap here */
	if (sync_type == ION_CACHE_CLEAN_BY_RANGE_USE_PA ||
	    sync_type == ION_CACHE_INVALID_BY_RANGE_USE_PA ||
	    sync_type == ION_CACHE_FLUSH_BY_RANGE_USE_PA) {
		m4u_mva_unmap_kernel((unsigned long)param->va,
				     (unsigned int)sync_size, sync_va);
	} else if (ion_need_unmap_flag) {
		ion_unmap_kernel(client, kernel_handle);
		ion_need_unmap_flag = 0;
	}

#ifndef CONFIG_ARM64
out:
#endif
	ion_drv_put_kernel_handle(kernel_handle);
	return ret;

err:
	IONMSG("%s sync[%d] err[k%d][hdl %d-%p][addr %p][sz:%d] clt[%s]\n",
	       __func__, sync_type, from_kernel,
	       param->handle, param->kernel_handle, param->va,
	       param->size, (*client->dbg_name) ?
	       client->dbg_name : client->name);
	ion_drv_put_kernel_handle(kernel_handle);
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
	size_t size;
	struct ion_handle *kernel_handle;
	enum ION_DMA_TYPE sync_type = param->dma_type;
	int ret = 0;

	if (!from_kernel) {
		IONMSG("%s from user(%d)\n", __func__, from_kernel);
		return -EINVAL;
	}
	if (sync_type == ION_DMA_CACHE_FLUSH_ALL) {
		IONMSG("%s error FLUSH_ALL\n", __func__);
		return -EINVAL;
	}

	start = (unsigned long)param->va;
	size = (size_t)param->size;
	kernel_handle = ion_drv_get_handle(client, param->handle,
					   param->kernel_handle, from_kernel);
	if (IS_ERR(kernel_handle)) {
		IONMSG("ion cache sync fail, user handle %d\n", param->handle);
		return -EINVAL;
	}

	ret = __ion_is_kernel_va(start, size);
	if (unlikely(ret == 0)) {
		IONMSG("%s va inv-kernel %d-hdl %d-%p-va%lx-sz%zu-clt[%s]\n",
		       __func__, from_kernel, param->handle,
		       param->kernel_handle, start, size,
		       (*client->dbg_name) ? client->dbg_name : client->name);
		ret = -EINVAL;
		goto out;
	}

	if (sync_type == ION_DMA_MAP_AREA)
		ion_dma_map_area_va((void *)start,
				    size,
				    param->dma_dir);
	else if (sync_type == ION_DMA_UNMAP_AREA)
		ion_dma_unmap_area_va((void *)start,
				      size,
				      param->dma_dir);
	else if (sync_type == ION_DMA_FLUSH_BY_RANGE)
		ion_cache_sync_flush(start, size, sync_type);

out:
	ion_drv_put_kernel_handle(kernel_handle);

	return ret;
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
		//ion_cache_flush_all();
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
				IONMSG("ION_PHYS:err handle %s(%s),%d, k:%d\n",
				       client->name, client->dbg_name,
				       client->pid, from_kernel);
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
				    (handle->buffer->handle_count) != 0)
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

static int ion_drv_probe(struct platform_device *pdev)
{
	int i;
	struct ion_platform_data *pdata = pdev->dev.platform_data;
	unsigned int num_heaps = pdata->nr;

	IONMSG("ion_drv_probe() heap_nr=%d\n", pdata->nr);
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

	platform_set_drvdata(pdev, g_ion_device);

	g_ion_device->dev.this_device->archdata.dma_ops = NULL;
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
	 .name = "ion_sec_heap_prot",
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
	 .size = 0,		/* reserve size, align to Mbytes; */
	 .align = 0x1000,	/* this must not be 0 if enable */
	 .priv = NULL,
	 },
};

struct ion_platform_data ion_drv_platform_data = {
	.nr = ARRAY_SIZE(ion_drv_platform_heaps),
	.heaps = ion_drv_platform_heaps,
};

static struct platform_driver ion_driver = {
		.probe = ion_drv_probe,
		.remove = ion_drv_remove,
		.driver = {
				.name = "ion-drv"
		}
};

static struct platform_device ion_device = {
	.name = "ion-drv",
	.id = 0,
	.dev = {
		.platform_data = &ion_drv_platform_data,
		},

};

static int __init ion_init(void)
{
	IONMSG("ion_init()\n");
	if (platform_device_register(&ion_device)) {
		IONMSG("%s platform device register failed.\n", __func__);
		return -ENODEV;
	}
	if (platform_driver_register(&ion_driver)) {
		platform_device_unregister(&ion_device);
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
	IONMSG("ion_exit()\n");
	platform_driver_unregister(&ion_driver);
	platform_device_unregister(&ion_device);
}

fs_initcall(ion_init);
__exitcall(ion_exit);
/*module_exit(ion_exit);*/
