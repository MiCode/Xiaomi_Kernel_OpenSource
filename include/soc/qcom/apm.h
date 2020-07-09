/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __LINUX_POWER_QCOM_APM_H__
#define __LINUX_POWER_QCOM_APM_H__

#include <linux/device.h>
#include <linux/err.h>

/**
 * enum msm_apm_supply - supported power rails to supply memory arrays
 * %MSM_APM_SUPPLY_APCC:	to enable selection of VDD_APCC rail as supply
 * %MSM_APM_SUPPLY_MX:		to enable selection of VDD_MX rail as supply
 */
enum msm_apm_supply {
	MSM_APM_SUPPLY_APCC,
	MSM_APM_SUPPLY_MX,
};

/* Handle used to identify an APM controller device  */
struct msm_apm_ctrl_dev;

#ifdef CONFIG_MSM_APM
struct msm_apm_ctrl_dev *msm_apm_ctrl_dev_get(struct device *dev);
int msm_apm_set_supply(struct msm_apm_ctrl_dev *ctrl_dev,
		       enum msm_apm_supply supply);
int msm_apm_get_supply(struct msm_apm_ctrl_dev *ctrl_dev);

#else
static inline struct msm_apm_ctrl_dev *msm_apm_ctrl_dev_get(struct device *dev)
{ return ERR_PTR(-EPERM); }
static inline int msm_apm_set_supply(struct msm_apm_ctrl_dev *ctrl_dev,
		       enum msm_apm_supply supply)
{ return -EPERM; }
static inline int msm_apm_get_supply(struct msm_apm_ctrl_dev *ctrl_dev)
{ return -EPERM; }
#endif
#endif
