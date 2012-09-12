/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/memory_alloc.h>
#include <media/msm/vidc_type.h>
#include "vcd_ddl_utils.h"
#include "vcd_res_tracker_api.h"

#if DEBUG
#define DBG(x...) printk(KERN_DEBUG x)
#else
#define DBG(x...)
#endif

#define DBG_TIME(x...) printk(KERN_DEBUG x)
#define ERR(x...) printk(KERN_ERR x)

struct time_data {
	unsigned int ddl_t1;
	unsigned int ddl_ttotal;
	unsigned int ddl_count;
};

static struct time_data proc_time[MAX_TIME_DATA];

#ifdef NO_IN_KERNEL_PMEM

void ddl_pmem_alloc(struct ddl_buf_addr *buff_addr, size_t sz, u32 align)
{
	u32 guard_bytes, align_mask;
	u32 physical_addr, align_offset;
	dma_addr_t phy_addr;

	if (align == DDL_LINEAR_BUFFER_ALIGN_BYTES) {

		guard_bytes = 31;
		align_mask = 0xFFFFFFE0U;

	} else {

		guard_bytes = DDL_TILE_BUF_ALIGN_GUARD_BYTES;
		align_mask = DDL_TILE_BUF_ALIGN_MASK;
	}

	buff_addr->virtual_base_addr =
		kmalloc((sz + guard_bytes), GFP_KERNEL);

	if (!buff_addr->virtual_base_addr) {
		ERR("\n ERROR %s:%u kamlloc fails to allocate"
			" sz + guard_bytes = %u\n", __func__, __LINE__,
			(sz + guard_bytes));
		return;
	}

	phy_addr = dma_map_single(NULL, buff_addr->virtual_base_addr,
				  sz + guard_bytes, DMA_TO_DEVICE);

	buff_addr->buffer_size = sz;
	physical_addr = (u32) phy_addr;
	buff_addr->align_physical_addr =
	    (u32 *) ((physical_addr + guard_bytes) & align_mask);
	align_offset =
	    (u32) (buff_addr->align_physical_addr) - physical_addr;
	buff_addr->align_virtual_addr =
	    (u32 *) ((u32) (buff_addr->virtual_base_addr)
		     + align_offset);
}

void ddl_pmem_free(struct ddl_buf_addr *buff_addr)
{
	kfree(buff_addr->virtual_base_addr);
	buff_addr->buffer_size = 0;
	buff_addr->virtual_base_addr = NULL;
}

#else

