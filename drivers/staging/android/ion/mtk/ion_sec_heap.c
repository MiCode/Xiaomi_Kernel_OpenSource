#include <asm/page.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <mmprofile.h>
#include <linux/debugfs.h>
#include <linux/kthread.h>
#include "mtk/mtk_ion.h"
#include "ion_profile.h"
#include "ion_drv_priv.h"
#include "ion_priv.h"
#include "mtk/ion_drv.h"
#include "ion_sec_heap.h"
#ifdef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
#include "tz_cross/trustzone.h"
#include "tz_cross/ta_mem.h"
#include "trustzone/kree/system.h"
#include "trustzone/kree/mem.h"
#endif

#define ION_PRINT_LOG_OR_SEQ(seq_file, fmt, args...) \
do {\
	if (seq_file)\
		seq_printf(seq_file, fmt, ##args);\
	else\
		pr_err(fmt, ##args);\
} while (0)

struct ion_sec_heap {
	struct ion_heap heap;
	void *priv;
};

#ifdef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
static KREE_SESSION_HANDLE ion_session;
KREE_SESSION_HANDLE ion_session_handle(void)
{
	if (ion_session == KREE_SESSION_HANDLE_NULL) {
		TZ_RESULT ret;

		ret = KREE_CreateSession(TZ_TA_MEM_UUID, &ion_session);
		if (ret != TZ_RESULT_SUCCESS) {
			IONMSG("KREE_CreateSession fail, ret=%d\n", ret);
			return KREE_SESSION_HANDLE_NULL;
		}
	}

	return ion_session;
}
#endif

static int ion_sec_heap_allocate(struct ion_heap *heap,
		struct ion_buffer *buffer, unsigned long size, unsigned long align,
		unsigned long flags) {
	u32 sec_handle = 0;
	ion_sec_buffer_info *pBufferInfo = NULL;
	u32 refcount = 0;

	IONDBG("%s enter id %d size 0x%lx align %ld flags 0x%lx\n", __func__, heap->id, size, align, flags);

	pBufferInfo = kzalloc(sizeof(ion_sec_buffer_info), GFP_KERNEL);
	if (IS_ERR_OR_NULL(pBufferInfo)) {
		IONMSG("%s Error. Allocate pBufferInfo failed.\n", __func__);
		return -EFAULT;
	}

#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) && defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
	if (flags & ION_FLAG_MM_HEAP_INIT_ZERO)
		secmem_api_alloc_zero(align, size, &refcount, &sec_handle, (uint8_t *)heap->name, heap->id);
	else
		secmem_api_alloc(align, size, &refcount, &sec_handle, (uint8_t *)heap->name, heap->id);
#elif defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
	{
		int ret = 0;

		if (flags & ION_FLAG_MM_HEAP_INIT_ZERO)
			ret = KREE_ZallocSecurechunkmemWithTag(ion_session_handle(),
				&sec_handle, align, size, heap->name);
		else
			ret = KREE_AllocSecurechunkmemWithTag(ion_session_handle(),
				&sec_handle, align, size, heap->name);
		if (ret != TZ_RESULT_SUCCESS) {
			IONMSG("KREE_AllocSecurechunkmemWithTag failed, ret is 0x%x\n", ret);
			return -ENOMEM;
		}
	}
	refcount = 0;
#else
	refcount = 0;
#endif
	if (sec_handle <= 0) {
		IONMSG("%s alloc security memory failed\n", __func__);
		return -ENOMEM;
	}

	pBufferInfo->priv_phys = sec_handle;
	pBufferInfo->pVA = 0;
	pBufferInfo->MVA = 0;
	pBufferInfo->eModuleID = -1;
	pBufferInfo->dbg_info.value1 = 0;
	pBufferInfo->dbg_info.value2 = 0;
	pBufferInfo->dbg_info.value3 = 0;
	pBufferInfo->dbg_info.value4 = 0;
	strncpy((pBufferInfo->dbg_info.dbg_name), "nothing", ION_MM_DBG_NAME_LEN);

	buffer->priv_virt = pBufferInfo;
	buffer->flags &= ~ION_FLAG_CACHED;
	buffer->size = size;

	IONDBG("%s exit priv_virt %p pa 0x%lx(%zu)\n", __func__, buffer->priv_virt,
							pBufferInfo->priv_phys, buffer->size);
	return 0;
}


void ion_sec_heap_free(struct ion_buffer *buffer)
{
	struct sg_table *table = buffer->sg_table;
	ion_sec_buffer_info *pBufferInfo = (ion_sec_buffer_info *) buffer->priv_virt;
	u32 sec_handle = 0;

	IONDBG("%s enter priv_virt %p\n", __func__, buffer->priv_virt);
	sec_handle = ((ion_sec_buffer_info *)buffer->priv_virt)->priv_phys;
#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) && defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
	secmem_api_unref(sec_handle, (uint8_t *)buffer->heap->name, buffer->heap->id);
#elif defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
	{
		TZ_RESULT ret = 0;

		ret = KREE_UnreferenceSecurechunkmem(ion_session_handle(), sec_handle);
		if (ret != TZ_RESULT_SUCCESS)
			IONMSG("KREE_UnreferenceSecurechunkmem failed, ret is 0x%x\n", ret);
	}
#endif
	kfree(table);
	buffer->priv_virt = NULL;
	kfree(pBufferInfo);

	IONDBG("%s exit\n", __func__);
}

struct sg_table *ion_sec_heap_map_dma(struct ion_heap *heap,
		struct ion_buffer *buffer) {
	struct sg_table *table;
	int ret;
#if ION_RUNTIME_DEBUGGER
	ion_sec_buffer_info *pBufferInfo = (ion_sec_buffer_info *) buffer->priv_virt;
#endif
	IONDBG("%s enter priv_virt %p\n", __func__, buffer->priv_virt);

	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		return ERR_PTR(-ENOMEM);
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret) {
		kfree(table);
		return ERR_PTR(ret);
	}

#if ION_RUNTIME_DEBUGGER
	sg_set_page(table->sgl, phys_to_page(pBufferInfo->priv_phys), buffer->size, 0);
#else
	sg_set_page(table->sgl, 0, 0, 0);
#endif
	IONDBG("%s exit\n", __func__);
	return table;

}

static void ion_sec_heap_unmap_dma(struct ion_heap *heap, struct ion_buffer *buffer)
{
	IONDBG("%s priv_virt %p\n", __func__, buffer->priv_virt);
	sg_free_table(buffer->sg_table);
}

static int ion_sec_heap_shrink(struct ion_heap *heap, gfp_t gfp_mask, int nr_to_scan)
{
	return 0;
}

static int ion_sec_heap_phys(struct ion_heap *heap, struct ion_buffer *buffer,
		ion_phys_addr_t *addr, size_t *len)
{

	ion_sec_buffer_info *pBufferInfo = (ion_sec_buffer_info *) buffer->priv_virt;

	IONDBG("%s priv_virt %p\n", __func__, buffer->priv_virt);
	*addr = pBufferInfo->priv_phys;
	*len = buffer->size;
	IONDBG("%s exit pa 0x%lx(%zu)\n", __func__, pBufferInfo->priv_phys, buffer->size);

	return 0;
}

void ion_sec_heap_add_freelist(struct ion_buffer *buffer)
{
}

int ion_sec_heap_pool_total(struct ion_heap *heap)
{
	return 0;
}

void *ion_sec_heap_map_kernel(struct ion_heap *heap,
			  struct ion_buffer *buffer) {
#if ION_RUNTIME_DEBUGGER
	void *vaddr = ion_heap_map_kernel(heap, buffer);

	IONMSG("%s enter priv_virt %p vaddr %p\n", __func__, buffer->priv_virt, vaddr);
	return vaddr;
#else
	return NULL;
#endif
}

void ion_sec_heap_unmap_kernel(struct ion_heap *heap,
			   struct ion_buffer *buffer)
{
#if ION_RUNTIME_DEBUGGER
	IONMSG("%s enter priv_virt %p\n", __func__, buffer->priv_virt);
	ion_heap_unmap_kernel(heap, buffer);
#else

#endif
}

int ion_sec_heap_map_user(struct ion_heap *heap, struct ion_buffer *buffer,
		      struct vm_area_struct *vma)
{
#if ION_RUNTIME_DEBUGGER
	struct sg_table *table = buffer->sg_table;
	unsigned long addr = vma->vm_start;
	unsigned long offset = vma->vm_pgoff * PAGE_SIZE;
	struct scatterlist *sg;
	int i;
	int ret;

	IONMSG("%s enter priv_virt %p\n", __func__, buffer->priv_virt);

	for_each_sg(table->sgl, sg, table->nents, i) {
		struct page *page = sg_page(sg);
		unsigned long remainder = vma->vm_end - addr;
		unsigned long len = sg->length;

		if (offset >= sg->length) {
			offset -= sg->length;
			continue;
		} else if (offset) {
			page += offset / PAGE_SIZE;
			len = sg->length - offset;
			offset = 0;
		}
		len = min(len, remainder);
		ret = remap_pfn_range(vma, addr, page_to_pfn(page), len,
				vma->vm_page_prot);
		if (ret) {
			IONMSG("%s remap_pfn_range failed vma:0x%p, addr = %lu, pfn = %lu, len = %lu, ret = %d.\n",
				__func__, vma, addr, page_to_pfn(page), len, ret);
			return ret;
		}
		addr += len;
		if (addr >= vma->vm_end)
			return 0;
	}
	IONMSG("%s exit\n", __func__);
	return 0;
#else
	IONMSG("%s do not suppuprt\n", __func__);
	return  (-ENOMEM);
#endif
}


int ion_sec_copy_dbg_info(ion_mm_buf_debug_info_t *src, ion_mm_buf_debug_info_t *dst)
{
	int i;

	dst->handle = src->handle;
	for (i = 0; i < ION_MM_DBG_NAME_LEN; i++)
		dst->dbg_name[i] = src->dbg_name[i];

	dst->dbg_name[ION_MM_DBG_NAME_LEN - 1] = '\0';
	dst->value1 = src->value1;
	dst->value2 = src->value2;
	dst->value3 = src->value3;
	dst->value4 = src->value4;

	return 0;

}

static struct ion_heap_ops mm_sec_heap_ops = {
		.allocate = ion_sec_heap_allocate,
		.free = ion_sec_heap_free,
		.map_dma = ion_sec_heap_map_dma,
		.unmap_dma = ion_sec_heap_unmap_dma,
		.map_kernel = ion_sec_heap_map_kernel,
		.unmap_kernel = ion_sec_heap_unmap_kernel,
		.map_user = ion_sec_heap_map_user,
		.phys = ion_sec_heap_phys,
		.shrink = ion_sec_heap_shrink,
		/*.add_freelist = ion_sec_heap_add_freelist,*/
		.page_pool_total = ion_sec_heap_pool_total,
};

static int ion_sec_heap_debug_show(struct ion_heap *heap, struct seq_file *s, void *unused)
{
	struct ion_device *dev = heap->dev;
	struct rb_node *n;
	int *secur_handle;

	if (heap->flags & ION_HEAP_FLAG_DEFER_FREE)
		ION_PRINT_LOG_OR_SEQ(s, "mm_heap_freelist total_size=0x%zu\n", ion_heap_freelist_size(heap));
	else
		ION_PRINT_LOG_OR_SEQ(s, "mm_heap defer free disabled\n");


	ION_PRINT_LOG_OR_SEQ(s, "----------------------------------------------------\n");
	ION_PRINT_LOG_OR_SEQ(s,
			"%8.s %8.s %4.s %3.s %3.s %10.s %4.s %3.s %s\n",
			"buffer", "size", "kmap", "ref", "hdl", "sec handle",
			"flag", "pid", "comm(client)");

	for (n = rb_first(&dev->buffers); n; n = rb_next(n)) {
		struct ion_buffer
		*buffer = rb_entry(n, struct ion_buffer, node);
		if (buffer->heap->type != heap->type)
			continue;
		mutex_lock(&dev->buffer_lock);
		secur_handle = (int *) buffer->priv_virt;

		ION_PRINT_LOG_OR_SEQ(s,
				"0x%p %8zu %3d %3d %3d 0x%x %3lu %3d %s",
				buffer, buffer->size, buffer->kmap_cnt, atomic_read(&buffer->ref.refcount),
				buffer->handle_count, *secur_handle,
				buffer->flags, buffer->pid, buffer->task_comm);
		ION_PRINT_LOG_OR_SEQ(s, ")\n");

		mutex_unlock(&dev->buffer_lock);

		ION_PRINT_LOG_OR_SEQ(s, "----------------------------------------------------\n");
	}

	down_read(&dev->lock);
	for (n = rb_first(&dev->clients); n; n = rb_next(n)) {
		struct ion_client
		*client = rb_entry(n, struct ion_client, node);

		if (client->task) {
			char task_comm[TASK_COMM_LEN];

			get_task_comm(task_comm, client->task);
			ION_PRINT_LOG_OR_SEQ(s,
					"client(0x%p) %s (%s) pid(%u) ================>\n",
					client, task_comm, client->dbg_name, client->pid);
		} else {
			ION_PRINT_LOG_OR_SEQ(s,
					"client(0x%p) %s (from_kernel) pid(%u) ================>\n",
					client, client->name, client->pid);
		}

		{
			struct rb_node *m;

			mutex_lock(&client->lock);
			for (m = rb_first(&client->handles); m; m = rb_next(m)) {
				struct ion_handle
				*handle = rb_entry(m, struct ion_handle, node);

				ION_PRINT_LOG_OR_SEQ(s,
						"\thandle=0x%p, buffer=0x%p, heap=%d\n",
						handle, handle->buffer, handle->buffer->heap->id);

			}
			mutex_unlock(&client->lock);
		}

	}
	up_read(&dev->lock);

	return 0;
}

struct ion_heap *ion_sec_heap_create(struct ion_platform_heap *heap_data)
{
#if ((defined(CONFIG_TRUSTONIC_TEE_SUPPORT) && defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT))\
	|| (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)))

	struct ion_sec_heap *heap;

	IONMSG("%s enter ion_sec_heap_create\n", __func__);

	heap = kzalloc(sizeof(*heap), GFP_KERNEL);
	if (!heap) {
		IONMSG("%s kzalloc failed heap is null.\n", __func__);
		return ERR_PTR(-ENOMEM);
	}
	heap->heap.ops = &mm_sec_heap_ops;
	heap->heap.type = ION_HEAP_TYPE_MULTIMEDIA_SEC;
	heap->heap.flags &= ~ION_HEAP_FLAG_DEFER_FREE;
	heap->heap.debug_show = ion_sec_heap_debug_show;

	return &heap->heap;

