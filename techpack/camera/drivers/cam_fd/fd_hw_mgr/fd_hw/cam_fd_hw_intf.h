/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_FD_HW_INTF_H_
#define _CAM_FD_HW_INTF_H_

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <media/cam_cpas.h>
#include <media/cam_req_mgr.h>
#include <media/cam_fd.h>

#include "cam_io_util.h"
#include "cam_soc_util.h"
#include "cam_hw.h"
#include "cam_hw_intf.h"
#include "cam_subdev.h"
#include "cam_cpas_api.h"
#include "cam_hw_mgr_intf.h"
#include "cam_debug_util.h"

#define CAM_FD_MAX_IO_BUFFERS        5
#define CAM_FD_MAX_HW_ENTRIES        5
#define CAM_FD_HW_DUMP_TAG_MAX_LEN   32
#define CAM_FD_HW_DUMP_NUM_WORDS     5

/**
 * enum cam_fd_hw_type - Enum for FD HW type
 *
 * @CAM_HW_FD : FaceDetection HW type
 */
enum cam_fd_hw_type {
	CAM_HW_FD,
};

/**
 * enum cam_fd_hw_mode - Mode in which HW can run
 *
 * @CAM_FD_MODE_FACEDETECTION : Face Detection mode in which face search
 *                              is done on the given frame
 * @CAM_FD_MODE_PYRAMID       : Pyramid mode where a pyramid image is generated
 *                              from an input image
 */
enum cam_fd_hw_mode {
	CAM_FD_MODE_FACEDETECTION    = 0x1,
	CAM_FD_MODE_PYRAMID          = 0x2,
};

/**
 * enum cam_fd_priority - FD priority levels
 *
 * @CAM_FD_PRIORITY_HIGH   : Indicates high priority client, driver prioritizes
 *                           frame requests coming from contexts with HIGH
 *                           priority compared to context with normal priority
 * @CAM_FD_PRIORITY_NORMAL : Indicates normal priority client
 */
enum cam_fd_priority {
	CAM_FD_PRIORITY_HIGH         = 0x0,
	CAM_FD_PRIORITY_NORMAL,
};

/**
 * enum cam_fd_hw_irq_type - FD HW IRQ types
 *
 * @CAM_FD_IRQ_FRAME_DONE : Indicates frame processing is finished
 * @CAM_FD_IRQ_HALT_DONE  : Indicates HW halt is finished
 * @CAM_FD_IRQ_RESET_DONE : Indicates HW reset is finished
 */
enum cam_fd_hw_irq_type {
	CAM_FD_IRQ_FRAME_DONE,
	CAM_FD_IRQ_HALT_DONE,
	CAM_FD_IRQ_RESET_DONE,
};

/**
 * enum cam_fd_hw_cmd_type - FD HW layer custom commands
 *
 * @CAM_FD_HW_CMD_PRESTART          : Command to process pre-start settings
 * @CAM_FD_HW_CMD_FRAME_DONE        : Command to process frame done settings
 * @CAM_FD_HW_CMD_UPDATE_SOC        : Command to process soc update
 * @CAM_FD_HW_CMD_REGISTER_CALLBACK : Command to set hw mgr callback
 * @CAM_FD_HW_CMD_MAX               : Indicates max cmd
 * @CAM_FD_HW_CMD_HW_DUMP           : Command to dump fd hw information
 */
enum cam_fd_hw_cmd_type {
	CAM_FD_HW_CMD_PRESTART,
	CAM_FD_HW_CMD_FRAME_DONE,
	CAM_FD_HW_CMD_UPDATE_SOC,
	CAM_FD_HW_CMD_REGISTER_CALLBACK,
	CAM_FD_HW_CMD_HW_DUMP,
	CAM_FD_HW_CMD_MAX,
};

