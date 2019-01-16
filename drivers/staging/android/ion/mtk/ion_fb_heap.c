/*
 * drivers/gpu/ion/ion_carveout_heap.c
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/ion_drv.h>
#include <linux/mtk_ion.h>
#include <linux/mm.h>
#include <mach/m4u.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/seq_file.h>
#include "ion_priv.h"

/*fb heap base and size denamic access*/
ion_phys_addr_t fb_heap_base;
size_t fb_heap_size;

struct ion_fb_heap {
	struct ion_heap heap;
	struct gen_pool *pool;
	ion_phys_addr_t base;
	size_t size;
};

/*struct ion_fb_buffer_info {
	struct mutex lock;
	unsigned int security;
	unsigned int coherent;
	void *pVA;
	ion_phys_addr_t priv_phys;
	ion_fb_buf_debug_info_t dbg_info;
};*/

static int ion_fb_heap_debug_show(struct ion_heap *heap, struct seq_file *s, void *unused);

/*extern size_t mtkfb_get_fb_size(void);
extern phys_addr_t mtkfb_get_fb_base(void);
int ion_fb_get_fb_heap_base(void)
{
	fb_heap_base = (ion_phys_addr_t)mtkfb_get_fb_base();
	return 0;
}

int ion_fb_get_fb_heap_size(void)
{
	fb_heap_size= mtkfb_get_fb_size();
	return 0;
}*/

ion_phys_addr_t ion_fb_allocate(struct ion_heap *heap,
				      unsigned long size, unsigned long align)
{
	
	struct ion_fb_heap *fb_heap =
	    container_of(heap, struct ion_fb_heap, heap);
	unsigned long offset = gen_pool_alloc(fb_heap->pool, size);

	if (!offset) {
		/*IONMSG("[ion_fb_alloc]:fail! size=0x%x, free=0x%x \n", size,
		       gen_pool_avail(fb_heap->pool));*/
		return ION_CARVEOUT_ALLOCATE_FAIL;
	}

	return offset;
}

void ion_fb_free(struct ion_heap *heap, ion_phys_addr_t addr,
		       unsigned long size)
{
	struct ion_fb_heap *fb_heap =
		container_of(heap, struct ion_fb_heap, heap);

	if (addr == ION_CARVEOUT_ALLOCATE_FAIL)
		return;

	gen_pool_free(fb_heap->pool, addr, size);
}

static int ion_fb_heap_phys(struct ion_heap *heap,
				  struct ion_buffer *buffer,
				  ion_phys_addr_t *addr, size_t *len)
{
	struct sg_table *table=buffer->priv_virt;
	struct page *page=sg_page(table->sgl);
	ion_phys_addr_t paddr=PFN_PHYS(page_to_pfn(page));

	*addr = paddr;
	*len = buffer->size;
	
	return 0;
}

static int ion_fb_heap_allocate(struct ion_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long size, unsigned long align,
				      unsigned long flags)
{
	struct sg_table *table;
	ion_phys_addr_t paddr;
	int ret;

	if (align > PAGE_SIZE)
		return -EINVAL;

	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		return -ENOMEM;
	ret=sg_alloc_table(table,1,GFP_KERNEL);
	if(ret)
		goto err_free;
	
	paddr=ion_fb_allocate(heap,size,align);
	if(paddr==ION_CARVEOUT_ALLOCATE_FAIL)
	{
		ret=-ENOMEM;
		goto err_free_table;
	}
	
	sg_set_page(table->sgl,pfn_to_page(PFN_DOWN(paddr)),size,0);
	return 0;

	err_free_table:
		sg_free_table(table);
	err_free:
		kfree(table);
	return ret;
}

static void ion_fb_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;	
	struct sg_table *table=buffer->priv_virt;
	struct page *page=sg_page(table->sgl);
	ion_phys_addr_t paddr=PFN_PHYS(page_to_pfn(page));

	ion_heap_buffer_zero(buffer);
	
	if(ion_buffer_cached(buffer))
		dma_sync_sg_for_device(NULL,table->sgl,table->nents,DMA_BIDIRECTIONAL);

	ion_fb_free(heap, paddr, buffer->size);
	sg_free_table(table);
	kfree(table);
}

struct sg_table *ion_fb_heap_map_dma(struct ion_heap *heap,
					      struct ion_buffer *buffer)
{	
	return buffer->priv_virt;
}

void ion_fb_heap_unmap_dma(struct ion_heap *heap,
				 struct ion_buffer *buffer)
{
	return;
}

static struct ion_heap_ops fb_heap_ops = {
	.allocate = ion_fb_heap_allocate,
	.free = ion_fb_heap_free,
	.phys = ion_fb_heap_phys,
	.map_dma = ion_fb_heap_map_dma,
	.unmap_dma = ion_fb_heap_unmap_dma,
	.map_user = ion_heap_map_user,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
};

