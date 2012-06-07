/* Qualcomm TrustZone communicator driver
 *
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#ifndef __TZCOMI_H_
#define __TZCOMI_H_

#include <linux/types.h>

enum tz_sched_cmd_id {
	TZ_SCHED_CMD_ID_INVALID      = 0,
	TZ_SCHED_CMD_ID_INIT_SB_OUT,    /**< Initialize the shared buffer */
	TZ_SCHED_CMD_ID_INIT_SB_LOG,    /**< Initialize the logging shared buf */
	TZ_SCHED_CMD_ID_UNKNOWN         = 0x7FFFFFFE,
	TZ_SCHED_CMD_ID_MAX             = 0x7FFFFFFF
};

enum tz_sched_cmd_type {
	TZ_SCHED_CMD_INVALID = 0,
	TZ_SCHED_CMD_NEW,      /** New TZ Scheduler Command */
	TZ_SCHED_CMD_PENDING,  /** Pending cmd...sched will restore stack */
	TZ_SCHED_CMD_COMPLETE, /** TZ sched command is complete */
	TZ_SCHED_CMD_MAX     = 0x7FFFFFFF
};

enum tz_sched_cmd_status {
	TZ_SCHED_STATUS_INCOMPLETE = 0,
	TZ_SCHED_STATUS_COMPLETE,
	TZ_SCHED_STATUS_MAX  = 0x7FFFFFFF
};

/** Command structure for initializing shared buffers (SB_OUT
    and SB_LOG)
*/
__packed struct tz_pr_init_sb_req_s {
	/** First 4 bytes should always be command id
	 * from enum tz_sched_cmd_id */
	uint32_t                  pr_cmd;
	/** Pointer to the physical location of sb_out buffer */
	uint32_t                  sb_ptr;
	/** length of shared buffer */
	uint32_t                  sb_len;
};


__packed struct tz_pr_init_sb_rsp_s {
	/** First 4 bytes should always be command id
	 * from enum tz_sched_cmd_id */
	uint32_t                  pr_cmd;
	/** Return code, 0 for success, Approp error code otherwise */
	int32_t                   ret;
};


/**
 * struct tzcom_command - tzcom command buffer
 * @cmd_type: value from enum tz_sched_cmd_type
 * @sb_in_cmd_addr: points to physical location of command
 *                buffer
 * @sb_in_cmd_len: length of command buffer
 */
__packed struct tzcom_command {
	uint32_t               cmd_type;
	uint8_t                *sb_in_cmd_addr;
	uint32_t               sb_in_cmd_len;
};

/**
 * struct tzcom_response - tzcom response buffer
 * @cmd_status: value from enum tz_sched_cmd_status
 * @sb_in_rsp_addr: points to physical location of response
 *                buffer
 * @sb_in_rsp_len: length of command response
 */
__packed struct tzcom_response {
	uint32_t                 cmd_status;
	uint8_t                  *sb_in_rsp_addr;
	uint32_t                 sb_in_rsp_len;
};

/**
 * struct tzcom_callback - tzcom callback buffer
 * @cmd_id: command to run in registered service
 * @sb_out_rsp_addr: points to physical location of response
 *                buffer
 * @sb_in_cmd_len: length of command response
 *
 * A callback buffer would be laid out in sb_out as follows:
 *
 *     --------------------- <--- struct tzcom_callback
 *     | callback header   |
 *     --------------------- <--- tzcom_callback.sb_out_cb_data_off
 *     | callback data     |
 *     ---------------------
 */
__packed struct tzcom_callback {
	uint32_t cmd_id;
	uint32_t sb_out_cb_data_len;
	uint32_t sb_out_cb_data_off;
};

#endif /* __TZCOMI_H_ */
