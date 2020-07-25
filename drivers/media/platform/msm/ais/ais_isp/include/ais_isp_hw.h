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

#ifndef _AIS_ISP_HW_H_
#define _AIS_ISP_HW_H_

#include <linux/completion.h>
#include <uapi/media/cam_isp.h>
#include "cam_hw.h"
#include "cam_hw_intf.h"
#include "cam_soc_util.h"

/* cam_control handle_type for IFE device */
#define AIS_ISP_CMD_TYPE 0xA151FE00

/* Maximum length of tag while dumping */
#define AIS_ISP_HW_DUMP_TAG_MAX_LEN 32

/*
 * struct ais_irq_register_set:
 * @Brief:                  Structure containing offsets of IRQ related
 *                          registers belonging to a Set
 *
 * @mask_reg_offset:        Offset of IRQ MASK register
 * @clear_reg_offset:       Offset of IRQ CLEAR register
 * @status_reg_offset:      Offset of IRQ STATUS register
 */
struct ais_irq_register_set {
	uint32_t                       mask_reg_offset;
	uint32_t                       clear_reg_offset;
	uint32_t                       status_reg_offset;
};

/*
 * struct ais_irq_controller_reg_info:
 * @Brief:                  Structure describing the IRQ registers
 *
 * @num_registers:          Number of sets(mask/clear/status) of IRQ registers
 * @irq_reg_set:            Array of Register Set Offsets.
 *                          Length of array = num_registers
 * @global_clear_offset:    Offset of Global IRQ Clear register. This register
 *                          contains the BIT that needs to be set for the CLEAR
 *                          to take effect
 * @global_clear_bitmask:   Bitmask needed to be used in Global Clear register
 *                          for Clear IRQ cmd to take effect
 */
struct ais_irq_controller_reg_info {
	uint32_t                      num_registers;
	struct ais_irq_register_set  *irq_reg_set;
	uint32_t                      global_clear_offset;
	uint32_t                      global_clear_bitmask;
};

/*
 * struct ais_isp_timestamp:
 *
 * @mono_time:          Monotonic boot time
 * @vt_time:            AV Timer time
 * @ticks:              Qtimer ticks
 * @time_usecs:         time in micro seconds
 */
struct ais_isp_timestamp {
	struct timeval          mono_time;
	struct timeval          vt_time;
	uint64_t                ticks;
	uint64_t                time_usecs;
};

/*
 * ais_isp_hw_get_timestamp()
 *
 * @Brief:              Get timestamp values
 *
 * @time_stamp:         Structure that holds different time values
 *
 * @Return:             Void
 */
void ais_isp_hw_get_timestamp(struct ais_isp_timestamp *time_stamp);

enum ais_isp_hw_type {
	AIS_ISP_HW_TYPE_CSID        = 0,
	AIS_ISP_HW_TYPE_ISPIF       = 1,
	AIS_ISP_HW_TYPE_VFE         = 2,
	AIS_ISP_HW_TYPE_IFE_CSID    = 3,
	AIS_ISP_HW_TYPE_MAX         = 4,
};

enum ais_isp_resource_state {
	AIS_ISP_RESOURCE_STATE_UNAVAILABLE   = 0,
	AIS_ISP_RESOURCE_STATE_AVAILABLE     = 1,
	AIS_ISP_RESOURCE_STATE_RESERVED      = 2,
	AIS_ISP_RESOURCE_STATE_INIT_HW       = 3,
	AIS_ISP_RESOURCE_STATE_STREAMING     = 4,
	AIS_ISP_RESOURCE_STATE_ERROR         = 5
};