void ddl_pmem_alloc(struct ddl_buf_addr *buff_addr, size_t sz, u32 align)
{
	u32 guard_bytes, align_mask;
	u32 physical_addr;
	u32 align_offset;
	u32 alloc_size, flags = 0;
	struct ddl_context *ddl_context;
	struct msm_mapped_buffer *mapped_buffer = NULL;
	unsigned long *kernel_vaddr = NULL;
	ion_phys_addr_t phyaddr = 0;
	size_t len = 0;
	int ret = -EINVAL;

	if (!buff_addr) {
		ERR("\n%s() Invalid Parameters\n", __func__);
		return;
	}
	if (align == DDL_LINEAR_BUFFER_ALIGN_BYTES) {
		guard_bytes = 31;
		align_mask = 0xFFFFFFE0U;
	} else {
		guard_bytes = DDL_TILE_BUF_ALIGN_GUARD_BYTES;
		align_mask = DDL_TILE_BUF_ALIGN_MASK;
	}
	ddl_context = ddl_get_context();
	alloc_size = sz + guard_bytes;
	if (res_trk_get_enable_ion()) {
		if (!ddl_context->video_ion_client)
			ddl_context->video_ion_client =
				res_trk_get_ion_client();
		if (!ddl_context->video_ion_client) {
			ERR("\n%s(): DDL ION Client Invalid handle\n",
				__func__);
			goto bailout;
		}
		buff_addr->mem_type = res_trk_get_mem_type();
		buff_addr->alloc_handle = ion_alloc(
					ddl_context->video_ion_client,
					alloc_size,
					SZ_4K,
					buff_addr->mem_type, 0);
		if (!buff_addr->alloc_handle) {
			ERR("\n%s(): DDL ION alloc failed\n",
					__func__);
			goto bailout;
		}
		ret = ion_phys(ddl_context->video_ion_client,
					buff_addr->alloc_handle,
					&phyaddr,
					&len);
		if (ret || !phyaddr) {
			ERR("\n%s(): DDL ION client physical failed\n",
					__func__);
			goto free_ion_buffer;
		}
		buff_addr->physical_base_addr = (u32 *)phyaddr;
		kernel_vaddr = (unsigned long *) ion_map_kernel(
					ddl_context->video_ion_client,
					buff_addr->alloc_handle);
		if (IS_ERR_OR_NULL(kernel_vaddr)) {
			ERR("\n%s(): DDL ION map failed\n", __func__);
			goto unmap_ion_buffer;
		}
		buff_addr->virtual_base_addr = (u32 *)kernel_vaddr;
		DBG("ddl_ion_alloc: handle(0x%x), mem_type(0x%x), "\
			"phys(0x%x), virt(0x%x), size(%u), align(%u), "\
			"alloced_len(%u)", (u32)buff_addr->alloc_handle,
			(u32)buff_addr->mem_type,
			(u32)buff_addr->physical_base_addr,
			(u32)buff_addr->virtual_base_addr,
			alloc_size, align, len);
	} else {
		physical_addr = (u32)
			allocate_contiguous_memory_nomap(alloc_size,
						ddl_context->memtype, SZ_4K);
		if (!physical_addr) {
			ERR("\n%s(): DDL pmem allocate failed\n",
			       __func__);
			goto bailout;
		}
		buff_addr->physical_base_addr = (u32 *) physical_addr;
		flags = MSM_SUBSYSTEM_MAP_KADDR;
		buff_addr->mapped_buffer =
		msm_subsystem_map_buffer((unsigned long)physical_addr,
		alloc_size, flags, NULL, 0);
		if (IS_ERR(buff_addr->mapped_buffer)) {
			ERR("\n%s() buffer map failed\n", __func__);
			goto free_pmem_buffer;
		}
		mapped_buffer = buff_addr->mapped_buffer;
		if (!mapped_buffer->vaddr) {
			ERR("\n%s() mapped virtual address is NULL\n",
				__func__);
			goto unmap_pmem_buffer;
		}
		buff_addr->virtual_base_addr = mapped_buffer->vaddr;
		DBG("ddl_pmem_alloc: mem_type(0x%x), phys(0x%x),"\
			" virt(0x%x), sz(%u), align(%u)",
			(u32)buff_addr->mem_type,
			(u32)buff_addr->physical_base_addr,
			(u32)buff_addr->virtual_base_addr,
			alloc_size, SZ_4K);
	}

	memset(buff_addr->virtual_base_addr, 0 , sz + guard_bytes);
	buff_addr->buffer_size = sz;
	buff_addr->align_physical_addr = (u32 *)
		(((u32)buff_addr->physical_base_addr + guard_bytes) &
		align_mask);
	align_offset = (u32) (buff_addr->align_physical_addr) -
		(u32)buff_addr->physical_base_addr;
	buff_addr->align_virtual_addr =
	    (u32 *) ((u32) (buff_addr->virtual_base_addr)
		     + align_offset);
	DBG("%s(): phys(0x%x) align_phys(0x%x), virt(0x%x),"\
		" align_virt(0x%x)", __func__,
		(u32)buff_addr->physical_base_addr,
		(u32)buff_addr->align_physical_addr,
		(u32)buff_addr->virtual_base_addr,
		(u32)buff_addr->align_virtual_addr);
	return;

unmap_pmem_buffer:
	if (buff_addr->mapped_buffer)
		msm_subsystem_unmap_buffer(buff_addr->mapped_buffer);
free_pmem_buffer:
	if (buff_addr->physical_base_addr)
		free_contiguous_memory_by_paddr((unsigned long)
			buff_addr->physical_base_addr);
	memset(buff_addr, 0, sizeof(struct ddl_buf_addr));
	return;

unmap_ion_buffer:
	if (ddl_context->video_ion_client) {
		if (buff_addr->alloc_handle)
			ion_unmap_kernel(ddl_context->video_ion_client,
				buff_addr->alloc_handle);
	}
free_ion_buffer:
	if (ddl_context->video_ion_client) {
		if (buff_addr->alloc_handle)
			ion_free(ddl_context->video_ion_client,
				buff_addr->alloc_handle);
	}
bailout:
	memset(buff_addr, 0, sizeof(struct ddl_buf_addr));
}

