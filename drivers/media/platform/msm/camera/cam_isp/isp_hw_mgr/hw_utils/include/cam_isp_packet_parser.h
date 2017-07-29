/* Copyright (c) 2017 The Linux Foundation. All rights reserved.
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

#ifndef _CAM_ISP_HW_PARSER_H_
#define _CAM_ISP_HW_PARSER_H_

#include <linux/types.h>
#include <uapi/media/cam_isp.h>
#include "cam_isp_hw_mgr_intf.h"
#include "cam_ife_hw_mgr.h"
#include "cam_hw_intf.h"
#include "cam_packet_util.h"

/**
 * @brief                  Add change base in the hw entries list
 *                         processe the isp source list get the change base from
 *                         ISP HW instance
 *
 * @prepare:               Contain the  packet and HW update variables
 * @res_list_isp_src:      Resource list for IFE/VFE source
 * @base_idx:              Base or dev index of the IFE/VFE HW instance for
 *                         which change change base need to be added
 * @kmd_buf_info:          Kmd buffer to store the change base command
 * @return:                0 for success
 *                         -EINVAL for Fail
 */
int cam_isp_add_change_base(
	struct cam_hw_prepare_update_args     *prepare,
	struct list_head                      *res_list_isp_src,
	uint32_t                               base_idx,
	struct cam_kmd_buf_info               *kmd_buf_info);

/**
 * @brief                  Add command buffer in the HW entries list for given
 *                         left or right VFE/IFE instance.
 *
 * @prepare:               Contain the  packet and HW update variables
 * @dual_type:             Left of right command buffers to be extracted
 *
 * @return:                0 for success
 *                         -EINVAL for Fail
 */
int cam_isp_add_command_buffers(
	struct cam_hw_prepare_update_args    *prepare,
	enum cam_isp_hw_split_id              split_id);

/**
 * @brief                  Add io buffer configurations in the HW entries list
 *                         processe the io configurations based on the base
 *                         index and update the HW entries list
 *
 * @iommu_hdl:             Iommu handle to get the IO buf from memory manager
 * @prepare:               Contain the  packet and HW update variables
 * @base_idx:              Base or dev index of the IFE/VFE HW instance
 * @kmd_buf_info:          Kmd buffer to store the change base command
 * @res_list_isp_out:      IFE /VFE out resource list
 * @size_isp_out:          Size of the res_list_isp_out array
 * @fill_fence:            If true, Fence map table will be filled
 *
 * @return:                0 for success
 *                         -EINVAL for Fail
 */
int cam_isp_add_io_buffers(int	 iommu_hdl,
	struct cam_hw_prepare_update_args    *prepare,
	uint32_t                              base_idx,
	struct cam_kmd_buf_info              *kmd_buf_info,
	struct cam_ife_hw_mgr_res            *res_list_isp_out,
	uint32_t                              size_isp_out,
	bool                                  fill_fence);


/**
 * @brief                  Add reg update in the hw entries list
 *                         processe the isp source list get the reg update from
 *                         ISP HW instance
 *
 * @prepare:               Contain the  packet and HW update variables
 * @res_list_isp_src:      Resource list for IFE/VFE source
 * @base_idx:              Base or dev index of the IFE/VFE HW instance
 * @kmd_buf_info:          Kmd buffer to store the change base command
 * @return:                0 for success
 *                         -EINVAL for Fail
 */
int cam_isp_add_reg_update(
	struct cam_hw_prepare_update_args    *prepare,
	struct list_head                     *res_list_isp_src,
	uint32_t                              base_idx,
	struct cam_kmd_buf_info              *kmd_buf_info);


#endif /*_CAM_ISP_HW_PARSER_H */