enum ais_isp_hw_cmd_type {
	AIS_ISP_HW_CMD_GET_CHANGE_BASE,
	AIS_ISP_HW_CMD_GET_BUF_UPDATE,
	AIS_ISP_HW_CMD_GET_BUF_UPDATE_RM,
	AIS_ISP_HW_CMD_GET_REG_UPDATE,
	AIS_ISP_HW_CMD_GET_HFR_UPDATE,
	AIS_ISP_HW_CMD_GET_HFR_UPDATE_RM,
	AIS_ISP_HW_CMD_GET_SECURE_MODE,
	AIS_ISP_HW_CMD_STRIPE_UPDATE,
	AIS_ISP_HW_CMD_CLOCK_UPDATE,
	AIS_ISP_HW_CMD_BW_UPDATE,
	AIS_ISP_HW_CMD_BW_CONTROL,
	AIS_ISP_HW_CMD_STOP_BUS_ERR_IRQ,
	AIS_ISP_HW_CMD_GET_REG_DUMP,
	AIS_ISP_HW_CMD_UBWC_UPDATE,
	AIS_ISP_HW_CMD_SOF_IRQ_DEBUG,
	AIS_ISP_HW_CMD_SET_CAMIF_DEBUG,
	AIS_ISP_HW_CMD_CSID_CLOCK_UPDATE,
	AIS_ISP_HW_CMD_FE_UPDATE_IN_RD,
	AIS_ISP_HW_CMD_FE_UPDATE_BUS_RD,
	AIS_ISP_HW_CMD_GET_IRQ_REGISTER_DUMP,
	AIS_ISP_HW_CMD_FPS_CONFIG,
	AIS_ISP_HW_CMD_DUMP_HW,
	AIS_ISP_HW_CMD_SET_STATS_DMI_DUMP,
	AIS_ISP_HW_CMD_GET_RDI_IRQ_MASK,
	AIS_ISP_HW_CMD_MAX,
};


/*
 * struct ais_isp_hw_cmd_buf_update:
 *
 * @Brief:           Contain the new created command buffer information
 *
 * @cmd_buf_addr:    Command buffer to store the change base command
 * @size:            Size of the buffer in bytes
 * @used_bytes:      Consumed bytes in the command buffer
 *
 */
struct ais_isp_hw_cmd_buf_update {
	uint32_t                       *cmd_buf_addr;
	uint32_t                        size;
	uint32_t                        used_bytes;
};

/*
 * struct ais_isp_hw_get_wm_update:
 *
 * @Brief:         Get cmd buffer for WM updates.
 *
 * @ image_buf:    image buffer address array
 * @ num_buf:      Number of buffers in the image_buf array
 * @ io_cfg:       IO buffer config information sent from UMD
 *
 */
struct ais_isp_hw_get_wm_update {
	uint64_t                       *image_buf;
	uint32_t                        num_buf;
	struct cam_buf_io_cfg          *io_cfg;
};

/*
 * struct ais_isp_hw_rup_data:
 *
 * @Brief:         RUP for required resources.
 *
 * @is_fe_enable   if fetch engine enabled
 * @res_bitmap     resource bitmap for set resources
 *
 */
struct ais_isp_hw_rup_data {
	bool                            is_fe_enable;
	unsigned long                   res_bitmap;
};

/*
 * struct ais_isp_hw_get_cmd_update:
 *
 * @Brief:         Get cmd buffer update for different CMD types
 *
 * @res:           Resource node
 * @cmd_type:      Command type for which to get update
 * @cmd:           Command buffer information
 *
 */
struct ais_isp_hw_get_cmd_update {
	enum ais_isp_hw_cmd_type          cmd_type;
	union {
		void                                 *data;
	};
};

/*
 * struct ais_isp_hw_dump_args:
 *
 * @Brief:        isp hw dump args
 *
 * @ req_id:         request id
 * @ cpu_addr:       cpu address
 * @ buf_len:        buf len
 * @ offset:         offset of buffer
 * @ ctxt_to_hw_map: ctx to hw map
 */
struct ais_isp_hw_dump_args {
	uint64_t                req_id;
	uintptr_t               cpu_addr;
	size_t                  buf_len;
	uint32_t                offset;
	void                    *ctxt_to_hw_map;
};

/**
 * struct ais_isp_hw_dump_header - ISP context dump header
 *
 * @Brief:        isp hw dump header
 *
 * @tag:       Tag name for the header
 * @word_size: Size of word
 * @size:      Size of data
 *
 */
