/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#ifndef _CAM_CONTEXT_UTILS_H_
#define _CAM_CONTEXT_UTILS_H_

#include <linux/types.h>

int cam_context_buf_done_from_hw(struct cam_context *ctx,
	void *done_event_data, uint32_t bubble_state);
int32_t cam_context_release_dev_to_hw(struct cam_context *ctx,
	struct cam_release_dev_cmd *cmd);
int32_t cam_context_prepare_dev_to_hw(struct cam_context *ctx,
	struct cam_config_dev_cmd *cmd);
int32_t cam_context_config_dev_to_hw(
	struct cam_context *ctx, struct cam_config_dev_cmd *cmd);
int32_t cam_context_acquire_dev_to_hw(struct cam_context *ctx,
	struct cam_acquire_dev_cmd *cmd);
int32_t cam_context_start_dev_to_hw(struct cam_context *ctx,
	struct cam_start_stop_dev_cmd *cmd);
int32_t cam_context_stop_dev_to_hw(struct cam_context *ctx);
int32_t cam_context_flush_dev_to_hw(struct cam_context *ctx,
	struct cam_flush_dev_cmd *cmd);
int32_t cam_context_flush_ctx_to_hw(struct cam_context *ctx);
int32_t cam_context_flush_req_to_hw(struct cam_context *ctx,
	struct cam_flush_dev_cmd *cmd);
int32_t cam_context_dump_pf_info_to_hw(struct cam_context *ctx,
	struct cam_packet *packet, unsigned long iova, uint32_t buf_info,
	bool *mem_found);

#endif /* _CAM_CONTEXT_UTILS_H_ */
