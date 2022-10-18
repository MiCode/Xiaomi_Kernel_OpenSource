/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_CONTEXT_UTILS_H_
#define _CAM_CONTEXT_UTILS_H_

#include <linux/types.h>
#include "cam_smmu_api.h"

int cam_context_buf_done_from_hw(struct cam_context *ctx,
	void *done_event_data, uint32_t evt_id);
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
	struct cam_hw_mgr_dump_pf_data *pf_data, bool *mem_found, bool *ctx_found,
	uint32_t  *resource_type,
	struct cam_smmu_pf_info *pf_info);
int32_t cam_context_dump_hw_acq_info(struct cam_context *ctx);
int32_t cam_context_dump_dev_to_hw(struct cam_context *ctx,
	struct cam_dump_req_cmd *cmd);
size_t cam_context_parse_config_cmd(struct cam_context *ctx, struct cam_config_dev_cmd *cmd,
	struct cam_packet **packet);
int cam_context_mini_dump(struct cam_context *ctx, void *args);
#endif /* _CAM_CONTEXT_UTILS_H_ */