struct ais_isp_hw_dump_header {
	char      tag[AIS_ISP_HW_DUMP_TAG_MAX_LEN];
	uint64_t  size;
	uint32_t  word_size;
};


/**
 * enum ais_ife_output_path_id
 *
 * @brief output path IDs
 */
enum ais_ife_output_path_id {
	AIS_IFE_PATH_RDI_0,
	AIS_IFE_PATH_RDI_1,
	AIS_IFE_PATH_RDI_2,
	AIS_IFE_PATH_RDI_3,
	AIS_IFE_PATH_MAX,
};

/**
 * struct ais_ife_rdi_in_cfg
 *
 * @brief Input Configuration
 *
 * @format : format
 * @width : width
 * @height : height
 * @crop_enable : is crop enabled
 * @crop_top : crop top line
 * @crop_bottom : crop bottom line
 * @crop_left : crop left pixel
 * @crop_right : crop right pixel
 * @reserved
 */
struct ais_ife_rdi_in_cfg {
	uint32_t format;
	uint32_t decode_format;
	uint32_t pack_type;
	uint32_t width;
	uint32_t height;
	uint32_t crop_enable;
	uint32_t crop_top;
	uint32_t crop_bottom;
	uint32_t crop_left;
	uint32_t crop_right;
	uint32_t init_frame_drop;
	uint32_t reserved;
};

/**
 * struct ais_ife_rdi_out_cfg
 *
 * @brief Output Configuration
 *
 * @format : format
 * @secure_mode : Data Type
 * @mode : line based or frame based
 * @width : width
 * @height : height
 * @stride : stride
 * @frame_increment : frame increment
 * @frame_drop_pattern : framedrop pattern
 * @frame_drop_period : framedrop period
 * @reserved
 */
struct ais_ife_rdi_out_cfg {
	uint32_t format;
	uint32_t secure_mode;
	uint32_t mode;
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	uint32_t frame_increment;
	uint32_t frame_drop_pattern;
	uint32_t frame_drop_period;
	uint32_t reserved;
};

/**
 * struct ais_ife_csid_csi_info
 *
 * @brief CSI Configuration
 *
 * @csiphy_id : CSIPHY id
 * @vc : Virtual Channel
 * @dt : Data Type
 * @num_lanes : Number of lanes
 * @lane_assign : Lane mapping
 * @is_3Phase : DPHY or CPHY
 */
struct ais_ife_csid_csi_info {
	uint32_t csiphy_id;
	uint32_t vc;
	uint32_t dt;
	uint32_t num_lanes;
	uint32_t lane_assign;
	uint32_t is_3Phase;
};

/**
 * struct ais_ife_rdi_init_args
 *
 * @brief Iniit RDI path
 *
 * @path : output path
 * @csi_cfg : CSI configuration
 * @in_cfg : Input configuration
 * @out_cfg : Output configuration
 */
struct ais_ife_rdi_init_args {
	enum ais_ife_output_path_id path;
	struct ais_ife_csid_csi_info csi_cfg;
	struct ais_ife_rdi_in_cfg    in_cfg;
	struct ais_ife_rdi_out_cfg   out_cfg;
};

/**
 * struct ais_ife_rdi_deinit_args
 *
 * @brief Deinit RDI path
 *
 * @path : output path
 */
struct ais_ife_rdi_deinit_args {
	enum ais_ife_output_path_id path;
};

/**
 * struct ais_ife_rdi_stop_args
 *
 * @brief Start RDI path
 *
 * @path : output path
 */
struct ais_ife_rdi_start_args {
	enum ais_ife_output_path_id path;
};

/**
 * struct ais_ife_rdi_stop_args
 *
 * @brief Stop RDI path
 *
 * @path : output path
 */
struct ais_ife_rdi_stop_args {
	enum ais_ife_output_path_id path;
};

