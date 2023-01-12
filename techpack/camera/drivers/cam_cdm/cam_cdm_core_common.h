/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_CDM_CORE_COMMON_H_
#define _CAM_CDM_CORE_COMMON_H_

#include "cam_mem_mgr.h"

#define CAM_CDM100_VERSION 0x10000000
#define CAM_CDM110_VERSION 0x10010000
#define CAM_CDM120_VERSION 0x10020000
#define CAM_CDM200_VERSION 0x20000000
#define CAM_CDM210_VERSION 0x20010000

#define CAM_CDM_AHB_BURST_LEN_1  (BIT(1) - 1)
#define CAM_CDM_AHB_BURST_LEN_4  (BIT(2) - 1)
#define CAM_CDM_AHB_BURST_LEN_8  (BIT(3) - 1)
#define CAM_CDM_AHB_BURST_LEN_16 (BIT(4) - 1)
#define CAM_CDM_AHB_BURST_EN      BIT(4)
#define CAM_CDM_AHB_STOP_ON_ERROR BIT(8)
#define CAM_CDM_ARB_SEL_RR        BIT(16)
#define CAM_CDM_IMPLICIT_WAIT_EN  BIT(17)

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
int cam_hw_cdm_reset_hw(struct cam_hw_info *cdm_hw, uint32_t handle);
int cam_hw_cdm_flush_hw(struct cam_hw_info *cdm_hw, uint32_t handle);
int cam_hw_cdm_handle_error(struct cam_hw_info *cdm_hw, uint32_t handle);
int cam_hw_cdm_hang_detect(struct cam_hw_info *cdm_hw, uint32_t handle);
struct cam_cdm_bl_cb_request_entry *cam_cdm_find_request_by_bl_tag(
	uint32_t tag, struct list_head *bl_list);
void cam_cdm_notify_clients(struct cam_hw_info *cdm_hw,
	enum cam_cdm_cb_status status, void *data);
void cam_hw_cdm_dump_core_debug_registers(
	struct cam_hw_info *cdm_hw, bool pause_core);

#endif /* _CAM_CDM_CORE_COMMON_H_ */
