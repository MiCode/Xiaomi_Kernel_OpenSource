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

/*
 * struct cam_isp_generic_blob_info
 *
 * @prepare:            Payload for prepare command
 * @ctx_base_info:      Base hardware information for the context
 * @kmd_buf_info:       Kmd buffer to store the custom cmd data
 */
struct cam_isp_generic_blob_info {
	struct cam_hw_prepare_update_args     *prepare;
	struct ctx_base_info                  *base_info;
	struct cam_kmd_buf_info               *kmd_buf_info;
};

/*
 * cam_isp_add_change_base()
 *
 * @brief                  Add change base in the hw entries list
 *                         processe the isp source list get the change base from
 *                         ISP HW instance
 *
 * @prepare:               Contain the packet and HW update variables
 * @res_list_isp_src:      Resource list for IFE/VFE source
 * @base_idx:              Base or dev index of the IFE/VFE HW instance for
 *                         which change change base need to be added
 * @kmd_buf_info:          Kmd buffer to store the change base command
 *
 * @return:                0 for success
 *                         -EINVAL for Fail
 */
int cam_isp_add_change_base(
	struct cam_hw_prepare_update_args     *prepare,
	struct list_head                      *res_list_isp_src,
	uint32_t                               base_idx,
	struct cam_kmd_buf_info               *kmd_buf_info);

/*
 * cam_isp_add_cmd_buf_update()
 *
 * @brief                  Add command buffer in the HW entries list for given
 *                         Blob Data.
 *
 * @hw_mgr_res:            HW resource to get the update from
 * @cmd_type:              Cmd type to get update for
 * @hw_cmd_type:           HW Cmd type corresponding to cmd_type
 * @base_idx:              Base hardware index
 * @cmd_buf_addr:          Cpu buf addr of kmd scratch buffer
 * @kmd_buf_remain_size:   Remaining size left for cmd buffer update
 * @cmd_update_data:       Data needed by HW to process the cmd and provide
 *                         cmd buffer
 * @bytes_used:            Address of the field to be populated with
 *                         total bytes used as output to caller
 *
 * @return:                Negative for Failure
 *                         otherwise returns bytes used
 */
int cam_isp_add_cmd_buf_update(
	struct cam_ife_hw_mgr_res            *hw_mgr_res,
	uint32_t                              cmd_type,
	uint32_t                              hw_cmd_type,
	uint32_t                              base_idx,
	uint32_t                             *cmd_buf_addr,
	uint32_t                              kmd_buf_remain_size,
	void                                 *cmd_update_data,
	uint32_t                             *bytes_used);

/*
 * cam_isp_add_command_buffers()
 *
 * @brief                  Add command buffer in the HW entries list for given
 *                         left or right VFE/IFE instance.
 *
 * @prepare:               Contain the packet and HW update variables
 * @kmd_buf_info:          KMD buffer to store the custom cmd data
 * @base_info:             base hardware information
 * @blob_handler_cb:       Call_back_function for Meta handling
 * @res_list_isp_out:      IFE /VFE out resource list
 * @size_isp_out:          Size of the res_list_isp_out array
 *
 * @return:                0 for success
 *                         Negative for Failure
 */
int cam_isp_add_command_buffers(
	struct cam_hw_prepare_update_args  *prepare,
	struct cam_kmd_buf_info            *kmd_buf_info,
	struct ctx_base_info               *base_info,
	cam_packet_generic_blob_handler     blob_handler_cb,
	struct cam_ife_hw_mgr_res          *res_list_isp_out,
	uint32_t                            size_isp_out);

/*
 * cam_isp_add_io_buffers()
 *
 * @brief                  Add io buffer configurations in the HW entries list
 *                         processe the io configurations based on the base
 *                         index and update the HW entries list
 *
 * @iommu_hdl:             Iommu handle to get the IO buf from memory manager
 * @sec_iommu_hdl:         Secure iommu handle to get the IO buf from
 *                         memory manager
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
int cam_isp_add_io_buffers(
	int                                   iommu_hdl,
	int                                   sec_iommu_hdl,
	struct cam_hw_prepare_update_args    *prepare,
	uint32_t                              base_idx,
	struct cam_kmd_buf_info              *kmd_buf_info,
	struct cam_ife_hw_mgr_res            *res_list_isp_out,
	uint32_t                              size_isp_out,
	bool                                  fill_fence);

/*
 * cam_isp_add_reg_update()
 *
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