#define ION_PRINT_LOG_OR_SEQ(seq_file, fmt, args...) \
		do {\
			if (seq_file)\
				seq_printf(seq_file, fmt, ##args);\
			else\
				printk(fmt, ##args);\
		} while (0)


static int ion_fb_heap_debug_show(struct ion_heap *heap, struct seq_file *s, void *unused)
{
	struct ion_fb_heap *fb_heap =
			container_of(heap, struct ion_fb_heap, heap);
	size_t size_avail, total_size;

	total_size = gen_pool_size(fb_heap->pool);
	size_avail = gen_pool_avail(fb_heap->pool);

	/*seq_printf(s, "total_size=0x%x, free=0x%x, base=0x%x\n",
			total_size, size_avail, (unsigned int)fb_heap->base);*/

	return 0;
}

struct ion_heap *ion_fb_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_fb_heap *fb_heap;
	int ret;

	struct page *page;
	size_t size;

	page=pfn_to_page(PFN_DOWN(heap_data->base));
	size=heap_data->size;

	ion_pages_sync_for_device(NULL, page, size, DMA_BIDIRECTIONAL);

	ret=ion_heap_pages_zero(page,size, pgprot_writecombine(PAGE_KERNEL));
	if (ret)
		return ERR_PTR(ret);
	
	fb_heap = kzalloc(sizeof(struct ion_fb_heap), GFP_KERNEL);
	if (!fb_heap)
		return ERR_PTR(-ENOMEM);

	fb_heap->pool = gen_pool_create(12, -1);
	if (!fb_heap->pool) {
		kfree(fb_heap);
		return ERR_PTR(-ENOMEM);
	}
	//ion_fb_get_fb_heap_base();
	//ion_fb_get_fb_heap_size();
	fb_heap->base = heap_data->base;//fb_heap_base;
	fb_heap->size = 0;//fb_heap_size;
	gen_pool_add(fb_heap->pool, fb_heap->base, fb_heap->size,
		     -1);
	fb_heap->heap.ops = &fb_heap_ops;
	fb_heap->heap.type = ION_HEAP_TYPE_FB;
	fb_heap->heap.flags = ION_HEAP_FLAG_DEFER_FREE;
	fb_heap->heap.debug_show = ion_fb_heap_debug_show;
	
	return &fb_heap->heap;
}

void ion_fb_heap_destroy(struct ion_heap *heap)
{	
	
	struct ion_fb_heap *fb_heap =
	     container_of(heap, struct  ion_fb_heap, heap);

	gen_pool_destroy(fb_heap->pool);
	kfree(fb_heap);
	fb_heap = NULL;
}

long ion_fb_heap_ioctl(struct ion_client *client, unsigned int cmd, unsigned long arg, int from_kernel)
{
	
	ion_fb_data_t Param;
	long ret = 0;
	
	unsigned long ret_copy;
	if (from_kernel)
		Param = *(ion_fb_data_t *) arg;
	else
		ret_copy = copy_from_user(&Param, (void __user *)arg, sizeof(ion_fb_data_t));

	switch (Param.fb_cmd) {
	case ION_FB_SET_DEBUG_INFO:
		{
			struct ion_buffer *buffer;
			if (Param.buf_debug_info_param.handle) {
				struct ion_handle *kernel_handle;
				kernel_handle = ion_drv_get_handle(client, 0,
									  Param.
									  buf_debug_info_param.
									  handle, from_kernel);
				if (IS_ERR(kernel_handle)) {
					IONMSG("[ion_fb_heap_ioctl]: ion config buffer fail!");
					ret = -EINVAL;
					break;
				}

				buffer = ion_handle_buffer(kernel_handle);
				if ((int)buffer->heap->type == ION_HEAP_TYPE_FB) {
					char *Msg = "buffer that is from fb carveout heap.";
					IONMSG("[ion_fb_heap_ioctl]: success. %s.\n", Msg);
					ret = -EFAULT;
				}
			} 
			else {
				IONMSG
				("[ion_fb_heap_ioctl]: Error. set dbg buffer with invalid handle.\n");
				ret = -EFAULT;
			}
		}
		break;

	case ION_FB_GET_DEBUG_INFO:
		{
			struct ion_buffer *buffer;
			if (Param.buf_debug_info_param.handle) {
				struct ion_handle *kernel_handle;
				kernel_handle = ion_drv_get_handle(client, 0,
									  Param.
									  buf_debug_info_param.
									  handle, from_kernel);
				if (IS_ERR(kernel_handle)) {
					IONMSG("[ion_fb_heap_ioctl]: ion config buffer fail! ");
					ret = -EINVAL;
					break;
				}
				buffer = ion_handle_buffer(kernel_handle);
				if ((int)buffer->heap->type == ION_HEAP_TYPE_FB) {
					char *Msg = "buffer that is not from fb carveout heap.";
					IONMSG("[ion_fb_heap_ioctl]: success. %s.\n", Msg);
					ret = -EFAULT;
				}
			} 
			else {
				IONMSG
				("[ion_fb_heap_ioctl]: Error. set dbg buffer with invalid handle.\n");
				ret = -EFAULT;
			}
		}
		break;

	default:
		IONMSG("[ion_fb_heap_ioctl]: Error. Invalid command.\n");
		ret = -EFAULT;
	}
	if (from_kernel)
		*(ion_fb_data_t *) arg = Param;
	else
		ret_copy = copy_to_user((void __user *)arg, &Param, sizeof(ion_fb_data_t));
	return ret;
}

