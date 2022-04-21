// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

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
#include <linux/fdtable.h>
//#include <mmprofile.h>
//#include <mmprofile_function.h>
#include <linux/kthread.h>
#include <linux/sched/task.h>
#include <linux/sched/signal.h>
#include <linux/delay.h>
#include "mtk/mtk_ion.h"
#include "ion_profile.h"
#include "ion_drv_priv.h"
#include "ion_priv.h"
#include "mtk/ion_drv.h"
#include "ion_sec_heap.h"
#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && \
	defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)

#define MTK_IN_HOUSE_SEC_ION_SUPPORT
#include "tz_cross/trustzone.h"
#include "tz_cross/ta_mem.h"
#include "trustzone/kree/system.h"
#include "trustzone/kree/mem.h"

#endif

#ifdef CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM
#define SECMEM_KERNEL_API
#include "trusted_mem_api.h"
#endif

#define ION_DUMP(seq_files, fmt, args...) \
do {\
	struct seq_file *file = (struct seq_file *)seq_files;\
	char *fmat = fmt;\
	if (file)\
		seq_printf(file, fmat, ##args);\
	else\
		printk(KERN_ERR fmt, ##args);\
} while (0)

struct ion_sec_heap {
	struct ion_heap heap;
	void *priv;
};

static size_t sec_heap_total_memory;
static unsigned int caller_pid;
static unsigned int caller_tid;

#if defined(MTK_IN_HOUSE_SEC_ION_SUPPORT)

static KREE_SESSION_HANDLE ion_session;
KREE_SESSION_HANDLE ion_session_handle(void)
{
	if (ion_session == KREE_SESSION_HANDLE_NULL) {
		int ret;

		ret = KREE_CreateSession(TZ_TA_MEM_UUID, &ion_session);
		if (ret != TZ_RESULT_SUCCESS) {
			IONMSG("KREE_CreateSession fail, ret=%d\n", ret);
			return KREE_SESSION_HANDLE_NULL;
		}
	}

	return ion_session;
}
#endif

struct sg_table *ion_sec_heap_map_dma(struct ion_heap *heap,
				      struct ion_buffer *buffer)
{
	struct sg_table *table;
	int ret;
#if ION_RUNTIME_DEBUGGER
	struct ion_sec_buffer_info *pbufferinfo =
		(struct ion_sec_buffer_info *)buffer->priv_virt;
#endif
	IONDBG("%s enter priv_virt %p\n", __func__, buffer->priv_virt);

	table = kzalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		return ERR_PTR(-ENOMEM);
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret) {
		kfree(table);
		return ERR_PTR(ret);
	}

#if ION_RUNTIME_DEBUGGER
#ifdef SECMEM_64BIT_PHYS_SHIFT
#define SEC_MEM_SHIFT SECMEM_64BIT_PHYS_SHIFT
	sg_set_page(table->sgl,
		    phys_to_page(pbufferinfo->priv_phys << SEC_MEM_SHIFT),
		    buffer->size, 0);
#undef SEC_MEM_SHIFT
#else
	sg_set_page(table->sgl, phys_to_page(pbufferinfo->priv_phys),
		    buffer->size, 0);
#endif
#else
	sg_set_page(table->sgl, 0, 0, 0);
#endif
	IONDBG("%s exit\n", __func__);
	return table;
}

static void ion_sec_heap_unmap_dma(struct ion_heap *heap,
				   struct ion_buffer *buffer)
{
	IONDBG("%s priv_virt %p\n", __func__, buffer->priv_virt);
	sg_free_table(buffer->sg_table);
}

#ifdef CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM

/* copy from tz_m4u.h */
enum ion_mtk_iommu_sec_id {
	SEC_ID_SEC_CAM = 0,
	SEC_ID_SVP,
	SEC_ID_SDSP,
	SEC_ID_WFD,
	SEC_ID_COUNT
};

/*
 * return value:
 *    <0: not MTEE mem, no sec_id
 *    >0: iommu_sec_id
 */
static int ion_tmem_type2sec_id(enum TRUSTED_MEM_REQ_TYPE tmem)
{
	switch (tmem) {
	case TRUSTED_MEM_REQ_PROT: /* sec cam, only MTEE on S */
		return SEC_ID_SEC_CAM;
#ifdef CONFIG_MTK_SVP_ON_MTEE_SUPPORT
	case TRUSTED_MEM_REQ_SVP:/* MTEE SVP on:MTEE, off: TEE */
		return SEC_ID_SVP;
	case TRUSTED_MEM_REQ_WFD:/* MTEE SVP on:MTEE, off: TEE */
		return SEC_ID_WFD;
#endif
	case TRUSTED_MEM_REQ_SDSP:
		return SEC_ID_SDSP;
	default:
		return -1;
	}
}

/*
 * return value:
 *    <0: not secure mem
 *    =0: protected mem, used in MTEE
 *    =1: secure memory, used in TEE
 */
static int ion_tmem_type_is_secure(enum TRUSTED_MEM_REQ_TYPE tmem)
{
	switch (tmem) {
	case TRUSTED_MEM_REQ_SVP:/* MTEE SVP on:MTEE, off: TEE */
	case TRUSTED_MEM_REQ_WFD:/* MTEE SVP on:MTEE, off: TEE */
#ifdef CONFIG_MTK_SVP_ON_MTEE_SUPPORT
		return 0;
#else
		return 1;
#endif
	case TRUSTED_MEM_REQ_PROT: /* sec cam, only MTEE on S */
	case TRUSTED_MEM_REQ_HAPP:      /* tmporarily as MTEE */
	case TRUSTED_MEM_REQ_HAPP_EXTRA:/* tmporarily as MTEE */
	case TRUSTED_MEM_REQ_SDSP:      /* tmporarily as MTEE */
		return 0;
	case TRUSTED_MEM_REQ_SDSP_SHARED:/* TEE */
	case TRUSTED_MEM_REQ_2D_FR:      /* TEE */
		return 1;
	default:
		return -1;
	}
}
static enum TRUSTED_MEM_REQ_TYPE get_trusted_mem_type(unsigned int heap_id)
{
	switch (heap_id) {
	case ION_HEAP_TYPE_MULTIMEDIA_SEC:
		return TRUSTED_MEM_REQ_SVP;
	case ION_HEAP_TYPE_MULTIMEDIA_PROT:
		return TRUSTED_MEM_REQ_PROT;
	case ION_HEAP_TYPE_MULTIMEDIA_2D_FR:
		return TRUSTED_MEM_REQ_2D_FR;
	case ION_HEAP_TYPE_MULTIMEDIA_WFD:
		return TRUSTED_MEM_REQ_WFD;
	case ION_HEAP_TYPE_MULTIMEDIA_HAPP:
		return TRUSTED_MEM_REQ_HAPP;
	case ION_HEAP_TYPE_MULTIMEDIA_HAPP_EXTRA:
		return TRUSTED_MEM_REQ_HAPP_EXTRA;
	case ION_HEAP_TYPE_MULTIMEDIA_SDSP:
		return TRUSTED_MEM_REQ_SDSP;
	case ION_HEAP_TYPE_MULTIMEDIA_SDSP_SHARED:
		return TRUSTED_MEM_REQ_SDSP_SHARED;
	default:
		return -1;
	}
}

enum TRUSTED_MEM_REQ_TYPE ion_get_trust_mem_type(struct dma_buf *dmabuf)
{
	enum TRUSTED_MEM_REQ_TYPE tmem_type = -1;
	struct ion_buffer *buffer;

	if (!dmabuf) {
		IONMSG("%s dmabuf is NULL\n", __func__);
		return tmem_type;
	}

	buffer = dmabuf->priv;
	if (buffer)
		tmem_type = get_trusted_mem_type(buffer->heap->id);

	return tmem_type;
}

/*
 * return: trustmem type.
 *      -1: nomal buffer
 *     >=0: valid tmem_type
 *
 * handle: input ion handle
 * sec: used for return.
 *      0: protected buffer;
 *      1: secure buffer;
 *     <0: error buffer;
 * iommu_sec_id: used for return
 *      <0: error buffer;
 *     >=0: valid sec_id
 * sec_hdl: used for return
 *       0: normal buffer, no secure handle
 *  others: secure handle
 */
enum TRUSTED_MEM_REQ_TYPE
ion_hdl2sec_type(struct ion_handle *handle,
		 int *sec, int *iommu_sec_id,
		 ion_phys_addr_t *sec_hdl)
{
	enum TRUSTED_MEM_REQ_TYPE tmem_type = -1;
	struct ion_buffer *buffer;
	struct ion_sec_buffer_info *pbufferinfo;

	if (!handle || !sec || !iommu_sec_id || !sec_hdl) {
		IONMSG("%s NULL ptr\n", __func__);
		return tmem_type;
	}

	*sec_hdl = 0;
	buffer = handle->buffer;
	if (buffer) {
		tmem_type = get_trusted_mem_type(buffer->heap->id);
	} else {
		IONMSG("handle invalid, handle id:%d client:%s\n",
		       handle->id, handle->client->display_name);
		dump_stack();
	}

	*sec = ion_tmem_type_is_secure(tmem_type);

	*iommu_sec_id = ion_tmem_type2sec_id(tmem_type);

	if (*sec >= 0) {
		pbufferinfo = (struct ion_sec_buffer_info *)buffer->priv_virt;
		*sec_hdl = pbufferinfo->priv_phys;
	}

	IONDBG("tmem type:%d, sec:%d, sec_id:%d, handle:0x%lx\n",
	       tmem_type, *sec, *iommu_sec_id, *sec_hdl);
	return tmem_type;
}

/*
 * return: trustmem type.
 *      -1: nomal buffer
 *     >=0: valid tmem_type
 *
 * fd: input ion buffer fd
 *
 * sec: used for return.
 *      0: protected buffer;
 *      1: secure buffer;
 *     <0: error buffer;
 *
 * iommu_sec_id: used for return
 *      <0: error buffer;
 *     >=0: valid sec_id
 *
 * sec_hdl: used for return
 *       0: normal buffer, no secure handle
 *  others: secure handle
 */
enum TRUSTED_MEM_REQ_TYPE
ion_fd2sec_type(int fd, int *sec, int *iommu_sec_id,
		ion_phys_addr_t *sec_hdl)
{
	enum TRUSTED_MEM_REQ_TYPE tmem_type = -1;
	struct ion_client *client;
	struct ion_handle *handle;

	if (fd < 0) {
		IONMSG("%s fd is invalid:%d\n", __func__, fd);
		return tmem_type;
	}

	client = ion_client_create(g_ion_device, "fd2sec_type");
	if (IS_ERR(client))
		return PTR_ERR(client);

	handle = ion_import_dma_buf_fd(client, fd);
	if (IS_ERR(handle)) {
		ion_client_destroy(client);
		return PTR_ERR(handle);
	}

	tmem_type = ion_hdl2sec_type(handle, sec, iommu_sec_id, sec_hdl);

	ion_free(client, handle);
	ion_client_destroy(client);

	return tmem_type;
}
#endif

static int ion_sec_heap_allocate(struct ion_heap *heap,
				 struct ion_buffer *buffer,
				 unsigned long size,
				 unsigned long align,
				 unsigned long flags)
{
	u32 sec_handle = 0;
	int ret = 0;
	struct ion_sec_buffer_info *pbufferinfo = NULL;
	u32 refcount = 0;
#ifdef CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM
	enum TRUSTED_MEM_REQ_TYPE tmem_type;
#endif

	ion_debug("%s enter id %d size 0x%lx align %ld flags 0x%lx\n",
		  __func__, heap->id, size, align, flags);

#ifdef CONFIG_PM
	if (sec_heap_total_memory <= 0)
		shrink_ion_by_scenario(0);
#endif
	caller_pid = (unsigned int)current->pid;
	caller_tid = (unsigned int)current->tgid;

	pbufferinfo = kzalloc(sizeof(*pbufferinfo), GFP_KERNEL);
	if (!pbufferinfo) {
		IONMSG("%s Error. Allocate pbufferinfo failed.\n", __func__);
		caller_pid = 0;
		caller_tid = 0;
		return -EFAULT;
	}

#ifdef CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM
	tmem_type = get_trusted_mem_type(heap->id);

	if (tmem_type < 0) {
		IONMSG("invalid heap:%d\n", heap->id);
		return -EINVAL;
	}
	if (flags & ION_FLAG_MM_HEAP_INIT_ZERO)
		ret = trusted_mem_api_alloc_zero(tmem_type,
						 align,
						 size,
						 &refcount,
						 &sec_handle,
						 (uint8_t *)heap->name,
						 heap->id);
	else
		ret = trusted_mem_api_alloc(tmem_type,
					    align,
					    size,
					    &refcount,
					    &sec_handle,
					    (uint8_t *)heap->name,
					    heap->id);
#elif defined(MTK_IN_HOUSE_SEC_ION_SUPPORT)
	{
		int ret = 0;

		if (flags & ION_FLAG_MM_HEAP_INIT_ZERO)
			ret =
			KREE_ZallocSecurechunkmemWithTag(ion_session_handle(),
							 &sec_handle,
							 align,
							 size,
							 heap->name);
		else
			ret =
			KREE_AllocSecurechunkmemWithTag(ion_session_handle(),
							&sec_handle,
							align,
							size,
							heap->name);
		if (ret != TZ_RESULT_SUCCESS) {
			ion_info("KREE_AllocSecurechunkmemWithTag failed, ret 0x%x\n",
				 ret);
			kfree(pbufferinfo);
			caller_pid = 0;
			caller_tid = 0;
			return -ENOMEM;
		}
	}
	refcount = 0;
#else
	refcount = 0;
#endif

	if (ret == -ENOMEM) {
		IONMSG("%s security out of memory, heap:%d\n",
			__func__, heap->id);
		/* avoid recursive deadlock */
		/* heap->debug_show(heap, NULL, NULL); */
	}
	if (sec_handle <= 0) {
		IONMSG("%s alloc security memory failed, total size %zu\n",
			__func__, sec_heap_total_memory);
		kfree(pbufferinfo);
		caller_pid = 0;
		caller_tid = 0;
		return -ENOMEM;
	}

	pbufferinfo->priv_phys = (ion_phys_addr_t)sec_handle;
	pbufferinfo->VA = 0;
	pbufferinfo->MVA = 0;
	pbufferinfo->FIXED_MVA = 0;
	pbufferinfo->iova_start = 0;
	pbufferinfo->iova_end = 0;
	pbufferinfo->module_id = -1;
	pbufferinfo->dbg_info.value1 = 0;
	pbufferinfo->dbg_info.value2 = 0;
	pbufferinfo->dbg_info.value3 = 0;
	pbufferinfo->dbg_info.value4 = 0;
	pbufferinfo->pid = buffer->pid;
	strncpy((pbufferinfo->dbg_info.dbg_name),
		"nothing", ION_MM_DBG_NAME_LEN);

	buffer->priv_virt = pbufferinfo;
	buffer->flags &= ~ION_FLAG_CACHED;
	buffer->size = size;
	buffer->sg_table = ion_sec_heap_map_dma(heap, buffer);
#ifdef CONFIG_MTK_IOMMU_V2
	sg_dma_address(buffer->sg_table->sgl) = (dma_addr_t)sec_handle;
	sg_dma_len(buffer->sg_table->sgl) = size;
#endif
	sec_heap_total_memory += size;
	caller_pid = 0;
	caller_tid = 0;
	ion_debug("%s exit priv_virt %p pa 0x%lx(%zu)\n",
		  __func__, buffer->priv_virt,
		  pbufferinfo->priv_phys, buffer->size);
	return 0;
}

void ion_sec_heap_free(struct ion_buffer *buffer)
{
	struct sg_table *table = buffer->sg_table;
	struct ion_heap *heap = buffer->heap;
	struct ion_sec_buffer_info *pbufferinfo =
		(struct ion_sec_buffer_info *)buffer->priv_virt;
	u32 sec_handle = 0;
#ifdef CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM
	enum TRUSTED_MEM_REQ_TYPE tmem_type;
#endif

	IONDBG("%s enter priv_virt %p\n", __func__, buffer->priv_virt);
	sec_heap_total_memory -= buffer->size;
	sec_handle =
		((struct ion_sec_buffer_info *)buffer->priv_virt)->priv_phys;

#ifdef CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM
	tmem_type = get_trusted_mem_type(buffer->heap->id);
	if (tmem_type < 0) {
		IONMSG("%s invalid buffer, heap:%d\n",
		       __func__, buffer->heap->id);
		goto unmap_dma;
	}
	trusted_mem_api_unref(tmem_type, sec_handle,
			      (uint8_t *)buffer->heap->name,
			      buffer->heap->id);
#elif defined(MTK_IN_HOUSE_SEC_ION_SUPPORT)
	{
		int ret = 0;

		ret = KREE_UnreferenceSecurechunkmem(ion_session_handle(),
						     sec_handle);
		if (ret != TZ_RESULT_SUCCESS)
			ion_info("KREE_UnreferenceSecurechunkmem failed, ret is 0x%x\n",
				 ret);
	}
#endif

#ifdef CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM
unmap_dma:
#endif

	ion_sec_heap_unmap_dma(heap, buffer);
	kfree(table);
	buffer->priv_virt = NULL;
	kfree(pbufferinfo);

	IONDBG("%s exit, total %zu\n", __func__, sec_heap_total_memory);
}

static int ion_sec_heap_shrink(struct ion_heap *heap,
			       gfp_t gfp_mask,
			       int nr_to_scan)
{
	return 0;
}

static int ion_sec_heap_phys(struct ion_heap *heap, struct ion_buffer *buffer,
			     ion_phys_addr_t *addr, size_t *len)
{
	struct ion_sec_buffer_info *pbufferinfo =
		(struct ion_sec_buffer_info *)buffer->priv_virt;

	IONDBG("%s priv_virt %p\n", __func__, buffer->priv_virt);
	*addr = pbufferinfo->priv_phys;
	*len = buffer->size;
	ion_debug("%s exit pa 0x%lx(%zu)\n",  __func__,
		  pbufferinfo->priv_phys, buffer->size);

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
			      struct ion_buffer *buffer)
{
#if ION_RUNTIME_DEBUGGER
	void *vaddr = ion_heap_map_kernel(heap, buffer);

	ion_info("%s enter priv_virt %p vaddr %p\n",
		 __func__, buffer->priv_virt, vaddr);
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
	int ret;

	ret = ion_heap_map_user(heap, buffer, vma);
	ion_info("%s vm_start=0x%lx, vm_end=0x%lx exit\n",
		 __func__, vma->vm_start, vma->vm_end);
	return ret;
#else
	IONMSG("%s do not suppuprt\n", __func__);
	return	(-ENOMEM);
#endif
}

static struct ion_heap_ops mm_sec_heap_ops = {
		.allocate = ion_sec_heap_allocate,
		.free = ion_sec_heap_free,
		//.map_dma = ion_sec_heap_map_dma,
		//.unmap_dma = ion_sec_heap_unmap_dma,
		.map_kernel = ion_sec_heap_map_kernel,
		.unmap_kernel = ion_sec_heap_unmap_kernel,
		.map_user = ion_sec_heap_map_user,
		.phys = ion_sec_heap_phys,
		.shrink = ion_sec_heap_shrink,
		/*.add_freelist = ion_sec_heap_add_freelist,*/
		.page_pool_total = ion_sec_heap_pool_total,
};

/*For sec memory heap user dump*/
static size_t ion_debug_sec_heap_total(struct ion_client *client)
{
	size_t size = 0;
	struct rb_node *n;

	if (mutex_trylock(&client->lock)) {
		for (n = rb_first(&client->handles); n; n = rb_next(n)) {
			struct ion_handle *handle =
				rb_entry(n, struct ion_handle, node);

			if (handle->buffer->heap->id ==
				ION_HEAP_TYPE_MULTIMEDIA_SEC ||
			    handle->buffer->heap->id ==
			    ION_HEAP_TYPE_MULTIMEDIA_PROT)
				size += handle->buffer->size;
		}
		mutex_unlock(&client->lock);
	}
	return size;
}

void ion_sec_heap_dump_info(void)
{
	struct ion_device *dev = g_ion_device;
	size_t total_size = 0;
	size_t total_orphaned_size = 0;
	struct rb_node *n;
	bool need_dev_lock = true;
	atomic_t buf_ref;
	const char *seq_line = "---------------------------------------";

	ION_DUMP(NULL, "%16.s(%16.s) %16.s %16.s %s\n",
		 "client", "dbg_name", "pid", "size", "address");
	ION_DUMP(NULL, "%s\n", seq_line);

	if (!down_read_trylock(&dev->lock)) {
		ION_DUMP(NULL,
			 "detail trylock fail, alloc pid(%d-%d)\n",
			 caller_pid, caller_tid);
		ION_DUMP(NULL, "current(%d-%d)\n",
			 (unsigned int)current->pid,
			 (unsigned int)current->tgid);
		if (caller_pid != (unsigned int)current->pid ||
		    caller_tid != (unsigned int)current->tgid)
			goto skip_client_entry;
		else
			need_dev_lock = false;
	}

	for (n = rb_first(&dev->clients); n; n = rb_next(n)) {
		struct ion_client
		*client = rb_entry(n, struct ion_client, node);
		size_t size = ion_debug_sec_heap_total(client);

		if (!size)
			continue;
		if (client->task) {
			char task_comm[TASK_COMM_LEN];

			get_task_comm(task_comm, client->task);
			ION_DUMP(NULL,
				 "%16.s(%16.s) %16u %16zu 0x%p\n",
				 task_comm,
				 (*client->dbg_name) ?
				 client->dbg_name :
				 client->name,
				 client->pid, size, client);
		} else {
			ION_DUMP(NULL,
				 "%16.s(%16.s) %16u %16zu 0x%p\n",
				 client->name,
				 "from_kernel",
				 client->pid, size,
				 client);
		}
	}

	if (need_dev_lock)
		up_read(&dev->lock);

	ION_DUMP(NULL, "%s\n", seq_line);
	ION_DUMP(NULL,
		 "orphaned allocations (info is from last known client):\n");

skip_client_entry:

	if (mutex_trylock(&dev->buffer_lock)) {
		for (n = rb_first(&dev->buffers); n; n = rb_next(n)) {
			struct ion_buffer
			*buffer = rb_entry(n, struct ion_buffer, node);
			unsigned int heap_id = buffer->heap->id;

			if ((1 << heap_id) & ION_HEAP_MULTIMEDIA_SEC_MASK) {
				/* heap = buffer->heap; */
				total_size += buffer->size;
				if (!buffer->handle_count) {
					buf_ref = buffer->ref.refcount.refs;
					ION_DUMP(NULL,
						 "%16.s %16u %16zu %d %d\n",
						 buffer->task_comm,
						 buffer->pid,
						 buffer->size,
						 buffer->kmap_cnt,
						 atomic_read(&buf_ref));
					total_orphaned_size += buffer->size;
				}
			}
		}
		mutex_unlock(&dev->buffer_lock);

		ION_DUMP(NULL, "%s\n", seq_line);
		ION_DUMP(NULL,
			 "%16.s %16zu\n", "total orphaned",
			 total_orphaned_size);
		ION_DUMP(NULL,
			 "%16.s %16zu\n", "total ",
			 total_size);
		ION_DUMP(NULL,
			 "ion sec heap total: %16zu\n",
			 sec_heap_total_memory);
		ION_DUMP(NULL, "%s\n", seq_line);
	} else {
		ION_DUMP(NULL,
			 "ion sec heap total memory: %16zu\n",
			 sec_heap_total_memory);
	}
}

struct dump_fd_data {
	struct task_struct *p;
	struct seq_file *s;
};

static int __do_dump_share_fd(const void *data,
			      struct file *file,
			      unsigned int fd)
{
	const struct dump_fd_data *d = data;
	struct seq_file *s = d->s;
	struct task_struct *p = d->p;
	struct ion_buffer *buffer;
	struct ion_sec_buffer_info *bug_info;

	buffer = ion_drv_file_to_buffer(file);
	if (IS_ERR_OR_NULL(buffer) || IS_ERR_OR_NULL(buffer->heap) ||
	    (int)buffer->heap->type != ION_HEAP_TYPE_MULTIMEDIA_SEC)
		return 0;

	bug_info = (struct ion_sec_buffer_info *)buffer->priv_virt;

	if (!bug_info ||
	    buffer->heap->type != (unsigned int)ION_HEAP_TYPE_MULTIMEDIA_SEC)
		return 0;

	if (!buffer->handle_count)
		ION_DUMP(s, "0x%p %9d %16s %5d %5d %16s %4d\n",
			 buffer, bug_info->pid,
			 buffer->alloc_dbg, p->pid,
			 p->tgid, p->comm, fd);

	return 0;
}

static int ion_dump_all_share_fds(struct seq_file *s)
{
	struct task_struct *p;
	int res;
	struct dump_fd_data data;

	ION_DUMP(s, "%18s %9s %16s %5s %5s %16s %4s\n",
		 "buffer", "alloc_pid", "alloc_client",
		 "pid", "tgid", "process", "fd");
	data.s = s;

	read_lock(&tasklist_lock);
	for_each_process(p) {
		task_lock(p);
		data.p = p;
		res = iterate_fd(p->files, 0, __do_dump_share_fd, &data);
		if (res)
			IONMSG("%s failed somehow\n", __func__);
		task_unlock(p);
	}
	read_unlock(&tasklist_lock);
	return 0;
}

static int ion_sec_heap_debug_show(struct ion_heap *heap,
				   struct seq_file *s,
				   void *p)
{
	struct ion_device *dev = heap->dev;
	struct rb_node *n;
	ion_phys_addr_t secur_handle;
	struct ion_sec_buffer_info *bug_info;
	bool has_orphaned = false;
	size_t fr_size = 0;
	size_t sec_size = 0;
	size_t wfd_size = 0;
	size_t prot_size = 0;
	const char *seq_line = "---------------------------------------";

	if (heap->flags & ION_HEAP_FLAG_DEFER_FREE)
		ION_DUMP(s, "sec_heap_freelist total_size=%zu\n",
			 ion_heap_freelist_size(heap));
	else
		ION_DUMP(s, "sec_heap defer free disabled\n");

	ION_DUMP(s, "%s\n", seq_line);
	ION_DUMP(s,
		 "%18.s %8.s %4.s %3.s %8.s %8.s %3.s %4.s %4.s %3.s %10.s %10.s\n",
		 "buffer", "size",
		 "kmap", "ref",
		 "hdl-cnt", "sec_hdl",
		 "flag", "port",
		 "heap_id", "pid(alloc)",
		 "comm(client)", "dbg_name");

	mutex_lock(&dev->buffer_lock);
	for (n = rb_first(&dev->buffers); n; n = rb_next(n)) {
		struct ion_buffer
		*buffer = rb_entry(n, struct ion_buffer, node);
		if (buffer->heap->type != heap->type)
			continue;
		bug_info = (struct ion_sec_buffer_info *)buffer->priv_virt;
		secur_handle = bug_info->priv_phys;

		if ((int)buffer->heap->type ==
			(int)ION_HEAP_TYPE_MULTIMEDIA_SEC) {
			ION_DUMP(s,
				 "0x%p %8zu %3d %3d %3d %10lx %3lu %3d %3d %3d %s %s\n",
				 buffer, buffer->size, buffer->kmap_cnt,
				 atomic_read(&buffer->ref.refcount.refs),
				 buffer->handle_count, secur_handle,
				 buffer->flags, bug_info->module_id,
				 buffer->heap->id,
				 buffer->pid, buffer->task_comm,
				 bug_info->dbg_info.dbg_name);

			if (buffer->heap->id == ION_HEAP_TYPE_MULTIMEDIA_SEC)
				sec_size += buffer->size;
			if (buffer->heap->id == ION_HEAP_TYPE_MULTIMEDIA_PROT)
				prot_size += buffer->size;
			if (buffer->heap->id == ION_HEAP_TYPE_MULTIMEDIA_2D_FR)
				fr_size += buffer->size;
			if (buffer->heap->id == ION_HEAP_TYPE_MULTIMEDIA_WFD)
				wfd_size += buffer->size;

			if (!buffer->handle_count)
				has_orphaned = true;
		}
	}
	if (has_orphaned) {
		ION_DUMP(s,
			 "-----orphaned buffer list:------------------\n");
		ion_dump_all_share_fds(s);
	}
	mutex_unlock(&dev->buffer_lock);
	ION_DUMP(s, "%s\n", seq_line);
	ION_DUMP(s, "%s\n", seq_line);
	ION_DUMP(s, "%16s %16zu\n", "sec-sz:", sec_size);
	ION_DUMP(s, "%16s %16zu\n", "wfd-sz:", wfd_size);
	ION_DUMP(s, "%16s %16zu\n", "prot-sz:", prot_size);
	ION_DUMP(s, "%16s %16zu\n", "2d-fr-sz:", fr_size);
	ION_DUMP(s, "%s\n", seq_line);

	if (!down_read_trylock(&dev->lock)) {
		ION_DUMP(s,
			 "[%s %d] get ion dev read lock fail, try again after 5ms\n",
			 __func__, __LINE__);
		mdelay(5);
		if (!down_read_trylock(&dev->lock)) {
			ION_DUMP(s,
				 "[%s %d] get ion dev lock fail again, bypass client dump\n",
				 __func__, __LINE__);
			goto out;
		}
	}

	for (n = rb_first(&dev->clients); n; n = rb_next(n)) {
		struct ion_client
		*client = rb_entry(n, struct ion_client, node);
		struct rb_node *m;

		if (client->task) {
			char task_comm[TASK_COMM_LEN];

			get_task_comm(task_comm, client->task);
			ION_DUMP(s,
				 "client(0x%p) %s (%s) pid(%u) ================>\n",
				 client, task_comm,
				 client->dbg_name,
				 client->pid);
		} else {
			ION_DUMP(s,
				 "client(0x%p) %s (from_kernel) pid(%u) ================>\n",
				 client, client->name, client->pid);
		}

		{
			mutex_lock(&client->lock);
			for (m = rb_first(&client->handles);
				m; m = rb_next(m)) {
				struct ion_handle
				*handle = rb_entry(m, struct ion_handle, node);

				if (handle->buffer->heap->id ==
					ION_HEAP_TYPE_MULTIMEDIA_SEC ||
				    handle->buffer->heap->id ==
				    ION_HEAP_TYPE_MULTIMEDIA_WFD ||
				    handle->buffer->heap->id ==
				    ION_HEAP_TYPE_MULTIMEDIA_PROT ||
				    handle->buffer->heap->id ==
				    ION_HEAP_TYPE_MULTIMEDIA_2D_FR) {
					ION_DUMP(s,
						 "\thandle=0x%p, buffer=0x%p, heap=%d, fd=%4d, ts: %lldms\n",
						 handle, handle->buffer,
						 handle->buffer->heap->id,
						 handle->dbg.fd,
						 handle->dbg.user_ts);
				}
			}
			mutex_unlock(&client->lock);
		}
	}
	up_read(&dev->lock);
out:
	return 0;
}

struct ion_heap *ion_sec_heap_create(struct ion_platform_heap *heap_data)
{
#if (defined(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM) || \
	defined(CONFIG_MTK_SECURE_MEM_SUPPORT) || \
	defined(MTK_IN_HOUSE_SEC_ION_SUPPORT))

	struct ion_sec_heap *heap;

	IONMSG("%s enter:%s\n", __func__, heap_data->name);

	heap = kzalloc(sizeof(*heap), GFP_KERNEL);
	if (!heap) {
		IONMSG("%s kzalloc failed heap is null.\n", __func__);
		return ERR_PTR(-ENOMEM);
	}
	heap->heap.ops = &mm_sec_heap_ops;
	heap->heap.type = (unsigned int)ION_HEAP_TYPE_MULTIMEDIA_SEC;
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
#if (defined(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM) || \
	defined(CONFIG_MTK_SECURE_MEM_SUPPORT) || \
	defined(MTK_IN_HOUSE_SEC_ION_SUPPORT))

		struct ion_sec_heap *sec_heap;

		IONMSG("%s enter\n", __func__);
		sec_heap = container_of(heap, struct ion_sec_heap, heap);
		kfree(sec_heap);
#else
		IONMSG("%s error: not support\n", __func__);
#endif
}

