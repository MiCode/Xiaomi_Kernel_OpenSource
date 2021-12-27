/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _CAM_ISP_HW_MGR_INTF_H_
#define _CAM_ISP_HW_MGR_INTF_H_

#include <linux/of.h>
#include <linux/time.h>
#include <linux/list.h>
#include <media/cam_isp.h>
#include "cam_hw_mgr_intf.h"

/* MAX IFE instance */
#define CAM_IFE_HW_NUM_MAX   7
#define CAM_IFE_RDI_NUM_MAX  4
#define CAM_ISP_BW_CONFIG_V1 1
#define CAM_ISP_BW_CONFIG_V2 2

/* Appliacble vote paths for dual ife, based on no. of UAPI definitions */
#define CAM_ISP_MAX_PER_PATH_VOTES 30
/**
 *  enum cam_isp_hw_event_type - Collection of the ISP hardware events
 */
enum cam_isp_hw_event_type {
	CAM_ISP_HW_EVENT_ERROR,
	CAM_ISP_HW_EVENT_SOF,
	CAM_ISP_HW_EVENT_REG_UPDATE,
	CAM_ISP_HW_EVENT_EPOCH,
	CAM_ISP_HW_EVENT_EOF,
	CAM_ISP_HW_EVENT_DONE,
	CAM_ISP_HW_EVENT_MAX
};


/**
 * enum cam_isp_hw_err_type - Collection of the ISP error types for
 *                         ISP hardware event CAM_ISP_HW_EVENT_ERROR
 */
enum cam_isp_hw_err_type {
	CAM_ISP_HW_ERROR_NONE,
	CAM_ISP_HW_ERROR_OVERFLOW,
	CAM_ISP_HW_ERROR_P2I_ERROR,
	CAM_ISP_HW_ERROR_VIOLATION,
	CAM_ISP_HW_ERROR_BUSIF_OVERFLOW,
	CAM_ISP_HW_ERROR_MAX,
};

/**
 *  enum cam_isp_hw_stop_cmd - Specify the stop command type
 */
enum cam_isp_hw_stop_cmd {
	CAM_ISP_HW_STOP_AT_FRAME_BOUNDARY,
	CAM_ISP_HW_STOP_IMMEDIATELY,
	CAM_ISP_HW_STOP_MAX,
};

/**
 * struct cam_isp_stop_args - hardware stop arguments
 *
 * @hw_stop_cmd:               Hardware stop command type information
 * @stop_only                  Send stop only to hw drivers. No Deinit to be
 *                             done.
 *
 */
struct cam_isp_stop_args {
	enum cam_isp_hw_stop_cmd      hw_stop_cmd;
	bool                          stop_only;
};

/**
 * struct cam_isp_start_args - isp hardware start arguments
 *
 * @config_args:               Hardware configuration commands.
 * @start_only                 Send start only to hw drivers. No init to
 *                             be done.
 *
 */
struct cam_isp_start_args {
	struct cam_hw_config_args     hw_config;
	bool                          start_only;
};

/**
 * struct cam_isp_bw_config_internal_v2 - Bandwidth configuration
 *
 * @usage_type:                 ife hw index
 * @num_paths:                  Number of data paths
 * @axi_path                    per path vote info
 */
struct cam_isp_bw_config_internal_v2 {
	uint32_t                          usage_type;
	uint32_t                          num_paths;
	struct cam_axi_per_path_bw_vote   axi_path[CAM_ISP_MAX_PER_PATH_VOTES];
};

/**
 * struct cam_isp_bw_config_internal - Internal Bandwidth configuration
 *
 * @usage_type:                 Usage type (Single/Dual)
 * @num_rdi:                    Number of RDI votes
 * @left_pix_vote:              Bandwidth vote for left ISP
 * @right_pix_vote:             Bandwidth vote for right ISP
 * @rdi_vote:                   RDI bandwidth requirements
 */
struct cam_isp_bw_config_internal {
	uint32_t                       usage_type;
	uint32_t                       num_rdi;
	struct cam_isp_bw_vote         left_pix_vote;
	struct cam_isp_bw_vote         right_pix_vote;
	struct cam_isp_bw_vote         rdi_vote[CAM_IFE_RDI_NUM_MAX];
};

/**
 * struct cam_isp_prepare_hw_update_data - hw prepare data
 *
 * @ife_mgr_ctx:            IFE HW manager Context for current request
 * @packet_opcode_type:     Packet header opcode in the packet header
 *                          this opcode defines, packet is init packet or
 *                          update packet
 * @bw_config_version:      BW config version indicator
 * @bw_config:              BW config information
 * @bw_config_v2:           BW config info for AXI bw voting v2
 * @bw_config_valid:        Flag indicating whether the bw_config at the index
 *                          is valid or not
 * @reg_dump_buf_desc:     cmd buffer descriptors for reg dump
 * @num_reg_dump_buf:      Count of descriptors in reg_dump_buf_desc
 *
 */
