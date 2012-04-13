/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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
#include <mach/msm_subsystem_map.h>
#include <asm/div64.h>
#include <media/msm/vidc_type.h>
#include "vcd.h"
#include "vdec_internal.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MAP_TABLE_SZ 64

struct vcd_msm_map_buffer {
	phys_addr_t phy_addr;
	struct msm_mapped_buffer *mapped_buffer;
	struct ion_handle *alloc_handle;
	u32 in_use;
};
static struct vcd_msm_map_buffer msm_mapped_buffer_table[MAP_TABLE_SZ];
static unsigned int vidc_mmu_subsystem[] = {MSM_SUBSYSTEM_VIDEO};

static int vcd_pmem_alloc(size_t sz, u8 **kernel_vaddr, u8 **phy_addr,
			 struct vcd_clnt_ctxt *cctxt)
{
	u32 memtype, i = 0, flags = 0;
	struct vcd_msm_map_buffer *map_buffer = NULL;
	struct msm_mapped_buffer *mapped_buffer = NULL;
	unsigned long iova = 0;
	unsigned long buffer_size = 0;
	int ret = 0;
	unsigned long ionflag = 0;

	if (!kernel_vaddr || !phy_addr || !cctxt) {
		pr_err("\n%s: Invalid parameters", __func__);
		goto bailout;
	}
	*phy_addr = NULL;
	*kernel_vaddr = NULL;
	for (i = 0; i  < MAP_TABLE_SZ; i++) {
		if (!msm_mapped_buffer_table[i].in_use) {
			map_buffer = &msm_mapped_buffer_table[i];
			map_buffer->in_use = 1;
			break;
		}
	}
	if (!map_buffer) {
		pr_err("%s() map table is full", __func__);
		goto bailout;
	}
	res_trk_set_mem_type(DDL_MM_MEM);
	memtype = res_trk_get_mem_type();
	if (!cctxt->vcd_enable_ion) {
		map_buffer->phy_addr = (phys_addr_t)
		allocate_contiguous_memory_nomap(sz, memtype, SZ_4K);
		if (!map_buffer->phy_addr) {
			pr_err("%s() acm alloc failed", __func__);
			goto free_map_table;
		}
		flags = MSM_SUBSYSTEM_MAP_IOVA | MSM_SUBSYSTEM_MAP_KADDR;
		map_buffer->mapped_buffer =
		msm_subsystem_map_buffer((unsigned long)map_buffer->phy_addr,
		sz, flags, vidc_mmu_subsystem,
		sizeof(vidc_mmu_subsystem)/sizeof(unsigned int));
		if (IS_ERR(map_buffer->mapped_buffer)) {
			pr_err(" %s() buffer map failed", __func__);
			goto free_acm_alloc;
		}
		mapped_buffer = map_buffer->mapped_buffer;
		if (!mapped_buffer->vaddr || !mapped_buffer->iova[0]) {
			pr_err("%s() map buffers failed", __func__);
			goto free_map_buffers;
		}
		*phy_addr = (u8 *) mapped_buffer->iova[0];
		*kernel_vaddr = (u8 *) mapped_buffer->vaddr;
	} else {
		map_buffer->alloc_handle = ion_alloc(
			    cctxt->vcd_ion_client, sz, SZ_4K,
			    memtype);
		if (!map_buffer->alloc_handle) {
			pr_err("%s() ION alloc failed", __func__);
			goto bailout;
		}
		if (ion_handle_get_flags(cctxt->vcd_ion_client,
				map_buffer->alloc_handle,
				&ionflag)) {
			pr_err("%s() ION get flag failed", __func__);
			goto bailout;
		}
		*kernel_vaddr = (u8 *) ion_map_kernel(
				cctxt->vcd_ion_client,
				map_buffer->alloc_handle,
				ionflag);
		if (!(*kernel_vaddr)) {
			pr_err("%s() ION map failed", __func__);
			goto ion_free_bailout;
		}
		ret = ion_map_iommu(cctxt->vcd_ion_client,
				map_buffer->alloc_handle,
				VIDEO_DOMAIN,
				VIDEO_MAIN_POOL,
				SZ_4K,
				0,
				(unsigned long *)&iova,
				(unsigned long *)&buffer_size,
				UNCACHED, 0);
		if (ret) {
			pr_err("%s() ION iommu map failed", __func__);
			goto ion_map_bailout;
		}
		map_buffer->phy_addr = iova;
		if (!map_buffer->phy_addr) {
			pr_err("%s() acm alloc failed", __func__);
			goto free_map_table;
		}
		*phy_addr = (u8 *)iova;
		mapped_buffer = NULL;
		map_buffer->mapped_buffer = NULL;
	}

	return 0;

free_map_buffers:
	if (map_buffer->mapped_buffer)
		msm_subsystem_unmap_buffer(map_buffer->mapped_buffer);
free_acm_alloc:
	if (!cctxt->vcd_enable_ion) {
		free_contiguous_memory_by_paddr(
		(unsigned long)map_buffer->phy_addr);
	}
	return -ENOMEM;
ion_map_bailout:
	ion_unmap_kernel(cctxt->vcd_ion_client, map_buffer->alloc_handle);
ion_free_bailout:
	ion_free(cctxt->vcd_ion_client, map_buffer->alloc_handle);
free_map_table:
	map_buffer->in_use = 0;
bailout:
	return -ENOMEM;
}

static int vcd_pmem_free(u8 *kernel_vaddr, u8 *phy_addr,
			 struct vcd_clnt_ctxt *cctxt)
{
	u32 i = 0;
	struct vcd_msm_map_buffer *map_buffer = NULL;

	if (!kernel_vaddr || !phy_addr || !cctxt) {
		pr_err("\n%s: Invalid parameters", __func__);
		goto bailout;
	}
	for (i = 0; i  < MAP_TABLE_SZ; i++) {
		if (msm_mapped_buffer_table[i].in_use &&
			(msm_mapped_buffer_table[i]
			.mapped_buffer->vaddr == kernel_vaddr)) {
			map_buffer = &msm_mapped_buffer_table[i];
			map_buffer->in_use = 0;
			break;
		}
	}
	if (!map_buffer) {
		pr_err("%s() Entry not found", __func__);
		goto bailout;
	}
	if (map_buffer->mapped_buffer)
		msm_subsystem_unmap_buffer(map_buffer->mapped_buffer);
	if (cctxt->vcd_enable_ion) {
		if (map_buffer->alloc_handle) {
			ion_unmap_kernel(cctxt->vcd_ion_client,
					map_buffer->alloc_handle);
			ion_unmap_iommu(cctxt->vcd_ion_client,
					map_buffer->alloc_handle,
					VIDEO_DOMAIN,
					VIDEO_MAIN_POOL);
			ion_free(cctxt->vcd_ion_client,
			map_buffer->alloc_handle);
		}
	} else {
		free_contiguous_memory_by_paddr(
			(unsigned long)map_buffer->phy_addr);
	}
bailout:
	kernel_vaddr = NULL;
	phy_addr = NULL;
	return 0;
}


u8 *vcd_pmem_get_physical(struct video_client_ctx *client_ctx,
			  unsigned long kernel_vaddr)
{
	unsigned long phy_addr, user_vaddr;
	int pmem_fd;
	struct file *file;
	s32 buffer_index = -1;

	if (vidc_lookup_addr_table(client_ctx, BUFFER_TYPE_INPUT,
					  false, &user_vaddr, &kernel_vaddr,
					  &phy_addr, &pmem_fd, &file,
					  &buffer_index)) {

		return (u8 *) phy_addr;
	} else if (vidc_lookup_addr_table(client_ctx, BUFFER_TYPE_OUTPUT,
		false, &user_vaddr, &kernel_vaddr, &phy_addr, &pmem_fd, &file,
		&buffer_index)) {
		return (u8 *) phy_addr;
	} else {
		VCD_MSG_ERROR("Couldn't get physical address");

		return NULL;
	}

}

u32 vcd_get_ion_flag(struct video_client_ctx *client_ctx,
			  unsigned long kernel_vaddr,
			struct ion_handle **buff_ion_handle)
{
	unsigned long phy_addr, user_vaddr;
	int pmem_fd;
	struct file *file;
	s32 buffer_index = -1;
	u32 ion_flag = 0;
	struct ion_handle *buff_handle = NULL;

	if (vidc_lookup_addr_table(client_ctx, BUFFER_TYPE_INPUT,
					  false, &user_vaddr, &kernel_vaddr,
					  &phy_addr, &pmem_fd, &file,
					  &buffer_index)) {

		ion_flag = vidc_get_fd_info(client_ctx, BUFFER_TYPE_INPUT,
				pmem_fd, kernel_vaddr, buffer_index,
				&buff_handle);
		*buff_ion_handle = buff_handle;
		return ion_flag;
	} else if (vidc_lookup_addr_table(client_ctx, BUFFER_TYPE_OUTPUT,
		false, &user_vaddr, &kernel_vaddr, &phy_addr, &pmem_fd, &file,
		&buffer_index)) {
		ion_flag = vidc_get_fd_info(client_ctx, BUFFER_TYPE_OUTPUT,
				pmem_fd, kernel_vaddr, buffer_index,
				&buff_handle);
		*buff_ion_handle = buff_handle;
		return ion_flag;
	} else {
		VCD_MSG_ERROR("Couldn't get ion flag");
		return 0;
	}

}

void vcd_reset_device_channels(struct vcd_dev_ctxt *dev_ctxt)
{
	dev_ctxt->ddl_frame_ch_free = dev_ctxt->ddl_frame_ch_depth;
	dev_ctxt->ddl_cmd_ch_free   = dev_ctxt->ddl_cmd_ch_depth;
	dev_ctxt->ddl_frame_ch_interim = 0;
	dev_ctxt->ddl_cmd_ch_interim = 0;
}

u32 vcd_get_command_channel(
	struct vcd_dev_ctxt *dev_ctxt,
	 struct vcd_transc **transc)
{
	u32 result = false;

	*transc = NULL;

	if (dev_ctxt->ddl_cmd_ch_free > 0) {
		if (dev_ctxt->ddl_cmd_concurrency) {
			--dev_ctxt->ddl_cmd_ch_free;
			result = true;
		} else if ((dev_ctxt->ddl_frame_ch_free +
			 dev_ctxt->ddl_frame_ch_interim)
			== dev_ctxt->ddl_frame_ch_depth) {
				--dev_ctxt->ddl_cmd_ch_free;
				result = true;
		}
	}

	if (result) {
		*transc = vcd_get_free_trans_tbl_entry(dev_ctxt);

		if (!*transc) {
			result = false;

			vcd_release_command_channel(dev_ctxt, *transc);
		}

	}
	return result;
}

u32 vcd_get_command_channel_in_loop(
	struct vcd_dev_ctxt *dev_ctxt,
	 struct vcd_transc **transc)
{
	u32 result = false;

	*transc = NULL;

	if (dev_ctxt->ddl_cmd_ch_interim > 0) {
		if (dev_ctxt->ddl_cmd_concurrency) {
			--dev_ctxt->ddl_cmd_ch_interim;
			result = true;
		} else if ((dev_ctxt->ddl_frame_ch_free +
				dev_ctxt->ddl_frame_ch_interim)
				== dev_ctxt->ddl_frame_ch_depth) {
				--dev_ctxt->ddl_cmd_ch_interim;
				result = true;
		}
	} else {
		result = vcd_get_command_channel(dev_ctxt, transc);
	}

	if (result && !*transc) {
		*transc = vcd_get_free_trans_tbl_entry(dev_ctxt);

		if (!*transc) {
			result = false;

			++dev_ctxt->ddl_cmd_ch_interim;
		}

	}

	return result;
}

void vcd_mark_command_channel(struct vcd_dev_ctxt *dev_ctxt,
	struct vcd_transc *transc)
{
	++dev_ctxt->ddl_cmd_ch_interim;

	vcd_release_trans_tbl_entry(transc);
	if (dev_ctxt->ddl_cmd_ch_interim +
		dev_ctxt->ddl_cmd_ch_free >
		dev_ctxt->ddl_cmd_ch_depth) {
		VCD_MSG_ERROR("\n Command channel access counters messed up");
	}
}

void vcd_release_command_channel(
	struct vcd_dev_ctxt *dev_ctxt, struct vcd_transc *transc)
{
	++dev_ctxt->ddl_cmd_ch_free;

	vcd_release_trans_tbl_entry(transc);
	if (dev_ctxt->ddl_cmd_ch_interim + dev_ctxt->ddl_cmd_ch_free >
		dev_ctxt->ddl_cmd_ch_depth) {
		VCD_MSG_ERROR("\n Command channel access counters messed up");
	}
}

void vcd_release_multiple_command_channels(struct vcd_dev_ctxt
	*dev_ctxt, u32 channels)
{
	dev_ctxt->ddl_cmd_ch_free += channels;

	if (dev_ctxt->ddl_cmd_ch_interim +
		dev_ctxt->ddl_cmd_ch_free >
		dev_ctxt->ddl_cmd_ch_depth) {
		VCD_MSG_ERROR("\n Command channel access counters messed up");
	}
}

void vcd_release_interim_command_channels(struct vcd_dev_ctxt *dev_ctxt)
{
	dev_ctxt->ddl_cmd_ch_free += dev_ctxt->ddl_cmd_ch_interim;
	dev_ctxt->ddl_cmd_ch_interim = 0;

	if (dev_ctxt->ddl_cmd_ch_interim + dev_ctxt->ddl_cmd_ch_free >
		dev_ctxt->ddl_cmd_ch_depth) {
		VCD_MSG_ERROR("\n Command channel access counters messed up");
	}
}

u32 vcd_get_frame_channel(struct vcd_dev_ctxt *dev_ctxt,
	struct vcd_transc **transc)
{
	u32 result = false;

	if (dev_ctxt->ddl_frame_ch_free > 0) {
		if (dev_ctxt->ddl_cmd_concurrency) {
			--dev_ctxt->ddl_frame_ch_free;
			result = true;
		} else if ((dev_ctxt->ddl_cmd_ch_free +
			 dev_ctxt->ddl_cmd_ch_interim)
			== dev_ctxt->ddl_cmd_ch_depth) {
			--dev_ctxt->ddl_frame_ch_free;
			result = true;
		}
	}

	if (result) {
		*transc = vcd_get_free_trans_tbl_entry(dev_ctxt);

		if (!*transc) {
			result = false;

			vcd_release_frame_channel(dev_ctxt, *transc);
		} else {
			(*transc)->type = VCD_CMD_CODE_FRAME;
		}

	}

	return result;
}

u32 vcd_get_frame_channel_in_loop(
	struct vcd_dev_ctxt *dev_ctxt,
	 struct vcd_transc **transc)
{
	u32 result = false;

	*transc = NULL;

	if (dev_ctxt->ddl_frame_ch_interim > 0) {
		if (dev_ctxt->ddl_cmd_concurrency) {
			--dev_ctxt->ddl_frame_ch_interim;
			result = true;
		} else if ((dev_ctxt->ddl_cmd_ch_free +
			 dev_ctxt->ddl_cmd_ch_interim)
			== dev_ctxt->ddl_cmd_ch_depth) {
			--dev_ctxt->ddl_frame_ch_interim;
			result = true;
		}
	} else {
		result = vcd_get_frame_channel(dev_ctxt, transc);
	}

	if (result && !*transc) {
		*transc = vcd_get_free_trans_tbl_entry(dev_ctxt);

		if (!*transc) {
			result = false;
			VCD_MSG_FATAL("\n%s: All transactions are busy;"
				"Couldnt find free one\n", __func__);
			++dev_ctxt->ddl_frame_ch_interim;
		} else
			(*transc)->type = VCD_CMD_CODE_FRAME;
	}

	return result;
}

