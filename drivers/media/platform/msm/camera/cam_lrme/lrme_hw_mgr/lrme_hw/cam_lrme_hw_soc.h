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

#ifndef _CAM_LRME_HW_SOC_H_
#define _CAM_LRME_HW_SOC_H_

#include "cam_soc_util.h"

struct cam_lrme_soc_private {
	uint32_t cpas_handle;
};

int cam_lrme_soc_enable_resources(struct cam_hw_info *lrme_hw);
int cam_lrme_soc_disable_resources(struct cam_hw_info *lrme_hw);
int cam_lrme_soc_init_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t irq_handler, void *private_data);
int cam_lrme_soc_deinit_resources(struct cam_hw_soc_info *soc_info);

#endif /* _CAM_LRME_HW_SOC_H_ */
