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

#ifndef _AIS_VFE_HW_INTF_H_
#define _AIS_VFE_HW_INTF_H_

#include "ais_isp_hw.h"

#define AIS_VFE_HW_NUM_MAX            8

#define VFE_CORE_BASE_IDX             0
/*
 * VBIF and BUS do not exist on same HW.
 * Hence both can be 1 below.
 */
#define VFE_VBIF_BASE_IDX             1
#define VFE_BUS_BASE_IDX              1

enum ais_vfe_hw_cmd_type {
	AIS_VFE_CMD_ENQ_BUFFER = 0
};

enum ais_vfe_hw_irq_status {
	AIS_VFE_IRQ_STATUS_ERR_COMP             = -3,
	AIS_VFE_IRQ_STATUS_COMP_OWRT            = -2,
	AIS_VFE_IRQ_STATUS_ERR                  = -1,
	AIS_VFE_IRQ_STATUS_SUCCESS              = 0,
	AIS_VFE_IRQ_STATUS_OVERFLOW             = 1,
	AIS_VFE_IRQ_STATUS_P2I_ERROR            = 2,
	AIS_VFE_IRQ_STATUS_VIOLATION            = 3,
	AIS_VFE_IRQ_STATUS_MAX,
};

enum ais_vfe_hw_irq_regs {
	CAM_IFE_IRQ_CAMIF_REG_STATUS0           = 0,
	CAM_IFE_IRQ_CAMIF_REG_STATUS1           = 1,
	CAM_IFE_IRQ_VIOLATION_STATUS            = 2,
	CAM_IFE_IRQ_REGISTERS_MAX,
};

enum ais_vfe_bus_irq_regs {
	CAM_IFE_IRQ_BUS_REG_STATUS0             = 0,
	CAM_IFE_IRQ_BUS_REG_STATUS1             = 1,
	CAM_IFE_IRQ_BUS_REG_STATUS2             = 2,
	CAM_IFE_IRQ_BUS_REG_COMP_ERR            = 3,
	CAM_IFE_IRQ_BUS_REG_COMP_OWRT           = 4,
	CAM_IFE_IRQ_BUS_DUAL_COMP_ERR           = 5,
	CAM_IFE_IRQ_BUS_DUAL_COMP_OWRT          = 6,
	CAM_IFE_BUS_IRQ_REGISTERS_MAX,
};

enum ais_vfe_reset_type {
	AIS_VFE_HW_RESET_HW_AND_REG,
	AIS_VFE_HW_RESET_HW,
	AIS_VFE_HW_RESET_MAX,
};

/**
 * struct ais_vfe_hw_evt_payload- handle vfe error
 * @hw_idx :     Hw index of vfe
 * @evt_type :   Event type from VFE
 */
struct ais_vfe_hw_evt_payload {
	uint32_t            hw_idx;
	uint32_t            evt_type;
};

/*
 * struct ais_vfe_hw_get_hw_cap:
 *
 * @max_width:               Max width supported by HW
 * @max_height:              Max height supported by HW
 * @max_pixel_num:           Max Pixel channels available
 * @max_rdi_num:             Max Raw channels available
 */
struct ais_vfe_hw_get_hw_cap {
	uint32_t                max_width;
	uint32_t                max_height;
	uint32_t                max_pixel_num;
	uint32_t                max_rdi_num;
};

/*
 * ais_vfe_hw_init()
 *
 * @Brief:                  Initialize VFE HW device
 *
 * @vfe_hw:                 vfe_hw interface to fill in and return on
 *                          successful initialization
 * @hw_idx:                 Index of VFE HW
 */
int ais_vfe_hw_init(struct cam_hw_intf **vfe_hw,
	struct ais_isp_hw_init_args *init,
	struct cam_hw_intf *csid_hw);


#endif /* _AIS_VFE_HW_INTF_H_ */