void vcd_mark_frame_channel(struct vcd_dev_ctxt *dev_ctxt)
{
	++dev_ctxt->ddl_frame_ch_interim;

	if (dev_ctxt->ddl_frame_ch_interim +
		dev_ctxt->ddl_frame_ch_free >
		dev_ctxt->ddl_cmd_ch_depth) {
		VCD_MSG_FATAL("Frame channel access counters messed up");
	}
}

void vcd_release_frame_channel(struct vcd_dev_ctxt *dev_ctxt,
	struct vcd_transc *transc)
{
	++dev_ctxt->ddl_frame_ch_free;

	vcd_release_trans_tbl_entry(transc);

	if (dev_ctxt->ddl_frame_ch_interim +
		dev_ctxt->ddl_frame_ch_free >
		dev_ctxt->ddl_cmd_ch_depth) {
		VCD_MSG_FATAL("Frame channel access counters messed up");
	}
}

void vcd_release_multiple_frame_channels(struct vcd_dev_ctxt
	*dev_ctxt, u32 channels)
{
	dev_ctxt->ddl_frame_ch_free += channels;

	if (dev_ctxt->ddl_frame_ch_interim +
		dev_ctxt->ddl_frame_ch_free >
		dev_ctxt->ddl_frame_ch_depth) {
		VCD_MSG_FATAL("Frame channel access counters messed up");
	}
}

void vcd_release_interim_frame_channels(struct vcd_dev_ctxt
	*dev_ctxt)
{
	dev_ctxt->ddl_frame_ch_free +=
		dev_ctxt->ddl_frame_ch_interim;
	dev_ctxt->ddl_frame_ch_interim = 0;

	if (dev_ctxt->ddl_frame_ch_free >
		dev_ctxt->ddl_cmd_ch_depth) {
		VCD_MSG_FATAL("Frame channel access counters messed up");
	}
}

u32 vcd_core_is_busy(struct vcd_dev_ctxt *dev_ctxt)
{
	if (((dev_ctxt->ddl_cmd_ch_free +
		  dev_ctxt->ddl_cmd_ch_interim) !=
		 dev_ctxt->ddl_cmd_ch_depth)
		||
		((dev_ctxt->ddl_frame_ch_free +
		  dev_ctxt->ddl_frame_ch_interim) !=
		 dev_ctxt->ddl_frame_ch_depth)
	  ) {
		return true;
	} else {
		return false;
	}
}

void vcd_device_timer_start(struct vcd_dev_ctxt *dev_ctxt)
{
	if (dev_ctxt->config.timer_start)
		dev_ctxt->config.timer_start(dev_ctxt->hw_timer_handle,
			dev_ctxt->hw_time_out);
}

void vcd_device_timer_stop(struct vcd_dev_ctxt *dev_ctxt)
{
	if (dev_ctxt->config.timer_stop)
		dev_ctxt->config.timer_stop(dev_ctxt->hw_timer_handle);
}


u32 vcd_common_allocate_set_buffer(
	struct vcd_clnt_ctxt *cctxt,
	 enum vcd_buffer_type buffer,
	 u32 buf_size, struct vcd_buffer_pool **buffer_pool)
{
	u32 rc = VCD_S_SUCCESS;
	struct vcd_buffer_requirement Buf_req;
	struct vcd_property_hdr Prop_hdr;
	struct vcd_buffer_pool *buf_pool;

	if (buffer == VCD_BUFFER_INPUT) {
		Prop_hdr.prop_id = DDL_I_INPUT_BUF_REQ;
		buf_pool = &cctxt->in_buf_pool;
	} else if (buffer == VCD_BUFFER_OUTPUT) {
		Prop_hdr.prop_id = DDL_I_OUTPUT_BUF_REQ;
		buf_pool = &cctxt->out_buf_pool;
	} else {
		rc = VCD_ERR_ILLEGAL_PARM;
	}
	VCD_FAILED_RETURN(rc, "Invalid buffer type provided");

	*buffer_pool = buf_pool;

	if (buf_pool->count > 0 &&
		buf_pool->validated == buf_pool->count) {
		VCD_MSG_ERROR("Buffer pool is full");
		return VCD_ERR_FAIL;
	}

	if (!buf_pool->entries) {
		Prop_hdr.sz = sizeof(Buf_req);
		rc = ddl_get_property(cctxt->ddl_handle, &Prop_hdr, &Buf_req);
		if (!VCD_FAILED(rc)) {
			rc = vcd_alloc_buffer_pool_entries(buf_pool,
							   &Buf_req);
		} else {
			VCD_MSG_ERROR("rc = 0x%x. Failed: ddl_get_property",
					  rc);
		}
	}

	if (!VCD_FAILED(rc)) {
		if (buf_pool->buf_req.sz > buf_size) {
			VCD_MSG_ERROR("\n required buffer sz %u "
				"allocated sz %u", buf_pool->buf_req.
				sz, buf_size);

			rc = VCD_ERR_ILLEGAL_PARM;
		}
	}

	return rc;
}

u32 vcd_set_buffer_internal(
	struct vcd_clnt_ctxt *cctxt,
	 struct vcd_buffer_pool *buf_pool, u8 *buffer, u32 buf_size)
{
	struct vcd_buffer_entry *buf_entry;
	u8 *physical;
	u32 ion_flag = 0;
	struct ion_handle *buff_handle = NULL;

	buf_entry = vcd_find_buffer_pool_entry(buf_pool, buffer);
	if (buf_entry) {
		VCD_MSG_ERROR("This buffer address already exists");

		return VCD_ERR_ILLEGAL_OP;
	}

	physical = (u8 *) vcd_pmem_get_physical(
		cctxt->client_data, (unsigned long)buffer);

	ion_flag = vcd_get_ion_flag(cctxt->client_data,
				(unsigned long)buffer,
				&buff_handle);
	if (!physical) {
		VCD_MSG_ERROR("Couldn't get physical address");
		return VCD_ERR_BAD_POINTER;
	}
	if (((u32) physical % buf_pool->buf_req.align)) {
		VCD_MSG_ERROR("Physical addr is not aligned");
		return VCD_ERR_BAD_POINTER;
	}

	buf_entry = vcd_get_free_buffer_pool_entry(buf_pool);
	if (!buf_entry) {
		VCD_MSG_ERROR("Can't allocate buffer pool is full");
		return VCD_ERR_FAIL;
	}
	buf_entry->virtual = buffer;
	buf_entry->physical = physical;
	buf_entry->sz = buf_size;
	buf_entry->frame.alloc_len = buf_size;
	buf_entry->allocated = false;

	buf_entry->frame.virtual = buf_entry->virtual;
	buf_entry->frame.physical = buf_entry->physical;
	buf_entry->frame.ion_flag = ion_flag;
	buf_entry->frame.buff_ion_handle = buff_handle;

	buf_pool->validated++;

	return VCD_S_SUCCESS;

}

u32 vcd_allocate_buffer_internal(
	struct vcd_clnt_ctxt *cctxt,
	 struct vcd_buffer_pool *buf_pool,
	 u32 buf_size, u8 **vir_buf_addr, u8 **phy_buf_addr)
{
	struct vcd_buffer_entry *buf_entry;
	struct vcd_buffer_requirement *buf_req;
	u32 addr;
	int rc = 0;

	buf_entry = vcd_get_free_buffer_pool_entry(buf_pool);
	if (!buf_entry) {
		VCD_MSG_ERROR("Can't allocate buffer pool is full");

		return VCD_ERR_FAIL;
	}

	buf_req = &buf_pool->buf_req;

	buf_size += buf_req->align;

	rc = vcd_pmem_alloc(buf_size, &buf_entry->alloc,
				&buf_entry->physical, cctxt);

	if (rc < 0) {
		VCD_MSG_ERROR("Buffer allocation failed");

		return VCD_ERR_ALLOC_FAIL;
	}

	buf_entry->sz = buf_size;
	buf_entry->frame.alloc_len = buf_size;

	if (!buf_entry->physical) {
		VCD_MSG_ERROR("Couldn't get physical address");

		return VCD_ERR_BAD_POINTER;
	}

	buf_entry->allocated = true;

	if (buf_req->align > 0) {

		addr = (u32) buf_entry->physical;
		addr += buf_req->align;
		addr -= (addr % buf_req->align);
		buf_entry->virtual = buf_entry->alloc;
		buf_entry->virtual += (u32) (addr - (u32)
			buf_entry->physical);
		buf_entry->physical = (u8 *) addr;
	} else {
		VCD_MSG_LOW("No buffer alignment required");

		buf_entry->virtual = buf_entry->alloc;

	}

	buf_entry->frame.virtual = buf_entry->virtual;
	buf_entry->frame.physical = buf_entry->physical;

	*vir_buf_addr = buf_entry->virtual;
	*phy_buf_addr = buf_entry->physical;

	buf_pool->allocated++;
	buf_pool->validated++;

	return VCD_S_SUCCESS;
}

u32 vcd_free_one_buffer_internal(
	struct vcd_clnt_ctxt *cctxt,
	 enum vcd_buffer_type buffer_type, u8 *buffer)
{
	struct vcd_buffer_pool *buf_pool;
	u32 rc = VCD_S_SUCCESS;
	struct vcd_buffer_entry *buf_entry;
	u32 first_frm_recvd = 0;

	if (buffer_type == VCD_BUFFER_INPUT) {
		buf_pool = &cctxt->in_buf_pool;
		first_frm_recvd = VCD_FIRST_IP_RCVD;
	} else if (buffer_type == VCD_BUFFER_OUTPUT) {
		buf_pool = &cctxt->out_buf_pool;
		first_frm_recvd = VCD_FIRST_OP_RCVD;
	} else
		rc = VCD_ERR_ILLEGAL_PARM;

	VCD_FAILED_RETURN(rc, "Invalid buffer type provided");

	first_frm_recvd &= cctxt->status.mask;
	if (first_frm_recvd && !cctxt->meta_mode) {
		VCD_MSG_ERROR(
			"VCD free buffer called when data path is active");
		return VCD_ERR_BAD_STATE;
	}

	buf_entry = vcd_find_buffer_pool_entry(buf_pool, buffer);
	if (!buf_entry) {
		VCD_MSG_ERROR("Buffer addr %p not found. Can't free buffer",
				  buffer);

		return VCD_ERR_ILLEGAL_PARM;
	}
	if (buf_entry->in_use) {
		VCD_MSG_ERROR("\n Buffer is in use and is not flushed");
		return VCD_ERR_ILLEGAL_OP;
	}

	VCD_MSG_LOW("Freeing buffer %p. Allocated %d",
			buf_entry->virtual, buf_entry->allocated);

	if (buf_entry->allocated) {
		vcd_pmem_free(buf_entry->alloc, buf_entry->physical, cctxt);
		buf_pool->allocated--;
	}

	memset(buf_entry, 0, sizeof(struct vcd_buffer_entry));
	buf_pool->validated--;
	if (buf_pool->validated == 0)
		vcd_free_buffer_pool_entries(buf_pool);

	return VCD_S_SUCCESS;
}

u32 vcd_free_buffers_internal(
	struct vcd_clnt_ctxt *cctxt,
	 struct vcd_buffer_pool *buf_pool)
{
	u32 rc = VCD_S_SUCCESS;
	u32 i;

	VCD_MSG_LOW("vcd_free_buffers_internal:");

	if (buf_pool->entries) {
		for (i = 1; i <= buf_pool->count; i++) {
			if (buf_pool->entries[i].valid &&
				buf_pool->entries[i].allocated) {
				vcd_pmem_free(buf_pool->entries[i].alloc,
						  buf_pool->entries[i].
						  physical, cctxt);
			}
		}

	}

	vcd_reset_buffer_pool_for_reuse(buf_pool);

	return rc;
}

u32 vcd_alloc_buffer_pool_entries(
	struct vcd_buffer_pool *buf_pool,
	 struct vcd_buffer_requirement *buf_req)
{

	VCD_MSG_LOW("vcd_alloc_buffer_pool_entries:");

	buf_pool->buf_req = *buf_req;

	buf_pool->count = buf_req->actual_count;
	buf_pool->entries = (struct vcd_buffer_entry *)
		kzalloc((sizeof(struct vcd_buffer_entry) *
			   (VCD_MAX_BUFFER_ENTRIES + 1)), GFP_KERNEL);

	if (!buf_pool->entries) {
		VCD_MSG_ERROR("Buf_pool entries alloc failed");
		return VCD_ERR_ALLOC_FAIL;
	}

	INIT_LIST_HEAD(&buf_pool->queue);
	buf_pool->entries[0].valid = true;
	buf_pool->q_len = 0;

	buf_pool->validated = 0;
	buf_pool->allocated = 0;
	buf_pool->in_use = 0;

	return VCD_S_SUCCESS;
}

void vcd_free_buffer_pool_entries(struct vcd_buffer_pool *buf_pool)
{
	VCD_MSG_LOW("vcd_free_buffer_pool_entries:");
	kfree(buf_pool->entries);
	memset(buf_pool, 0, sizeof(struct vcd_buffer_pool));
	INIT_LIST_HEAD(&buf_pool->queue);
}

void vcd_flush_in_use_buffer_pool_entries(struct vcd_clnt_ctxt *cctxt,
	struct vcd_buffer_pool *buf_pool, u32 event)
{
	u32 i;
	VCD_MSG_LOW("vcd_flush_buffer_pool_entries: event=0x%x", event);

	if (buf_pool->entries) {
		for (i = 0; i <= buf_pool->count; i++) {
			if (buf_pool->entries[i].virtual &&
				buf_pool->entries[i].in_use) {
				cctxt->callback(event, VCD_S_SUCCESS,
					&buf_pool->entries[i].frame,
					sizeof(struct vcd_frame_data),
					cctxt, cctxt->client_data);
				buf_pool->entries[i].in_use = false;
				VCD_BUFFERPOOL_INUSE_DECREMENT(
					buf_pool->in_use);
		 }
		}
	}
}


void vcd_reset_buffer_pool_for_reuse(struct vcd_buffer_pool *buf_pool)
{
	VCD_MSG_LOW("vcd_reset_buffer_pool_for_reuse:");

	if (buf_pool->entries) {
		memset(&buf_pool->entries[1], 0,
			sizeof(struct vcd_buffer_entry) *
			VCD_MAX_BUFFER_ENTRIES);
	}
	buf_pool->q_len = 0;

	buf_pool->validated = 0;
	buf_pool->allocated = 0;
	buf_pool->in_use = 0;
	INIT_LIST_HEAD(&buf_pool->queue);
}

struct vcd_buffer_entry *vcd_get_free_buffer_pool_entry
	(struct vcd_buffer_pool *pool) {
	u32 i;

	i = 1;
	while (i <= pool->count && pool->entries[i].valid)
		i++;


	if (i <= pool->count) {
		pool->entries[i].valid = true;

		return &pool->entries[i];
	} else {
		return NULL;
	}
}

struct vcd_buffer_entry *vcd_find_buffer_pool_entry
	(struct vcd_buffer_pool *pool, u8 *addr)
{
	u32 i;
	u32 found = false;

	for (i = 0; i <= pool->count && !found; i++) {
		if (pool->entries[i].virtual == addr)
			found = true;

	}

	if (found)
		return &pool->entries[i - 1];
	else
		return NULL;

}

u32 vcd_buffer_pool_entry_en_q(
	struct vcd_buffer_pool *pool,
	 struct vcd_buffer_entry *entry)
{
	struct vcd_buffer_entry *list_itr;

	if (pool->q_len == pool->count)
		return false;

	list_for_each_entry(list_itr, &pool->queue, list)
	if (list_itr == entry) {
		VCD_MSG_HIGH("\n this output buffer is already present"
			" in queue");
		VCD_MSG_HIGH("\n Vir Addr %p Phys Addr %p",
			entry->virtual, entry->physical);
		return false;
	}

	list_add_tail(&entry->list, &pool->queue);
	pool->q_len++;

	return true;
}

