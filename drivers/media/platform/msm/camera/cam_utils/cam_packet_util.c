/* Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/types.h>
#include <linux/slab.h>

#include "cam_mem_mgr.h"
#include "cam_packet_util.h"
#include "cam_debug_util.h"

int cam_packet_util_get_cmd_mem_addr(int handle, uint32_t **buf_addr,
	size_t *len)
{
	int rc = 0;
	uintptr_t kmd_buf_addr = 0;

	rc = cam_mem_get_cpu_buf(handle, &kmd_buf_addr, len);
	if (rc) {
		CAM_ERR(CAM_UTIL, "Unable to get the virtual address %d", rc);
	} else {
		if (kmd_buf_addr && *len) {
			*buf_addr = (uint32_t *)kmd_buf_addr;
		} else {
			CAM_ERR(CAM_UTIL, "Invalid addr and length :%zd", *len);
			rc = -ENOMEM;
		}
	}
	return rc;
}

int cam_packet_util_validate_cmd_desc(struct cam_cmd_buf_desc *cmd_desc)
{
	if ((cmd_desc->length > cmd_desc->size) ||
		(cmd_desc->mem_handle <= 0)) {
		CAM_ERR(CAM_UTIL, "invalid cmd arg %d %d %d %d",
			cmd_desc->offset, cmd_desc->length,
			cmd_desc->mem_handle, cmd_desc->size);
		return -EINVAL;
	}

	return 0;
}

int cam_packet_util_validate_packet(struct cam_packet *packet,
	size_t remain_len)
{
	size_t sum_cmd_desc = 0;
	size_t sum_io_cfgs = 0;
	size_t sum_patch_desc = 0;
	size_t pkt_wo_payload = 0;

	if (!packet)
		return -EINVAL;

	if ((size_t)packet->header.size > remain_len) {
		CAM_ERR(CAM_UTIL,
			"Invalid packet size: %zu, CPU buf length: %zu",
			(size_t)packet->header.size, remain_len);
		return -EINVAL;
	}


	CAM_DBG(CAM_UTIL, "num cmd buf:%d num of io config:%d kmd buf index:%d",
		packet->num_cmd_buf, packet->num_io_configs,
		packet->kmd_cmd_buf_index);

	sum_cmd_desc = packet->num_cmd_buf * sizeof(struct cam_cmd_buf_desc);
	sum_io_cfgs = packet->num_io_configs * sizeof(struct cam_buf_io_cfg);
	sum_patch_desc = packet->num_patches * sizeof(struct cam_patch_desc);
	pkt_wo_payload = offsetof(struct cam_packet, payload);

	if ((!packet->header.size) ||
		((pkt_wo_payload + (size_t)packet->cmd_buf_offset +
		sum_cmd_desc) > (size_t)packet->header.size) ||
		((pkt_wo_payload + (size_t)packet->io_configs_offset +
		sum_io_cfgs) > (size_t)packet->header.size) ||
		((pkt_wo_payload + (size_t)packet->patch_offset +
		sum_patch_desc) > (size_t)packet->header.size)) {
		CAM_ERR(CAM_UTIL, "params not within mem len:%zu %zu %zu %zu",
			(size_t)packet->header.size, sum_cmd_desc,
			sum_io_cfgs, sum_patch_desc);
		return -EINVAL;
	}

	return 0;
}

int cam_packet_util_get_kmd_buffer(struct cam_packet *packet,
	struct cam_kmd_buf_info *kmd_buf)
{
	int                      rc = 0;
	size_t                   len = 0;
	size_t                   remain_len = 0;
	struct cam_cmd_buf_desc *cmd_desc;
	uint32_t                *cpu_addr;

	if (!packet || !kmd_buf) {
		CAM_ERR(CAM_UTIL, "Invalid arg %pK %pK", packet, kmd_buf);
		return -EINVAL;
	}

	if ((packet->kmd_cmd_buf_index < 0) ||
		(packet->kmd_cmd_buf_index >= packet->num_cmd_buf)) {
		CAM_ERR(CAM_UTIL, "Invalid kmd buf index: %d",
			packet->kmd_cmd_buf_index);
		return -EINVAL;
	}

	/* Take first command descriptor and add offset to it for kmd*/
	cmd_desc = (struct cam_cmd_buf_desc *) ((uint8_t *)
		&packet->payload + packet->cmd_buf_offset);
	cmd_desc += packet->kmd_cmd_buf_index;

	rc = cam_packet_util_validate_cmd_desc(cmd_desc);
	if (rc)
		return rc;

	rc = cam_packet_util_get_cmd_mem_addr(cmd_desc->mem_handle, &cpu_addr,
		&len);
	if (rc)
		return rc;

	remain_len = len;
	if (((size_t)cmd_desc->offset >= len) ||
		((size_t)cmd_desc->size > (len - (size_t)cmd_desc->offset))) {
		CAM_ERR(CAM_UTIL, "invalid memory len:%zd and cmd desc size:%d",
			len, cmd_desc->size);
		rc = -EINVAL;
		goto rel_kmd_buf;
	}

	remain_len -= (size_t)cmd_desc->offset;
	if ((size_t)packet->kmd_cmd_buf_offset >= remain_len) {
		CAM_ERR(CAM_UTIL, "Invalid kmd cmd buf offset: %zu",
			(size_t)packet->kmd_cmd_buf_offset);
		rc = -EINVAL;
		goto rel_kmd_buf;
	}

	cpu_addr += (cmd_desc->offset / 4) + (packet->kmd_cmd_buf_offset / 4);
	CAM_DBG(CAM_UTIL, "total size %d, cmd size: %d, KMD buffer size: %d",
		cmd_desc->size, cmd_desc->length,
		cmd_desc->size - cmd_desc->length);
	CAM_DBG(CAM_UTIL, "hdl 0x%x, cmd offset %d, kmd offset %d, addr 0x%pK",
		cmd_desc->mem_handle, cmd_desc->offset,
		packet->kmd_cmd_buf_offset, cpu_addr);

	kmd_buf->cpu_addr   = cpu_addr;
	kmd_buf->handle     = cmd_desc->mem_handle;
	kmd_buf->offset     = cmd_desc->offset + packet->kmd_cmd_buf_offset;
	kmd_buf->size       = cmd_desc->size - cmd_desc->length;
	kmd_buf->used_bytes = 0;

