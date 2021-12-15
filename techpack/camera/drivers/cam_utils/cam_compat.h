/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2014-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_COMPAT_H_
#define _CAM_COMPAT_H_

#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/component.h>

#include "cam_csiphy_dev.h"
#include "cam_cpastop_hw.h"

#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE

#include <linux/msm_ion.h>
#include <linux/ion.h>
#include <linux/qcom_scm.h>

#else

#include <linux/msm_ion.h>
#include <linux/ion_kernel.h>
#include <soc/qcom/scm.h>

#endif

struct cam_fw_alloc_info {
	struct device *fw_dev;
	void          *fw_kva;
	uint64_t       fw_hdl;
};

int cam_reserve_icp_fw(struct cam_fw_alloc_info *icp_fw, size_t fw_length);
void cam_unreserve_icp_fw(struct cam_fw_alloc_info *icp_fw, size_t fw_length);
void cam_cpastop_scm_write(struct cam_cpas_hw_errata_wa *errata_wa);
int cam_ife_notify_safe_lut_scm(bool safe_trigger);
int camera_component_match_add_drivers(struct device *master_dev,
	struct component_match **match_list);
int cam_csiphy_notify_secure_mode(struct csiphy_device *csiphy_dev,
	bool protect, int32_t offset);

#endif /* _CAM_COMPAT_H_ */
