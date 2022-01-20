// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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
#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#endif
#if IS_ENABLED(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#endif
#include <linux/seq_file.h>
#include <linux/dma-buf.h>

#include "ion_priv.h"
#include "ion_drv_priv.h"
#include "mtk/mtk_ion.h"
#include "mtk/ion_drv.h"

//tablet
#ifdef CONFIG_MTK_IOMMU
#include "pseudo_m4u.h"
#endif
//smart phone m4u
#ifdef CONFIG_MTK_M4U
#include "m4u_v2_ext.h"
#endif
//smart phone iommu
#ifdef CONFIG_MTK_IOMMU_V2
#include <mach/pseudo_m4u.h>
#endif

#ifdef CONFIG_PM
#include <linux/fb.h>
#endif
#include <aee.h>
//#include <mmprofile.h>
//#include <mmprofile_function.h>

#define ION_FUNC_ENTER
#define ION_FUNC_LEAVE

/* #pragma GCC optimize ("O0") */
#define DEFAULT_PAGE_SIZE 0x1000
#define PAGE_ORDER 12

#define ION_CACHEOPS_IS_WRITE(type)			\
	((type) == ION_CACHE_CLEAN_BY_RANGE ||		\
	 (type) == ION_CACHE_CLEAN_BY_RANGE_USE_PA ||	\
	 (type) == ION_CACHE_CLEAN_ALL ||		\
	 (type) == ION_CACHE_FLUSH_BY_RANGE ||		\
	 (type) == ION_CACHE_FLUSH_BY_RANGE_USE_PA ||	\
	 (type) == ION_CACHE_FLUSH_ALL)
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

/*
 * vma info check
 * @return 0 : vma from non-ion
 * @return 1 : vma from ion
 */
static int vma_is_ion_node(struct vm_area_struct *vma)
{
	struct dma_buf *dmabuf;

	if (unlikely(!vma))
		return 0;

	dmabuf = vma->vm_private_data;

	if (dmabuf && dmabuf->exp_name)
		return !strcmp(dmabuf->exp_name, "ion");
	return 0;
}

/* user va range check
 * @return 0: check fail
 * @return 1: check pass
 */
static int ion_check_user_va(unsigned long va, size_t size)
{
	struct vm_area_struct *vma;
	unsigned long va_start = va;
	unsigned long va_end;
	int ret = 0;

	va_end = va_start + size;

	/* overflow check */
	if (unlikely(va_end < va_start))
		return 0;

	down_read(&current->mm->mmap_sem);
	vma = find_vma(current->mm, va_start);
	if (!vma || va_start < vma->vm_start ||
	    va_end > vma->vm_end) {
		ret = 0;
	} else {
		ret = vma_is_ion_node(vma);
	}
	up_read(&current->mm->mmap_sem);

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

	/* overflow check */
	if (unlikely(va + size < va))
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

	/* add more check */
	if (ret)
		ret = ion_check_user_va(va, size);

	return ret;
}