rel_kmd_buf:
	if (cam_mem_put_cpu_buf(cmd_desc->mem_handle))
		CAM_WARN(CAM_UTIL, "Put KMD Buf failed for: 0x%x",
			cmd_desc->mem_handle);

	return rc;
}

int cam_packet_util_process_patches(struct cam_packet *packet,
	int32_t iommu_hdl, int32_t sec_mmu_hdl, int pf_dump_flag)
{
	struct cam_patch_desc *patch_desc = NULL;
	dma_addr_t iova_addr;
	uintptr_t  cpu_addr = 0;
	uint32_t   temp;
	uint32_t  *dst_cpu_addr;
	uint32_t  *src_buf_iova_addr;
	size_t     dst_buf_len = 0;
	size_t     src_buf_size;
	int        i;
	int        rc = 0;
	int32_t    hdl;
	uint64_t   requestId;
	uint32_t   num_patches;

	/* process patch descriptor */
	patch_desc = (struct cam_patch_desc *)
			((uint32_t *) &packet->payload +
			packet->patch_offset/4);
	requestId = packet->header.request_id;
	num_patches = packet->num_patches;
	CAM_DBG(CAM_UTIL, "packet = %pK patch_desc = %pK size = %lu",
			(void *)packet, (void *)patch_desc,
			sizeof(struct cam_patch_desc));

	for (i = 0; i < packet->num_patches; i++) {
		hdl = cam_mem_is_secure_buf(patch_desc[i].src_buf_hdl) ?
			sec_mmu_hdl : iommu_hdl;
		rc = cam_mem_get_io_buf(patch_desc[i].src_buf_hdl,
			hdl, &iova_addr, &src_buf_size);
		if (rc < 0) {
			CAM_ERR(CAM_UTIL,
				"unable to get src buf address ReqId: %llu, num_patches = %d",
				requestId, num_patches);
			CAM_ERR(CAM_UTIL,
				"i = %d patch info = %x %x %x %x src_bfsz:0x%x",
				i, patch_desc[i].dst_buf_hdl,
				patch_desc[i].dst_offset,
				patch_desc[i].src_buf_hdl,
				patch_desc[i].src_offset,
				(uint32_t)src_buf_size);
			return rc;
		}
		src_buf_iova_addr = (uint32_t *)iova_addr;
		temp = iova_addr;

		rc = cam_mem_get_cpu_buf(patch_desc[i].dst_buf_hdl,
			&cpu_addr, &dst_buf_len);
		if (rc < 0 || !cpu_addr || (dst_buf_len == 0)) {
			CAM_ERR(CAM_UTIL,
				"unable to get dst buf address ReqId: %llu, num_patches = %d",
				requestId, num_patches);
			CAM_ERR(CAM_UTIL,
				"i = %d patch info = %x %x %x %x dst_bfsz:0x%x",
				i, patch_desc[i].dst_buf_hdl,
				patch_desc[i].dst_offset,
				patch_desc[i].src_buf_hdl,
				patch_desc[i].src_offset,
				(uint32_t)dst_buf_len);
			return rc;
		}
		dst_cpu_addr = (uint32_t *)cpu_addr;

		CAM_DBG(CAM_UTIL,
			"ReqId: %llu, i = %d patch info = %x %x %x %x",
			requestId, i, patch_desc[i].dst_buf_hdl,
			patch_desc[i].dst_offset,
			patch_desc[i].src_buf_hdl, patch_desc[i].src_offset);

		if ((size_t)patch_desc[i].src_offset >= src_buf_size) {
			CAM_ERR(CAM_UTIL,
				"Invalid src buf patch offset ReqId: %llu, num_patches = %d",
				requestId, num_patches);
			CAM_ERR(CAM_UTIL,
				"i = %d patch info = %x %x %x %x src_bfsz:0x%x",
				i, patch_desc[i].dst_buf_hdl,
				patch_desc[i].dst_offset,
				patch_desc[i].src_buf_hdl,
				patch_desc[i].src_offset,
				(uint32_t)src_buf_size);
			return -EINVAL;
		}

		if ((dst_buf_len < sizeof(void *)) ||
			((dst_buf_len - sizeof(void *)) <
			(size_t)patch_desc[i].dst_offset)) {
			CAM_ERR(CAM_UTIL,
				"Invalid dst buf patch offset ReqId: %llu, num_patches = %d",
				requestId, num_patches);
			CAM_ERR(CAM_UTIL,
				"i = %d patch info = %x %x %x %x dst_bfsz:0x%x",
				i, patch_desc[i].dst_buf_hdl,
				patch_desc[i].dst_offset,
				patch_desc[i].src_buf_hdl,
				patch_desc[i].src_offset,
				(uint32_t)dst_buf_len);
			return -EINVAL;
		}

		dst_cpu_addr = (uint32_t *)((uint8_t *)dst_cpu_addr +
			patch_desc[i].dst_offset);
		temp += patch_desc[i].src_offset;

		*dst_cpu_addr = temp;

		CAM_DBG(CAM_UTIL,
			"patch is done for dst %pK with src %pK value %llx",
			dst_cpu_addr, src_buf_iova_addr,
			*((uint64_t *)dst_cpu_addr));
		if (cam_mem_put_cpu_buf(patch_desc[i].dst_buf_hdl))
			CAM_WARN(CAM_UTIL, "unable to put dst buf address:0x%x",
				patch_desc[i].dst_buf_hdl);

		if (pf_dump_flag) {
			CAM_INFO(CAM_UTIL,
				"patch[%d]: patched addr 0x%x sz 0x%x offset:0x%x",
				i, *((uint64_t *)dst_cpu_addr),
				(uint32_t)src_buf_size,
				patch_desc[i].src_offset);
		}
	}

	return rc;
}

