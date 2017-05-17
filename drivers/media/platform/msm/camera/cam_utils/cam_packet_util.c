/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__

#include "cam_mem_mgr.h"
#include "cam_packet_util.h"

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

int cam_packet_util_process_patches(struct cam_packet *packet,
	int32_t iommu_hdl)
{
	struct cam_patch_desc *patch_desc = NULL;
	uint64_t   iova_addr;
	uint64_t   cpu_addr;
	uint32_t   temp;
	uint32_t  *dst_cpu_addr;
	uint32_t  *src_buf_iova_addr;
	size_t     dst_buf_len;
	size_t     src_buf_size;
	int        i;
	int        rc = 0;

	/* process patch descriptor */
	patch_desc = (struct cam_patch_desc *)
			((uint32_t *) &packet->payload +
			packet->patch_offset/4);
	CDBG("packet = %pK patch_desc = %pK size = %lu\n",
			(void *)packet, (void *)patch_desc,
			sizeof(struct cam_patch_desc));

	for (i = 0; i < packet->num_patches; i++) {
		rc = cam_mem_get_io_buf(patch_desc[i].src_buf_hdl,
			iommu_hdl, &iova_addr, &src_buf_size);
		if (rc < 0) {
			pr_err("unable to get src buf address\n");
			return rc;
		}
		src_buf_iova_addr = (uint32_t *)iova_addr;
		temp = iova_addr;

		rc = cam_mem_get_cpu_buf(patch_desc[i].dst_buf_hdl,
			&cpu_addr, &dst_buf_len);
		if (rc < 0) {
			pr_err("unable to get dst buf address\n");
			return rc;
		}
		dst_cpu_addr = (uint32_t *)cpu_addr;

		CDBG("i = %d patch info = %x %x %x %x\n", i,
			patch_desc[i].dst_buf_hdl, patch_desc[i].dst_offset,
			patch_desc[i].src_buf_hdl, patch_desc[i].src_offset);

		dst_cpu_addr = (uint32_t *)((uint8_t *)dst_cpu_addr +
			patch_desc[i].dst_offset);
		temp += patch_desc[i].src_offset;

		*dst_cpu_addr = temp;

		CDBG("patch is done for dst %pK with src %pK value %llx\n",
			dst_cpu_addr, src_buf_iova_addr,
			*((uint64_t *)dst_cpu_addr));
	}

	return rc;
}