/**
 * struct cam_fd_hw_io_buffer : FD HW IO Buffer information
 *
 * @valid    : Whether this IO Buf configuration is valid
 * @io_cfg   : IO Configuration information
 * @num_buf  : Number planes in io_addr, cpu_addr array
 * @io_addr  : Array of IO address information for planes
 * @cpu_addr : Array of CPU address information for planes
 */
struct cam_fd_hw_io_buffer {
	bool                   valid;
	struct cam_buf_io_cfg *io_cfg;
	uint32_t               num_buf;
	uint64_t               io_addr[CAM_PACKET_MAX_PLANES];
	uintptr_t              cpu_addr[CAM_PACKET_MAX_PLANES];
};

/**
 * struct cam_fd_hw_req_private : FD HW layer's private information
 *               specific to a request
 *
 * @ctx_hw_private  : FD HW layer's ctx specific private data
 * @request_id      : Request ID corresponding to this private information
 * @get_raw_results : Whether to get raw results for this request
 * @ro_mode_enabled : Whether RO mode is enabled for this request
 * @fd_results      : Pointer to save face detection results
 * @raw_results     : Pointer to save face detection raw results
 */
struct cam_fd_hw_req_private {
	void                  *ctx_hw_private;
	uint64_t               request_id;
	bool                   get_raw_results;
	bool                   ro_mode_enabled;
	struct cam_fd_results *fd_results;
	uint32_t              *raw_results;
};

/**
 * struct cam_fd_hw_reserve_args : Reserve args for this HW context
 *
 * @hw_ctx         : HW context for which reserve is requested
 * @mode           : Mode for which this reserve is requested
 * @ctx_hw_private : Pointer to save HW layer's private information specific
 *                   to this hw context. This has to be passed while calling
 *                   further HW layer calls
 */
struct cam_fd_hw_reserve_args {
	void                  *hw_ctx;
	enum cam_fd_hw_mode    mode;
	void                  *ctx_hw_private;
};

/**
 * struct cam_fd_hw_release_args : Release args for this HW context
 *
 * @hw_ctx         : HW context for which release is requested
 * @ctx_hw_private : HW layer's private information specific to this hw context
 */
struct cam_fd_hw_release_args {
	void    *hw_ctx;
	void    *ctx_hw_private;
};

/**
 * struct cam_fd_hw_init_args : Init args for this HW context
 *
 * @hw_ctx         : HW context for which init is requested
 * @ctx_hw_private : HW layer's private information specific to this hw context
 * @reset_required : Indicates if the reset is required during init or not
 */
struct cam_fd_hw_init_args {
	void    *hw_ctx;
	void    *ctx_hw_private;
	bool     reset_required;
};

/**
 * struct cam_fd_hw_deinit_args : Deinit args for this HW context
 *
 * @hw_ctx         : HW context for which deinit is requested
 * @ctx_hw_private : HW layer's private information specific to this hw context
 */
struct cam_fd_hw_deinit_args {
	void    *hw_ctx;
	void    *ctx_hw_private;
};

/**
 * struct cam_fd_hw_cmd_prestart_args : Prestart command args
 *
 * @hw_ctx               : HW context which submitted this prestart
 * @ctx_hw_private       : HW layer's private information specific to
 *                         this hw context
 * @request_id           : Request ID corresponds to this pre-start command
 * @get_raw_results      : Whether to get raw results for this request
 * @input_buf            : Input IO Buffer information for this request
 * @output_buf           : Output IO Buffer information for this request
 * @cmd_buf_addr         : Command buffer address to fill kmd commands
 * @size                 : Size available in command buffer
 * @pre_config_buf_size  : Buffer size filled with commands by KMD that has
 *                         to be inserted before umd commands
 * @post_config_buf_size : Buffer size filled with commands by KMD that has
 *                         to be inserted after umd commands
 * @hw_req_private       : HW layer's private information specific to
 *                         this request
 */