int cam_packet_util_process_generic_cmd_buffer(
	struct cam_cmd_buf_desc *cmd_buf,
	cam_packet_generic_blob_handler blob_handler_cb, void *user_data)
{
	int       rc;
	uintptr_t  cpu_addr = 0;
	size_t    buf_size;
	size_t    remain_len = 0;
	uint32_t *blob_ptr;
	uint32_t  blob_type, blob_size, blob_block_size, len_read;

	if (!cmd_buf || !blob_handler_cb) {
		CAM_ERR(CAM_UTIL, "Invalid args %pK %pK",
			cmd_buf, blob_handler_cb);
		return -EINVAL;
	}

	if (!cmd_buf->length || !cmd_buf->size) {
		CAM_ERR(CAM_UTIL, "Invalid cmd buf size %d %d",
			cmd_buf->length, cmd_buf->size);
		return -EINVAL;
	}

	rc = cam_mem_get_cpu_buf(cmd_buf->mem_handle, &cpu_addr, &buf_size);
	if (rc || !cpu_addr || (buf_size == 0)) {
		CAM_ERR(CAM_UTIL, "Failed in Get cpu addr, rc=%d, cpu_addr=%pK",
			rc, (void *)cpu_addr);
		return rc;
	}

	remain_len = buf_size;
	if ((buf_size < sizeof(uint32_t)) ||
		((size_t)cmd_buf->offset > (buf_size - sizeof(uint32_t)))) {
		CAM_ERR(CAM_UTIL, "Invalid offset for cmd buf: %zu",
			(size_t)cmd_buf->offset);
		goto rel_cmd_buf;
	}
	remain_len -= (size_t)cmd_buf->offset;

	if (remain_len < (size_t)cmd_buf->length) {
		CAM_ERR(CAM_UTIL, "Invalid length for cmd buf: %zu",
			(size_t)cmd_buf->length);
		goto rel_cmd_buf;
	}

	blob_ptr = (uint32_t *)(((uint8_t *)cpu_addr) +
		cmd_buf->offset);

	CAM_DBG(CAM_UTIL,
		"GenericCmdBuffer cpuaddr=%pK, blobptr=%pK, len=%d",
		(void *)cpu_addr, (void *)blob_ptr, cmd_buf->length);

	len_read = 0;
	while (len_read < cmd_buf->length) {
		blob_type =
			((*blob_ptr) & CAM_GENERIC_BLOB_CMDBUFFER_TYPE_MASK) >>
			CAM_GENERIC_BLOB_CMDBUFFER_TYPE_SHIFT;
		blob_size =
			((*blob_ptr) & CAM_GENERIC_BLOB_CMDBUFFER_SIZE_MASK) >>
			CAM_GENERIC_BLOB_CMDBUFFER_SIZE_SHIFT;

		blob_block_size = sizeof(uint32_t) +
			(((blob_size + sizeof(uint32_t) - 1) /
			sizeof(uint32_t)) * sizeof(uint32_t));

		CAM_DBG(CAM_UTIL,
			"Blob type=%d size=%d block_size=%d len_read=%d total=%d",
			blob_type, blob_size, blob_block_size, len_read,
			cmd_buf->length);

		if (len_read + blob_block_size > cmd_buf->length) {
			CAM_ERR(CAM_UTIL, "Invalid Blob %d %d %d %d",
				blob_type, blob_size, len_read,
				cmd_buf->length);
			rc = -EINVAL;
			goto rel_cmd_buf;
		}

		len_read += blob_block_size;

		rc = blob_handler_cb(user_data, blob_type, blob_size,
			(uint8_t *)(blob_ptr + 1));
		if (rc) {
			CAM_ERR(CAM_UTIL, "Error in handling blob type %d %d",
				blob_type, blob_size);
			goto rel_cmd_buf;
		}

		blob_ptr += (blob_block_size / sizeof(uint32_t));
	}

rel_cmd_buf:
	if (cam_mem_put_cpu_buf(cmd_buf->mem_handle))
		CAM_WARN(CAM_UTIL, "unable to put dst buf address: 0x%x",
			cmd_buf->mem_handle);

	return rc;
}

