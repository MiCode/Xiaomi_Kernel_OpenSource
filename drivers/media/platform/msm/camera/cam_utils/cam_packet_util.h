/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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

#ifndef _CAM_PACKET_UTIL_H_
#define _CAM_PACKET_UTIL_H_

#include <uapi/media/cam_defs.h>

/**
 * @brief                  KMD scratch buffer information
 *
 * @handle:                Memory handle
 * @cpu_addr:              Cpu address
 * @offset:                Offset from the start of the buffer
 * @size:                  Size of the buffer
 * @used_bytes:            Used memory in bytes
 *
 */
struct cam_kmd_buf_info {
	int        handle;
	uint32_t  *cpu_addr;
	uint32_t   offset;
	uint32_t   size;
	uint32_t   used_bytes;
};

/* Generic Cmd Buffer blob callback function type */
typedef int (*cam_packet_generic_blob_handler)(void *user_data,
	uint32_t blob_type, uint32_t blob_size, uint8_t *blob_data);

/* set resource bitmap callback function type */
typedef void (*cam_fill_res_bitmap)(uint32_t res_type, unsigned long *bitmap);

/**
 * cam_packet_util_get_cmd_mem_addr()
 *
 * @brief                  Get command buffer address
 *
 * @handle:                Command buffer memory handle
 * @buf_addr:              Command buffer cpu mapped address
 * @len:                   Command buffer length
 *
 * @return:                0 for success
 *                         -EINVAL for Fail
 */
int cam_packet_util_get_cmd_mem_addr(int handle, uint32_t **buf_addr,
	size_t *len);

/**
 * cam_packet_util_validate_packet()
 *
 * @brief                  Validate the packet
 *
 * @packet:                Packet to be validated
 *
 * @remain_len:            CPU buff length after config offset
 *
 * @return:                0 for success
 *                         -EINVAL for Fail
 */
int cam_packet_util_validate_packet(struct cam_packet *packet,
	size_t remain_len);

/**
 * cam_packet_util_validate_cmd_desc()
 *
 * @brief                  Validate the packet
 *
 * @cmd_desc:              Command descriptor to be validated
 *
 * @return:                0 for success
 *                         -EINVAL for Fail
 */
int cam_packet_util_validate_cmd_desc(struct cam_cmd_buf_desc *cmd_desc);

/**
 * cam_packet_util_get_kmd_buffer()
 *
 * @brief                  Get the kmd buffer from the packet command descriptor
 *
 * @packet:                Packet data
 * @kmd_buf:               Extracted the KMD buffer information
 *
 * @return:                0 for success
 *                         -EINVAL for Fail
 */
int cam_packet_util_get_kmd_buffer(struct cam_packet *packet,
	struct cam_kmd_buf_info *kmd_buf_info);

/**
 * cam_packet_util_process_patches()
 *
 * @brief:              Replace the handle in Packet to Address using the
 *                      information from patches.
 *
 * @packet:             Input packet containing Command Buffers and Patches
 * @iommu_hdl:          IOMMU handle of the HW Device that received the packet
 * @sec_iommu_hdl:      Secure IOMMU handle of the HW Device that
 *                      received the packet
 * @pf_dump_flag:       if set, it will dump the info,
 *                      otherwise will do patching
 *
 * @return:             0: Success
 *                      Negative: Failure
 */
int cam_packet_util_process_patches(struct cam_packet *packet,
	int32_t iommu_hdl, int32_t sec_mmu_hdl, int pf_dump_flag);

/**
 * cam_packet_util_process_generic_cmd_buffer()
 *
 * @brief:              Process Generic Blob command buffer. This utility
 *                      function process the command buffer and calls the
 *                      blob_handle_cb callback for each blob that exists
 *                      in the command buffer.
 *
 * @cmd_buf:            Generic Blob Cmd Buffer handle
 * @blob_handler_cb:    Callback pointer to call for each blob exists in the
 *                      command buffer
 * @user_data:          User data to be passed while callback
 *
 * @return:             0: Success
 *                      Negative: Failure
 */
int cam_packet_util_process_generic_cmd_buffer(
	struct cam_cmd_buf_desc *cmd_buf,
	cam_packet_generic_blob_handler blob_handler_cb, void *user_data);

#endif /* _CAM_PACKET_UTIL_H_ */
