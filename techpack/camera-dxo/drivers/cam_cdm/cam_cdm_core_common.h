/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _CAM_CDM_CORE_COMMON_H_
#define _CAM_CDM_CORE_COMMON_H_

#include "cam_mem_mgr.h"

#define CAM_CDM170_VERSION 0x10000000
#define CAM_CDM175_VERSION 0x10010000
#define CAM_CDM480_VERSION 0x10020000

extern struct cam_cdm_utils_ops CDM170_ops;

int cam_hw_cdm_init(void *hw_priv, void *init_hw_args, uint32_t arg_size);
int cam_hw_cdm_deinit(void *hw_priv, void *init_hw_args, uint32_t arg_size);
int cam_hw_cdm_alloc_genirq_mem(void *hw_priv);
int cam_hw_cdm_release_genirq_mem(void *hw_priv);
int cam_cdm_get_caps(void *hw_priv, void *get_hw_cap_args, uint32_t arg_size);
int cam_cdm_stream_ops_internal(void *hw_priv, void *start_args,
	bool operation);
int cam_cdm_stream_start(void *hw_priv, void *start_args, uint32_t size);
int cam_cdm_stream_stop(void *hw_priv, void *start_args, uint32_t size);
int cam_cdm_process_cmd(void *hw_priv, uint32_t cmd, void *cmd_args,
	uint32_t arg_size);
bool cam_cdm_set_cam_hw_version(
	uint32_t ver, struct cam_hw_version *cam_version);
bool cam_cdm_cpas_cb(uint32_t client_handle, void *userdata,
	struct cam_cpas_irq_data *irq_data);
struct cam_cdm_utils_ops *cam_cdm_get_ops(
	uint32_t ver, struct cam_hw_version *cam_version, bool by_cam_version);
int cam_virtual_cdm_submit_bl(struct cam_hw_info *cdm_hw,
	struct cam_cdm_hw_intf_cmd_submit_bl *req,
	struct cam_cdm_client *client);
int cam_hw_cdm_submit_bl(struct cam_hw_info *cdm_hw,
	struct cam_cdm_hw_intf_cmd_submit_bl *req,
	struct cam_cdm_client *client);
struct cam_cdm_bl_cb_request_entry *cam_cdm_find_request_by_bl_tag(
	uint32_t tag, struct list_head *bl_list);
void cam_cdm_notify_clients(struct cam_hw_info *cdm_hw,
	enum cam_cdm_cb_status status, void *data);

#endif /* _CAM_CDM_CORE_COMMON_H_ */