int32_t cam_packet_validate_plane_size(
	struct cam_buf_io_cfg *io_cfg,
	int plane_index,
	size_t size)
{
	int rc = 0;
	uint32_t kmd_plane_size = 0;
	uint32_t plane_stride = 0;
	uint32_t slice_height = 0;
	uint32_t metadata_size = 0;
	uint32_t format = io_cfg->format;
	uint32_t plane_pixel_size = 0;

	if (plane_index < CAM_PACKET_MAX_PLANES) {
		plane_stride = io_cfg->planes[plane_index].plane_stride;
		slice_height = io_cfg->planes[plane_index].slice_height;
	}

	if (!(plane_stride && slice_height)) {
		CAM_ERR(CAM_ISP,
			"Invalid values from UMD stride %d, slice height %d",
			plane_stride,
			slice_height);
		return -EINVAL;
	}

	switch (format) {
	case CAM_FORMAT_MIPI_RAW_6:
	case CAM_FORMAT_MIPI_RAW_8:
		kmd_plane_size = ((plane_stride * slice_height) + 16 - 1)
			/ 16 * 16;
		break;
	case CAM_FORMAT_MIPI_RAW_10:
		if (plane_stride % 4 == 0)
			kmd_plane_size = ((plane_stride * slice_height)
				+ 16 - 1) / 16 * 16;
		break;
	case CAM_FORMAT_MIPI_RAW_12:
		if (plane_stride % 2 == 0)
			kmd_plane_size = ((plane_stride * slice_height)
				+ 16 - 1) / 16 * 16;
		break;
	case CAM_FORMAT_MIPI_RAW_14:
		if (plane_stride % 4 == 0)
			kmd_plane_size = plane_stride * slice_height * 7 / 4;
		break;
	case CAM_FORMAT_PLAIN16_8:
	case CAM_FORMAT_PLAIN16_10:
	case CAM_FORMAT_PLAIN16_12:
	case CAM_FORMAT_PLAIN16_14:
	case CAM_FORMAT_PLAIN16_16:
	case CAM_FORMAT_PLAIN64:
			kmd_plane_size = plane_stride * slice_height;
		break;
	case CAM_FORMAT_NV21:
	case CAM_FORMAT_NV12:
	if (plane_index < CAM_PACKET_MAX_PLANES)
		kmd_plane_size = plane_stride * slice_height;
		break;
	case CAM_FORMAT_PD10:
	if (plane_index < CAM_PACKET_MAX_PLANES)
		kmd_plane_size = plane_stride * slice_height;
	break;
	case CAM_FORMAT_UBWC_NV12:
	case CAM_FORMAT_UBWC_NV12_4R:
	case CAM_FORMAT_UBWC_TP10:
		metadata_size = io_cfg->planes[plane_index].meta_size;
		plane_pixel_size = ((plane_stride * slice_height) +
	       (4096 - 1)) & ~((uint32_t) 4096 - 1);
		kmd_plane_size = metadata_size + plane_pixel_size;
		break;
	case CAM_FORMAT_UBWC_P010:
	case CAM_FORMAT_PLAIN32_20:
	case CAM_FORMAT_TP10:
	case CAM_FORMAT_YUV422:
	case CAM_FORMAT_PD8:
	case CAM_FORMAT_PLAIN128:
	case CAM_FORMAT_ARGB:
	case CAM_FORMAT_ARGB_10:
	case CAM_FORMAT_ARGB_12:
	case CAM_FORMAT_ARGB_14:
	case CAM_FORMAT_MIPI_RAW_16:
	case CAM_FORMAT_MIPI_RAW_20:
	case CAM_FORMAT_QTI_RAW_8:
	case CAM_FORMAT_QTI_RAW_10:
	case CAM_FORMAT_QTI_RAW_12:
	case CAM_FORMAT_QTI_RAW_14:
	case CAM_FORMAT_PLAIN8:
	case CAM_FORMAT_PLAIN8_SWAP:
	case CAM_FORMAT_PLAIN8_10:
	case CAM_FORMAT_PLAIN8_10_SWAP:
		kmd_plane_size = plane_stride * slice_height;
		break;
	default:
		kmd_plane_size = plane_stride * slice_height;
		break;
	}
	if (!kmd_plane_size ||
		kmd_plane_size > (size - io_cfg->offsets[plane_index])) {
		CAM_ERR(CAM_ISP,
			"kmd size: %d umd size: %d width: %d height: %d stride: %d sliceheight: %d ",
			kmd_plane_size,
			size,
			io_cfg->planes[plane_index].width,
			io_cfg->planes[plane_index].height,
			plane_stride,
			slice_height);
		return -EINVAL;
	}
	return rc;
}