struct vcd_buffer_entry *vcd_buffer_pool_entry_de_q
	(struct vcd_buffer_pool *pool) {
	struct vcd_buffer_entry *entry;

	if (!pool || !pool->q_len)
		return NULL;

	entry = list_first_entry(&pool->queue,
		struct vcd_buffer_entry, list);

	if (entry) {
		list_del(&entry->list);
		pool->q_len--;
	}

	return entry;
}

void vcd_flush_bframe_buffers(struct vcd_clnt_ctxt *cctxt, u32 mode)
{
	int i;
	struct vcd_buffer_pool *buf_pool;

	if (!cctxt->decoding && cctxt->bframe) {
		buf_pool = (mode == VCD_FLUSH_INPUT) ?
			&cctxt->in_buf_pool : &cctxt->out_buf_pool;
		if (buf_pool->entries != NULL) {
			for (i = 1; i <= buf_pool->count; i++) {
				if ((buf_pool->entries[i].in_use) &&
					(buf_pool->entries[i].frame.virtual
					 != NULL)) {
					if (mode == VCD_FLUSH_INPUT) {
						cctxt->callback(
						VCD_EVT_RESP_INPUT_FLUSHED,
						VCD_S_SUCCESS,
						&(buf_pool->entries[i].frame),
						sizeof(struct vcd_frame_data),
						cctxt, cctxt->client_data);
					} else {
						buf_pool->entries[i].
							frame.data_len = 0;
						cctxt->callback(
						VCD_EVT_RESP_OUTPUT_FLUSHED,
						VCD_S_SUCCESS,
						&(buf_pool->entries[i].frame),
						sizeof(struct vcd_frame_data),
						cctxt,
						cctxt->client_data);
					}
				VCD_BUFFERPOOL_INUSE_DECREMENT(
					buf_pool->in_use);
				buf_pool->entries[i].in_use = false;
				}
			}
		}
	}
}

void vcd_flush_output_buffers(struct vcd_clnt_ctxt *cctxt)
{
	struct vcd_buffer_pool *buf_pool;
	struct vcd_buffer_entry *buf_entry;
	u32 count = 0;
	struct vcd_property_hdr prop_hdr;

	VCD_MSG_LOW("vcd_flush_output_buffers:");
	buf_pool = &cctxt->out_buf_pool;
	buf_entry = vcd_buffer_pool_entry_de_q(buf_pool);
	while (buf_entry) {
		if (!cctxt->decoding || buf_entry->in_use) {
			buf_entry->frame.data_len = 0;
			cctxt->callback(VCD_EVT_RESP_OUTPUT_FLUSHED,
					VCD_S_SUCCESS,
					&buf_entry->frame,
					sizeof(struct vcd_frame_data),
					cctxt, cctxt->client_data);
			if (buf_entry->in_use) {
				VCD_BUFFERPOOL_INUSE_DECREMENT(
					buf_pool->in_use);
				buf_entry->in_use = false;
			}
			count++;
		}
		buf_entry = vcd_buffer_pool_entry_de_q(buf_pool);
	}
	vcd_flush_bframe_buffers(cctxt, VCD_FLUSH_OUTPUT);
	if (buf_pool->in_use || buf_pool->q_len) {
		VCD_MSG_ERROR("%s(): WARNING in_use(%u) or q_len(%u) not zero!",
			__func__, buf_pool->in_use, buf_pool->q_len);
		buf_pool->in_use = buf_pool->q_len = 0;
		}
	if (cctxt->sched_clnt_hdl) {
		if (count > cctxt->sched_clnt_hdl->tkns)
			cctxt->sched_clnt_hdl->tkns = 0;
		else
			cctxt->sched_clnt_hdl->tkns -= count;
	}

	if (cctxt->ddl_hdl_valid && cctxt->decoding) {
		prop_hdr.prop_id = DDL_I_REQ_OUTPUT_FLUSH;
		prop_hdr.sz = sizeof(u32);
		count = 0x1;

		(void)ddl_set_property(cctxt->ddl_handle, &prop_hdr,
					&count);
	}
	vcd_release_all_clnt_frm_transc(cctxt);
	cctxt->status.mask &= ~VCD_IN_RECONFIG;
}

u32 vcd_flush_buffers(struct vcd_clnt_ctxt *cctxt, u32 mode)
{
	u32 rc = VCD_S_SUCCESS;
	struct vcd_buffer_entry *buf_entry;

	VCD_MSG_LOW("vcd_flush_buffers:");

	if (mode > VCD_FLUSH_ALL || !(mode & VCD_FLUSH_ALL)) {
		VCD_MSG_ERROR("Invalid flush mode %d", mode);
		return VCD_ERR_ILLEGAL_PARM;
	}

	VCD_MSG_MED("Flush mode %d requested", mode);
	if ((mode & VCD_FLUSH_INPUT) &&
		cctxt->sched_clnt_hdl) {

		rc = vcd_sched_dequeue_buffer(
			cctxt->sched_clnt_hdl, &buf_entry);
		while (!VCD_FAILED(rc) && buf_entry) {
			if (buf_entry->virtual) {
				cctxt->callback(VCD_EVT_RESP_INPUT_FLUSHED,
						VCD_S_SUCCESS,
						&buf_entry->frame,
						sizeof(struct
							 vcd_frame_data),
						cctxt,
						cctxt->client_data);
				}

			buf_entry->in_use = false;
			VCD_BUFFERPOOL_INUSE_DECREMENT(
				cctxt->in_buf_pool.in_use);
			buf_entry = NULL;
			rc = vcd_sched_dequeue_buffer(
				cctxt->sched_clnt_hdl, &buf_entry);
		}
	}
	if (rc != VCD_ERR_QEMPTY)
		VCD_FAILED_RETURN(rc, "Failed: vcd_sched_dequeue_buffer");
	if (cctxt->status.frame_submitted > 0)
		cctxt->status.mask |= mode;
	else {
		if (mode & VCD_FLUSH_INPUT)
			vcd_flush_bframe_buffers(cctxt, VCD_FLUSH_INPUT);
		if (mode & VCD_FLUSH_OUTPUT)
			vcd_flush_output_buffers(cctxt);
	}
	return VCD_S_SUCCESS;
}

void vcd_flush_buffers_in_err_fatal(struct vcd_clnt_ctxt *cctxt)
{
	VCD_MSG_LOW("\n vcd_flush_buffers_in_err_fatal:");
	(void) vcd_flush_buffers(cctxt, VCD_FLUSH_ALL);
	vcd_flush_in_use_buffer_pool_entries(cctxt,
		&cctxt->in_buf_pool, VCD_EVT_RESP_INPUT_FLUSHED);
	vcd_flush_in_use_buffer_pool_entries(cctxt,
		&cctxt->out_buf_pool, VCD_EVT_RESP_OUTPUT_FLUSHED);
	vcd_send_flush_done(cctxt, VCD_S_SUCCESS);
}

u32 vcd_init_client_context(struct vcd_clnt_ctxt *cctxt)
{
	u32 rc;
	VCD_MSG_LOW("vcd_init_client_context:");
	rc = ddl_open(&cctxt->ddl_handle, cctxt->decoding);
	VCD_FAILED_RETURN(rc, "Failed: ddl_open");
	cctxt->vcd_enable_ion = res_trk_get_enable_ion();
	if (cctxt->vcd_enable_ion) {
		cctxt->vcd_ion_client = res_trk_get_ion_client();
		if (!cctxt->vcd_ion_client) {
			VCD_MSG_LOW("vcd_init_ion_get_client_failed:");
			return -EINVAL;
		}
	}
	cctxt->ddl_hdl_valid = true;
	cctxt->clnt_state.state = VCD_CLIENT_STATE_OPEN;
	cctxt->clnt_state.state_table =
		vcd_get_client_state_table(VCD_CLIENT_STATE_OPEN);
	cctxt->signature = VCD_SIGNATURE;
	cctxt->live = true;
	cctxt->bframe = 0;
	cctxt->cmd_q.pending_cmd = VCD_CMD_NONE;
	cctxt->status.last_evt = VCD_EVT_RESP_BASE;
	return rc;
}

void vcd_destroy_client_context(struct vcd_clnt_ctxt *cctxt)
{
	struct vcd_dev_ctxt *dev_ctxt;
	struct vcd_clnt_ctxt *client;
	struct vcd_buffer_entry *buf_entry;
	u32 rc = VCD_S_SUCCESS;
	VCD_MSG_LOW("vcd_destroy_client_context:");

	dev_ctxt = cctxt->dev_ctxt;

	if (cctxt == dev_ctxt->cctxt_list_head) {
		VCD_MSG_MED("Clnt list head clnt being removed");

		dev_ctxt->cctxt_list_head = cctxt->next;
	} else {
		client = dev_ctxt->cctxt_list_head;
		while (client && cctxt != client->next)
			client = client->next;
		if (client)
			client->next = cctxt->next;
		if (!client) {
			rc = VCD_ERR_FAIL;
			VCD_MSG_ERROR("Client not found in client list");
		}
	}

	if (VCD_FAILED(rc))
		return;

	if (cctxt->sched_clnt_hdl) {
		rc = VCD_S_SUCCESS;
		while (!VCD_FAILED(rc)) {
			rc = vcd_sched_dequeue_buffer(
				cctxt->sched_clnt_hdl, &buf_entry);
			if (rc != VCD_ERR_QEMPTY && VCD_FAILED(rc))
				VCD_MSG_ERROR("\n Failed: "
					"vcd_sched_de_queue_buffer");
		}
		rc = vcd_sched_remove_client(cctxt->sched_clnt_hdl);
		if (VCD_FAILED(rc))
			VCD_MSG_ERROR("\n Failed: sched_remove_client");
		cctxt->sched_clnt_hdl = NULL;
	}

	if (cctxt->seq_hdr.sequence_header) {
		vcd_pmem_free(cctxt->seq_hdr.sequence_header,
				  cctxt->seq_hdr_phy_addr, cctxt);
		cctxt->seq_hdr.sequence_header = NULL;
	}

	vcd_free_buffers_internal(cctxt, &cctxt->in_buf_pool);
	vcd_free_buffers_internal(cctxt, &cctxt->out_buf_pool);
	vcd_free_buffer_pool_entries(&cctxt->in_buf_pool);
	vcd_free_buffer_pool_entries(&cctxt->out_buf_pool);
	vcd_release_all_clnt_transc(cctxt);

	if (cctxt->ddl_hdl_valid) {
		(void)ddl_close(&cctxt->ddl_handle);
		cctxt->ddl_hdl_valid = false;
	}

	cctxt->signature = 0;
	cctxt->clnt_state.state = VCD_CLIENT_STATE_NULL;
	cctxt->clnt_state.state_table = NULL;
	cctxt->vcd_ion_client = NULL;
	kfree(cctxt);
}

u32 vcd_check_for_client_context(
	struct vcd_dev_ctxt *dev_ctxt, s32 driver_id)
{
	struct vcd_clnt_ctxt *client;

	client = dev_ctxt->cctxt_list_head;
	while (client && client->driver_id != driver_id)
		client = client->next;

	if (!client)
		return false;
	else
		return true;
}

u32 vcd_validate_driver_handle(
	struct vcd_dev_ctxt *dev_ctxt, s32 driver_handle)
{
	driver_handle--;

	if (driver_handle < 0 ||
		driver_handle >= VCD_DRIVER_INSTANCE_MAX ||
		!dev_ctxt->driver_ids[driver_handle]) {
		return false;
	} else {
		return true;
	}
}

u32 vcd_client_cmd_en_q(
	struct vcd_clnt_ctxt *cctxt, enum vcd_command command)
{
	u32 result;

	if (cctxt->cmd_q.pending_cmd == VCD_CMD_NONE) {
		cctxt->cmd_q.pending_cmd = command;
		result = true;
	} else {
		result = false;
	}

	return result;
}

void vcd_client_cmd_flush_and_en_q(
	struct vcd_clnt_ctxt *cctxt, enum vcd_command command)
{
	cctxt->cmd_q.pending_cmd = command;
}

u32 vcd_client_cmd_de_q(struct vcd_clnt_ctxt *cctxt,
	enum vcd_command *command)
{
	if (cctxt->cmd_q.pending_cmd == VCD_CMD_NONE)
		return false;

	*command = cctxt->cmd_q.pending_cmd;
	cctxt->cmd_q.pending_cmd = VCD_CMD_NONE;

	return true;
}

u32 vcd_get_next_queued_client_cmd(struct vcd_dev_ctxt *dev_ctxt,
	struct vcd_clnt_ctxt **cctxt, enum vcd_command *command)
{
	struct vcd_clnt_ctxt *client = dev_ctxt->cctxt_list_head;
	u32 result = false;

	while (client && !result) {
		*cctxt = client;
		result = vcd_client_cmd_de_q(client, command);
		client = client->next;
	}
	return result;
}

u32 vcd_submit_cmd_sess_start(struct vcd_transc *transc)
{
	u32 rc;
	struct vcd_sequence_hdr Seq_hdr;

	VCD_MSG_LOW("vcd_submit_cmd_sess_start:");

	if (transc->cctxt->decoding) {

		if (transc->cctxt->seq_hdr.sequence_header) {
			Seq_hdr.sequence_header_len =
				transc->cctxt->seq_hdr.
				sequence_header_len;
			Seq_hdr.sequence_header =
				transc->cctxt->seq_hdr_phy_addr;

			rc = ddl_decode_start(transc->cctxt->ddl_handle,
						  &Seq_hdr, (void *)transc);
		} else {
			rc = ddl_decode_start(transc->cctxt->ddl_handle,
						  NULL, (void *)transc);
		}

	} else {
		rc = ddl_encode_start(transc->cctxt->ddl_handle,
					  (void *)transc);
	}
	if (!VCD_FAILED(rc)) {
		transc->cctxt->status.cmd_submitted++;
		vcd_device_timer_start(transc->cctxt->dev_ctxt);
	} else
		VCD_MSG_ERROR("rc = 0x%x. Failed: ddl start", rc);

	return rc;
}

u32 vcd_submit_cmd_sess_end(struct vcd_transc *transc)
{
	u32 rc;

	VCD_MSG_LOW("vcd_submit_cmd_sess_end:");

	if (transc->cctxt->decoding) {
		rc = ddl_decode_end(transc->cctxt->ddl_handle,
					(void *)transc);
	} else {
		rc = ddl_encode_end(transc->cctxt->ddl_handle,
					(void *)transc);
	}
	if (!VCD_FAILED(rc)) {
		transc->cctxt->status.cmd_submitted++;
		vcd_device_timer_start(transc->cctxt->dev_ctxt);
	} else
		VCD_MSG_ERROR("rc = 0x%x. Failed: ddl end", rc);

	return rc;
}

void vcd_submit_cmd_client_close(struct vcd_clnt_ctxt *cctxt)
{
	(void) ddl_close(&cctxt->ddl_handle);
	cctxt->ddl_hdl_valid = false;
	cctxt->status.mask &= ~VCD_CLEANING_UP;
	if (cctxt->status.mask & VCD_CLOSE_PENDING) {
		vcd_destroy_client_context(cctxt);
		vcd_handle_for_last_clnt_close(cctxt->dev_ctxt, true);
	}
}