struct cam_isp_prepare_hw_update_data {
	struct cam_ife_hw_mgr_ctx            *ife_mgr_ctx;
	uint32_t                              packet_opcode_type;
	uint32_t                              bw_config_version;
	struct cam_isp_bw_config_internal     bw_config[CAM_IFE_HW_NUM_MAX];
	struct cam_isp_bw_config_internal_v2  bw_config_v2[CAM_IFE_HW_NUM_MAX];
	bool                                bw_config_valid[CAM_IFE_HW_NUM_MAX];
	struct cam_cmd_buf_desc               reg_dump_buf_desc[
						CAM_REG_DUMP_MAX_BUF_ENTRIES];
	uint32_t                              num_reg_dump_buf;
};


/**
 * struct cam_isp_hw_sof_event_data - Event payload for CAM_HW_EVENT_SOF
 *
 * @timestamp:   Time stamp for the sof event
 * @boot_time:   Boot time stamp for the sof event
 *
 */
struct cam_isp_hw_sof_event_data {
	uint64_t       timestamp;
	uint64_t       boot_time;
};

/**
 * struct cam_isp_hw_reg_update_event_data - Event payload for
 *                         CAM_HW_EVENT_REG_UPDATE
 *
 * @timestamp:     Time stamp for the reg update event
 *
 */
struct cam_isp_hw_reg_update_event_data {
	uint64_t       timestamp;
};

/**
 * struct cam_isp_hw_epoch_event_data - Event payload for CAM_HW_EVENT_EPOCH
 *
 * @timestamp:     Time stamp for the epoch event
 *
 */
struct cam_isp_hw_epoch_event_data {
	uint64_t       timestamp;
};

/**
 * struct cam_isp_hw_done_event_data - Event payload for CAM_HW_EVENT_DONE
 *
 * @num_handles:           Number of resource handeles
 * @resource_handle:       Resource handle array
 * @timestamp:             Timestamp for the buf done event
 *
 */
struct cam_isp_hw_done_event_data {
	uint32_t             num_handles;
	uint32_t             resource_handle[
				CAM_NUM_OUT_PER_COMP_IRQ_MAX];
	uint64_t       timestamp;
};

/**
 * struct cam_isp_hw_eof_event_data - Event payload for CAM_HW_EVENT_EOF
 *
 * @timestamp:             Timestamp for the eof event
 *
 */
struct cam_isp_hw_eof_event_data {
	uint64_t       timestamp;
};

/**
 * struct cam_isp_hw_error_event_data - Event payload for CAM_HW_EVENT_ERROR
 *
 * @error_type:            Error type for the error event
 * @timestamp:             Timestamp for the error event
 * @recovery_enabled:      Identifies if the context needs to recover & reapply
 *                         this request
 * @enable_req_dump:       Enable request dump on HW errors
 */
struct cam_isp_hw_error_event_data {
	uint32_t             error_type;
	uint64_t             timestamp;
	bool                 recovery_enabled;
	bool                 enable_req_dump;
};

/* enum cam_isp_hw_mgr_command - Hardware manager command type */
enum cam_isp_hw_mgr_command {
	CAM_ISP_HW_MGR_CMD_IS_RDI_ONLY_CONTEXT,
	CAM_ISP_HW_MGR_CMD_PAUSE_HW,
	CAM_ISP_HW_MGR_CMD_RESUME_HW,
	CAM_ISP_HW_MGR_CMD_SOF_DEBUG,
	CAM_ISP_HW_MGR_CMD_CTX_TYPE,
	CAM_ISP_HW_MGR_CMD_MAX,
};

enum cam_isp_ctx_type {
	CAM_ISP_CTX_FS2 = 1,
	CAM_ISP_CTX_RDI,
	CAM_ISP_CTX_PIX,
	CAM_ISP_CTX_MAX,
};
/**
 * struct cam_isp_hw_cmd_args - Payload for hw manager command
 *
 * @cmd_type               HW command type
 * @sof_irq_enable         To debug if SOF irq is enabled
 * @ctx_type               RDI_ONLY, PIX and RDI, or FS2
 */
struct cam_isp_hw_cmd_args {
	uint32_t                          cmd_type;
	union {
		uint32_t                      sof_irq_enable;
		uint32_t                      ctx_type;
	} u;
};


/**
 * cam_isp_hw_mgr_init()
 *
 * @brief:              Initialization function for the ISP hardware manager
 *
 * @of_node:            Device node input
 * @hw_mgr:             Input/output structure for the ISP hardware manager
 *                          initialization
 * @iommu_hdl:          Iommu handle to be returned
 */
int cam_isp_hw_mgr_init(struct device_node *of_node,
	struct cam_hw_mgr_intf *hw_mgr, int *iommu_hdl);

#endif /* __CAM_ISP_HW_MGR_INTF_H__ */