void ddl_pmem_free(struct ddl_buf_addr *buff_addr)
{
	struct ddl_context *ddl_context;
	ddl_context = ddl_get_context();
	if (!buff_addr) {
		ERR("\n %s() invalid arguments %p", __func__, buff_addr);
		return;
	}
	DBG("ddl_pmem_free: phys(0x%x) align_phys(0x%x), "\
		"virt(0x%x), align_virt(0x%x), size(%u)",
		(u32)buff_addr->physical_base_addr,
		(u32)buff_addr->align_physical_addr,
		(u32)buff_addr->virtual_base_addr,
		(u32)buff_addr->align_virtual_addr,
		buff_addr->buffer_size);
	if (ddl_context->video_ion_client) {
		if (buff_addr->alloc_handle) {
			ion_unmap_kernel(ddl_context->video_ion_client,
				buff_addr->alloc_handle);
			ion_free(ddl_context->video_ion_client,
				buff_addr->alloc_handle);
		}
	} else {
		if (buff_addr->mapped_buffer)
			msm_subsystem_unmap_buffer(
				buff_addr->mapped_buffer);
		if (buff_addr->physical_base_addr)
			free_contiguous_memory_by_paddr((unsigned long)
				buff_addr->physical_base_addr);
	}
	memset(buff_addr, 0, sizeof(struct ddl_buf_addr));
}
#endif

void ddl_set_core_start_time(const char *func_name, u32 index)
{
	u32 act_time;
	struct timeval ddl_tv;
	struct time_data *time_data = &proc_time[index];
	do_gettimeofday(&ddl_tv);
	act_time = (ddl_tv.tv_sec * 1000) + (ddl_tv.tv_usec / 1000);
	if (!time_data->ddl_t1) {
		time_data->ddl_t1 = act_time;
		DBG("\n%s(): Start Time (%u)", func_name, act_time);
	} else {
		DBG_TIME("\n%s(): Timer already started! St(%u) Act(%u)",
			func_name, time_data->ddl_t1, act_time);
	}
}

void ddl_calc_core_proc_time(const char *func_name, u32 index)
{
	struct time_data *time_data = &proc_time[index];
	if (time_data->ddl_t1) {
		int ddl_t2;
		struct timeval ddl_tv;
		do_gettimeofday(&ddl_tv);
		ddl_t2 = (ddl_tv.tv_sec * 1000) + (ddl_tv.tv_usec / 1000);
		time_data->ddl_ttotal += (ddl_t2 - time_data->ddl_t1);
		time_data->ddl_count++;
		DBG_TIME("\n%s(): cnt(%u) Diff(%u) Avg(%u)",
			func_name, time_data->ddl_count,
			ddl_t2 - time_data->ddl_t1,
			time_data->ddl_ttotal/time_data->ddl_count);
		time_data->ddl_t1 = 0;
	}
}

void ddl_reset_core_time_variables(u32 index)
{
	proc_time[index].ddl_t1 = 0;
	proc_time[index].ddl_ttotal = 0;
	proc_time[index].ddl_count = 0;
}
int ddl_get_core_decode_proc_time(u32 *ddl_handle)
{
	return 0;
}

void ddl_reset_avg_dec_time(u32 *ddl_handle)
{
	return;
}