u32 vcd_submit_command_in_continue(struct vcd_dev_ctxt
	*dev_ctxt, struct vcd_transc *transc)
{
	struct vcd_property_hdr   prop_hdr;
	struct vcd_clnt_ctxt *client = NULL;
	enum vcd_command cmd = VCD_CMD_NONE;
	u32 rc = VCD_ERR_FAIL;
	u32 result = false, flush = 0, event = 0;
	u32 command_break = false;

	VCD_MSG_LOW("\n vcd_submit_command_in_continue:");

	while (!command_break) {
		result = vcd_get_next_queued_client_cmd(dev_ctxt,
			&client, &cmd);

		if (!result)
			command_break = true;
		else {
			transc->type = cmd;
			transc->cctxt = client;

		 switch (cmd) {
		 case VCD_CMD_CODEC_START:
			{
				rc = vcd_submit_cmd_sess_start(transc);
				event = VCD_EVT_RESP_START;
				break;
			}
		 case VCD_CMD_CODEC_STOP:
			{
				rc = vcd_submit_cmd_sess_end(transc);
				event = VCD_EVT_RESP_STOP;
				break;
			}
		 case VCD_CMD_OUTPUT_FLUSH:
			{
				prop_hdr.prop_id = DDL_I_REQ_OUTPUT_FLUSH;
				prop_hdr.sz = sizeof(u32);
				flush = 0x1;
				(void) ddl_set_property(client->ddl_handle,
						 &prop_hdr, &flush);
				vcd_release_command_channel(dev_ctxt,
					transc);
				rc = VCD_S_SUCCESS;
				break;
			}
		 case VCD_CMD_CLIENT_CLOSE:
			{
				vcd_submit_cmd_client_close(client);
				vcd_release_command_channel(dev_ctxt,
					transc);
				rc = VCD_S_SUCCESS;
				break;
			}
		 default:
			{
				VCD_MSG_ERROR("\n vcd_submit_command: Unknown"
					"command %d", (int)cmd);
				break;
			}
		 }

		 if (!VCD_FAILED(rc)) {
			command_break = true;
		 } else	{
			VCD_MSG_ERROR("vcd_submit_command %d: failed 0x%x",
				cmd, rc);
			client->callback(event, rc, NULL, 0, client,
				client->client_data);
		 }
	  }
	}
	return result;
}

u32 vcd_schedule_frame(struct vcd_dev_ctxt *dev_ctxt,
	struct vcd_clnt_ctxt **cctxt, struct vcd_buffer_entry
	**ip_buf_entry)
{
	u32 rc = VCD_S_SUCCESS;
	VCD_MSG_LOW("vcd_schedule_frame:");

	if (!dev_ctxt->cctxt_list_head) {
		VCD_MSG_HIGH("Client list empty");
		return false;
	}
	rc = vcd_sched_get_client_frame(&dev_ctxt->sched_clnt_list,
		cctxt, ip_buf_entry);
	if (rc == VCD_ERR_QEMPTY) {
		VCD_MSG_HIGH("No frame available. Sched queues are empty");
		return false;
	}
	if (VCD_FAILED(rc)) {
		VCD_MSG_FATAL("vcd_submit_frame: sched_de_queue_frame"
			"failed 0x%x", rc);
	  return false;
	}
	if (!*cctxt || !*ip_buf_entry) {
		VCD_MSG_FATAL("Sched returned invalid values. ctxt=%p,"
			"ipbuf=%p",	*cctxt, *ip_buf_entry);
		return false;
	}
	return true;
}

void vcd_try_submit_frame(struct vcd_dev_ctxt *dev_ctxt)
{
	struct vcd_transc *transc;
	u32 rc = VCD_S_SUCCESS;
	struct vcd_clnt_ctxt *cctxt = NULL;
	struct vcd_buffer_entry *ip_buf_entry = NULL;
	u32 result = false;

	VCD_MSG_LOW("vcd_try_submit_frame:");

	if (!vcd_get_frame_channel(dev_ctxt, &transc))
		return;

	if (!vcd_schedule_frame(dev_ctxt, &cctxt, &ip_buf_entry)) {
		vcd_release_frame_channel(dev_ctxt, transc);
		return;
	}

	rc = vcd_power_event(dev_ctxt, cctxt, VCD_EVT_PWR_CLNT_CMD_BEGIN);

	if (!VCD_FAILED(rc)) {
		transc->cctxt = cctxt;
		transc->ip_buf_entry = ip_buf_entry;

		result = vcd_submit_frame(dev_ctxt, transc);
	} else {
		VCD_MSG_ERROR("Failed: VCD_EVT_PWR_CLNT_CMD_BEGIN");
		(void) vcd_sched_queue_buffer(
			cctxt->sched_clnt_hdl, ip_buf_entry, false);
		cctxt->sched_clnt_hdl->tkns++;
	}

	if (!result) {
		vcd_release_frame_channel(dev_ctxt, transc);
		(void) vcd_power_event(dev_ctxt, cctxt,
				VCD_EVT_PWR_CLNT_CMD_FAIL);
	}
}

u32 vcd_submit_frame(struct vcd_dev_ctxt *dev_ctxt,
					 struct vcd_transc *transc)
{
	struct vcd_clnt_ctxt *cctxt = NULL;
	struct vcd_frame_data *ip_frm_entry;
	struct vcd_buffer_entry *op_buf_entry = NULL;
	u32 rc = VCD_S_SUCCESS;
	u32 evcode = 0;
	struct ddl_frame_data_tag ddl_ip_frm;
	struct ddl_frame_data_tag ddl_op_frm;

	VCD_MSG_LOW("vcd_submit_frame:");
	cctxt = transc->cctxt;
	ip_frm_entry = &transc->ip_buf_entry->frame;

	transc->op_buf_entry = op_buf_entry;
	transc->ip_frm_tag = ip_frm_entry->ip_frm_tag;
	transc->time_stamp = ip_frm_entry->time_stamp;
	transc->flags = ip_frm_entry->flags;
	ip_frm_entry->ip_frm_tag = (u32) transc;
	memset(&ddl_ip_frm, 0, sizeof(ddl_ip_frm));
	memset(&ddl_op_frm, 0, sizeof(ddl_op_frm));
	if (cctxt->decoding) {
		evcode = CLIENT_STATE_EVENT_NUMBER(decode_frame);
		ddl_ip_frm.vcd_frm = *ip_frm_entry;
		rc = ddl_decode_frame(cctxt->ddl_handle, &ddl_ip_frm,
							   (void *) transc);
	} else {
		op_buf_entry = vcd_buffer_pool_entry_de_q(
			&cctxt->out_buf_pool);
		if (!op_buf_entry) {
			VCD_MSG_ERROR("Sched provided frame when no"
				"op buffer was present");
			rc = VCD_ERR_FAIL;
		} else {
			op_buf_entry->in_use = true;
			cctxt->out_buf_pool.in_use++;
			ddl_ip_frm.vcd_frm = *ip_frm_entry;
			ddl_ip_frm.frm_delta =
				vcd_calculate_frame_delta(cctxt,
					ip_frm_entry);

			ddl_op_frm.vcd_frm = op_buf_entry->frame;

			evcode = CLIENT_STATE_EVENT_NUMBER(encode_frame);

			rc = ddl_encode_frame(cctxt->ddl_handle,
				&ddl_ip_frm, &ddl_op_frm, (void *) transc);
		}
	}
	ip_frm_entry->ip_frm_tag = transc->ip_frm_tag;
	if (!VCD_FAILED(rc)) {
		vcd_device_timer_start(dev_ctxt);
		cctxt->status.frame_submitted++;
		if (ip_frm_entry->flags & VCD_FRAME_FLAG_EOS)
			vcd_do_client_state_transition(cctxt,
				VCD_CLIENT_STATE_EOS, evcode);
	} else {
		VCD_MSG_ERROR("Frame submission failed. rc = 0x%x", rc);
		vcd_handle_submit_frame_failed(dev_ctxt, transc);
	}
	return true;
}

u32 vcd_try_submit_frame_in_continue(struct vcd_dev_ctxt *dev_ctxt,
	struct vcd_transc *transc)
{
	struct vcd_clnt_ctxt *cctxt = NULL;
	struct vcd_buffer_entry *ip_buf_entry = NULL;

	VCD_MSG_LOW("vcd_try_submit_frame_in_continue:");

	if (!vcd_schedule_frame(dev_ctxt, &cctxt, &ip_buf_entry))
		return false;

	transc->cctxt = cctxt;
	transc->ip_buf_entry = ip_buf_entry;

	return vcd_submit_frame(dev_ctxt, transc);
}

u32 vcd_process_cmd_sess_start(struct vcd_clnt_ctxt *cctxt)
{
	struct vcd_transc *transc;
	u32 rc = VCD_S_SUCCESS;

	VCD_MSG_LOW("vcd_process_cmd_sess_start:");
	if (vcd_get_command_channel(cctxt->dev_ctxt, &transc)) {
		rc = vcd_power_event(cctxt->dev_ctxt,
					 cctxt, VCD_EVT_PWR_CLNT_CMD_BEGIN);

		if (!VCD_FAILED(rc)) {
			transc->type = VCD_CMD_CODEC_START;
			transc->cctxt = cctxt;
			rc = vcd_submit_cmd_sess_start(transc);
		} else {
			VCD_MSG_ERROR("Failed: VCD_EVT_PWR_CLNT_CMD_BEGIN");
		}

		if (VCD_FAILED(rc)) {
			vcd_release_command_channel(cctxt->dev_ctxt,
							transc);
		}
	} else {
		u32 result;

		result = vcd_client_cmd_en_q(cctxt, VCD_CMD_CODEC_START);
		if (!result) {
			rc = VCD_ERR_BUSY;
			VCD_MSG_ERROR("%s(): vcd_client_cmd_en_q() "
				"failed\n", __func__);
		}
	}

	if (VCD_FAILED(rc)) {
		(void)vcd_power_event(cctxt->dev_ctxt,
					  cctxt, VCD_EVT_PWR_CLNT_CMD_FAIL);
	}

	return rc;
}

void vcd_send_frame_done_in_eos(struct vcd_clnt_ctxt *cctxt,
	 struct vcd_frame_data *input_frame, u32 valid_opbuf)
{
	VCD_MSG_LOW("vcd_send_frame_done_in_eos:");

	if (!input_frame->virtual && !valid_opbuf) {
		VCD_MSG_MED("Sending NULL output with EOS");

		cctxt->out_buf_pool.entries[0].frame.flags =
			VCD_FRAME_FLAG_EOS;
		cctxt->out_buf_pool.entries[0].frame.data_len = 0;
		cctxt->out_buf_pool.entries[0].frame.time_stamp =
			input_frame->time_stamp;
		cctxt->out_buf_pool.entries[0].frame.ip_frm_tag =
			input_frame->ip_frm_tag;

		cctxt->callback(VCD_EVT_RESP_OUTPUT_DONE,
				  VCD_S_SUCCESS,
				  &cctxt->out_buf_pool.entries[0].frame,
				  sizeof(struct vcd_frame_data),
				  cctxt, cctxt->client_data);

		memset(&cctxt->out_buf_pool.entries[0].frame,
			   0, sizeof(struct vcd_frame_data));
	} else if (!input_frame->data_len) {
		if (cctxt->decoding) {
			vcd_send_frame_done_in_eos_for_dec(cctxt,
							   input_frame);
		} else {
			vcd_send_frame_done_in_eos_for_enc(cctxt,
							   input_frame);
		}

	}
}

void vcd_send_frame_done_in_eos_for_dec(
	struct vcd_clnt_ctxt *cctxt,
	 struct vcd_frame_data *input_frame)
{
	struct vcd_buffer_entry *buf_entry;
	struct vcd_property_hdr prop_hdr;
	u32 rc;
	struct ddl_frame_data_tag ddl_frm;

	prop_hdr.prop_id = DDL_I_DPB_RETRIEVE;
	prop_hdr.sz = sizeof(struct ddl_frame_data_tag);
	memset(&ddl_frm, 0, sizeof(ddl_frm));
	rc = ddl_get_property(cctxt->ddl_handle, &prop_hdr, &ddl_frm);

	if (VCD_FAILED(rc) || !ddl_frm.vcd_frm.virtual) {
		cctxt->status.eos_trig_ip_frm = *input_frame;
		cctxt->status.mask |= VCD_EOS_WAIT_OP_BUF;
		return;
	}

	buf_entry = vcd_find_buffer_pool_entry(&cctxt->out_buf_pool,
		ddl_frm.vcd_frm.virtual);
	if (!buf_entry) {
		VCD_MSG_ERROR("Unrecognized buffer address provided = %p",
				  ddl_frm.vcd_frm.virtual);
		return;
	} else {
		if (cctxt->sched_clnt_hdl->tkns)
			cctxt->sched_clnt_hdl->tkns--;

		VCD_MSG_MED("Sending non-NULL output with EOS");

		buf_entry->frame.data_len = 0;
		buf_entry->frame.offset = 0;
		buf_entry->frame.flags |= VCD_FRAME_FLAG_EOS;
		buf_entry->frame.ip_frm_tag = input_frame->ip_frm_tag;
		buf_entry->frame.time_stamp = input_frame->time_stamp;

		cctxt->callback(VCD_EVT_RESP_OUTPUT_DONE,
				  VCD_S_SUCCESS,
				  &buf_entry->frame,
				  sizeof(struct vcd_frame_data),
				  cctxt, cctxt->client_data);

		buf_entry->in_use = false;
		VCD_BUFFERPOOL_INUSE_DECREMENT(cctxt->out_buf_pool.in_use);
	}
}

void vcd_send_frame_done_in_eos_for_enc(
	struct vcd_clnt_ctxt *cctxt,
	 struct vcd_frame_data *input_frame)
{
	struct vcd_buffer_entry *op_buf_entry;

	if (!cctxt->out_buf_pool.q_len) {
		cctxt->status.eos_trig_ip_frm = *input_frame;

		cctxt->status.mask |= VCD_EOS_WAIT_OP_BUF;

		return;
	}

	op_buf_entry = vcd_buffer_pool_entry_de_q(&cctxt->out_buf_pool);
	if (!op_buf_entry) {
		VCD_MSG_ERROR("%s(): vcd_buffer_pool_entry_de_q() "
			"failed\n", __func__);
	} else {
		if (cctxt->sched_clnt_hdl->tkns)
			cctxt->sched_clnt_hdl->tkns--;

		VCD_MSG_MED("Sending non-NULL output with EOS");

		op_buf_entry->frame.data_len = 0;
		op_buf_entry->frame.flags |= VCD_FRAME_FLAG_EOS;
		op_buf_entry->frame.ip_frm_tag =
			input_frame->ip_frm_tag;
		op_buf_entry->frame.time_stamp = input_frame->time_stamp;

		cctxt->callback(VCD_EVT_RESP_OUTPUT_DONE,
				  VCD_S_SUCCESS,
				  &op_buf_entry->frame,
				  sizeof(struct vcd_frame_data),
				  cctxt, cctxt->client_data);
	}
}

u32 vcd_handle_recvd_eos(
	struct vcd_clnt_ctxt *cctxt,
	 struct vcd_frame_data *input_frame, u32 *pb_eos_handled)
{
	u32 rc;

	VCD_MSG_LOW("vcd_handle_recvd_eos:");

	*pb_eos_handled = false;

	if (input_frame->virtual &&
			input_frame->data_len)
		return VCD_S_SUCCESS;

	input_frame->data_len = 0;
	rc = vcd_sched_mark_client_eof(cctxt->sched_clnt_hdl);
	if (VCD_FAILED(rc) && rc != VCD_ERR_QEMPTY)
		return rc;

	if (rc == VCD_S_SUCCESS)
		*pb_eos_handled = true;
	else if (cctxt->decoding && !input_frame->virtual)
		cctxt->sched_clnt_hdl->tkns++;
	else if (!cctxt->decoding) {
		vcd_send_frame_done_in_eos(cctxt, input_frame, false);
		if (cctxt->status.mask & VCD_EOS_WAIT_OP_BUF) {
			vcd_do_client_state_transition(cctxt,
				VCD_CLIENT_STATE_EOS,
				CLIENT_STATE_EVENT_NUMBER
				(encode_frame));
		}
		*pb_eos_handled = true;
	}

	if (*pb_eos_handled &&
		input_frame->virtual &&
		!input_frame->data_len) {
		cctxt->callback(VCD_EVT_RESP_INPUT_DONE,
				  VCD_S_SUCCESS,
				  input_frame,
				  sizeof(struct vcd_frame_data),
				  cctxt, cctxt->client_data);
	}
	return VCD_S_SUCCESS;
}