/**
 * struct ais_ife_enqueue_buffer_args
 *
 * @brief buffer definition for enqueue
 *
 * @mem_handle :   allocated mem_handle
 * @idx : buffer index used as identifier
 * @offset : offset into buffer for hw to write to
 */
struct ais_ife_buffer_enqueue {
	int32_t mem_handle;
	uint32_t idx;
	uint32_t offset;
};

/**
 * struct ais_ife_enqueue_buffer_args
 *
 * @brief Enqueue buffer command argument
 *
 * @path :   output path to enqueue to
 * @buffer : image buffer
 * @buffer_header : frame header buffer
 */
struct ais_ife_enqueue_buffer_args {
	enum ais_ife_output_path_id   path;
	struct ais_ife_buffer_enqueue buffer;
	struct ais_ife_buffer_enqueue buffer_header;
};

/**
 * struct ais_ife_rdi_timestamps
 *
 * @brief :  timestamps for RDI path
 *
 * @cur_sof_ts : current SOF time stamp
 * @prev_sof_ts : previous SOF time stamp
 */
struct ais_ife_rdi_timestamps {
	uint64_t cur_sof_ts;
	uint64_t prev_sof_ts;
};

/**
 * struct ais_ife_rdi_get_timestamp_args
 *
 * @brief :  time stamp capture arguments
 *
 * @path :   output path to get the time stamp
 * @ts :     timestamps
 */
struct ais_ife_rdi_get_timestamp_args {
	enum ais_ife_output_path_id    path;
	struct ais_ife_rdi_timestamps *ts;
};

/**
 * struct ais_ife_sof_msg
 *
 * @brief SOF event message
 *
 * @hw_ts :   HW timestamp
 * @frame_id : frame count
 */
struct ais_ife_sof_msg {
	uint64_t  hw_ts;
	uint32_t  frame_id;
};

/**
 * struct ais_ife_error_msg
 *
 * @brief Error event message
 *
 * @reserved : payload information
 */
struct ais_ife_error_msg {
	uint32_t  reserved;
};

/**
 * struct ais_ife_frame_msg
 *
 * @brief Frame done event message
 *
 * @hw_ts : SOF HW timestamp
 * @ts :    SOF timestamp
 * @frame_id : frame count
 * @buf_idx : buffer index
 */
struct ais_ife_frame_msg {
	uint64_t  hw_ts;
	uint64_t  ts;
	uint32_t  frame_id;
	uint32_t  buf_idx;
};

/**
 * enum ais_ife_msg_type
 *
 * @brief event message type
 */
enum ais_ife_msg_type {
	AIS_IFE_MSG_SOF,
	AIS_IFE_MSG_FRAME_DONE,
	AIS_IFE_MSG_OUTPUT_WARNING,
	AIS_IFE_MSG_OUTPUT_ERROR,
	AIS_IFE_MSG_CSID_WARNING,
	AIS_IFE_MSG_CSID_ERROR
};

/**
 * struct ais_ife_frame_msg
 *
 * @brief Frame done event message
 *
 * @type :   message type
 * @idx :    IFE idx
 * @path :   input/output path
 * @reserved: reserved for alignment
 * @reserved1: reserved for alignment
 * @boot_ts : event timestamp
 * @u       : event message
 */
struct ais_ife_event_data {
	uint8_t   type;
	uint8_t   idx;
	uint8_t   path;
	uint8_t   reserved;
	uint32_t  reserved1;
	uint64_t  boot_ts;
	union {
		struct ais_ife_sof_msg sof_msg;
		struct ais_ife_error_msg err_msg;
		struct ais_ife_frame_msg frame_msg;
	} u;
};

/* hardware event callback function type */
typedef int (*ais_ife_event_cb_func)(void *priv,
	struct ais_ife_event_data *evt_data);


struct ais_isp_hw_init_args {
	uint32_t hw_idx;
	int iommu_hdl;
	int iommu_hdl_secure;
	ais_ife_event_cb_func    event_cb;
	void                    *event_cb_priv;
};

#endif /* _AIS_ISP_HW_H_ */
