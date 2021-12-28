/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_VFE_CORE_H_
#define _CAM_VFE_CORE_H_

#include <linux/spinlock_types.h>
#include "cam_hw_intf.h"
#include "cam_vfe_top.h"
#include "cam_vfe_bus.h"
#include "cam_vfe_hw_intf.h"

#define CAM_VFE_HW_IRQ_CAP_SOF             BIT(0)
#define CAM_VFE_HW_IRQ_CAP_EPOCH_0         BIT(1)
#define CAM_VFE_HW_IRQ_CAP_EPOCH_1         BIT(2)
#define CAM_VFE_HW_IRQ_CAP_RUP             BIT(3)
#define CAM_VFE_HW_IRQ_CAP_BUF_DONE        BIT(4)
#define CAM_VFE_HW_IRQ_CAP_EOF             BIT(5)
#define CAM_VFE_HW_IRQ_CAP_RESET           BIT(6)

#define CAM_VFE_HW_IRQ_CAP_INT_CSID        0x7F
#define CAM_VFE_HW_IRQ_CAP_LITE_INT_CSID   0x79
#define CAM_VFE_HW_IRQ_CAP_EXT_CSID        0x27
#define CAM_VFE_HW_IRQ_CAP_LITE_EXT_CSID   0x21

struct cam_vfe_irq_hw_info {
	int                                   reset_irq_handle;
	uint32_t                              reset_mask;
	struct cam_irq_controller_reg_info   *top_irq_reg;
	uint32_t                              supported_irq;
};

struct cam_vfe_hw_info {
	struct cam_vfe_irq_hw_info       *irq_hw_info;

	uint32_t                          bus_version;
	void                             *bus_hw_info;

	uint32_t                          bus_rd_version;
	void                             *bus_rd_hw_info;

	uint32_t                          top_version;
	void                             *top_hw_info;
	uint32_t                          camif_version;
	void                             *camif_reg;

	uint32_t                          camif_lite_version;
	void                             *camif_lite_reg;
};

#define CAM_VFE_EVT_MAX                    256

struct cam_vfe_hw_core_info {
	struct cam_vfe_hw_info             *vfe_hw_info;
	void                               *vfe_irq_controller;
	struct cam_vfe_top                 *vfe_top;
	struct cam_vfe_bus                 *vfe_bus;
	struct cam_vfe_bus                 *vfe_rd_bus;
	void                               *tasklet_info;
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
