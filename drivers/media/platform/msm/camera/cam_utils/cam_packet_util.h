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

#ifndef _CAM_PACKET_UTIL_H_
#define _CAM_PACKET_UTIL_H_

#include <uapi/media/cam_defs.h>

/**
 * cam_packet_util_process_patches()
 *
 * @brief:              Replace the handle in Packet to Address using the
 *                      information from patches.
 *
 * @packet:             Input packet containing Command Buffers and Patches
 * @iommu_hdl:          IOMMU handle of the HW Device that received the packet
 *
 * @return:             0: Success
 *                      Negative: Failure
 */
int cam_packet_util_process_patches(struct cam_packet *packet,
	int32_t iommu_hdl);

#endif /* _CAM_PACKET_UTIL_H_ */