struct cam_fd_hw_cmd_prestart_args {
	void                         *hw_ctx;
	void                         *ctx_hw_private;
	uint64_t                      request_id;
	bool                          get_raw_results;
	struct cam_fd_hw_io_buffer    input_buf[CAM_FD_MAX_IO_BUFFERS];
	struct cam_fd_hw_io_buffer    output_buf[CAM_FD_MAX_IO_BUFFERS];
	uint32_t                     *cmd_buf_addr;
	uint32_t                      size;
	uint32_t                      pre_config_buf_size;
	uint32_t                      post_config_buf_size;
	struct cam_fd_hw_req_private  hw_req_private;
};

/**
 * struct cam_fd_hw_cmd_start_args : Start command args
 *
 * @hw_ctx                : HW context which submitting start command
 * @ctx_hw_private        : HW layer's private information specific to
 *                            this hw context
 * @hw_req_private        : HW layer's private information specific to
 *          this request
 * @hw_update_entries     : HW update entries corresponds to this request
 * @num_hw_update_entries : Number of hw update entries
 */
struct cam_fd_hw_cmd_start_args {
	void                          *hw_ctx;
	void                          *ctx_hw_private;
	struct cam_fd_hw_req_private  *hw_req_private;
	struct cam_hw_update_entry    *hw_update_entries;
	uint32_t                       num_hw_update_entries;
};

/**
 * struct cam_fd_hw_stop_args : Stop command args
 *
 * @hw_ctx         : HW context which submitting stop command
 * @ctx_hw_private : HW layer's private information specific to this hw context
 * @request_id     : Request ID that need to be stopped
 * @hw_req_private : HW layer's private information specific to this request
 */
struct cam_fd_hw_stop_args {
	void                         *hw_ctx;
	void                         *ctx_hw_private;
	uint64_t                      request_id;
	struct cam_fd_hw_req_private *hw_req_private;
};

/**
 * struct cam_fd_hw_frame_done_args : Frame done command args
 *
 * @hw_ctx         : HW context which submitting frame done request
 * @ctx_hw_private : HW layer's private information specific to this hw context
 * @request_id     : Request ID that need to be stopped
 * @hw_req_private : HW layer's private information specific to this request
 */
struct cam_fd_hw_frame_done_args {
	void                         *hw_ctx;
	void                         *ctx_hw_private;
	uint64_t                      request_id;
	struct cam_fd_hw_req_private *hw_req_private;
};

/**
 * struct cam_fd_hw_reset_args : Reset command args
 *
 * @hw_ctx         : HW context which submitting reset command
 * @ctx_hw_private : HW layer's private information specific to this hw context
 */
struct cam_fd_hw_reset_args {
	void    *hw_ctx;
	void    *ctx_hw_private;
};

/**
 * struct cam_fd_hw_cmd_set_irq_cb : Set IRQ callback command args
 *
 * @cam_fd_hw_mgr_cb : HW Mgr's callback pointer
 * @data             : HW Mgr's private data
 */
struct cam_fd_hw_cmd_set_irq_cb {
	int (*cam_fd_hw_mgr_cb)(void *data, enum cam_fd_hw_irq_type irq_type);
	void *data;
};

/**
 * struct cam_fd_hw_dump_args : Args for dump request
 *
 * @request_id   : Issue request id
 * @offset       : offset of the buffer
 * @buf_len      : Length of target buffer
 * @cpu_addr     : start address of the target buffer
 */
struct cam_fd_hw_dump_args {
	uint64_t  request_id;
	size_t    offset;
	size_t    buf_len;
	uintptr_t cpu_addr;
};

/**
 * struct cam_fd_hw_dump_header : fd hw dump header
 *
 * @tag       : fd hw dump header tag
 * @size      : Size of data
 * @word_size : size of each word
 */
struct cam_fd_hw_dump_header {
	uint8_t  tag[CAM_FD_HW_DUMP_TAG_MAX_LEN];
	uint64_t size;
	uint32_t word_size;
};

#endif /* _CAM_FD_HW_INTF_H_ */