u32 vcd_handle_first_decode_frame(struct vcd_clnt_ctxt *cctxt)
{
	u32 rc =  VCD_ERR_BAD_STATE;

	VCD_MSG_LOW("vcd_handle_first_decode_frame:");
	if (!cctxt->in_buf_pool.entries ||
		!cctxt->out_buf_pool.entries ||
		cctxt->in_buf_pool.validated !=
		cctxt->in_buf_pool.count ||
		cctxt->out_buf_pool.validated !=
		cctxt->out_buf_pool.count)
		VCD_MSG_ERROR("Buffer pool is not completely setup yet");
	else if (!cctxt->sched_clnt_hdl) {
		rc = vcd_sched_add_client(cctxt);
		VCD_FAILED_RETURN(rc, "Failed: vcd_add_client_to_sched");
		cctxt->sched_clnt_hdl->tkns =
			cctxt->out_buf_pool.q_len;
	} else
		rc = vcd_sched_suspend_resume_clnt(cctxt, true);
	return rc;
}

u32 vcd_setup_with_ddl_capabilities(struct vcd_dev_ctxt *dev_ctxt)
{
	struct vcd_property_hdr Prop_hdr;
	struct ddl_property_capability capability;
	u32 rc = VCD_S_SUCCESS;

	VCD_MSG_LOW("vcd_setup_with_ddl_capabilities:");

	if (!dev_ctxt->ddl_cmd_ch_depth) {
		Prop_hdr.prop_id = DDL_I_CAPABILITY;
		Prop_hdr.sz = sizeof(capability);

		/*
		** Since this is underlying core's property we don't need a
		** ddl client handle.
		*/
		rc = ddl_get_property(NULL, &Prop_hdr, &capability);

		if (!VCD_FAILED(rc)) {
			/*
			** Allocate the transaction table.
			*/
			dev_ctxt->trans_tbl_size =
				(VCD_MAX_CLIENT_TRANSACTIONS *
				capability.max_num_client) +
				capability.general_command_depth;

			dev_ctxt->trans_tbl = (struct vcd_transc *)
				kzalloc((sizeof(struct vcd_transc) *
				dev_ctxt->trans_tbl_size), GFP_KERNEL);

			if (!dev_ctxt->trans_tbl) {
				VCD_MSG_ERROR("Transaction table alloc failed");
				rc = VCD_ERR_ALLOC_FAIL;
			} else	{
				dev_ctxt->ddl_cmd_concurrency =
					!capability.exclusive;
				dev_ctxt->ddl_frame_ch_depth =
					capability.frame_command_depth;
				dev_ctxt->ddl_cmd_ch_depth =
					capability.general_command_depth;

				vcd_reset_device_channels(dev_ctxt);

				dev_ctxt->hw_time_out =
					capability.ddl_time_out_in_ms;

			}
		}
	}
	return rc;
}

struct vcd_transc *vcd_get_free_trans_tbl_entry
	(struct vcd_dev_ctxt *dev_ctxt) {
	u32 i;

	if (!dev_ctxt->trans_tbl)
		return NULL;

	i = 0;
	while (i < dev_ctxt->trans_tbl_size &&
		   dev_ctxt->trans_tbl[i].in_use)
		i++;

	if (i == dev_ctxt->trans_tbl_size) {
		return NULL;
	} else {
		memset(&dev_ctxt->trans_tbl[i], 0,
			   sizeof(struct vcd_transc));

		dev_ctxt->trans_tbl[i].in_use = true;

		return &dev_ctxt->trans_tbl[i];
	}
}

void vcd_release_trans_tbl_entry(struct vcd_transc *trans_entry)
{
	if (trans_entry)
		trans_entry->in_use = false;
}

u32 vcd_handle_input_done(
	struct vcd_clnt_ctxt *cctxt,
	 void *payload, u32 event, u32 status)
{
	struct vcd_transc *transc;
	struct ddl_frame_data_tag *frame =
		(struct ddl_frame_data_tag *) payload;
	struct vcd_buffer_entry *orig_frame = NULL;
	u32 rc;

	if (!cctxt->status.frame_submitted &&
		!cctxt->status.frame_delayed) {
		VCD_MSG_ERROR("Input done was not expected");
		return VCD_ERR_BAD_STATE;
	}

	rc = vcd_validate_io_done_pyld(cctxt, payload, status);
	if (rc == VCD_ERR_CLIENT_FATAL)
		vcd_handle_clnt_fatal_input_done(cctxt, frame->frm_trans_end);
	VCD_FAILED_RETURN(rc, "Bad input done payload");

	transc = (struct vcd_transc *)frame->vcd_frm.ip_frm_tag;
	orig_frame = vcd_find_buffer_pool_entry(&cctxt->in_buf_pool,
					 transc->ip_buf_entry->virtual);

	if ((transc->ip_buf_entry->frame.virtual !=
		 frame->vcd_frm.virtual)
		|| !transc->ip_buf_entry->in_use) {
		VCD_MSG_ERROR("Bad frm transaction state");
		vcd_handle_clnt_fatal_input_done(cctxt, frame->frm_trans_end);
		return VCD_ERR_BAD_POINTER;
	}

	frame->vcd_frm.ip_frm_tag = transc->ip_frm_tag;
	transc->frame = frame->vcd_frm.frame;

	cctxt->callback(event,
			status,
			&frame->vcd_frm,
			sizeof(struct vcd_frame_data),
			cctxt, cctxt->client_data);

	orig_frame->in_use--;
	VCD_BUFFERPOOL_INUSE_DECREMENT(cctxt->in_buf_pool.in_use);

	if (cctxt->decoding && orig_frame->in_use) {
		VCD_MSG_ERROR("When decoding same input buffer not "
				"supposed to be queued multiple times");
		return VCD_ERR_FAIL;
	}

	if (orig_frame != transc->ip_buf_entry)
		kfree(transc->ip_buf_entry);
	transc->ip_buf_entry = NULL;
	transc->input_done = true;

	if (transc->input_done && transc->frame_done)
		vcd_release_trans_tbl_entry(transc);

	if (VCD_FAILED(status)) {
		VCD_MSG_ERROR("INPUT_DONE returned err = 0x%x", status);
		vcd_handle_input_done_failed(cctxt, transc);
	} else
		cctxt->status.mask |= VCD_FIRST_IP_DONE;

	if (cctxt->status.frame_submitted > 0)
		cctxt->status.frame_submitted--;
	else
		cctxt->status.frame_delayed--;

	if (!VCD_FAILED(status) &&
		cctxt->decoding) {
		if (frame->vcd_frm.flags & VCD_FRAME_FLAG_CODECCONFIG) {
			VCD_MSG_HIGH(
				"INPUT_DONE with VCD_FRAME_FLAG_CODECCONFIG");
			vcd_handle_input_done_with_codec_config(cctxt,
				transc, frame);
			frame->vcd_frm.flags &= ~VCD_FRAME_FLAG_CODECCONFIG;
		}
		if (frame->vcd_frm.interlaced)
			vcd_handle_input_done_for_interlacing(cctxt);
		if (frame->frm_trans_end)
			vcd_handle_input_done_with_trans_end(cctxt);
	}

	return VCD_S_SUCCESS;
}

u32 vcd_handle_input_done_in_eos(
	struct vcd_clnt_ctxt *cctxt, void *payload, u32 status)
{
	struct vcd_transc *transc;
	struct ddl_frame_data_tag *frame =
		(struct ddl_frame_data_tag *) payload;
	u32 rc = VCD_ERR_FAIL, codec_config = false;
	u32 core_type = res_trk_get_core_type();
	rc = vcd_validate_io_done_pyld(cctxt, payload, status);
	if (rc == VCD_ERR_CLIENT_FATAL)
		vcd_handle_clnt_fatal_input_done(cctxt, frame->frm_trans_end);
	VCD_FAILED_RETURN(rc, "Failed: vcd_validate_io_done_pyld");
	transc = (struct vcd_transc *)frame->vcd_frm.ip_frm_tag;
	codec_config = frame->vcd_frm.flags & VCD_FRAME_FLAG_CODECCONFIG;
	rc = vcd_handle_input_done(cctxt,
		payload, VCD_EVT_RESP_INPUT_DONE, status);
	VCD_FAILED_RETURN(rc, "Failed: vcd_handle_input_done");
	if (frame->vcd_frm.flags & VCD_FRAME_FLAG_EOS) {
		VCD_MSG_HIGH("Got input done for EOS initiator");
		transc->input_done = false;
		transc->in_use = true;
		if ((codec_config &&
			 (status != VCD_ERR_BITSTREAM_ERR)) ||
			((status == VCD_ERR_BITSTREAM_ERR) &&
			 !(cctxt->status.mask & VCD_FIRST_IP_DONE) &&
			 (core_type == VCD_CORE_720P)))
			vcd_handle_eos_done(cctxt, transc, VCD_S_SUCCESS);
	}
	return rc;
}

u32 vcd_validate_io_done_pyld(
	struct vcd_clnt_ctxt *cctxt, void *payload, u32 status)
{
	struct ddl_frame_data_tag *frame =
		(struct ddl_frame_data_tag *) payload;
	struct vcd_dev_ctxt *dev_ctxt = cctxt->dev_ctxt;
	struct vcd_transc *transc = NULL;
	u32 rc = VCD_S_SUCCESS, i = 0;

	if (!frame) {
		VCD_MSG_ERROR("Bad payload from DDL");
		return VCD_ERR_BAD_POINTER;
	}

	transc = (struct vcd_transc *)frame->vcd_frm.ip_frm_tag;
	if (dev_ctxt->trans_tbl) {
		while (i < dev_ctxt->trans_tbl_size &&
			transc != &dev_ctxt->trans_tbl[i])
			i++;
		if (i == dev_ctxt->trans_tbl_size ||
			!dev_ctxt->trans_tbl[i].in_use)
			rc = VCD_ERR_CLIENT_FATAL;
	} else
		rc = VCD_ERR_CLIENT_FATAL;

	if (VCD_FAILED(rc)) {
		VCD_MSG_FATAL(
			"vcd_validate_io_done_pyld: invalid transaction");
	} else if (!frame->vcd_frm.virtual &&
		status != VCD_ERR_INTRLCD_FIELD_DROP)
		rc = VCD_ERR_BAD_POINTER;

	return rc;
}

void vcd_handle_input_done_failed(
	struct vcd_clnt_ctxt *cctxt, struct vcd_transc *transc)
{
	if (cctxt->decoding) {
		cctxt->sched_clnt_hdl->tkns++;
		vcd_release_trans_tbl_entry(transc);
	}
}

void vcd_handle_input_done_with_codec_config(
	struct vcd_clnt_ctxt *cctxt, struct vcd_transc *transc,
	struct ddl_frame_data_tag *frm)
{
	cctxt->sched_clnt_hdl->tkns++;
	if (frm->frm_trans_end)
		vcd_release_trans_tbl_entry(transc);
}

void vcd_handle_input_done_for_interlacing(struct vcd_clnt_ctxt *cctxt)
{
	cctxt->status.int_field_cnt++;
	if (cctxt->status.int_field_cnt == 1)
		cctxt->sched_clnt_hdl->tkns++;
	else if (cctxt->status.int_field_cnt ==
		VCD_DEC_NUM_INTERLACED_FIELDS)
		cctxt->status.int_field_cnt = 0;
}

void vcd_handle_input_done_with_trans_end(
	struct vcd_clnt_ctxt *cctxt)
{
	if (!cctxt->decoding)
		return;
	if (cctxt->out_buf_pool.in_use <
		cctxt->out_buf_pool.buf_req.min_count)
		return;
	if (!cctxt->sched_clnt_hdl->tkns)
		cctxt->sched_clnt_hdl->tkns++;
}

u32 vcd_handle_output_required(struct vcd_clnt_ctxt
	*cctxt, void *payload, u32 status)
{
	struct vcd_transc *transc;
	struct ddl_frame_data_tag *frame =
		(struct ddl_frame_data_tag *)payload;
	u32 rc = VCD_S_SUCCESS;

	if (!cctxt->status.frame_submitted &&
		!cctxt->status.frame_delayed) {
		VCD_MSG_ERROR("\n Input done was not expected");
		return VCD_ERR_BAD_STATE;
	}

	rc = vcd_validate_io_done_pyld(cctxt, payload, status);
	if (rc == VCD_ERR_CLIENT_FATAL)
		vcd_handle_clnt_fatal_input_done(cctxt, frame->frm_trans_end);
	VCD_FAILED_RETURN(rc, "\n Bad input done payload");

	transc = (struct vcd_transc *)frame->
		vcd_frm.ip_frm_tag;

	if ((transc->ip_buf_entry->frame.virtual !=
		 frame->vcd_frm.virtual) ||
		!transc->ip_buf_entry->in_use) {
		VCD_MSG_ERROR("\n Bad frm transaction state");
		vcd_handle_clnt_fatal_input_done(cctxt, frame->frm_trans_end);
		return VCD_ERR_BAD_STATE;
	}
	rc = vcd_sched_queue_buffer(cctxt->sched_clnt_hdl,
			transc->ip_buf_entry, false);
	VCD_FAILED_RETURN(rc, "Failed: vcd_sched_queue_buffer");

	transc->ip_buf_entry = NULL;
	vcd_release_trans_tbl_entry(transc);
	frame->frm_trans_end = true;

	if (VCD_FAILED(status))
		VCD_MSG_ERROR("\n OUTPUT_REQ returned err = 0x%x",
			status);

	if (cctxt->status.frame_submitted > 0)
		cctxt->status.frame_submitted--;
	else
		cctxt->status.frame_delayed--;


	if (!VCD_FAILED(status) &&
		cctxt->decoding &&
		frame->vcd_frm.interlaced) {
		if (cctxt->status.int_field_cnt > 0) {
			VCD_MSG_ERROR("\n Not expected: OUTPUT_REQ"
				"for 2nd interlace field");
			rc = VCD_ERR_FAIL;
		}
	}

	return rc;
}

u32 vcd_handle_output_required_in_flushing(
struct vcd_clnt_ctxt *cctxt, void *payload)
{
	u32 rc;
	struct vcd_transc *transc;
	struct ddl_frame_data_tag *frame =
		(struct ddl_frame_data_tag *)payload;

	rc = vcd_validate_io_done_pyld(cctxt, payload, VCD_S_SUCCESS);
	if (rc == VCD_ERR_CLIENT_FATAL)
		vcd_handle_clnt_fatal_input_done(cctxt, frame->frm_trans_end);
	VCD_FAILED_RETURN(rc, "Bad input done payload");

	transc = (struct vcd_transc *)
		(((struct ddl_frame_data_tag *)payload)->
		 vcd_frm.ip_frm_tag);

	((struct ddl_frame_data_tag *)payload)->
		vcd_frm.interlaced = false;

	rc = vcd_handle_input_done(cctxt, payload,
			VCD_EVT_RESP_INPUT_FLUSHED, VCD_S_SUCCESS);
	VCD_FAILED_RETURN(rc, "Failed: vcd_handle_input_done");

	vcd_release_trans_tbl_entry(transc);
	((struct ddl_frame_data_tag *)payload)->frm_trans_end = true;

	return rc;
}

