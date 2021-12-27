/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _CAM_CUSTOM_SUB_MOD_CORE_H_
#define _CAM_CUSTOM_SUB_MOD_CORE_H_

#include "cam_debug_util.h"
#include "cam_custom_hw.h"
#include "cam_custom_sub_mod_soc.h"
#include "cam_custom_hw_mgr_intf.h"

struct cam_custom_sub_mod_set_irq_cb {
	int32_t (*custom_hw_mgr_cb)(void *data,
		struct cam_custom_hw_cb_args *cb_args);
	void *data;
};

struct cam_custom_sub_mod_req_to_dev {
	uint64_t req_id;
	uint32_t ctx_idx;
	uint32_t dev_idx;
};

struct cam_custom_device_hw_info {
	uint32_t hw_ver;
	uint32_t irq_status;
	uint32_t irq_mask;
	uint32_t irq_clear;
};

struct cam_custom_sub_mod_core_info {
	uint32_t cpas_handle;
	bool cpas_start;
	bool clk_enable;
	struct cam_custom_sub_mod_set_irq_cb irq_cb;
	struct cam_custom_sub_mod_req_to_dev *curr_req;
	struct cam_custom_device_hw_info *device_hw_info;
	struct cam_hw_info *custom_hw_info;
};

enum cam_custom_hw_resource_type {
	CAM_CUSTOM_HW_RESOURCE_UNINT,
	CAM_CUSTOM_HW_RESOURCE_SRC,
	CAM_CUSTOM_HW_RESOURCE_MAX,
};

struct cam_custom_sub_mod_acq {
	enum cam_custom_hw_resource_type   rsrc_type;
	int32_t                            acq;
	struct cam_custom_resource_node   *rsrc_node;
};

int cam_custom_hw_sub_mod_get_hw_caps(void *hw_priv,
	void *get_hw_cap_args, uint32_t arg_size);
int cam_custom_hw_sub_mod_init_hw(void *hw_priv,
	void *init_hw_args, uint32_t arg_size);
int cam_custom_hw_sub_mod_deinit_hw(void *hw_priv,
	void *deinit_hw_args, uint32_t arg_size);
int cam_custom_hw_sub_mod_reset(void *hw_priv,
	void *deinit_hw_args, uint32_t arg_size);
int cam_custom_hw_sub_mod_reserve(void *hw_priv,
	void *reserve_args, uint32_t arg_size);
int cam_custom_hw_sub_mod_release(void *hw_priv,
	void *release_args, uint32_t arg_size);
int cam_custom_hw_sub_mod_start(void *hw_priv,
	void *start_args, uint32_t arg_size);
int cam_custom_hw_sub_mod_stop(void *hw_priv,
	void *stop_args, uint32_t arg_size);
int cam_custom_hw_sub_mod_read(void *hw_priv,
	void *read_args, uint32_t arg_size);
int cam_custom_hw_sub_mod_write(void *hw_priv,
	void *write_args, uint32_t arg_size);
int cam_custom_hw_sub_mod_process_cmd(void *hw_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size);
irqreturn_t cam_custom_hw_sub_mod_irq(int irq_num, void *data);

#endif /* _CAM_CUSTOM_SUB_MOD_CORE_H_ */
