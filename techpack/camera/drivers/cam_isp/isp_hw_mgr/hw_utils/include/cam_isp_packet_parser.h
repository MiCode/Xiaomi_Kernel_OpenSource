/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_ISP_HW_PARSER_H_
#define _CAM_ISP_HW_PARSER_H_

#include <linux/types.h>
#include <media/cam_isp.h>
#include "cam_isp_hw_mgr_intf.h"
#include "cam_isp_hw_mgr.h"
#include "cam_hw_intf.h"
#include "cam_packet_util.h"
#include "cam_cdm_intf_api.h"

/* enum cam_isp_cdm_bl_type - isp cdm packet type*/
enum cam_isp_cdm_bl_type {
	CAM_ISP_UNUSED_BL,
	CAM_ISP_IQ_BL,
	CAM_ISP_IOCFG_BL,
	CAM_ISP_BL_MAX,
};

/*
 * struct cam_isp_generic_blob_info
 *
 * @prepare:            Payload for prepare command
 * @base_info:          Base hardware information for the context
 * @kmd_buf_info:       Kmd buffer to store the custom cmd data
 */
struct cam_isp_generic_blob_info {
	struct cam_hw_prepare_update_args     *prepare;
	struct cam_isp_ctx_base_info          *base_info;
	struct cam_kmd_buf_info               *kmd_buf_info;
};

/*
 * struct cam_isp_frame_header_info
 *
 * @frame_header_enable:    Enable frame header
 * @frame_header_iova_addr: frame header iova
 * @frame_header_res_id:    res id for which frame header is enabled
 */
struct cam_isp_frame_header_info {
	bool                     frame_header_enable;
	uint64_t                 frame_header_iova_addr;
	uint32_t                 frame_header_res_id;
};

/*
 * struct cam_isp_change_base_args
 *
 * @cdm_id:       CDM id
 * @base_idx:     Base index
 */
struct cam_isp_change_base_args {
	enum cam_cdm_id  cdm_id;
	uint32_t         base_idx;
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
 * @change_base_args:      Arguments for Change base function
 * @kmd_buf_info:          Kmd buffer to store the change base command
 * @return:                0 for success
 *                         -EINVAL for Fail
 */
int cam_isp_add_change_base(
	struct cam_hw_prepare_update_args     *prepare,
	struct list_head                      *res_list_isp_src,
	struct cam_isp_change_base_args       *change_base_args,
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
	struct cam_isp_hw_mgr_res            *hw_mgr_res,
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
	struct cam_isp_ctx_base_info       *base_info,
	cam_packet_generic_blob_handler     blob_handler_cb,
	struct cam_isp_hw_mgr_res          *res_list_isp_out,
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
 * @res_list_ife_in_rd:    IFE /VFE in rd resource list
 * @size_isp_out:          Size of the res_list_isp_out array
 * @fill_fence:            If true, Fence map table will be filled
 * @frame_header_info:     Frame header related params
 * @return:                0 for success
 *                         -EINVAL for Fail
 */
int cam_isp_add_io_buffers(
	int                                   iommu_hdl,
	int                                   sec_iommu_hdl,
	struct cam_hw_prepare_update_args    *prepare,
	uint32_t                              base_idx,
	struct cam_kmd_buf_info              *kmd_buf_info,
	struct cam_isp_hw_mgr_res            *res_list_isp_out,
	struct list_head                     *res_list_ife_in_rd,
	uint32_t                              size_isp_out,
	bool                                  fill_fence,
	struct cam_isp_frame_header_info     *frame_header_info);

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

/*
 * cam_isp_add_comp_wait()
 *
 * @brief                  Add reg update in the hw entries list
 *                         processe the isp source list get the reg update from
 *                         ISP HW instance
 *
 * @prepare:               Contain the  packet and HW update variables
 * @res_list_isp_src:      Resource list for IFE/VFE source
 * @base_idx:              Base or dev index of the IFE/VFE HW instance
 * @kmd_buf_info:          Kmd buffer to store the change base command
 *
 * @return:                0 for success
 *                         -EINVAL for Fail
 */
int cam_isp_add_comp_wait(
	struct cam_hw_prepare_update_args    *prepare,
	struct list_head                     *res_list_isp_src,
	uint32_t                              base_idx,
	struct cam_kmd_buf_info              *kmd_buf_info);

/*
 * cam_isp_add_wait_trigger()
 *
 * @brief                  Add reg update in the hw entries list
 *                         processe the isp source list get the reg update from
 *                         ISP HW instance
 *
 * @prepare:               Contain the  packet and HW update variables
 * @res_list_isp_src:      Resource list for IFE/VFE source
 * @base_idx:              Base or dev index of the IFE/VFE HW instance
 * @kmd_buf_info:          Kmd buffer to store the change base command
 * @trigger_cdm_en         Used to reset and set trigger_cdm_events register
 *
 * @return:                0 for success
 *                         -EINVAL for Fail
 */
int cam_isp_add_wait_trigger(
	struct cam_hw_prepare_update_args    *prepare,
	struct list_head                     *res_list_isp_src,
	uint32_t                              base_idx,
	struct cam_kmd_buf_info              *kmd_buf_info,
	bool                                  trigger_cdm_en);


/*
 * cam_isp_add_go_cmd()
 *
 * @brief                  Add go_cmd in the hw entries list for each rd source
 *
 * @prepare:               Contain the  packet and HW update variables
 * @res_list_isp_rd:       Resource list for BUS RD ports
 * @base_idx:              Base or dev index of the IFE/VFE HW instance
 * @kmd_buf_info:          Kmd buffer to store the change base command
 * @return:                0 for success
 *                         -EINVAL for Fail
 */
int cam_isp_add_go_cmd(
	struct cam_hw_prepare_update_args    *prepare,
	struct list_head                     *res_list_isp_rd,
	uint32_t                              base_idx,
	struct cam_kmd_buf_info              *kmd_buf_info);

#endif /*_CAM_ISP_HW_PARSER_H */