u32 vcd_handle_frame_done(
	struct vcd_clnt_ctxt *cctxt,
	 void *payload, u32 event, u32 status)
{
	struct vcd_buffer_entry *op_buf_entry = NULL;
	struct ddl_frame_data_tag *op_frm =
		(struct ddl_frame_data_tag *) payload;
	struct vcd_transc *transc;
	u32 rc;
	s64 time_stamp;

	rc = vcd_validate_io_done_pyld(cctxt, payload, status);
	if (rc == VCD_ERR_CLIENT_FATAL)
		vcd_handle_clnt_fatal(cctxt, op_frm->frm_trans_end);
	VCD_FAILED_RETURN(rc, "Bad payload recvd");

	transc = (struct vcd_transc *)op_frm->vcd_frm.ip_frm_tag;

	if (op_frm->vcd_frm.virtual) {

		if (!transc->op_buf_entry) {
			op_buf_entry =
				vcd_find_buffer_pool_entry(
					&cctxt->out_buf_pool,
					op_frm->vcd_frm.
					virtual);
		} else {
			op_buf_entry = transc->op_buf_entry;
		}

		if (!op_buf_entry) {
			VCD_MSG_ERROR("Invalid output buffer returned"
				"from DDL");
			vcd_handle_clnt_fatal(cctxt, op_frm->frm_trans_end);
			rc = VCD_ERR_BAD_POINTER;
		} else if (!op_buf_entry->in_use) {
			VCD_MSG_ERROR("Bad output buffer 0x%p recvd from DDL",
					  op_buf_entry->frame.virtual);
			vcd_handle_clnt_fatal(cctxt, op_frm->frm_trans_end);
			rc = VCD_ERR_BAD_POINTER;
		} else {
			op_buf_entry->in_use = false;
			VCD_BUFFERPOOL_INUSE_DECREMENT(
				cctxt->out_buf_pool.in_use);
			VCD_MSG_LOW("outBufPool.InUse = %d",
						cctxt->out_buf_pool.in_use);
		}
	}
	VCD_FAILED_RETURN(rc, "Bad output buffer pointer");
	op_frm->vcd_frm.time_stamp = transc->time_stamp;
	op_frm->vcd_frm.ip_frm_tag = transc->ip_frm_tag;

	if (transc->flags & VCD_FRAME_FLAG_EOSEQ)
		op_frm->vcd_frm.flags |= VCD_FRAME_FLAG_EOSEQ;
	else
		op_frm->vcd_frm.flags &= ~VCD_FRAME_FLAG_EOSEQ;

	if (cctxt->decoding)
		op_frm->vcd_frm.frame = transc->frame;
	else
		transc->frame = op_frm->vcd_frm.frame;
	transc->frame_done = true;

	if (transc->input_done && transc->frame_done) {
		time_stamp = transc->time_stamp;
		vcd_release_trans_tbl_entry(transc);
	}

	if (status == VCD_ERR_INTRLCD_FIELD_DROP ||
		(op_frm->vcd_frm.intrlcd_ip_frm_tag !=
		VCD_FRAMETAG_INVALID &&
		op_frm->vcd_frm.intrlcd_ip_frm_tag)) {
		vcd_handle_frame_done_for_interlacing(cctxt, transc,
							  op_frm, status);
		if (status == VCD_ERR_INTRLCD_FIELD_DROP) {
			cctxt->callback(VCD_EVT_IND_INFO_FIELD_DROPPED,
				VCD_S_SUCCESS,
				&time_stamp,
				sizeof(time_stamp),
				cctxt, cctxt->client_data);
		}
	}

	if (status != VCD_ERR_INTRLCD_FIELD_DROP) {
		cctxt->callback(event,
			status,
			&op_frm->vcd_frm,
			sizeof(struct vcd_frame_data),
			cctxt, cctxt->client_data);
	}
	return rc;
}

u32 vcd_handle_frame_done_in_eos(
	struct vcd_clnt_ctxt *cctxt, void *payload, u32 status)
{
	struct ddl_frame_data_tag *frame =
		(struct ddl_frame_data_tag *) payload;
	u32 rc = VCD_S_SUCCESS;
	VCD_MSG_LOW("vcd_handle_frame_done_in_eos:");
	rc = vcd_validate_io_done_pyld(cctxt, payload, status);
	if (rc == VCD_ERR_CLIENT_FATAL)
		vcd_handle_clnt_fatal(cctxt, frame->frm_trans_end);
	VCD_FAILED_RETURN(rc, "Bad payload received");

	if (cctxt->status.mask & VCD_EOS_PREV_VALID) {
		rc = vcd_handle_frame_done(cctxt,
			(void *)&cctxt->status.
			eos_prev_op_frm,
			VCD_EVT_RESP_OUTPUT_DONE,
			cctxt->status.eos_prev_op_frm_status);
		VCD_FAILED_RETURN(rc, "Failed: vcd_handle_frame_done");
	}

	cctxt->status.eos_prev_op_frm = *frame;
	cctxt->status.eos_prev_op_frm_status = status;
	cctxt->status.mask |= VCD_EOS_PREV_VALID;
	return rc;
}

void vcd_handle_frame_done_for_interlacing(
	struct vcd_clnt_ctxt *cctxt,
	 struct vcd_transc *transc_ip1,
	 struct ddl_frame_data_tag *op_frm, u32 status)
{
	struct vcd_transc *transc_ip2 =
		(struct vcd_transc *)op_frm->\
		vcd_frm.intrlcd_ip_frm_tag;

	if (status == VCD_ERR_INTRLCD_FIELD_DROP) {
		cctxt->status.int_field_cnt = 0;
		return;
	}

	op_frm->vcd_frm.intrlcd_ip_frm_tag = transc_ip2->ip_frm_tag;

	transc_ip2->frame_done = true;

	if (transc_ip2->input_done && transc_ip2->frame_done)
		vcd_release_trans_tbl_entry(transc_ip2);

	if (!transc_ip1->frame || !transc_ip2->frame) {
		VCD_MSG_ERROR("DDL didn't provided frame type");
		return;
	}
}

u32 vcd_handle_first_fill_output_buffer(
	struct vcd_clnt_ctxt *cctxt,
	struct vcd_frame_data *buffer,
	u32 *handled)
{
	u32 rc = VCD_S_SUCCESS;
	rc = vcd_check_if_buffer_req_met(cctxt, VCD_BUFFER_OUTPUT);
	VCD_FAILED_RETURN(rc, "Output buffer requirements not met");
	if (cctxt->out_buf_pool.q_len > 0) {
		VCD_MSG_ERROR("Old output buffers were not flushed out");
		return VCD_ERR_BAD_STATE;
	}
	cctxt->status.mask |= VCD_FIRST_OP_RCVD;
	if (cctxt->sched_clnt_hdl)
		rc = vcd_sched_suspend_resume_clnt(cctxt, true);
	VCD_FAILED_RETURN(rc, "Failed: vcd_sched_suspend_resume_clnt");
	if (cctxt->decoding)
		rc = vcd_handle_first_fill_output_buffer_for_dec(
			cctxt, buffer, handled);
	else
		rc = vcd_handle_first_fill_output_buffer_for_enc(
			cctxt, buffer, handled);
	return rc;
}

u32 vcd_handle_first_fill_output_buffer_for_enc(
	struct vcd_clnt_ctxt *cctxt,
	struct vcd_frame_data *frm_entry,
	u32 *handled)
{
	u32 rc, seqhdr_present = 0;
	struct vcd_property_hdr prop_hdr;
	struct vcd_sequence_hdr seq_hdr;
	struct vcd_property_codec codec;
	*handled = true;
	prop_hdr.prop_id = DDL_I_SEQHDR_PRESENT;
	prop_hdr.sz = sizeof(seqhdr_present);
	rc = ddl_get_property(cctxt->ddl_handle, &prop_hdr, &seqhdr_present);
	VCD_FAILED_RETURN(rc, "Failed: DDL_I_SEQHDR_PRESENT");
	if (!seqhdr_present) {
		*handled = false;
		return VCD_S_SUCCESS;
	}

	prop_hdr.prop_id = VCD_I_CODEC;
	prop_hdr.sz = sizeof(struct vcd_property_codec);
	rc = ddl_get_property(cctxt->ddl_handle, &prop_hdr, &codec);
	if (!VCD_FAILED(rc)) {
		if (codec.codec != VCD_CODEC_H263) {
			prop_hdr.prop_id = VCD_I_SEQ_HEADER;
			prop_hdr.sz = sizeof(struct vcd_sequence_hdr);
			seq_hdr.sequence_header = frm_entry->virtual;
			seq_hdr.sequence_header_len =
				frm_entry->alloc_len;
			rc = ddl_get_property(cctxt->ddl_handle,
				&prop_hdr, &seq_hdr);
			if (!VCD_FAILED(rc)) {
				frm_entry->data_len =
					seq_hdr.sequence_header_len;
				frm_entry->time_stamp = 0;
				frm_entry->flags |=
					VCD_FRAME_FLAG_CODECCONFIG;
				cctxt->callback(VCD_EVT_RESP_OUTPUT_DONE,
					VCD_S_SUCCESS, frm_entry,
					sizeof(struct vcd_frame_data),
					cctxt,
					cctxt->client_data);
			} else
			VCD_MSG_ERROR(
				"rc = 0x%x. Failed:\
				ddl_get_property: VCD_I_SEQ_HEADER",
				rc);
		} else
			VCD_MSG_LOW("Codec Type is H.263\n");
	} else
		VCD_MSG_ERROR(
			"rc = 0x%x. Failed: ddl_get_property:VCD_I_CODEC",
			rc);
	return rc;
}

u32 vcd_handle_first_fill_output_buffer_for_dec(
	struct vcd_clnt_ctxt *cctxt,
	struct vcd_frame_data *frm_entry,
	u32 *handled)
{
	u32 rc;
	struct vcd_property_hdr prop_hdr;
	struct vcd_buffer_pool *out_buf_pool;
	struct ddl_property_dec_pic_buffers dpb;
	struct ddl_frame_data_tag *dpb_list;
	u8 i;

	(void)frm_entry;
	*handled = true;
	prop_hdr.prop_id = DDL_I_DPB;
	prop_hdr.sz = sizeof(dpb);
	out_buf_pool = &cctxt->out_buf_pool;

	dpb_list = (struct ddl_frame_data_tag *)
		kmalloc((sizeof(struct ddl_frame_data_tag) *
		out_buf_pool->count), GFP_KERNEL);

	if (!dpb_list) {
		VCD_MSG_ERROR("Memory allocation failure");
		return VCD_ERR_ALLOC_FAIL;
	}

	for (i = 1; i <= out_buf_pool->count; i++)
		dpb_list[i - 1].vcd_frm = out_buf_pool->entries[i].frame;

	dpb.dec_pic_buffers = dpb_list;
	dpb.no_of_dec_pic_buf = out_buf_pool->count;
	rc = ddl_set_property(cctxt->ddl_handle, &prop_hdr, &dpb);

	kfree(dpb_list);
	*handled = false;

	return VCD_S_SUCCESS;
}

void vcd_handle_eos_trans_end(struct vcd_clnt_ctxt *cctxt)
{
	u32 rc = VCD_S_SUCCESS;
	if (cctxt->status.mask & VCD_EOS_PREV_VALID) {
		rc = vcd_handle_frame_done(cctxt,
			(void *)&cctxt->status.eos_prev_op_frm,
			VCD_EVT_RESP_OUTPUT_DONE,
			cctxt->status.eos_prev_op_frm_status);
		cctxt->status.mask &= ~VCD_EOS_PREV_VALID;
	}
	if (VCD_FAILED(rc))
		return;

	if (cctxt->status.mask & VCD_FLUSH_ALL)
		vcd_process_pending_flush_in_eos(cctxt);

	if (cctxt->status.mask & VCD_STOP_PENDING)
		vcd_process_pending_stop_in_eos(cctxt);
	else {
		vcd_do_client_state_transition(cctxt,
			VCD_CLIENT_STATE_RUN,
			CLIENT_STATE_EVENT_NUMBER(clnt_cb));
	}
}

void vcd_handle_eos_done(struct vcd_clnt_ctxt *cctxt,
	 struct vcd_transc *transc, u32 status)
{
	struct vcd_frame_data  vcd_frm;
	u32 rc = VCD_S_SUCCESS, sent_eos_frm = false;
	VCD_MSG_LOW("vcd_handle_eos_done:");

	if (VCD_FAILED(status))
		VCD_MSG_ERROR("EOS DONE returned error = 0x%x", status);

	if (cctxt->status.mask & VCD_EOS_PREV_VALID) {
		cctxt->status.eos_prev_op_frm.vcd_frm.flags |=
			VCD_FRAME_FLAG_EOS;

		rc = vcd_handle_frame_done(cctxt,
						(void *)&cctxt->status.
						eos_prev_op_frm,
						VCD_EVT_RESP_OUTPUT_DONE,
						cctxt->status.
							eos_prev_op_frm_status);
		cctxt->status.mask &= ~VCD_EOS_PREV_VALID;
		if (!VCD_FAILED(rc) &&
			cctxt->status.eos_prev_op_frm_status !=
				VCD_ERR_INTRLCD_FIELD_DROP)
			sent_eos_frm = true;
	}
	if (!sent_eos_frm) {
		if (transc->ip_buf_entry) {
			transc->ip_buf_entry->frame.ip_frm_tag =
				transc->ip_frm_tag;

			vcd_send_frame_done_in_eos(cctxt,
				&transc->ip_buf_entry->frame, false);
		} else {
			memset(&vcd_frm, 0, sizeof(struct vcd_frame_data));
			vcd_frm.ip_frm_tag = transc->ip_frm_tag;
			vcd_frm.time_stamp = transc->time_stamp;
			vcd_frm.flags = VCD_FRAME_FLAG_EOS;
			vcd_send_frame_done_in_eos(cctxt, &vcd_frm, true);
		}
	}
	if (VCD_FAILED(rc))
		return;
	if (transc->ip_buf_entry) {
		if (transc->ip_buf_entry->frame.virtual) {
			transc->ip_buf_entry->frame.ip_frm_tag =
				transc->ip_frm_tag;
			cctxt->callback(VCD_EVT_RESP_INPUT_DONE,
					  VCD_S_SUCCESS,
					  &transc->ip_buf_entry->frame,
					  sizeof(struct vcd_frame_data),
					  cctxt, cctxt->client_data);
		}
		transc->ip_buf_entry->in_use = false;
		VCD_BUFFERPOOL_INUSE_DECREMENT(cctxt->in_buf_pool.in_use);
		transc->ip_buf_entry = NULL;
		if (cctxt->status.frame_submitted)
			cctxt->status.frame_submitted--;
		else
			cctxt->status.frame_delayed--;
	}

	vcd_release_trans_tbl_entry(transc);
	if (cctxt->status.mask & VCD_FLUSH_ALL)
		vcd_process_pending_flush_in_eos(cctxt);

	if (cctxt->status.mask & VCD_STOP_PENDING) {
		vcd_process_pending_stop_in_eos(cctxt);
	} else if (!(cctxt->status.mask & VCD_EOS_WAIT_OP_BUF)) {
		vcd_do_client_state_transition(cctxt,
						   VCD_CLIENT_STATE_RUN,
						   CLIENT_STATE_EVENT_NUMBER
						   (clnt_cb));
	}
}

void vcd_handle_start_done(struct vcd_clnt_ctxt *cctxt,
	struct vcd_transc *transc, u32 status)
{
	cctxt->status.cmd_submitted--;
	vcd_mark_command_channel(cctxt->dev_ctxt, transc);

	if (!VCD_FAILED(status)) {
		cctxt->callback(VCD_EVT_RESP_START, status, NULL,
			0, cctxt,	cctxt->client_data);

		vcd_do_client_state_transition(cctxt,
			VCD_CLIENT_STATE_RUN,
			CLIENT_STATE_EVENT_NUMBER(clnt_cb));
	} else {
		VCD_MSG_ERROR("ddl callback returned failure."
			"status = 0x%x", status);
		vcd_handle_err_in_starting(cctxt, status);
	}
}

void vcd_handle_stop_done(struct vcd_clnt_ctxt *cctxt,
	struct vcd_transc *transc, u32 status)
{

	VCD_MSG_LOW("vcd_handle_stop_done:");
	cctxt->status.cmd_submitted--;
	vcd_mark_command_channel(cctxt->dev_ctxt, transc);

