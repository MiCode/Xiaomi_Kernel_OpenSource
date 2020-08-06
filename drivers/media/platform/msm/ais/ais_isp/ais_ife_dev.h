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

#ifndef _AIS_IFE_DEV_H_
#define _AIS_IFE_DEV_H_

#include "cam_subdev.h"
#include "cam_hw_intf.h"

#define AIS_IFE_DEV_NAME_MAX_LENGTH 20

/**
 * struct ais_ife_dev - Camera IFE V4l2 device node
 *
 * @sd:                    IFE subdevice node
 * @ctx:                   IFE base context storage
 * @ctx_isp:               IFE private context storage
 * @mutex:                 IFE dev mutex
 * @open_cnt:              Open device count
 */
struct ais_ife_dev {
	/*subdev info*/
	char device_name[AIS_IFE_DEV_NAME_MAX_LENGTH];
	struct cam_subdev cam_sd;

	uint32_t hw_idx;

	struct cam_hw_intf *p_vfe_drv;
	struct cam_hw_intf *p_csid_drv;

	int iommu_hdl;
	int iommu_hdl_secure;

	struct mutex mutex;
	int32_t open_cnt;
};

#endif /* _AIS_IFE_DEV_H_ */