static int __cache_sync_by_range(struct ion_client *client,
				 enum ION_CACHE_SYNC_TYPE sync_type,
				 unsigned long start, size_t size,
				 int is_kernel_addr)
{
	char ion_name[200];
	int ret = 0;

	/* for minimum change, here do nothing for kernel flow
	 * when we need check kernel flow, also need check source and valid
	 * such as "if (!is_kernel_addr)"
	 */
	if (sync_type == ION_CACHE_CLEAN_BY_RANGE_USE_PA ||
	    sync_type == ION_CACHE_INVALID_BY_RANGE_USE_PA ||
	    sync_type == ION_CACHE_FLUSH_BY_RANGE_USE_PA ||
	    is_kernel_addr)
		goto start_sync;

	/* userspace va check */
	ret  = __ion_is_user_va(start, size);
	if (!ret) {
		scnprintf(ion_name, 199,
			  "CRDISPATCH_KEY(%s),(%d) sz/addr %zx/%lx is_kernel_addr:%d",
			  (*client->dbg_name) ?
			  client->dbg_name : client->name,
			  (unsigned int)current->pid, size, start, is_kernel_addr);
		IONMSG("%s %s\n", __func__, ion_name);
		//aee_kernel_warning(ion_name, "[ION]: Wrong Address Range");
		return -EFAULT;
	}

start_sync:

	__ion_cache_mmp_start(sync_type, size, start);

	switch (sync_type) {
	case ION_CACHE_CLEAN_BY_RANGE:
	case ION_CACHE_CLEAN_BY_RANGE_USE_PA:
		if (!is_kernel_addr)
			__clean_dcache_user_area((void *)start, size);
		else
			__clean_dcache_area_poc((void *)start, size);
		break;
	case ION_CACHE_FLUSH_BY_RANGE:
	case ION_CACHE_FLUSH_BY_RANGE_USE_PA:
		if (!is_kernel_addr)
			__flush_dcache_user_area((void *)start, size);
		else
			__flush_dcache_area((void *)start, size);
		break;
	case ION_CACHE_INVALID_BY_RANGE:
	case ION_CACHE_INVALID_BY_RANGE_USE_PA:
		if (!is_kernel_addr)
			__inval_dcache_user_area((void *)start, size);
		else
			__inval_dcache_area((void *)start, size);
		break;
	default:
		IONMSG("%s err type. (%d):clt(%s)cache(%d)\n",
		       __func__, (unsigned int)current->pid,
		       client->dbg_name, sync_type);
		dump_stack();
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
			__cache_sync_by_range(client, sync_type,
					      start, PAGE_SIZE, true);
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
#ifdef CONFIG_MTK_PSEUDO_M4U
	unsigned long kernel_size = 0;
#else
	unsigned int kernel_size = 0;
#endif
	struct sg_table *table;
	struct ion_heap *heap = NULL;
	int is_kernel_addr = from_kernel;

	/* Get kernel handle
	 * For cache sync all cases, some users
	 *     don't send valid handle, return error here.
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

	if (kernel_handle->buffer->size < param->size) {
		IONMSG("%s error, sync size is larger than buf_sz:%zu\n",
		       __func__, kernel_handle->buffer->size);
		goto err;
	}

	buffer = kernel_handle->buffer;
	sync_size = param->size;

	switch (sync_type) {
	case ION_CACHE_CLEAN_BY_RANGE:
	case ION_CACHE_INVALID_BY_RANGE:
	case ION_CACHE_FLUSH_BY_RANGE:

		sync_va = (unsigned long)param->va;

		if (sync_size == 0 || sync_va == 0) {
			/* whole buffer cache sync
			 * get sync_va and sync_size here
			 */
			sync_size = buffer->size;

			/* Do kernel map and do cache sync
			 * 32bit project, vmap space is small,
			 *    4MB as a boundary for mapping.
			 * 64bit vmap space is huge
			 */
#ifdef CONFIG_ARM64
			sync_va = (unsigned long)ion_map_kernel(client,
								kernel_handle);
			is_kernel_addr = 1;
			ion_need_unmap_flag = 1;
#else
			if (sync_size <= SZ_4M) {
				sync_va = (unsigned long)
				ion_map_kernel(client, kernel_handle);
				is_kernel_addr = 1;
				ion_need_unmap_flag = 1;
			} else {
				ret =
				ion_sys_cache_sync_buf(client, sync_type,
						       kernel_handle);
				goto out;
			}
#endif
			if (IS_ERR_OR_NULL(ERR_PTR((long)sync_va))) {
				IONMSG("%s #%d: map failed\n", __func__,
				       __LINE__);
				ret = -ENOMEM;
				goto err;
			}
		}
		break;

	/* range PA(means mva) mode, need map
	 * NOTICE: m4u_mva_map_kernel only support m4u0
	 */
	case ION_CACHE_CLEAN_BY_RANGE_USE_PA:
	case ION_CACHE_INVALID_BY_RANGE_USE_PA:
	case ION_CACHE_FLUSH_BY_RANGE_USE_PA:
		sync_va = param->iova;
#ifdef	CONFIG_MTK_PSEUDO_M4U
		table = buffer->sg_table;
#if defined(CONFIG_MTK_IOMMU_PGTABLE_EXT) && \
	(CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
		heap = buffer->heap;
		if (heap->ops->get_table)
			heap->ops->get_table(buffer, table);
		if (!table) {
			IONMSG("%s invalid table\n", __func__);
			goto err;
		}
		ret = m4u_mva_map_kernel(
				sync_va, sync_size, &kernel_va,
				&kernel_size, table);
#else
		ret = m4u_mva_map_kernel(
				(unsigned int)sync_va, (unsigned int)sync_size,
				&kernel_va, (unsigned int *)&kernel_size);
#endif
#else
	ret = m4u_mva_map_kernel(sync_va, sync_size,
				 &kernel_va, &kernel_size);
#endif

		if (ret)
			goto err;
		sync_va = kernel_va;
		is_kernel_addr = 1;
		break;
	default:
		ret = -EINVAL;
		goto err;
	}

	ret = __cache_sync_by_range(client, sync_type,
				    sync_va, sync_size, is_kernel_addr);
	if (ret < 0)
		goto err;

	/* range operation PA mode, unmap here */
	if (sync_type == ION_CACHE_CLEAN_BY_RANGE_USE_PA ||
	    sync_type == ION_CACHE_INVALID_BY_RANGE_USE_PA ||
	    sync_type == ION_CACHE_FLUSH_BY_RANGE_USE_PA) {
#if defined(CONFIG_MTK_IOMMU_PGTABLE_EXT) && \
	(CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
		m4u_mva_unmap_kernel((unsigned long)param->va,
				     sync_size, sync_va);
#else
		m4u_mva_unmap_kernel((unsigned int)param->va,
				     (unsigned int)sync_size, sync_va);
#endif
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
	IONMSG("%s sync err:%d|k%d|hdl:%d-%p|addr:0x%lx|iova:0x%llx|sz:%d|%s\n",
	       __func__, sync_type, from_kernel,
	       param->handle, param->kernel_handle,
	       (unsigned long)param->va, param->iova, param->size,
	       (*client->dbg_name) ? client->dbg_name : client->name);
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

static long ion_sys_ioctl(struct ion_client *client, unsigned int cmd,
			  unsigned long arg, int from_kernel)
{
	struct ion_sys_data param;
	long ret = 0;
	unsigned long ret_copy = 0;
	ion_phys_addr_t phy_addr;

	ION_FUNC_ENTER;
	if (!arg) {
		IONMSG("%s:err arg = NULL. %s(%s),%d, k:%d\n",
		       __func__, client->name, client->dbg_name,
		       client->pid, from_kernel);
		ret = -EINVAL;
		goto err;
	}

	if (from_kernel) {
		param = *(struct ion_sys_data *)arg;
	} else {
		ret_copy =
		    copy_from_user(&param, (void __user *)arg,
				   sizeof(struct ion_sys_data));
		if (ret_copy != 0) {
			IONMSG("%s:err arg copy failed, ret_copy = %lu. %s(%s),%d, k:%d\n",
			       __func__, ret_copy, client->name, client->dbg_name,
			       client->pid, from_kernel);
			ret = -EFAULT;
			goto err;
		}
	}


	switch (param.sys_cmd) {
	case ION_SYS_CACHE_SYNC:
		ret = ion_sys_cache_sync(client,
					 &param.cache_sync_param,
					 from_kernel);
		break;
	case ION_SYS_GET_PHYS:
		{
			struct ion_handle *kernel_handle;

			phy_addr = param.get_phys_param.phy_addr;
			kernel_handle =
			    ion_drv_get_handle(client,
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
			param.get_phys_param.phy_addr = phy_addr;
			ion_drv_put_kernel_handle(kernel_handle);
		}
		break;
	case ION_SYS_SET_CLIENT_NAME:
		ret = ion_sys_copy_client_name(param.client_name_param.name,
					       client->dbg_name);
		break;
	default:
		IONMSG("[%s]: Error. Invalid command(%d).\n",
		       __func__, param.sys_cmd);
		ret = -EFAULT;
		break;
	}

	if (ret) {
		IONMSG("[%s]:failed to finish io-cmd(%d). %s(%s),%d, k:%d\n",
		       __func__, param.sys_cmd,
		       client->name, client->dbg_name,
		       client->pid, from_kernel);
		goto err;
	}

	if (from_kernel)
		*(struct ion_sys_data *)arg = param;
	else
		ret_copy =
		    copy_to_user((void __user *)arg, &param,
				 sizeof(struct ion_sys_data));
err:
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

#if IS_ENABLED(CONFIG_DEBUG_FS) || IS_ENABLED(CONFIG_PROC_FS)
/*for clients ion mm heap summary size*/
static int ion_clients_summary_show(struct seq_file *s, void *unused)
{
	struct ion_device *dev = g_ion_device;
	struct rb_node *n, *m;
	unsigned int buffer_size = 0;
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
					(unsigned int)(handle->buffer->size) /
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

#if IS_ENABLED(CONFIG_DEBUG_FS)
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
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
static int ion_proc_client_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_clients_summary_show, PDE_DATA(inode));
}

static const struct file_operations proc_client_fops = {
	.open = ion_proc_client_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif
#endif

#ifdef CONFIG_MTK_IOMMU_V2
struct device *g_iommu_device;
#endif
static int ion_drv_probe(struct platform_device *pdev)
{
	int i;
	struct ion_platform_data *pdata;
	unsigned int num_heaps;
#ifdef CONFIG_MTK_IOMMU_V2
	struct device *dev = &pdev->dev;

	IONMSG("%s, start\n", __func__);
	if (!iommu_get_domain_for_dev(dev)) {
		IONMSG("%s, iommu is not ready, waiting\n", __func__);
		return -EPROBE_DEFER;
	}
	pdata = (struct ion_platform_data *)of_device_get_match_data(dev);
	if (!pdata) {
		IONMSG("get match data fail\n");
		return EPROBE_DEFER;
	}
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

#ifdef CONFIG_MTK_IOMMU_V2
	g_iommu_device = dev;
#endif
	platform_set_drvdata(pdev, g_ion_device);
	#ifdef CONFIG_XEN
	g_ion_device->dev.this_device->archdata.dev_dma_ops = NULL;
	#endif
	arch_setup_dma_ops(g_ion_device->dev.this_device, 0, 0, NULL, false);

#if IS_ENABLED(CONFIG_DEBUG_FS)
	debugfs_create_file("clients_summary", 0644,
			    g_ion_device->clients_debug_root, NULL,
			    &debug_client_fops);
	debugfs_create_symlink("ion_mm_heap", g_ion_device->debug_root,
			       "./heaps/ion_mm_heap");
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
	proc_create("clients_summary", S_IFREG | 0664,
		    g_ion_device->clients_proc_root,
		    &proc_client_fops);
	proc_symlink("ion_mm_heap", g_ion_device->proc_root,
		     "./heaps/ion_mm_heap");
#endif

	ion_history_init();
	ion_profile_init();
	ion_dmabuf_init();

	IONMSG("%s, done\n", __func__);
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
	 .type = (unsigned int)ION_HEAP_TYPE_SYSTEM,
	 .id = ION_HEAP_TYPE_SYSTEM,
	 .name = "ion_system_heap",
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
	 .type = (unsigned int)ION_HEAP_TYPE_CARVEOUT,
	 .id = ION_HEAP_TYPE_CARVEOUT,
	 .name = "ion_carveout_heap",
	 .base = 0,
#ifdef CONFIG_FPGA_EARLY_PORTING
	 .size = 0x10000000, /* reserve 256MB for Camera; */
#else
	 .size = 0xc000, /* reserve 48KB for Audio; */
#endif
	 .align = 0x1000, /* this must not be 0 if enable */
	 .priv = NULL,
	 },
};

struct ion_platform_data ion_drv_platform_data = {
	.nr = ARRAY_SIZE(ion_drv_platform_heaps),
	.heaps = ion_drv_platform_heaps,
};

#ifdef CONFIG_MTK_IOMMU_V2
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
#ifdef CONFIG_MTK_IOMMU_V2
		.of_match_table = mtk_ion_match_table,
#endif
	}
};

#ifndef CONFIG_MTK_IOMMU_V2
static struct platform_device ion_device = {
	.name = "ion-drv",
	.id = 0,
	.dev = {
		.platform_data = &ion_drv_platform_data,
		},

};
#endif
#include <linux/of_reserved_mem.h>
static int __init ion_reserve_memory_to_camera(struct reserved_mem *mem)
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
#ifndef CONFIG_MTK_IOMMU_V2
	if (platform_device_register(&ion_device)) {
		IONMSG("%s platform device register failed.\n", __func__);
		return -ENODEV;
	}
#endif
	if (platform_driver_register(&ion_driver)) {
#ifndef CONFIG_MTK_IOMMU_V2
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
#ifndef CONFIG_MTK_IOMMU_V2
	platform_device_unregister(&ion_device);
#endif
}

fs_initcall(ion_init);
__exitcall(ion_exit);
/*module_exit(ion_exit);*/