	if (!VCD_FAILED(status)) {
		vcd_do_client_state_transition(cctxt,
			VCD_CLIENT_STATE_OPEN,
			CLIENT_STATE_EVENT_NUMBER(clnt_cb));
	} else {
		VCD_MSG_FATAL("STOP_DONE returned error = 0x%x", status);
		status = VCD_ERR_HW_FATAL;
		vcd_handle_device_err_fatal(cctxt->dev_ctxt, cctxt);
		vcd_do_client_state_transition(cctxt,
			VCD_CLIENT_STATE_INVALID,
			CLIENT_STATE_EVENT_NUMBER(clnt_cb));
	}

	cctxt->callback(VCD_EVT_RESP_STOP, status, NULL, 0, cctxt,
					  cctxt->client_data);

	memset(&cctxt->status, 0, sizeof(struct vcd_clnt_status));
}

void vcd_handle_stop_done_in_starting(struct vcd_clnt_ctxt
	*cctxt, struct vcd_transc *transc, u32 status)
{
	VCD_MSG_LOW("vcd_handle_stop_done_in_starting:");
	cctxt->status.cmd_submitted--;
	vcd_mark_command_channel(cctxt->dev_ctxt, transc);
	if (!VCD_FAILED(status)) {
		cctxt->callback(VCD_EVT_RESP_START, cctxt->status.
			last_err, NULL, 0, cctxt, cctxt->client_data);
		vcd_do_client_state_transition(cctxt, VCD_CLIENT_STATE_OPEN,
			   CLIENT_STATE_EVENT_NUMBER(clnt_cb));
	} else {
		VCD_MSG_FATAL("VCD Cleanup: STOP_DONE returned error "
			"= 0x%x", status);
		vcd_handle_err_fatal(cctxt, VCD_EVT_RESP_START,
			VCD_ERR_HW_FATAL);
	}
}

void vcd_handle_stop_done_in_invalid(struct vcd_clnt_ctxt *cctxt,
	struct vcd_transc *transc, u32 status)
{
	u32 rc;
	VCD_MSG_LOW("vcd_handle_stop_done_in_invalid:");

	cctxt->status.cmd_submitted--;
	vcd_mark_command_channel(cctxt->dev_ctxt, transc);

	if (!VCD_FAILED(status)) {
		vcd_client_cmd_flush_and_en_q(cctxt, VCD_CMD_CLIENT_CLOSE);
		if (cctxt->status.frame_submitted) {
			vcd_release_multiple_frame_channels(cctxt->dev_ctxt,
			cctxt->status.frame_submitted);

			cctxt->status.frame_submitted = 0;
			cctxt->status.frame_delayed = 0;
		}
		if (cctxt->status.cmd_submitted) {
			vcd_release_multiple_command_channels(
				cctxt->dev_ctxt,
				cctxt->status.cmd_submitted);
			cctxt->status.cmd_submitted = 0;
		}
	} else {
		VCD_MSG_FATAL("VCD Cleanup: STOP_DONE returned error "
			"= 0x%x", status);
		vcd_handle_device_err_fatal(cctxt->dev_ctxt, cctxt);
		cctxt->status.mask &= ~VCD_CLEANING_UP;
	}
	vcd_flush_buffers_in_err_fatal(cctxt);
	VCD_MSG_HIGH("VCD cleanup: All buffers are returned");
	if (cctxt->status.mask & VCD_STOP_PENDING) {
		cctxt->callback(VCD_EVT_RESP_STOP, VCD_S_SUCCESS, NULL, 0,
			cctxt, cctxt->client_data);
		cctxt->status.mask &= ~VCD_STOP_PENDING;
	}
	rc = vcd_power_event(cctxt->dev_ctxt, cctxt,
						  VCD_EVT_PWR_CLNT_ERRFATAL);
	if (VCD_FAILED(rc))
		VCD_MSG_ERROR("VCD_EVT_PWR_CLNT_ERRFATAL failed");
	if (!(cctxt->status.mask & VCD_CLEANING_UP) &&
		cctxt->status.mask & VCD_CLOSE_PENDING) {
		vcd_destroy_client_context(cctxt);
		vcd_handle_for_last_clnt_close(cctxt->dev_ctxt, false);
	}
}

u32 vcd_handle_input_frame(
	struct vcd_clnt_ctxt *cctxt,
	 struct vcd_frame_data *input_frame)
{
	struct vcd_dev_ctxt *dev_ctxt = cctxt->dev_ctxt;
	struct vcd_buffer_entry *buf_entry, *orig_frame;
	struct vcd_frame_data *frm_entry;
	u32 rc = VCD_S_SUCCESS;
	u32 eos_handled = false;

	VCD_MSG_LOW("vcd_handle_input_frame:");

	VCD_MSG_LOW("input buffer: addr=(0x%p), sz=(%d), len=(%d)",
			input_frame->virtual, input_frame->alloc_len,
			input_frame->data_len);

	if (!input_frame->virtual &&
		!(input_frame->flags & VCD_FRAME_FLAG_EOS)) {
		VCD_MSG_ERROR("Bad frame ptr/len/EOS combination");
		return VCD_ERR_ILLEGAL_PARM;
	}


	if (!input_frame->data_len &&
		!(input_frame->flags & VCD_FRAME_FLAG_EOS)) {
		VCD_MSG_MED("data_len = 0, returning INPUT DONE");
		cctxt->callback(VCD_EVT_RESP_INPUT_DONE,
				  VCD_ERR_INPUT_NOT_PROCESSED,
				  input_frame,
				  sizeof(struct vcd_frame_data),
				  cctxt, cctxt->client_data);
		return VCD_S_SUCCESS;
	}

	if (!(cctxt->status.mask & VCD_FIRST_IP_RCVD)) {
		if (cctxt->decoding)
			rc = vcd_handle_first_decode_frame(cctxt);

		if (!VCD_FAILED(rc)) {
			cctxt->status.first_ts = input_frame->time_stamp;
			cctxt->status.prev_ts = cctxt->status.first_ts;

			cctxt->status.mask |= VCD_FIRST_IP_RCVD;

			(void)vcd_power_event(cctxt->dev_ctxt,
						  cctxt,
						  VCD_EVT_PWR_CLNT_FIRST_FRAME);
		}
	}
	VCD_FAILED_RETURN(rc, "Failed: First frame handling");

	orig_frame = vcd_find_buffer_pool_entry(&cctxt->in_buf_pool,
						 input_frame->virtual);
	if (!orig_frame) {
		VCD_MSG_ERROR("Bad buffer addr: %p", input_frame->virtual);
		return VCD_ERR_FAIL;
	}

	if (orig_frame->in_use) {
		/*
		 * This path only allowed for enc., dec. not allowed
		 * to queue same buffer multiple times
		 */
		if (cctxt->decoding) {
			VCD_MSG_ERROR("An inuse input frame is being "
					"re-queued to scheduler");
			return VCD_ERR_FAIL;
		}

		buf_entry = kzalloc(sizeof(*buf_entry), GFP_KERNEL);
		if (!buf_entry) {
			VCD_MSG_ERROR("Unable to alloc memory");
			return VCD_ERR_FAIL;
		}

		INIT_LIST_HEAD(&buf_entry->sched_list);
		/*
		 * Pre-emptively poisoning this, as these dupe entries
		 * shouldn't get added to any list
		 */
		INIT_LIST_HEAD(&buf_entry->list);
		buf_entry->list.next = LIST_POISON1;
		buf_entry->list.prev = LIST_POISON2;

		buf_entry->valid = orig_frame->valid;
		buf_entry->alloc = orig_frame->alloc;
		buf_entry->virtual = orig_frame->virtual;
		buf_entry->physical = orig_frame->physical;
		buf_entry->sz = orig_frame->sz;
		buf_entry->allocated = orig_frame->allocated;
		buf_entry->in_use = 1; /* meaningless for the dupe buffers */
		buf_entry->frame = orig_frame->frame;
	} else
		buf_entry = orig_frame;

	if (input_frame->alloc_len > buf_entry->sz) {
		VCD_MSG_ERROR("Bad buffer Alloc_len %d, Actual sz=%d",
			input_frame->alloc_len, buf_entry->sz);

		return VCD_ERR_ILLEGAL_PARM;
	}

	frm_entry = &buf_entry->frame;

	*frm_entry = *input_frame;
	frm_entry->physical = buf_entry->physical;

	if (input_frame->flags & VCD_FRAME_FLAG_EOS) {
		rc = vcd_handle_recvd_eos(cctxt, input_frame,
					  &eos_handled);
	}

	if (VCD_FAILED(rc) || eos_handled) {
		VCD_MSG_HIGH("rc = 0x%x, eos_handled = %d", rc,
				 eos_handled);

		return rc;
	}
	rc = vcd_sched_queue_buffer(
		cctxt->sched_clnt_hdl, buf_entry, true);
	VCD_FAILED_RETURN(rc, "Failed: vcd_sched_queue_buffer");

	orig_frame->in_use++;
	cctxt->in_buf_pool.in_use++;
	vcd_try_submit_frame(dev_ctxt);
	return rc;
}

void vcd_release_all_clnt_frm_transc(struct vcd_clnt_ctxt *cctxt)
{
	struct vcd_dev_ctxt *dev_ctxt = cctxt->dev_ctxt;
	u32 i, cntr = 0;
	VCD_MSG_LOW("vcd_release_all_clnt_frm_transc:");
	for (i = 0; i < dev_ctxt->trans_tbl_size; i++) {
		if (dev_ctxt->trans_tbl[i].in_use &&
			cctxt == dev_ctxt->trans_tbl[i].cctxt) {
			if (dev_ctxt->trans_tbl[i].
				type == VCD_CMD_CODE_FRAME ||
				dev_ctxt->trans_tbl[i].
				type == VCD_CMD_NONE) {
				vcd_release_trans_tbl_entry(&dev_ctxt->
								trans_tbl[i]);
			} else {
				VCD_MSG_LOW("vcd_transaction in use type(%u)",
					dev_ctxt->trans_tbl[i].type);
				cntr++;
			}
		}
	}
	if (cntr)
		VCD_MSG_ERROR("vcd_transactions still in use: (%d)", cntr);
}

void vcd_release_all_clnt_transc(struct vcd_clnt_ctxt *cctxt)
{
	struct vcd_dev_ctxt *dev_ctxt = cctxt->dev_ctxt;
	u32 i;

	VCD_MSG_LOW("vcd_release_all_clnt_transc:");

	for (i = 0; i < dev_ctxt->trans_tbl_size; i++) {
		if (dev_ctxt->trans_tbl[i].in_use &&
			cctxt == dev_ctxt->trans_tbl[i].cctxt) {
				vcd_release_trans_tbl_entry(
					&dev_ctxt->trans_tbl[i]);
		}
	}
}

void vcd_send_flush_done(struct vcd_clnt_ctxt *cctxt, u32 status)
{
	VCD_MSG_LOW("vcd_send_flush_done:");

	if (cctxt->status.mask & VCD_FLUSH_INPUT) {
		cctxt->callback(VCD_EVT_RESP_FLUSH_INPUT_DONE,
			status, NULL, 0, cctxt, cctxt->client_data);
		cctxt->status.mask &= ~VCD_FLUSH_INPUT;
	}

	if (cctxt->status.mask & VCD_FLUSH_OUTPUT) {
		cctxt->callback(VCD_EVT_RESP_FLUSH_OUTPUT_DONE,
			status, NULL, 0, cctxt, cctxt->client_data);
		cctxt->status.mask &= ~VCD_FLUSH_OUTPUT;
	}
}

u32 vcd_store_seq_hdr(
	struct vcd_clnt_ctxt *cctxt,
	 struct vcd_sequence_hdr *seq_hdr)
{
	u32 rc;
	struct vcd_property_hdr prop_hdr;
	u32 align;
	u8 *virtual_aligned;
	u32 addr;
	int ret = 0;

	if (!seq_hdr->sequence_header_len
		|| !seq_hdr->sequence_header) {
		VCD_MSG_ERROR("Bad seq hdr");

		return VCD_ERR_BAD_POINTER;
	}

	if (cctxt->seq_hdr.sequence_header) {
		VCD_MSG_HIGH("Old seq hdr detected");

		vcd_pmem_free(cctxt->seq_hdr.sequence_header,
				  cctxt->seq_hdr_phy_addr, cctxt);
		cctxt->seq_hdr.sequence_header = NULL;
	}

	cctxt->seq_hdr.sequence_header_len =
		seq_hdr->sequence_header_len;

	prop_hdr.prop_id = DDL_I_SEQHDR_ALIGN_BYTES;
	prop_hdr.sz = sizeof(u32);

	rc = ddl_get_property(cctxt->ddl_handle, &prop_hdr, &align);

	VCD_FAILED_RETURN(rc,
			  "Failed: ddl_get_property DDL_I_SEQHDR_ALIGN_BYTES");

	VCD_MSG_MED("Seq hdr alignment bytes = %d", align);

	ret = vcd_pmem_alloc(cctxt->seq_hdr.sequence_header_len + align +
				 VCD_SEQ_HDR_PADDING_BYTES,
				 &(cctxt->seq_hdr.sequence_header),
				 &(cctxt->seq_hdr_phy_addr), cctxt);

	if (ret < 0) {
		VCD_MSG_ERROR("Seq hdr allocation failed");

		return VCD_ERR_ALLOC_FAIL;
	}

	if (!cctxt->seq_hdr_phy_addr) {
		VCD_MSG_ERROR("Couldn't get physical address");

		return VCD_ERR_BAD_POINTER;
	}

	if (align > 0) {
		addr = (u32) cctxt->seq_hdr_phy_addr;
		addr += align;
		addr -= (addr % align);
		virtual_aligned = cctxt->seq_hdr.sequence_header;
		virtual_aligned += (u32) (addr -
			(u32) cctxt->seq_hdr_phy_addr);
		cctxt->seq_hdr_phy_addr = (u8 *) addr;
	} else {
		virtual_aligned = cctxt->seq_hdr.sequence_header;
	}

	memcpy(virtual_aligned, seq_hdr->sequence_header,
		seq_hdr->sequence_header_len);

	return VCD_S_SUCCESS;
}

u32 vcd_set_frame_rate(
	struct vcd_clnt_ctxt *cctxt,
	 struct vcd_property_frame_rate *fps)
{
	u32 rc;
	cctxt->frm_rate = *fps;
	rc = vcd_update_clnt_perf_lvl(cctxt, &cctxt->frm_rate,
					  cctxt->frm_p_units);
	if (VCD_FAILED(rc)) {
		VCD_MSG_ERROR("rc = 0x%x. Failed: vcd_update_clnt_perf_lvl",
				  rc);
	}
	rc = vcd_sched_update_config(cctxt);
	return rc;
}

u32 vcd_req_perf_level(
	struct vcd_clnt_ctxt *cctxt,
	 struct vcd_property_perf_level *perf_level)
{
	u32 rc;
	u32 res_trk_perf_level;
	if (!perf_level) {
		VCD_MSG_ERROR("Invalid parameters\n");
		return -EINVAL;
	}
	res_trk_perf_level = get_res_trk_perf_level(perf_level->level);
	if (res_trk_perf_level < 0) {
		rc = -ENOTSUPP;
		goto perf_level_not_supp;
	}
	rc = vcd_set_perf_level(cctxt->dev_ctxt, res_trk_perf_level);
	if (!rc) {
		cctxt->reqd_perf_lvl = res_trk_perf_level;
		cctxt->perf_set_by_client = 1;
	}
perf_level_not_supp:
	return rc;
}

