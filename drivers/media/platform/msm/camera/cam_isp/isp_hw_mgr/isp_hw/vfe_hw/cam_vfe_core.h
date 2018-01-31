/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#ifndef _CAM_VFE_CORE_H_
#define _CAM_VFE_CORE_H_

#include <linux/spinlock.h>
#include "cam_hw_intf.h"
#include "cam_vfe_top.h"
#include "cam_vfe_bus.h"
#include "cam_vfe_hw_intf.h"

struct cam_vfe_hw_info {
	struct cam_irq_controller_reg_info *irq_reg_info;

	uint32_t                          bus_version;
	void                             *bus_hw_info;

	uint32_t                          top_version;
	void                             *top_hw_info;
	uint32_t                          camif_version;
	void                             *camif_reg;

	uint32_t                          testgen_version;
	void                             *testgen_reg;

	uint32_t                          num_qos_settings;
	struct cam_isp_reg_val_pair      *qos_settings;

	uint32_t                          num_ds_settings;
	struct cam_isp_reg_val_pair      *ds_settings;

	uint32_t                          num_vbif_settings;
	struct cam_isp_reg_val_pair      *vbif_settings;
};

#define CAM_VFE_EVT_MAX                    256

struct cam_vfe_hw_core_info {
	struct cam_vfe_hw_info             *vfe_hw_info;
	void                               *vfe_irq_controller;
	struct cam_vfe_top                 *vfe_top;
	struct cam_vfe_bus                 *vfe_bus;
	void                               *tasklet_info;
	struct cam_vfe_top_irq_evt_payload  evt_payload[CAM_VFE_EVT_MAX];
	struct list_head                    free_payload_list;
	struct cam_vfe_irq_handler_priv     irq_payload;
	uint32_t                            cpas_handle;
	int                                 irq_handle;
	int                                 irq_err_handle;
	spinlock_t                          spin_lock;
};

int cam_vfe_get_hw_caps(void *device_priv,
	void *get_hw_cap_args, uint32_t arg_size);
int cam_vfe_init_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size);
int cam_vfe_deinit_hw(void *hw_priv,
	void *deinit_hw_args, uint32_t arg_size);
int cam_vfe_reset(void *device_priv,
	void *reset_core_args, uint32_t arg_size);
int cam_vfe_reserve(void *device_priv,
	void *reserve_args, uint32_t arg_size);
int cam_vfe_release(void *device_priv,
	void *reserve_args, uint32_t arg_size);
int cam_vfe_start(void *device_priv,
	void *start_args, uint32_t arg_size);
int cam_vfe_stop(void *device_priv,
	void *stop_args, uint32_t arg_size);
int cam_vfe_read(void *device_priv,
	void *read_args, uint32_t arg_size);
int cam_vfe_write(void *device_priv,
	void *write_args, uint32_t arg_size);
int cam_vfe_process_cmd(void *device_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size);

irqreturn_t cam_vfe_irq(int irq_num, void *data);

int cam_vfe_core_init(struct cam_vfe_hw_core_info *core_info,
	struct cam_hw_soc_info             *soc_info,
	struct cam_hw_intf                 *hw_intf,
	struct cam_vfe_hw_info             *vfe_hw_info);

int cam_vfe_core_deinit(struct cam_vfe_hw_core_info *core_info,
	struct cam_vfe_hw_info             *vfe_hw_info);

#endif /* _CAM_VFE_CORE_H_ */