#else
	struct ion_sec_heap heap;

	heap.heap.ops = &mm_sec_heap_ops;
	heap.heap.debug_show = ion_sec_heap_debug_show;
	IONMSG("%s error: not support\n", __func__);
	return NULL;
#endif

}

void ion_sec_heap_destroy(struct ion_heap *heap)
{
#if ((defined(CONFIG_TRUSTONIC_TEE_SUPPORT) && defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT))\
	|| (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)))


	struct ion_sec_heap *sec_heap;

	IONMSG("%s enter\n", __func__);

	sec_heap = container_of(heap, struct ion_sec_heap, heap);
	kfree(sec_heap);
#else
	IONMSG("%s error: not support\n", __func__);
#endif
}

#if 0
long ion_sec_ioctl(struct ion_client *client, unsigned int cmd, unsigned long arg, int from_kernel)
{
	ion_mm_data_t param;
	long ret = 0;
	unsigned long ret_copy = 0;

#if ((defined(CONFIG_TRUSTONIC_TEE_SUPPORT) && defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT))
	|| defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT))
	IONMSG("%s enter\n", __func__);
#else
	IONMSG("%s error: not support\n", __func__);
	return (-EFAULT);
#endif
	if (from_kernel)
		param = *(ion_mm_data_t *) arg;
	else
		ret_copy = copy_from_user(&param, (void __user *)arg, sizeof(ion_mm_data_t));

	switch (param.mm_cmd) {
	case ION_MM_CONFIG_BUFFER:
		if (param.config_buffer_param.handle) {
			struct ion_buffer *buffer;
			struct ion_handle *kernel_handle;

			kernel_handle = ion_drv_get_handle(client, param.config_buffer_param.handle,
							     param.config_buffer_param.kernel_handle, from_kernel);
			if (IS_ERR(kernel_handle)) {
				IONMSG("ion config buffer fail! port=%d.\n", param.config_buffer_param.eModuleID);
				ret = -EINVAL;
				break;
			}

			buffer = ion_handle_buffer(kernel_handle);
			if ((int) buffer->heap->type == ION_HEAP_TYPE_MULTIMEDIA_SEC) {
				ion_sec_buffer_info *pBufferInfo = buffer->priv_virt;

				if (pBufferInfo->MVA == 0) {
					pBufferInfo->eModuleID = param.config_buffer_param.eModuleID;
					pBufferInfo->security = param.config_buffer_param.security;
					pBufferInfo->coherent = param.config_buffer_param.coherent;
				} else {
					if (pBufferInfo->security
						!= param.config_buffer_param.security
						|| pBufferInfo->coherent != param.config_buffer_param.coherent) {
						IONMSG("[sec]: Warning. config buffer param error from %c heap:.\n",
								buffer->heap->type);
						IONMSG("[sec]:%d(%d), coherent: %d(%d)\n",
								pBufferInfo->security,
								param.config_buffer_param.security,
								pBufferInfo->coherent,
								param.config_buffer_param.coherent);
						ret = -ION_ERROR_CONFIG_LOCKED;
					}
				}
			} else {
				IONMSG("[ion_sec_heap]: Error. Cannot configure buffer that is not from %c heap.\n",
						buffer->heap->type);
				ret = 0;
			}
			ion_drv_put_kernel_handle(kernel_handle);
		} else {
			IONMSG("[ion_sec_heap]: Error config buf with invalid handle.\n");
			ret = -EFAULT;
		}
	break;
	case ION_MM_SET_DEBUG_INFO:
	{
		struct ion_buffer *buffer;

		if (param.buf_debug_info_param.handle) {
			struct ion_handle *kernel_handle;

			kernel_handle = ion_drv_get_handle(client, param.buf_debug_info_param.handle,
					param.buf_debug_info_param.kernel_handle, from_kernel);
			if (IS_ERR(kernel_handle)) {
				IONMSG("ion set debug info fail! kernel_handle=0x%p.\n", kernel_handle);
				ret = -EINVAL;
				break;
			}

			buffer = ion_handle_buffer(kernel_handle);
			if ((int) buffer->heap->type == ION_HEAP_TYPE_MULTIMEDIA_SEC) {
				ion_sec_buffer_info *pBufferInfo = buffer->priv_virt;

				ion_sec_copy_dbg_info(&(param.buf_debug_info_param), &(pBufferInfo->dbg_info));
			} else {
				IONMSG("[ion_sec_heap]: Error. Cannot set dbg buffer that is not from %c heap.\n",
						buffer->heap->type);
				ret = -EFAULT;
			}
			ion_drv_put_kernel_handle(kernel_handle);
		} else {
			IONMSG("[ion_sec_heap]: Error. set dbg buffer with invalid handle.\n");
			ret = -EFAULT;
		}
	}
	break;
	case ION_MM_GET_DEBUG_INFO:
	{
		struct ion_buffer *buffer;

		if (param.buf_debug_info_param.handle) {
			struct ion_handle *kernel_handle;

			kernel_handle = ion_drv_get_handle(client, param.buf_debug_info_param.handle,
					param.buf_debug_info_param.kernel_handle, from_kernel);
			if (IS_ERR(kernel_handle)) {
				IONMSG("ion get debug info fail! kernel_handle=0x%p.\n", kernel_handle);
				ret = -EINVAL;
				break;
			}
			buffer = ion_handle_buffer(kernel_handle);
			if ((int) buffer->heap->type == ION_HEAP_TYPE_MULTIMEDIA_SEC) {
				ion_sec_buffer_info *pBufferInfo = buffer->priv_virt;

				ion_sec_copy_dbg_info(&(pBufferInfo->dbg_info), &(param.buf_debug_info_param));
			} else {
				IONMSG("[ion_sec_heap]: Error. Cannot get dbg buffer that is not from %c heap.\n",
						buffer->heap->type);
				ret = -EFAULT;
			}
			ion_drv_put_kernel_handle(kernel_handle);
		} else {
			IONMSG("[ion_sec_heap]: Error. get dbg buffer with invalid handle.\n");
			ret = -EFAULT;
		}
	}
	break;

	default:
		IONMSG("[ion_sec_heap]: Error. Invalid command.\n");
		ret = -EFAULT;
	}

	if (from_kernel)
		*(ion_mm_data_t *)arg = param;
	else
		ret_copy = copy_to_user((void __user *)arg, &param, sizeof(ion_mm_data_t));

	IONMSG("%s cmd %d exit\n", __func__, param.mm_cmd);
	return ret;

}
#endif

