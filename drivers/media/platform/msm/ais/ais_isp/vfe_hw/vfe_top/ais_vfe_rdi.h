/* Copyright (c) 2017-2018, 2020, The Linux Foundation. All rights reserved.
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

#ifndef _AIS_VFE_RDI_H_
#define _AIS_VFE_RDI_H_

#include "ais_vfe_top.h"

#define AIS_VFE_RDI_VER2_MAX  4

struct ais_vfe_rdi_ver2_reg {
	uint32_t     reg_update_cmd;
};

struct ais_vfe_rdi_reg_data {
	uint32_t     reg_update_cmd_data;
	uint32_t     sof_irq_mask;
	uint32_t     reg_update_irq_mask;
};

struct ais_vfe_rdi_ver2_hw_info {
	struct ais_vfe_top_ver2_reg_offset_common  *common_reg;
	struct ais_vfe_rdi_ver2_reg                *rdi_reg;
	struct ais_vfe_rdi_reg_data  *reg_data[AIS_VFE_RDI_VER2_MAX];
};

#endif /* _AIS_VFE_RDI_H_ */