u32 vcd_set_frame_size(
	struct vcd_clnt_ctxt *cctxt,
	 struct vcd_property_frame_size *frm_size)
{
	struct vcd_property_hdr prop_hdr;
	u32 rc;
	u32 frm_p_units;
	(void)frm_size;
	if (res_trk_get_disable_fullhd() && frm_size &&
		(frm_size->width * frm_size->height > 1280 * 720)) {
		VCD_MSG_ERROR("Frame size = %dX%d greater than 1280X720 not"
			"supported", frm_size->width, frm_size->height);
		return VCD_ERR_FAIL;
	}
	prop_hdr.prop_id = DDL_I_FRAME_PROC_UNITS;
	prop_hdr.sz = sizeof(frm_p_units);
	rc = ddl_get_property(cctxt->ddl_handle, &prop_hdr, &frm_p_units);
	VCD_FAILED_RETURN(rc, "Failed: Get DDL_I_FRAME_PROC_UNITS");

	cctxt->frm_p_units = frm_p_units;

	rc = vcd_update_clnt_perf_lvl(cctxt, &cctxt->frm_rate,
					  frm_p_units);
	if (VCD_FAILED(rc)) {
		VCD_MSG_ERROR("rc = 0x%x. Failed: vcd_update_clnt_perf_lvl",
				  rc);
	}
	return rc;
}

void vcd_process_pending_flush_in_eos(struct vcd_clnt_ctxt *cctxt)
{
	u32 rc = VCD_S_SUCCESS;
	VCD_MSG_HIGH("Buffer flush is pending");
	rc = vcd_flush_buffers(cctxt, cctxt->status.mask & VCD_FLUSH_ALL);
	if (VCD_FAILED(rc))
		VCD_MSG_ERROR("rc = 0x%x. Failed: vcd_flush_buffers", rc);
	cctxt->status.mask &= ~VCD_EOS_WAIT_OP_BUF;
	vcd_send_flush_done(cctxt, VCD_S_SUCCESS);
}

void vcd_process_pending_stop_in_eos(struct vcd_clnt_ctxt *cctxt)
{
	u32 rc = VCD_S_SUCCESS;
	rc = vcd_flush_buffers(cctxt, VCD_FLUSH_ALL);
	if (VCD_FAILED(rc))
		VCD_MSG_ERROR("rc = 0x%x. Failed: vcd_flush_buffers", rc);
	VCD_MSG_HIGH("All buffers are returned. Enqueuing stop cmd");
	vcd_client_cmd_flush_and_en_q(cctxt, VCD_CMD_CODEC_STOP);
	cctxt->status.mask &= ~VCD_STOP_PENDING;
	vcd_do_client_state_transition(cctxt,
					   VCD_CLIENT_STATE_STOPPING,
					   CLIENT_STATE_EVENT_NUMBER(stop));
}

u32 vcd_calculate_frame_delta(
	struct vcd_clnt_ctxt *cctxt,
	 struct vcd_frame_data *frame)
{
	u32 frm_delta;
	u64 temp, max = ~((u64)0);

	if (frame->time_stamp >= cctxt->status.prev_ts)
		temp = frame->time_stamp - cctxt->status.prev_ts;
	else
		temp = (max - cctxt->status.prev_ts) +
			frame->time_stamp;

	VCD_MSG_LOW("Curr_ts=%lld  Prev_ts=%lld Diff=%llu",
			frame->time_stamp, cctxt->status.prev_ts, temp);

	temp *= cctxt->time_resoln;
	(void)do_div(temp, VCD_TIMESTAMP_RESOLUTION);
	frm_delta = temp;
	cctxt->status.time_elapsed += frm_delta;

	temp = (cctxt->status.time_elapsed * VCD_TIMESTAMP_RESOLUTION);
	(void)do_div(temp, cctxt->time_resoln);
	cctxt->status.prev_ts = cctxt->status.first_ts + temp;

	VCD_MSG_LOW("Time_elapsed=%llu, Drift=%llu, new Prev_ts=%lld",
			cctxt->status.time_elapsed, temp,
			cctxt->status.prev_ts);

	return frm_delta;
}

struct vcd_buffer_entry *vcd_check_fill_output_buffer
	(struct vcd_clnt_ctxt *cctxt,
	 struct vcd_frame_data *buffer) {
	struct vcd_buffer_pool *buf_pool = &cctxt->out_buf_pool;
	struct vcd_buffer_entry *buf_entry;

	if (!buf_pool->entries) {
		VCD_MSG_ERROR("Buffers not set or allocated yet");

		return NULL;
	}

	if (!buffer->virtual) {
		VCD_MSG_ERROR("NULL buffer address provided");
		return NULL;
	}

	buf_entry =
		vcd_find_buffer_pool_entry(buf_pool, buffer->virtual);
	if (!buf_entry) {
		VCD_MSG_ERROR("Unrecognized buffer address provided = %p",
				  buffer->virtual);
		return NULL;
	}

	if (buf_entry->in_use) {
		VCD_MSG_ERROR
			("An inuse output frame is being provided for reuse");
		return NULL;
	}

	if ((buffer->alloc_len < buf_pool->buf_req.sz ||
		 buffer->alloc_len > buf_entry->sz) &&
		 !(cctxt->status.mask & VCD_IN_RECONFIG)) {
		VCD_MSG_ERROR
			("Bad buffer Alloc_len = %d, Actual sz = %d, "
			 " Min sz = %u",
			 buffer->alloc_len, buf_entry->sz,
			 buf_pool->buf_req.sz);
		return NULL;
	}

	return buf_entry;
}

void vcd_handle_ind_hw_err_fatal(struct vcd_clnt_ctxt *cctxt,
	u32 event, u32 status)
{
	if (cctxt->status.frame_submitted) {
		cctxt->status.frame_submitted--;
		vcd_mark_frame_channel(cctxt->dev_ctxt);
	}
	vcd_handle_err_fatal(cctxt, event, status);
}

void vcd_handle_err_fatal(struct vcd_clnt_ctxt *cctxt, u32 event,
						  u32 status)
{
	VCD_MSG_LOW("vcd_handle_err_fatal: event=%x, err=%x", event, status);
	if (!VCD_FAILED_FATAL(status))
		return;

	if (VCD_FAILED_DEVICE_FATAL(status)) {
		vcd_clnt_handle_device_err_fatal(cctxt, event);
		vcd_handle_device_err_fatal(cctxt->dev_ctxt, cctxt);
	} else if (VCD_FAILED_CLIENT_FATAL(status)) {
		cctxt->status.last_evt = event;
		cctxt->callback(event, VCD_ERR_HW_FATAL, NULL, 0, cctxt,
						   cctxt->client_data);
		cctxt->status.mask |= VCD_CLEANING_UP;
		vcd_client_cmd_flush_and_en_q(cctxt, VCD_CMD_CODEC_STOP);
		vcd_do_client_state_transition(cctxt,
			VCD_CLIENT_STATE_INVALID,
			CLIENT_STATE_EVENT_NUMBER(clnt_cb));
	}
}

void vcd_handle_err_in_starting(struct vcd_clnt_ctxt *cctxt,
								u32 status)
{
	VCD_MSG_LOW("\n vcd_handle_err_in_starting:");
	if (VCD_FAILED_FATAL(status)) {
		vcd_handle_err_fatal(cctxt, VCD_EVT_RESP_START, status);
	} else {
		cctxt->status.last_err = status;
		VCD_MSG_HIGH("\n VCD cleanup: Enqueuing stop cmd");
		vcd_client_cmd_flush_and_en_q(cctxt, VCD_CMD_CODEC_STOP);
	}
}

void vcd_handle_trans_pending(struct vcd_clnt_ctxt *cctxt)
{
	if (!cctxt->status.frame_submitted) {
		VCD_MSG_ERROR("Transaction pending response was not expected");
		return;
	}
	cctxt->status.frame_submitted--;
	cctxt->status.frame_delayed++;
	vcd_mark_frame_channel(cctxt->dev_ctxt);
}
void vcd_handle_submit_frame_failed(struct vcd_dev_ctxt
	*dev_ctxt, struct vcd_transc *transc)
{
	struct vcd_clnt_ctxt *cctxt = transc->cctxt;
	u32 rc;

	vcd_mark_frame_channel(dev_ctxt);
	vcd_release_trans_tbl_entry(transc);

	vcd_handle_err_fatal(cctxt, VCD_EVT_IND_HWERRFATAL,
		VCD_ERR_CLIENT_FATAL);

	if (vcd_get_command_channel(dev_ctxt, &transc)) {
		transc->type = VCD_CMD_CODEC_STOP;
		transc->cctxt = cctxt;
		rc = vcd_submit_cmd_sess_end(transc);
		if (VCD_FAILED(rc))	{
			vcd_release_command_channel(dev_ctxt, transc);
			VCD_MSG_ERROR("rc = 0x%x. Failed: VCD_SubmitCmdSessEnd",
				rc);
		}
	}
}

u32 vcd_check_if_buffer_req_met(struct vcd_clnt_ctxt *cctxt,
	enum vcd_buffer_type buffer)
{
	struct vcd_property_hdr prop_hdr;
	struct vcd_buffer_pool *buf_pool;
	struct vcd_buffer_requirement buf_req;
	u32 rc;
	u8 i;

	if (buffer == VCD_BUFFER_INPUT) {
		prop_hdr.prop_id = DDL_I_INPUT_BUF_REQ;
		buf_pool = &cctxt->in_buf_pool;
	} else {
		prop_hdr.prop_id = DDL_I_OUTPUT_BUF_REQ;
		buf_pool = &cctxt->out_buf_pool;
	}

	prop_hdr.sz = sizeof(buf_req);
	rc = ddl_get_property(cctxt->ddl_handle, &prop_hdr, &buf_req);
	VCD_FAILED_RETURN(rc, "Failed: ddl_GetProperty");

	buf_pool->buf_req = buf_req;
	if (buf_pool->count < buf_req.actual_count) {
		VCD_MSG_ERROR("Buf requirement count not met");
		return VCD_ERR_FAIL;
	}

	if (buf_pool->count > buf_req.actual_count)
		buf_pool->count = buf_req.actual_count;

	if (!buf_pool->entries ||
	buf_pool->validated != buf_pool->count) {
		VCD_MSG_ERROR("Buffer pool is not completely setup yet");
		return VCD_ERR_BAD_STATE;
	}
	for (i = 1; (rc == VCD_S_SUCCESS && i <= buf_pool->count); i++) {
		if (buf_pool->entries[i].sz <
			buf_pool->buf_req.sz) {
			VCD_MSG_ERROR(
				"BufReq sz not met:\
					addr=(0x%p) sz=%d ReqSize=%d",
				buf_pool->entries[i].virtual,
				buf_pool->entries[i].sz,
				buf_pool->buf_req.sz);
			rc = VCD_ERR_FAIL;
		}
	}
	return rc;
}

u32 vcd_handle_ind_output_reconfig(
	struct vcd_clnt_ctxt *cctxt, void* payload, u32 status)
{
	struct ddl_frame_data_tag *frame =
		(struct ddl_frame_data_tag *)payload;
	struct vcd_property_hdr prop_hdr;
	u32 rc = VCD_S_SUCCESS;
	struct vcd_buffer_pool *out_buf_pool;
	struct vcd_buffer_requirement buf_req;

	if (frame)
		rc = vcd_handle_output_required(cctxt, payload, status);
	VCD_FAILED_RETURN(rc, "Failed: vcd_handle_output_required in reconfig");
	vcd_mark_frame_channel(cctxt->dev_ctxt);

	rc = vcd_sched_suspend_resume_clnt(cctxt, false);
	VCD_FAILED_RETURN(rc, "Failed: vcd_sched_suspend_resume_clnt");
	out_buf_pool = &cctxt->out_buf_pool;
	prop_hdr.prop_id = DDL_I_OUTPUT_BUF_REQ;
	prop_hdr.sz = sizeof(buf_req);
	rc = ddl_get_property(cctxt->ddl_handle, &prop_hdr, &buf_req);
	VCD_FAILED_RETURN(rc, "Failed: ddl_GetProperty");

	out_buf_pool->buf_req = buf_req;

	if (out_buf_pool->count < buf_req.actual_count) {
		VCD_MSG_HIGH("Output buf requirement count increased");
		out_buf_pool->count = buf_req.actual_count;
	}

	if (buf_req.actual_count > VCD_MAX_BUFFER_ENTRIES) {
		VCD_MSG_ERROR("\n New act count exceeds Max count(32)");
		return VCD_ERR_FAIL;
	}

	if (!VCD_FAILED(rc)) {
		rc = vcd_set_frame_size(cctxt, NULL);
		VCD_FAILED_RETURN(rc, "Failed: set_frame_size in reconfig");
		cctxt->status.mask &= ~VCD_FIRST_OP_RCVD;
		cctxt->status.mask |= VCD_IN_RECONFIG;
		cctxt->callback(VCD_EVT_IND_OUTPUT_RECONFIG,
			status, NULL, 0, cctxt,
			cctxt->client_data);
	}
	return rc;
}

u32 vcd_handle_ind_output_reconfig_in_flushing(
	struct vcd_clnt_ctxt *cctxt, void* payload, u32 status)
{
	u32 rc = VCD_S_SUCCESS;
	if (cctxt->status.mask & VCD_FLUSH_INPUT && payload) {
		(void)vcd_handle_input_done(cctxt, payload,
		VCD_EVT_RESP_INPUT_FLUSHED, status);
		payload = NULL;
	}
	rc = vcd_handle_ind_output_reconfig(cctxt, payload, status);
	return rc;
}

u32 vcd_return_op_buffer_to_hw(struct vcd_clnt_ctxt *cctxt,
	struct vcd_buffer_entry *buf_entry)
{
	u32 rc = VCD_S_SUCCESS;
	struct vcd_frame_data *frm_entry = &buf_entry->frame;

	VCD_MSG_LOW("vcd_return_op_buffer_to_hw in %d:",
		    cctxt->clnt_state.state);
	frm_entry->physical = buf_entry->physical;
	frm_entry->ip_frm_tag = VCD_FRAMETAG_INVALID;
	frm_entry->intrlcd_ip_frm_tag = VCD_FRAMETAG_INVALID;
	frm_entry->data_len = 0;

	if (cctxt->decoding) {
		struct vcd_property_hdr Prop_hdr;
		struct ddl_frame_data_tag ddl_frm;
		Prop_hdr.prop_id = DDL_I_DPB_RELEASE;
		Prop_hdr.sz =
			sizeof(struct ddl_frame_data_tag);
		memset(&ddl_frm, 0, sizeof(ddl_frm));
		ddl_frm.vcd_frm = *frm_entry;
		rc = ddl_set_property(cctxt->ddl_handle, &Prop_hdr,
				      &ddl_frm);
		if (VCD_FAILED(rc)) {
			VCD_MSG_ERROR("Error returning output buffer to"
					" HW. rc = 0x%x", rc);
			buf_entry->in_use = false;
		} else {
			cctxt->out_buf_pool.in_use++;
			buf_entry->in_use = true;
		}
	}
	return rc;
}

void vcd_handle_clnt_fatal(struct vcd_clnt_ctxt *cctxt, u32 trans_end)
{
	if (trans_end)
		vcd_mark_frame_channel(cctxt->dev_ctxt);
	vcd_handle_err_fatal(cctxt,
		VCD_EVT_IND_HWERRFATAL, VCD_ERR_CLIENT_FATAL);
}

void vcd_handle_clnt_fatal_input_done(struct vcd_clnt_ctxt *cctxt,
	u32 trans_end)
{
	if (cctxt->status.frame_submitted > 0)
		cctxt->status.frame_submitted--;
	vcd_handle_clnt_fatal(cctxt, trans_end);
}

void vcd_handle_ind_info_output_reconfig(
	struct vcd_clnt_ctxt *cctxt, u32 status)
{
	if (cctxt) {
		cctxt->callback(VCD_EVT_IND_INFO_OUTPUT_RECONFIG, status, NULL,
		 0, cctxt, cctxt->client_data);
	}
}
